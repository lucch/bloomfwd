#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "miht.h"
#include "uint128.h"

enum miht_node_type {
	MIHT_INTERNAL, MIHT_EXTERNAL
};

static const uint128 zero128 = (uint128){ .hi = 0, .lo = 0 };

uint128 default_route = (uint128){ .hi = 0, .lo = 0 };

struct ptrie_node *ptrie_node()
{
	struct ptrie_node *ptrie_node = malloc(sizeof(struct ptrie_node));
	ptrie_node->is_priority = true;
	ptrie_node->suffix = 0;
	ptrie_node->len = 0;
	ptrie_node->next_hop = zero128;
	ptrie_node->left = NULL;
	ptrie_node->right = NULL;

	return ptrie_node;
}

struct bplus_node *bplus_node(int m, enum miht_node_type type)
{
	struct bplus_node *bplus_node = malloc(sizeof(struct bplus_node));
	bplus_node->num_indices = 0;
	/* Allocate extra node to ease implementation: `indices[0]` won't be
	 * used.
	 */
#if defined(__MIC__)
	bplus_node->indices = _mm_malloc(m * sizeof(uint64_t), 64);
#else
	bplus_node->indices = malloc(m * sizeof(uint64_t));
#endif
	for (int i = 0; i < m; i++) {
		bplus_node->indices[i] = UINT64_MAX;
	}
	if (type == MIHT_INTERNAL) {
		bplus_node->is_leaf = false;
		bplus_node->children = calloc(m, sizeof(struct bplus_node *));
	} else {  /* type == MIHT_EXTERNAL */
		bplus_node->is_leaf = true;
		/* Allocate extra node to ease implementation: `data[0]` won't be
		 * used.
		 */
		bplus_node->data = calloc(m, sizeof(struct ptrie_node *));
	}

	return bplus_node;
}

struct miht *miht_create(int k, int m)
{
	struct miht *miht = malloc(sizeof(struct miht));
	miht->k = k;
	miht->m = m;
	miht->root0 = NULL;
	miht->root1 = bplus_node(m, MIHT_EXTERNAL);

	return miht;
}

static inline uint64_t prefix_key(int k, uint64_t p, int len)
{
	uint64_t ret = len > k ? p >> (len - k) : p;
	return ret;
}

/*
 * Assumes len > k.
 */
int suffix(int k, uint64_t p, int len)
{
	uint64_t ret = p & ~(0xffffffffffffffff << (len - k));
	return ret;
}

void quicksort(uint64_t *a, struct ptrie_node **ptries, int n)
{
	int i, j;
	uint64_t p, t;
	struct ptrie_node *t_ptrie;
	if (n < 2)
		return;
	p = a[n / 2];
	for (i = 0, j = n - 1;; i++, j--) {
		while (a[i] < p)
			i++;
		while (p < a[j])
			j--;
		if (i >= j)
			break;
		/* Swap indices. */
		t = a[i];
		a[i] = a[j];
		a[j] = t;
		/* Swap ptries. */
		t_ptrie = ptries[i];
		ptries[i] = ptries[j];
		ptries[j] = t_ptrie;
	}
	quicksort(a, ptries, i);
	quicksort(a + i, ptries + i, n - i);
}

/*
 * node must be an EXTERNAL_NODE (leaf).
 */
void sort(struct bplus_node *node, int count)
{
	if (node->is_leaf)
		quicksort(&node->indices[1], &node->data[1], count);
}

