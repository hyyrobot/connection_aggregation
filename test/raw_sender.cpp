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

    net_devices_t devices;
    in_addr address_remote;
    inet_pton(AF_INET, "192.168.18.183", &address_remote);
    unsigned id = 0;
    while (true)
    {
        auto result = devices.send_to((const uint8_t *)"Hello", 6, address_remote, ++id);
        std::cout << id << ": " << std::endl;
        for (auto pair : result)
            std::cout << '<' << devices[pair.first] << ": " << pair.second << '>' << std::endl;

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
