#include "native.hpp"
#include "event_impl.hpp"

namespace beltpp
{
namespace native
{
#ifdef B_OS_WINDOWS
bool get_connected_status(beltpp::detail::event_handler_impl* peh,
                          uint64_t id,
                          SOCKET socket_descritor,
                          int& error_code)
{
    auto& async_data = peh->m_poll_master.m_events.at(id);
    error_code = async_data->last_error;
    int res = (0 == error_code ? 0 : -1);

    if (0 == res)
    {
        int res2 = ::setsockopt(socket_descritor,
            SOL_SOCKET,
            SO_UPDATE_CONNECT_CONTEXT,
            NULL,
            0);

        if (res2 != NO_ERROR)
            throw std::runtime_error("setsockopt(): " + native::net_last_error());
    }

    return res == 0;
}
#endif
#ifdef B_OS_WINDOWS
SOCKET accept(beltpp::detail::event_handler_impl* peh,
              uint64_t id,
              SOCKET socket_descriptor,
              bool& res,
              int& error_code)
{
    auto& async_data = peh->m_poll_master.m_events.at(id);

    error_code = async_data->last_error;
    res = (0 == error_code ? true : false);
    SOCKET result = INVALID_SOCKET;

    if (res)
    {
        result = async_data->accept_socket;

        int res2 = ::setsockopt(result,
            SOL_SOCKET,
            SO_UPDATE_ACCEPT_CONTEXT,
            (char const*)(&socket_descriptor),
            sizeof(SOCKET));

        if (res2 != NO_ERROR)
            throw std::runtime_error("setsockopt(): " + native::net_last_error());
    }

    async_data->accept_socket = INVALID_SOCKET;
    async_data->init();
    return result;
}
#else
int accept(beltpp::detail::event_handler_impl*,
           uint64_t,
           int socket_descriptor,
           bool& res,
           int& error_code)
{
    sockaddr_storage address;
    socklen_t size = sizeof(sockaddr_storage);
    res = true;
    error_code = 0;
    return ::accept(socket_descriptor, reinterpret_cast<sockaddr*>(&address), &size);
}
#endif

#ifdef B_OS_WINDOWS
size_t recv(beltpp::detail::event_handler_impl* peh,
            uint64_t id,
            SOCKET,
            char* buf,
            size_t len,
            int& error_code)
{
    auto& async_data = peh->m_poll_master.m_events.at(id);
    if (0 != async_data->last_error)
    {
        error_code = async_data->last_error;
        return size_t(SOCKET_ERROR);
    }
    else
    {
        if (len > async_data->bytes_copied - async_data->bytes_offset)
            len = async_data->bytes_copied - async_data->bytes_offset;

        memcpy(buf, async_data->receive_buffer + async_data->bytes_offset, len);

        async_data->bytes_offset += (DWORD)len;
        if (async_data->bytes_copied == async_data->bytes_offset)
        {
            async_data->async_task_running &= ~async_data->async_task_running_receive;
            async_data->init();
        }
        else
            async_data->async_task_running |= async_data->async_task_running_receive;

        return len;
    }
}
#else
size_t recv(beltpp::detail::event_handler_impl*,
            uint64_t,
            int socket_descriptor,
            char* buf,
            size_t len,
            int& error_code)
{
    auto result = ::recv(socket_descriptor, reinterpret_cast<void*>(buf), len, 0);
    error_code = errno;

    return size_t(result);
}
#endif
#ifdef B_OS_WINDOWS

void async_send(SOCKET socket_descriptor,
                beltpp::detail::event_handler_impl* peh,
                ievent_item& ev_it,
                uint64_t ev_id,
                uint64_t eh_id,
                beltpp::queue<char>& send_stream,
                std::string const& message)
{
    auto& async_data = peh->m_poll_master.m_events.at(eh_id);

    bool stream_was_empty = send_stream.empty();

    for (char const& ch : message)
        send_stream.push(ch);

    auto message_copy = message;
    if (message_copy.length() > 4 * 1024)
        message_copy.resize(4 * 1024);

    if (stream_was_empty)
    {
        strcpy(async_data->send_buffer, message_copy.c_str());
        async_data->action = event_handler::task::send;
    }
}

/*size_t send(SOCKET socket_descriptor, char const* buf, size_t size)
{
    int res = ::send(socket_descriptor, buf, (int)size, 0);
    if (-1 == res &&
        WSAEWOULDBLOCK == ::WSAGetLastError())
        res = 0;

    return size_t(res);
}*/

int send(beltpp::detail::event_handler_impl* peh,
         ievent_item& /*ev_it*/,
         uint64_t /*ev_id*/,
         uint64_t eh_id,
         beltpp::queue<char>& send_stream,
         int& error_code)
{
    auto& async_data = peh->m_poll_master.m_events.at(eh_id);

    if (async_data->result_receive_send)
        return 0;
    if (0 != async_data->last_error)
    {
        error_code = async_data->last_error;
        return size_t(SOCKET_ERROR);
    }

    size_t sent = async_data->bytes_copied;

    for (size_t index = 0; index < sent; ++index)
    {
        assert(false == send_stream.empty());
        send_stream.pop();
    }

    if (sent &&
        false == send_stream.empty())
    {
        std::string message(send_stream.cbegin(), send_stream.cend());
        if (message.length() > 4 * 1024)
        {
            message.resize(4 * 1024);
            strcpy(async_data->send_buffer, message.c_str());
            async_data->action = event_handler::task::send;
        }
    }

    return sent;
}

void connect(beltpp::detail::event_handler_impl* peh,
             uint64_t id,
             SOCKET /*fd*/,
             const struct sockaddr* addr,
             size_t len,
             beltpp::ip_address const& address,
             sync_result& result)
{
    auto& async_data = peh->m_poll_master.m_events.at(id);
    async_data->connect(addr, len, address);
    B_UNUSED(result);
}
#else
void async_send(int socket_descriptor,
                beltpp::detail::event_handler_impl* peh,
                ievent_item& ev_it,
                uint64_t ev_id,
                uint64_t eh_id,
                beltpp::queue<char>& send_stream,
                std::string const& message)
{
    if (send_stream.empty())
    {
        peh->remove(socket_descriptor,
                    eh_id,
                    false,   //  already_closed
                    event_handler::task::receive,
                    true);

        peh->add(ev_it,
                 socket_descriptor,
                 ev_id,
                 event_handler::task::send,
                 true,
                 eh_id);
    }

    for (char const& ch : message)
        send_stream.push(ch);
}

size_t send(int socket_descriptor,
            beltpp::detail::event_handler_impl* peh,
            ievent_item& ev_it,
            uint64_t ev_id,
            uint64_t eh_id,
            beltpp::queue<char>& send_stream,
            int& error_code)
{
    string str_send_buffer(send_stream.cbegin(),
                           send_stream.cend());
    ssize_t res = 0;
    if (false == str_send_buffer.empty())
    {
        res = ::send(socket_descriptor,
                     reinterpret_cast<void const*>(str_send_buffer.c_str()),
                     str_send_buffer.size(),
                     MSG_NOSIGNAL);
        if (-1 == res &&
            (
                EWOULDBLOCK == errno ||
                EAGAIN == errno
            ))
            res = 0;
        else if (-1 == res)
            error_code = errno;

        if (-1 != res)
        {
            for (size_t index = 0; index < size_t(res); ++index)
            {
                assert(false == send_stream.empty());
                send_stream.pop();
            }

            if (send_stream.empty())
            {
                //  send buffer cleared
                peh->remove(socket_descriptor,
                            eh_id,
                            false,   //  already_closed
                            event_handler::task::send,
                            true);

                peh->add(ev_it,
                         socket_descriptor,
                         ev_id,
                         event_handler::task::receive,
                         true,
                         eh_id);
            }
        }
    }

    return size_t(res);
}
#endif

}
}
