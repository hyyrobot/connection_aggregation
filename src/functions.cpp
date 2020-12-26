#include "functions.h"

#include <arpa/inet.h>

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

uint16_t check_sum(const void *data, size_t n)
{
    auto p = reinterpret_cast<const uint16_t *>(data);
    uint32_t sum = 0;

    for (; n > 1; n -= 2)
        sum += *p++;

    if (n)
        sum += *p;

    p = reinterpret_cast<uint16_t *>(&sum);
    sum = p[0] + p[1];
    sum = p[0] + p[1];

    return ~p[0];
}

void fill_checksum(ip *data)
{
    data->ip_sum = 0;
    data->ip_sum = check_sum(data, sizeof(ip));
}