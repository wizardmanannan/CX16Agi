#include "menu.h"

//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP

int numOfMenus = 0;                 // Total number of top-level menus created
boolean menuShown;                  // Global flag: is the menu currently visible?

#define MENU_WIDTH 40               // Maximum width of the menu bar in tiles/characters

boolean menuDirty;                  // Flag to indicate menu needs redrawing
boolean menuAllowed;                // Global flag controlling if menu system is enabled

#pragma bss-name (push, "BANKRAM0A")
MENU the_menu[MAX_MENUS];                           // Array of top-level menu items
MENU the_menuChildren[MAX_MENU_CHILDREN * MAX_MENUS]; // Storage for all child menu items
MENU* bAFirstMenuChild[MAX_MENUS];                  // Pointers to first child of each menu
char menuTextBuffer[MENU_TEXT_BUFFER_SIZE];         // Buffer for storing menu text strings
byte bAMenuChildWidth[MAX_MENUS];                   // Width of each child menu (for borders)
byte bAMenuChildShiftBack[MAX_MENUS];               // Horizontal shift to prevent child menu overflow
byte bAMenuChildCount[MAX_MENUS];                   // Number of children per top-level menu
byte bAEnabledMenuControllers[NO_CONTROLLERS];      // Enable/disable state per controller
char* nextMenuTextBufferAddr = menuTextBuffer;      // Next free position in text buffer
byte bAMenuSelected;                                // Currently selected top-level menu
byte bAMenuChildSelected;                           // Currently selected child menu item
byte bAChildMenuToClear = NO_MENU_TO_CLEAR;         // Menu index to clear after closing
#pragma bss-name (pop)


