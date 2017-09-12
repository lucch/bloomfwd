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

#include <offload.h>

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
	printf("Usage: %s -d <file1> -D <file2> -dla <file3> -DLA <file4> -g1 <file5> -G1 <file6> -g2 <file7> -G2 <file8> -r <file9> [-b <buffer length>] -n <count1> -N <count2>]\n", argv[0]);
	printf("\n");
	printf("Options:\n");
	printf("  -d   \t [CPU] Distribution of prefixes according to size (netmask).\n");
	printf("  -D   \t [MIC] Distribution of prefixes according to size (netmask).\n");
	printf("  -dla \t [CPU] Prefixes to initialize DLA in the forwarding table.\n");
	printf("  -DLA \t [MIC] Prefixes to initialize DLA in the forwarding table.\n");
	printf("  -g1  \t [CPU] Prefixes to initialize G1 in the forwarding table.\n");
	printf("  -G1  \t [MIC] Prefixes to initialize G1 in the forwarding table.\n");
	printf("  -g2  \t [CPU] Prefixes to initialize G2 in the forwarding table.\n");
	printf("  -G2  \t [MIC] Prefixes to initialize G2 in the forwarding table.\n");
	printf("  -r   \t Forward IPv4 addresses in a dry-run fashion.\n");
	printf("  -b   \t Size of buffer in address for each offload (must be a multiple of 16 and, optimally, a multiple of 3904).\n");
	printf("  -n   \t Number of addresses to forward.\n");
	printf("  -z   \t Offload addresses ratio.\n");
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
	
	*addresses = _mm_malloc(len * sizeof(uint32_t), 64);
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
void forward(const char *addrs_path, unsigned long buf_len,
		unsigned long count, double mic_ratio)
{
	if (fw_tbl == NULL) {
		fprintf(stderr, "forward: 'fw_tbl' is NULL.\n");
		exit(1);
	}

	FILE *input_addr = fopen(addrs_path, "r");
	if (input_addr == NULL) {
		fprintf(stderr, "Couldn't open input addresses file: '%s'.\n",
				addrs_path);
		exit(1);
	}

	uint32_t *addresses = NULL;
	static unsigned long len;
	len = read_addresses(input_addr, &addresses);
	if (count == 0)
		count = len;

	fclose(input_addr);

#ifndef NDEBUG
	printf("Number of addresses is %lu.\n", len);
	printf("Forwarding %.2lf times (%lu addresses).\n", (double)count / len, count);
#endif
	/* 
	 * `buffer_len` must be a multiple of 16 and optimally a multiple of 
	 * 244 threads * 16 addresses = 3904 for better resource usage on Phi.
	 */
	static size_t buffer_len = 3904;
	if (buf_len > 0)
		buffer_len = buf_len;

///	/* CPU processes first half of `addresses` */
/////	size_t i_cpu = 0;
/////	size_t j_cpu = count / 2 + ((count / 2) % buffer_len) ;
/////	size_t len_cpu = j_cpu - i_cpu;
///	size_t i_cpu = 0;
///	size_t j_cpu = 0;
///	size_t len_cpu = j_cpu - i_cpu;
///
///	/* MIC processes the remaining addresses */
///	size_t i_mic = j_cpu;
///	size_t j_mic = count;
///	static size_t len_mic;
///	len_mic = j_mic - i_mic;

//	/* Percent of addresses to be processed in MIC */
//	/* TODO: Config using preprocessor! */
//	double mic_ratio = 1.0;
//	size_t mic_count = mic_ratio * count;
//	size_t mic_len = mic_ratio * len;
//	size_t cpu_count = count - mic_count;
//	size_t cpu_len = len - mic_len;
//	/* CPU processes addresses in the second part */
//	size_t cpu_offset = mic_count;

	size_t mic_count = mic_ratio * count;
	mic_count -= mic_count % buffer_len; 


	/* Output buffer (must have the same size as the `addresses` input buffer) */
	bool *found;
	uint32_t *next_hops;

#ifndef NDEBUG
	printf("mic_ratio = %lf\n", mic_ratio);
	printf("mic_count = %zu\n", mic_count);
	printf("len = %zu\n", len);
	printf("count = %zu\n", count);
	printf("buffer_len = %zu\n", buffer_len);
#endif

	/*
	 * Total number of addresses must be a multiple of `buffer_len` because
	 * there's no peeling loop implemented yet.
	 */
	if (mic_count % buffer_len != 0 || buffer_len % 16 != 0) {
		printf("forward: mic_count `mod` buffer_len != 0 || buffer_len `mod` 16 != 0\n");
		exit(1);
	}

	/* Allocate buffers on CPU */
	next_hops = _mm_malloc(buffer_len * sizeof(uint32_t), 64);
	found = _mm_malloc(buffer_len * sizeof(bool), 64);
	/* Allocate buffers on MIC */
	#pragma offload_transfer target(mic:0) \
		nocopy(addresses : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(found : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(next_hops : length(buffer_len) alloc_if(1) free_if(0))
	
	int mic_thread_id = -1;
	int max_proc = kmp_get_affinity_max_proc();

	double total_exec_time = omp_get_wtime();

#pragma omp parallel num_threads(31)
{
#ifndef NDEBUG
	char addr_str[16];
	char next_hop_str[16];
#endif
	/* MIC */
	#pragma omp single nowait
	{
		mic_thread_id = omp_get_thread_num();

		kmp_affinity_mask_t mask;
		kmp_create_affinity_mask(&mask);
		for (int i = 0; i < max_proc; i++)
			kmp_unset_affinity_mask_proc(i, &mask);
		kmp_set_affinity_mask_proc(15, &mask);
		kmp_set_affinity_mask_proc(31, &mask);
		kmp_set_affinity(&mask);

		for (size_t i = 0; i < mic_count; i += buffer_len) {
			#pragma offload target(mic:0) \
				in(addresses[(i % len):buffer_len] : into(addresses[0:buffer_len]) alloc_if(0) free_if(0)) \
				out(found : length(buffer_len) alloc_if(0) free_if(0)) \
				out(next_hops : length(buffer_len) alloc_if(0) free_if(0))
			{
				/* 
				 * IMPORTANT: `buffer_len` must be a multiple of
				 * 16 so that this loop works correctly!
				 */
				#pragma omp parallel for schedule(dynamic, 1) num_threads(244)
				for (unsigned long i = 0; i < buffer_len; i += 16)
					lookup_address_intrin(&addresses[i], &found[i], &next_hops[i]);
			}
	
			#ifndef NDEBUG
			for (int j = 0; j < buffer_len; j++) {
				/* I/O. */
				straddr(addresses[(i % len) + j], addr_str);
				straddr(next_hops[j], next_hop_str);
				#pragma omp critical
				{
					if (!found[j])
						printf("MIC: \t%s -> (none)\n", addr_str);
					else
						printf("MIC: \t%s -> %s.\n", addr_str, next_hop_str);
	  			}
			}
			#endif
		}
	}

	/* CPU */
	if (omp_get_thread_num() != mic_thread_id) {
		kmp_affinity_mask_t mask;
		kmp_create_affinity_mask(&mask);
		for (int i = 0; i <= max_proc; i++)
			kmp_set_affinity_mask_proc(i, &mask);
		kmp_unset_affinity_mask_proc(15, &mask);
		kmp_unset_affinity_mask_proc(31, &mask);
		kmp_set_affinity(&mask);

		#pragma omp for schedule(dynamic, 1) nowait
		for (size_t i = mic_count; i < count; i++) {
			/* Decode address. */
			uint32_t addr = addresses[i % len];

			/* Lookup */
			uint32_t next_hop;
		#ifndef NDEBUG
			bool found = lookup_address(addr, &next_hop);
		#else
			lookup_address(addr, &next_hop);
		#endif

		#ifndef NDEBUG
			/* I/O. */
			straddr(addr, addr_str);
			straddr(next_hop, next_hop_str);
			#pragma omp critical
			{
				if (!found)
					printf("\t%s -> (none)\n", addr_str);
				else
					printf("\t%s -> %s.\n", addr_str, next_hop_str);
			}
		#endif
		}
	}

}

	total_exec_time = omp_get_wtime() - total_exec_time;

#ifdef BENCHMARK
	printf("%lf", total_exec_time);
#endif

	/* Free allocated memory */
	_mm_free(addresses);
	_mm_free(next_hops);
	_mm_free(found);
	#pragma offload_transfer target(mic:0) \
		nocopy(addresses : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(found : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(next_hops : length(buffer_len) alloc_if(0) free_if(1))
}

void forward_async(const char *addrs_path, unsigned long buf_len,
		unsigned long count, double mic_ratio)
{
	if (fw_tbl == NULL) {
		fprintf(stderr, "forward: 'fw_tbl' is NULL.\n");
		exit(1);
	}

	FILE *input_addr = fopen(addrs_path, "r");
	if (input_addr == NULL) {
		fprintf(stderr, "Couldn't open input addresses file: '%s'.\n",
				addrs_path);
		exit(1);
	}

	uint32_t *addresses = NULL;
	static unsigned long len;
	len = read_addresses(input_addr, &addresses);
	if (count == 0)
		count = len;

	fclose(input_addr);

#ifndef NDEBUG
	printf("Number of addresses is %lu.\n", len);
	printf("Forwarding %.2lf times (%lu addresses).\n", (double)count / len, count);
#endif
	/* 
	 * `buffer_len` must be a multiple of 16 and optimally a multiple of 
	 * 244 threads * 16 addresses = 3904 for better resource usage on Phi.
	 */
	static size_t buffer_len = 3904;
	if (buf_len > 0)
		buffer_len = buf_len;

	/* Percent of addresses to be processed in MIC */
	/* TODO: Config using preprocessor! */
//	double mic_ratio = 3.0 / 4.0; /* Assuming Phi is ~ 3x faster than CPU */
//	double mic_ratio = 4.0 / 5.0; /* Assuming Phi is ~ 4x faster than CPU */
//	double mic_ratio = 5.0 / 6.0; /* Assuming Phi is ~ 5x faster than CPU */
//	double mic_ratio = 6.0 / 7.0; /* Assuming Phi is ~ 6x faster than CPU */
//	double mic_ratio = 7.0 / 8.0; /* Assuming Phi is ~ 7x faster than CPU */
//	double mic_ratio = 9.0 / 10.0; /* Assuming Phi is ~ 9x faster than CPU */
//	double mic_ratio = 10.0 / 11.0; /* Assuming Phi is ~ 10x faster than CPU */
//	double mic_ratio = 12.0 / 13.0; /* Assuming Phi is ~ 12x faster than CPU */
//	double mic_ratio = 21.0 / 22.0; /* Assuming Phi is ~ 21x faster than CPU */
//	double mic_ratio = 1.0; /* Assuming Phi is infinitely faster than CPU */
	size_t mic_count = mic_ratio * count;
	mic_count -= mic_count % buffer_len; 

	/* Phi processes the first part: 0 -> mic_count */
	/* CPU processes the second part: mic_count -> count */

	/* Buffers (must have the same size as the `addresses` input buffer) */
	uint32_t *addresses_1;
	uint32_t *addresses_2;
	bool *found_1;
	bool *found_2;
	uint32_t *next_hops_1;
	uint32_t *next_hops_2;

#ifndef NDEBUG
	printf("mic_ratio = %lf\n", mic_ratio);
	printf("mic_count = %zu\n", mic_count);
	printf("len = %zu\n", len);
	printf("count = %zu\n", count);
	printf("buffer_len = %zu\n", buffer_len);
#endif

	/*
	 * Total number of addresses must be a multiple of `buffer_len` because
	 * there's no peeling loop implemented yet.
	 */
	if (mic_count % buffer_len != 0 || buffer_len % 16 != 0) {
		printf("forward: mic_count `mod` buffer_len != 0 || buffer_len `mod` 16 != 0\n");
		exit(1);
	}

	/* Allocate buffers on CPU */
	found_1 = _mm_malloc(buffer_len * sizeof(bool), 64);
	found_2 = _mm_malloc(buffer_len * sizeof(bool), 64);
	next_hops_1 = _mm_malloc(buffer_len * sizeof(uint32_t), 64);
	next_hops_2 = _mm_malloc(buffer_len * sizeof(uint32_t), 64);
	/* Signals used to synchronize execution using double buffers */
	addresses_1 = next_hops_1;
	addresses_2 = next_hops_2;
	/* Allocate buffers on MIC */
	#pragma offload_transfer target(mic:0) \
		nocopy(addresses_1 : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(addresses_2 : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(found_1 : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(found_2 : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(next_hops_1 : length(buffer_len) alloc_if(1) free_if(0)) \
		nocopy(next_hops_2 : length(buffer_len) alloc_if(1) free_if(0))

	int mic_thread_id = -1;
	int max_proc = kmp_get_affinity_max_proc();

	double total_exec_time = omp_get_wtime();
//	double cpu_exec_time = 0.0;
//	double mic_exec_time = 0.0;

//cpu_exec_time = omp_get_wtime();


#pragma omp parallel num_threads(31)
{
#ifndef NDEBUG
	char addr_str[16];
	char next_hop_str[16];
#endif
	/* MIC */
	#pragma omp single nowait
	{
		mic_thread_id = omp_get_thread_num();

		kmp_affinity_mask_t mask;
		kmp_create_affinity_mask(&mask);
		for (int i = 0; i < max_proc; i++)
			kmp_unset_affinity_mask_proc(i, &mask);
		kmp_set_affinity_mask_proc(15, &mask);
		kmp_set_affinity_mask_proc(31, &mask);
		kmp_set_affinity(&mask);

		#pragma offload_transfer target(mic:0) if(mic_count > 0) \
			in(addresses[0:buffer_len] : into(addresses_1[0:buffer_len]) alloc_if(0) free_if(0)) \
			signal(addresses_1)
		for (size_t i = buffer_len, j = 0; i <= mic_count; i += buffer_len, j++) {
			if (j % 2 == 0) {
				#pragma offload_transfer target(mic:0) if(i < mic_count) \
					in(addresses[(i % len):buffer_len] : into(addresses_2[0:buffer_len]) alloc_if(0) free_if(0)) \
					signal(addresses_2)

				#pragma offload target(mic:0) \
					wait(addresses_1) \
					nocopy(addresses_1 : length(buffer_len) alloc_if(0) free_if(0)) \
					out(found_1 : length(buffer_len) alloc_if(0) free_if(0)) \
					out(next_hops_1 : length(buffer_len) alloc_if(0) free_if(0))
				{
					/* 
					 * IMPORTANT: `buffer_len` must be a multiple of
					 * 16 so that this loop works correctly!
					 */
					#pragma omp parallel for schedule(dynamic, 1) num_threads(244)
					for (unsigned long i = 0; i < buffer_len; i += 16) {
						lookup_address_intrin(&addresses_1[i], &found_1[i], &next_hops_1[i]);
					}
				}

				#ifndef NDEBUG
				for (int k = 0; k < buffer_len; k++) {
					/* I/O. */
					straddr(addresses[((i - buffer_len) % len) + k], addr_str);
					straddr(next_hops_1[k], next_hop_str);
					#pragma omp critical
					{
						if (!found_1[k])
							printf("B1: \t%s -> (none)\n", addr_str);
						else
							printf("B1: \t%s -> %s.\n", addr_str, next_hop_str);
					}
				}
				#endif
			} else {
				#pragma offload_transfer target(mic:0) if(i < mic_count) \
					in(addresses[(i % len):buffer_len] : into(addresses_1[0:buffer_len]) alloc_if(0) free_if(0)) \
					signal(addresses_1)

				#pragma offload target(mic:0) \
					wait(addresses_2) \
					nocopy(addresses_2 : length(buffer_len) alloc_if(0) free_if(0)) \
					out(found_2 : length(buffer_len) alloc_if(0) free_if(0)) \
					out(next_hops_2 : length(buffer_len) alloc_if(0) free_if(0))
				{
					/* 
					 * IMPORTANT: `buffer_len` must be a multiple of
					 * 16 so that this loop works correctly!
					 */
					#pragma omp parallel for schedule(dynamic, 1) num_threads(244)
					for (unsigned long i = 0; i < buffer_len; i += 16) {
						lookup_address_intrin(&addresses_2[i], &found_2[i], &next_hops_2[i]);
					}
				}

				#ifndef NDEBUG
				for (int k = 0; k < buffer_len; k++) {
					/* I/O. */
					straddr(addresses[((i - buffer_len) % len) + k], addr_str);
					straddr(next_hops_2[k], next_hop_str);
					#pragma omp critical
					{
						if (!found_2[k])
							printf("B2: \t%s -> (none)\n", addr_str);
						else
							printf("B2: \t%s -> %s.\n", addr_str, next_hop_str);
					}
				}
				#endif
			}
		}
//		mic_exec_time = omp_get_wtime() - mic_exec_time;
	}

	/* CPU */
	if (omp_get_thread_num() != mic_thread_id) {
		kmp_affinity_mask_t mask;
		kmp_create_affinity_mask(&mask);
		for (int i = 0; i <= max_proc; i++)
			kmp_set_affinity_mask_proc(i, &mask);
		kmp_unset_affinity_mask_proc(15, &mask);
		kmp_unset_affinity_mask_proc(31, &mask);
		kmp_set_affinity(&mask);

		#pragma omp for schedule(dynamic, 1) nowait
		for (size_t i = mic_count; i < count; i++) {
			/* Decode address. */
			uint32_t addr = addresses[i % len];

			/* Lookup */
			uint32_t next_hop;
		#ifndef NDEBUG
			bool found = lookup_address(addr, &next_hop);
		#else
			lookup_address(addr, &next_hop);
		#endif

		#ifndef NDEBUG
			/* I/O. */
			straddr(addr, addr_str);
			straddr(next_hop, next_hop_str);
			#pragma omp critical
			{
				if (!found)
					printf("\t%s -> (none)\n", addr_str);
				else
					printf("\t%s -> %s.\n", addr_str, next_hop_str);
			}
		#endif
		}
	}

}

//	cpu_exec_time = omp_get_wtime() - cpu_exec_time;
	total_exec_time = omp_get_wtime() - total_exec_time;

#ifdef BENCHMARK
//	printf("%lf", mic_exec_time);
//	printf("%lf", cpu_exec_time);
	printf("%lf", total_exec_time);
#endif

	/* Free allocated memory */
	_mm_free(addresses);
	_mm_free(found_1);
	_mm_free(found_2);
	_mm_free(next_hops_1);
	_mm_free(next_hops_2);
	#pragma offload_transfer target(mic:0) \
		nocopy(addresses_1 : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(addresses_2 : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(found_1 : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(found_2 : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(next_hops_1 : length(buffer_len) alloc_if(0) free_if(1)) \
		nocopy(next_hops_2 : length(buffer_len) alloc_if(0) free_if(1))
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

/* Options:
 * 	CPU:
 * 		-d <distrib file>
 * 	MIC:
 * 		-D <distrib file>
 */
static void allocate_forwarding_table(int argc, char *argv[])
{
	int index_d;
	int index_D;

	index_d = contains(argc, argv, "-d");
	index_D = contains(argc, argv, "-D");

	if (index_d != -1 && index_D != -1) {
		if (index_d + 1 < argc && index_D + 1 < argc) {
			const char *path_d = argv[index_d + 1];
			const char *path_D = argv[index_D + 1];
			/* CPU */
			init_fwtbl(path_d, NULL);
			/* MIC */
#pragma offload target(mic:0) in(path_D)
			init_fwtbl(path_D, NULL);
		} else {
			print_usage(argv);
			exit(1);
		}
	} else {
		print_usage(argv);
		exit(1);
	}
}

/* Options:
 * 	CPU:
 * 		-dla <DLA file>
 * 		-g1  <G1 file>
 * 		-g2  <G2 file>
 * 	MIC:
 * 		-DLA <DLA file>
 * 		-G1  <G1 file>
 * 		-G2  <G2 file>
 */
static void initialize_forwarding_table(int argc, char *argv[])
{
	int index_dla, index_g1, index_g2;
	int index_DLA, index_G1, index_G2;

	index_dla = contains(argc, argv, "-dla");
	index_g1 = contains(argc, argv, "-g1");
	index_g2 = contains(argc, argv, "-g2");
	index_DLA = contains(argc, argv, "-DLA");
	index_G1 = contains(argc, argv, "-G1");
	index_G2 = contains(argc, argv, "-G2");

	if (index_dla != -1 && index_g1 != -1 && index_g2 != -1 &&
	    index_DLA != -1 && index_G1 != -1 && index_G2 != -1) {
		if (index_dla + 1 < argc && index_DLA + 1 < argc) {
			const char *path_dla = argv[index_dla + 1];
			const char *path_DLA = argv[index_DLA + 1];
			/* CPU */
			load_prefixes(path_dla);
			/* MIC */
#pragma offload target(mic:0) in(path_DLA)
			load_prefixes(path_DLA);
		}
		if (index_g1 + 1 < argc && index_G1 + 1 < argc) {
			const char *path_g1 = argv[index_g1 + 1];
			const char *path_G1 = argv[index_G1 + 1];
			/* CPU */
			load_prefixes(path_g1);
			/* MIC */
#pragma offload target(mic:0) in(path_G1)
			load_prefixes(path_G1);
		}
		if (index_g2 + 1 < argc && index_G2 + 1 < argc) {
			const char *path_g2 = argv[index_g2 + 1];
			const char *path_G2 = argv[index_G2 + 1];
			/* CPU */
			load_prefixes(path_g2);
			/* MIC */
#pragma offload target(mic:0) in(path_G2)
			load_prefixes(path_G2);
		}
	}
}

/* Options:
 * 	Both CPU and MIC:
 * 		-r <address file> 
 * 		[-n <number of addresses to forward>]
 */
static void run(int argc, char *argv[])
{
	int index_r, index_b, index_n, index_z;

	index_r = contains(argc, argv, "-r");
	index_b = contains(argc, argv, "-b");
	index_n = contains(argc, argv, "-n");
	index_z = contains(argc, argv, "-z");

	if (index_r != -1) {
		if (index_r + 1 < argc) {
			size_t buffer_len = 0;
			index_b = contains(argc, argv, "-b");
			if (index_b != -1) {
				if (index_b + 1 < argc) {
					buffer_len = (size_t)strtoul(argv[index_b + 1], NULL, 10);
				} else {
					print_usage(argv);
					exit(1);
				}
			}

			unsigned long count = 0;
			index_n = contains(argc, argv, "-n");
			if (index_n != -1) {
				if (index_n + 1 < argc) {
					count = strtoul(argv[index_n + 1], NULL, 10);
				} else {
					print_usage(argv);
					exit(1);
				}
			}

			double mic_ratio = 0.9;
			if (index_z != -1) {
				if (index_z + 1 < argc) {
					mic_ratio = strtod(argv[index_z + 1], NULL);
				} else {
					print_usage(argv);
					exit(1);
				}

				if (mic_ratio < 0.0)
					mic_ratio = 0.0;

				if (mic_ratio > 1.0)
					mic_ratio = 1.0;
			}

			const char *path_r = argv[index_r + 1];
#ifdef ASYNC_OFFLOAD
//			double numers[] = { 3.0, 4.0, 5.0, 6.0, 7.0, 9.0, 10.0, 12.0, 15.0, 21.0, 1.0 };
//			double denoms[] = { 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 11.0, 13.0, 16.0, 22.0, 1.0 };
//			size_t len = 11;
//			for (int i = 0; i < len; i++) {
//				double mic_ratio = numers[i] / denoms[i];
//				for (int j = 0; j < 3; j++) {
//					forward_async(path_r, buffer_len, count, mic_ratio);
//					printf(" ");
//				}
//				printf("\n");
//			}
//
			forward_async(path_r, buffer_len, count, mic_ratio);
#else
			forward(path_r, buffer_len, count, mic_ratio);
#endif
		} else {
			print_usage(argv);
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


	allocate_forwarding_table(argc, argv);  /* Prefixes distrib. */
	initialize_forwarding_table(argc, argv);  /* Load prefixes. */
	run(argc, argv);  /* Dry-run only. */

	return 0;
}

