#include "connection_info_t.h"

namespace autolabor::connection_aggregation
{
    connection_info_t::connection_info_t()
        : _pack_in{}, _out_id(0) {}

    uint16_t connection_info_t::next_id()
    {
        return _out_id++;
    }

} // namespace autolabor::connection_aggregation