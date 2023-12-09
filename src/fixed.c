#include "fixed.h"

#pragma code-name (push, "BANKRAM01")
// Convert a standard integer to fixed-point representation
fix32 fp_fromInt(unsigned int integer) {
    return (long) integer << FP_SHIFT; // Shift left by the mantissa size
}

// Convert a fixed-point number back to a standard integer
int fp_toInt(fix32 fixed) {
    return fixed >> FP_SHIFT; // Shift right by the mantissa size
}

int getMantissa(fix32 fp) {
    return (int)(fp & 0xFFFF); // Mask off the upper byte to get only the mantissa
}

int floor_fix_32(fix32 fp) {
    // Mask out the mantissa and create a new fixed-point number
    return fp_toInt(fp & 0xFF0000);
}

int ceil_fix_32(fix32 fp) {
    if ((fp & 0xFFFF) == 0) { // If there's no fractional part, return the number as it is
        return fp;
    }
    // Mask out the mantissa, add 1 to the integer part, and create a new fixed-point number
    return fp_toInt((fp & 0xFF0000) + 0x10000);
}

#pragma code-name (pop);