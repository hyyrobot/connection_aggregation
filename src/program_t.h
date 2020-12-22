#include "net_device_t.h"

#include <unordered_map>

namespace autolabor::connection_aggregation
{
    struct connection_t
    {
        unsigned index;
        in_addr remote;
        unsigned remote_index;
    };

    struct program_t
    {
    private:
        // 本地网卡表
        std::unordered_map<unsigned, net_device_t>
            _devices;

        // 远程网卡表
        std::unordered_map<in_addr_t, std::unordered_map<unsigned, in_addr>>
            _remotes;

        // 连接表
    };

} // namespace autolabor::connection_aggregation
