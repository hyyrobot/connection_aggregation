#include "net_devices_t.h"

namespace autolabor::connection_aggregation
{
    size_t
    net_devices_t::send(ip header, uint8_t id, const uint8_t *payload) const
    {
        _extra.id = id;
        _extra.protocol = header.ip_p;

        _iov[0].iov_base = &header;
        _iov[2] = {.iov_base = (void *)payload, .iov_len = header.ip_len - sizeof(ip)};

        header.ip_hl += 4;
        header.ip_len += 16;
        header.ip_p = IPPROTO_MINE;
        header.ip_sum = 0;

        auto p = _remotes1.find(header.ip_dst.s_addr);
        if (p == _remotes1.end())
        {
            _extra.dst = _remote.sin_addr = header.ip_dst;
            for (const auto &[index, device] : _devices)
            {
                _extra.src = device.address();
                std::make_pair(_extra.dst, device.send(&_msg));
            }
        }
        else
            for (auto dst : _remotes2.at(p->second))
            {
                _extra.dst = _remote.sin_addr = in_addr{dst};
                for (const auto &[index, device] : _devices)
                {
                    _extra.src = device.address();
                    std::make_pair(_extra.dst, device.send(&_msg));
                }
            }
        return header.ip_len;
    }
} // namespace autolabor::connection_aggregation