/* 
 * bloomfwd_opt.c
 *
 * Copyright (C) 2016  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 *
 * A Bloom filter is a probabilistic data structure tunned by 3 parameters in
 * order to achieve a given (desired) false probability ratio:
 *
 *	- The maximum amount of elements it can store;
 *	- The maximum size (in terms of storage) it can have;
 *	- The number of hash functions that must be computed on each lookup/store.
 *
 * This module implements an IPv4 Forwarding Table using hash tables and
 * Counting Bloom filters, which is a variant of standard Bloom filters that
 * allows update operations.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bloomfwd_opt.h"
#include "config.h"
#include "prettyprint.h"
#include "hashfunctions.h"


#pragma offload_attribute (push,target(mic))
//__declspec(target(mic)) struct forwarding_table *fw_tbl = NULL;
struct forwarding_table *fw_tbl = NULL;

uint32_t new_ipv4_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return a << 24 | b << 16 | c << 8 | d;
}

static inline bool is_prefix_valid(const struct ipv4_prefix *pfx)
{
	return pfx != NULL && pfx->netmask >= 0 && pfx->netmask <= 32;
}

struct ipv4_prefix *new_ipv4_prefix(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
		uint8_t netmask, uint32_t next_hop)
{
	struct ipv4_prefix *pfx = malloc(sizeof(struct ipv4_prefix));
	if (pfx == NULL) {
		printf("bloomfwd.new_ipv4_prefix: Couldn't malloc IPv4 prefix.\n");
		exit(1);
	}

	pfx->prefix = (a << 24 | b << 16 | c << 8 | d) &
		(0xffffffff << (32 - netmask));
	pfx->netmask = netmask;
	pfx->next_hop = next_hop;

	if (!is_prefix_valid(pfx)) {
		free(pfx);
		pfx = NULL;
	}

	return pfx;
}

static struct hash_table *new_hash_table(uint32_t capacity)
{
	assert(capacity > 0);
	/*
	 * qLibc's author recomendation:
	 *
	 * Setting the right range is a magic. In practice, pick a value between:
	 *
	 * (total keys / 3) ~ (total keys * 2).
	 *
	 * Thus, choosing 'total keys' seems reasonable.
	 */
	/* My tests showed that 'total keys' has better performance. */
//	uint32_t range = capacity / 3;
	uint32_t range = capacity;
//	uint32_t range = 2 * capacity;

	struct hash_table *tbl = malloc(sizeof(struct hash_table));
	if (tbl == NULL) {
		printf("new_hash_table: Couldn't malloc hash table.\n");
		exit(1);
	}

	/* Allocate table space. */
	tbl->slots = (struct hash_table_entry **)malloc(
			range * sizeof(struct hash_table_entry *));
	if (tbl->slots == NULL) {
		printf("new_hash_table: Couldn't allocate memory for 'tbl->slots'.\n");
		exit(1);
	}

	/* Initialize table data. */
	for (uint32_t i = 0; i < range; i++)
		tbl->slots[i] = NULL;
	tbl->total = 0;
	tbl->range = range;

	return tbl;
}


static bool store_next_hop(struct hash_table *tbl, uint32_t pfx_key,
		uint32_t next_hop)
{
	uint32_t hash = HASHTBL_HASH_FUNCTION(pfx_key);
	uint32_t idx = hash % tbl->range;

	/* Find key. */
	struct hash_table_entry *entry;
	for (entry = tbl->slots[idx]; entry != NULL; entry = entry->next) {
		if (entry->hash == hash && entry->prefix == pfx_key)
			break;
	}

	bool create = entry == NULL;
	if (create) { /* Create */
		entry = malloc(sizeof(struct hash_table_entry));
		if (entry == NULL) {
			printf("qhashtbl.qhashtbl_put: Couldn't allocate memory.\n");
			exit(1);
		}

		/* Set key data. */
		entry->hash = hash;
		entry->prefix = pfx_key;

		/* Always insert new entryects at the beginning of the list. */
		entry->next = tbl->slots[idx];
		tbl->slots[idx] = entry;

		tbl->total++;
	}

	/* Set the next hop (both for create and update operations). */
	entry->next_hop = next_hop;

	return create;
}

#ifdef SAME_HASH_FUNCTIONS
/*
 * Useful for reusing precomputed hash.
 */
