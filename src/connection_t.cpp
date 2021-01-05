#include "connection_t.h"

namespace autolabor::connection_aggregation
{
    connection_t::connection_t()
        : _state(0), _oppesite(0),
          _sent(0), _received(0), _counter(0) {}

    uint8_t connection_t::state() const
    {
        return _state;
    }

    void connection_t::snapshot(snapshot_t *buffer) const
    {
        *buffer = {
            .state = _state,
            .opposite = _oppesite,
            .sent = _sent,
            .received = _received,
            .counter = _counter,
        };
    }

    bool connection_t::need_handshake() const
    {
        return (_state < 3 || _oppesite < 3) && _counter < 3 * COUNT_OUT;
    }

    size_t connection_t::sent_once()
    {
        // 计数并退出，除非发了 `COUNT_OUT` 包对方都不回
        if (++_counter < COUNT_OUT)
            return ++_sent;
        // 如果状态就是 0，退出
        auto s = _state.load();
        if (!s)
            return ++_sent;
        // 进行降级：3 -> 2 | {2, 1} -> 0
        // 降级不一定要完成，不管成功失败都退出
        _state.compare_exchange_weak(s, s == 3 ? 2 : 0);
        _oppesite.store(s == 3 ? 2 : 0);
        return ++_sent;
    }

    size_t connection_t::received_once(const uint8_t state)
    {
        // 计数清零
        _counter = 0;
        // 交换判断是否发生升级
        const auto o = _oppesite.exchange(state);

        auto s = _state.load();
        switch (s)
        {
        case 0: // 只要收到，就证明可以收到
            _state.compare_exchange_weak(s, 1);
            break;
        case 1: // 如果发现对端升级，证明对端可以收到
            if (o)
                _state.compare_exchange_weak(s, 2);
            break;
        case 2: // 如果发现对端升级，确认对端可以收到
            if (state > o)
                _state.compare_exchange_weak(s, 3);
            break;
        case 3: // 如果发现对端没有升级两次，证明连接状态有变化（可能丢包率极高或对端已经重启），降级
            if (state < 2)
                _state.compare_exchange_weak(s, 1);
            break;
        }

        return ++_received;
    }

} // namespace autolabor::connection_aggregation