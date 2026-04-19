#ifndef _MENU_H_
#define _MENU_H_

#include "general.h"
#include "memoryManager.h"
#include "logic.h"
#include "parser.h"

#define MAX_MENUS 10
#define MAX_MENU_CHILDREN 15

#pragma wrapped-call (push, trampoline, MENU_BANK)
void b5SetMenu(byte messageNo);
void b5SetMenuItem(int messageNum, int controllerNum);
#pragma wrapped-call  (pop);


#endif

