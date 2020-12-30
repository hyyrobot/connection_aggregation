#ifndef SRAND_T_H
#define SRAND_T_H

#include "connection_t.h"
#include "typealias.h"

#include <netinet/in.h>

#include <unordered_map>
#include <shared_mutex>
#include <ostream>

#define WRITE_GRAUD(MUTEX) std::unique_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)
#define READ_GRAUD(MUTEX) std::shared_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)

namespace autolabor::connection_aggregation
{
    // 连接表示法
    union connection_key_union
    {
        static_assert(std::is_same_v<uint16_t, device_index_t>);

        // 用一个整型作为索引以免手工实现 hash
        using key_t = uint32_t;

        key_t key;
        struct
        {
            uint16_t
                src_index, // 本机网卡序号
                dst_port;  // 远程主机端口号
        };
    };

    using connection_key_t = connection_key_union::key_t;

    struct srand_t
    {
        friend std::ostream &operator<<(std::ostream &, const srand_t &);

        // 增加了一个网卡
        void device_detected(device_index_t);

        // 删除了一个网卡
        void device_removed(device_index_t);

        // 增加了一个远程套接字
        bool remote_detected(uint16_t, in_addr);

        // 增加连接
        template <class t>
        void add_connections(uint16_t port, const std::unordered_map<device_index_t, t> &devices)
        {
            WRITE_GRAUD(_connection_mutex);
            connection_key_union _union{.dst_port = port};
            for (const auto &[i, _] : devices)
            {
                _union.src_index = i;
                _connections.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(_union.key),
                    std::forward_as_tuple());
            }
        }

    private:
        mutable std::shared_mutex _port_mutex, _connection_mutex;

        std::unordered_map<uint16_t, in_addr> _ports;
        std::unordered_map<connection_key_t, connection_t> _connections;
    };

} // namespace autolabor::connection_aggregation

#endif // SRAND_T_H