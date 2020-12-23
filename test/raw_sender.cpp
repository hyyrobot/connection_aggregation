#include "program_t.h"

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

std::ostream &operator<<(std::ostream &, const ip &);

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address0, address1;
    inet_pton(AF_INET, "10.0.0.1", &address0);
    program_t program("user", address0);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    inet_pton(AF_INET, "10.0.0.2", &address0);
    inet_pton(AF_INET, "192.168.18.186", &address1);
    program.add_remote(address0, 2, address1);

    inet_pton(AF_INET, "10.0.0.2", &address0);
    inet_pton(AF_INET, "8.8.8.8", &address1);
    program.add_remote(address0, 4, address1);

    std::cout << program;

    while (true)
        ;

    return 0;
}
