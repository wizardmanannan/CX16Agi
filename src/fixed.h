#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include "general.h"

// Define the fixed point number representation
typedef long fix32;

// Constants
#define FP_SHIFT 16 // 16 bits for the mantissa
#define FP_MASK 0xFFFF // Mask to extract the mantissa

// Function prototypes
extern fix32 floatDivision(byte numerator, byte denominator);
extern fix32 fp_fromInt(unsigned int integer);
extern int fp_toInt(fix32 fixed);
extern int getMantissa(fix32 fp);
extern int floor_fix_32(fix32 fp);
extern int ceil_fix_32(fix32 fp);
#endif // FIXED_POINT_H
