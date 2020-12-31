cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

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
    src/host/host_t_receive.cpp
    src/host/host_t_access.cpp
)

find_package(Threads)

add_executable(test test/test.cpp)
target_link_libraries(test kernel ${CMAKE_THREAD_LIBS_INIT})
