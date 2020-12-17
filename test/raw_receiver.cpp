#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/ip.h>
#include <unistd.h>

#include "common/fd_guard_t.h"
#include <iostream>
#include <cstring>
#include <sstream>

std::ostream &operator<<(std::ostream &, const ip *);

int main()
{
    using namespace autolabor::connection_aggregation;
    
    const fd_guard_t fd(socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP)));

    ip header;
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
        auto size = recvmsg(fd, &msg, 0);
        if (header.ip_p != 64)
            continue;
        std::cout << &header << std::endl
                  << extra.host << '[' << extra.id << "]: "
                  << buffer << std::endl;
    }
}

std::string ip_address_text(in_addr ip)
{
    std::stringstream builder;
    uint8_t *temp = reinterpret_cast<uint8_t *>(&ip);
    builder << +temp[0] << '.'
            << +temp[1] << '.'
            << +temp[2] << '.'
            << +temp[3];
    return builder.str();
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
           << "source:          " << ip_address_text(ip_pack->ip_src) << std::endl
           << "destination:     " << ip_address_text(ip_pack->ip_dst);
    return stream;
}