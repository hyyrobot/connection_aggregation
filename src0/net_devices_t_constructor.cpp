auto loop = true;
uint8_t buffer[8192];
while (loop)
{
    auto pack_size = read(_netlink, buffer, sizeof(buffer));
    if (pack_size < 0)
    {
        std::stringstream builder;
        builder << "Failed to read netlink, because: " << strerror(errno);
        throw std::runtime_error(builder.str());
    }

    for (auto h = reinterpret_cast<const nlmsghdr *>(buffer); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, pack_size))
        switch (h->nlmsg_type)
        {
        case RTM_NEWADDR:
        {
            const char *name = nullptr;
            in_addr address;

            ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(h);
            const rtattr *attributes[IFLA_MAX + 1]{};
            const rtattr *rta = IFA_RTA(ifa);
            auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
            for (; RTA_OK(rta, l); rta = RTA_NEXT(rta, l))
                switch (rta->rta_type)
                {
                case IFA_LABEL:
                    name = reinterpret_cast<char *>(RTA_DATA(rta));
                    break;
                case IFA_LOCAL:
                    address = *reinterpret_cast<in_addr *>(RTA_DATA(rta));
                    break;
                }
            if (name && std::strcmp(name, _tun_name) != 0 && std::strcmp(name, "lo") != 0)
            {
                auto [p, _] = _devices.try_emplace(ifa->ifa_index, name);
                p->second.push_address(address);
            }
        }
        break;
        case NLMSG_DONE:
            loop = false;
            break;
        }
}

{ // 显示内部细节
    std::stringstream builder;
    builder << _tun_name << '(' << _tun_index << ')' << std::endl;
    char text[16];
    for (const auto &device : _devices)
    {
        auto address = device.second.address();
        inet_ntop(AF_INET, &address, text, sizeof(text));
        builder << device.second.name() << '(' << device.first << "): " << text << std::endl;
    }
    std::cout << builder.str() << std::endl;
}
