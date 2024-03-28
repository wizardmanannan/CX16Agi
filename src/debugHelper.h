#ifndef _DEBUGHELPER_
#define _DEBUGHELPER_

#include "general.h"
#include "helpers.h"
#include "view.h"
#include "graphics.h"
#include "logic.h"
#include <time.h>


#pragma wrapped-call (push, trampoline, DEBUG_BANK)
void b5CheckMemory();
void b5DumpBitmap();
#pragma wrapped-call (pop)
#endif
