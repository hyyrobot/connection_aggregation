#ifndef CONNECTION_H
#define CONNECTION_H

#include <unordered_map>
#include <list>

#include <atomic>
#include <ostream>
#include <chrono>

#include "protocol.h"

namespace autolabor::connection_aggregation
{
    // 我的第 3.5 层协议
    constexpr uint8_t IPPROTO_MINE = IPPROTO_IPIP;

    // 网卡序号类型
    using net_index_t = uint16_t;

    /** 
     * 连接附加信息
     * 这个结构体无锁线程安全
     */
    struct connection_info_t
    {
        connection_info_t();
        friend std::ostream &operator<<(std::ostream &, const connection_info_t &);

        // 连接状态
        uint8_t state() const;

        // 用于发送
        uint16_t get_id();

        // 成功发送时调用
        size_t sent_once();

        // 成功接收时调用
        size_t received_once(extra_t);

    private:
        // 状态
        std::atomic_uint8_t _state;

        // 发送、接收次数统计
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

    // 连接束类型
    class connection_srand_t
    {
        using stamp_t = std::chrono::steady_clock::time_point;
        constexpr static uint16_t ID_SIZE = 0x20'00;

        // 发
        std::atomic_uint16_t _out_id;
        // 收
        std::list<uint16_t> _received_id;
        std::unordered_map<uint16_t, stamp_t> _received_time;
        
    public:
        connection_srand_t();

        std::unordered_map<connection_key_t, connection_info_t> items;

        uint16_t get_id();

        // 这个方法不是线程安全的，只在收函数中使用，用别的锁来保护
        bool update_time_stamp(uint16_t, stamp_t);
    };
} // namespace autolabor::connection_aggregation

#endif // CONNECTION_H
