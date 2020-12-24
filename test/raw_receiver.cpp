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
    char text[32];

    while (true)
    {
        const auto n = read(program.receiver(), buffer, sizeof(buffer));
        const auto header = reinterpret_cast<ip *>(buffer);
        if (header->ip_p != IPPROTO_MINE)
            continue;

        const auto common = reinterpret_cast<common_extra_t *>(buffer + sizeof(ip));
        program.add_remote(common->host, common->connection.src_index, header->ip_src);

        inet_ntop(AF_INET, &common->host, text, sizeof(text));
        std::cout << *header << std::endl
                  << text << '(' << common->connection.src_index << ") -> " << common->connection.dst_index << std::endl
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