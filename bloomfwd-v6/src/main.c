/* 
 * main.c
 * 
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#include <assert.h>

#include <ctype.h>

#include <omp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bloomfwd_opt.h"
#include "config.h"  /* LOOKUP_PARALLEL, LOOKUP_ADDRESS() */
#include "prettyprint.h"

/* Execution control macros. */
#ifdef NOPRINTF
#define printf(...)  /* Disable printf. */
#endif

/* Handy macro to perform string comparison. */
#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)



void print_usage(char *argv[])
{
	printf("Usage: %s -d <file1> -p <file2> -r <file3> [-n <count>]\n", argv[0]);
	printf("\n");
	printf("Options:\n");
	printf("  -d --distribution-file \t Distribution of prefixes according to size (netmask).\n");
	printf("  -p --prefixes-file     \t Prefixes to initialize the forwarding table.\n");
	printf("  -r --run-address-file  \t Forward IPv4 addresses in a dry-run fashion.\n");
	printf("  -n --num-addresses     \t Number of addresses to forward.\n");
}

/*
 * Return the number of addresses read.
 */
static unsigned long read_addresses(FILE *input_addr, uint128 **addresses)
{
	if (input_addr == NULL) {
		fprintf(stderr, "main.read_addresses: 'input_addr' is NULL.\n");
		exit(1);
	}

	unsigned long len;
	int rc = fscanf(input_addr, "%lu", &len);

#if defined(LOOKUP_VEC_INTRIN) && defined(__MIC__)
	*addresses = _mm_malloc(len * sizeof(uint128), 64);
#else
	*addresses = malloc(len * sizeof(uint128));
#endif
	if (addresses == NULL) {
		fprintf(stderr, "main.read_addresses: Could not malloc addresses.\n");
		exit(1);
	}

	if (rc == 1) {
		unsigned int a, b, c, d, e, f, g, h;
		for (unsigned long i = 0; i < len; i++) {
			if (fscanf(input_addr, "%x:%x:%x:%x:%x:%x:%x:%x",
				&a, &b, &c, &d, &e, &f, &g, &h) != 8) {
				fprintf(stderr, "main.read_addresses: fscanf error.\n");
				exit(1);
			}
			(*addresses)[i] = new_ipv6_addr(a, b, c, d, e, f, g, h);
		}
	}

	return len;
}
/*
 * Simply forward IPv4 addresses read from 'input_addr' file 'count' times. If
 * the number of addresses in the file is smaller than 'count', this function
 * goes back to the beginning and forwards the same packets again until 'count'
 * is reached. If 'count' is 0, it forwards each address in the file once. The
 * 'input_addr' file must be formatted as follows:
 *
 * 	- First line is the number of addresses in the file;
 * 	- Remaining lines are addresses in the form A.B.C.D, where A, B, C and D
 * 	are numbers from 0 to 255.
 */
