/*
 * This takes as input a file containing network prefixes and do a
 * preprocessing, generating three output files:
 *
 * 	- dla.txt: prefixes to be stored in the Direct Lookup Array. Includes
 * 	all prefixes whose length is in the inclusive range [1, 20] expanded to
 * 	have length equal to 20.
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

struct prefix {
	uint32_t prefix;
	int len;
	int nhop;
};

struct btrie_node {
	bool has_nhop;
	int nhop;
	struct btrie_node *left;
	struct btrie_node *right;
};

/* Forwarding table. */
/*
int prefixes_len = 5;

struct prefix prefixes[] = {
	{
		.prefix = 0b00,
		.len = 2,
		.nhop = 1
	},
	{
		.prefix = 0b0,
		.len = 1,
		.nhop = 2,
	},
	{
		.prefix = 0b100,
		.len = 3,
		.nhop = 3
	},
	{
		.prefix = 0b10,
		.len = 2,
		.nhop = 4
	},
	{
		.prefix = 0b110,
		.len = 3,
		.nhop = 5
	}
};

static const char *byte_to_binary(uint32_t x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}
*/

struct btrie_node* btrie_node()
{
	struct btrie_node *node = malloc(sizeof(struct btrie_node));
	assert(node != NULL);
	node->has_nhop = false;
	node->left = NULL;
	node->right = NULL;

	return node;
}

int get_bit(unsigned int p, unsigned int len, unsigned int index)
{
	return (p >> (len - 1 - index)) & 0x1; 
}

/*
 * Create: return 0.
 * Update: return 1.
 */
int btrie_insert(struct btrie_node *btrie, struct prefix *p, bool allow_update)
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
	rc = btrie->has_nhop && allow_update ? 1 : 0;
	if (!btrie->has_nhop || allow_update) {
//	if (allow_update || (!allow_update && !btrie->has_nhop)) {
		btrie->has_nhop = true;
		btrie->nhop = p->nhop;
	}

	return rc;
}

struct btrie_node* btrie_create(FILE *fp, int start, int end)
{
	unsigned char a, b, c, d, mask;
	unsigned char a1, b1, c1, d1;
	assert(fp != NULL);

	struct btrie_node *btrie = btrie_node();

	while (fscanf(fp, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) == 4) {
		if (fscanf(fp, "/%hhu", &mask) != 1) {
			mask = 0;
			if (d > 0)
				mask = 32;
			else if (c > 0)
				mask = 24;
			else if (b > 0)
				mask = 16;
			else if (a > 0)
				mask = 8;
		}

		/* Insert only the prefixes that fall in the range. */
		if (mask >= start && mask <= end) {
			/* Read next hop. */
			int rc = fscanf(fp, "%hhu.%hhu.%hhu.%hhu", &a1, &b1, &c1, &d1);
			if(rc != 4) {
				fprintf(stdout, "btrie_create: Error reading next hop.\n");
				exit(1);
			}
			uint32_t pfx = (a << 24) | (b << 16) | (c << 8) | d;
			if (mask > 0)
				pfx = pfx >> (32 - mask);
			uint32_t nhop = (a1 << 24) | (b1 << 16) | (c1 << 8) | d1;

			struct prefix p = {
				.prefix = pfx,
				.nhop = nhop,
				.len = mask
			};

			btrie_insert(btrie, &p, true);
		}

		int nline;
		while((nline = fgetc(fp)) != '\n' && nline != EOF);
	};

//	for (int i = 0; i < prefixes_len; i++)
//		btrie_insert(btrie, &prefixes[i], true);

	return btrie;
}

void btrie_print(struct btrie_node *btrie, uint32_t prefix, int len, FILE *out)
{
	if (btrie->left != NULL) {
		btrie_print(btrie->left, prefix << 1, len + 1, out);
	}
	if (btrie->right != NULL) {
		btrie_print(btrie->right, (prefix << 1) | 1, len + 1, out);
	}
	if (btrie->has_nhop) {
		unsigned char a, b, c, d;
		unsigned char a1, b1, c1, d1;
		if (len < 32)
			prefix = prefix << (32 - len);
		a = prefix >> 24;
		b = (prefix >> 16) & 0xff;
		c = (prefix >> 8) & 0xff;
		d = prefix & 0xff;

		a1 = btrie->nhop >> 24;
		b1 = (btrie->nhop >> 16) & 0xff;
		c1 = (btrie->nhop >> 8) & 0xff;
		d1 = btrie->nhop & 0xff;
		fprintf(out, "%hhu.%hhu.%hhu.%hhu/%hhu %hhu.%hhu.%hhu.%hhu\n", a, b, c, d, (unsigned char)len, a1, b1, c1, d1);
	}
//	if (btrie->has_nhop) {
//		printf("prefix = %s, len = %d, next_hop = %d\n", byte_to_binary(prefix), len, btrie->nhop);
//	} else {
//		printf("prefix = %s, len = %d, next_hop = -\n", byte_to_binary(prefix), len);
//	}
}

