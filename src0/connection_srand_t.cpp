#include "connection.h"

namespace autolabor::connection_aggregation
{
    connection_srand_t::connection_srand_t() {}

    uint16_t connection_srand_t::get_id()
    {
        auto old = _out_id.load();
        while (!_out_id.compare_exchange_strong(old, (old + 1) % ID_SIZE))
            ;
        return (old + 1) % ID_SIZE;
    }

    bool connection_srand_t::update_time_stamp(uint16_t id, connection_srand_t::stamp_t stamp)
    {
        constexpr static auto timeout = std::chrono::milliseconds(500);

        auto p = _received_time.find(id);
        if (p == _received_time.end())
        {
            _received_time.emplace(id, stamp);
            _received_id.push_back(id);
        }
        else if (stamp - p->second > timeout)
        {
            p->second = stamp;
            while (true)
            {
                auto pop = _received_id.front();
                _received_id.pop_front();
                if (pop != id)
                    _received_time.erase(pop);
                else
                    break;
            }
        }
        else
            return false;
        while (true)
        {
            auto pop = _received_id.front();
            auto qoq = _received_time.find(pop);
            if (stamp - qoq->second > timeout)
            {
                _received_id.pop_front();
                _received_time.erase(qoq);
            }
            else
                break;
        }
        return true;
    }

} // namespace autolabor::connection_aggregation
