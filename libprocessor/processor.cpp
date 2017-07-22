#include "processor.hpp"

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <list>
#include <assert.h>

namespace beltpp
{

using threads = std::list<std::thread>;
using mutex = std::mutex;
using cv = std::condition_variable;
enum class pending_policy { allow_new, prevent_new, empty_queue };
/*
 * internals
 */
namespace detail
{
template <typename task_t>
class internals
{
public:
    using queue = std::queue<task_t>;
    internals(size_t count)
        : m_policy(pending_policy::allow_new)
        , m_thread_count(count)
        , m_busy_count(0)
    {}

    pending_policy m_policy;
    size_t m_thread_count;
    size_t m_busy_count;
    threads m_threads;
    mutex m_mutex;
    cv m_cv_add_exit__thrun;
    cv m_cv_thrun__wait;
    queue m_queue;
};
}
/*
 * processor
 */
template <typename task_t>
processor<task_t>::processor(size_t count)
    : m_pimpl(new detail::internals<task_t>(count))
{
    setCoreCount(count);
}

template <typename task_t>
processor<task_t>::~processor()
{
    //  make sure we do not throw here
    setCoreCount(0);
}

template <typename task_t>
void processor<task_t>::setCoreCount(size_t count)
{
    {
        std::lock_guard<mutex> lock(m_pimpl->m_mutex);
        m_pimpl->m_thread_count = count;
    }
    size_t current_thread_count = 0;
    size_t desired_thread_count = 0;
    {
        std::lock_guard<mutex> lock(m_pimpl->m_mutex);
        current_thread_count = m_pimpl->m_threads.size();
        desired_thread_count = m_pimpl->m_thread_count;
    }
    while (current_thread_count < desired_thread_count)
    {
        void (*fptrthrun)(size_t, processor<task_t>&);
        fptrthrun = &detail::thread_run;
        m_pimpl->m_threads.push_back(
                    std::thread(fptrthrun,
                                current_thread_count,
                                std::ref(*this)));
        ++current_thread_count;
    }
    if (current_thread_count > desired_thread_count)
        m_pimpl->m_cv_add_exit__thrun.notify_all();  //  notify all thread_run() to wake up from empty state sleep
    while (current_thread_count > desired_thread_count)
    {
        m_pimpl->m_threads.back().join();
        m_pimpl->m_threads.pop_back();
        --current_thread_count;
    }
}

template <typename task_t>
size_t processor<task_t>::getCoreCount()
{
    std::lock_guard<mutex> lock(m_pimpl->m_mutex);
    return m_pimpl->m_threads.size();
}

template <typename task_t>
void processor<task_t>::run(processor<task_t>::task const& obtask)  //  add
{
    bool bNotify = false;
    {
        std::lock_guard<mutex> lock(m_pimpl->m_mutex);
        if (m_pimpl->m_policy == pending_policy::allow_new)
        {
            m_pimpl->m_queue.push(obtask);
            bNotify = true;
        }
    }
    if (bNotify)
        m_pimpl->m_cv_add_exit__thrun.notify_one();  //  notify one thread_run() to wake up from empty wait
}

template <typename task_t>
void processor<task_t>::wait()
{
    std::unique_lock<std::mutex> lock(m_pimpl->m_mutex);

    auto& qu = m_pimpl->m_queue;
    auto& busy_count = m_pimpl->m_busy_count;
    m_pimpl->m_cv_thrun__wait.wait(lock, [&qu, &busy_count]
    {
        return (qu.empty() && 0 == busy_count);
    });
}

namespace detail
{
template <typename task_t>
inline void thread_run(size_t id, processor<task_t>& host) noexcept
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(host.m_pimpl->m_mutex);

        if (host.m_pimpl->m_queue.empty())
        {
            if (host.m_pimpl->m_thread_count < id + 1)
                return;

            {
                auto const& qu = host.m_pimpl->m_queue;
                auto const& thread_count = host.m_pimpl->m_thread_count;

                host.m_pimpl->m_cv_add_exit__thrun.wait(lock,
                                               [&qu,
                                               &thread_count,
                                               id]
                {
                    return (false == qu.empty() ||  //  one wait for processor::add
                            thread_count < id + 1); //  all wait for processor::exit
                });
            }
        }

        if (false == host.m_pimpl->m_queue.empty())
        {
            auto obtask = host.m_pimpl->m_queue.front();
            host.m_pimpl->m_queue.pop();
            ++host.m_pimpl->m_busy_count;

            lock.unlock();

            try
            {
                if (pending_policy::empty_queue != host.m_pimpl->m_policy)
                    obtask();
            }
            catch (...){}

            lock.lock();
            --host.m_pimpl->m_busy_count;
            if (host.m_pimpl->m_queue.empty() &&
                0 == host.m_pimpl->m_busy_count)
            {
                host.m_pimpl->m_cv_thrun__wait.notify_one();
            }
            lock.unlock();
        }
    }
}
}   //  end detail

}

template class beltpp::processor<beltpp::ctask>;
