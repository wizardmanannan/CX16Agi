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
    DISPLAY_TEXT = 4,
    IRQ_CMD_L0_L1_ONLY = 5,
    CLEAR = 6
} IRQ_COMMAND;

extern unsigned int vSyncCounter; //Updated by IRQ every 60ms

//Only to be called directly by bank 6
extern void b6SetAndWaitForIrqStateAsm(IRQ_COMMAND state);

#pragma wrapped-call (push, trampoline, IRQ_BANK)
extern void b6SetAndWaitForIrqState(IRQ_COMMAND state);
#pragma wrapped-call (pop)

#define VSYNC_BIT 1
#define VERA_ISR 0x9F27

#define REENABLE_INTERRUPTS() \
do { \
    asm("lda #%w", VSYNC_BIT); \
    asm("sta %w", VERA_ISR); \
    asm("cli"); \
  \
} while (0);

#endif