static inline bool find_next_hop_with_hash(struct hash_table *tbl, uint32_t hash,
		uint32_t pfx_key, uint32_t *next_hop)
{
	uint32_t idx = hash % tbl->range;

	struct hash_table_entry *entry;
	for (entry = tbl->slots[idx]; entry != NULL; entry = entry->next) {
		if (entry->hash == hash && entry->prefix == pfx_key)
			break;
	}

	bool found = entry != NULL;
	if (found)
		*next_hop = entry->next_hop;

	return found;
}
#else
static bool find_next_hop(struct hash_table *tbl, uint32_t pfx_key,
		uint32_t *next_hop)
{
	uint32_t hash = HASHTBL_HASH_FUNCTION(pfx_key);
	uint32_t idx = hash % tbl->range;

	struct hash_table_entry *entry;
	for (entry = tbl->slots[idx]; entry != NULL; entry = entry->next) {
		if (entry->hash == hash && entry->prefix == pfx_key)
			break;
	}

	bool found = entry != NULL;
	if (found)
		*next_hop = entry->next_hop;

	return found;
}
#endif

static struct counting_bloom_filter *new_counting_bloom_filter(uint32_t capacity)
{
	struct counting_bloom_filter *bf =
		malloc(sizeof(struct counting_bloom_filter));
	if (bf == NULL) {
		printf("new_counting_bloom_filter: Couldn't malloc bloom filter.\n");
		exit(1);
	}

	/*
	 * These formulas give the optimal bitmap size (len) and number of
	 * hashes (bf_num_hashes) based on the amount of elements do be stored
	 * (capacity) and the desired false positive ratio
	 * (FALSE_POSITIVE_RATIO).
	 */
	uint32_t bitmap_len = ceil((capacity *  log2(1.0 / FALSE_POSITIVE_RATIO)) / log(2.0)); 
	bf->num_hashes = ceil(log(2.0) * bitmap_len / capacity);

	bf->bitmap = malloc(bitmap_len * sizeof(bool));
	if (bf->bitmap == NULL) {
		printf("new_counting_bloom_filter: Could not calloc bitmap array of size: %"PRIu32".\n", bitmap_len);
		exit(1);
	}
	/* Initialize array elements to `false`. */
	memset(bf->bitmap, false, bitmap_len * sizeof(bool));

	/* Initialize counters to 0. */
	bf->counters = calloc(bitmap_len, sizeof(uint8_t));
	if (bf->counters == NULL) {
		printf("new_counting_bloom_filter: Could not calloc counters array of size: %"PRIu32".\n", bitmap_len);
		exit(1);
	}
	bf->bitmap_len = bitmap_len;
	bf->capacity = capacity;

	return bf;
}

static inline bool set_default_route(uint32_t gw_def)
{
	bool create = fw_tbl->default_route == NULL;
	if (create) {  /* Create default route. */
		struct ipv4_prefix *def_route = malloc(sizeof(struct ipv4_prefix));
		if (def_route == NULL) {
			printf("set_default_route: could not malloc default route.");
			exit(1);
		}

		def_route->prefix = 0;
		def_route->netmask = 0;
		def_route->next_hop = gw_def;
		fw_tbl->default_route = def_route;
	} else {  /* Update default route. */
		fw_tbl->default_route->next_hop = gw_def;
	}

	return create;
}

static inline int bloom_filter_id(const struct ipv4_prefix *pfx)
{
	return pfx->netmask == 32 ? 0 : 1; /* 0 -> G2, 1 -> G1 */
}

static void init_cbf(FILE *pfx_distribution)
{
	//uint32_t total = 0;

	if (pfx_distribution != NULL) {
		/*
		 * Choose 'bitmap_len' wisely for each Bloom filter from the
		 * prefixes distribution.
		 */
		uint8_t netmask;
		uint32_t quantity;

		int rc;
		while ((rc = fscanf(pfx_distribution, "%"SCNu8 " %"SCNu32 "\n",
						&netmask, &quantity)) != EOF) {
			if (rc != 2) {
				printf("init_cbf: couldn't read prefixes distribution file.\n");
				exit(1);
			}

			if (netmask == 32) {
				fw_tbl->counting_bloom_filters[0] = new_counting_bloom_filter(quantity);
			} else if (netmask == 24) {
				fw_tbl->counting_bloom_filters[1] = new_counting_bloom_filter(quantity);
			}
		}
	}
}

static inline void init_ht()
{
	struct hash_table **hash_tables = fw_tbl->hash_tables;
	struct counting_bloom_filter **bfs = fw_tbl->counting_bloom_filters;
	for (int i = 0; i < 2; i++) {
		if (bfs[i] != NULL)
			hash_tables[i] = new_hash_table(bfs[i]->capacity);
		else
			hash_tables[i] = NULL;
	}
}

