#include "fixed.h"

#pragma code-name (push, "BANKRAM01")
// Convert a standard integer to fixed-point representation
fix32 b1FpFromInt(unsigned int integer) {
    return (long) integer << FP_SHIFT; // Shift left by the mantissa size
}

// Convert a fixed-point number back to a standard integer
int b1fpToInt(fix32 fixed) {
    return fixed >> FP_SHIFT; // Shift right by the mantissa size
}

int b1GetMantissa(fix32 fp) {
    return (int)(fp & 0xFFFF); // Mask off the upper byte to get only the mantissa
}

int b1FloorFix32(fix32 fp) {
    // Mask out the mantissa and create a new fixed-point number
    return b1fpToInt(fp & 0xFF0000);
}

int b1CeilFix32(fix32 fp) {
    if ((fp & (fix32) 0xFFFF) == 0) { // If there's no fractional part, return the number as it is
        return b1fpToInt(fp);
    }
    // Mask out the mantissa, add 1 to the integer part, and create a new fixed-point number
    return b1fpToInt((fp & 0xFF0000) + 0x10000);
}

#pragma code-name (pop);