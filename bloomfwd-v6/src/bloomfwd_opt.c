/* 
 * bloomfwd.c
 *
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
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


uint128 new_ipv6_addr(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint16_t e, uint16_t f, uint16_t g, uint16_t h)
{
	uint64_t hi = (uint64_t)a << 48 | (uint64_t)b << 32 | (uint64_t)c << 16 | (uint64_t)d;
	uint64_t lo = (uint64_t)e << 48 | (uint64_t)f << 32 | (uint64_t)g << 16 | (uint64_t)h;
	return NEW_UINT128(hi, lo);
}

static inline bool is_prefix_valid(const struct ipv6_prefix *pfx)
{
	return pfx != NULL && pfx->len >= 0 && pfx->len <= 64;
}

static inline uint64_t prefix_key(uint64_t prefix, uint8_t len)
{
	//assert(len <= 64);
	return prefix & (0xffffffffffffffff << (64 - len));
}

struct ipv6_prefix *new_ipv6_prefix(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
		uint8_t len, uint128 next_hop)
{
	struct ipv6_prefix *pfx = malloc(sizeof(struct ipv6_prefix));
	//assert (pfx != NULL);

	uint64_t prefix = (uint64_t)a << 48 | (uint64_t)b << 32 | (uint64_t)c << 16 | (uint64_t)d;
	pfx->prefix = prefix_key(prefix, len);
	pfx->len = len;
	pfx->next_hop = next_hop;

	if (!is_prefix_valid(pfx)) {
		free(pfx);
		pfx = NULL;
	}

	return pfx;
}

static struct hash_table *new_hash_table(uint32_t capacity)
{
	//assert(capacity > 0);
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
		fprintf(stderr, "hash_table.new_hash_table: Couldn't malloc hash table.\n");
		exit(1);
	}

	/* Allocate table space. */
	tbl->slots = (struct hash_table_entry **)malloc(
			range * sizeof(struct hash_table_entry *));
	if (tbl->slots == NULL) {
		fprintf(stderr, "hash_table.new_hash_table: Couldn't allocate memory for 'tbl->slots'.\n");
		exit(1);
	}

	/* Initialize table data. */
	for (uint32_t i = 0; i < range; i++)
		tbl->slots[i] = NULL;
	tbl->total = 0;
	tbl->range = range;

	return tbl;
}

static bool store_next_hop(struct hash_table *tbl, uint64_t pfx_key,
		uint128 next_hop)
{
	uint32_t hash = HASHTBL_HASH_FUNCTION_64(pfx_key);
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
			fprintf(stderr, "qhashtbl.qhashtbl_put: Couldn't allocate memory.\n");
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
		uint64_t pfx_key, uint128 *next_hop)
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
static bool find_next_hop(struct hash_table *tbl, uint64_t pfx_key,
		uint128 *next_hop)
{
	uint32_t hash = HASHTBL_HASH_FUNCTION_64(pfx_key);
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
		fprintf(stderr, "bloomfwd.new_counting_bloom_filter: Couldn't malloc bloom filter.\n");
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
		fprintf(stderr, "bloomfwd.new_counting_bloom_filter: Could not calloc bitmap array of size: %"PRIu32".\n", bitmap_len);
		exit(1);
	}
	/* Initialize array elements to `false`. */
	memset(bf->bitmap, false, bitmap_len * sizeof(bool));

	/* Initialize counters to 0. */
	bf->counters = calloc(bitmap_len, sizeof(uint8_t));
	if (bf->counters == NULL) {
		fprintf(stderr, "bloomfwd.new_counting_bloom_filter: Could not calloc counters array of size: %"PRIu32".\n", bitmap_len);
		exit(1);
	}
	bf->bitmap_len = bitmap_len;
	bf->capacity = capacity;

	return bf;
}

static inline bool set_default_route(struct forwarding_table *fw_tbl, uint128 gw_def)
{
	bool create = fw_tbl->default_route == NULL;
	if (create) {  /* Create default route. */
		struct ipv6_prefix *def_route = malloc(sizeof(struct ipv6_prefix));
		if (def_route == NULL) {
			fprintf(stderr, "bloomfwd.set_default_route: Could not malloc default route.");
			exit(1);
		}

		def_route->prefix = 0;
		def_route->len = 0;
		def_route->next_hop = gw_def;
		fw_tbl->default_route = def_route;
	} else {  /* Update default route. */
		fw_tbl->default_route->next_hop = gw_def;
	}

	return create;
}

