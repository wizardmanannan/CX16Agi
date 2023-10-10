#ifndef _LOADINGSCREEN_H_
#define _LOADINGSCREEN_H_
#include "irq.h"
#include "textLayer.h"

extern boolean loadingScreenDisplayed;

#pragma wrapped-call (push, trampoline, LOADING_SCREEN_CODE_BANK)
void b6DisplayLoadingScreen();
void b6DismissLoadingScreen();
#pragma wrapped-call (pop)
#endif