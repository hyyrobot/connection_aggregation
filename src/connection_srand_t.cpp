#include "connection.h"

namespace autolabor::connection_aggregation
{
    connection_srand_t::connection_srand_t()
        : _received{} {}

    uint16_t connection_srand_t::get_id()
    {
        auto old = _out_id.load();
        while (!_out_id.compare_exchange_strong(old, (old + 1) % ID_SIZE))
            ;
        return (old + 1) % ID_SIZE;
    }

    bool connection_srand_t::update_time_stamp(uint16_t id, connection_srand_t::stamp_t stamp)
    {
        if (stamp - _received[id] > std::chrono::milliseconds(500))
        {
            _received[id] = stamp;
            return true;
        }
        return false;
    }

} // namespace autolabor::connection_aggregation
