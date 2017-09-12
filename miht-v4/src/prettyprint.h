#ifndef PRETTY_PRINT_H
#define PRETTY_PRINT_H

#include "ip.h"

/*
 * A string representation of the prefix in the format:
 *   "<CIDR Address>/<Netmask> -> <Next Hop>"
 */
char *strpfx(const struct ip_prefix *pfx);

void straddr(uint32_t addr, char *str);

#endif

