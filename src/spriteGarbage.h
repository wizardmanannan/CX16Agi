#ifndef _GARBAGE_H_
#define _GARBAGE_H_

#include "graphics.h"


#pragma wrapped-call (push, trampoline, SPRITE_GARBAGE_BANK)
extern void bCDeleteSpriteMemoryForViewTab(ViewTableMetadata* viewMetadata, byte currentLoop, View* localView, boolean inActiveOnly);
#pragma wrapped-call (pop)

#endif