void forward(struct forwarding_table *fw_tbl, FILE *input_addr, unsigned long count)
{
	if (fw_tbl == NULL) {
		fprintf(stderr, "main.forward: 'fw_tbl' is NULL.\n");
		exit(1);
	}

	uint128 *addresses = NULL;
	unsigned long len = read_addresses(input_addr, &addresses);
	if (count == 0)
		count = len;

#ifndef NDEBUG
	printf("Number of addresses is %lu.\n", len);
	printf("Forwarding %.2lf times (%lu addresses).\n", (double)count / len, count);
#endif

#ifdef BENCHMARK
	double exec_time = 0.0;
#endif

#ifdef LOOKUP_PARALLEL
#pragma omp parallel
{
#endif

#ifndef NDEBUG
	char addr_str[40];
	char next_hop_str[40];

#ifdef LOOKUP_PARALLEL
  #pragma omp single
  {
	printf("$OMP_NUM_THREADS = %d\n", omp_get_num_threads());
	    
	omp_sched_t sched;
	int chunk_size;
	omp_get_schedule(&sched, &chunk_size);
	switch(sched) {
	case 1:
		printf("$OMP_SCHEDULE = \"static,%d\"\n", chunk_size);
		break;
	case 2:
		printf("$OMP_SCHEDULE = \"dynamic,%d\"\n", chunk_size);
		break;
	case 3:
		printf("$OMP_SCHEDULE = \"guided,%d\"\n", chunk_size);
		break;
	default:
		printf("$OMP_SCHEDULE = \"auto,%d\"\n", chunk_size);
	}
  }
#else
	printf("SERIAL\n");
#endif
#endif


#if !(defined(__MIC__) && defined(LOOKUP_VEC_INTRIN))  /* Lookup scalar */

#ifdef BENCHMARK
	exec_time = omp_get_wtime();
#endif

#ifdef LOOKUP_PARALLEL
#pragma omp for schedule(runtime)
#endif
	for (unsigned long i = 0; i < count; i++) {
		uint128 addr = addresses[i % len];
		uint128 next_hop;
#ifndef NDEBUG
		bool found = lookup_address(fw_tbl, addr, &next_hop);
#else
		lookup_address(fw_tbl, addr, &next_hop);
#endif

#ifndef NDEBUG
		/* I/O. */
		straddr(addr, addr_str);
		straddr(next_hop, next_hop_str);
#ifdef LOOKUP_PARALLEL
#pragma omp critical
  {
#endif
		if (!found)
			printf("%s -> (none)\n", addr_str);
		else
			printf("%s -> %s.\n", addr_str, next_hop_str);
#ifdef LOOKUP_PARALLEL
  }
#endif
#endif
	}
#else  /* Lookup MIC */
	/* Find the smallest number of addresses (`array_len`) that must be
	 * passed to  `lookup_address_intrin` so that the condition `(array_len
	 * * fw_tbl->distinct_lengths) % 16 == 0` holds!
	 */
	size_t array_len = 16;
	for (int i = 1; i < 16; i++) {
		if ((i * fw_tbl->distinct_lengths) % 16 == 0) {
			array_len = i;
			break;
		}
	}

	// NOTE: The execution time of the code above is not computed because
	// that computation could've been done at compile time.
#ifdef BENCHMARK
	exec_time = omp_get_wtime();
#endif

//	TODO: Create a peeling loop with `count % 16` iterations (consider using OpenMP sections).

#ifdef LOOKUP_PARALLEL
#pragma omp for schedule(runtime)
#endif
	for (unsigned long i = 0; i < count; i += array_len) {
		__declspec(align(64)) uint128 next_hops[array_len];
#ifndef NDEBUG
		__declspec(align(64)) bool found[array_len];
		lookup_address_intrin(fw_tbl, &addresses[i % len], next_hops, found, array_len);
#else
		lookup_address_intrin(fw_tbl, &addresses[i % len], next_hops, NULL, array_len);
#endif

#ifndef NDEBUG
		/* I/O. */
		for (int j = 0; j < array_len; j++) {
			straddr(addresses[(i % len) + j], addr_str);
			straddr(next_hops[j], next_hop_str);
#ifdef LOOKUP_PARALLEL
#pragma omp critical
  {
#endif
			if (!found[j])
				printf("%s -> (none)\n", addr_str);
			else
				printf("%s -> %s.\n", addr_str, next_hop_str);
#ifdef LOOKUP_PARALLEL
  }
#endif
  		}
#endif
	}
#endif  /* __MIC__ */
#ifdef LOOKUP_PARALLEL
}
#endif

#ifdef BENCHMARK
	exec_time = omp_get_wtime() - exec_time;
	printf("%lf", exec_time);
#endif

#if defined(__MIC__) && defined(LOOKUP_VEC_INTRIN)
	_mm_free(addresses);
#else
	free(addresses);
#endif
}

