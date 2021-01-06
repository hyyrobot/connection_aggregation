#ifndef HOST_T_H
#define HOST_T_H

#include "device_t.h"
#include "connection_t.h"

#include <linux/if.h>

#include <unordered_map>
#include <queue>
#include <vector>
#include <shared_mutex>
#include <chrono>
#include <ostream>

#define READ_LOCK(MUTEX) std::shared_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)
#define WRITE_LOCK(MUTEX) std::unique_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX)
#define TRY_LOCK(MUTEX, WHAT)                                                 \
    std::unique_lock<decltype(MUTEX)> MUTEX##_guard(MUTEX, std::try_to_lock); \
    if (!MUTEX##_guard.owns_lock())                                           \
    WHAT

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
        constexpr static auto TIMEOUT = std::chrono::seconds(2);

        mutable std::shared_mutex port_mutex, connection_mutex;

        std::unordered_map<uint16_t, in_addr> ports;
        std::unordered_map<connection_key_t, connection_t> connections;

        // 获得一个新的发送序号
        uint16_t get_id();

        // 检查接收序号是否已经接收过
        bool check_received(uint16_t);

        // 查找最优连接
        std::vector<connection_key_t> filter_for_forward() const;

        // 查找需要握手的连接
        std::vector<connection_key_t> filter_for_handshake(bool) const;

    private:
        std::atomic_uint16_t _id{};

        using stamp_t = std::chrono::steady_clock::time_point;
        std::mutex _id_updater;
        std::queue<uint16_t> _received_id;
        std::unordered_map<uint16_t, stamp_t> _received_time;
    };

    struct host_t
    {
        constexpr static uint8_t MAX_TTL = 16;

        host_t(const char *, in_addr);
        friend std::ostream &operator<<(std::ostream &, const host_t &);

        // 为某网卡的套接字绑定一个端口号
        // 应该仅由服务器调用，不要滥用
        bool bind(device_index_t, uint16_t);

        // 外部使用：添加一个已知的服务器的地址
        void add_remote(in_addr, uint16_t, in_addr);

        // 添加路由条目
        void add_route(in_addr, in_addr, uint8_t);

        // 接收服务函数
        // 缓冲区由外部提供，循环在内部
        void receive(uint8_t *, size_t);

        // 主动发送握手
        size_t send_handshake(in_addr = {}, bool = true);

    private:
        constexpr static uint32_t
            ID_TUN = 1u << 16u,
            ID_NETLINK = 2u << 16u;

        decltype(std::chrono::steady_clock::now()) _t0;

        std::mutex _receiving;

        fd_guard_t _netlink, _tun, _epoll;

        void send_list_request() const;
        void read_netlink(uint8_t *, size_t);
        void forward(uint8_t *, size_t);

        void device_added(device_index_t, const char *);
        void device_removed(device_index_t);

        size_t send_single(in_addr, connection_key_union, uint8_t *, size_t);

        char _name[IFNAMSIZ];
        device_index_t _index;
        in_addr _address;

        mutable std::shared_mutex
            _device_mutex,
            _srand_mutex,
            _route_mutex;

        // 本机网卡
        std::unordered_map<device_index_t, device_t> _devices;

        // 远程主机
        std::unordered_map<in_addr_t, srand_t> _srands;

        struct route_item_t
        {
            in_addr next;
            uint8_t length;
        };

        // 路由表
        std::unordered_map<in_addr_t, route_item_t> _route;
    };

} // namespace autolabor::connection_aggregation

#endif // HOST_T_H