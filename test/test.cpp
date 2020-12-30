#include "host_t.h"

#include <arpa/inet.h>

#include <iostream>
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
    inet_pton(AF_INET, "192.168.1.1", &ip);
    host.add_remote(address, 9999, ip);
    std::this_thread::sleep_for(.2s);
    std::cout << host << std::endl;
    return 0;
}
