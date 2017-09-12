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
 * Enable or disable benchmark.
 *
 * Default: disable.
 */
#if defined(BENCHMARK) && !defined(NDEBUG)
#define NDEBUG
#endif

#endif

