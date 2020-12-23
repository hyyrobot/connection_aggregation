#include "program_t.h"

#include <arpa/inet.h>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.1", &address);
    program_t program("robot", address);

    while (true)
        ;

    return 0;
}
