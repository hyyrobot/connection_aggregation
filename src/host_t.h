#ifndef HOST_T_H
#define HOST_T_H

#include "device_t.h"
#include "connection_t.h"

#include <netinet/in.h>
#include <linux/if.h>

#include <unordered_map>
#include <shared_mutex>
#include <ostream>

#define READ_LOCK(MUTEX) std::shared_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)
#define WRITE_LOCK(MUTEX) std::unique_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)

namespace autolabor::connection_aggregation
{
    using read_lock = std::shared_lock<std::shared_mutex>;
    using write_lock = std::unique_lock<std::shared_mutex>;

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

    struct srand_t
    {
        mutable std::shared_mutex port_mutex, connection_mutex;

        std::unordered_map<uint16_t, in_addr> ports;
        std::unordered_map<connection_key_t, connection_t> connections;
    };

    struct host_t
    {
        host_t(const char *, in_addr);
        friend std::ostream &operator<<(std::ostream &, const host_t &);

        // 为某网卡的套接字绑定一个端口号
        // 应该仅由服务器调用，不要滥用
        void bind(device_index_t, uint16_t);

        // 外部使用：添加一个已知的服务器的地址
        void add_remote(in_addr, uint16_t, in_addr);

        // 接收
        size_t receive(uint8_t *, size_t);

        size_t send_handshake(in_addr);

    private:
        fd_guard_t _netlink, _tun, _epoll;
        void local_monitor();
        void device_added(device_index_t, const char *);
        void device_removed(device_index_t);

        size_t send_single(in_addr, connection_key_union, pack_type_t, const uint8_t * = nullptr, size_t = 0);

        char _name[IFNAMSIZ];
        device_index_t _index;
        in_addr _address;

        mutable std::shared_mutex _device_mutex, _srand_mutex;

        // 本机网卡
        std::unordered_map<device_index_t, device_t> _devices;

        // 远程主机
        std::unordered_map<in_addr_t, srand_t> _srands;
    };

} // namespace autolabor::connection_aggregation

#endif // HOST_T_H