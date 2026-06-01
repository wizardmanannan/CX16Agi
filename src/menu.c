#include "menu.h"

//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP

int numOfMenus = 0;
boolean menuShown;

#define MENU_WIDTH 40

boolean menuDirty;
boolean menuAllowed;
#pragma bss-name (push, "BANKRAM0A")
MENU the_menu[MAX_MENUS];
MENU the_menuChildren[MAX_MENU_CHILDREN * MAX_MENUS];
MENU* bAFirstMenuChild[MAX_MENUS];
char menuTextBuffer[MENU_TEXT_BUFFER_SIZE];
byte bAMenuChildWidth[MAX_MENUS];
byte bAMenuChildShiftBack[MAX_MENUS];
byte bAMenuChildCount[MAX_MENUS];
byte bAEnabledMenuControllers[NO_CONTROLLERS];
char* nextMenuTextBufferAddr = menuTextBuffer;
byte bAMenuSelected;
byte bAMenuChildSelected;
byte bAChildMenuToClear = NO_MENU_TO_CLEAR;
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

#pragma code-name (push, "BANKRAM0A");

int bAMenuUpdate(byte menu)
{
	return 0;
}



extern char* getMessagePointer(byte logicFileNo, byte messageNo);

void bACalculateFirstMenuChildAddress()
{
	byte i;

	for(i = 0; i < MAX_MENUS; i++)
	{
		bAFirstMenuChild[i] = the_menuChildren + i * MAX_MENU_CHILDREN;
	}
}

void bAInitMenuState()
{
	menuAllowed = TRUE;
	menuShown = FALSE;
	bAMenuSelected = FALSE;

	bACalculateFirstMenuChildAddress();
	
	memset(bAMenuChildWidth, 0, MAX_MENUS);
	memset(bAMenuChildShiftBack, 0, MAX_MENUS);
	memset(bAMenuChildCount, 0, MAX_MENUS);
	memset(bAEnabledMenuControllers, TRUE, NO_CONTROLLERS);

	bAMenuSelected = 0;
	bAMenuChildSelected = 0;
	menuDirty = TRUE; //Setting this to dirty initially is required so that the menu bar area is clear
}

void bAMenuChildInit()
{
	int i;

	asm("sei");

	for (i = 0; i < MAX_MENUS * MAX_MENU_CHILDREN; i++)
	{
		the_menuChildren[i].controller = NO_ASSOCIATED;
		the_menuChildren[i].text = NULL;

	}
	REENABLE_INTERRUPTS();
}

void bAAllowMenu(boolean allowed)
{
	asm("sei");
	menuAllowed = allowed;
	REENABLE_INTERRUPTS();
}

void bAGoToNextMenu(signed char direction)
{
  asm("sei");
  menuDirty = TRUE;
  bAMenuSelected += direction;
  bAMenuChildSelected = 0;

  if(bAMenuSelected == 0xFF)
  {
	bAMenuSelected = numOfMenus - 1;
  }
  else if(bAMenuSelected == numOfMenus)
  {
	bAMenuSelected = 0;
  }
  REENABLE_INTERRUPTS();	
}

void bAGoToNextChildMenu(signed char direction)
{
  asm("sei");
  menuDirty = TRUE;
  bAMenuChildSelected += direction;

  if(bAMenuChildSelected == 0xFF)
  {
	bAMenuChildSelected = bAMenuChildCount[bAMenuSelected] - 1;
  }
  else if(bAMenuChildSelected >= bAMenuChildCount[bAMenuSelected])
  {
	bAMenuChildSelected = 0;
  }
  REENABLE_INTERRUPTS();	
}

void bAShowMenu(boolean shown)
{
	byte ch, controller;

	asm("sei");
	menuShown = TRUE;
	menuDirty = TRUE;
	REENABLE_INTERRUPTS();

	do 
	{
		GET_IN(ch);
		
		if(ch == KEY_LEFT)
		{
			bAGoToNextMenu(-1);
		}
		else if(ch == KEY_RIGHT)
		{
		   bAGoToNextMenu(1);
		}
		else if(ch == KEY_UP)
		{
		   bAGoToNextChildMenu(-1);
		}
		else if(ch == KEY_DOWN)
		{
		   bAGoToNextChildMenu(1);
		}
		else if(ch == KEY_ENTER)
		{
			controller = the_menuChildren[bAMenuSelected * MAX_MENU_CHILDREN + bAMenuChildSelected].controller;

			if(controller != NO_ASSOCIATED)
			{
				b1SetController(controller);
			}
		}

	} while(ch != KEY_ESC && ch != KEY_ENTER || ch == KEY_ENTER && !bAEnabledMenuControllers[controller]);

	asm("sei");
	menuDirty = TRUE;
	menuShown = FALSE;
	bAChildMenuToClear = bAMenuSelected;
	REENABLE_INTERRUPTS();
}