void miht_node_split(int m, struct bplus_node *x, int y_pos, struct bplus_node *y,
		uint64_t pkey, struct miht *miht)
{
	struct bplus_node *z = y->is_leaf ? bplus_node(m, MIHT_EXTERNAL) :
		bplus_node(m, MIHT_INTERNAL);

	int count = x->num_indices - y_pos;
	if (count > 0) {  /* x is always an internal node. */
		memmove(&x->indices[y_pos + 2],
			&x->indices[y_pos + 1],
			count * sizeof(uint64_t));
		memmove(&x->children[y_pos + 2],
			&x->children[y_pos + 1],
			count * sizeof(struct bplus_node *));
	}
	x->children[y_pos + 1] = z;
	x->num_indices += 1;
	int g = ceil(m / 2.0);
	if (y->is_leaf) {
		if (pkey >= y->indices[g]) {
			memmove(&z->indices[1],
				&y->indices[g + 1],
				(m - g - 1) * sizeof(uint64_t));
			memmove(&z->data[1],
				&y->data[g + 1],
				(m - g - 1) * sizeof(struct ptrie_node *));
			memset(&y->data[g + 1], 0, m - g - 1);  /* Set NULL. */
			z->indices[m - g] = pkey;
			sort(z, m - g);
		} else {
			memmove(&z->indices[1],
				&y->indices[g],
				(m - g) * sizeof(uint64_t));
			memmove(&z->data[1],
				&y->data[g],
				(m - g) * sizeof(struct ptrie_node *));
			memset(&y->data[g], 0, m - g);  /* Set NULL. */
			y->indices[g] = pkey;
			sort(y, g);
		}
		y->num_indices = g;
		z->num_indices = m - g;
		x->indices[y_pos + 1] = z->indices[1];
	} else {
		memmove(&z->indices[1],
			&y->indices[g + 1],
			(m - g - 1) * sizeof(uint64_t));
		memmove(&z->children[0],
			&y->children[g],
			(m - g) * sizeof(struct bplus_node *));
		memset(&y->children[g], 0, m - g);
		y->num_indices = g - 1;
		z->num_indices = m - g - 1;
		x->indices[y_pos + 1] = y->indices[g];
	}
}

void byte_to_binary(uint64_t x, char buf[], int len)
{
	buf[0] = '\0';
	for (int z = pow(2, len - 1); z > 0; z >>= 1) {
		strcat(buf, ((x & z) == z) ? "1" : "0");
	}
}

/*
 * prefix2 must be GREATER OR EQUAL THAN prefix1.
 */
static inline bool ptrie_prefix_match(uint64_t prefix1, int len1, uint64_t prefix2, int len2)
{
	if (len1 == 0)
		return true;
	return len2 >= len1 ? (prefix2 >> (len2 - len1)) == prefix1 : false;
}

/*
 * pos start at 1.
 */
static inline bool ptrie_check_bit(int pos, uint64_t suffix, int len)
{
	return len >= pos ? suffix & ((uint64_t)1 << (len - pos)) : false;
}


struct ptrie_node *ptrie_insert_prime(struct ptrie_node *ptrie, uint64_t suffix,
		int len, uint128 next_hop, int level)
{
	if (ptrie == NULL) {
		ptrie = ptrie_node();
		ptrie->is_priority = len > level;
		ptrie->suffix = suffix;
		ptrie->len = len;
		ptrie->next_hop = next_hop;
	} else if (ptrie->suffix == suffix && ptrie->len == len) {  /* Update */
		ptrie->next_hop = next_hop;
	} else {
		if (len == level) {
			if (ptrie->is_priority) {
				uint64_t suffix_tmp = ptrie->suffix;
				int len_tmp = ptrie->len;
				uint128 next_hop_tmp = ptrie->next_hop;
				ptrie->suffix = suffix;
				ptrie->len = len;
				ptrie->next_hop = next_hop;
				suffix = suffix_tmp;
				len = len_tmp;
				next_hop = next_hop_tmp;
				ptrie->is_priority = false;
			}
		} else {
			int node_len = ptrie->len;
			bool match = ptrie_prefix_match(ptrie->suffix, node_len, suffix, len);
			if (len > node_len && match && ptrie->is_priority) {
				uint64_t suffix_tmp = ptrie->suffix;
				int len_tmp = ptrie->len;
				uint128 next_hop_tmp = ptrie->next_hop;
				ptrie->suffix = suffix;
				ptrie->len = len;
				ptrie->next_hop = next_hop;
				suffix = suffix_tmp;
				len = len_tmp;
				next_hop = next_hop_tmp;
			}
		}

		if (ptrie_check_bit(level + 1, suffix, len)) {
			ptrie->right = ptrie_insert_prime(ptrie->right,
					suffix, len, next_hop, level + 1);
		} else {
			ptrie->left = ptrie_insert_prime(ptrie->left,
					suffix, len, next_hop, level + 1);
		}
	}

	return ptrie;
}

