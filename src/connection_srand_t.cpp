#include "connection.h"

namespace autolabor::connection_aggregation
{
    connection_srand_t::connection_srand_t()
        : received{} {}

    uint16_t connection_srand_t::get_id()
    {
        auto old = _out_id.load();
        while (!_out_id.compare_exchange_strong(old, (old + 1) % size))
            ;
        return (old + 1) % size;
    }

} // namespace autolabor::connection_aggregation