#ifdef VERBOSE_MENU_DUMP
// ================================================================
// testMenus
// ================================================================
// Purpose: Debug function to dump all menu data to console.
//          Switches RAM banks and prints menu + child information.
// Input:   None
// Output:  None (prints to screen)
// ================================================================
void testMenus()
{
	byte previousRamBank = RAM_BANK;    // Save current RAM bank to restore later
	int i, j;
	MENU menuToPrint, childMenuToPrint;

	RAM_BANK = MENU_BANK;               // Switch to menu data bank

	for (i = 0; i < numOfMenus; i++)
	{
		menuToPrint = the_menu[i];

		RAM_BANK = menuToPrint.menuTextBank;    // Switch to the bank containing this menu's text
		
		asm("stp");                             // Breakpoint for debugging

		RAM_BANK = MENU_BANK;                   // Return to main menu bank

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

	RAM_BANK = previousRamBank;         // Restore original RAM bank
}
#endif // VERBOSE_MENU_DUMP


#pragma code-name (push, "BANKRAM0A");


extern char* getMessagePointer(byte logicFileNo, byte messageNo);

// ================================================================
// bACalculateFirstMenuChildAddress
// ================================================================
// Purpose: Initializes the array of pointers to the first child 
//          menu item for each top-level menu.
// Input:   None
// Output:  Populates bAFirstMenuChild array
// ================================================================
void bACalculateFirstMenuChildAddress()
{
	byte i;

	for(i = 0; i < MAX_MENUS; i++)
	{
		bAFirstMenuChild[i] = the_menuChildren + i * MAX_MENU_CHILDREN;
	}
}

// ================================================================
// bAInitMenuState
// ================================================================
// Purpose: Resets and initializes the entire menu system state.
// Input:   None
// Output:  All menu state variables set to defaults
// ================================================================
void bAInitMenuState()
{
	menuAllowed = TRUE;                 // Enable menu system by default
	menuShown = FALSE;                  // Menu starts hidden
	bAMenuSelected = 0;

	bACalculateFirstMenuChildAddress(); // Set up child menu pointers
	
	memset(bAMenuChildWidth, 0, MAX_MENUS);
	memset(bAMenuChildShiftBack, 0, MAX_MENUS);
	memset(bAMenuChildCount, 0, MAX_MENUS);
	memset(bAEnabledMenuControllers, TRUE, NO_CONTROLLERS);

	bAMenuSelected = 0;
	bAMenuChildSelected = 0;
	menuDirty = TRUE; // Setting this to dirty initially is required so that the menu bar area is clear
}

// ================================================================
// bAMenuChildInit
// ================================================================
// Purpose: Clears all child menu slots and marks them as unused.
// Input:   None
// Output:  All child menu entries reset
// ================================================================
void bAMenuChildInit()
{
	int i;

	asm("sei");                         // Disable interrupts during initialization

	for (i = 0; i < MAX_MENUS * MAX_MENU_CHILDREN; i++)
	{
		the_menuChildren[i].controller = NOT_ASSOCIATED;
		the_menuChildren[i].text = NULL;
	}
	REENABLE_INTERRUPTS();
}

// ================================================================
// bAAllowMenu
// ================================================================
// Purpose: Globally enables or disables the menu system.
// Input:   allowed - TRUE to allow, FALSE to disable
// Output:  Updates menuAllowed flag
// ================================================================
void bAAllowMenu(boolean allowed)
{
	asm("sei");
	menuAllowed = allowed;
	REENABLE_INTERRUPTS();
}

// ================================================================
// bAGoToNextMenu
// ================================================================
// Purpose: Changes the currently selected top-level menu (wraps around).
// Input:   direction - +1 for next, -1 for previous
// Output:  Updates bAMenuSelected and marks menu as dirty
// ================================================================
void bAGoToNextMenu(signed char direction)
{
  asm("sei");
  menuDirty = TRUE;
  bAMenuSelected += direction;
  bAMenuChildSelected = 0;

  if(bAMenuSelected == 0xFF)                    // Underflow (went before first)
  {
	bAMenuSelected = numOfMenus - 1;
  }
  else if(bAMenuSelected == numOfMenus)         // Overflow (went past last)
  {
	bAMenuSelected = 0;
  }
  REENABLE_INTERRUPTS();	
}

// ================================================================
// bAGoToNextChildMenu
// ================================================================
// Purpose: Changes the currently selected child menu item (wraps around).
// Input:   direction - +1 for next, -1 for previous
// Output:  Updates bAMenuChildSelected and marks menu as dirty
// ================================================================
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

// ================================================================
// bAShowMenu
// ================================================================
// Purpose: Main menu interaction loop. Handles keyboard input for 
//          navigation and selection while menu is shown.
// Input:   shown - not currently used (legacy parameter)
// Output:  Processes input until ESC or ENTER is pressed
// ================================================================
void bAShowMenu(boolean shown)
{
	byte ch, controller;

	asm("sei");
	menuShown = TRUE;
	menuDirty = TRUE;
	REENABLE_INTERRUPTS();

	do 
	{
		GET_IN(ch);                     // Get keyboard input
		
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

			if(controller != NOT_ASSOCIATED)
			{
				b1SetController(controller);    // Activate associated controller/action
			}
		}

	} while(ch != KEY_ESC && ch != KEY_ENTER || ch == KEY_ENTER && !bAEnabledMenuControllers[controller]);

	asm("sei");
	menuDirty = TRUE;
	menuShown = FALSE;
	bAChildMenuToClear = bAMenuSelected;    // Mark which child menu to clear on next redraw
	REENABLE_INTERRUPTS();
}

// ================================================================
// bAGetMenu
// ================================================================
// Purpose: Copies a top-level menu struct by index.
// Input:   menu    - pointer to destination MENU struct
//          menuNo  - index of menu to retrieve
// Output:  *menu is filled with copy of the_menu[menuNo]
// ================================================================
void bAGetMenu(MENU* menu, byte menuNo)
{
	*menu = the_menu[menuNo];
}

