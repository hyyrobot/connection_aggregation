#include "program_t.h"
#include "functions.h"

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;
    using namespace std::chrono_literals;

    constexpr static auto text = "Hello";
    const auto L = sizeof(ip) + sizeof(udphdr) + std::strlen(text) + 1;

    in_addr address;
    inet_pton(AF_INET, "10.0.0.1", &address);
    program_t program("user", address);

    std::thread([tun = program.tun(), address] {
        in_addr local;
        inet_pton(AF_INET, "10.0.0.2", &local);

        uint8_t buffer[128];
        auto ip_ = reinterpret_cast<ip *>(buffer);
        auto udp_ip_ = reinterpret_cast<udp_ip_t *>(buffer + udp_ip_offset);
        auto udp_ = reinterpret_cast<udphdr *>(buffer + sizeof(ip));
        auto data = reinterpret_cast<char *>(buffer + sizeof(ip) + sizeof(udphdr));

        std::strcpy(data, text);
        // 填充 udp 首部
        udp_->source = 0;                                      // 源端口，可以为 0
        udp_->dest = 9999;                                     // 目的端口
        udp_->len = htons(sizeof(udphdr) + std::strlen(text)); // 段长度 = 网络字节序(udp 头长度 + 数据长度)
        udp_->check = 0;                                       // 清零校验和段
        // 填充 udp 伪首部
        *udp_ip_ = {
            .length = udp_->len,     // 再来一遍长度
            .protocol = IPPROTO_UDP, // udp 协议号
            .src = local,            // 源 ip 地址
            .dst = address,          // 目的 ip 地址
        };
        fill_checksum_udp(udp_ip_);

        *ip_ = {
            .ip_hl = 5,
            .ip_v = 4,
            .ip_len = htons(L),
            .ip_ttl = 64,
            .ip_p = IPPROTO_UDP,
            .ip_src = local,
            .ip_dst = address,
        };
        fill_checksum_ip(ip_);

        while (true)
        {
            write(tun, buffer, L);
            std::this_thread::sleep_for(1s);
        }
    }).detach();

    sockaddr_in local{
        .sin_family = AF_INET,
        .sin_port = 9999,
        .sin_addr = address,
    };
    fd_guard_t udp(socket(AF_INET, SOCK_DGRAM, 0));
    bind(udp, (sockaddr *)&local, sizeof(local));

    uint8_t buffer[128];
    while (true)
    {
        auto n = read(udp, buffer, sizeof(buffer));
        if (n > 0)
            std::cout << buffer << std::endl;
    }

    return 0;
}
