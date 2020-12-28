#include <netinet/in.h>

struct pack_type_t
{
    uint8_t state : 2; // 状态
    bool forward : 1;  // 转发包
    uint8_t zero : 5;  // 空
};

struct extra_t
{
    pack_type_t type; // 包类型和连接状态信息
    uint8_t zero[3];
};
