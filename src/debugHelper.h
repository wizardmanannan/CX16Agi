#ifndef _DEBUGHELPER_
#define _DEBUGHELPER_

#include "general.h"
#include "helpers.h"
#include "view.h"
#include "graphics.h"
#include "logic.h"
#include <time.h>
#include <cbm.h>

#pragma wrapped-call (push, trampoline, DEBUG_INIT_BANK)
void b5DumpBitmap();
void b5InitializeDebugging();
#pragma wrapped-call (pop)
#endif