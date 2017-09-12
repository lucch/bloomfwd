/*
 * Copyright (C) 2016 Alexandre Lucchesi <alexandrelucchesi@gmail.com>
 *
 * This program generates IPv6 addresses or prefixes.
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
			unsigned short a, b, c, d, e, f, g, h;
			a = rand() % 65536;
			b = rand() % 65536;
			c = rand() % 65536;
			d = rand() % 65536;
			e = rand() % 65536;
			f = rand() % 65536;
			g = rand() % 65536;
			h = rand() % 65536;
			printf("%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n", a, b, c, d, e, f, g, h);
		}
	} else { // Generate prefixes
		// NOTE: This is a very stupid algorithm!
		unsigned min_netmask = atoi(argv[2]);
		if (min_netmask > 128)
			min_netmask = 128;

		for (unsigned i = 0; i < qty; i++) {
			unsigned short a, b, c, d, e, f, g, h; 
			unsigned char mask;
			a = rand() % 65536;
			b = rand() % 65536;
			c = rand() % 65536;
			d = rand() % 65536;
			e = rand() % 65536;
			f = rand() % 65536;
			g = rand() % 65536;
			h = rand() % 65536;
			do mask = rand() % 129; while (mask < min_netmask);
			printf("%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu\n", a, b, c, d, e, f, g, h, mask);
		}
	}

	return 0;
}