static inline void init_hash_tables_array(struct forwarding_table *fw_tbl)
{
	struct hash_table **hash_tables = fw_tbl->hash_tables;
	struct counting_bloom_filter **bfs = fw_tbl->counting_bloom_filters;
	for (int i = 0; i < 64; i++) {
		if (bfs[i] != NULL)
			hash_tables[i] = new_hash_table(bfs[i]->capacity);
		else
			hash_tables[i] = NULL;
	}
}

static inline int bloom_filter_id(const struct ipv6_prefix *pfx)
{
	return 64 - pfx->len;
}

/*
 * Create a Bloom filter array of size 32. Each filter stores prefixes whose
 * netmask is equal to 32 - i, where i is the filter's index, i.e.
 * 'bloom_filter_arr[8]' will contain only prefixes of 24 bits. Each Bloom
 * filter is allocated according to the maximum number of elements it'll hold,
 * which is taken from a prefixes distribution file. Each line of this file
 * should contain the size of the prefix in bits (or netmask) followed by the
 * maximum number of prefixes of that size, for instance:
 *	
 *	1 5
 *	2 3
 *	...
 *	32 6
 *
 * As IPv4 address are 32 bits long, the first column MUST range from 1 to 32.
 */
static void init_counting_bloom_filters_array(FILE *pfx_distribution,
		bool *has_prefix_length, uint8_t *distinct_lengths,
		uint8_t **bf_ids, struct counting_bloom_filter *bf_arr[])
{
	memset(has_prefix_length, false, 64 * sizeof(bool));

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
				fprintf(stderr, "Couldn't read prefixes distribution file.\n");
				exit(1);
			}

			if (quantity > 0) {
				int bf_id = 64 - netmask;
				bf_arr[bf_id] = new_counting_bloom_filter(quantity);
				if (bf_arr[bf_id] == NULL) {
					fprintf(stderr, "ERRORR\n");
					exit(1);
				}
				has_prefix_length[bf_id] = true; 
				(*distinct_lengths)++;
			}
		}
	}

#if defined(LOOKUP_VEC_INTRIN) && defined(__MIC__)
	*bf_ids = _mm_malloc((*distinct_lengths) * sizeof(uint8_t), 64);
#else
	*bf_ids = malloc((*distinct_lengths) * sizeof(uint8_t));
#endif
	if (bf_ids == NULL) {
		fprintf(stderr, "MERDA\n");
		exit(1);
	}
	for (int i = 0, j = 0; i < 64; i++)
		if (!has_prefix_length[i])
			bf_arr[i] = NULL;
		else
			(*bf_ids)[j++] = i;

#ifndef NDEBUG
	/* Print Bloom filters info. */
	printf("False positive ratio = %.2lf\n", FALSE_POSITIVE_RATIO);
	for (int i = 0; i < 64; i++) {
		if (bf_arr[i] != NULL) {
			printf("#%2d: num_hashes = %"PRIu8" bitmap_len = %d\n",
				i, bf_arr[i]->num_hashes, bf_arr[i]->bitmap_len);
		} else {
			printf("#%2d: EMPTY\n", i);
		}
	}
	printf("\n");
#endif
}

struct forwarding_table *new_forwarding_table(FILE *pfx_distribution,
		uint128 *gw_def)
{
	struct forwarding_table *fw_tbl = malloc(sizeof(struct forwarding_table));
	if (fw_tbl == NULL) {
		fprintf(stderr, "bloomfwd.new_forwarding_table: Could not malloc forwarding table.\n");
		exit(1);
	}

	fw_tbl->default_route = NULL;  /* Init default route. */
	init_counting_bloom_filters_array(pfx_distribution,
			fw_tbl->has_prefix_length, &fw_tbl->distinct_lengths,
			&fw_tbl->bf_ids, fw_tbl->counting_bloom_filters);
	init_hash_tables_array(fw_tbl);

	return fw_tbl;
}


