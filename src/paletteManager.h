#ifndef _PALETTE_MANAGER_H_
#define _PALETTE_MANAGER_H_


#include "general.h"
#include "memoryManager.h"

#define BASE_SPRITE_ID 0
#define BASE_TEXT_ID 16

#define BASE_MANAGED_PALETTE 3 //First 3 entries 0, 1 and 2 are hard coded
#define NO_PALETTES 16
#define COLOURS_PER_PALETTE 16
#define NO_MANAGED_PALETTES NO_PALETTES - BASE_MANAGED_PALETTE
#define BYTES_PER_PALETTE_COLOUR 2


typedef enum
{
	Allocated = 0,
	AlreadyAllocated = 1,
	FailToAllocate = 2
} PaletteGetResult;

#pragma wrapped-call (push, trampoline, PALETTE_MANAGER_BANK)
byte bFInitPaletteManager();
byte bFGetPalette(int id, PaletteGetResult* result);
#pragma wrapped-call (pop)

#endif

