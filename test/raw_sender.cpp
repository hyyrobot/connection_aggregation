#include "program_t.h"

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address0, address1;
    inet_pton(AF_INET, "10.0.0.1", &address0);
    program_t program("user", address0);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    inet_pton(AF_INET, "10.0.0.2", &address0);
    inet_pton(AF_INET, "192.168.100.2", &address1);
    program.add_remote(address0, 2, address1);

    std::cout << program;

    std::thread([&program, address0] {
        uint8_t buffer[2048];
        while (true)
        {
            auto n = read(program.tun(), buffer, sizeof(buffer));
            if (n <= 0)
            {
                std::cout << "n = " << n << std::endl;
                continue;
            }

            auto header = reinterpret_cast<const ip *>(buffer);
            if (header->ip_p != IPPROTO_UDP)
            {
                std::cout << "p = " << +header->ip_p << std::endl;
                continue;
            }

            program.forward(address0, header, buffer + sizeof(ip), n - sizeof(header));
            std::cout << program << std::endl;
        }
    }).detach();

    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    sockaddr_in remote{
        .sin_family = AF_INET,
        .sin_port = 100,
        .sin_addr = address0,
    };
    while (true)
    {
        std::cout << sendto(udp, "Hello", 6, 0, (sockaddr *)&remote, sizeof(remote)) << std::endl;
        std::this_thread::sleep_for(50ms);
    }

    return 0;
}
