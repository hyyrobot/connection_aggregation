#include <linux/if_packet.h>
#include <netinet/ip.h>

#include "net_devices_t.h"
#include <iostream>
#include <cstring>
#include <sstream>

std::ostream &operator<<(std::ostream &, const ip *);

int main()
{
    using namespace autolabor::connection_aggregation;

    const net_devices_t devices;

    ip header{};
    struct
    {
        char host[14];
        uint16_t id;
    } extra;
    unsigned char buffer[32];
    iovec iov[]{
        {.iov_base = &header, .iov_len = sizeof(header)},
        {.iov_base = &extra, .iov_len = sizeof(extra)},
        {.iov_base = &buffer, .iov_len = sizeof(buffer)},
    };
    sockaddr_ll remote;
    msghdr msg{
        .msg_name = &remote,
        .msg_namelen = sizeof(remote),
        .msg_iov = iov,
        .msg_iovlen = sizeof(iov) / sizeof(iovec),
    };
    while (1)
    {
        auto size = devices.receive(&msg);
        switch (header.ip_p)
        {
        case 0:  // ipv6 hbh
        case 1:  // icmp
        case 2:  // igmp
        case 4:  // ip in ip
        case 6:  // tcp
        case 17: // udp
            break;

        default:
        {
            std::cout << &header << std::endl
                      << extra.host << '[' << extra.id << "]: \"" << buffer
                      << "\" from " << devices[remote.sll_ifindex] << std::endl;
            break;
        }
        }
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
           << "len_pack:        " << ntohs(ip_pack->ip_len) << std::endl
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