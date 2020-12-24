#include <netinet/in.h>

struct pack_type_t
{
    uint8_t state : 2; // 状态
    bool forward : 1;  // 转发包
    uint8_t zero : 5;  // 空
};

struct nothing_t
{
    uint8_t zero[4];
};

struct forward_t
{
    pack_type_t type; // 包类型和连接状态信息
    uint8_t protocol; // 需要恢复到 ip 头的真实协议号
    uint16_t offset;  // 需要恢复到 ip 头的真实段偏移
    in_addr
        src, // 虚拟网络中的源地址
        dst; // 虚拟网络中的目的地址
};
