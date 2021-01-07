#include "remote_t.h"

namespace autolabor::connection_aggregation
{
    remote_t::remote_t() : _id_s(0), _distance(-1), _union{} {}

    remote_t::~remote_t()
    {
        // 直连路由
        if (!_distance)
            delete _union._strand;
    }

    // 设定到主机的直连路由
    bool remote_t::add_direct(in_addr address, uint16_t port, const std::vector<device_index_t> &devices)
    {
        auto result = !_distance;
        if (result)
        {
            _distance = 0;
            _union._strand = new strand_t;
        }
        auto s = _union._strand;
        auto [p, b] = s->_ports.try_emplace(port, address);
        if (b)
        {
            connection_key_union key{.pair{.dst_port = port}};
            for (const auto i : devices)
            {
                key.pair.src_index = i;
                s->_connections.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(key.key),
                    std::forward_as_tuple());
            }
        }
        else
            p->second = address;
        return result || b;
    }

    // 设定到主机的路由
    bool remote_t::add_route(in_addr next, uint8_t distance)
    {
        if (_distance <= distance)
            return false;
        _union._next = next;
        _distance = distance;
        return true;
    }

    // 获得一个唯一报文 id
    uint16_t remote_t::get_id()
    {
        return _id_s++;
    }

    // 填写目标连接地址和状态
    bool remote_t::get_address_set_state(connection_key_union key, in_addr *address, pack_type_t *type) const
    {
        auto p = _union._strand->_ports.find(key.pair.dst_port);
        if (p == _union._strand->_ports.end())
            return false;
        *address = p->second;
        type->state = _union._strand->_connections.at(key.key).state();
        return true;
    }
} // namespace autolabor::connection_aggregation