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

    std::this_thread::sleep_for(.2s);
    std::cout << host << std::endl;

    host.bind(2, 9999);

    uint8_t buffer[2048];
    while (true)
        if (host.receive(buffer, sizeof(buffer)))
            std::cout << host << std::endl;

    return 0;
}
