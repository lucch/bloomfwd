#ifndef PRETTY_PRINT_H
#define PRETTY_PRINT_H

#include "bloomfwd_opt.h"

/*
 * A string representation of the prefix in the format:
 *   "<CIDR Address>/<Netmask> -> <Next Hop>"
 */
char *strpfx(const struct ipv4_prefix *pfx);

void straddr(uint32_t addr, char *str);

#endif

