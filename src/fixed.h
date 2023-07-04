#ifndef _FIXED_H_
#define _FIXED_H_

#include <stdint.h>

typedef uint16_t fix16; // We are using a 16-bit number for our fixed point numbers

#define FIX16_SHIFT 8 // Fractional part is the lower 8 bits

// Convert an integer to a fixed point number
#define int_to_fix16(num) ((num) << FIX16_SHIFT)

// Convert a fixed point number to integer (round down)
#define fix16_to_int_round_down(num) ((num) >> FIX16_SHIFT)

// Convert a fixed point number to integer (round nearest)
#define fix16_to_int_round_nearest(x) ((x) >= 0 ? ((x + (1 << (FIX16_SHIFT - 1))) >> FIX16_SHIFT) : ((x - (1 << (FIX16_SHIFT - 1))) >> FIX16_SHIFT))

// Fixed point multiplication
#define fix16_mul(a, b) (((uint32_t)(a) * (uint32_t)(b)) >> FIX16_SHIFT)

// Fixed point division
#define fix16_div(a, b) (((uint32_t)(a) << FIX16_SHIFT) / (b))

// Fixed point addition
#define fix16_add(a, b) ((a) + (b))

// Fixed point subtraction
#define fix16_sub(a, b) ((a) - (b))

#endif
