#ifndef HOST_T_H
#define HOST_T_H

#include "device_t.h"
#include "srand_t.h"

#include <netinet/in.h>
#include <linux/if.h>

namespace autolabor::connection_aggregation
{
    struct host_t
    {
        host_t(const char *, in_addr);
        friend std::ostream &operator<<(std::ostream &, const host_t &);

        // 为某网卡的套接字绑定一个端口号
        // 应该仅由服务器调用，不要滥用
        void bind(device_index_t, uint16_t);

        // 添加一个地址，建议对每个主机只调用一次
        void add_remote(in_addr, uint16_t, in_addr);

    private:
        fd_guard_t _netlink, _tun;
        void local_monitor();
        void device_added(device_index_t, const char *);
        void device_removed(device_index_t);

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