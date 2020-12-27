#include "../program_t.h"

#include "../protocol.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <cstring>

namespace autolabor::connection_aggregation
{
    size_t program_t::receive(ip *header, uint8_t *buffer, size_t size)
    {
        // 构造接收结构
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
        // 接收
        const auto n = recvmsg(_receiver, &msg, 0);
        if (header->ip_p != IPPROTO_MINE)
        {
            // std::cout << +header->ip_p << std::endl;
            return 0;
        }
        // 创建连接
        add_remote(common.host, common.src_index, header->ip_src);
        // 即时回应
        std::vector<connection_key_t> connections;
        {
            READ_GRAUD(_connection_mutex);
            connection_key_union reverse;
            reverse.src_index = common.dst_index;
            reverse.dst_index = common.src_index;
            _connections
                .at(common.host.s_addr)
                .at(reverse.key)
                .received_once(*buffer);
            for (const auto &[key, info] : _connections.at(common.host.s_addr))
                if (info.state() < 2)
                    connections.push_back(key);
        }
        for (auto key : connections)
            send_handshake(common.host, key);
        // 返回 buffer 中有用的长度
        return n - sizeof(ip) - sizeof(common);
    }

} // namespace autolabor::connection_aggregation
