
#ifndef _SHOWOBJ_H_
#define _SHOWOBJ_H_

#include "memoryManager.h"
#include "view.h"
#include "spriteAllocator.h"
#define MAX_SHOW_OBJ_SPLIT_SPRITES MAX_SPRITES_ROW_OR_COLUMN_SIZE
#pragma wrapped-call (push, trampoline, OBJ_SHOW_BANK)
byte b11AllocateShowObjSlots(SpriteAllocationSize width, SpriteAllocationSize height,
	byte requestedSlots, VeraSpriteAddress* spriteAddresses);
void b11ReleaseShowObjSlots(SpriteAllocationSize width, SpriteAllocationSize height,
	byte slotCount, VeraSpriteAddress* spriteAddresses);
void b11ShowObj(byte objNum);
#pragma wrapped-call (pop)

#endif
