#include "general.h"
#include "fillStack.h"
#include "graphics.h"
#include "memoryManager.h"
#include <stdint.h>

#pragma wrapped-call (push, trampoline, FILL_BANK)
extern void b8AsmFloodFill(uint8_t x, uint8_t y);
#pragma wrapped-call (pop)