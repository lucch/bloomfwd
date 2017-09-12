#ifndef MIHT_H
#define MIHT_H

#include <stdbool.h>

#include "ip.h"

struct ptrie_node {
	bool is_priority;
	int suffix;
	int len;  /* Suffix length. */
	unsigned int next_hop;
	struct ptrie_node *left;
	struct ptrie_node *right;
};

struct bplus_node {
	bool is_leaf;
	int num_indices;  /* t(u) */
	int *indices;  /* 1 <= i <= m - 1 */
	union {
		struct bplus_node **children;  /* 0 <= i <= m - 1 */
		struct ptrie_node **data;  /* 1 <= i <= m - 1 */
	};
};

struct miht {
	int k;
	int m;
	struct ptrie_node *root0;
	struct bplus_node *root1;
};

/*
 * \brief Recommended value for both \c k and \c m is 16 for IPv4.
 * \param k Length of prefix keys.
 * \param m Order of B+ tree.
 */
struct miht *miht_create(int k, int m);

void miht_insert(struct miht *miht, struct bplus_node *bplus,
		struct ip_prefix prefix);

void miht_load(struct miht *miht, FILE *pfxs);

bool miht_lookup(const struct miht *miht, unsigned int addr, int len, unsigned int *next_hop);

void miht_print(const struct miht *miht);

#endif

