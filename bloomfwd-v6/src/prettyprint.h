#ifndef PRETTY_PRINT_H
#define PRETTY_PRINT_H

#include "bloomfwd_opt.h"

/*
 * A string representation of the prefix in the format:
 *   "<CIDR Address>/<Netmask> -> <Next Hop>"
 */
char *strpfx(const struct ipv6_prefix *pfx);

void straddr(uint128 addr, char *str);

#endif

