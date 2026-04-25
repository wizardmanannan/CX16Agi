#ifndef _MENU_H_
#define _MENU_H_

#include "general.h"
#include "memoryManager.h"
#include "logic.h"
#include "parser.h"

#define MAX_MENUS 10
#define MAX_MENU_CHILDREN 10

#pragma wrapped-call (push, trampoline, MENU_BANK)
void bFSetMenu(byte messageNo);
void bFSetMenuItem(int messageNum, int controllerNum);
#pragma wrapped-call  (pop);


#endif

