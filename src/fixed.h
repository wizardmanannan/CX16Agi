#ifndef _FIXED_H_
#define _FIXED_H_

#include <stdint.h>

typedef long fix32; // We are using a 32-bit number for our fixed point numbers

#define FIX32_SHIFT 16 // Fractional part is the lower 16 bits

// Convert an integer to a fixed point number
#define int_to_fix32(num) (((long)num) << FIX32_SHIFT)

#endif
