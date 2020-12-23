#include "program_t.h"

#include <arpa/inet.h>
#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.1", &address);
    program_t program("robot", address);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    std::cout << program << std::endl;

    while (true)
        ;

    return 0;
}
