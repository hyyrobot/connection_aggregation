#include "netinet/ip.h"

#include <chrono>
#include <atomic>
#include <ostream>

namespace autolabor::connection_aggregation
{
    /** 连接附加信息 */
    struct connection_info_t
    {
        connection_info_t();
        friend std::ostream &operator<<(std::ostream &, const connection_info_t &);

        // 用于发送
        uint8_t state() const;
        uint16_t get_id();
        uint16_t next_id() const;

        // 用于记录
        size_t sent_once();
        size_t received_once(uint8_t);

    private:
        uint8_t _state;
        std::atomic<size_t> _sent, _received;

        // 发送序号
        std::atomic<uint16_t> _out_id;
    };
} // namespace autolabor::connection_aggregation
