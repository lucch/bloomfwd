#include <stdio.h>


#include <ctype.h>
#include <inttypes.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ip.h"
#include "miht.h"
#include "config.h"  /* LOOKUP_PARALLEL, LOOKUP_ADDRESS() */
#include "prettyprint.h"

/* Handy macro to perform string comparison. */
#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)

typedef struct miht fwdtbl;

void print_usage(char *argv[])
{
	printf("Usage: %s -p <file1> -r <file2> [-n <count>]\n", argv[0]);
	printf("\n");
	printf("Options:\n");
	printf("  -p --prefixes-file     \t Prefixes to initialize the forwarding table.\n");
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
	
	*addresses = malloc(len * sizeof(uint32_t));
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
			(*addresses)[i] = ip_addr(a, b, c, d);
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
void forward(fwdtbl *fw_tbl, FILE *input_addr, unsigned long count)
{
	if (fw_tbl == NULL) {
		fprintf(stderr, "main.forward: 'fw_tbl' is NULL.\n");
		exit(1);
	}

	uint32_t *addresses = NULL;
	unsigned long len = read_addresses(input_addr, &addresses);
	if (count == 0)
		count = len;

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

#ifdef LOOKUP_PARALLEL
#pragma omp for schedule(runtime)
#endif
	for (unsigned long i = 0; i < count; i++) {
		/* Decode address. */
		uint32_t addr = addresses[i % len];

		/* Lookup */
		unsigned int next_hop;
#ifdef BENCHMARK
		LOOKUP_ADDRESS(fw_tbl, addr, 32, &next_hop);  /* Discard return. */
#else
		bool found = LOOKUP_ADDRESS(fw_tbl, addr, 32, &next_hop);
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
#ifdef LOOKUP_PARALLEL
}
#endif

#ifdef BENCHMARK
	exec_time = omp_get_wtime() - exec_time;
	printf("%lf", exec_time);
#endif

	free(addresses);
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
		fwdtbl **fw_tbl)
{
	*fw_tbl = miht_create(16, 16);
}

/* Options: -p, --prefixes-file. */
static void initialize_forwarding_table(fwdtbl *fw_tbl, int argc,
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

			miht_load(fw_tbl, prefixes);
			fclose(prefixes);
		}
	}
}

/* Options:
 *   -r, --run-address-file
 *   -n, --num-addresses
 */
static void run(fwdtbl *fw_tbl, int argc, char *argv[])
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
	if (argc < 3 || STREQ(argv[1], "--help")) {
		print_usage(argv);
		return 0;
	}

	fwdtbl *fw_tbl = NULL;

	allocate_forwarding_table(argc, argv, &fw_tbl);
	initialize_forwarding_table(fw_tbl, argc, argv);  /* Load prefixes. */
	run(fw_tbl, argc, argv);  /* Dry-run only. */

	return 0;
}