void btrie_perform_cpe(struct btrie_node *root, struct btrie_node *b, int
		stride, uint32_t prefix, int len)
{
	if (len >= stride)
		return;

	if (b->left != NULL) {
		btrie_perform_cpe(root, b->left, stride, prefix << 1, len + 1);
	}
	if (b->right != NULL) {
		btrie_perform_cpe(root, b->right, stride, (prefix << 1) | 1, len + 1);
	}
	if (b->has_nhop) {
		int k = stride - len;
//		printf("----> Inserting prefixes...\n");
		for (int i = 0; i < pow(2, k); i++) {
			struct prefix p = {
				.prefix = (prefix << k) | i,
				.len = stride,
				.nhop = b->nhop
			};
	//		printf("---> prefix = %s, len = %d, next_hop = %d\n", byte_to_binary(p.prefix), p.len, p.nhop);
			btrie_insert(root, &p, false);
		}
		b->has_nhop = false;
	}
}

static void dla_fill(uint32_t *dla, const struct btrie_node *cpe_trie, int stride, uint32_t prefix, int len)
{
	if (cpe_trie->left != NULL)
		dla_fill(dla, cpe_trie->left, stride, prefix << 1, len + 1);
	if (cpe_trie->right != NULL)
		dla_fill(dla, cpe_trie->right, stride, (prefix << 1) | 1, len + 1);
	if (cpe_trie->has_nhop)
		dla[prefix] = cpe_trie->nhop;
}

/*
 * Create Direct Lookup Array.
 */
uint32_t *dla_create(const struct btrie_node *cpe_trie, int stride)
{
	size_t len = pow(2, stride);
	/* Assuming 0 is the default next hop (default route). */
	uint32_t *dla = calloc(len, sizeof(uint32_t));
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
void dla_print(const uint32_t *dla, int stride, FILE *out)
{
	assert(out != NULL);
	unsigned char a, b, c, d;
	unsigned char a1, b1, c1, d1;
	size_t len = pow(2, stride);
	for (size_t i = 0; i < len; i++) {
		size_t j = i << (32 - stride);
		a = j >> 24;
		b = (j >> 16) & 0xff;
		c = (j >> 8) & 0xff;
		d = j & 0xff;
		uint32_t nhop = dla[i]; // << (32 - stride);
		a1 = nhop >> 24;
		b1 = (nhop >> 16) & 0xff;
		c1 = (nhop >> 8) & 0xff;
		d1 = nhop & 0xff;
		fprintf(out, "%hhu.%hhu.%hhu.%hhu/%hhu %hhu.%hhu.%hhu.%hhu\n", a, b, c, d, (unsigned char)stride, a1, b1, c1, d1);
		//fprintf(out, "%"PRIu32"\n", dla[i]);
	}
}

int main(int argc, char *argv[])
{
//	struct btrie_node *btrie = create_btrie();
//	printf("Trie:\n");
//	btrie_print(btrie, 0, 0);
//	printf("\n\nCPE Trie:\n");
//	btrie_perform_cpe(btrie, btrie, 3, 0, 0);
//	btrie_print(btrie, 0, 0);
//	printf("\n\nDLA:\n");
//	int *dla = dla_create(btrie, 3);
//	dla_print(dla, 3);

	if (argc != 2) {
		fprintf(stderr, "%s: generate {dla, g1, g2}.txt from a prefixes file.\n", argv[0]);
		fprintf(stderr, "Usage: %s <prefixes file>\n", argv[0]);
		exit(1);
	}

	FILE *fp = fopen(argv[1], "r");
	assert(fp != NULL);

	// DIRECT LOOKUP ARRAY
	struct btrie_node *btrie = btrie_create(fp, 1, 20);
	btrie_perform_cpe(btrie, btrie, 20, 0, 0);
	uint32_t *dla = dla_create(btrie, 20);
	FILE *out = fopen("dla.txt", "w");
	assert(out != NULL);
	dla_print(dla, 20, out);
	fclose(out);

	// TODO: btrie_free();

	// G1
	rewind(fp);
	struct btrie_node *g1 = btrie_create(fp, 21, 24);
	btrie_perform_cpe(g1, g1, 24, 0, 0);
	out = fopen("g1.txt", "w");
	btrie_print(g1, 0, 0, out);
	fclose(out);

	// G2
	rewind(fp);
	struct btrie_node *g2 = btrie_create(fp, 25, 32);
	btrie_perform_cpe(g2, g2, 32, 0, 0);
	out = fopen("g2.txt", "w");
	btrie_print(g2, 0, 0, out);
	fclose(out);
	fclose(fp);
	
	return 0;
}

