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
    // 我的第 3.5 层协议
    constexpr uint8_t IPPROTO_MINE = 3;

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

    // 通用 ip 头附加信息
    struct common_extra_t
    {
        in_addr host;                  // 虚拟网络中的源地址
        uint32_t src_index, dst_index; // 本机网卡号、远程网卡号
    };

    struct program_t
    {
        program_t(const char *, in_addr);

        // 创建新的远程网卡
        bool add_remote(in_addr, uint32_t, in_addr);

        const char *name() const;
        in_addr address() const;

        // 发送基于连接的握手包
        bool send_handshake(in_addr, connection_key_t);

        // 向特定地址转发数据包
        size_t forward(in_addr, const ip *, const uint8_t *, size_t);

        size_t receive(ip *, uint8_t *, size_t);

        inline int tun() const
        {
            return _tun.socket();
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
        void address_added(uint32_t, const char *, in_addr = {});
        // 移除地址
        void address_removed(uint32_t, in_addr);
        // 移除网卡
        void device_removed(uint32_t);

        // 发送单个包
        // 1. 目的地址
        // 2. 通过连接
        // 3. 要发送的分散内存块
        //    **注意！这个数组的 [0] 需要留空，将会填入 ip 头！**
        //    **注意！这个数组的 [1] 的第一个字节会被修改，不要加 const！**
        // 4. 内存块数量，至少是 2
        bool send_single(in_addr, connection_key_t, iovec *, size_t);

        // 接收二层套接字
        fd_guard_t _receiver;

        // tun 设备
        tun_device_t _tun;

        // 本地网卡表
        // - 用网卡序号索引
        // - 来源：本地网络监测
        std::unordered_map<uint32_t, net_device_t>
            _devices;

        // 远程网卡表
        // - 用远程主机地址和网卡序号索引
        // - 来源：远程数据包接收
        std::unordered_map<in_addr_t, std::unordered_map<uint32_t, in_addr>>
            _remotes;

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
