#ifndef REMOTE_T_H
#define REMOTE_T_H

#include "device_t.h"
#include "connection_t.h"

#include <netinet/in.h>

#include <chrono>
#include <queue>
#include <unordered_map>

namespace autolabor::connection_aggregation
{
    struct pack_type_t
    {
        uint8_t
            state : 2;
        bool
            multiple : 1, // 需要去重/包含 id
            forward : 1;  // 到达后向应用层上传
    };

    // 连接表示法
    union connection_key_union
    {
        // 用一个整型作为索引以免手工实现 hash
        using key_t = uint32_t;

        key_t key;
        struct
        {
            uint16_t
                src_index, // 本机网卡序号
                dst_port;  // 远程主机端口号
        } pair;
    };

    using connection_key_t = connection_key_union::key_t;

    struct remote_t
    {
        remote_t();
        ~remote_t();

        // 新网卡已添加到主机
        void device_added(device_index_t);

        // 新网卡已从主机移除
        void device_removed(device_index_t);

        // 设定到主机的直连路由
        bool add_direct(in_addr, uint16_t, const std::vector<device_index_t> &);

        // 设定到主机的路由
        bool add_route(in_addr, uint8_t);

        // 获得一个唯一报文 id
        uint16_t get_id();

        // 检查接收 id 唯一性
        bool check_multiple(uint16_t);

        // 填写目标连接地址和状态
        bool set_address_and_state(connection_key_union, in_addr *, pack_type_t *) const;

    private:
        using stamp_t = std::chrono::steady_clock::time_point;
        struct strand_t
        {
            std::unordered_map<uint16_t, in_addr> _ports;
            std::unordered_map<connection_key_t, connection_t> _connections;
        };

        // 去重
        constexpr static auto TIMEOUT = std::chrono::seconds(2);

        std::atomic_uint16_t _id_s;
        std::queue<uint16_t> _id_r;
        std::unordered_map<uint16_t, stamp_t> _received_time;

        // 路由
        uint8_t _distance;
        union
        {
            in_addr _next;
            strand_t *_strand;
        } _union;
    };
} // namespace autolabor::connection_aggregation

#endif // REMOTE_T_H