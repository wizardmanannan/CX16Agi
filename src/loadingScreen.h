#ifndef _LOADINGSCREEN_H_
#define _LOADINGSCREEN_H_
#include "irq.h"
#include "textLayer.h"

extern boolean loadingScreenDisplayed;

void b6DisplayLoadingScreen();
void b6DismissLoadingScreen();
#endif