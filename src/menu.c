#include "menu.h"

int numOfMenus = 0;

typedef struct MENU
{
	char* text;                   /* menu item text */
	byte menuTextBank;
	int (*proc)(void);            /* callback function */
	int flags;                    /* flags about the menu state */
	void* dp;                     /* any data the menu might require */
	boolean enabled;
} MENU;

#pragma code-name (push, "BANKRAM05");

int b5MenuUpdate(byte menu)
{
	return 0;
}

int menuEvent0() { return b5MenuUpdate(0); }
int menuEvent1() { return b5MenuUpdate(1); }
int menuEvent2() { return b5MenuUpdate(2); }
int menuEvent3() { return b5MenuUpdate(3); }
int menuEvent4() { return b5MenuUpdate(4); }
int menuEvent5() { return b5MenuUpdate(5); }
int menuEvent6() { return b5MenuUpdate(6); }
int menuEvent7() { return b5MenuUpdate(7); }
int menuEvent8() { return b5MenuUpdate(8); }
int menuEvent9() { return b5MenuUpdate(9); }
int menuEvent10() { return b5MenuUpdate(10); }
int menuEvent11() { return b5MenuUpdate(11); }
int menuEvent12() { return b5MenuUpdate(12); }
int menuEvent13() { return b5MenuUpdate(13); }
int menuEvent14() { return b5MenuUpdate(14); }
int menuEvent15() { return b5MenuUpdate(15); }
int menuEvent16() { return b5MenuUpdate(16); }
int menuEvent17() { return b5MenuUpdate(17); }
int menuEvent18() { return b5MenuUpdate(18); }
int menuEvent19() { return b5MenuUpdate(19); }
int menuEvent20() { return b5MenuUpdate(20); }
int menuEvent21() { return b5MenuUpdate(21); }
int menuEvent22() { return b5MenuUpdate(22); }
int menuEvent23() { return b5MenuUpdate(23); }
int menuEvent24() { return b5MenuUpdate(24); }
int menuEvent25() { return b5MenuUpdate(25); }
int menuEvent26() { return b5MenuUpdate(26); }
int menuEvent27() { return b5MenuUpdate(27); }
int menuEvent28() { return b5MenuUpdate(28); }
int menuEvent29() { return b5MenuUpdate(29); }
int menuEvent30() { return b5MenuUpdate(30); }
int menuEvent31() { return b5MenuUpdate(31); }
int menuEvent32() { return b5MenuUpdate(32); }
int menuEvent33() { return b5MenuUpdate(33); }
int menuEvent34() { return b5MenuUpdate(34); }
int menuEvent35() { return b5MenuUpdate(35); }
int menuEvent36() { return b5MenuUpdate(36); }
int menuEvent37() { return b5MenuUpdate(37); }
int menuEvent38() { return b5MenuUpdate(38); }
int menuEvent39() { return b5MenuUpdate(39); }
int menuEvent40() { return b5MenuUpdate(40); }
int menuEvent41() { return b5MenuUpdate(41); }
int menuEvent42() { return b5MenuUpdate(42); }
int menuEvent43() { return b5MenuUpdate(43); }
int menuEvent44() { return b5MenuUpdate(44); }
int menuEvent45() { return b5MenuUpdate(45); }
int menuEvent46() { return b5MenuUpdate(46); }
int menuEvent47() { return b5MenuUpdate(47); }
int menuEvent48() { return b5MenuUpdate(48); }
int menuEvent49() { return b5MenuUpdate(49); }

int (*(menuFunctions[50]))() = {
	menuEvent0, menuEvent1, menuEvent2, menuEvent3, menuEvent4,
	menuEvent5, menuEvent6, menuEvent7, menuEvent8, menuEvent9,
	menuEvent10, menuEvent11, menuEvent12, menuEvent13, menuEvent14,
	menuEvent15, menuEvent16, menuEvent17, menuEvent18, menuEvent19,
	menuEvent20, menuEvent21, menuEvent22, menuEvent23, menuEvent24,
	menuEvent25, menuEvent26, menuEvent27, menuEvent28, menuEvent29,
	menuEvent30, menuEvent31, menuEvent32, menuEvent33, menuEvent34,
	menuEvent35, menuEvent36, menuEvent37, menuEvent38, menuEvent39,
	menuEvent40, menuEvent41, menuEvent42, menuEvent43, menuEvent44,
	menuEvent45, menuEvent46, menuEvent47, menuEvent48, menuEvent49
};

#pragma bss-name (push, "BANKRAM05")
MENU the_menu[MAX_MENUS];
MENU the_menuChildren[MAX_MENU_CHILDREN];
#pragma bss-name (pop)

extern char* getMessagePointer(byte logicFileNo, byte messageNo);

void b5MenuChildInit()
{
	int i;

	for (i = 0; i < MAX_MENU_CHILDREN; i++)
	{
		MENU menuChild;
		menuChild.dp = NULL;
		menuChild.flags = 0;
		menuChild.menuTextBank = 0;
		menuChild.proc = NULL;
		menuChild.text = NULL;
		the_menuChildren[i] = menuChild;
	}
}

void b5GetMenu(MENU* menu, byte menuNo)
{
	*menu = the_menu[menuNo];
}

void b5SetMenuChild(MENU* menu, byte menuNo)
{
	int i;

	for (i = 0; i < MAX_MENU_SIZE && the_menuChildren[menuNo * MAX_MENU_SIZE + i].text != NULL; i++);

	if (i < MAX_MENU_SIZE)
	{
#ifdef VERBOSE_MENU
		printf("-- Adding menu childen %p at position %d dp %p flags %d proc %p text %p \n", menu, menuNo * MAX_MENU_SIZE + i, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU
		the_menuChildren[menuNo * MAX_MENU_SIZE + i] = *menu;
	}
}

void b5SetMenu(byte messageNum)
{
	int messNum, startOffset;
	char* messData;

	MENU newMenu;
	LOGICFile currentLogicFile;

	if (numOfMenus == 0)
	{
		b5MenuChildInit();
	}

	b5GetLogicFile(&currentLogicFile, currentLog);

	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = 0;
	newMenu.menuTextBank = currentLogicFile.messageBank;
	/* Create new menu and allocate space for MAX_MENU_SIZE items */
	newMenu.text = getMessagePointer(currentLog, messNum - 1);

#ifdef VERBOSE_MENU
	printf("The result is %p \n", newMenu.text);
#endif // VERBOSE_MENU

	newMenu.proc = NULL;
	the_menu[numOfMenus] = newMenu;

	numOfMenus++;

	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = NULL;
	newMenu.text = NULL;
	newMenu.menuTextBank = 0;

	/* Mark end of menu */
		the_menu[numOfMenus] = newMenu;

	return;
}

void b5SetMenuItem(int messNum, int controllerNum)
{
	int i;
	MENU childMenu;
	LOGICFile currentLogicFile;
	EventType event;

	b5GetLogicFile(&currentLogicFile, currentLog);
	b7GetEvent(&event, controllerNum);

	if (event.type == NO_EVENT) {
		event.type = MENU_EVENT;
	}
	event.activated = 0;

	childMenu.text = getMessagePointer(currentLog, messNum - 1);
	childMenu.proc = menuFunctions[controllerNum];
	childMenu.menuTextBank = currentLogicFile.messageBank;

	b5SetMenuChild(&childMenu, numOfMenus - 1);


#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU

	return;
}

#pragma code-name (pop);


