#include "menu.h"

//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP

int numOfMenus = 0;

#define MENU_WIDTH 40

boolean menuDirty;

#pragma bss-name (push, "BANKRAM0F")
MENU the_menu[MAX_MENUS];
MENU the_menuChildren[MAX_MENU_CHILDREN * MAX_MENUS];
MENU* bFFirstMenuChild[MAX_MENUS];
char menuTextBuffer[MENU_TEXT_BUFFER_SIZE];
byte bFMenuChildWidth[MAX_MENUS];
byte bFMenuChildShiftBack[MAX_MENUS];
byte bFMenuChildCount[MAX_MENUS];
char* nextMenuTextBufferAddr = menuTextBuffer;
boolean bFMenuAllowed;
boolean bFMenuShown;
byte bFMenuSelected;
byte bFMenuChildSelected;
byte bFChildMenuToClear = NO_MENU_TO_CLEAR;
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



extern char* getMessagePointer(byte logicFileNo, byte messageNo);

void bFCalculateFirstMenuChildAddress()
{
	byte i;

	for(i = 0; i < MAX_MENU_CHILDREN; i++)
	{
		bFFirstMenuChild[i] = the_menuChildren + i * MAX_MENU_CHILDREN;
	}
}

void bFInitMenuState()
{
	bFMenuAllowed = TRUE;
	bFMenuShown = FALSE;
	bFMenuSelected = FALSE;

	bFCalculateFirstMenuChildAddress();
	
	memset(bFMenuChildWidth, 0, MAX_MENUS);
	memset(bFMenuChildShiftBack, 0, MAX_MENUS);
	memset(bFMenuChildCount, 0, MAX_MENUS);

	bFMenuSelected = 0;
	bFMenuChildSelected = 0;
	menuDirty = TRUE; //Setting this to dirty initially is required so that the menu bar area is clear
}

void bFMenuChildInit()
{
	int i;

	asm("sei");
	for (i = 0; i < MAX_MENUS * MAX_MENU_CHILDREN; i++)
	{
		MENU menuChild;
		menuChild.controller = NO_ASSOCIATED;
		menuChild.text = NULL;
		the_menuChildren[i] = menuChild;
	}
	REENABLE_INTERRUPTS();
}

void bFAllowMenu(boolean allowed)
{
	asm("sei");
	bFMenuAllowed = allowed;
	REENABLE_INTERRUPTS();
}

void bFGoToNextMenu(signed char direction)
{
  asm("sei");
  menuDirty = TRUE;
  bFMenuSelected += direction;
  bFMenuChildSelected = 0;

  if(bFMenuSelected == 0xFF)
  {
	bFMenuSelected = numOfMenus - 1;
  }
  else if(bFMenuSelected == numOfMenus)
  {
	bFMenuSelected = 0;
  }
  REENABLE_INTERRUPTS();	
}

void bFGoToNextChildMenu(signed char direction)
{
  asm("sei");
  menuDirty = TRUE;
  bFMenuChildSelected += direction;

  if(bFMenuChildSelected == 0xFF)
  {
	bFMenuChildSelected = bFMenuChildCount[bFMenuSelected] - 1;
  }
  else if(bFMenuChildSelected >= bFMenuChildCount[bFMenuSelected])
  {
	bFMenuChildSelected = 0;
  }
  REENABLE_INTERRUPTS();	
}

void bFShowMenu(boolean shown)
{
	byte ch;
	MENU selectedMenu;

	asm("sei");
	bFMenuShown = TRUE;
	menuDirty = TRUE;
	REENABLE_INTERRUPTS();

	do 
	{
		GET_IN(ch);
		
		if(ch == KEY_LEFT)
		{
			bFGoToNextMenu(-1);
		}
		else if(ch == KEY_RIGHT)
		{
		   bFGoToNextMenu(1);
		}
		else if(ch == KEY_UP)
		{
		   bFGoToNextChildMenu(-1);
		}
		else if(ch == KEY_DOWN)
		{
		   bFGoToNextChildMenu(1);
		}
		else if(ch == KEY_ENTER)
		{
			selectedMenu = the_menuChildren[bFMenuSelected * MAX_MENU_CHILDREN + bFMenuChildSelected];

			if(selectedMenu.controller != NO_ASSOCIATED)
			{
				b1SetController(selectedMenu.controller);
			}
		}

	} while(ch != KEY_ESC && ch != KEY_ENTER);

	asm("sei");
	menuDirty = TRUE;
	bFMenuShown = FALSE;
	bFChildMenuToClear = bFMenuSelected;
	REENABLE_INTERRUPTS();
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
		bFMenuChildCount[menuNo]++;
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

	asm("sei");


	if (numOfMenus == 0)
	{
		bFMenuChildInit();
	}

	b5GetLogicFile(&currentLogicFile, currentLog);
	
	
	newMenu.text = bFStoreMessageInBuffer(&currentLogicFile, messNum);
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

void bFSetMenuChildShiftBack(byte menuNumber)
{
  unsigned int menuLocation = FIRST_MENU_CHILD;
  byte i;

  for(i = 0; i < menuNumber; i++)
  {
	menuLocation+= (strlen(the_menu[i].text) + 1) * 2;
  }

  if(menuLocation + bFMenuChildWidth[i] * 2 > MENU_BAR_MAX_CHILD_FIRST_ROW)
  {
	bFMenuChildShiftBack[i] = (menuLocation + bFMenuChildWidth[i] * 2 - MENU_BAR_MAX_CHILD_FIRST_ROW) + 2; //Shift by 1 title (2 spaces to create a gap)
  }
  else
  {
    bFMenuChildShiftBack[i] = 0;
  }

// printf("the address is %p, %u\n", bFMenuChildShiftBack, bFMenuChildShiftBack[i]);
// asm("stp");
}

void bFSetMenuItem(int messNum, int controllerNum)
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

	childMenu.text = bFStoreMessageInBuffer(&currentLogicFile, messNum);
	childMenu.controller = controllerNum;

	bFSetMenuChild(&childMenu, numOfMenus - 1);

	menuTextLength = strlen(childMenu.text);

	if(menuTextLength + 2 > bFMenuChildWidth[numOfMenus - 1])
	{
		bFMenuChildWidth[numOfMenus - 1] = menuTextLength + 2; //Plus 2 to take in account the border on the left and right
		bFSetMenuChildShiftBack(numOfMenus - 1);
	}

#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU

    REENABLE_INTERRUPTS();

	return;
}

#pragma code-name (pop);


