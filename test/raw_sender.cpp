#include "program_t.h"

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address0, address1;
    inet_pton(AF_INET, "10.0.0.1", &address0);
    program_t program("user", address0);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    inet_pton(AF_INET, "10.0.0.2", &address0);
    inet_pton(AF_INET, "192.168.18.189", &address1);
    program.add_remote(address0, 2, address1);

    std::cout << program;

    connection_key_union key;
    key.src_index = key.dst_index = 2;
    while (true)
    {
        std::cout << program.send_single(nullptr, 0, address0, key.key) << std::endl;

        std::this_thread::sleep_for(.5s);
    }
    return 0;
}
