#include <netinet/ip.h>
#include <arpa/inet.h>

#include "net_devices_t.h"
#include <iostream>
#include <cstring>
#include <sstream>

std::ostream &operator<<(std::ostream &, const ip *);

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address;
    inet_pton(AF_INET, "100.100.100.100", &address);
    net_devices_t devices("user", address);

    unsigned char buffer[32];
    ip header{
        .ip_hl = 5,
        .ip_v = 4,
        .ip_ttl = 64,
        .ip_dst = devices.tun_address(),
    };
    while (1)
    {
        auto [msg, index] = devices.receive_from(buffer, sizeof(buffer));
        if (index <= 0)
            continue;
        header.ip_len = 20 + msg.size;
        header.ip_p = msg.protocol;
        header.ip_src = msg.remote;
        std::cout << &header << std::endl
                  << '[' << msg.id << "]: \"" << buffer
                  << "\" from " << devices[index] << std::endl;
    }
}

std::ostream &operator<<(std::ostream &stream, const in_addr ip)
{
    auto temp = reinterpret_cast<const uint8_t *>(&ip);
    stream << +temp[0] << '.'
           << +temp[1] << '.'
           << +temp[2] << '.'
           << +temp[3];
    return stream;
}

std::ostream &operator<<(std::ostream &stream, const ip *ip_pack)
{
    stream << "=================" << std::endl
           << "IP" << std::endl
           << "=================" << std::endl
           << "verson:          " << ip_pack->ip_v << std::endl
           << "len_head:        " << ip_pack->ip_hl << std::endl
           << "service type:    " << +ip_pack->ip_tos << std::endl
           << "len_pack:        " << ip_pack->ip_len << std::endl
           << "-----------------" << std::endl
           << "pack_id:         " << (ip_pack->ip_id & 0x1fu) << std::endl
           << "fragment offset: " << ip_pack->ip_off << std::endl
           << "ttl:             " << +ip_pack->ip_ttl << std::endl
           << "protocol:        " << +ip_pack->ip_p << std::endl
           << "check sum:       " << ip_pack->ip_sum << std::endl
           << "-----------------" << std::endl
           << "source:          " << ip_pack->ip_src << std::endl
           << "destination:     " << ip_pack->ip_dst;
    return stream;
}