static inline void hashes(uint64_t key, uint8_t num_hashes, uint32_t *result)
{
	//assert(result != NULL);

	result[0] = BLOOM_HASH_FUNCTION_64(key); 
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

static bool store_prefix(struct forwarding_table *fw_tbl,
		const struct ipv6_prefix *pfx)
{
	if (!is_prefix_valid(pfx)) {
		char *prefix_str = strpfx(pfx);
		fprintf(stderr, "bloomfwd.store_prefix: Invalid prefix: %s.\n",
				prefix_str);
		free(prefix_str);
		exit(1);
	}

	if (pfx->len == 0)
		return set_default_route(fw_tbl, pfx->next_hop);
	
	int id = bloom_filter_id(pfx);
	struct counting_bloom_filter *bf = fw_tbl->counting_bloom_filters[id];
	struct hash_table *hash_tbl = fw_tbl->hash_tables[id];
	bool *bitmap = bf->bitmap;
	uint32_t bitmap_len = bf->bitmap_len;
	uint8_t *counters = bf->counters;
	uint8_t num_hashes = bf->num_hashes;

	bool created = store_next_hop(hash_tbl, pfx->prefix, pfx->next_hop);

	uint32_t bitmap_idxs[num_hashes];
	hashes(pfx->prefix, num_hashes, bitmap_idxs);
	for (int i = 0; i < num_hashes; i++) {
		uint32_t idx = bitmap_idxs[i] % bitmap_len;
		bitmap[idx] = true;
		counters[idx] += 1;
	}

	return created;
}

void load_prefixes(struct forwarding_table *fw_tbl, FILE *pfxs)
{
	if (pfxs == NULL) {
		fprintf(stderr, "load error!\n");
		exit(1);
	}

	int ignored = 0;
	unsigned char len;
	unsigned a0 = 0, b0 = 0, c0 = 0, d0 = 0, e0 = 0, f0 = 0, g0 = 0, h0 = 0;
	unsigned a1 = 0, b1 = 0, c1 = 0, d1 = 0, e1 = 0, f1 = 0, g1 = 0, h1 = 0;
	while (fscanf(pfxs, "%x:%x:%x:%x:%x:%x:%x:%x/%hhu\n",
				&a0, &b0, &c0, &d0, &e0, &f0, &g0, &h0, &len) == 9) {
		if (fscanf(pfxs, "%x:%x:%x:%x:%x:%x:%x:%x",
				&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1) != 8) {
			fprintf(stderr, "bloomfwd_opt.load_prefixes: fscanf error!\n");
			exit(1);
		}
		if (len > 64) {  /* Only prefixes up to 64 bits are allowed. */
			if (ignored++ == 0)
				printf("Ignored prefixes:\n");
			printf("\t%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu\n",
				a0, b0, c0, d0, e0, f0, g0, h0, len);
			continue;
		}
		uint128 next_hop = new_ipv6_addr(a1, b1, c1, d1, e1, f1, g1, h1);
		struct ipv6_prefix *pfx = new_ipv6_prefix(a0, b0, c0, d0, len, next_hop);
		store_prefix(fw_tbl, pfx);
	}
}

/* Optimized serial implementation! */
/* Compiler is not vectorizing anything! */
bool lookup_address(const struct forwarding_table *fw_tbl, uint128 addr,
		uint128 *next_hop)
{
	uint64_t addr_hi = addr.hi;

	bool found = false;
	for (int i = 0; !found && i < 64; i++) {
		if (!fw_tbl->has_prefix_length[i])
			continue;

		struct counting_bloom_filter *bf =
			fw_tbl->counting_bloom_filters[i];
		if (bf == NULL)
			continue;

		uint64_t pfx_key = prefix_key(addr_hi, 64 - i);

		bool *bitmap = bf->bitmap;
		uint32_t bitmap_len = bf->bitmap_len;
		uint8_t num_hashes = bf->num_hashes;

		/* Calculate hash. */
		uint32_t h1 = BLOOM_HASH_FUNCTION_64(pfx_key);
        // TMP
//            printf("%u\n", h1);
//            continue;
        // ENDTMP
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
//				#pragma omp critical
//				stats.bf_match = stats.bf_match + 1;

				struct hash_table *ht = fw_tbl->hash_tables[i];
#ifdef SAME_HASH_FUNCTIONS
				found = find_next_hop_with_hash(ht, h1, pfx_key,
						next_hop);
#else
				found = find_next_hop(ht, pfx_key, next_hop);
#endif
//				if (found) {
//					#pragma omp critical
//					stats.ht_match = stats.ht_match + 1;
//				}
			}
		}
	}

	if (!found && fw_tbl->default_route != NULL) {
		*next_hop = fw_tbl->default_route->next_hop;
		found = true;
	}

	return found;
}

// Initialize in main()
//struct stats stats;

