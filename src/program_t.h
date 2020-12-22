#ifndef PROGRAM_T_H
#define PROGRAM_T_H

#include "net_device_t.h"
#include "tun_device_t.h"

#include <unordered_map>

namespace autolabor::connection_aggregation
{
    /** 连接附加信息 */
    struct connection_info_t
    {
        // 此处应有同步状态、带宽、时延、丢包率等
    };

    struct program_t
    {
        program_t();

    private:
        // tun 设备
        tun_device_t _tun;

        // 本地网卡表
        // - 用网卡序号索引
        // - 来源：本地网络监测
        std::unordered_map<unsigned, net_device_t>
            _devices;

        // 远程网卡表
        // - 用远程主机地址和网卡序号索引
        // - 来源：远程数据包接收
        std::unordered_map<in_addr_t, std::unordered_map<unsigned, in_addr>>
            _remotes;

        // 连接表示法
        union connection_key_union
        {
            using key_t = uint64_t;
            key_t key;
            struct
            {
                uint32_t
                    src_index, // 本机网卡序号
                    dst_indet; // 远程主机网卡序号
            };
        };

        // 用一个整型作为索引以免手工实现 hash
        using connection_key_t = connection_key_union::key_t;

        // 连接信息用连接表示索引
        using connection_map_t = std::unordered_map<connection_key_t, connection_info_t>;

        // 连接表
        // - 用远程主机地址索引方便发送时构造连接束
        // - 来源：本地网卡表和远程网卡表的笛卡尔积
        std::unordered_map<in_addr_t, connection_map_t>
            _connection;
    };

} // namespace autolabor::connection_aggregation

#endif // PROGRAM_T_H
