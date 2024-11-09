#ifndef _SPRITEMANAGER_H_
#define _SPRITE_MANAGER_H_

#include "general.h"
#include "memoryManager.h"
#include "helpers.h"
#include "graphics.h"

#define ALLOCATE_BLOCK_SIZE_32 1
#define ALLOCATOR_BLOCK_SIZE_64 2


#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_BANK)
void bEResetSpriteMemoryManager();
void bEInitSpriteMemoryManager();
void bEDeleteFromAllocationTable(VeraSpriteAddress addressToDelete);

typedef enum {
	SIZE_8 = 1,
	SIZE_16 = 2,
	SIZE_32 = 0, //Out of order on purpose so as not to break memory allocator. Will change this again
	SIZE_64 = 3
} AllocationSize;

typedef enum {
	MAX_8_WIDTH_OR_HEIGHT = 8,
	MAX_16_WIDTH_OR_HEIGHT = 16,
	MAX_32_WIDTH_OR_HEIGHT = 32,
	MAX_64_WIDTH_OR_HEIGHT = 64
} SprSizes;


//Warning must pop return addresses of system stack after call. The number of return addresses will be 2 * number. 
//The first value popped off will be the middle byte of the first result, and the second the high byte of the first result and so on until the last result
//The low byte of the result is always zero and is therefore omitted from the stack
extern boolean bEAllocateSpriteMemoryBulk(AllocationSize size, byte number);
#pragma wrapped-call (pop)

#endif
