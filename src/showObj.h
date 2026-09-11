
#ifndef _SHOWOBJ_H_
#define _SHOWOBJ_H_

#include "memoryManager.h"
#include "view.h"
#include "spriteAllocator.h"
#pragma wrapped-call (push, trampoline, OBJ_SHOW_BANK)
void b11ShowObj(byte objNum);
#pragma wrapped-call (pop)

#endif