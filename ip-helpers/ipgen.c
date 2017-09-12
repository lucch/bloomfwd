/*
 * Copyright (C) 2014 Alexandre Lucchesi <alexandrelucchesi@gmail.com>
 *
 * This program generates IPv4 addresses or prefixes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

int main(int argc, char *argv[]) {

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <quantity> [<min netmask>]\n", argv[0]);
		exit(0);
	}

	unsigned qty = atoi(argv[1]);

	srand(time(NULL));

	if (argc == 2) { // Generate IP addresses
		printf("%u\n", qty);
		for (unsigned i = 0; i < qty; i++) {
			unsigned char a, b, c, d;
			a = rand() % 256;
			b = rand() % 256;
			c = rand() % 256;
			d = rand() % 256;
			printf("%hhu.%hhu.%hhu.%hhu\n", a, b, c, d);
		}
	} else { // Generate prefixes
		// NOTE: This is a very stupid algorithm!
		unsigned min_netmask = atoi(argv[2]);
		if (min_netmask > 32)
			min_netmask = 32;

		for (unsigned i = 0; i < qty; i++) {
			unsigned char a, b, c, d, mask;
			a = rand() % 256;
			b = rand() % 256;
			c = rand() % 256;
			d = rand() % 256;
			do mask = rand() % 33; while (mask < min_netmask);
			printf("%hhu.%hhu.%hhu.%hhu/%hhu\n", a, b, c, d, mask);
		}
	}

	return 0;
}

