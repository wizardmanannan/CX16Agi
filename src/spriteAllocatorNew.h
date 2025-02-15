#include "memoryManager.h"




#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_NEW_BANK)
void bDInitSpriteMemoryManager();
void bDFindFreeVramBlock(byte width, byte height);
#pragma wrapped-call (pop)
