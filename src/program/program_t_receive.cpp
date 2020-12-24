#include "../program_t.h"

#include "../protocol.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <cstring>

std::ostream &operator<<(std::ostream &stream, const ip &ip_pack)
{
    stream << "=================" << std::endl
           << "IP" << std::endl
           << "=================" << std::endl
           << "verson:          " << ip_pack.ip_v << std::endl
           << "len_head:        " << ip_pack.ip_hl << std::endl
           << "service type:    " << +ip_pack.ip_tos << std::endl
           << "len_pack:        " << ip_pack.ip_len << std::endl
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
            return 0;
        // 创建连接
        add_remote(common.host, common.src_index, header->ip_src);
        // 即时回应
        std::vector<connection_key_t> connections;
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
        // 显示（临时）
        // char text[32];
        // inet_ntop(AF_INET, &common.host, text, sizeof(text));
        // std::cout << text << '(' << common.src_index << ") -> ";

        // auto address = _tun.address();
        // inet_ntop(AF_INET, &address, text, sizeof(text));
        // std::cout << text << '(' << common.dst_index << ')' << std::endl;
        if (reinterpret_cast<pack_type_t *>(buffer)->forward)
        {
            auto extra = reinterpret_cast<forward_t *>(buffer);
            header->ip_hl = 5;
            header->ip_len = ntohs(header->ip_len) - sizeof(common_extra_t) - sizeof(forward_t);
            header->ip_p = extra->protocol;
            header->ip_off = extra->offset;
            header->ip_src = extra->src;
            header->ip_dst = extra->dst;

            std::vector<uint8_t> temp(header->ip_len);
            header->ip_len = htons(header->ip_len);
            *reinterpret_cast<ip *>(temp.data()) = *header;
            std::copy(buffer + sizeof(extra), buffer + temp.size() - sizeof(ip), temp.begin() + sizeof(ip));
            // std::cout << *header << std::endl;
            std::cout << write(_tun.socket(), temp.data(), temp.size()) << std::endl;
        }
        // 返回 buffer 中有用的长度
        return n - sizeof(ip) - sizeof(common);
    }

} // namespace autolabor::connection_aggregation
