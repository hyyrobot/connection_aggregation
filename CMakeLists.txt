cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include_directories(src)

add_library(kernel 
    src/fd_guard_t.h
    src/fd_guard_t.cpp
    
    src/net_device_t.h
    src/net_device_t.cpp

    src/net_devices_t.h
    src/net_devices_t_constructor.cpp
    src/net_devices_t_others.cpp
)

add_executable(send test/raw_sender.cpp)
target_link_libraries(send kernel)

add_executable(receive test/raw_receiver.cpp)
target_link_libraries(receive kernel)
