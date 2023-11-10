#ifndef _SPRITEMANAGER_H_
#define _SPRITE_MANAGER_H_

#include "general.h"
#include "memoryManager.h"

#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_BANK)
void bEResetSpriteMemoryManager();
void bEInitSpriteMemoryManager();
#pragma wrapped-call (pop)

#endif
