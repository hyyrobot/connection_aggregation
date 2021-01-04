cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Release")

include_directories(src)

add_library(kernel
    # 产生能定位的异常
    src/ERRNO_MACRO.h
    # RAII 文件描述符
    src/fd_guard_t.h
    src/fd_guard_t.cpp
    # 本机网卡
    src/device_t.h
    src/device_t.cpp
    # 连接
    src/connection_t.h
    src/connection_t.cpp
    # 主机
    src/host_t.h
    src/host/host_t_constructor.cpp
    src/host/host_t_local.cpp
    src/host/host_t_remote.cpp
    src/host/host_t_send.cpp
    src/host/host_t_receive.cpp
    src/host/host_t_access.cpp
)

find_package(Threads)

add_executable(service test/service.cpp
    test/cin_to_list.h
    test/cin_to_list.cpp
)
target_link_libraries(service kernel ${CMAKE_THREAD_LIBS_INIT})

add_executable(send test/udp_sender.cpp)
add_executable(receive test/udp_receiver.cpp)
