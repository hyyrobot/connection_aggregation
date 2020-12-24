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

    in_addr address;
    inet_pton(AF_INET, "10.0.0.2", &address);
    program_t program("user", address);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    std::cout << program << std::endl;

    ip header;
    unsigned char buffer[1024];
    char text[32];

    while (true)
    {
        if (program.receive(&header, buffer, sizeof(buffer)))
            std::cout << header << std::endl
                      << std::endl
                      << program;
    }

    return 0;
}

std::ostream &operator<<(std::ostream &stream, const ip &ip_pack)
{
    stream << "=================" << std::endl
           << "IP" << std::endl
           << "=================" << std::endl
           << "verson:          " << ip_pack.ip_v << std::endl
           << "len_head:        " << ip_pack.ip_hl << std::endl
           << "service type:    " << +ip_pack.ip_tos << std::endl
           << "len_pack:        " << ntohs(ip_pack.ip_len) << std::endl
           << "-----------------" << std::endl
           << "id:              " << ip_pack.ip_id << std::endl
           << "fragment type:   " << (ip_pack.ip_off >> 13) << std::endl
           << "fragment offset: " << (ip_pack.ip_off & ~0xe000) << std::endl
           << "ttl:             " << +ip_pack.ip_ttl << std::endl
           << "protocol:        " << +ip_pack.ip_p << std::endl
           << "check sum:       " << ip_pack.ip_sum << std::endl
           << "-----------------" << std::endl;
    char text[17];
    inet_ntop(AF_INET, &ip_pack.ip_src, text, sizeof(text));
    stream << "source:          " << text << std::endl;
    inet_ntop(AF_INET, &ip_pack.ip_dst, text, sizeof(text));
    return stream << "destination:     " << text;
}