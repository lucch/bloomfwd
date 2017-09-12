/*
 * Copyright (C) 2014 Alexandre Lucchesi <alexandrelucchesi@gmail.com>
 *
 * This program outputs strings in a format suitable for plotting histograms in
 * PGFPlots of the distribution of IPv4 addresses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	unsigned char a, b, c, d, mask;
	unsigned int length[33] = { 0 };
	unsigned int total = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		exit(0);
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file: %s.\n", argv[1]);
		exit(1);
	}

	while (fscanf(fp, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) == 4) {
		if (fscanf(fp, "/%hhu", &mask) != 1) {
			mask = 0;
			if (d > 0)
				mask = 32;
			else if (c > 0)
				mask = 24;
			else if (b > 0)
				mask = 16;
			else if (a > 0)
				mask = 8;
		}

		if (mask < 0 || mask > 32) {
			fprintf(stderr, "ipstat.main: Invalid netmask: %hhu.%hhu.%hhu.%hhu/%hhu\n", a, b, c, d, mask);
		} else {
			length[mask]++;
			total++;
		}

		int nline;
		while((nline = fgetc(fp)) != '\n' && nline != EOF);
	};

	unsigned int count = 0;
	for (int i = 0; i <= 32; i++)
		count += length[i];

	assert(count == total);

	printf("PrefixLength NumberofRoutes\n");
	for (int i = 0; i <= 32; i++) {
		printf("%d %u\n", i, length[i]);
	}
	printf("TOTAL: %u\n\n", total);

	int leq20 = 0;
	for (int i = 0; i <= 20; i++) {
		printf("%d %u\n", i, length[i]);
		leq20 += length[i];
	}
	printf("LEQ20: %u\n\n", leq20);

	int leq24 = 0;
	for (int i = 21; i <= 24; i++) {
		printf("%d %u\n", i, length[i]);
		leq24 += length[i];
	}
	printf("LEQ24: %u\n\n", leq24);

	int gt24 = 0;
	for (int i = 25; i <= 32; i++) {
		printf("%d %u\n", i, length[i]);
		gt24 += length[i];
	}
	printf("GT24: %u\n", gt24);

	assert(gt24 + leq24 + leq20 == total);

	return 0;
}

