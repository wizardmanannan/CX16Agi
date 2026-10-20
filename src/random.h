#ifndef _RANDOM_H_
#define _RANDOM_H_
#include "memoryManager.h"
#include <stdlib.h>

#define NO_RANDOM_NUMBERS 256

extern void b6InitRandom();
/* Exclusive bound; zero selects all 256 byte values. */
extern byte rand8Bit(byte max);
extern byte randBetween(byte min, byte max);

extern byte randomSeed;

#endif
