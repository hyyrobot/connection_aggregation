#ifndef HOST_T_H
#define HOST_T_H

#include "remote_t.h"
#include "utilities/fixed_thread_pool_t.hpp"

#include <linux/if.h>
#include <sys/un.h> // sockaddr_un

#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <ostream>

namespace autolabor::connection_aggregation
{
    std::ostream &operator<<(std::ostream &, in_addr);

    struct host_t
    {
        host_t(const char *, in_addr);

        // 为某网卡的套接字绑定一个端口号
        // 应该仅由服务器调用，不要滥用
        bool bind(device_index_t, uint16_t);

        // 外部使用：添加一个已知的服务器的地址
        void add_remote(in_addr, in_addr, uint16_t);

        // 接收服务函数
        // 缓冲区由外部提供，循环在内部
        void receive(uint8_t *, size_t);

        // 主动发送握手
        void yell(in_addr = {});

        // 打印
        void print();

    private:
        constexpr static uint8_t
            SUBNET_PREFIX = 24;
        constexpr static in_addr_t
            SUBNET_MASK = 0xffffffff << (32 - SUBNET_PREFIX) >> (32 - SUBNET_PREFIX);
        constexpr static uint8_t
            MAX_TTL = 0b1111;
        constexpr static uint32_t
            ID_TUN = 1u << 16u,
            ID_UNIX = 2u << 16u,
            ID_NETLINK = 3u << 16u;

        enum unix_t : uint8_t
        {
            VIEW = 8,
            BIND,
            YELL,
            SEND_HANDSHAKE,
            ADD_REMOTE,
        };

        // 指令：创建直连路由
        struct cmd_remote_t
        {
            unix_t type;
            uint8_t zero;
            uint16_t port;
            in_addr virtual_, actual_;
        };

        // 指令：绑定端口
        struct cmd_bind_t
        {
            unix_t type;
            uint8_t zero;
            uint16_t index, port;
        };

        // 本机网卡
        std::unordered_map<device_index_t, device_t> _devices;

        // 远程主机
        std::unordered_map<in_addr_t, remote_t> _remotes;

        std::mutex _receiving;
        fd_guard_t _netlink, _tun, _unix, _epoll;
        fixed_thread_pool_t _threads;

        char _name[IFNAMSIZ];
        device_index_t _index;
        in_addr _address;

        uint16_t _id_s{};
        sockaddr_un _address_un;

        std::string to_string() const;

        // 配置已注册的 tun
        // 仅在构造器调用
        void config_tun() const;
        // 向 netlink 发送查询网卡请求
        void send_list_request() const;

        void read_tun(uint8_t *, size_t);
        void read_unix(uint8_t *, size_t);
        void read_netlink(uint8_t *, size_t);
        struct forward_t
        {
            in_addr origin;
            uint32_t size;
        };
        forward_t read_from_device(device_index_t, uint8_t *, size_t);
        constexpr static auto SCAN_COUNT_OUT = 10000;
        int _scan_counter;

        void send_void(in_addr, bool);
        void add_remote_inner(in_addr, in_addr, uint16_t);

        void device_added(device_index_t, const char *);
        void device_removed(device_index_t);

        bool send_single(sending_t, const uint8_t *, size_t);
        size_t send_strand(in_addr, const uint8_t *, size_t);
    };

} // namespace autolabor::connection_aggregation

#endif // HOST_T_H