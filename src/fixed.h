#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include "general.h"
#include "memoryManager.h"

// Define the fixed point number representation
typedef unsigned long fix32;

// Constants
#define FP_SHIFT 16 // 16 bits for the mantissa
#define FP_MASK 0xFFFF // Mask to extract the mantissa

// Function prototypes
#pragma wrapped-call (push, trampoline, FLOAT_BANK)
extern fix32 b12FpFromInt(unsigned int integer);
extern int b12FpToInt(fix32 fixed);
extern int b12GetMantissa(fix32 fp);
extern int b12FloorFix32(fix32 fp);
extern int b12CeilFix32(fix32 fp);
#pragma wrapped-call (pop)
#endif // FIXED_POINT_H
