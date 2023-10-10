#ifndef _IRQ_H_
#define _IRQ_H_

#include "general.h"
#include "memoryManager.h"
#include "helpers.h"

typedef enum {
    DONT_CHANGE = 0,
    BLANK_SCREEN = 1,
    TEXT_ONLY = 2,
    NORMAL = 3,
    DISPLAY_TEXT = 4
} IRQ_COMMAND;

extern unsigned int vSyncCounter; //Updated by IRQ every 60ms

//Only to be called directly by bank 6
extern void b6SetAndWaitForIrqStateAsm(IRQ_COMMAND state);

#pragma wrapped-call (push, trampoline, IRQ_BANK)
extern void b6SetAndWaitForIrqState(IRQ_COMMAND state);
#pragma wrapped-call (pop)
#endif
