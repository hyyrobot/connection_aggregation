#include "../program_t.h"

#include "../protocol.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <cstring>

namespace autolabor::connection_aggregation
{
    size_t program_t::receive(uint8_t *buffer, size_t size)
    {
        // 构造接收结构
        sockaddr_in remote{.sin_family = AF_INET};
        ip header;
        common_extra_t common;
        iovec iov[]{
            {.iov_base = &header, .iov_len = sizeof(ip)},
            {.iov_base = &common, .iov_len = sizeof(common_extra_t)},
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
        if (header.ip_p != IPPROTO_MINE)
            return 0;
        // 创建连接
        add_remote(common.host, common.src_index, header.ip_src);
        // 即时回应
        {
            connection_key_union reverse; // 交换源序号和目的序号
            reverse.src_index = common.dst_index;
            reverse.dst_index = common.src_index;
            READ_GRAUD(_connection_mutex);
            _connections
                .at(common.host.s_addr)
                .at(reverse.key)
                .received_once(*buffer);
        }
        send_handshake(common.host);
        auto extra = reinterpret_cast<const extra_t *>(buffer);
        auto ip_ = reinterpret_cast<const ip *>(extra + 1);
        auto ip_len = ntohs(ip_->ip_len);
        if (extra->type.forward && ip_->ip_dst.s_addr == address().s_addr)
            return write(_tun.socket(), ip_, ip_len);
        return 0;
    }

} // namespace autolabor::connection_aggregation