// ================================================================
// bASetMenuChild
// ================================================================
// Purpose: Adds a child menu item to a top-level menu.
// Input:   menu    - pointer to MENU struct to add
//          menuNo  - index of parent menu
// Output:  Adds child and increments child count
// ================================================================
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

// ================================================================
// bAStoreMessageInBuffer
// ================================================================
// Purpose: Copies a message from a logic file into the menu text buffer.
// Input:   currentLogicFile - pointer to current logic file
//          messNum          - message number to store
// Output:  Returns pointer to stored text in menuTextBuffer
// ================================================================
char* bAStoreMessageInBuffer(LOGICFile* currentLogicFile, byte messNum)
{
	char* messData,*result;
	byte messLength;


	messData = getMessagePointer(currentLog, messNum - 1);
	messLength = strLenBanked(messData, currentLogicFile->messageBank) + 1;
	
	memCpyBankedBetween((byte*)nextMenuTextBufferAddr, MENU_BANK, (byte*)messData, currentLogicFile->messageBank, messLength);

	result = nextMenuTextBufferAddr;

	nextMenuTextBufferAddr += messLength;   // Advance buffer pointer

	return result;
}

// ================================================================
// bASetMenu
// ================================================================
// Purpose: Creates a new top-level menu from a message.
// Input:   messNum - message number to use as menu title
// Output:  Adds new menu to the_menu array
// ================================================================
void bASetMenu(byte messNum)
{
	int startOffset;
	MENU newMenu;
	LOGICFile currentLogicFile;

	asm("sei");

	if (numOfMenus == 0)
	{
		bAMenuChildInit();              // Initialize child storage on first menu
	}

	b5GetLogicFile(&currentLogicFile, currentLog);
	
	
	newMenu.text = bAStoreMessageInBuffer(&currentLogicFile, messNum);
	newMenu.controller = NOT_ASSOCIATED;
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

// ================================================================
// bASetMenuChildShiftBack
// ================================================================
// Purpose: Calculates how much to shift a child menu left so it 
//          doesn't go off the right edge of the screen.
// Input:   menuNumber - index of the menu
// Output:  Sets bAMenuChildShiftBack[menuNumber]
// ================================================================
void bASetMenuChildShiftBack(byte menuNumber)
{
  unsigned int menuLocation = FIRST_MENU_CHILD;
  byte i;

  for(i = 0; i < menuNumber; i++)
  {
	menuLocation += (strlen(the_menu[i].text) + 1) * 2;   // 2 bytes per tile (tile + attr)
  }

  if(menuLocation + bAMenuChildWidth[i] * 2 > MENU_BAR_MAX_CHILD_FIRST_ROW)
  {
	bAMenuChildShiftBack[i] = (menuLocation + bAMenuChildWidth[i] * 2 - MENU_BAR_MAX_CHILD_FIRST_ROW) + 2;
  }
  else
  {
    bAMenuChildShiftBack[i] = 0;
  }
}

// ================================================================
// bASetMenuItem
// ================================================================
// Purpose: Adds a child menu item with associated controller/action.
// Input:   messNum       - message number for item text
//          controllerNum - controller ID to trigger on selection
// Output:  Adds child and updates width/shift if needed
// ================================================================
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
		bAMenuChildWidth[numOfMenus - 1] = menuTextLength + 2; // Plus 2 to account for left and right borders
		bASetMenuChildShiftBack(numOfMenus - 1);
	}

#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU

    REENABLE_INTERRUPTS();

	return;
}

// ================================================================
// bASetMenuControllerEnabled
// ================================================================
// Purpose: Enables or disables a controller/action in the menu.
// Input:   controllerNumber - controller ID
//          enabled          - TRUE/FALSE
// Output:  Updates enable array
// ================================================================
void bASetMenuControllerEnabled(byte controllerNumber, boolean enabled)
{
	bAEnabledMenuControllers[controllerNumber] = enabled;
}

#pragma code-name (pop);