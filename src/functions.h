#include <ostream>
#include <netinet/ip.h>

std::ostream &operator<<(std::ostream &, const ip &);

uint16_t check_sum(const void *, size_t);

void fill_checksum(ip*);
