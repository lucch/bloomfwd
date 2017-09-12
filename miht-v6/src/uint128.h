#ifndef UINT128_H
#define UINT128_H

#include <stdint.h>

#define NEW_UINT128(hi, lo) ((uint128) { .hi = (hi), .lo = (lo) })

#define UINT128_EQ(u1, u2) ((u1).hi == (u2).hi && (u1).lo == (u2).lo)

typedef struct uint128 {
	uint64_t hi;
	uint64_t lo;
} uint128;


//extern inline uint128 NEW_UINT128(uint64_t hi, uint64_t lo) {
//	return (uint128){ .hi = hi, .lo = lo }; 
//}

#endif

