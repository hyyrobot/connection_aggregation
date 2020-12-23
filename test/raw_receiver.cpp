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
    inet_pton(AF_INET, "10.0.0.1", &address);
    program_t program("user", address);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(.1s);

    std::cout << program << std::endl;

    unsigned char buffer[1024];

    while (true)
    {
        auto n = read(program.receiver(), buffer, sizeof(buffer));
        auto header = reinterpret_cast<ip *>(buffer);
        if (header->ip_p != 6 && header->ip_p != 17)
        {
            std::cout << *header << std::endl;
            std::cout << program;
        }
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
           << "len_pack:        " << ip_pack.ip_len << std::endl
           << "-----------------" << std::endl
           << "pack_id:         " << (ip_pack.ip_id & 0x1fu) << std::endl
           << "fragment offset: " << ip_pack.ip_off << std::endl
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