static inline int contains(int argc, char *argv[], const char *option)
{
	int index = -1;
	for (int i = 1; (i < argc) && (index == -1); i++) {
		if (STREQ(argv[i], option)) {
			index = i; 
			break;
		}
	}

	return index;
}

/* Options: -d, --distribution-file. */
static void allocate_forwarding_table(int argc, char *argv[],
		struct forwarding_table **fw_tbl)
{
	int index;

	if ((index = contains(argc, argv, "--distribution-file")) == -1)
		index = contains(argc, argv, "-d");

	if (index != -1) {
		if (index + 1 < argc) {
			FILE *pfx_distribution = fopen(argv[index + 1], "r");
			if (pfx_distribution == NULL) {
				fprintf(stderr, "Couldn't open prefixes distribution file: '%s'.\n",
						argv[index + 1]);
				exit(1);
			}

			/*
			 * TODO: Optionally get second argument for
			 * 'new_forwarding_table' (gateway default) from 'argv'.
			 */
			*fw_tbl = new_forwarding_table(pfx_distribution, NULL);
			fclose(pfx_distribution);
		} else {
			fprintf(stderr, "Please specify prefixes distribution file after '%s'.\n",
					argv[index]);
			exit(1);
		}
	} else {
		*fw_tbl = new_forwarding_table(NULL, NULL);
	}
}

/* Options: -p, --prefixes-file. */
static void initialize_forwarding_table(struct forwarding_table *fw_tbl, int argc,
		char *argv[])
{
	int index;

	if ((index = contains(argc, argv, "--prefixes-file")) == -1)
		index = contains(argc, argv, "-p");

	if (index != -1) {
		if (index + 1 < argc) {
			FILE *prefixes = fopen(argv[index + 1], "r");
			if (prefixes == NULL) {
				fprintf(stderr, "Couldn't open prefixes file: '%s'.\n",
						argv[index + 1]);
				exit(1);
			}

			load_prefixes(fw_tbl, prefixes);
			fclose(prefixes);
		}
	}
}

/* Options:
 *   -r, --run-address-file
 *   -n, --num-addresses
 */
static void run(struct forwarding_table *fw_tbl, int argc, char *argv[])
{
	int index;

	if ((index = contains(argc, argv, "--run-address-file")) == -1)
		index = contains(argc, argv, "-r");

	if (index != -1) {
		if (index + 1 < argc) {
			FILE *input_addr = fopen(argv[index + 1], "r");
			if (input_addr == NULL) {
				fprintf(stderr, "Couldn't open input addresses file: '%s'.\n",
						argv[index + 1]);
				exit(1);
			}

			unsigned long count = 0;

			index = contains(argc, argv, "--num-addresses");
			if (index == -1)
				index = contains(argc, argv, "-n");

			if (index != -1) {
				if (index + 1 < argc) {
					count = strtoul(argv[index + 1], NULL, 10);
				} else {
					fprintf(stderr, "main.run: Missing number of addresses.\n");
					exit(1);
				}
			}

			forward(fw_tbl, input_addr, count);

			fclose(input_addr);
		} else {
			fprintf(stderr, "main.run: Missing address file.\n");
			exit(1);
		}
	} else {
		print_usage(argv);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2 || STREQ(argv[1], "--help")) {
		print_usage(argv);
		return 0;
	}

	struct forwarding_table *fw_tbl = NULL;

//	stats.bf_match = 0;
//	stats.ht_match = 0;

	allocate_forwarding_table(argc, argv, &fw_tbl);  /* Prefixes distrib. */
	initialize_forwarding_table(fw_tbl, argc, argv);  /* Load prefixes. */
	run(fw_tbl, argc, argv);  /* Dry-run only. */

//	printf("\n\nstats.bf_match = %llu\n", stats.bf_match);
//	printf("\n\nstats.ht_match = %llu\n", stats.ht_match);

	return 0;
}

