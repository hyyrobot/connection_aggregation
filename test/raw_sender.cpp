#include <netinet/ip.h>
#include <arpa/inet.h>

#include "net_devices_t.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address;
    inet_pton(AF_INET, "100.100.100.100", &address);
    net_devices_t devices("robot", address);
    const char *text = "hello";
    ip header{
        .ip_hl = 5,
        .ip_v = 4,
        .ip_len = 26,
        .ip_ttl = 64,
        .ip_p = 77,
    };
    uint8_t id = 0;
    inet_pton(AF_INET, "192.168.18.183", &header.ip_dst);
    while (true)
    {
        std::cout << +id << ": " << devices.send(header, id++, (const uint8_t *)text) << std::endl;

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
