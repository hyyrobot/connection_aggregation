#include "../program_t.h"

namespace autolabor::connection_aggregation
{
    bool program_t::add_remote(in_addr virtual_address, net_index_t index, in_addr real_address)
    {
        { // 修改远程网卡表
            WRITE_GRAUD(_remote_mutex);
            auto [p, _] = _remotes.try_emplace(virtual_address.s_addr);
            auto &remote = p->second;
            auto [q, new_remote] = remote.try_emplace(index, real_address);
            if (!new_remote)
            {
                q->second = real_address;
                return false;
            }
        }
        // 修改连接表
        {
            READ_GRAUD(_local_mutex);
            WRITE_GRAUD(_connection_mutex);
            auto [p, _] = _connections.try_emplace(virtual_address.s_addr);
            connection_key_union key{.dst_index = index};
            for (const auto &[local_index, _] : _devices)
            {
                key.src_index = local_index;
                p->second.items.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(key.key),
                                        std::forward_as_tuple());
            }
        }
        return true;
    }

} // namespace autolabor::connection_aggregation
