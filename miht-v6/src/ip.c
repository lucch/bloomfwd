#include <assert.h>
#include <stdlib.h>

#include "ip.h"

struct ip_prefix ip_prefix(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint8_t len, uint128 next_hop)
{
	struct ip_prefix pfx;

	pfx.prefix = ((uint64_t)a << 48 | (uint64_t)b << 32 | (uint64_t)c << 16 | d) >> (64 - len);
	pfx.len = len;
	pfx.next_hop = next_hop;

	assert(is_prefix_valid(&pfx));

	return pfx;
}

uint128 ip_addr(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		 uint16_t e, uint16_t f, uint16_t g, uint16_t h)
{
	uint64_t hi = (uint64_t)a << 48 | (uint64_t)b << 32 | (uint64_t)c << 16 | (uint64_t)d;
	uint64_t lo = (uint64_t)e << 48 | (uint64_t)f << 32 | (uint64_t)g << 16 | (uint64_t)h;
	return NEW_UINT128(hi, lo);
}

bool is_prefix_valid(const struct ip_prefix *pfx)
{
	return pfx != NULL && pfx->len >= 0 && pfx->len <= 64;
}

