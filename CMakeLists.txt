cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include_directories(src)

add_library(kernel
    src/functions.h
    src/functions.cpp

    src/fd_guard_t.h
    src/fd_guard_t.cpp

    src/net_device_t.h
    src/net_device_t.cpp

    src/tun_device_t.h
    src/tun_device_t.cpp
    
    src/connection_info_t.h
    src/connection_info_t.cpp

    src/program_t.h
    src/program/program_t_constructor.cpp
    src/program/program_t_local.cpp
    src/program/program_t_remote.cpp
    src/program/program_t_access.cpp
    src/program/program_t_send.cpp
    src/program/program_t_receive.cpp
)

find_package(Threads)

add_executable(test test/test.cpp)
target_link_libraries(test kernel ${CMAKE_THREAD_LIBS_INIT})

add_executable(send test/raw_sender.cpp)
target_link_libraries(send kernel ${CMAKE_THREAD_LIBS_INIT})

add_executable(receive test/raw_receiver.cpp)
target_link_libraries(receive kernel ${CMAKE_THREAD_LIBS_INIT})
