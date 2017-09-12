/* 
 * prettyprint.c
 *
 * Copyright (C) 2015  Alexandre Lucchesi <alexandrelucchesi@gmail.com> 
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "prettyprint.h"

char *strpfx(const struct ip_prefix *pfx)
{
	/* IPv6 addr  :  39 chars
	 * IPv6 prefix:  53 chars
	 * " -> "     :   4 chars
	 * ________________________
	 * Total      :  96 chars
	 */
	char *str = malloc(96 * sizeof(char));
	assert (str != NULL);

	unsigned char len;
	unsigned a0, b0, c0, d0, e0, f0, g0, h0;
	unsigned a1, b1, c1, d1, e1, f1, g1, h1;
	a0 = (unsigned)(pfx->prefix >> 48);
	b0 = (unsigned)((pfx->prefix & 0x0000ffff00000000) >> 32);
	c0 = (unsigned)((pfx->prefix & 0x00000000ffff0000) >> 16);
	d0 = (unsigned)(pfx->prefix & 0x000000000000ffff);
	e0 = f0 = g0 = h0 = 0;  /* Prefixes length are up to 64 bits! */
	len = (unsigned char)pfx->len;
	a1 = (unsigned)(pfx->next_hop.hi >> 48);
	b1 = (unsigned)((pfx->next_hop.hi & 0x0000ffff00000000) >> 32);
	c1 = (unsigned)((pfx->next_hop.hi & 0x00000000ffff0000) >> 16);
	d1 = (unsigned)(pfx->next_hop.hi & 0x000000000000ffff);
	e1 = (unsigned)(pfx->next_hop.lo >> 48);
	f1 = (unsigned)((pfx->next_hop.lo & 0x0000ffff00000000) >> 32);
	g1 = (unsigned)((pfx->next_hop.lo & 0x00000000ffff0000) >> 16);
	h1 = (unsigned)(pfx->next_hop.lo & 0x000000000000ffff);

	sprintf(str, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%hhu -> "
		     "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
			a0, b0, c0, d0, e0, f0, g0, h0, len,
			a1, b1, c1, d1, e1, f1, g1, h1);

	return str;
}

void straddr(uint128 addr, char *str)
{
	unsigned a, b, c, d, e, f, g, h;
	a = (unsigned)(addr.hi >> 48);
	b = (unsigned)((addr.hi & 0x0000ffff00000000) >> 32);
	c = (unsigned)((addr.hi & 0x00000000ffff0000) >> 16);
	d = (unsigned)(addr.hi & 0x000000000000ffff);
	e = (unsigned)(addr.lo >> 48);
	f = (unsigned)((addr.lo & 0x0000ffff00000000) >> 32);
	g = (unsigned)((addr.lo & 0x00000000ffff0000) >> 16);
	h = (unsigned)(addr.lo & 0x000000000000ffff);

	sprintf(str, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", a, b, c, d, e, f, g, h);
}