void ptrie_insert(struct ptrie_node **ptrie, uint64_t suffix, int len, uint128 next_hop)
{
	if (*ptrie == NULL) {
		*ptrie = ptrie_node();
		(*ptrie)->is_priority = len > 0;
		(*ptrie)->suffix = suffix;
		(*ptrie)->len = len;
		(*ptrie)->next_hop = next_hop;
	} else {
		ptrie_insert_prime(*ptrie, suffix, len, next_hop, 0);
	}
}

static inline int miht_bsearch(uint64_t *indices, int len, uint64_t pkey)
{
	int low = 0;
	int high = len - 1;
	while (low <= high) {
		int mid = (low + high) / 2;
		if (indices[mid] > pkey)
			high = mid - 1;
		else
			low = mid + 1;
	}
	return low;
}

void miht_insert(struct miht *miht, struct bplus_node *bplus,
		struct ip_prefix prefix)
{
	if (prefix.len == 0) {
		default_route = prefix.next_hop;
		return;
	}

	int k = miht->k;
	int m = miht->m;

	if (prefix.len >= k) {
		if (miht->root1->num_indices == m - 1) {
			struct bplus_node *new_root = bplus_node(m, MIHT_INTERNAL);
			new_root->children[0] = miht->root1;
			miht->root1 = new_root;
			miht_node_split(m, miht->root1, 0,
				miht->root1->children[0], prefix_key(k, prefix.prefix, prefix.len), miht);
			bplus = miht->root1;
		}

		uint64_t p = prefix_key(k, prefix.prefix, prefix.len);
		/* Find the index i in B+ tree node. */
		int i = miht_bsearch(&bplus->indices[1], bplus->num_indices, p);

		if (bplus->is_leaf) {  /* External node. */
			if (i == 0 || p != bplus->indices[i]) {
				int count = bplus->num_indices - i;
				i = i + 1;
				if (count > 0) {
					memmove(&bplus->indices[i + 1],
						&bplus->indices[i],
						count * sizeof(uint64_t));
					memmove(&bplus->data[i + 1],
						&bplus->data[i],
						count * sizeof(struct ptrie_node *));
				}
				bplus->indices[i] = p;
				bplus->data[i] = NULL;
				bplus->num_indices = bplus->num_indices + 1;
			}
			ptrie_insert(&bplus->data[i], suffix(k, prefix.prefix, prefix.len),
					prefix.len - k, prefix.next_hop);
		} else {  /* Internal node. */
			struct bplus_node *child = bplus->children[i];
			if (child->num_indices == m - 1) {
				int j = miht_bsearch(&child->indices[1],
						child->num_indices, p);
				bool prefix_exists = child->indices[j] == p;
				if ((child->is_leaf && !prefix_exists) ||
						!child->is_leaf) {
					miht_node_split(miht->m, bplus, i, child, p, miht);
					if (p >= bplus->indices[i + 1])
						i = i + 1;
				}
			}
			miht_insert(miht, bplus->children[i], prefix);
		}
	} else {
		/* Insert into PT[-1]. */
		ptrie_insert(&miht->root0, prefix.prefix, prefix.len, prefix.next_hop);
	}
}

void miht_load(struct miht *miht, FILE *pfxs)
{
	assert(pfxs != NULL);

	int ignored = 0;
	uint8_t len;
	unsigned a0 = 0, b0 = 0, c0 = 0, d0 = 0, e0 = 0, f0 = 0, g0 = 0, h0 = 0;
	unsigned a1 = 0, b1 = 0, c1 = 0, d1 = 0, e1 = 0, f1 = 0, g1 = 0, h1 = 0;

	while (fscanf(pfxs, "%x:%x:%x:%x:%x:%x:%x:%x/%hhu\n",
				&a0, &b0, &c0, &d0, &e0, &f0, &g0, &h0, &len) == 9) {
		if (fscanf(pfxs, "%x:%x:%x:%x:%x:%x:%x:%x",
				&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1) != 8) {
			fprintf(stderr, "miht_load.fscanf error!\n");
			exit(1);
		}
		if (len > 64) {  /* Only prefixes up to 64 bits are allowed. */
			if (ignored++ == 0)
				printf("Ignored prefixes:\n");
			printf("\t%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu\n",
				a0, b0, c0, d0, e0, f0, g0, h0, len);
			continue;
		}

		uint128 next_hop = ip_addr(a1, b1, c1, d1, e1, f1, g1, h1);
		struct ip_prefix pfx = ip_prefix(a0, b0, c0, d0, len, next_hop);
		miht_insert(miht, miht->root1, pfx);
	}
}

