#include "connection.h"

namespace autolabor::connection_aggregation
{
    connection_info_t::connection_info_t()
        : _state_local(0),
          _state_remote(0),
          _sent(0),
          _received(0) {}

    uint8_t connection_info_t::state_local() const
    {
        return _state_local;
    }

    uint8_t connection_info_t::state_remote() const
    {
        return _state_remote;
    }

    size_t connection_info_t::sent_once()
    {
        return ++_sent;
    }

    size_t connection_info_t::received_once(extra_t extra)
    {
        _state_remote = extra.state;
        _state_local = std::min(2, _state_remote + 1);

        return ++_received;
    }

    std::ostream &operator<<(std::ostream &out, const connection_info_t &info)
    {
        return out << "state\t| in\t| out " << std::endl
                   << +info._state_local
                   << "\t| " << info._received
                   << "\t| " << info._sent;
    }

} // namespace autolabor::connection_aggregation