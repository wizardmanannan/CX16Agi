#ifndef _MENU_H_
#define _MENU_H_

#include "general.h"
#include "memoryManager.h"
#include "logic.h"
#include "parser.h"

#define MAX_MENUS 10
#define MAX_MENU_CHILDREN 10
#define MENU_TEXT_BUFFER_SIZE 500
#define NO_MENU_TO_CLEAR 0xFF

#define MENU_BAR_WIDTH 40
#define MENU_BAR_LOCATION 0xDA00
#define MENU_BAR_END MENU_BAR_LOCATION + (MENU_BAR_WIDTH * 2)
#define MENU_BAR_MAX_CHILD_FIRST_ROW (MENU_BAR_END + TILE_LAYER_WIDTH * 2)
#define FIRST_MENU_CHILD MENU_BAR_LOCATION + TILE_LAYER_WIDTH * 2

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
void bFAllowMenu(boolean allowed);
void bFShowMenu(boolean shown);
#pragma wrapped-call  (pop);


#endif

