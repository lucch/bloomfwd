/*
 * TODO: Fix description!
 *
 * This takes as input a file containing network prefixes and do a
 * preprocessing, generating three output files:
 *
 * 	- dla.txt: prefixes to be stored in the Direct Lookup Array. Includes
 * 	all prefixes whose length is in the inclusive range [1, 20] expanded to
 * 	have length equal to 20.
 *
 * 	- g1.txt: prefixes in the range [21, 24] expanded to have length equal
 * 	to 24.
 * 	- g2.txt: prefixes in the range [25, 32] expanded to have length equal
 * 	to 32.
 */

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uint128.h"
#include "bloomfwd_opt.h"


struct btrie_node {
	bool has_next_hop;
	uint128 next_hop;
	struct btrie_node *left;
	struct btrie_node *right;
};

struct btrie_node* btrie_node()
{
	struct btrie_node *node = malloc(sizeof(struct btrie_node));
	assert(node != NULL);
	node->has_next_hop = false;
	node->left = NULL;
	node->right = NULL;

	return node;
}

int get_bit(uint128 pfx, unsigned int len, unsigned int index)
{
	uint64_t p;
	if (index > 64) {
		p = pfx.lo;
		index -= 64;
	} else {
		p = pfx.hi;
	}
		
	return (p >> (64 - index - 1)) & 0x1; 
}

/*
 * Create: return 0.
 * Update: return 1.
 */
int btrie_insert(struct btrie_node *btrie, struct ipv6_prefix *p, bool allow_update)
{
	int rc = 0;
	for (int i = 0; i < p->len; i++) {
		int bit = get_bit(p->prefix, p->len, i);
		if (bit == 0) { /* Branch left */
			if (btrie->left == NULL) {
				btrie->left = btrie_node();
			}
			btrie = btrie->left;
		} else { /* Branch right */
			if (btrie->right == NULL) {
				btrie->right = btrie_node();
			}
			btrie = btrie->right;
		}
	}
	rc = btrie->has_next_hop && allow_update ? 1 : 0;
	if (!btrie->has_next_hop || allow_update) {
		btrie->has_next_hop = true;
		btrie->next_hop = p->next_hop;
	}

	return rc;
}


struct btrie_node* btrie_create(FILE *fp, int start, int end)
{
	unsigned char len;
	unsigned a0 = 0, b0 = 0, c0 = 0, d0 = 0, e0 = 0, f0 = 0, g0 = 0, h0 = 0;
	unsigned a1 = 0, b1 = 0, c1 = 0, d1 = 0, e1 = 0, f1 = 0, g1 = 0, h1 = 0;
	assert(fp != NULL);

	struct btrie_node *btrie = btrie_node();

	while (fscanf(fp, "%x:%x:%x:%x:%x:%x:%x:%x/%hhu\n",
				&a0, &b0, &c0, &d0, &e0, &f0, &g0, &h0, &len) == 9) {
		/* Insert only the prefixes that fall in the range. */
		if (len >= start && len <= end) {
			/* Read next hop. */
			assert(fscanf(fp, "%x:%x:%x:%x:%x:%x:%x:%x",
					&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1) == 8);

			uint128 next_hop = new_ipv6_addr(a1, b1, c1, d1, e1, f1, g1, h1);
			struct ipv6_prefix *pfx = new_ipv6_prefix(a0, b0, c0, d0, e0, f0, g0, h0, len, next_hop);

			btrie_insert(btrie, pfx, true);
		}

		int nline;
		while((nline = fgetc(fp)) != '\n' && nline != EOF);
	};

	return btrie;
}

