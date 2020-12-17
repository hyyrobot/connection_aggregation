#include <netinet/in.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "common/fd_guard_t.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

int main()
{
    using namespace autolabor::connection_aggregation;

    constexpr static auto on = 1;
    constexpr static auto IPPROTO_MINE = 64;
    constexpr static auto link = "ens33";

    const fd_guard_t fd(socket(AF_INET, SOCK_RAW, IPPROTO_MINE));         // 建立一个网络层原始套接字
    setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));              // 指定协议栈不再向发出的分组添加 ip 头
    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, link, std::strlen(link)); // 绑定套接字到网卡

    in_addr address_local, address_remote;
    inet_pton(AF_INET, "192.168.18.176", &address_local);
    inet_pton(AF_INET, "192.168.18.177", &address_remote);
    sockaddr_in
        local{
            .sin_family = AF_INET,
            .sin_port = 0,
            .sin_addr = address_local,
        },
        remote{
            .sin_family = AF_INET,
            .sin_port = 0,
            .sin_addr = address_remote,
        };
    bind(fd, (sockaddr *)&local, sizeof(local)); // 绑定套接字 ip 地址

    ip header{
        .ip_hl = 5 + 4,
        .ip_v = 4,
        .ip_len = 10752,
        .ip_id = 4,
        .ip_off = 0,
        .ip_ttl = 64,
        .ip_p = IPPROTO_MINE,
        .ip_sum = 0,
        .ip_src = address_local,
        .ip_dst = address_remote,
    };
    struct
    {
        char host[14];
        uint16_t id;
    } extra{
        .id = 0,
    };
    std::strcpy(extra.host, "robot");
    iovec iov[]{
        {.iov_base = &header, .iov_len = sizeof(header)},
        {.iov_base = &extra, .iov_len = sizeof(extra)},
        {.iov_base = (void *)"Hello", .iov_len = 6},
    };
    msghdr msg{
        .msg_name = &remote,
        .msg_namelen = sizeof(remote),
        .msg_iov = iov,
        .msg_iovlen = sizeof(iov) / sizeof(iovec),
    };

    while (true)
    {
        ++extra.id;
        header.ip_sum = 0;

        uint32_t sum = 0;
        const uint16_t *temp = reinterpret_cast<uint16_t *>(&header);
        for (auto i = 0; i < 10; ++i)
            sum += *temp++;
        temp = reinterpret_cast<uint16_t *>(&extra);
        for (auto i = 0; i < 8; ++i)
            sum += *temp++;
        temp = reinterpret_cast<uint16_t *>(&sum);
        header.ip_sum = ~(temp[0] + temp[1]);

        std::cout << extra.id << ": " << header.ip_sum << ' ' << sendmsg(fd, &msg, 0) << std::endl;

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
