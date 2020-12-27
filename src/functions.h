#include <ostream>
#include <netinet/ip.h>
#include <netinet/udp.h>

std::ostream &operator<<(std::ostream &, const ip &);

uint16_t checksum(const void *, size_t);
void fill_checksum_ip(ip *);

struct udp_ip_t
{
    uint16_t length;
    uint8_t zero, protocol;
    in_addr src, dst;
};

constexpr size_t udp_ip_offset = sizeof(ip) - sizeof(udp_ip_t);

void fill_checksum_udp(udp_ip_t *);