static inline uint128 ptrie_lookup(const struct ptrie_node *ptrie, uint64_t suffix, int len)
{
	uint128 next_hop = default_route; 
	int level = 0;

	while (ptrie != NULL) {
		if (ptrie_prefix_match(ptrie->suffix, ptrie->len, suffix, len)) {
			next_hop = ptrie->next_hop;
			if (ptrie->is_priority)
				break;
		}
		ptrie = ptrie_check_bit(++level, suffix, len) ?
			ptrie->right : ptrie->left;
	}

	return next_hop;
}

void ptrie_printhex(const struct ptrie_node *ptrie)
{
	if (ptrie != NULL) {
		printf("(%x/%d, %x%x)%c\n", (unsigned)ptrie->suffix, ptrie->len,
			(unsigned)ptrie->next_hop.hi, (unsigned)ptrie->next_hop.lo,
			ptrie->is_priority ? 'P' : ' ');
		if (ptrie->left != NULL) {
			printf("L");
			ptrie_printhex(ptrie->left);
		}
		if (ptrie->right != NULL) {
			printf("R");
			ptrie_printhex(ptrie->right);
		}
	}
}

bool miht_lookup(const struct miht *miht, uint128 addr, uint128 *nhop)
{
	uint64_t addr_hi = addr.hi;
	uint128 next_hop;
	const struct ptrie_node *ptminusone = miht->root0;
	const struct bplus_node *bplus = miht->root1;
	int k = miht->k;
	uint64_t p = prefix_key(k, addr_hi, 64);
	while (!bplus->is_leaf) {
		int i = miht_bsearch(&bplus->indices[1], bplus->num_indices, p);
		bplus = bplus->children[i];
	}

	int i = miht_bsearch(&bplus->indices[1], bplus->num_indices, p);
	if (p == bplus->indices[i]) {
		next_hop = ptrie_lookup(bplus->data[i], suffix(k, addr_hi, 64), 64 - k);
		if (!UINT128_EQ(next_hop, default_route)) {
			*nhop = next_hop;
			return true;
		}
	}

	*nhop = ptrie_lookup(ptminusone, addr_hi, 64);
	return !(UINT128_EQ(*nhop, default_route) && UINT128_EQ(default_route, zero128));
}

void ptrie_print(const struct ptrie_node *ptrie)
{
	if (ptrie != NULL) {
		char str[ptrie->len + 1];
		byte_to_binary(ptrie->suffix, str, ptrie->len);
		printf("(%s*, (%llu << 64) | %llu)%c\n", str,
				ptrie->next_hop.hi, ptrie->next_hop.lo,
				ptrie->is_priority ? 'P' : '\0');
		if (ptrie->left != NULL) {
			printf("L");
			ptrie_print(ptrie->left);
		}
		if (ptrie->right != NULL) {
			printf("R");
			ptrie_print(ptrie->right);
		}
	}
}

void miht_print_prime(const struct bplus_node *bplus, int level)
{
	for (int i = 1; i <= bplus->num_indices; i++) {
		for (int j = 0; j < level; j++)
			printf("\t");
		printf("0x%x\n", (unsigned int)bplus->indices[i]);
	}
	printf("\n");
	if (!bplus->is_leaf)
		for (int i = 0; i <= bplus->num_indices; i++) {
			miht_print_prime(bplus->children[i], level + 1);
		}
}

void miht_print(const struct miht *miht)
{
	miht_print_prime(miht->root1, 0);
}


