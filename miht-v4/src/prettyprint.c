/* 
 * prettyprint.c
 *
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "prettyprint.h"

char *strpfx(const struct ip_prefix *pfx)
{
	char *str = malloc(38 * sizeof(char));
	if (str == NULL) {
		fprintf(stderr, "prettyprint.strpfx: Couldn't malloc string.\n");
		exit(1);
	}

	sprintf(str, "%3"PRIu8 ".%3"PRIu8 ".%3"PRIu8 ".%3"PRIu8 "/%2"PRIu8
			" -> %3"PRIu8 ".%3"PRIu8 ".%3"PRIu8 ".%3"PRIu8,
			(uint8_t)(pfx->prefix >> 24),
			(uint8_t)((pfx->prefix & 0x00ff0000) >> 16),
			(uint8_t)((pfx->prefix & 0x0000ff00) >> 8),
			(uint8_t)(pfx->prefix & 0x000000ff),
			pfx->len,
			(uint8_t)(pfx->next_hop >> 24),
			(uint8_t)((pfx->next_hop & 0x00ff0000) >> 16),
			(uint8_t)((pfx->next_hop & 0x0000ff00) >> 8),
			(uint8_t)(pfx->next_hop & 0x000000ff));

	return str;
}

void straddr(uint32_t addr, char *str)
{
	sprintf(str, "%3" PRIu8 ".%3" PRIu8 ".%3" PRIu8 ".%3" PRIu8,
			(uint8_t)(addr >> 24),
			(uint8_t)((addr & 0X00ff0000) >> 16),
			(uint8_t)((addr & 0X0000ff00) >> 8),
			(uint8_t)(addr & 0X000000ff));
}

