#include "netinet/ip.h"

#include <chrono>
#include <atomic>

namespace autolabor::connection_aggregation
{
    struct pack_in_t
    {
        uint32_t id;
        uint64_t time;
    };

    /** 连接附加信息 */
    struct connection_info_t
    {
        // 同步包
        struct sync_pack_t
        {
            // 最新收到的两个包的序号和时延
            uint32_t id[2], dt;
        };

        uint16_t get_id();
        uint16_t next_id() const;

        constexpr static size_t sync_size = sizeof(ip) + sizeof(sync_pack_t);
        connection_info_t();

    private:
        // 收报记录
        pack_in_t _pack_in[2];
        // 发送序号
        std::atomic<uint16_t> _out_id;
    };
} // namespace autolabor::connection_aggregation
