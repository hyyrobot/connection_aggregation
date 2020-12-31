#ifndef CONNECTION_T_H
#define CONNECTION_T_H

#include <atomic>
#include <cstddef>

namespace autolabor::connection_aggregation
{
    struct pack_type_t
    {
        uint8_t state : 2;
        bool forward : 1;
        bool reply : 1;
    };

    struct direct_t
    {
        pack_type_t byte;
        uint8_t zero;
        uint16_t id;
    };

    struct connection_t
    {
        // 连接降级
        constexpr static size_t COUNT_OUT = 300;

        connection_t();

        uint8_t state() const;
        uint16_t get_id();

        size_t sent_once();
        size_t received_once(uint8_t);

        direct_t handshake();

    private:
        std::atomic_uint8_t _state, _oppesite;
        std::atomic_uint16_t _id;
        std::atomic_int64_t _sent, _received, _counter;
    };

} // namespace autolabor::connection_aggregation

#endif // CONNECTION_T_H