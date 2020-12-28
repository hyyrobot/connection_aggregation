#ifndef CONNECTION_H
#define CONNECTION_H

#include <unordered_map>
#include <atomic>
#include <ostream>
#include <chrono>

#include "protocol.h"

namespace autolabor::connection_aggregation
{
    /** 连接附加信息 */
    struct connection_info_t
    {
        connection_info_t();
        friend std::ostream &operator<<(std::ostream &, const connection_info_t &);

        // 用于发送
        uint8_t state() const;
        uint16_t get_id();

        // 用于记录
        size_t sent_once();
        size_t received_once(extra_t);

    private:
        uint8_t _state;
        std::atomic<size_t> _sent, _received;

        // 发送序号
        std::atomic<uint16_t> _out_id;
    };

    // 连接表示法
    union connection_key_union
    {
        using key_t = uint64_t;
        key_t key;
        struct
        {
            uint32_t
                src_index, // 本机网卡序号
                dst_index; // 远程主机网卡序号
        };
    };

    // 用一个整型作为索引以免手工实现 hash
    using connection_key_t = connection_key_union::key_t;

    // 连接信息用连接表示索引
    struct connection_srand_t
    {
        constexpr static uint16_t size = 0x20'00;

        connection_srand_t();

        std::unordered_map<connection_key_t, connection_info_t> items;
        std::chrono::steady_clock::time_point received[size];

        uint16_t get_id();

    private:
        std::atomic_uint16_t _out_id;
    };
} // namespace autolabor::connection_aggregation

#endif // CONNECTION_H
