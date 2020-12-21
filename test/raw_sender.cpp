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

    net_devices_t devices("robot");
    in_addr address_remote;
    inet_pton(AF_INET, "192.168.18.183", &address_remote);
    for (unsigned id = 0;; ++id)
    {
        std::cout << id << ": " << std::endl;
        for (auto [index, size] : devices.send_to((const uint8_t *)"Hello", 6, address_remote, id))
            std::cout << '<' << devices[index] << ": " << size << '>' << std::endl;

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
