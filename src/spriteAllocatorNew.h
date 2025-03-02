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

#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_NEW_BANK)
void bDInitSpriteMemoryManager();
unsigned long bDFindFreeVramBlock(SpriteAllocationSize width, SpriteAllocationSize height);
extern void bDDeleteAllocation(VeraSpriteAddress address, SpriteAllocationSize width, SpriteAllocationSize height);
#pragma wrapped-call (pop)
