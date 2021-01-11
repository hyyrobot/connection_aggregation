#ifndef CONNECTION_T_H
#define CONNECTION_T_H

#include <atomic>
#include <chrono>

namespace autolabor::connection_aggregation
{
    using clock = std::chrono::steady_clock;
    using stamp_t = clock::time_point;

    struct connection_t
    {
        struct snapshot_t
        {
            uint8_t state, opposite;
            int64_t sent, received, counter;
        };

        // 连接降级
        constexpr static auto TIMEOUT = std::chrono::seconds(1);
        constexpr static auto COUNT_OUT = 250;

        connection_t();

        uint8_t state() const;
        void snapshot(snapshot_t *) const;

        bool need_handshake() const;
        bool died() const;

        size_t sent_once();
        size_t received_once(uint8_t);

    private:
        stamp_t _t_r;
        std::atomic_uint8_t _state, _oppesite;
        std::atomic_int64_t _sent, _received, _counter;
    };

} // namespace autolabor::connection_aggregation

#endif // CONNECTION_T_H