add_definitions(-DPROCESSOR_LIBRARY)

# define the library
add_library(processor
    global.hpp
    processor.cpp
    processor.hpp)

# libraries this module links to
target_link_libraries(processor
    PUBLIC
        belt.pp
    PRIVATE
        packet)

if(NOT WIN32 AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(processor PRIVATE Threads::Threads)
endif()

# what to do on make install
install(TARGETS processor
        EXPORT belt.pp.package
        RUNTIME DESTINATION ${BELTPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${BELTPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${BELTPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    processor.hpp
    DESTINATION ${BELTPP_INSTALL_DESTINATION_INCLUDE}/libprocessor)