void btrie_print(struct btrie_node *btrie, uint128 pfx, int len, FILE *out)
{
	if (btrie->left != NULL) {
		uint128 p = len < 64 ?
				(uint128){ .hi = pfx.hi << 1, .lo = pfx.lo } :
				(uint128){ .hi = pfx.hi, .lo = pfx.lo << 1 };
		btrie_print(btrie->left, p, len + 1, out);
	}
	if (btrie->right != NULL) {
		uint128 p = len < 64 ?
				(uint128){ .hi = (pfx.hi << 1) | 1, .lo = pfx.lo } :
				(uint128){ .hi = pfx.hi, .lo = (pfx.lo << 1) | 1 };
		btrie_print(btrie->right, p, len + 1, out);
	}
	if (btrie->has_next_hop) {
		unsigned a0, b0, c0, d0, e0, f0, g0, h0;
		unsigned a1, b1, c1, d1, e1, f1, g1, h1;
		if (len < 64)
			pfx.hi = pfx.hi << (64 - len);
		else
			pfx.lo = pfx.lo << (64 - (len - 64));

		a0 = (unsigned)(pfx.hi >> 48);
		b0 = (unsigned)((pfx.hi & 0x0000ffff00000000) >> 32);
		c0 = (unsigned)((pfx.hi & 0x00000000ffff0000) >> 16);
		d0 = (unsigned)(pfx.hi & 0x000000000000ffff);
		e0 = (unsigned)(pfx.lo >> 48);
		f0 = (unsigned)((pfx.lo & 0x0000ffff00000000) >> 32);
		g0 = (unsigned)((pfx.lo & 0x00000000ffff0000) >> 16);
		h0 = (unsigned)(pfx.lo & 0x000000000000ffff);
		a1 = (unsigned)(btrie->next_hop.hi >> 48);
		b1 = (unsigned)((btrie->next_hop.hi & 0x0000ffff00000000) >> 32);
		c1 = (unsigned)((btrie->next_hop.hi & 0x00000000ffff0000) >> 16);
		d1 = (unsigned)(btrie->next_hop.hi & 0x000000000000ffff);
		e1 = (unsigned)(btrie->next_hop.lo >> 48);
		f1 = (unsigned)((btrie->next_hop.lo & 0x0000ffff00000000) >> 32);
		g1 = (unsigned)((btrie->next_hop.lo & 0x00000000ffff0000) >> 16);
		h1 = (unsigned)(btrie->next_hop.lo & 0x000000000000ffff);

		fprintf(out, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu "
			     "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				a0, b0, c0, d0, e0, f0, g0, h0, (unsigned char)len,
				a1, b1, c1, d1, e1, f1, g1, h1);
	}
}

void btrie_perform_cpe(struct btrie_node *root, struct btrie_node *b,
		int stride, uint128 pfx, int len)
{
	if (len >= stride)
		return;

	if (b->left != NULL) {
		uint128 p = len < 64 ?
				(uint128){ .hi = pfx.hi << 1, .lo = pfx.lo } :
				(uint128){ .hi = pfx.hi, .lo = pfx.lo << 1 };
		btrie_perform_cpe(root, b->left, stride, p, len + 1);
	}
	if (b->right != NULL) {
		uint128 p = len < 64 ?
				(uint128){ .hi = (pfx.hi << 1) | 1, .lo = pfx.lo } :
				(uint128){ .hi = pfx.hi, .lo = (pfx.lo << 1) | 1 };
		btrie_perform_cpe(root, b->right, stride, p, len + 1);
	}
	if (b->has_next_hop) {
		int k = stride - len;
		//printf("----> Inserting pfxes...\n");
		for (unsigned long i = 0; i < pow(2, k); i++) {
			struct ipv6_prefix p = {
				.prefix = k < 64 ?
					(uint128){ .hi = (pfx.hi << k) | i,
						   .lo = pfx.lo } :
					(uint128){ .hi = pfx.hi,
						   .lo = (pfx.lo << k) | i },
				.len = stride,
				.next_hop = b->next_hop
			};
			
			if (stride < 64)
				p.prefix.hi <<= 64 - stride;
			else
				p.prefix.lo <<= 128 - stride;

			btrie_insert(root, &p, false);
		}
		b->has_next_hop = false;
	}
}

/*
 * `dla` shouldn't have more than 2^64 positions and `cpe_trie` shouldn't have
 * more than 64 levels...
 */
static void dla_fill(uint128 *dla, const struct btrie_node *cpe_trie, int stride,
		uint64_t pfx_hi, int len)
{
	if (cpe_trie->left != NULL) {
		dla_fill(dla, cpe_trie->left, stride, pfx_hi << 1, len + 1);
	}
	if (cpe_trie->right != NULL) {
		dla_fill(dla, cpe_trie->right, stride, (pfx_hi << 1) | 1, len + 1);
	}
	if (cpe_trie->has_next_hop) {
		dla[pfx_hi] = cpe_trie->next_hop;
	}
}

