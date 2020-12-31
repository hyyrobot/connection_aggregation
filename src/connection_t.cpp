#include "connection_t.h"

namespace autolabor::connection_aggregation
{
    connection_t::connection_t()
        : _state(0), _oppesite(0),
          _id(0),
          _sent(0), _received(0), _counter(0) {}

    uint8_t connection_t::state() const
    {
        return _state;
    }

    uint16_t connection_t::get_id()
    {
        return ++_id;
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
        const auto up = _oppesite.exchange(state) < state;

        auto s = _state.load();
        auto loop = true;
        do
            switch (s)
            {
            case 0: // 只要收到，就证明可以收到
                loop = !_state.compare_exchange_strong(s, 1);
                break;
            case 1: // 如果发现对端升级，证明对端可以收到
                loop = up && !_state.compare_exchange_strong(s, 2);
                break;
            case 2: // 如果发现对端升级，确认对端可以收到
                loop = up && !_state.compare_exchange_strong(s, 3);
                break;
            case 3: // 如果发现对端没有升级两次，证明连接状态有变化（可能丢包率极高或对端已经重启），降级
                loop = state < 2 && !_state.compare_exchange_strong(s, 1);
                break;
            }
        while (loop);

        return ++_received;
    }

} // namespace autolabor::connection_aggregation