void bAGetMenu(MENU* menu, byte menuNo)
{
	*menu = the_menu[menuNo];
}

void bASetMenuChild(MENU* menu, byte menuNo)
{
	int i;
	
	for (i = 0; i < MAX_MENU_CHILDREN && the_menuChildren[menuNo * MAX_MENU_CHILDREN + i].text != NULL; i++);

	if (i < MAX_MENU_CHILDREN)
	{
#ifdef VERBOSE_MENU
		printf("-- Adding menu childen %p at position %d dp %p flags %d proc %p text %p \n", menu, menuNo * MAX_MENU_CHILDREN + i, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU
		the_menuChildren[menuNo * MAX_MENU_CHILDREN + i] = *menu;		
		bAMenuChildCount[menuNo]++;
	}
}

char* bAStoreMessageInBuffer(LOGICFile* currentLogicFile, byte messNum)
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

void bASetMenu(byte messNum)
{
	int startOffset;
	MENU newMenu;
	LOGICFile currentLogicFile;

	asm("sei");


	if (numOfMenus == 0)
	{
		bAMenuChildInit();
	}

	b5GetLogicFile(&currentLogicFile, currentLog);
	
	
	newMenu.text = bAStoreMessageInBuffer(&currentLogicFile, messNum);
	newMenu.controller = NO_ASSOCIATED;
	/* Create new menu and allocate space for MAX_MENU_SIZE items */

	
#ifdef VERBOSE_MENU
	printf("The result is %p \n", newMenu.text);
#endif // VERBOSE_MENU

	the_menu[numOfMenus] = newMenu;

	numOfMenus++;

	newMenu.text = NULL;

	/* Mark end of menu */
		the_menu[numOfMenus] = newMenu;

	REENABLE_INTERRUPTS();

	return;
}

void bASetMenuChildShiftBack(byte menuNumber)
{
  unsigned int menuLocation = FIRST_MENU_CHILD;
  byte i;

  for(i = 0; i < menuNumber; i++)
  {
	menuLocation+= (strlen(the_menu[i].text) + 1) * 2;
  }

  if(menuLocation + bAMenuChildWidth[i] * 2 > MENU_BAR_MAX_CHILD_FIRST_ROW)
  {
	bAMenuChildShiftBack[i] = (menuLocation + bAMenuChildWidth[i] * 2 - MENU_BAR_MAX_CHILD_FIRST_ROW) + 2; //Shift by 1 title (2 spaces to create a gap)
  }
  else
  {
    bAMenuChildShiftBack[i] = 0;
  }

// printf("the address is %p, %u\n", bAMenuChildShiftBack, bAMenuChildShiftBack[i]);
// asm("stp");
}

void bASetMenuItem(int messNum, int controllerNum)
{
	int i;
	MENU childMenu;
	LOGICFile currentLogicFile;
	EventType event;
	byte menuTextLength;
	
	asm("sei");

	b5GetLogicFile(&currentLogicFile, currentLog);

	if (event.type == NO_EVENT) {
		event.type = MENU_EVENT;
	}
	event.activated = 0;

	childMenu.text = bAStoreMessageInBuffer(&currentLogicFile, messNum);
	childMenu.controller = controllerNum;

	bASetMenuChild(&childMenu, numOfMenus - 1);

	menuTextLength = strlen(childMenu.text);

	if(menuTextLength + 2 > bAMenuChildWidth[numOfMenus - 1])
	{
		bAMenuChildWidth[numOfMenus - 1] = menuTextLength + 2; //Plus 2 to take in account the border on the left and right
		bASetMenuChildShiftBack(numOfMenus - 1);
	}

#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU

    REENABLE_INTERRUPTS();

	return;
}

void bASetMenuControllerEnabled(byte controllerNumber, boolean enabled)
{
	bAEnabledMenuControllers[controllerNumber] = enabled;
}

#pragma code-name (pop);


