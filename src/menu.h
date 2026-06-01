#ifndef _MENU_H_
#define _MENU_H_

#include "general.h"
#include "memoryManager.h"
#include "logic.h"
#include "parser.h"
#include "controllers.h"

/* ================================================================
   Menu System Configuration Constants
   ================================================================ */

#define MAX_MENUS 10                    // Maximum number of top-level menus supported
#define MAX_MENU_CHILDREN 15            // Maximum number of child items per top-level menu
#define MENU_TEXT_BUFFER_SIZE 500       // Total memory reserved for storing all menu text strings
#define NO_MENU_TO_CLEAR 0xFF           // Special value indicating no child menu needs clearing

#define MENU_BAR_WIDTH 40               // Width of the menu bar in tiles (matches screen width)
#define MENU_BAR_LOCATION 0xDA00        // VRAM address where the menu bar starts
#define MENU_BAR_END MENU_BAR_LOCATION + (MENU_BAR_WIDTH * 2)  // End address of menu bar (2 bytes per tile)
#define MENU_BAR_MAX_CHILD_FIRST_ROW (MENU_BAR_END + TILE_LAYER_WIDTH * 2) // First safe row for child menus
#define FIRST_MENU_CHILD MENU_BAR_LOCATION + TILE_LAYER_WIDTH * 2  // VRAM position directly below menu bar


/* ================================================================
   Menu Data Structure
   ================================================================ */

// Represents a single menu item (used for both top-level menus and child items)
typedef struct MENU
{
	char* text;      /* Pointer to the menu item text string (stored in menuTextBuffer) */
	byte controller; /* Associated controller/action ID (NO_ASSOCIATED if none) */
} MENU;


/* ================================================================
   Menu Management Functions (Banked)
   ================================================================ */

#pragma wrapped-call (push, trampoline, MENU_BANK)

// ================================================================
// bASetMenu
// ================================================================
// Purpose: Creates a new top-level menu using a message from the 
//          current logic file as the title.
// Input:   messageNo - message number to use as menu title
// Output:  Adds menu to the_menu array and increments numOfMenus
// ================================================================
void bASetMenu(byte messageNo);

// ================================================================
// bASetMenuItem
// ================================================================
// Purpose: Adds a child menu item to the most recently created 
//          top-level menu, with optional controller binding.
// Input:   messageNum   - message number for the child item text
//          controllerNum - controller ID to trigger when selected
// Output:  Adds child to the_menuChildren and updates width/shift
// ================================================================
void bASetMenuItem(int messageNum, int controllerNum);

// ================================================================
// bAAllowMenu
// ================================================================
// Purpose: Globally enables or disables the entire menu system.
// Input:   allowed - TRUE to enable, FALSE to disable
// Output:  Updates global menuAllowed flag
// ================================================================
void bAAllowMenu(boolean allowed);

// ================================================================
// bAShowMenu
// ================================================================
// Purpose: Displays the menu and runs the interactive input loop.
//          Handles navigation (arrow keys) and selection (ENTER).
// Input:   shown - legacy parameter (currently not used internally)
// Output:  Processes input until ESC or valid ENTER is pressed
// ================================================================
void bAShowMenu(boolean shown);

// ================================================================
// bASetMenuControllerEnabled
// ================================================================
// Purpose: Enables or disables a specific controller/action 
//          in the menu system (affects selection highlighting).
// Input:   controllerNumber - controller ID
//          enabled          - TRUE to enable, FALSE to disable
// Output:  Updates bAEnabledMenuControllers array
// ================================================================
void bASetMenuControllerEnabled(byte controllerNumber, boolean enabled);

#pragma wrapped-call (pop);


#endif