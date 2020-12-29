#include "../program_t.h"

#include "../protocol.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
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
        ip_extra_t common;
        extra_t extra;
        iovec iov[]{
            {.iov_base = &common, .iov_len = sizeof(ip_extra_t)},
            {.iov_base = &extra, .iov_len = sizeof(extra_t)},
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
        // 创建连接
        add_remote(common.host, common.src_index, remote.sin_addr);
        // 交换源序号和目的序号获得本地连接键
        connection_key_union reverse;
        reverse.src_index = common.dst_index;
        reverse.dst_index = common.src_index;
        auto upload = false;
        {
            READ_GRAUD(_connection_mutex);
            auto &srand = _connections.at(common.host.s_addr);
            srand.items.at(reverse.key).received_once(extra);
            if (extra.forward)
            {
                auto now = std::chrono::steady_clock::now();
                upload = srand.update_time_stamp(extra.id, now);
            }
        }
        // 即时回应
        send_handshake(common.host);
        return upload ? write(_tun.socket(), buffer, n - sizeof(ip_extra_t) - sizeof(extra_t)) : 0;
    }

} // namespace autolabor::connection_aggregation
