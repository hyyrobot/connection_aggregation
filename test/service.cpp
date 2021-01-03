#include "host_t.h"

#include <arpa/inet.h>
#include <string>
#include <thread>
#include <iostream>

int main(int argc, char *argv[])
{
    using namespace autolabor::connection_aggregation;

    std::cout << argc << " arguments:" << std::endl;
    for (auto i = 0; i < argc; ++i)
        std::cout << "  " << i << ": " << argv[i] << std::endl;

    if (argc != 3 && argc != 5)
    {
        std::cout << "argc must be 3 or 5!" << std::endl;
        return 1;
    }

    in_addr address;
    inet_pton(AF_INET, argv[2], &address);
    host_t host(argv[1], address);
    if (argc == 5)
    {
        auto index = std::stod(argv[3]);
        auto port = std::stod(argv[4]);
        while (!host.bind(index, port))
            std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    std::cout << host << std::endl;

    auto forwarding = std::thread([&host] {
        uint8_t buffer[65536];
        host.forward(buffer, sizeof(buffer));
    });
    auto receiving = std::thread([&host] {
        uint8_t buffer[65536];
        host.receive(buffer, sizeof(buffer));
    });

    while (true)
    {
        std::string _;
        std::getchar();
        std::cout << host << std::endl;
    }

    return 0;
}
