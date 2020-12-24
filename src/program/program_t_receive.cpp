#include "../program_t.h"

#include <arpa/inet.h>
#include <vector>
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

        std::vector<connection_key_t> connections;
        add_remote(common.host, common.src_index, header->ip_src);
        {
            READ_GRAUD(_connection_mutex);
            _connections
                .at(common.host.s_addr)
                .at(reinterpret_cast<connection_key_union *>(&common.src_index)->key)
                .received_once(*buffer);
            for (const auto &[key, info] : _connections.at(common.host.s_addr))
                if (info.state() < 2)
                    connections.push_back(key);
        }
        for (auto key : connections)
            send_handshake(common.host, key);

        char text[32];
        inet_ntop(AF_INET, &common.host, text, sizeof(text));
        std::cout << text << '(' << common.src_index << ") -> ";

        auto address = _tun.address();
        inet_ntop(AF_INET, &address, text, sizeof(text));
        std::cout << text << '(' << common.dst_index << ')' << std::endl;

        return n - sizeof(ip) - sizeof(common_extra_t);
    }

} // namespace autolabor::connection_aggregation
