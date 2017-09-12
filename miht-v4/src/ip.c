#include <assert.h>
#include <stdlib.h>

#include "ip.h"

struct ip_prefix ip_prefix(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
		uint8_t len, uint32_t next_hop)
{
	struct ip_prefix pfx;

	//pfx.prefix = (a << 24 | b << 16 | c << 8 | d) &
	//	(0xffffffff << (32 - len));
	pfx.prefix = (unsigned int)(a << 24 | b << 16 | c << 8 | d) >> (32 - len);
	pfx.len = len;
	pfx.next_hop = next_hop;

	assert(is_prefix_valid(&pfx));

	return pfx;
}

uint32_t ip_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return a << 24 | b << 16 | c << 8 | d;
}

bool is_prefix_valid(const struct ip_prefix *pfx)
{
	return pfx != NULL && pfx->len >= 0 && pfx->len <= 32;
}

