#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>

// struct ip
// https://stackoverflow.com/questions/42840636/difference-between-struct-ip-and-struct-iphdr
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <arpa/inet.h>

// read
#include <unistd.h>

#include "netlink.h"
#include "ip_table_t.h"

std::ostream &operator<<(std::ostream &, const ip *);
std::ostream &operator<<(std::ostream &, const network_info_t &);

constexpr static auto IPPROTO_MINE = 64;

int main()
{
    char tun_name[IFNAMSIZ]{};       // tun 设备名
    int tun_index;                   // tun 设备序号
    std::vector<fd_guard_t> sockets; // 一些网络操作符
    ip_table_t ip_table;             // 主机-地址对应表

    fd_guard_t fd_tun;
    { // 通过 rtnetlink 机制启动网卡并添加 ip 地址
        // 顺序不能变：
        // 1. 绑定 rtnetlink，以接收网卡变动的通知
        // 2. 注册 tun 设备
        // 3. 读取变动通知中给出的 tun network interface index
        fd_guard_t fd_netlink = bind_netlink();
        fd_tun = register_tun(tun_name);
        config_tun(fd_netlink, tun_index = read_link(fd_netlink, tun_name));
        std::cout << "created `" << tun_name << "` at index `" << tun_index << '`' << std::endl;

        // 构造真实网卡套接字
        auto networks = list_networks(fd_netlink);
        for (auto p = networks.begin(); p != networks.end();)
            if (p->first == 1 || p->first == tun_index)
                p = networks.erase(p);
            else
                std::cout << p++->second << std::endl;

        sockets.resize(networks.size());
        constexpr static auto on = 1;
        auto p = networks.begin();
        for (auto i = 0; i < sockets.size(); ++i, ++p)
        {
            const auto fd = socket(AF_INET, SOCK_RAW, IPPROTO_MINE);                             // 建立一个网络层原始套接字
            setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));                             // 指定协议栈不再向发出的分组添加 ip 头
            setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, p->second.name, strlen(p->second.name)); // 绑定套接字到网卡
            sockaddr_in localaddr = {
                .sin_family = AF_INET,
                .sin_port = 0,
                .sin_addr = p->second.addresses.front().address,
            };
            bind(fd, (sockaddr *)&localaddr, sizeof(localaddr)); // 绑定套接字 ip 地址
            sockets[i] = fd;
        }
    }

    uint8_t buffer[2048u];
    uint16_t next_id = 0;
    while (true)
    {
        auto size = read(fd_tun, buffer, sizeof(buffer));
        std::cout << "size: " << size << std::endl;

        auto ip_pack = reinterpret_cast<ip *>(buffer);

        if (ip_pack->ip_v == 4)
        {
            std::cout << ip_pack << std::endl;
            auto payload = buffer + 4 * ip_pack->ip_hl;

            ip_pack->ip_hl += 2;
            ip_pack->ip_len += 8;
            ip_pack->ip_v = 0;
            ip_pack->ip_p = IPPROTO_MINE;
            ip_pack->ip_sum = 0;

            struct
            {
                char host[32];
                uint16_t id;
                uint8_t protocol;
                uint8_t zero;
            } expand{
                .id = next_id++,
                .protocol = ip_pack->ip_p,
            };
            std::strcpy(expand.host, "robot");

            iovec iov[]{
                {.iov_base = ip_pack, .iov_len = sizeof(ip)},
                {.iov_base = &expand, .iov_len = sizeof(expand)},
                {.iov_base = payload, .iov_len = size - sizeof(ip)},
            };
            sockaddr_in remote{
                .sin_family = AF_INET,
                .sin_addr = ip_pack->ip_dst,
            };
            msghdr msg{
                .msg_name = &remote,
                .msg_namelen = sizeof(remote),
                .msg_iov = iov,
                .msg_iovlen = sizeof(iov) / sizeof(iovec),
            };

            auto host = ip_table[ip_pack->ip_dst];
            if (host.empty())
                for (const auto &fd : sockets)
                    sendmsg(fd, &msg, 0);
            else
            {
                auto addresses = ip_table[host];
                for (auto address : addresses)
                {
                    ip_pack->ip_dst = in_addr{address};
                    for (const auto &fd : sockets)
                        sendmsg(fd, &msg, 0);
                }
            }
        }

        std::cout << std::endl;
    }

    return 0;
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

std::ostream &operator<<(std::ostream &stream, const network_info_t &network)
{
    stream << network.name << ':' << std::endl;
    for (auto item : network.addresses)
        std::cout << "  " << ip_address_text(item.address) << '/' << +item.subnet_length << std::endl;
    return stream;
}
