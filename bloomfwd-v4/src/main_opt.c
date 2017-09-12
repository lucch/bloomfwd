/* 
 * main_opt.c
 *
 * Optimized version of the algorithm!
 * 
 * Copyright (C) 2016  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#include <assert.h>

#include <ctype.h>
#include <inttypes.h>
#include <omp.h>
#include <stdint.h>
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
	printf("  -dla --dla-file        \t Prefixes to initialize DLA in the forwarding table.\n");
	printf("  -g1 --g1-file          \t Prefixes to initialize G1 in the forwarding table.\n");
	printf("  -g2 --g2-file          \t Prefixes to initialize G2 in the forwarding table.\n");
	printf("  -r --run-address-file  \t Forward IPv4 addresses in a dry-run fashion.\n");
	printf("  -n --num-addresses     \t Number of addresses to forward.\n");
}

/*
 * Return the number of addresses read.
 */
static unsigned long read_addresses(FILE *input_addr, uint32_t **addresses)
{
	if (input_addr == NULL) {
		fprintf(stderr, "main.read_addresses: 'input_addr' is NULL.\n");
		exit(1);
	}

	unsigned long len;
	int rc = fscanf(input_addr, "%lu", &len);
	
#if defined(LOOKUP_VECTOR) && defined(__MIC__)
	*addresses = _mm_malloc(len * sizeof(uint32_t), 64);
#else
	*addresses = malloc(len * sizeof(uint32_t));
#endif
	if (addresses == NULL) {
		fprintf(stderr, "main.read_addresses: Could not malloc addresses.\n");
		exit(1);
	}

	if (rc == 1) {
		uint8_t a, b, c, d;
		for (unsigned long i = 0; i < len; i++) {
			if (fscanf(input_addr,
				"%" SCNu8 ".%" SCNu8 ".%" SCNu8 ".%" SCNu8,
				&a, &b, &c, &d) != 4) {
				fprintf(stderr, "main.forward: fscanf error.\n");
				exit(1);
			}
			(*addresses)[i] = new_ipv4_addr(a, b, c, d);
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

	uint32_t *addresses = NULL;
	unsigned long len = read_addresses(input_addr, &addresses);
	if (count == 0)
		count = len;

#ifdef LOOKUP_VECTOR
	assert(count % 16 == 0);
#endif

#ifndef NDEBUG
	printf("Number of addresses is %lu.\n", len);
	printf("Forwarding %.2lf times (%lu addresses).\n", (double)count / len, count);
#endif
	
#ifdef BENCHMARK
	double exec_time = omp_get_wtime();
#endif

#ifdef LOOKUP_PARALLEL
#pragma omp parallel
{
#endif

#ifndef NDEBUG
	char addr_str[16];
	char next_hop_str[16];

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

#ifndef NDEBUG
	for (int i = 0; i < 2; i++) {
		bloomf_match_addrs[i] = NULL;
		bloomf_match_addrs_count[i] = 0;
		bloomf_maybe_addrs[i] = NULL;
		bloomf_maybe_addrs_count[i] = 0;
	}
#endif

#ifdef LOOKUP_PARALLEL
#pragma omp for schedule(runtime)
#endif
#ifdef LOOKUP_SCALAR
	for (unsigned long i = 0; i < count; i++) {
		/* Decode address. */
		uint32_t addr = addresses[i % len];

		/* Lookup */
		uint32_t next_hop;
#ifndef NDEBUG
		bool found = LOOKUP_ADDRESS(fw_tbl, addr, &next_hop);
#else
		LOOKUP_ADDRESS(fw_tbl, addr, &next_hop);
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
			printf("\t%s -> (none)\n", addr_str);
		else
			printf("\t%s -> %s.\n", addr_str, next_hop_str);
#ifdef LOOKUP_PARALLEL
  }
#endif
#endif
	}
#else /* #ifdef LOOKUP_VECTOR */
	/* 
	 * IMPORTANT: `count` must be a multiple of 16 so that this loop works
	 * correctly!
	 */
	for (unsigned long i = 0; i < count; i += 16) {
		__declspec(align(64)) uint32_t next_hops[16];
		__declspec(align(64)) bool found[16];
#ifndef NDEBUG
		LOOKUP_ADDRESS(fw_tbl, &addresses[i % len], found, next_hops);
#else
		LOOKUP_ADDRESS(fw_tbl, &addresses[i % len], found, next_hops);
#endif

#ifndef NDEBUG
		for (int j = 0; j < 16; j++) {
			/* I/O. */
			straddr(addresses[(i % len) + j], addr_str);
			straddr(next_hops[j], next_hop_str);
#ifdef LOOKUP_PARALLEL
#pragma omp critical
  {
#endif
			if (!found[j])
				printf("\t%s -> (none)\n", addr_str);
			else
				printf("\t%s -> %s.\n", addr_str, next_hop_str);
#ifdef LOOKUP_PARALLEL
  }
#endif
		}
#endif
	}

#endif
#ifdef LOOKUP_PARALLEL
}
#endif

#ifdef BENCHMARK
	exec_time = omp_get_wtime() - exec_time;
	printf("%lf", exec_time);
#endif

#if defined(LOOKUP_VECTOR) && defined(__MIC__)
	_mm_free(addresses);
#else
	free(addresses);
#endif

//#ifndef NDEBUG
//	// MATCH
//	printf("\nMatching addresses...\n");
//	unsigned long long total_addrs = 0;
//	for (int i = 0; i < 2; i++) {
//		printf("#%2d = %llu\n", i, bloomf_match_addrs_count[i]);
//		total_addrs += bloomf_match_addrs_count[i];
//	}
//	printf("TOTAL MATCH ADDRS: %llu\n", total_addrs);
//
//	char addr_s[16];
//	/*
//	printf("\n#3\n");
//	for (struct addr_list *a = bloomf_match_addrs[3]; a != NULL; a = a->next) {
//		straddr(a->addr, addr_s);
//		printf("%s\n", addr_s);
//	}
//	*/
//	FILE *fp = NULL;
//	char filename[1000];
//	char filename_fmt[] = "/Users/alexandrelucchesi/Development/c/bloomfwd/tmp/%d.txt";
//	for (int i = 0; i < 2; i++) {
//		sprintf(filename, filename_fmt, 32 - i);
//		fp = fopen(filename, "w");
//		assert(fp != NULL);
//		struct addr_list *a = bloomf_match_addrs[i];
//		while (a != NULL) {
//			straddr(a->addr, addr_s);
//			fprintf(fp, "%s\n", addr_s);
//			a = a->next;
//		}
//		fclose(fp);
//	}
//
//	// MAYBE
//	printf("\nMaybe addresses...\n");
//	total_addrs = 0;
//	for (int i = 0; i < 2; i++) {
//		printf("#%2d = %llu\n", i, bloomf_maybe_addrs_count[i]);
//		total_addrs += bloomf_maybe_addrs_count[i];
//	}
//	printf("MAYBE COUNT: %llu\n", total_addrs);
//
//	printf("\n#3\n");
//	for (struct addr_list *a = bloomf_maybe_addrs[3]; a != NULL; a = a->next) {
//		straddr(a->addr, addr_s);
//		printf("%s\n", addr_s);
//	}
//#endif
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

/* Options: -g1, --g1-file and -g2, --g2-file. */
static void initialize_forwarding_table(struct forwarding_table *fw_tbl, int argc,
		char *argv[])
{
	int index_dla, index_g1, index_g2;

	if ((index_dla = contains(argc, argv, "--dla-file")) == -1)
		index_dla = contains(argc, argv, "-dla");

	if ((index_g1 = contains(argc, argv, "--g1-file")) == -1)
		index_g1 = contains(argc, argv, "-g1");

	if ((index_g2 = contains(argc, argv, "--g2-file")) == -1)
		index_g2 = contains(argc, argv, "-g2");

	if (index_dla != -1 && index_g1 != -1 && index_g2 != -1) {
		if (index_dla + 1 < argc) {
			FILE *prefixes = fopen(argv[index_dla + 1], "r");
			if (prefixes == NULL) {
				fprintf(stderr, "Couldn't open prefixes file: '%s'.\n",
						argv[index_dla + 1]);
				exit(1);
			}

			load_prefixes(fw_tbl, prefixes);
			fclose(prefixes);
		}
		if (index_g1 + 1 < argc) {
			FILE *prefixes = fopen(argv[index_g1 + 1], "r");
			if (prefixes == NULL) {
				fprintf(stderr, "Couldn't open prefixes file: '%s'.\n",
						argv[index_g1 + 1]);
				exit(1);
			}

			load_prefixes(fw_tbl, prefixes);
			fclose(prefixes);
		}
		if (index_g2 + 1 < argc) {
			FILE *prefixes = fopen(argv[index_g2 + 1], "r");
			if (prefixes == NULL) {
				fprintf(stderr, "Couldn't open prefixes file: '%s'.\n",
						argv[index_g2 + 1]);
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

    stats.bf_match = 0;
    stats.ht_match = 0;

	allocate_forwarding_table(argc, argv, &fw_tbl);  /* Prefixes distrib. */
	initialize_forwarding_table(fw_tbl, argc, argv);  /* Load prefixes. */
	run(fw_tbl, argc, argv);  /* Dry-run only. */

    printf("\n\nstats.bf_match = %llu\n", stats.bf_match);
    printf("\n\nstats.ht_match = %llu\n", stats.ht_match);

	return 0;
}