#if defined(__MIC__) && defined(LOOKUP_VEC_INTRIN)
#pragma optimization_level 2 // NOTE: DO NOT REMOVE THIS #PRAGMA!
void lookup_address_intrin(const struct forwarding_table *fw_tbl,
		uint128 *addrs, uint128 *next_hops, bool *found_vec, size_t len)
{
	uint8_t distinct_lengths = fw_tbl->distinct_lengths;
	uint8_t *bf_ids = fw_tbl->bf_ids;

	size_t prefix_keys_len = len * distinct_lengths;
	if (prefix_keys_len % 16 != 0) {
		fprintf(stderr, "lookup error!\n");
		exit(1);
	}

	__declspec(align(64)) uint64_t pfx_keys[prefix_keys_len];
	__declspec(align(64)) uint32_t h1[prefix_keys_len];
	__declspec(align(64)) uint32_t h1_tmp[32];
	__declspec(align(64)) uint32_t h2[prefix_keys_len];

	/* Calculate keys. */
	for (int i = 0, k = 0; i < len; i++)
		for (int j = 0; j < distinct_lengths; j++)
			pfx_keys[k++] = prefix_key(addrs[i].hi, 64 - bf_ids[j]);

	/* Calculate hashes. */
	/* `prefix_keys_len` is always a multiple of 16! */
	/* max(120) (bigger case) = 960 (prefix_key_len) / 8 (iterations):
	 * 	prefix_key_len = 960: (distinct_lengths = 60) * (len = 16)
	 * Then: 16 <= prefix_key_len <= 960, prefix_keys_len `mod` 64 == 0.
	 */
	/* IMPORTANT: Pragma `loop_count min(2) max(120)` didn't make any
	 * difference.  Even with `unroll(2)` it didn't generate the desired
	 * result. Just add those pragmas and check the compiler's optimization
	 * report (*.optrpt) in case of doubt.  Thus, I perform a manual
	 * unrolling below. */
	//for (int i = 0; i < prefix_keys_len; i += 8) {
	for (int i = 0; i < prefix_keys_len; i += 16) {
		BLOOM_HASH_FUNCTION_INTRIN_64(pfx_keys + i, h1_tmp);
		BLOOM_HASH_FUNCTION_INTRIN_64(pfx_keys + i + 8, h1_tmp + 16);
		h1[i] = h1_tmp[1];
		h1[i + 1] = h1_tmp[3];
		h1[i + 2] = h1_tmp[5];
		h1[i + 3] = h1_tmp[7];
		h1[i + 4] = h1_tmp[9];
		h1[i + 5] = h1_tmp[11];
		h1[i + 6] = h1_tmp[13];
		h1[i + 7] = h1_tmp[15];
		h1[i + 8] = h1_tmp[17];
		h1[i + 9] = h1_tmp[19];
		h1[i + 10] = h1_tmp[21];
		h1[i + 11] = h1_tmp[23];
		h1[i + 12] = h1_tmp[25];
		h1[i + 13] = h1_tmp[27];
		h1[i + 14] = h1_tmp[29];
		h1[i + 15] = h1_tmp[31];
	}

    // TMP
//    for (int i = 0; i < prefix_keys_len; i++) {
//        printf("%u\n", h1[i]);
//    }
//    exit(1);
    // ENDTMP

	for (int i = 0; i < prefix_keys_len; i += 16) {
		BLOOM_HASH_FUNCTION_INTRIN(h1 + i, h2 + i);
	}

	for (int i = 0; i < len; i++) {
		int j = distinct_lengths * i;
		bool found = false;
		for (int k = 0; !found && k < distinct_lengths; k++) {
			struct counting_bloom_filter *bf =
				fw_tbl->counting_bloom_filters[bf_ids[k]];
			if (bf == NULL) {
				fprintf(stderr, "lookup error!\n");
				exit(1);
			}

			bool *bitmap = bf->bitmap;
			uint32_t bitmap_len = bf->bitmap_len;
			uint8_t num_hashes = bf->num_hashes;

			bool maybe = bitmap[h1[j + k] % bitmap_len];
			if (maybe && num_hashes > 1)
				maybe = bitmap[h2[j + k] % bitmap_len];
			for (int l = 2; maybe && l < num_hashes; l++) {
				uint32_t idx = (h1[j + k] + l * h2[j + k]) % bitmap_len;
				maybe = bitmap[idx];
			}

			if (maybe) {
//				#pragma omp critical
//				stats.bf_match = stats.bf_match + 1;

				struct hash_table *ht = fw_tbl->hash_tables[bf_ids[k]];
#ifdef SAME_HASH_FUNCTIONS
				found = find_next_hop_with_hash(ht, h1[j + k], pfx_keys[j + k],
						&next_hops[i]);
#else
				found = find_next_hop(ht, pfx_keys[j + k], &next_hops[i]);
#endif

//				if (found) {
//					#pragma omp critical
//					stats.ht_match = stats.ht_match + 1;
//				}
			}
		}

		if (!found && fw_tbl->default_route != NULL) {
			next_hops[i] = fw_tbl->default_route->next_hop;
			found = true;
		}
#ifndef NDEBUG
		found_vec[i] = found;
#endif
	}
}
#endif  /* __MIC__ && LOOKUP_VEC_INTRIN */

