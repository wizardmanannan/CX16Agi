#ifndef _RANDOM_H_
#define _RANDOM_H_
#include "memoryManager.h"
#include <stdlib.h>

#define NO_RANDOM_NUMBERS 256

extern void b6InitRandom();
extern byte rand8Bit(byte max);

extern byte randomSeed;

#endif
