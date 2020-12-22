cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(pm1_sdk LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include_directories(src)

add_library(kernel 
    src/fd_guard_t.h
    src/fd_guard_t.cpp

    src/net_device_t.h
    src/net_device_t.cpp

    src/tun_device_t.h
    src/program_t.h
)

# add_executable(receive test/raw_receiver.cpp)
# target_link_libraries(receive kernel)
