#include "connection_info_t.h"

namespace autolabor::connection_aggregation
{
    connection_info_t::connection_info_t()
        : _pack_in{}, _out_id(0) {}

    uint16_t connection_info_t::next_id()
    {
        auto temp = ++_out_id; // `ip::ip_id` 填 0 会导致随机值，需要跳过 0
        return temp ? temp : ++_out_id;
    }

} // namespace autolabor::connection_aggregation