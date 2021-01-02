#include "host_t.h"

#include <arpa/inet.h>

#include <iostream>
#include <sstream>
#include <thread>

int main()
{
    using namespace autolabor::connection_aggregation;
    using namespace std::chrono_literals;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.1", &address);
    host_t host("user", address);

    in_addr ip;
    inet_pton(AF_INET, "10.0.0.2", &address);
    inet_pton(AF_INET, "192.168.100.3", &ip);
    host.add_remote(address, 9999, ip);
    std::this_thread::sleep_for(std::chrono::microseconds(500));

    std::cout << host << std::endl;
    host.send_handshake(address);

    std::thread([&host] {
        uint8_t buffer[2048];
        host.receive(buffer, sizeof(buffer));
    }).detach();

    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    sockaddr_in remote{
        .sin_family = AF_INET,
        .sin_port = 12345,
        .sin_addr = address,
    };

    for (auto i = 0;; ++i)
    {
        std::stringstream builder;
        builder << i << '\t' << "Hello, world!";
        auto text = builder.str();
        sendto(udp, text.c_str(), text.size() + 1, MSG_WAITALL, (sockaddr *)&remote, sizeof(remote));
        std::this_thread::sleep_for(.02s);
    }

    return 0;
}
