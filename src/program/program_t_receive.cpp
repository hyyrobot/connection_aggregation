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
        ip header;
        ip_extra_t common;
        iovec iov[]{
            {.iov_base = &header, .iov_len = sizeof(ip)},
            {.iov_base = &common, .iov_len = sizeof(ip_extra_t)},
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
        // 交换源序号和目的序号获得本地连接键
        connection_key_union reverse;
        reverse.src_index = common.dst_index;
        reverse.dst_index = common.src_index;
        auto extra = reinterpret_cast<const extra_t *>(buffer);
        auto upload = false;
        {
            READ_GRAUD(_connection_mutex);
            auto &info = _connections.at(common.host.s_addr);
            info.items.at(reverse.key).received_once(*extra);
            if (extra->forward)
            {
                std::cout << extra->id << std::endl;
                using namespace std::chrono_literals;
                auto now = std::chrono::steady_clock::now();
                if (upload = now - info.received[extra->id] > 500ms)
                    info.received[extra->id] = now;
            }
        }
        // 即时回应
        send_handshake(common.host);
        auto ip_ = reinterpret_cast<const ip *>(extra + 1);
        auto ip_len = ntohs(ip_->ip_len);
        return upload ? write(_tun.socket(), ip_, ip_len) : 0;
    }

} // namespace autolabor::connection_aggregation
