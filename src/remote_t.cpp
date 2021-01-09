#include "remote_t.h"

#include <arpa/inet.h>

#include <sstream>

namespace autolabor::connection_aggregation
{
    remote_t::remote_t() : _distance(-1), _union{}, _seq(-1) {}

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
            timeout = now - TIMEOUT0;
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

    bool remote_t::check_timeout(uint16_t id, stamp_t timeout) const
    {
        auto p = _time.find(id);
        if (p == _time.end())
            return true;
        return p->second <= timeout;
    }

    // 换出缓存
    std::vector<std::vector<uint8_t>>
    remote_t::exchange(const uint8_t *data, size_t size)
    {
        const auto
            now = std::chrono::steady_clock::now(),
            id_timeout = now - TIMEOUT0,
            data_timeout = now - TIMEOUT1;

#define ID(DATA, N) reinterpret_cast<const aggregator_t *>(DATA)->id##N // 获取数据报中的序号
        // ------------------------------------------------------------- 去重
        auto duplicate = false;
        if (!data)
            // 如果没有生产新的数据，就如同生产了重复的数据
            duplicate = true;
        else
        {
            // 检查是否发生重复
            auto id = ID(data, 1);
            auto p = _time.find(id);
            // id 不存在
            if (p == _time.end())
            {
                _time.emplace(id, now);
                _id.push(id);
            }
            // id 存在但已超时
            else if (p->second <= id_timeout)
            {
                // 如果这个 id 发生超时，则比这个 id 早的必然全部超时
                // 清扫这些以节约内存
                // 在这里清理算法更简单，更快
                uint16_t pop;
                while ((pop = _id.front()) != id)
                {
                    _id.pop();
                    _time.erase(pop);
                }
                // id 出现在队尾，更新时间
                _id.pop();
                _id.push(id);
                p->second = now;
            }
            // id 重复
            else
                duplicate = true;
        }
        // 无论是否有效，利用调用机会清扫所有超时的记录以节约内存
        // 在这里清理更复杂，需要每个分别判断超时
        // it 引发变量冲突，缩小作用域消除歧义
        {
            decltype(_time.end()) it;
            while ((it = _time.find(_id.front()))->second <= id_timeout)
            {
                _id.pop();
                _time.erase(it);
            }
        }
        // ------------------------------------------------------------- 消费缓存
        // 缓存为空
        if (_buffer.empty())
        {
            // 当前包也重复了，跳过
            if (duplicate)
                return {};
            // 不连续，拷贝缓存
            if (_seq != ID(data, 0))
            {
                std::copy(data, data + size, _buffer.emplace_back(size).data());
                return {};
            }
            // 恰好连续，免拷贝
            _seq = ID(data, 1);
            return {std::vector<uint8_t>(0)};
        }
        // 在此消费缓存数据包
        std::vector<std::vector<uint8_t>> result;
        // 当前是否处于连续输出状态
        // 既可以从缓存头部消费的状态
        // 只有当前包补足了头部的缺失，
        // 或者头部发生超时（此时先不考虑），
        // 才有可能在初始状态处在连续输出状态
        auto combo = _seq == ID(data, 0);
        // 当前包是否已处理
        // 如果发生重复就如同已处理，
        // 如果发生恰好补足引起的连续输出状态，也说明已处理
        auto processed = duplicate || combo;
        // 考虑第一包的补足逻辑
        if (combo)
        {
            result.emplace_back(0);
            _seq = ID(data, 1);
        }
        // 判断是否因超时以连续输出状态开始
        // 可能由于当前包恰好补足空缺，或头部超时
        auto it = _buffer.begin();
        combo = (combo && _seq == ID(data, 0)) || check_timeout(ID(it->data(), 1), data_timeout);
        // at end | combo | processed | discription
        //    v   |       |           | 不可能
        //    x   |   x   |     x     | 有效的数据包，没能补足头部，头部也没有超时
        //    x   |   x   |     v     | 无效的数据包
        //    x   |   v   |     x     | 头部超时
        //    x   |   v   |     v     | 恰好补足头部
        while (combo)
        {
            // 输出
            _seq = ID(it->data(), 1);
            result.push_back(std::move(*it++));
            // 连续输出状态下，当前包恰好后随迭代器
            if (!processed && _seq == ID(data, 0))
            {
                _seq = ID(data, 1);
                processed = true;
                result.emplace_back(0);
            }
            // 缓存耗尽
            if (it == _buffer.end())
            {
                combo = false;
                break;
            }
            // 判断是否仍可输出
            combo = _seq == ID(it->data(), 0) || check_timeout(ID(it->data(), 1), data_timeout);
        }
        // 无论何种情况，迭代器之前已全部输出
        _buffer.erase(_buffer.begin(), it);
        // at end | combo | processed | discription
        //        |   v   |           | 不可能
        //        |   x   |     v     | 且当前包无效或已处理
        //    v   |   x   |     x     | 缓存耗尽，仍未找到当前包的前驱
        //    x   |   x   |     x     | 输出中断，仍未找到当前包的前驱
        while (!processed)
        {
            // 缓存已耗尽
            if (processed = it == _buffer.end())
                std::copy(data, data + size, _buffer.emplace_back(size).data());
            // 当前包后随迭代器
            else if (processed = ID(it->data(), 1) == ID(data, 0))
                // emplace = 前插
                std::copy(data, data + size, _buffer.emplace(++it, size)->data());
            // 迭代器后随当前包
            else if (processed = ID(it->data(), 0) == ID(data, 1))
                std::copy(data, data + size, _buffer.emplace(it, size)->data());
            // 当前包先于迭代器
            else if (processed = ID(it->data(), 1) > ID(data, 1))
                std::copy(data, data + size, _buffer.emplace(it, size)->data());
            // 没能插入，迭代
            else
                ++it;
        }
#undef ID
        // at end | combo | processed | discription
        //        |   x   |     v     | 完成
        return result;
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
