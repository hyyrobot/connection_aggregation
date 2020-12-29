#include "program_t.h"

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;
    using namespace std::chrono_literals;

    in_addr address0, address1;
    inet_pton(AF_INET, "10.0.0.1", &address0);
    program_t program("user", address0);

    std::this_thread::sleep_for(.1s);

    inet_pton(AF_INET, "10.0.0.2", &address0);
    // inet_pton(AF_INET, "188.131.141.243", &address1);
    inet_pton(AF_INET, "192.168.18.204", &address1);
    program.add_remote(address0, 2, address1);
    program.send_handshake(address0);

    std::this_thread::sleep_for(.2s);
    std::cout << program;

    std::thread([&program] {
        unsigned char buffer[1024];
        while (true)
        {
            program.receive(buffer, sizeof(buffer));
            std::cout << program << std::endl;
        }
    }).detach();

    std::thread([&program, address0] {
        uint8_t buffer[2048];
        while (true)
        {
            auto n = read(program.tun(), buffer, sizeof(buffer));
            if (n > 0)
                switch (reinterpret_cast<const ip *>(buffer)->ip_p)
                {
                case IPPROTO_ICMP:
                case IPPROTO_TCP:
                case IPPROTO_UDP:
                    program.forward(address0, buffer, n);
                    break;
                }
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
        sendto(udp, "Hello", 6, 0, (sockaddr *)&remote, sizeof(remote));
        std::this_thread::sleep_for(1s);
    }

    return 0;
}
