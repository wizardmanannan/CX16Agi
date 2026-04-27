#include "menu.h"

//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP

int numOfMenus = 0;

#pragma bss-name (push, "BANKRAM0F")
MENU the_menu[MAX_MENUS];
MENU the_menuChildren[MAX_MENU_CHILDREN * MAX_MENUS];
char menuTextBuffer[MENU_TEXT_BUFFER_SIZE];
char* nextMenuTextBufferAddr = menuTextBuffer;
boolean bFMenuAllowed;
boolean bFMenuShown;
#pragma bss-name (pop)

#ifdef VERBOSE_MENU_DUMP
void testMenus()
{
	byte previousRamBank = RAM_BANK;
	int i, j;
	MENU menuToPrint, childMenuToPrint;

	RAM_BANK = MENU_BANK;

	for (i = 0; i < numOfMenus; i++)
	{
		menuToPrint = the_menu[i];

		RAM_BANK = menuToPrint.menuTextBank;
		
		asm("stp");


		RAM_BANK = MENU_BANK;

		for (j = 0; the_menuChildren[i * MAX_MENU_CHILDREN + j].text != NULL; j++)
		{
			childMenuToPrint = the_menuChildren[i * MAX_MENU_CHILDREN + j];

			RAM_BANK = childMenuToPrint.menuTextBank;
			printf("menu children addr %p %s\n",&the_menuChildren, childMenuToPrint.text);

			RAM_BANK = MENU_BANK;
		}
	}

	asm("stp");
	asm("nop");

	RAM_BANK = previousRamBank;
}
#endif // VERBOSE_MENU

#pragma code-name (push, "BANKRAM0F");

int bFMenuUpdate(byte menu)
{
	return 0;
}

int menuEvent0() { return bFMenuUpdate(0); }
int menuEvent1() { return bFMenuUpdate(1); }
int menuEvent2() { return bFMenuUpdate(2); }
int menuEvent3() { return bFMenuUpdate(3); }
int menuEvent4() { return bFMenuUpdate(4); }
int menuEvent5() { return bFMenuUpdate(5); }
int menuEvent6() { return bFMenuUpdate(6); }
int menuEvent7() { return bFMenuUpdate(7); }
int menuEvent8() { return bFMenuUpdate(8); }
int menuEvent9() { return bFMenuUpdate(9); }
int menuEvent10() { return bFMenuUpdate(10); }
int menuEvent11() { return bFMenuUpdate(11); }
int menuEvent12() { return bFMenuUpdate(12); }
int menuEvent13() { return bFMenuUpdate(13); }
int menuEvent14() { return bFMenuUpdate(14); }
int menuEvent15() { return bFMenuUpdate(15); }
int menuEvent16() { return bFMenuUpdate(16); }
int menuEvent17() { return bFMenuUpdate(17); }
int menuEvent18() { return bFMenuUpdate(18); }
int menuEvent19() { return bFMenuUpdate(19); }
int menuEvent20() { return bFMenuUpdate(20); }
int menuEvent21() { return bFMenuUpdate(21); }
int menuEvent22() { return bFMenuUpdate(22); }
int menuEvent23() { return bFMenuUpdate(23); }
int menuEvent24() { return bFMenuUpdate(24); }
int menuEvent25() { return bFMenuUpdate(25); }
int menuEvent26() { return bFMenuUpdate(26); }
int menuEvent27() { return bFMenuUpdate(27); }
int menuEvent28() { return bFMenuUpdate(28); }
int menuEvent29() { return bFMenuUpdate(29); }
int menuEvent30() { return bFMenuUpdate(30); }
int menuEvent31() { return bFMenuUpdate(31); }
int menuEvent32() { return bFMenuUpdate(32); }
int menuEvent33() { return bFMenuUpdate(33); }
int menuEvent34() { return bFMenuUpdate(34); }
int menuEvent35() { return bFMenuUpdate(35); }
int menuEvent36() { return bFMenuUpdate(36); }
int menuEvent37() { return bFMenuUpdate(37); }
int menuEvent38() { return bFMenuUpdate(38); }
int menuEvent39() { return bFMenuUpdate(39); }
int menuEvent40() { return bFMenuUpdate(40); }
int menuEvent41() { return bFMenuUpdate(41); }
int menuEvent42() { return bFMenuUpdate(42); }
int menuEvent43() { return bFMenuUpdate(43); }
int menuEvent44() { return bFMenuUpdate(44); }
int menuEvent45() { return bFMenuUpdate(45); }
int menuEvent46() { return bFMenuUpdate(46); }
int menuEvent47() { return bFMenuUpdate(47); }
int menuEvent48() { return bFMenuUpdate(48); }
int menuEvent49() { return bFMenuUpdate(49); }

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

extern char* getMessagePointer(byte logicFileNo, byte messageNo);

void bFMenuChildInit()
{
	int i;

	for (i = 0; i < MAX_MENUS * MAX_MENU_CHILDREN; i++)
	{
		MENU menuChild;
		menuChild.dp = NULL;
		menuChild.flags = 0;
		menuChild.proc = NULL;
		menuChild.text = NULL;
		the_menuChildren[i] = menuChild;
	}
}

void bFAllowMenu(boolean allowed)
{
	bFMenuAllowed = allowed;
}

void bFShowMenu(boolean shown)
{
	asm("stp");
	bFMenuShown = shown;
}

void bFGetMenu(MENU* menu, byte menuNo)
{
	*menu = the_menu[menuNo];
}

void bFSetMenuChild(MENU* menu, byte menuNo)
{
	int i;

	for (i = 0; i < MAX_MENU_CHILDREN && the_menuChildren[menuNo * MAX_MENU_CHILDREN + i].text != NULL; i++);

	if (i < MAX_MENU_CHILDREN)
	{
#ifdef VERBOSE_MENU
		printf("-- Adding menu childen %p at position %d dp %p flags %d proc %p text %p \n", menu, menuNo * MAX_MENU_CHILDREN + i, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU
		the_menuChildren[menuNo * MAX_MENU_CHILDREN + i] = *menu;
	}
}

char* bFStoreMessageInBuffer(LOGICFile* currentLogicFile, byte messNum)
{
	char* messData,*result;
	byte messLength;


	messData = getMessagePointer(currentLog, messNum - 1);
	messLength = strLenBanked(messData, currentLogicFile->messageBank) + 1;
	
	memCpyBankedBetween((byte*)nextMenuTextBufferAddr, MENU_BANK, (byte*)messData, currentLogicFile->messageBank, messLength);

	result = nextMenuTextBufferAddr;

	nextMenuTextBufferAddr += messLength;

	return result;
}

void bFSetMenu(byte messNum)
{
	int startOffset;


	MENU newMenu;
	LOGICFile currentLogicFile;

	if (numOfMenus == 0)
	{
		bFMenuChildInit();
	}

	b5GetLogicFile(&currentLogicFile, currentLog);
	
	
	newMenu.text = bFStoreMessageInBuffer(&currentLogicFile, messNum);
	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = 0;
	/* Create new menu and allocate space for MAX_MENU_SIZE items */

	
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

	/* Mark end of menu */
		the_menu[numOfMenus] = newMenu;

	return;
}

void bFSetMenuItem(int messNum, int controllerNum)
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

	childMenu.text = bFStoreMessageInBuffer(&currentLogicFile, messNum);
	childMenu.proc = menuFunctions[controllerNum];

	bFSetMenuChild(&childMenu, numOfMenus - 1);


#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU

	return;
}

#pragma code-name (pop);


