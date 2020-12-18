cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include_directories(src)

add_executable(send test/raw_sender.cpp
    src/common/fd_guard_t.h
    src/common/fd_guard_t.cpp
    
    src/net_device/net_devices_t.h
    src/net_device/net_devices_t.cpp
)

add_executable(receive test/raw_receiver.cpp
    src/common/fd_guard_t.h
    src/common/fd_guard_t.cpp
    
    src/net_device/net_devices_t.h
    src/net_device/net_devices_t.cpp
)
