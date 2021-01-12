#ifndef REMOTE_T_H
#define REMOTE_T_H

#include "device_t.h"
#include "connection_t.h"

#include <string>
#include <chrono>
#include <queue>
#include <list>
#include <vector>
#include <unordered_map>

namespace autolabor::connection_aggregation
{
    enum type0_enum : uint8_t
    {
        SPECIAL, // 特殊功能包
        FORWARD, // 转发
    };

    struct type1_t
    {
        type0_enum
            type0 : 2;
        uint8_t
            state : 2,
            ttl : 4;
    };

    struct aggregator_t
    {
        in_addr source;
        type1_t type1;
        uint8_t data[3];
        // id0 用于排队，id1 用于去重
        // id0 == 0: 只去重，不排队，用于源端开始排序前或不需要转发的包
        // id1 绝不为 0
        uint16_t id0, id1;
        // 用于携带路由信息，与 ip 头重合
        in_addr ip_src;
    };

    static_assert(sizeof(aggregator_t) == 16);

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

    struct sending_t
    {
        uint8_t state : 2;
        device_index_t index : 14;
        uint16_t port;
        in_addr actual;
    };

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

        // 确认存在性
        bool scan();

        // 交换发送 id
        uint16_t exchange(uint16_t);

        // 换出缓存
        std::vector<std::vector<uint8_t>>
        exchange(stamp_t &, const uint8_t *, size_t);

        // 通过某个连接发送计数
        size_t sent_once(connection_key_union);

        // 通过某个连接接收计数
        size_t received_once(connection_key_union, uint8_t);

        // 查询下一跳地址
        in_addr next() const;

        // 查询距离
        uint8_t distance() const;

        // 提取发送信息
        std::vector<sending_t> handshake(bool) const;
        std::vector<sending_t> forward() const;
        sending_t sending(connection_key_union) const;

        // 显示格式
        constexpr static auto
            TITLE = "|      host       | index |  port  |     address     | state | input | output | counter |",
            _____ = "| --------------- | ----- | ------ | --------------- | ----- | ----- | ------ | ------- |",
            SPACE = "|                 |       |        |                 |  -->  |       |        |         |";

        std::string to_string() const;

        // 去重、排队超时
        constexpr static auto DEFAULT_ID_TIMEOUT = std::chrono::seconds(3);
        constexpr static auto DEFAULT_DATA_TIMEOUT = std::chrono::milliseconds(100);
        std::chrono::microseconds id_timeout, data_timeout;

    private:
        struct strand_t
        {
            std::unordered_map<uint16_t, in_addr> _ports;
            std::unordered_map<connection_key_t, connection_t> _connections;
        };

        std::queue<uint16_t> _id;
        std::unordered_map<uint16_t, stamp_t> _time;
        uint16_t _id0_s, _seq;
        std::list<std::vector<uint8_t>> _buffer;

        bool check_timeout(uint16_t, stamp_t) const;

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
