#include "remote_t.h"

#include <arpa/inet.h>

#include <sstream>

namespace autolabor::connection_aggregation
{
    remote_t::remote_t() : _distance(-1), _union{} {}

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
        bool result = _distance;
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

    // 检查接收 id 唯一性
    bool remote_t::check_unique(uint16_t id)
    {
        const auto
            now = std::chrono::steady_clock::now(),
            timeout = now + TIMEOUT;
        auto p = _time.find(id);
        // id 不存在
        if (p == _time.end())
        {
            _time.emplace(id, now);
            _id.push(id);
        }
        // id 存在但已超时
        else if (p->second < timeout)
        {
            p->second = now;
            decltype(id) pop;
            while ((pop = _id.front()) != id)
            {
                _id.pop();
                _time.erase(pop);
            }
        }
        // id 不唯一
        else
            return false;
        // 清扫所有超时的
        decltype(_time.end()) it;
        while (!_id.empty() && (it = _time.find(_id.front()))->second < timeout)
        {
            _id.pop();
            _time.erase(it);
        }
        return true;
    }

    // 通过某个连接发送计数
    size_t remote_t::sent_once(connection_key_union key)
    {
        return _distance ? -1 : _union._strand->_connections.at(key.key).sent_once();
    }

    // 通过某个连接接收计数
    size_t remote_t::received_once(connection_key_union key, uint8_t state)
    {
        return _distance ? -1 : _union._strand->_connections.at(key.key).received_once(state);
    }

    // 查询下一跳地址
    in_addr remote_t::next() const
    {
        return _distance ? _union._next : in_addr{};
    }

    // 查询距离
    uint8_t remote_t::distance() const
    {
        return _distance;
    }

    // 筛选连接

    std::vector<connection_key_t>
    remote_t::filter_for_handshake(bool on_demand) const
    {
        if (_distance)
            return {};
        std::vector<connection_key_t> result;
        auto &connections = _union._strand->_connections;
        if (on_demand)
        { // 按需发送握手
            for (auto &[k, c] : connections)
                if (c.need_handshake())
                    result.push_back(k);
        }
        else
        { // 向所有连接发送握手
            result.resize(connections.size());
            auto p = connections.begin();
            for (auto q = result.begin(); q != result.end(); ++p, ++q)
                *q = p->first;
        }
        return result;
    }

    std::vector<connection_key_t>
    remote_t::filter_for_forward() const
    {
        if (_distance)
            return {};
        std::vector<connection_key_t> result;
        auto max = 0;
        for (const auto &[k, c] : _union._strand->_connections)
        {
            auto s_ = c.state();
            if (s_ < max)
                continue;
            if (s_ > max)
            {
                max = s_;
                result.resize(1);
                result[0] = k;
            }
            else
                result.push_back(k);
        }
        return result;
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

    std::string remote_t::to_string() const
    {
        std::stringstream builder;
        if (_distance)
        {
            std::string buffer(SPACE);
            auto b = buffer.data();
            inet_ntop(AF_INET, &_union._next, b + 37, 17);
            std::to_string(_distance).copy(b + 80, 9);
            builder << buffer << std::endl;
            return builder.str();
        }
        connection_key_union key;
        connection_t::snapshot_t snapshot;
        for (const auto &[k, c] : _union._strand->_connections)
        {
            std::string buffer(SPACE);
            auto b = buffer.data();
            key.key = k;
            std::to_string(key.pair.src_index).copy(b + 22, 5);
            std::to_string(key.pair.dst_port).copy(b + 29, 6);
            auto a = _union._strand->_ports.at(key.pair.dst_port);
            inet_ntop(AF_INET, &a, b + 37, 17);
            c.snapshot(&snapshot);
            b[55] = snapshot.state + '0';
            b[59] = snapshot.opposite + '0';
            std::to_string(snapshot.received).copy(b + 63, 5);
            std::to_string(snapshot.sent).copy(b + 71, 6);
            std::to_string(snapshot.counter).copy(b + 80, 9);
            for (auto &c : buffer)
                if (c < ' ')
                    c = ' ';
            builder << buffer << std::endl;
        }
        return builder.str();
    }

} // namespace autolabor::connection_aggregation
