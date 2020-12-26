#include "program_t.h"
#include "functions.h"

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    in_addr address0, address1;
    inet_pton(AF_INET, "10.0.0.1", &address0);
    program_t program("user", address0);
    const auto tun = program.tun();

    uint8_t buffer[128]{};
    while (true)
    {
        // 从 tun 读
        auto n = read(tun, buffer, sizeof(buffer));
        auto header = reinterpret_cast<ip *>(buffer);
        // 只响应 ICMP
        if (header->ip_p != IPPROTO_ICMP)
            continue;
        // 处理 IP 报头
        // offset 字段只能是 0 或 64
        // length 字段只能是网络字节序
        // id 字段是任意的
        // 字段的顺序不影响校验和
        header->ip_id = 999;
        fill_checksum(header);
        // 处理 ICMP 报头
        auto icmp_ = reinterpret_cast<icmp *>(buffer + sizeof(ip));
        // 交换 IP 头中的地址，不影响 IP 校验和
        std::swap(header->ip_src, header->ip_dst);
        // 类型变为响应
        icmp_->icmp_type = ICMP_ECHOREPLY;
        // ICMP 包长度是 8 报头 + 56 报文
        icmp_->icmp_cksum = 0;
        icmp_->icmp_cksum = check_sum(icmp_, n - sizeof(ip));
        // 写回 tun
        std::cout << *header << std::endl;
        write(tun, buffer, n);
    }

    return 0;
}
