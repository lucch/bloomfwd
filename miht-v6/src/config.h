/* 
 * config.h
 * 
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * Enable or disable parallelism in lookup (OpenMP threads).
 *
 * Default: disable.
 */
#ifndef LOOKUP_PARALLEL
#undef LOOKUP_PARALLEL
#endif

/*
 * Enable or disable vectorization in lookup (set the lookup variant to be used).
 *
 * Default: disable.
 */
#define LOOKUP_ADDRESS miht_lookup
//#if defined(LOOKUP_VEC_AUTOVEC)
//#define LOOKUP_ADDRESS lookup_address_autovec
//#elif defined(LOOKUP_VEC_AUTOVEC_TWOSTEPS)
//#define LOOKUP_ADDRESS lookup_address_autovec_twosteps
//#elif defined(LOOKUP_VEC_INTRIN)
//#define LOOKUP_ADDRESS lookup_address_intrin
//#elif defined(LOOKUP_VEC_INTRIN_TWOSTEPS)
//#define LOOKUP_ADDRESS lookup_address_intrin_twosteps
//#else  /* Scalar */
//#define LOOKUP_ADDRESS lookup_address
//#endif

/*
 * Enable or disable benchmark.
 *
 * Default: disable.
 */
#if defined(BENCHMARK) && !defined(NDEBUG)
#define NDEBUG
#endif

#endif

