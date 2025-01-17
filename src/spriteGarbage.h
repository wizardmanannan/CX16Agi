#ifndef _GARBAGE_H_
#define _GARBAGE_H_

#include "graphics.h"
#include "zeroPointer.h"

#pragma wrapped-call (push, trampoline, SPRITE_GARBAGE_BANK)
extern void bAGarbageCollectorInit();
extern void bARunSpriteGarbageCollectorAll();
#pragma wrapped-call (pop)

extern void runIncrementalGarbageCollector();
extern void runSpriteGarbageCollector(byte start, byte end);

#endif
