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
    actual_msg_t msg{
        .protocol = 77,
        .buffer = (uint8_t *)text,
        .size = 6,
    };
    inet_pton(AF_INET, "192.168.18.183", &msg.remote);
    while (true)
    {
        std::cout << msg.id << ": " << std::endl;
        for (auto [index, size] : devices.send_to(msg))
        {
            std::cout << '<' << devices[index] << ": " << size << '>' << std::endl;
            ++msg.id;
        }

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
