#ifndef _SPRITEMANAGER_H_
#define _SPRITE_MANAGER_H_

#include "general.h"
#include "memoryManager.h"
#include "helpers.h"

#define ALLOCATE_BLOCK_SIZE_32 1
#define ALLOCATOR_BLOCK_SIZE_64 2


#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_BANK)
void bEResetSpriteMemoryManager();
void bEInitSpriteMemoryManager();

typedef enum {
	SIZE_32 = 0,
	SIZE_64 = 1
} AllocationSize;

//Warning must pop return addresses of system stack after call. The number of return addresses will be 2 * number. 
//The first value popped off will be the middle byte of the first result, and the second the high byte of the first result and so on until the last result
//The low byte of the result is always zero and is therefore omitted from the stack
extern void bEAllocateSpriteMemoryBulk(AllocationSize size, byte number);
#pragma wrapped-call (pop)

#endif