/*
 * Allocates memory for storing prefixes of length in the range [1, 20]
 * initializing each position with 0 (assumed default route).
 */
static void init_dla()
{
	fw_tbl->dla = calloc(pow(2, 20), sizeof(uint32_t));
	if (fw_tbl->dla == NULL) {
		printf("init_dla: calloc error!\n");
		exit(1);
	}
}

void init_fwtbl(const char *distrib_path, uint32_t *gw_def)
{
	FILE *pfx_distribution = fopen(distrib_path, "r");
	if (pfx_distribution == NULL) {
		printf("init_fwtbl: couldn't open prefixes distribution file: '%s'.\n", distrib_path);
		exit(1);
	}

	fw_tbl = malloc(sizeof(struct forwarding_table));
	if (fw_tbl == NULL) {
		printf("bloomfwd.new_forwarding_table: could not malloc forwarding table.\n");
		exit(1);
	}

	fw_tbl->default_route = NULL;  /* Init default route. */
	init_dla();
	init_cbf(pfx_distribution);
	init_ht();

	fclose(pfx_distribution);
}


static inline void hashes(uint32_t key, uint8_t num_hashes, uint32_t *result)
{
	assert(result != NULL);

	result[0] = BLOOM_HASH_FUNCTION(key); 
	if (num_hashes > 1) {
		result[1] = BLOOM_HASH_FUNCTION(result[0]); 

		/*
		 * The technique below allows us to produce multiple hash values without
		 * any loss in the asymptotic false positive probability. It was taken
		 * from paper: "Less Hashing, Same Performance: Building a Better Bloom
		 * Filter".
		 */
		for (int i = 2; i < num_hashes; i++)
			result[i] = (result[0] + i * result[1]);
	}
}

static bool store_prefix(const struct ipv4_prefix *pfx)
{
	if (!is_prefix_valid(pfx)) {
		printf("store_prefix: Invalid prefix!\n");
		exit(1);
	}

	if (pfx->netmask == 0)
		return set_default_route(pfx->next_hop);

	bool created;
	if (pfx->netmask == 20) {
		uint32_t index = pfx->prefix >> (32 - pfx->netmask);
		created = fw_tbl->dla[index] == 0;
		fw_tbl->dla[index] = pfx->next_hop;
	} else {
		int id = bloom_filter_id(pfx);
		struct counting_bloom_filter *bf = fw_tbl->counting_bloom_filters[id];
		struct hash_table *hash_tbl = fw_tbl->hash_tables[id];
		bool *bitmap = bf->bitmap;
		uint32_t bitmap_len = bf->bitmap_len;
		uint8_t *counters = bf->counters;
		uint8_t num_hashes = bf->num_hashes;

		created = store_next_hop(hash_tbl, pfx->prefix, pfx->next_hop);

		uint32_t bitmap_idxs[num_hashes];
		hashes(pfx->prefix, num_hashes, bitmap_idxs);
		for (int i = 0; i < num_hashes; i++) {
			uint32_t idx = bitmap_idxs[i] % bitmap_len;
			bitmap[idx] = true;
			counters[idx] += 1;
		}
	}

	return created;
}

unsigned long long calc_num_collisions_hashtbl()
{
	unsigned long long num_collisions = 0;
	for (int i = 0; i < 2; i++) {
		struct hash_table *ht =
			fw_tbl->hash_tables[i];
		if (ht == NULL)
			continue;

		for (uint32_t j = 0; j < ht->range; j++) {
			struct hash_table_entry *e = ht->slots[j];
			int x = 0;
			for ( ; e != NULL; x++)
				e = e->next;
			if (x > 1)
				num_collisions += x;
		}
		
	}

	return num_collisions;
}

unsigned long long calc_num_collisions_bloomf()
{
	unsigned long long num_collisions = 0;
	for (int i = 0; i < 2; i++) {
		struct counting_bloom_filter *bf =
			fw_tbl->counting_bloom_filters[i];
		if (bf == NULL)
			continue;

		for (uint32_t j = 0; j < bf->bitmap_len; j++) {
			int x = bf->counters[i];
			if (x > 1)
				num_collisions += x;
		}
		
	}

	return num_collisions;
}

