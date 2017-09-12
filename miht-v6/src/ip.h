#ifndef IP_H
#define IP_H

#include <stdbool.h>
#include <stdint.h>

#include "uint128.h"

struct ip_prefix {
	uint64_t prefix;
	uint128 next_hop;
	uint8_t len;
};

struct ip_prefix ip_prefix(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint8_t len, uint128 next_hop);

//extern inline uint32_t ip_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
//{
//	return (uint32_t)a << 24 | b << 16 | c << 8 | d;
//}
//
//extern inline bool is_prefix_valid(const struct ip_prefix *pfx)
//{
//	return pfx != NULL && pfx->len >= 0 && pfx->len <= 32;
//}

uint128 ip_addr(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint16_t e, uint16_t f, uint16_t g, uint16_t h);

bool is_prefix_valid(const struct ip_prefix *pfx);

#endif

