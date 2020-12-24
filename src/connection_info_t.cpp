#include "connection_info_t.h"
#include "protocol.h"

namespace autolabor::connection_aggregation
{
    connection_info_t::connection_info_t()
        : _state(0), _out_id(0), _sent(0), _received(0) {}

    uint8_t connection_info_t::state() const
    {
        return _state;
    }

    uint16_t connection_info_t::get_id()
    {
        auto temp = ++_out_id; // `ip::ip_id` 填 0 会导致随机值，需要跳过 0
        return temp ? temp : ++_out_id;
    }

    uint16_t connection_info_t::next_id() const
    {
        return _out_id;
    }

    size_t connection_info_t::sent_once()
    {
        return ++_sent;
    }

    size_t connection_info_t::received_once(uint8_t type)
    {
        if (_state < 2)
        {
            union
            {
                uint8_t byte;
                pack_type_t x;
            } convertor{.byte = type};
            _state = convertor.x.state + 1;
        }

        return ++_received;
    }

    std::ostream &operator<<(std::ostream &out, const connection_info_t &info)
    {
        return out << "state\t| id\t| in\t| out " << std::endl
                   << +info._state
                   << "\t| " << info._out_id
                   << "\t| " << info._received
                   << "\t| " << info._sent;
    }

} // namespace autolabor::connection_aggregation