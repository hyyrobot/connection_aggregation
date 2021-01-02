#include "host_t.h"

#include <arpa/inet.h>

#include <iostream>
#include <thread>

int main()
{
    using namespace autolabor::connection_aggregation;
    using namespace std::chrono_literals;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.2", &address);
    host_t host("server", address);
    while (!host.bind(4, 9999))
        ;

    std::cout << host << std::endl;

    std::thread([&host] {
        uint8_t buffer[2048];
        host.receive(buffer, sizeof(buffer));
    }).detach();

    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    sockaddr_in local{
        .sin_family = AF_INET,
        .sin_port = 12345,
    };
    bind(udp, (sockaddr *)&local, sizeof(local));
    while (true)
    {
        uint8_t buffer[32];
        recv(udp, buffer, sizeof(buffer), MSG_WAITALL);
        std::cout << buffer << std::endl;
    }

    return 0;
}