void load_prefixes(const char *pfxs_path)
{
	FILE *pfxs = fopen(pfxs_path, "r");
	if (pfxs == NULL) {
		printf("Couldn't open prefixes file: '%s'.\n", pfxs_path);
		exit(1);
	}

	uint8_t a0, b0, c0, d0, len;
	uint8_t a1, b1, c1, d1;
	while(fscanf(pfxs,"%"SCNu8".%"SCNu8".%"SCNu8".%"SCNu8,
				&a0, &b0, &c0, &d0) == 4) {
		if (fscanf(pfxs, "/%"SCNu8, &len) != 1) {
			len = 0;
			if (d0 > 0)
				len = 32;
			else if (c0 > 0)
				len = 24;
			else if (b0 > 0)
				len = 16;
			else if (a0 > 0)
				len = 8;
		}
		if(fscanf(pfxs," %"SCNu8".%"SCNu8".%"SCNu8".%"SCNu8,
				&a1, &b1, &c1, &d1) != 4) {
			printf("Couldn't parse network prefix: "
				"%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"/%"PRIu8"\n",
				a0, b0, c0, d0, len);
			exit(1);
		}
		uint32_t next_hop = new_ipv4_addr(a1, b1, c1, d1);
		struct ipv4_prefix *pfx =
			new_ipv4_prefix(a0, b0, c0, d0, len, next_hop);
		store_prefix(pfx);
	}

	fclose(pfxs);
}

/* Optimized serial implementation! */
/* Compiler is not vectorizing anything! */
bool lookup_address(uint32_t addr, uint32_t *next_hop)
{
	bool found = false;
	/* Query G2 */
	struct counting_bloom_filter *bf = fw_tbl->counting_bloom_filters[0];
	uint32_t pfx_key = addr; //& (0xffffffff << i);
	bool *bitmap = bf->bitmap;
	uint32_t bitmap_len = bf->bitmap_len;
	uint8_t num_hashes = bf->num_hashes;

	/* Calculate hash. */
	uint32_t h1 = BLOOM_HASH_FUNCTION(pfx_key);
	bool maybe = bitmap[h1 % bitmap_len];
	if (maybe) {
		if (num_hashes > 1) {
			uint32_t h2 = BLOOM_HASH_FUNCTION(h1);
			maybe = bitmap[h2 % bitmap_len];
			for (int j = 2; maybe && j < num_hashes; j++) {
				uint32_t idx = (h1 + j * h2) % bitmap_len;
				maybe = bitmap[idx];
			}
		}
		if (maybe) {
			struct hash_table *ht = fw_tbl->hash_tables[0];
#ifdef SAME_HASH_FUNCTIONS
			found = find_next_hop_with_hash(ht, h1, pfx_key,
					next_hop);
#else
			found = find_next_hop(ht, pfx_key, next_hop);
#endif
		}
	}

	if (!found) {
		/* Query G1 */
		bf = fw_tbl->counting_bloom_filters[1];
		pfx_key = addr & 0xffffff00;
		bitmap = bf->bitmap;
		bitmap_len = bf->bitmap_len;
		num_hashes = bf->num_hashes;

		/* Calculate hash. */
		h1 = BLOOM_HASH_FUNCTION(pfx_key);
		maybe = bitmap[h1 % bitmap_len];
		if (maybe) {
			if (num_hashes > 1) {
				uint32_t h2 = BLOOM_HASH_FUNCTION(h1);
				maybe = bitmap[h2 % bitmap_len];
				for (int j = 2; maybe && j < num_hashes; j++) {
					uint32_t idx = (h1 + j * h2) % bitmap_len;
					maybe = bitmap[idx];
				}
			}
			if (maybe) {
				struct hash_table *ht = fw_tbl->hash_tables[1];
#ifdef SAME_HASH_FUNCTIONS
				found = find_next_hop_with_hash(ht, h1, pfx_key,
						next_hop);
#else
				found = find_next_hop(ht, pfx_key, next_hop);
#endif
			}
		}

	}

	if (!found) {
		*next_hop = fw_tbl->dla[addr >> 12];
		if ((*next_hop) != 0) {
			found = true;
		} else if (fw_tbl->default_route != NULL) {
			*next_hop = fw_tbl->default_route->next_hop;
			found = true;
		}
	}

	return found;
}

#ifdef __MIC__

/*
 * found[16]: must be a false initialized array.
 */
