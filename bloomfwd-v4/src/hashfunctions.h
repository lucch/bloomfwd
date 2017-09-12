#ifndef HASHFUNCTIONS_H
#define HASHFUNCTIONS_H

#include <stdint.h>

/*
 * Scalar version of MurmurHash3 in plain C.
 *
 * MurmurHash3 was created by Austin Appleby
 * (https://code.google.com/p/smhasher/wiki/MurmurHash3). This is a simplified
 * version for fixed-size 32-bit integer keys.
 */
extern inline uint32_t murmurhash3_32(uint32_t key)
{
	key *= 0xcc9e2d51;  /* 0xcc9e2d51 is the c1 constant. */
	key = (key << 15) | (key >> 17);
	key *= 0x1b873593;  /* 0x1b873593 is the c2 constant. */

	key ^= 0;  /* 0 is the initial hash value. */
	key = (key << 13) | (key >> 19);
	key = (key * 5) + 0xe6546b64;

	key ^= 4;  /* 4 is the size of the key in bytes. */

	key ^= key >> 16;
	key *= 0x85ebca6b;
	key ^= key >> 13;
	key *= 0xc2b2ae35;
	key ^= key >> 16;

	return key;
}

/*
 * See:
 * http://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
 */
extern inline uint32_t knuthhash_32(uint32_t key)
{
	return key * 2654435761;
}

/*
 * See:
 * http://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
 * https://github.com/h2database/h2database/blob/master/h2/src/test/org/h2/test/store/CalculateHashConstant.java
 */
extern inline uint32_t h2hash_32(uint32_t key)
{
	key = ((key >> 16) ^ key) * 0x45d9f3b;
	key = ((key >> 16) ^ key) * 0x45d9f3b;
	key = ((key >> 16) ^ key);

	return key;
}

#ifdef __MIC__
#include <immintrin.h>

/*
 * This function expects as input a 64-byte aligned array containing sixteen
 * 32-bit integer keys. It returns a pointer to sixteen 32-bit hash values.
 */
extern inline void murmurhash3_32_vec512(uint32_t *keys, uint32_t *hashes)
{
	__m512i h = _mm512_load_epi32(keys);
	__m512i c1 = _mm512_set1_epi32(0xcc9e2d51);
	h = _mm512_mullo_epi32(h, c1);
	__m512i h1 = _mm512_slli_epi32(h, 15);
	__m512i h2 = _mm512_srli_epi32(h, 17);
	h = _mm512_or_epi32(h1, h2);
	__m512i c2 = _mm512_set1_epi32(0x1b873593);
	h = _mm512_mullo_epi32(h, c2);

	__m512i zero = _mm512_setzero_epi32();
	h = _mm512_xor_epi32(h, zero);
	h1 = _mm512_slli_epi32(h, 13);
	h2 = _mm512_srli_epi32(h, 19);
	h = _mm512_or_epi32(h1, h2);
	__m512i five = _mm512_set1_epi32(5);
	__m512i c3 = _mm512_set1_epi32(0xe6546b64);
	h = _mm512_fmadd_epi32(h, five, c3);

	__m512i four = _mm512_set1_epi32(4);
	h = _mm512_xor_epi32(h, four);

	h1 = _mm512_srli_epi32(h, 16);
	h = _mm512_xor_epi32(h, h1);
	__m512i c4 = _mm512_set1_epi32(0x85ebca6b);
	h = _mm512_mullo_epi32(h, c4);
	h1 = _mm512_srli_epi32(h, 13);
	h = _mm512_xor_epi32(h, h1);
	__m512i c5 = _mm512_set1_epi32(0xc2b2ae35);
	h = _mm512_mullo_epi32(h, c5);
	h1 = _mm512_srli_epi32(h, 16);
	h = _mm512_xor_epi32(h, h1);

	/* Return the calculated hashes. */
	_mm512_store_epi32(hashes, h);
}

/*
 * A version of MurmurHash3 that uses less registers (is it faster?).
 */
