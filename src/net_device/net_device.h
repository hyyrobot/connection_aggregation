#include "../common/fd_guard_t.h"

#include <netinet/in.h>
#include <linux/if.h>

#include <unordered_map>
#include <ostream>

namespace autolabor::connection_aggregation
{
    // 注册并配置一个 TUN 设备
    fd_guard_t register_tun(const fd_guard_t &netlink, char *name, uint32_t *index);

    struct net_device_t
    {
        char name[IFNAMSIZ];
        std::unordered_map<in_addr_t, uint8_t> addresses;
    };

    // 列出可用的网卡和其上的地址
    std::unordered_map<unsigned, net_device_t> list_devices(const fd_guard_t &);

    // 打印网卡及地址列表
    std::ostream &operator<<(std::ostream &, const std::unordered_map<unsigned, net_device_t> &);
} // namespace autolabor::connection_aggregation
