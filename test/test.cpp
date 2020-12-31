#include <sys/epoll.h>
#include <netinet/in.h>

#include <iostream>
#include <thread>

int main()
{
    auto udp0 = socket(AF_INET, SOCK_DGRAM, 0);
    auto udp1 = socket(AF_INET, SOCK_DGRAM, 0);
    auto udp2 = socket(AF_INET, SOCK_DGRAM, 0);
    auto epoll = epoll_create1(0);

    sockaddr_in local{.sin_family = AF_INET};
    epoll_event event{.events = EPOLLIN | EPOLLET};

    local.sin_port = 9997;
    bind(udp0, (sockaddr *)&local, sizeof(local));
    event.data.u32 = 0;
    std::cout << epoll_ctl(epoll, EPOLL_CTL_ADD, udp0, &event) << std::endl;

    local.sin_port = 9998;
    bind(udp1, (sockaddr *)&local, sizeof(local));
    event.data.u32 = 1;
    std::cout << epoll_ctl(epoll, EPOLL_CTL_ADD, udp1, &event) << std::endl;

    local.sin_port = 9999;
    bind(udp2, (sockaddr *)&local, sizeof(local));
    event.data.u32 = 2;
    std::cout << epoll_ctl(epoll, EPOLL_CTL_ADD, udp2, &event) << std::endl;

    std::thread([&] {
        while (true)
        {
            epoll_wait(epoll, &event, 1, -1);
            std::cout << event.data.u32 << std::endl;
        }
    }).detach();

    auto udp3 = socket(AF_INET, SOCK_DGRAM, 0);

    for (auto i = 0;; ++i)
    {
        using namespace std::chrono_literals;

        local.sin_port = 9999 - i % 3;
        std::cout << "  " << local.sin_port << std::endl;
        sendto(udp3, "Hello, world!", 14, MSG_WAITALL, (sockaddr *)&local, sizeof(local));
        std::this_thread::sleep_for(1s);
    }
}