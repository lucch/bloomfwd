#ifndef BLOOMFWD_OPT_H
#define BLOOMFWD_OPT_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "uint128.h"

struct ipv6_prefix {
	uint128 next_hop;
	uint64_t prefix;
	uint8_t len;
};

//extern struct stats {
//	unsigned long long bf_match;
//	unsigned long long ht_match;
//} stats;

struct counting_bloom_filter {
	bool *bitmap;
	uint32_t bitmap_len;
	uint8_t *counters;  /* This array is as long as 'bitmap'. */
	uint32_t capacity;
	uint8_t num_hashes;
};

struct hash_table_entry {
    uint32_t hash;
    uint64_t prefix;
    uint128 next_hop;
    struct hash_table_entry *next;
};

struct hash_table {
    uint32_t total;  /* Number of stored keys. */
    uint32_t range;  /* Vertical length (i.e. number of buckets). */
    struct hash_table_entry **slots;
};

struct forwarding_table {
	struct ipv6_prefix *default_route;  /* 0.0.0.0/0. */
	struct counting_bloom_filter *counting_bloom_filters[64];
	struct hash_table *hash_tables[64];
	bool has_prefix_length[64];
	uint8_t distinct_lengths;
	uint8_t *bf_ids;
};

struct ipv6_prefix *new_ipv6_prefix(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint8_t len, uint128 next_hop);

uint128 new_ipv6_addr(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint16_t e, uint16_t f, uint16_t g, uint16_t h);

struct forwarding_table *new_forwarding_table(FILE *pfx_distribution,
		uint128 *gw_def);

void load_prefixes(struct forwarding_table *fw_tbl, FILE *pfxs);

/* Scalar */
bool lookup_address(const struct forwarding_table *fw_tbl,
		uint128 addr, uint128 *next_hop);

/* MIC */
void lookup_address_intrin(const struct forwarding_table *fw_tbl,
		uint128 *addrs, uint128 *next_hops, bool *found_vec, size_t len);

#endif

