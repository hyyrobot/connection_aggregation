#ifndef PROGRAM_T_H
#define PROGRAM_T_H

#include "net_device_t.h"
#include "tun_device_t.h"
#include "connection_info_t.h"

#include <unordered_map>
#include <shared_mutex>
#include <ostream>

#define WRITE_GRAUD(MUTEX) std::unique_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)
#define READ_GRAUD(MUTEX) std::shared_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)

namespace autolabor::connection_aggregation
{
    constexpr uint8_t IPPROTO_MINE = 3;

    struct common_extra_t
    {
        in_addr host;
        enum class type_t : uint8_t
        {
            HAND_SHAKE
        } type;
        uint8_t zero[3];
        uint64_t connection;
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

    struct program_t
    {
        program_t(const char *, in_addr);

        // 发现新的远程网卡
        bool add_remote(in_addr, unsigned, in_addr);

        // private:
        size_t send_single(uint8_t *, size_t, in_addr, uint64_t);

        inline int receiver() const
        {
            return _receiver;
        }

        friend std::ostream &operator<<(std::ostream &, const program_t &);

    private:
        mutable std::shared_mutex
            _local_mutex,      // 访问本机网卡表
            _remote_mutex,     // 访问远程网卡表
            _connection_mutex; // 访问连接表

        // rtnetlink 套接字
        fd_guard_t _netlink;
        // 监视本地网络变化
        void local_monitor();
        // 发现新的地址或网卡
        void address_added(unsigned, const char *, in_addr = {});
        // 移除地址
        void address_removed(unsigned, in_addr);
        // 移除网卡
        void device_removed(unsigned);

        // 接收二层套接字
        fd_guard_t _receiver;

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

        // 用一个整型作为索引以免手工实现 hash
        using connection_key_t = connection_key_union::key_t;

        // 连接信息用连接表示索引
        using connection_map_t = std::unordered_map<connection_key_t, connection_info_t>;

        // 连接表
        // - 用远程主机地址索引方便发送时构造连接束
        // - 来源：本地网卡表和远程网卡表的笛卡尔积
        std::unordered_map<in_addr_t, connection_map_t>
            _connections;
    };

} // namespace autolabor::connection_aggregation

#endif // PROGRAM_T_H
