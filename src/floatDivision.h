#include "general.h"
#include "memoryManager.h"
#include "fixed.h"
#include "helpers.h"

void b6InitFloatDivision();

#pragma wrapped-call (push, trampoline, FLOAT_BANK)
extern fix32 b12Div(int numerator, int denominator);
#pragma wrapped-call (pop)