/*
 * Create Direct Lookup Array.
 */
uint128 *dla_create(const struct btrie_node *cpe_trie, int stride)
{
	size_t len = pow(2, stride);
	/* Assuming 0 is the default next hop (default route). */
	uint128 *dla = calloc(len, sizeof(uint128));
	assert(dla != NULL);
	dla_fill(dla, cpe_trie, stride, 0, 0);

	return dla;
}

/*
 * Prints the entries that must be filled in the Direct Lookup Array.
 *
 * To build the array:
 *   for (i = 0; i < 2^stride; i++) dla[i] = readLine(file).toInt()
 *
 */
void dla_print(const uint128 *dla, int stride, FILE *out)
{
	assert(out != NULL);
	unsigned a0, b0, c0, d0; // [e0 .. h0] == [0, 0, 0, 0]
	unsigned a1, b1, c1, d1, e1, f1, g1, h1;
	size_t len = pow(2, stride);
	for (size_t i = 0; i < len; i++) {
		size_t j = i << (64 - stride);
		a0 = (unsigned)(j >> 48);
		b0 = (unsigned)((j & 0x0000ffff00000000) >> 32);
		c0 = (unsigned)((j & 0x00000000ffff0000) >> 16);
		d0 = (unsigned)(j & 0x000000000000ffff);
		
		uint128 nhop = dla[i]; // << (32 - stride);
		a1 = (unsigned)(nhop.hi >> 48);
		b1 = (unsigned)((nhop.hi & 0x0000ffff00000000) >> 32);
		c1 = (unsigned)((nhop.hi & 0x00000000ffff0000) >> 16);
		d1 = (unsigned)(nhop.hi & 0x000000000000ffff);
		e1 = (unsigned)(nhop.lo >> 48);
		f1 = (unsigned)((nhop.lo & 0x0000ffff00000000) >> 32);
		g1 = (unsigned)((nhop.lo & 0x00000000ffff0000) >> 16);
		h1 = (unsigned)(nhop.lo & 0x000000000000ffff);

		fprintf(out, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu "
			     "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				a0, b0, c0, d0, 0, 0, 0, 0, (unsigned char)stride,
				a1, b1, c1, d1, e1, f1, g1, h1);
		//fprintf(out, "%"PRIu32"\n", dla[i]);
	}
}

// TODO: Parameterize `main()` to generate n groups from G1 to Gn (from `argv`).
/*
 * Usage:
 * ./cpe_v6 <prefixes files> <lengths: 1,8,9,17...>
 */
int main(int argc, char *argv[])
{
	assert(argc == 3);

	FILE *fp = fopen(argv[1], "r");
	assert(fp != NULL);

	// NO DLA FOR NOW!
	
	char *len = strtok(argv[2], ",");
	int from = 1;
	int to;
	while (len != NULL) {
		to = atoi(len);
//		printf("%d\n", to);

		rewind(fp);
		struct btrie_node *gn = btrie_create(fp, from, to);
		btrie_perform_cpe(gn, gn, to, (uint128){ .hi = 0, .lo = 0 }, 0);
		char filename[20];
		sprintf(filename, "g%d.txt", to);
		FILE *out = fopen(filename, "w");
		btrie_print(gn, (uint128){ .hi = 0, .lo = 0 }, 0, out);
		fclose(out);
		from = to + 1;

		len = strtok(NULL, ",");
	}


//	// G1
//	rewind(fp);
//	struct btrie_node *g1 = btrie_create(fp, 21, 24);
//	btrie_perform_cpe(g1, g1, 24, 0, 0);
//	out = fopen("g1.txt", "w");
//	btrie_print(g1, 0, 0, out);
//	fclose(out);
//
//	// G2
//	rewind(fp);
//	struct btrie_node *g2 = btrie_create(fp, 25, 32);
//	btrie_perform_cpe(g2, g2, 32, 0, 0);
//	out = fopen("g2.txt", "w");
//	btrie_print(g2, 0, 0, out);
//	fclose(out);
//	fclose(fp);

	return 0;
}


