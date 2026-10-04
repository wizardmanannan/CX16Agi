#ifndef _SPRITEALLOCATOR_H_
#define _SPRITEALLOCATOR_H_

#include "memoryManager.h"
#include "graphics.h"

#define VRAM_START 0xEA00 //Base VRAM address
#define VRAM_SIZE 69120 //Total available VRAM for sprites
#define BLOCK_SIZE 32 //8x8 block size(64 bytes)

#define TOTAL_REAL_BLOCKS (VRAM_SIZE / BLOCK_SIZE)
#define TOTAL_BLOCKS 2304
#define SPRITE_ALLOC_TERMINATOR 0xFF
#define FAST_LOOKUP_SIZE 130

typedef enum {
	SPR_SIZE_8 = 8,
	SPR_SIZE_16 = 16,
	SPR_SIZE_32 = 32, //Out of order on purpose so as not to break memory allocator. Will change this again
	SPR_SIZE_64 = 64
} SpriteAllocationSize;

#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_BANK)
void bDInitSpriteMemoryManager();
VeraSpriteAddress bDFindFreeVramBlock(SpriteAllocationSize width, SpriteAllocationSize height);
extern void bDDeleteAllocation(VeraSpriteAddress address, SpriteAllocationSize width, SpriteAllocationSize height);
extern void bDResetSpriteMemoryManager();
#define MAX_BULK_ALLOCATED_SIZE 256
#define MAX_BULK_ALLOCATED_COUNT (MAX_BULK_ALLOCATED_SIZE / sizeof(VeraSpriteAddress))

#pragma wrapped-call (pop)

/* Returns TRUE only when all results are allocated. Zero, over-capacity, and
 * partially failing requests return FALSE; the result buffer is valid only on TRUE. */
extern boolean bEAllocateSpriteMemoryBulk(SpriteAllocationSize width, SpriteAllocationSize height, byte number);

#endif
