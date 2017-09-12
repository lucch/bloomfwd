#ifndef HASHFUNCTIONS_H
#define HASHFUNCTIONS_H

/*
 * For some reason, this function is not declared in the Intel C Compiler's
 * `stdlib.h`, which causes "implicit declaration" warnings. It links and the
 * program works properly, though.
 */
extern int rand_r(unsigned int *seedp);

#endif
