#ifndef _GARBAGE_H_
#define _GARBAGE_H_

#include "graphics.h"

#pragma wrapped-call (push, trampoline, SPRITE_GARBAGE_BANK)
extern void bCDeallocSpriteMemory(VeraSpriteAddress address);
#pragma wrapped-call (pop)

#endif
