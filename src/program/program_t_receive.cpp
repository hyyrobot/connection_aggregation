#include "../program_t.h"

#include <arpa/inet.h>
#include <iostream>

namespace autolabor::connection_aggregation
{
    size_t program_t::receive(ip *header, uint8_t *buffer, size_t size)
    {
        sockaddr_in remote{.sin_family = AF_INET};
        common_extra_t common;
        iovec iov[]{
            {.iov_base = header, .iov_len = sizeof(ip)},
            {.iov_base = &common, .iov_len = sizeof(common)},
            {.iov_base = buffer, .iov_len = size},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        const auto n = recvmsg(_receiver, &msg, 0);
        if (header->ip_p != IPPROTO_MINE)
            return 0;

        add_remote(common.host, common.connection.src_index, header->ip_src);

        char text[32];
        inet_ntop(AF_INET, &common.host, text, sizeof(text));
        std::cout << text << '(' << common.connection.src_index << ") -> ";

        auto address = _tun.address();
        inet_ntop(AF_INET, &address, text, sizeof(text));
        std::cout << text << '(' << common.connection.dst_index << ')' << std::endl;

        return n - sizeof(ip) - sizeof(common_extra_t);
    }

} // namespace autolabor::connection_aggregation
