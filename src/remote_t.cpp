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

    // 新网卡已添加到主机
    void remote_t::device_added(device_index_t index)
    {
        if (_distance)
            return;
        connection_key_union key{.pair{.src_index = index}};
        auto s = _union._strand;
        for (auto &[port, _] : s->_ports)
        {
            key.pair.dst_port = port;
            s->_connections.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(key.key),
                std::forward_as_tuple());
        }
    }

    // 新网卡已从主机移除
    void remote_t::device_removed(device_index_t index)
    {
        if (_distance)
            return;
        connection_key_union key{.pair{.src_index = index}};
        auto s = _union._strand;
        for (auto &[port, _] : s->_ports)
        {
            key.pair.dst_port = port;
            s->_connections.erase(key.key);
        }
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

    // 检查接收 id 唯一性
    bool remote_t::check_multiple(uint16_t id)
    {
        const auto
            now = std::chrono::steady_clock::now(),
            timeout = now + TIMEOUT;
        auto p = _received_time.find(id);
        // id 不存在
        if (p == _received_time.end())
        {
            _received_time.emplace(id, now);
            _id_r.push(id);
        }
        // id 存在但已超时
        else if (p->second < timeout)
        {
            p->second = now;
            decltype(id) pop;
            while ((pop = _id_r.front()) != id)
            {
                _id_r.pop();
                _received_time.erase(pop);
            }
        }
        // id 不唯一
        else
            return false;
        // 清扫所有超时的
        decltype(_received_time.end()) it;
        while ((it = _received_time.find(_id_r.front()))->second < timeout)
        {
            _id_r.pop();
            _received_time.erase(it);
        }
        return true;
    }

    // 填写目标连接地址和状态
    bool remote_t::set_address_and_state(connection_key_union key, in_addr *address, pack_type_t *type) const
    {
        auto s = _union._strand;
        auto p = s->_ports.find(key.pair.dst_port);
        if (p == s->_ports.end())
            return false;
        *address = p->second;
        type->state = s->_connections.at(key.key).state();
        return true;
    }

} // namespace autolabor::connection_aggregation