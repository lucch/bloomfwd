#ifndef IP_H
#define IP_H

#include <stdbool.h>
#include <stdint.h>

struct ip_prefix {
	uint32_t prefix;
	uint32_t next_hop;
	uint8_t len;
};

struct ip_prefix ip_prefix(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
		uint8_t len, uint32_t next_hop);

//extern inline uint32_t ip_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
//{
//	return (uint32_t)a << 24 | b << 16 | c << 8 | d;
//}
//
//extern inline bool is_prefix_valid(const struct ip_prefix *pfx)
//{
//	return pfx != NULL && pfx->len >= 0 && pfx->len <= 32;
//}

uint32_t ip_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

bool is_prefix_valid(const struct ip_prefix *pfx);

#endif

