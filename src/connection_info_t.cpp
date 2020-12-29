#include "connection.h"

namespace autolabor::connection_aggregation
{
    connection_info_t::connection_info_t()
        : _state(0), _sent(0), _received(0) {}

    uint8_t connection_info_t::state() const
    {
        return _state;
    }

    size_t connection_info_t::sent_once()
    {
        return ++_sent;
    }

    size_t connection_info_t::received_once(extra_t extra)
    {
        auto s = _state.load();
        while (s < 2 && !_state.compare_exchange_strong(s, extra.state + 1))
            ;

        return ++_received;
    }

    std::ostream &operator<<(std::ostream &out, const connection_info_t &info)
    {
        return out << "state\t| in\t| out " << std::endl
                   << +info._state
                   << "\t| " << info._received
                   << "\t| " << info._sent;
    }

} // namespace autolabor::connection_aggregation