void lookup_address_intrin(uint32_t g2_addrs[16], bool found[16],
		uint32_t next_hops[16])
{
	__declspec(align(64)) uint32_t g1_addrs[16];
	__declspec(align(64)) uint32_t g2_h1[16];
	__declspec(align(64)) uint32_t g2_h2[16];
	__declspec(align(64)) uint32_t g1_h1[16];
	__declspec(align(64)) uint32_t g1_h2[16];

	/* To be autovectorized */
	for (int i = 0; i < 16; i++) {
		g1_addrs[i] = g2_addrs[i] & 0xffffff00;
	}

	for (int i = 0; i < 16; i++) {
		found[i] = false;
	}

	/* Calculate hashes. */
	/* G2 */
	BLOOM_HASH_FUNCTION_INTRIN(g2_addrs, g2_h1);
	BLOOM_HASH_FUNCTION_INTRIN(g2_h1, g2_h2);
	/* G1 */
	BLOOM_HASH_FUNCTION_INTRIN(g1_addrs, g1_h1);
	BLOOM_HASH_FUNCTION_INTRIN(g1_h1, g1_h2);

	/* Query G2 */
	struct counting_bloom_filter *bf = fw_tbl->counting_bloom_filters[0];
	struct hash_table *ht = fw_tbl->hash_tables[0];
	bool *bitmap = bf->bitmap;
	uint32_t bitmap_len = bf->bitmap_len;
	uint8_t num_hashes = bf->num_hashes;
	for (int i = 0; i < 16; i++) {
		bool maybe = bitmap[g2_h1[i] % bitmap_len];
		if (maybe) {
			if (num_hashes > 1) {
				maybe = bitmap[g2_h2[i] % bitmap_len];
				for (int j = 2; maybe && j < num_hashes; j++) {
					uint32_t idx = (g2_h1[i] + j * g2_h2[i]) % bitmap_len;
					maybe = bitmap[idx];
				}
			}

			if (maybe) {
#ifdef SAME_HASH_FUNCTIONS
				found[i] = find_next_hop_with_hash(ht, g2_h1[i], g2_addrs[i],
						&next_hops[i]);
#else
				found[i] = find_next_hop(ht, g2_addrs[i], &next_hops[i]);
#endif
			}
		}
	}

	/* Query G1 */
	bf = fw_tbl->counting_bloom_filters[1];
	ht = fw_tbl->hash_tables[1];
	bitmap = bf->bitmap;
	bitmap_len = bf->bitmap_len;
	num_hashes = bf->num_hashes;
	for (int i = 0; i < 16; i++) {
		if (found[i])
			continue;

		bool maybe = bitmap[g1_h1[i] % bitmap_len];
		if (maybe) {
			if (num_hashes > 1) {
				maybe = bitmap[g1_h2[i] % bitmap_len];
				for (int j = 2; maybe && j < num_hashes; j++) {
					uint32_t idx = (g1_h1[i] + j * g1_h2[i]) % bitmap_len;
					maybe = bitmap[idx];
				}
			}

			if (maybe) {
#ifdef SAME_HASH_FUNCTIONS
				found[i] = find_next_hop_with_hash(ht, g1_h1[i], g1_addrs[i],
						&next_hops[i]);
#else
				found[i] = find_next_hop(ht, g1_addrs[i], &next_hops[i]);
#endif
			}
		}

		if (!found[i]) {
			next_hops[i] = fw_tbl->dla[g2_addrs[i] >> 12];
			if (next_hops[i] != 0) {
				found[i] = true;
			} else if (fw_tbl->default_route != NULL) {
				next_hops[i] = fw_tbl->default_route->next_hop;
				found[i] = true;
			}
		}
	}
}

#endif


#pragma offload_attribute(pop)


// Backup...

// TODO: Enclose *_mic functions in...
// #pragma offload_attribute
// (push,target(mic))
//
// <functions here!>
//
// #pragma offload_attribute(pop)
//
//static void init_dla_mic()
//{
//	fw_tbl->dla = calloc(pow(2, 20), sizeof(uint32_t));
//	if (fw_tbl->dla == NULL) {
//		printf("init_dla_mic: calloc error!\n");
//		exit(1);
//	}
//}

//void init_fwtbl_mic(const char *distrib_path, uint32_t *gw_def)
//{
//#pragma offload target(mic:0) in(distrib_path) nocopy(fw_tbl)
//	{
//		FILE *pfx_distribution = fopen(distrib_path, "r");
//		if (pfx_distribution == NULL) {
//			printf("init_fwtbl_cpu: couldn't open prefixes distribution file: '%s'.\n", distrib_path);
//			exit(1);
//		}
//
//		struct forwarding_table *fw_tbl = malloc(sizeof(struct forwarding_table));
//		if (fw_tbl == NULL) {
//			printf("bloomfwd.new_forwarding_table: could not malloc forwarding table.\n");
//			exit(1);
//		}
//
//		fw_tbl->default_route = NULL;  /* Init default route. */
//		init_dla_mic();
//		init_cbf_mic(pfx_distribution);
//		init_ht_mic();
//
//		fclose(pfx_distribution);
//	}
//}

