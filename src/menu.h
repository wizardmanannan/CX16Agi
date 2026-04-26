#ifndef _MENU_H_
#define _MENU_H_

#include "general.h"
#include "memoryManager.h"
#include "logic.h"
#include "parser.h"

#define MAX_MENUS 10
#define MAX_MENU_CHILDREN 10
#define MENU_TEXT_BUFFER_SIZE 500

typedef struct MENU
{
	char* text;                   /* menu item text */
	int (*proc)(void);            /* callback function */
	int flags;                    /* flags about the menu state */
	void* dp;                     /* any data the menu might require */
	boolean enabled;
} MENU;

#pragma wrapped-call (push, trampoline, MENU_BANK)
void bFSetMenu(byte messageNo);
void bFSetMenuItem(int messageNum, int controllerNum);
#pragma wrapped-call  (pop);


#endif

