#ifndef _IRQ_H_
#define _IRQ_H_

#include "general.h"

typedef enum {
    DONT_CHANGE = 0,
    BLANK_SCREEN = 1,
    LOAD_SCREEN = 2,
    NORMAL = 3,
    DISPLAY_TEXT = 4
} IRQ_COMMAND;

extern void b6SetAndWaitForIrqState(IRQ_COMMAND state);

#endif
