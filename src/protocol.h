#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>

// 通用 ip 头附加信息
struct ip_extra_t
{
    in_addr host;                  // 虚拟网络中的源地址
    uint32_t src_index, dst_index; // 本机网卡号、远程网卡号
};

// 包类型
struct extra_t
{
    uint8_t state : 2; // 状态
    bool forward : 1;  // 转发包
    uint16_t id : 15;
};

#endif // PROTOCOL_H
