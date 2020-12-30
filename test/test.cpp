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
    std::this_thread::sleep_for(1s);
    std::cout << host << std::endl;
    return 0;
}
