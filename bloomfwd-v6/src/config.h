/* 
 * config.h
 * 
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * The desired false positive ratio for the Bloom filters.
 *
 * Default: 0.01 (1%). */
#ifndef FALSE_POSITIVE_RATIO
#define FALSE_POSITIVE_RATIO 0.01
#endif

/*
 * Enable or disable parallelism in lookup (OpenMP threads).
 *
 * Default: disable.
 */
#ifndef LOOKUP_PARALLEL
#undef LOOKUP_PARALLEL
#endif

/*
 * Set the hash function to be used.
 *
 * Default: MurmurHash3
 */
/* WARNING: For IPv6, only scalar Murmur is currently implemented! */
#define BLOOM_HASH_FUNCTION murmurhash3_32
#define BLOOM_HASH_FUNCTION_64 murmurhash3_64_32
#define BLOOM_HASH_FUNCTION_128 murmurhash3_128_32
// TODO: Rename '*INTRIN*' hash functions.
#define BLOOM_HASH_FUNCTION_INTRIN murmurhash3_32_vec512_v3
#define BLOOM_HASH_FUNCTION_INTRIN_64 murmurhash3_64_vec512_v3
#define BLOOM_HASH_FUNCTION_INTRIN_128 murmurhash3_128_vec512_v3
#define BLOOM_HASH_FUNCTION_MURMUR
//#if defined(BLOOM_H2_HASH)
//#define BLOOM_HASH_FUNCTION h2hash_32
//#define BLOOM_HASH_FUNCTION_INTRIN h2hash_32_vec512
//#define BLOOM_HASH_FUNCTION_H2
//#elif defined(BLOOM_KNUTH_HASH)
//#define BLOOM_HASH_FUNCTION knuthhash_32
//#define BLOOM_HASH_FUNCTION_INTRIN knuthhash_32_vec512
//#define BLOOM_HASH_FUNCTION_KNUTH
//#else  /* if defined(BLOOM_MURMUR_HASH) */
//#define BLOOM_HASH_FUNCTION murmurhash3_32
//#define BLOOM_HASH_FUNCTION_INTRIN murmurhash3_32_vec512_v3
//#define BLOOM_HASH_FUNCTION_MURMUR
//#endif

#define HASHTBL_HASH_FUNCTION murmurhash3_32
#define HASHTBL_HASH_FUNCTION_64 murmurhash3_64_32
#define HASHTBL_HASH_FUNCTION_128 murmurhash3_128_32
// TODO: Rename '*INTRIN*' hash functions.
#define HASHTBL_HASH_FUNCTION_INTRIN murmurhash3_32_vec512_v3
#define HASHTBL_HASH_FUNCTION_INTRIN_64 murmurhash3_64_vec512_v3
#define HASHTBL_HASH_FUNCTION_INTRIN_128 murmurhash3_128_vec512_v3
#ifdef BLOOM_HASH_FUNCTION_MURMUR
#define SAME_HASH_FUNCTIONS
#endif
//#if defined(HASHTBL_H2_HASH)
//#define HASHTBL_HASH_FUNCTION h2hash_32
//#define HASHTBL_HASH_FUNCTION_INTRIN h2hash_32_vec512
//#ifdef BLOOM_HASH_FUNCTION_H2
//#define SAME_HASH_FUNCTIONS
//#endif
//#elif defined(HASHTBL_KNUTH_HASH)
//#define HASHTBL_HASH_FUNCTION knuthhash_32
//#define HASHTBL_HASH_FUNCTION_INTRIN knuthhash_32_vec512
//#ifdef BLOOM_HASH_FUNCTION_KNUTH
//#define SAME_HASH_FUNCTIONS
//#endif
//#else  /* if defined(HASHTBL_MURMUR_HASH) */
//#define HASHTBL_HASH_FUNCTION murmurhash3_32
//#define HASHTBL_HASH_FUNCTION_INTRIN murmurhash3_32_vec512_v3
//#ifdef BLOOM_HASH_FUNCTION_MURMUR
//#define SAME_HASH_FUNCTIONS
//#endif
//#endif


/*
 * Enable or disable vectorization in lookup (set the lookup variant to be used).
 *
 * Default: disable.
 */
#if defined(LOOKUP_VEC_AUTOVEC)
#define LOOKUP_ADDRESS lookup_address_autovec
#elif defined(LOOKUP_VEC_AUTOVEC_TWOSTEPS)
#define LOOKUP_ADDRESS lookup_address_autovec_twosteps
#elif defined(LOOKUP_VEC_INTRIN)
#define LOOKUP_ADDRESS lookup_address_intrin
#elif defined(LOOKUP_VEC_INTRIN_TWOSTEPS)
#define LOOKUP_ADDRESS lookup_address_intrin_twosteps
#else  /* Scalar */
#define LOOKUP_ADDRESS lookup_address
#endif

/*
 * Enable or disable benchmark.
 *
 * Default: disable.
 */
#if defined(BENCHMARK) && !defined(NDEBUG)
#define NDEBUG
#endif

#endif

