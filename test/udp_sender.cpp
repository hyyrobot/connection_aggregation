#include <netinet/in.h>
#include <arpa/inet.h>

#include <chrono>
#include <thread>

#include "../src/fd_guard_t.h"

#include <iostream>

int main()
{
    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    sockaddr_in
        local{
            .sin_family = AF_INET,
            .sin_port = 999,
        },
        remote{
            .sin_family = AF_INET,
            .sin_port = 100,
        };
    inet_pton(AF_INET, "10.10.10.10", &local.sin_addr);
    inet_pton(AF_INET, "192.168.0.3", &remote.sin_addr);
    bind(udp, (sockaddr *)&local, sizeof(local));

    iovec iov{
        .iov_base = (void *)"Hello",
        .iov_len = 6,
    };
    msghdr msg{
        .msg_name = &remote,
        .msg_namelen = sizeof(remote),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    while (true)
    {
        using namespace std::chrono_literals;

        std::cout << sendmsg(udp, &msg, 0) << std::endl;
        std::this_thread::sleep_for(1s);
    }
    return 0;
}