#ifndef CONNECTION_T_H
#define CONNECTION_T_H

#include <atomic>
#include <cstddef>

namespace autolabor::connection_aggregation
{
    struct connection_t
    {
        // 连接降级
        constexpr static size_t COUNT_OUT = 300;

        connection_t();

        uint8_t state() const;
        uint16_t get_id();

        size_t sent_once();
        size_t received_once(uint8_t);

    private:
        std::atomic_uint8_t _state, _oppesite;
        std::atomic_uint16_t _id;
        std::atomic_int64_t _sent, _received, _counter;
    };

} // namespace autolabor::connection_aggregation

#endif // CONNECTION_T_H