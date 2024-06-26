#ifndef BLOOMFWD_OPT_H
#define BLOOMFWD_OPT_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#ifndef NDEBUG
struct addr_list {
	uint32_t addr;
	struct addr_list *next;
};

extern struct addr_list *bloomf_match_addrs[2];

extern unsigned long long bloomf_match_addrs_count[2];

extern struct addr_list *bloomf_maybe_addrs[2];

extern unsigned long long bloomf_maybe_addrs_count[2];
#endif

struct ipv4_prefix {
	uint32_t next_hop;
	uint32_t prefix;
	uint8_t netmask;
};

extern struct stats {
    unsigned long long bf_match;
    unsigned long long ht_match;
} stats;

struct counting_bloom_filter {
	bool *bitmap;
	uint32_t bitmap_len;
	uint8_t *counters;  /* This array is as long as 'bitmap'. */
	uint32_t capacity;
	uint8_t num_hashes;
};

struct hash_table_entry {
    uint32_t hash;
    uint32_t prefix;
    uint32_t next_hop;
    struct hash_table_entry *next;
};

struct hash_table {
    uint32_t total;  /* Number of stored keys. */
    uint32_t range;  /* Vertical length (i.e. number of buckets). */
    struct hash_table_entry **slots;
};

struct forwarding_table {
	struct ipv4_prefix *default_route;  /* 0.0.0.0/0. */
	uint32_t *dla; /* For the first 20 prefixes lengths. */
	struct counting_bloom_filter *counting_bloom_filters[2]; /* 0 -> G2, 1 -> G1 */
	struct hash_table *hash_tables[2]; /* 0 -> G2, 1 -> G1 */
};

struct ipv4_prefix *new_ipv4_prefix(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
		uint8_t netmask, uint32_t next_hop);

uint32_t new_ipv4_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

struct forwarding_table *new_forwarding_table(FILE *pfx_distribution,
		uint32_t *gw_def);

void load_prefixes(struct forwarding_table *fw_tbl, FILE *pfxs);

/* Scalar */
bool lookup_address(const struct forwarding_table *fw_tbl,
		uint32_t addr, uint32_t *next_hop);

void lookup_address_intrin(const struct forwarding_table *fw_tbl,
		uint32_t g2_addrs[16], bool found[16], uint32_t next_hops[16]);
#endif

