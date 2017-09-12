#include <limits.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "hashfunctions.h"


int main()
{
	__declspec(align(64)) uint64_t keys[8];
	__declspec(align(64)) uint32_t tmp[16];
	__declspec(align(64)) uint32_t hashes[8];

	srand(time(NULL));

	for (int j = 0; j < 100000; j++) {

		for (int i = 0; i < 8; i++) {
			uint32_t hi = rand();
			uint32_t lo = rand();
			uint64_t key = (((uint64_t)hi) << 32) | lo;
			keys[i] = key;
		}

		murmurhash3_64_vec512_v3(keys, tmp);
		hashes[0] = tmp[1];
		hashes[1] = tmp[3];
		hashes[2] = tmp[5];
		hashes[3] = tmp[7];
		hashes[4] = tmp[9];
		hashes[5] = tmp[11];
		hashes[6] = tmp[13];
		hashes[7] = tmp[15];

		for (int i = 0; i < 8; i++) {
			uint32_t hash = murmurhash3_64_32(keys[i]);
			if (hash != hashes[i]) {
				fprintf(stderr, "ERROR!\n");
				exit(1);
			}
		}
	}

	printf("OK!\n");

	return 0;
}

