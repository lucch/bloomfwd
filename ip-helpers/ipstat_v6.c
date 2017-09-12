/*
 * Copyright (C) 2016 Alexandre Lucchesi <alexandrelucchesi@gmail.com>
 *
 * This program outputs strings in a format suitable for plotting histograms in
 * PGFPlots of the distribution of IPv6 prefixes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	unsigned char mask;
	unsigned int a, b, c, d, e, f, g, h;
	unsigned int length[129] = { 0 };
	unsigned int total = 0;

	for (int i = 0; i <= 128; i++) {
		assert(length[i] == 0);
	}

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		exit(0);
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file: %s.\n", argv[1]);
		exit(1);
	}

	while (fscanf(fp, "%x:%x:%x:%x:%x:%x:%x:%x/%hhu\n",
				&a, &b, &c, &d, &e, &f, &g, &h, &mask) == 9) {
		assert(mask >= 0 && mask <= 128);

		length[mask]++;
		total++;

		int nline;
		while((nline = fgetc(fp)) != '\n' && nline != EOF);
	};

	unsigned int count = 0;
	for (int i = 0; i <= 128; i++)
		count += length[i];

	assert(count == total);

	printf("PrefixLength NumberofRoutes\n");
	for (int i = 0; i <= 128; i++) {
		printf("%d %u\n", i, length[i]);
	}
	printf("TOTAL: %u\n\n", total);
	
	return 0;
}