extern inline void murmurhash3_32_vec512_v2(uint32_t *keys, uint32_t *hashes)
{
	__m512i h = _mm512_load_epi32(keys);
	__m512i c = _mm512_set1_epi32(0xcc9e2d51);  /* c = 0xcc9e2d51 (c1) */
	h = _mm512_mullo_epi32(h, c);
	__m512i h1 = _mm512_slli_epi32(h, 15);
	__m512i h2 = _mm512_srli_epi32(h, 17);
	h = _mm512_or_epi32(h1, h2);
	c = _mm512_set1_epi32(0x1b873593);  /* c = 0x1b873593 (c2) */
	h = _mm512_mullo_epi32(h, c);

	__m512i n = _mm512_setzero_epi32();  /* n = 0 */
	h = _mm512_xor_epi32(h, n);
	h1 = _mm512_slli_epi32(h, 13);
	h2 = _mm512_srli_epi32(h, 19);
	h = _mm512_or_epi32(h1, h2);
	n = _mm512_set1_epi32(5);  /* n = 5 */
	c = _mm512_set1_epi32(0xe6546b64);  /* c = 0xe6546b64 (c3) */
	h = _mm512_fmadd_epi32(h, n, c);

	n = _mm512_set1_epi32(4);  /* n = 4 */
	h = _mm512_xor_epi32(h, n);

	h1 = _mm512_srli_epi32(h, 16);
	h = _mm512_xor_epi32(h, h1);
	c = _mm512_set1_epi32(0x85ebca6b);  /* c = 0x85ebca6b (c4) */
	h = _mm512_mullo_epi32(h, c);
	h1 = _mm512_srli_epi32(h, 13);
	h = _mm512_xor_epi32(h, h1);
	c = _mm512_set1_epi32(0xc2b2ae35);  /* c = 0xc2b2ae35 (c5) */
	h = _mm512_mullo_epi32(h, c);
	h1 = _mm512_srli_epi32(h, 16);
	h = _mm512_xor_epi32(h, h1);

	/* Return the calculated hashes. */
	_mm512_store_epi32(hashes, h);
}

/*
 * In this version, MurmurHash3 is implemented using explicitly only three
 * registers: r0, r1 and r2 (is it faster?).
 */ 
extern inline void murmurhash3_32_vec512_v3(uint32_t *keys, uint32_t *hashes)
{
	__m512i r0 = _mm512_load_epi32(keys);
	__m512i r1 = _mm512_set1_epi32(0xcc9e2d51);  /* c = 0xcc9e2d51 (c1) */
	__m512i r2 = _mm512_mullo_epi32(r0, r1);
	r0 = _mm512_slli_epi32(r2, 15);
	r1 = _mm512_srli_epi32(r2, 17);
	r2 = _mm512_or_epi32(r0, r1);
	r0 = _mm512_set1_epi32(0x1b873593);  /* c = 0x1b873593 (c2) */
	r1 = _mm512_mullo_epi32(r2, r0);

	r0 = _mm512_setzero_epi32();  /* n = 0 */
	r2 = _mm512_xor_epi32(r1, r0);
	r0 = _mm512_slli_epi32(r2, 13);
	r1 = _mm512_srli_epi32(r2, 19);
	r2 = _mm512_or_epi32(r0, r1);
	r0 = _mm512_set1_epi32(5);  /* n = 5 */
	r1 = _mm512_set1_epi32(0xe6546b64);  /* c = 0xe6546b64 (c3) */
	r2 = _mm512_fmadd_epi32(r2, r0, r1);

	r0 = _mm512_set1_epi32(4);  /* n = 4 */
	r1 = _mm512_xor_epi32(r2, r0);

	r0 = _mm512_srli_epi32(r1, 16);
	r2 = _mm512_xor_epi32(r1, r0);
	r0 = _mm512_set1_epi32(0x85ebca6b);  /* c = 0x85ebca6b (c4) */
	r1 = _mm512_mullo_epi32(r2, r0);
	r0 = _mm512_srli_epi32(r1, 13);
	r2 = _mm512_xor_epi32(r1, r0);
	r0 = _mm512_set1_epi32(0xc2b2ae35);  /* c = 0xc2b2ae35 (c5) */
	r1 = _mm512_mullo_epi32(r2, r0);
	r0 = _mm512_srli_epi32(r1, 16);
	r2 = _mm512_xor_epi32(r1, r0);

	/* Return the calculated hashes. */
	_mm512_store_epi32(hashes, r2);
}

extern inline void knuthhash_32_vec512(uint32_t *keys, uint32_t *hashes)
{
	__m512i r0 = _mm512_load_epi32(keys);
	__m512i r1 = _mm512_set1_epi32(2654435761);
	__m512i r2 = _mm512_mullo_epi32(r0, r1);

	/* Return the calculated hashes. */
	_mm512_store_epi32(hashes, r2);
}

extern inline void h2hash_32_vec512(uint32_t *keys, uint32_t *hashes)
{
	__m512i r0 = _mm512_load_epi32(keys);
	__m512i r1 = _mm512_srli_epi32(r0, 16);
	__m512i r2 = _mm512_xor_epi32(r1, r0);
	r0 = _mm512_set1_epi32(0x45d9f3b);
	r1 = _mm512_mullo_epi32(r2, r0);

	r0 = _mm512_srli_epi32(r1, 16);
	r2 = _mm512_xor_epi32(r0, r1);
	r0 = _mm512_set1_epi32(0x45d9f3b);
	r1 = _mm512_mullo_epi32(r2, r0);

	r0 = _mm512_srli_epi32(r1, 16);
	r2 = _mm512_xor_epi32(r0, r1);

	/* Return the calculated hashes. */
	_mm512_store_epi32(hashes, r2);
}

#endif

#endif

