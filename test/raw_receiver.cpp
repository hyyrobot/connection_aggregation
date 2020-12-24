#include "program_t.h"

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.2", &address);
    program_t program("user", address);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    std::cout << program << std::endl;

    char text[32];

    std::thread([&program] {
        ip header;
        unsigned char buffer[1024];
        while (true)
            program.receive(&header, buffer, sizeof(buffer));
    }).detach();

    unsigned char buffer[1024];
    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    sockaddr_in port{
        .sin_family = AF_INET,
        .sin_port = 100,
    };
    bind(udp, (sockaddr *)&port, sizeof(port));
    while (true)
    {
        recv(udp, buffer, sizeof(buffer), MSG_WAITALL);
        std::cout << buffer << std::endl;
    }

    return 0;
}
