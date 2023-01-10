/***************************************************************************
** commands.c
**
** These functions are used to execute the LOGIC files. The function
** executeLogic() is called by the main AGI interpret function in order to
** execute LOGIC.0 for each cycle of interpretation. Most of the other
** functions correspond almost exactly with the AGI equivalents.
**
** (c) 1997, 1998 Lance Ewing - Initial code (30 Aug 97)
**                              Additions (10 Jan 98) (4-5 Jul 98)
***************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <cx16.h>

#include "commands.h"
#include "general.h"
#include "logic.h"
#include "memoryManager.h"
#include "view.h"
#include "stub.h"
#include "helpers.h"

#define HIGHEST_BANK1_FUNC 36
#define HIGHEST_BANK2_FUNC 91
#define HIGHEST_BANK3_FUNC 138
#define HIGHEST_BANK4_FUNC 181


#define  PLAYER_CONTROL   0
#define  PROGRAM_CONTROL  1
#define CODE_WINDOW_SIZE 10
//#define VERBOSE_STRING_CHECK
#define VERBOSE_LOGIC_EXEC
#define VERBOSE_SCRIPT_START
//#define VERBOSE_PRINT_COUNTER;
//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP
//#define VERBOSE_MESSAGE_TEXT

//#define  DEBUG

extern byte* var;
extern boolean* flag;
extern char string[12][40];
extern int newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern byte horizon;
extern int controlMode;

//extern int picFNum;   // Just a debug variable. Delete at some stage!!

int currentLog, agi_bg = 1, agi_fg = 16;
extern char cursorChar;
boolean oldQuit = FALSE;


int numOfMenus = 0;
MENU* the_menu = (MENU*)&BANK_RAM[MENU_START];
MENU* the_menuChildren = (MENU*)&BANK_RAM[MENU_CHILD_START];

long opCounter = 0;
int printCounter = 1;
byte* _codeWindowAddress;

void executeLogic(int logNum);

//TEMP Should Be In Events
typedef struct {
	byte type;     /* either key or menu item */
	byte eventID;  /* either scancode or menu item ID */
	byte asciiValue;
	byte scanCodeValue;
	boolean activated;
} eventType;

eventType events[256];

//

//Temp Should Be In Objects
typedef struct {
	int roomNum;
	char* name;
} objectType;

objectType* objects;

//

int getNum(char* inputString, int* i, int inputStringBank)
{
	/* char strPos = 0;
	 char* tempString = &BANK_RAM[GET_NUM_TEMP_START];
	 byte previousRamBank = RAM_BANK;

	 RAM_BANK = inputStringBank;

	 while (inputString[*i] == ' ') { *i++; }
	 if ((inputString[*i] < '0') && (inputString[*i] > '9')) return 0;
	 while ((inputString[*i] >= '0') && (inputString[*i] <= '9')) {
		 tempString[strPos++] = inputString[(*i)++];
	 }
	 tempString[strPos] = 0;

	 (*i)--;

	 RAM_BANK = previousRamBank;
	 return (atoi(tempString));*/

	return 0;
}

void menuChildInit()
{
	int i;
	int previousRamBank = RAM_BANK;

	RAM_BANK = MENU_BANK;

	for (i = 0; i < MAX_MENU_SIZE * MAX_MENU_SIZE; i++)
	{
		MENU menuChild;
		menuChild.dp = NULL;
		menuChild.flags = 0;
		menuChild.menuTextBank = 0;
		menuChild.proc = NULL;
		menuChild.text = NULL;
		the_menuChildren[i] = menuChild;
	}

	RAM_BANK = previousRamBank;
}

void getMenu(MENU* menu, byte menuNo)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = MENU_BANK;
	*menu = the_menu[menuNo];

	RAM_BANK = previousBank;
}

void setMenu(MENU* menu, byte menuNo)
{
	byte previousBank = RAM_BANK;

#ifdef VERBOSE_MENU
	printf("-- Adding menu %p at position %d dp %p flags %d proc %p address %p \n", menu, menuNo, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU

	RAM_BANK = MENU_BANK;
	the_menu[menuNo] = *menu;

	RAM_BANK = previousBank;
}

void setMenuChild(MENU* menu, byte menuNo)
{
	int i;
	byte previousBank = RAM_BANK;

	RAM_BANK = MENU_BANK;

	for (i = 0; i < MAX_MENU_SIZE && the_menuChildren[menuNo * MAX_MENU_SIZE + i].text != NULL; i++);

	if (i < MAX_MENU_SIZE)
	{
#ifdef VERBOSE_MENU
		printf("-- Adding menu childen %p at position %d dp %p flags %d proc %p text %p \n", menu, menuNo * MAX_MENU_SIZE + i, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU
		the_menuChildren[menuNo * MAX_MENU_SIZE + i] = *menu;
	}

	RAM_BANK = previousBank;
}

//void getMenuChild(MENU* menu, byte menuNo, byte menuChildNo)
//{
//    byte previousBank = RAM_BANK;
//
//    RAM_BANK = MENU_BANK;
//
//#ifdef VERBOSE_MENU
//    printf("-- Get menu childen %p at position %d child %p dp %p flags %d proc %p text %p \n", menu, menuNo * MAX_MENU_SIZE + i, menu->child, menu->dp, menu->flags, menu->proc, menu->text);
//#endif // VERBOSE_MENU
//
//    *menu = the_menuChildren[menuChildNo * MAX_MENU_SIZE + menuChildNo];
//
//    RAM_BANK = previousBank;
//}

char* getMessagePointer(byte logicFileNo, byte messageNo)
{
	byte previousBank = RAM_BANK;
	char* result;
	int i;

	LOGICFile logicFile;
	getLogicFile(&logicFile, logicFileNo);

	RAM_BANK = logicFile.messageBank;

	result = (char*)logicFile.messages[messageNo];

	for (i = 1; i < strlen(result) && *result == ' '; i++)
	{
		result = (char*)logicFile.messages[messageNo + i];
	}

#ifdef VERBOSE_MESSAGE_TEXT
	printf("The menu is %s \n", result);
#endif //VERBOSE_MENU


	RAM_BANK = previousBank;

	return result;
}

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
		printf("- %s dp %dp flags %d proc %d \n", menuToPrint.text, menuToPrint.dp, menuToPrint.flags, menuToPrint.proc);
		RAM_BANK = MENU_BANK;

		for (j = 0; the_menuChildren[i * MAX_MENU_SIZE + j].text != NULL; j++)
		{
			childMenuToPrint = the_menuChildren[i * MAX_MENU_SIZE + j];

			RAM_BANK = childMenuToPrint.menuTextBank;
			printf("    -- %s %p dp %d flags %d proc %p \n", childMenuToPrint.text, childMenuToPrint.text, childMenuToPrint.dp, childMenuToPrint.flags, childMenuToPrint.proc);

			RAM_BANK = MENU_BANK;
		}
	}

	printf("\n\n_____________________________________________________\n");
	RAM_BANK = previousRamBank;
}
#endif // VERBOSE_MENU

#pragma code-name (push, "BANKRAM01");
/****************************************************************************
** addLogLine
****************************************************************************/
void b1AddLogLine(char* message)
{
	FILE* fp;

	if ((fp = fopen("log.txt", "a")) == NULL) {
#ifdef VERBOSE_LOGIC_EXEC
		fprintf(stderr, "Error opening log file.");
#endif // VERBOSE
		return;
	}

	fprintf(fp, "%s\n", message);

	fclose(fp);
}

/***************************************************************************
** //lprintf
**
** This function behaves exactly the same as printf except that it writes
** the output to the log file using addLogLine().
***************************************************************************/
int b1Lprintf(char* fmt, ...)
{
	/*va_list args;
	char tempStr[10];

	va_start(args, fmt);
	vsprintf(tempStr, fmt, args);
	va_end(args);

	addLogLine(tempStr);*/

	//printf("Implement lprinf");

	return 0;
}

/* TEST COMMANDS */

boolean b1Equaln() // 2, 0x80 
{
	int varVal, value, variable;

	variable = *_codeWindowAddress++;
	varVal = var[variable];
	value = *_codeWindowAddress++;

	return (varVal == value);
}

boolean b1Equalv() // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*_codeWindowAddress++];
	varVal2 = var[*_codeWindowAddress++];
	return (varVal1 == varVal2);
}

boolean b1Lessn() // 2, 0x80 
{
	int varVal, value;

	varVal = var[*_codeWindowAddress++];
	value = *_codeWindowAddress++;
	return (varVal < value);
}

boolean b1Lessv() // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*_codeWindowAddress++];
	varVal2 = var[*_codeWindowAddress++];
	return (varVal1 < varVal2);
}

boolean b1Greatern() // 2, 0x80 
{
	int varVal, value;

	varVal = var[*_codeWindowAddress++];
	value = *_codeWindowAddress++;

	printf("varVal %d value %d \n", varVal, value);

	return (varVal > value);
}

boolean b1Greaterv() // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*_codeWindowAddress++];
	varVal2 = var[*_codeWindowAddress++];
	return (varVal1 > varVal2);
}

boolean b1Isset() // 1, 0x00 
{
	int flagNo = *_codeWindowAddress++;
	
	printf("Checking whether %d is set and it is %d \n", flagNo, flag[flagNo]);
	
	return (flag[flagNo]);
}

boolean b1Issetv() // 1, 0x80 
{
	return (flag[var[*_codeWindowAddress++]]);
}

boolean b1Has() // 1, 0x00 
{
	return (objects[*_codeWindowAddress++].roomNum == 255);
}

boolean b1Obj_in_room() // 2, 0x40 
{
	int objNum, varNum;

	objNum = *_codeWindowAddress++;
	varNum = var[*_codeWindowAddress++];
	return (objects[objNum].roomNum == varNum);
}

boolean b1Posn() // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;

	objNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, objNum);

	x1 = *_codeWindowAddress++;
	y1 = *_codeWindowAddress++;
	x2 = *_codeWindowAddress++;
	y2 = *_codeWindowAddress++;

	return ((localViewtab.xPos >= x1) && (localViewtab.yPos >= y1)
		&& (localViewtab.xPos <= x2) && (localViewtab.yPos <= y2));
}

boolean b1Controller() // 1, 0x00 
{
	int eventNum = *_codeWindowAddress++, retVal = 0;

	/* Some events can be activated by menu input or key input. */

	/* Following code detects key presses at the current time */
	switch (events[eventNum].type) {
	case ASCII_KEY_EVENT:
		if (events[eventNum].activated) {
			events[eventNum].activated = FALSE;
			return TRUE;
		}
		return (asciiState[events[eventNum].eventID]);
	case SCAN_KEY_EVENT:
		if (events[eventNum].activated) {
			events[eventNum].activated = FALSE;
			return TRUE;
		}
		if ((events[eventNum].eventID < 59) &&
			(asciiState[0] == 0)) return FALSE;   /* ALT Combinations */
		return (keyState[events[eventNum].eventID]);
	case MENU_EVENT:
		retVal = events[eventNum].activated;
		events[eventNum].activated = 0;
		return (retVal);
	default:
		return (FALSE);
	}
}

boolean b1Have_key() // 0, 0x00
{
	/* return (TRUE); */
	/* return (haveKey); */
	/* return (keypressed() || haveKey); */
	if (haveKey && key[lastKey]) return TRUE;
	return keypressed();
}

boolean b1Said()
{
	int numOfArgs, wordNum, argValue;
	boolean wordsMatch = TRUE;
	byte argLo, argHi;

	numOfArgs = *_codeWindowAddress++;

	if ((flag[2] == 0) || (flag[4] == 1)) {  /* Not valid input waiting */
		_codeWindowAddress += (numOfArgs * 2); /* Jump over arguments */
		return FALSE;
	}

	/* Needs to deal with ANYWORD and ROL */
	for (wordNum = 0; wordNum < numOfArgs; wordNum++) {
		argLo = *_codeWindowAddress++;
		argHi = *_codeWindowAddress++;
		argValue = (argLo + (argHi << 8));
		if (argValue == 9999) break; /* Should always be last argument */
		if (argValue == 1) continue; /* Word comparison does not matter */
		if (inputWords[wordNum] != argValue) wordsMatch = FALSE;
	}

	if ((numInputWords != numOfArgs) && (argValue != 9999)) return FALSE;

	if (wordsMatch) {
		flag[4] = TRUE;    /* said() accepted input */
		numInputWords = 0;
		flag[2] = FALSE;   /* not sure about this one */
	}

	return (wordsMatch);
}

/* The said() command is in parser.h
boolean said(byte **data)
{

}
*/

boolean b1Compare_strings() // 2, 0x00 
{
	int s1, s2;

	s1 = *_codeWindowAddress++;
	s2 = *_codeWindowAddress++;
	if (strcmp(string[s1], string[s2]) == 0) return TRUE;
	return FALSE;
}

boolean b1Obj_in_box() // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;
	objNum = *_codeWindowAddress++;
	x1 = *_codeWindowAddress++;
	y1 = *_codeWindowAddress++;
	x2 = *_codeWindowAddress++;
	y2 = *_codeWindowAddress++;

	getViewTab(&localViewtab, objNum);

	return ((localViewtab.xPos >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + localViewtab.xsize - 1) <= x2) &&
		(localViewtab.yPos <= y2));
}

boolean b1Center_posn() // 5, 0x00 }
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;
	objNum = *_codeWindowAddress++;
	x1 = *_codeWindowAddress++;
	y1 = *_codeWindowAddress++;
	x2 = *_codeWindowAddress++;
	y2 = *_codeWindowAddress++;

	getViewTab(&localViewtab, objNum);

	return (((localViewtab.xPos + (localViewtab.xsize / 2)) >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + (localViewtab.xsize / 2)) <= x2) &&
		(localViewtab.yPos <= y2));
}

boolean b1Right_posn() // 5, 0x00
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;

	objNum = *_codeWindowAddress++;

	getViewTab(&localViewtab, objNum);

	x1 = *_codeWindowAddress++;
	y1 = *_codeWindowAddress++;
	x2 = *_codeWindowAddress++;
	y2 = *_codeWindowAddress++;

	return (((localViewtab.xPos + localViewtab.xsize - 1) >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + localViewtab.xsize - 1) <= x2) &&
		(localViewtab.yPos <= y2));
}



/* ACTION COMMANDS */

void b1Increment() // 1, 0x80 
{
	if (var[*_codeWindowAddress] < 0xFF)
		var[*_codeWindowAddress]++;
	_codeWindowAddress++;

	/* var[*_codeWindowAddress++]++;  This one doesn't check bounds */
}

void b1Decrement() // 1, 0x80 
{
	if (var[*_codeWindowAddress] > 0)
		var[*_codeWindowAddress]--;
	_codeWindowAddress++;

	/* var[*_codeWindowAddress++]--;  This one doesn't check bounds */
}

void b1Assignn() // 2, 0x80 
{
	int varNum, value;

	varNum = *_codeWindowAddress++;
	value = *_codeWindowAddress++;
	var[varNum] = value;
}

void b1Assignv() // 2, 0xC0 
{
	int var1, var2;

	var1 = *_codeWindowAddress++;
	var2 = *_codeWindowAddress++;
	var[var1] = var[var2];
}

void b1Addn() // 2, 0x80 
{
	int varNum, value;

	varNum = *_codeWindowAddress++;
	value = *_codeWindowAddress++;
	var[varNum] += value;
}

void b1Addv() // 2, 0xC0 
{
	int var1, var2;

	var1 = *_codeWindowAddress++;
	var2 = *_codeWindowAddress++;
	var[var1] += var[var2];
}

void b1Subn() // 2, 0x80 
{
	int varNum, value;

	varNum = *_codeWindowAddress++;
	value = *_codeWindowAddress++;
	var[varNum] -= value;
}

void b1Subv() // 2, 0xC0 
{
	int var1, var2;

	var1 = *_codeWindowAddress++;
	var2 = *_codeWindowAddress++;
	var[var1] -= var[var2];
}

void b1Lindirectv() // 2, 0xC0 
{
	int var1, var2;

	var1 = *_codeWindowAddress++;
	var2 = *_codeWindowAddress++;
	var[var[var1]] = var[var2];
}

void b1Rindirect() // 2, 0xC0 
{
	int var1, var2;

	var1 = *_codeWindowAddress++;
	var2 = *_codeWindowAddress++;
	var[var1] = var[var[var2]];
}

void b1Lindirectn() // 2, 0x80 
{
	int varNum, value;

	varNum = *_codeWindowAddress++;
	value = *_codeWindowAddress++;
	var[var[varNum]] = value;
}

void b1Set() // 1, 0x00 
{
	flag[*_codeWindowAddress++] = TRUE;
}

void b1Reset() // 1, 0x00 
{
	flag[*_codeWindowAddress++] = FALSE;
}

void b1Toggle() // 1, 0x00 
{
	int f = *_codeWindowAddress++;

	flag[f] = (flag[f] ? FALSE : TRUE);
}

void b1Set_v() // 1, 0x80 
{
	flag[var[*_codeWindowAddress++]] = TRUE;
}

void b1Reset_v() // 1, 0x80 
{
	flag[var[*_codeWindowAddress++]] = FALSE;
}

void b1Toggle_v() // 1, 0x80 
{
	int f = var[*_codeWindowAddress++];

	flag[f] = (flag[f] ? FALSE : TRUE);
}

void b1New_room() // 1, 0x00 
{
	/* This function is handled in meka.c */
	newRoomNum = *_codeWindowAddress++;
	hasEnteredNewRoom = TRUE;
}

void b1New_room_v() // 1, 0x80 
{
	/* This function is handled in meka.c */
	newRoomNum = var[*_codeWindowAddress++];
	hasEnteredNewRoom = TRUE;
}

void b1Load_logics() // 1, 0x00 
{
	trampoline_1Int(&b8LoadLogicFile, *_codeWindowAddress++, LOGIC_CODE_BANK);
}

void b1Load_logics_v() // 1, 0x80 
{
	trampoline_1Int(&b8LoadLogicFile, var[*_codeWindowAddress++], LOGIC_CODE_BANK);
}

void b1Call() // 1, 0x00 
{
	printf("About to execute logic %d", *_codeWindowAddress);
	executeLogic(*_codeWindowAddress++);
}

void b1Call_v() // 1, 0x80 
{
	printf("About to execute logic %d", *_codeWindowAddress);
	executeLogic(var[*_codeWindowAddress++]);
}

void b1Load_pic() // 1, 0x80 
{
	loadPictureFile(var[*_codeWindowAddress++]);
}

void b1Draw_pic() // 1, 0x80 
{
	int pNum;

	pNum = var[*_codeWindowAddress++];
	//picFNum = pNum;  // Debugging. Delete at some stage!!!
	drawPic(loadedPictures[pNum].data, loadedPictures[pNum].size, TRUE);
}

void b1Show_pic() // 0, 0x00 
{
	okToShowPic = TRUE;   /* Says draw picture with next object update */
	/*stretch_blit(picture, working_screen, 0, 0, 160, 168, 0, 20, 640, 336);*/
	showPicture();
}

void b1Discard_pic() // 1, 0x80 
{
	discardPictureFile(var[*_codeWindowAddress++]);
}

void b1Overlay_pic() // 1, 0x80 
{
	int pNum;

	pNum = var[*_codeWindowAddress++];
	drawPic(loadedPictures[pNum].data, loadedPictures[pNum].size, FALSE);
}

void b1Show_pri_screen() // 0, 0x00 
{
	//showPriority();
	showDebugPri();
	//getch();
	//while (!keypressed()) { /* Wait for key */ }
}

/************************** VIEW ACTION COMMANDS **************************/

void b1Load_view() // 1, 0x00 
{
	trampoline_1Int(&b9LoadViewFile, *_codeWindowAddress++, VIEW_CODE_BANK_1);
}

void b1Load_view_v() // 1, 0x80 
{
	trampoline_1Int(&b9LoadViewFile, var[*_codeWindowAddress++], VIEW_CODE_BANK_1);
}

void b1Discard_view() // 1, 0x00 
{
	trampoline_1Int(&b9DiscardView, *_codeWindowAddress++, VIEW_CODE_BANK_1);
}

void b1Animate_obj() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewTab;

	entryNum = *_codeWindowAddress++;
	//viewtab[entryNum].flags |= (ANIMATED | UPDATE | CYCLING);
	getViewTab(&localViewTab, entryNum);

	//localViewtab.flags |= (ANIMATED | UPDATE | CYCLING);
	localViewTab.flags = (ANIMATED | UPDATE | CYCLING);
	/* Not sure about CYCLING */
	/* Not sure about whether these two are set to zero */
	localViewTab.motion = 0;
	localViewTab.cycleStatus = 0;
	localViewTab.flags |= MOTION;
	if (entryNum != 0) localViewTab.direction = 0;

	setViewTab(&localViewTab, entryNum);

	getViewTab(&localViewTab, entryNum);
}

void b1Unanimate_all() // 0, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	/* Mark all objects as unanimated and not drawn */
	for (entryNum = 0; entryNum < TABLESIZE; entryNum++)
	{
		getViewTab(&localViewtab, entryNum);

		localViewtab.flags &= ~(ANIMATED | DRAWN);

		setViewTab(&localViewtab, entryNum);
	}
}

void b1Draw() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= (DRAWN | UPDATE);   /* Not sure about update */



	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, localViewtab.currentCel, VIEW_CODE_BANK_1);

	trampoline_1Int(&bADrawObject, entryNum, VIEW_CODE_BANK_2);

	setViewTab(&localViewtab, entryNum);
}

void b1Erase() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags &= ~DRAWN;

	setViewTab(&localViewtab, entryNum);
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM02")

void b2Position() // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = *_codeWindowAddress++;
	localViewtab.yPos = *_codeWindowAddress++;

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
}

void b2Position_v() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[*_codeWindowAddress++];
	localViewtab.yPos = var[*_codeWindowAddress++];

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
}

void b2Get_posn() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	var[*_codeWindowAddress++] = localViewtab.xPos;
	var[*_codeWindowAddress++] = localViewtab.yPos;
}

void b2Reposition() // 3, 0x60 
{
	int entryNum, dx, dy;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	dx = (signed char)var[*_codeWindowAddress++];
	dy = (signed char)var[*_codeWindowAddress++];
	localViewtab.xPos += dx;
	localViewtab.yPos += dy;

	setViewTab(&localViewtab, entryNum);
}


void b2Set_view() // 2, 0x00 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	viewNum = *_codeWindowAddress++;

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_view_v() // 2, 0x40 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	viewNum = var[*_codeWindowAddress++];

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	getViewTab(&localViewtab, entryNum);
}

void b2Set_loop() // 2, 0x00 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	loopNum = *_codeWindowAddress++;

	getViewTab(&localViewtab, entryNum);
	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, 0, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_loop_v() // 2, 0x40 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	loopNum = var[*_codeWindowAddress++];

	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, loopNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Fix_loop() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= FIXLOOP;

	setViewTab(&localViewtab, entryNum);

}

void b2Release_loop() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXLOOP;
	setViewTab(&localViewtab, entryNum);
}

void b2Set_cel() // 2, 0x00 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	celNum = *_codeWindowAddress++;

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_cel_v() // 2, 0x40 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	celNum = var[*_codeWindowAddress++];

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Last_cel() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);
	varNum = *_codeWindowAddress++;

	var[varNum] = localViewtab.numberOfCels - 1;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_cel() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);
	varNum = *_codeWindowAddress++;

	var[varNum] = localViewtab.currentCel;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_loop() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	varNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	var[varNum] = localViewtab.currentLoop;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_view() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	varNum = *_codeWindowAddress++;
	var[varNum] = localViewtab.currentView;

	setViewTab(&localViewtab, entryNum);
}

void b2Number_of_loops() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	varNum = *_codeWindowAddress++;
	var[varNum] = localViewtab.numberOfLoops;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_priority() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = *_codeWindowAddress++;
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_priority_v() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = var[*_codeWindowAddress++];
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Release_priority() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Get_priority() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	varNum = *_codeWindowAddress++;
	var[varNum] = localViewtab.priority;

	setViewTab(&localViewtab, entryNum);
}

void b2Stop_update() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~UPDATE;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_update() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= UPDATE;

	setViewTab(&localViewtab, entryNum);
}

void b2Force_update() // 1, 0x00 
{
	int entryNum;

	entryNum = *_codeWindowAddress++;
	/* Do immediate update here. Call update(entryNum) */

	trampoline_1Int(&bAUpdateObj, entryNum, VIEW_CODE_BANK_1);
}

void b2Ignore_horizon() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_horizon() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_horizon() // 1, 0x00 
{
	horizon = *_codeWindowAddress++;
}

void b2Object_on_water() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONWATER;

	setViewTab(&localViewtab, entryNum);
}

void b2Object_on_land() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONLAND;

	setViewTab(&localViewtab, entryNum);
}

void b2Object_on_anything() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~(ONWATER | ONLAND);

	setViewTab(&localViewtab, entryNum);
}

void b2Ignore_objs() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_objs() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
}


void b2Distance() // 3, 0x20 
{
	int o1, o2, varNum, x1, y1, x2, y2;
	ViewTable localViewtab1, localViewtab2;

	o1 = *_codeWindowAddress++;
	o2 = *_codeWindowAddress++;

	getViewTab(&localViewtab1, o1);
	getViewTab(&localViewtab2, o2);

	varNum = *_codeWindowAddress++;
	/* Check that both objects are on screen here. If they aren't
	** then 255 should be returned. */
	if (!((localViewtab1.flags & DRAWN) && (localViewtab2.flags & DRAWN))) {
		var[varNum] = 255;
		return;
	}
	x1 = localViewtab1.xPos;
	y1 = localViewtab1.yPos;
	x2 = localViewtab2.xPos;
	y2 = localViewtab2.yPos;
	var[varNum] = abs(x1 - x2) + abs(y1 - y2);
}

void b2Stop_cycling() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~CYCLING;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_cycling() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= CYCLING;

	setViewTab(&localViewtab, entryNum);
}

void b2Normal_cycle() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleStatus = 0;

	setViewTab(&localViewtab, entryNum);
}

void b2End_of_loop() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *_codeWindowAddress++;
	localViewtab.cycleStatus = 1;
	localViewtab.flags |= (UPDATE | CYCLING);

	setViewTab(&localViewtab, entryNum);
}

void b2Reverse_cycle() // 1, 0x00
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);
	/* Store the other parameters here */

	localViewtab.cycleStatus = 3;

	setViewTab(&localViewtab, entryNum);
}

void b2Reverse_loop() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *_codeWindowAddress++;
	localViewtab.cycleStatus = 2;
	localViewtab.flags |= (UPDATE | CYCLING);

	setViewTab(&localViewtab, entryNum);
}

void b2Cycle_time() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleTime = var[*_codeWindowAddress++];
	setViewTab(&localViewtab, entryNum);
}

void b2Stop_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~MOTION;
	localViewtab.direction = 0;
	localViewtab.motion = 0;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= MOTION;
	localViewtab.motion = 0;        /* Not sure about this */

	setViewTab(&localViewtab, entryNum);
}

void b2Step_size() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepSize = var[*_codeWindowAddress++];

	setViewTab(&localViewtab, entryNum);
}

void b2Step_time() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepTime = var[*_codeWindowAddress++];

	setViewTab(&localViewtab, entryNum);
}

void b2Move_obj() // 5, 0x00 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *_codeWindowAddress++;
	localViewtab.param2 = *_codeWindowAddress++;
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = *_codeWindowAddress++;
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = *_codeWindowAddress++;
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Move_obj_v() // 5, 0x70 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = var[*_codeWindowAddress++];
	localViewtab.param2 = var[*_codeWindowAddress++];
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = var[*_codeWindowAddress++];
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = *_codeWindowAddress++;
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Follow_ego() // 3, 0x00 
{
	int entryNum, stepVal, flagNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	stepVal = *_codeWindowAddress++;
	flagNum = *_codeWindowAddress++;
	localViewtab.param1 = localViewtab.stepSize;
	/* Might need to put 'if (stepVal != 0)' */
	//localViewtab.stepSize = stepVal;
	localViewtab.param2 = flagNum;
	localViewtab.motion = 2;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Wander() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.motion = 1;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Normal_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.motion = 0;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_dir() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.direction = var[*_codeWindowAddress++];

	setViewTab(&localViewtab, entryNum);
}

void b2Get_dir() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	var[*_codeWindowAddress++] = localViewtab.direction;

	setViewTab(&localViewtab, entryNum);
}

void b2Ignore_blocks() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_blocks() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
}

void b2Block() // 4, 0x00 
{
	/* Is this used anywhere? - Not implemented at this stage */
	_codeWindowAddress += 4;
}

void b2Unblock() // 0, 0x00 
{

}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM03")

void b3Get() // 1, 00 
{
	objects[*_codeWindowAddress++].roomNum = 255;
}

void b3Get_v() // 1, 0x80 
{
	objects[var[*_codeWindowAddress++]].roomNum = 255;
}

void b3Drop() // 1, 0x00 
{
	objects[*_codeWindowAddress++].roomNum = 0;
}


void b3Put() // 2, 0x00 
{
	int objNum, room;

	objNum = *_codeWindowAddress++;
	room = *_codeWindowAddress++;
	objects[objNum].roomNum = room;
}

void b3Put_v() // 2, 0x40 
{
	int objNum, room;

	objNum = *_codeWindowAddress++;
	room = var[*_codeWindowAddress++];
	objects[objNum].roomNum = room;
}

void b3Get_room_v() // 2, 0xC0 
{
	int objNum, room;

	objNum = var[*_codeWindowAddress++];
	var[*_codeWindowAddress++] = objects[objNum].roomNum;
}

void b3Load_sound() // 1, 0x00 
{
	int soundNum;

	soundNum = *_codeWindowAddress++;
	loadSoundFile(soundNum);
}

void b3Play_sound() // 2, 00  sound() renamed to avoid clash
{
	int soundNum;

	soundNum = *_codeWindowAddress++;
	soundEndFlag = *_codeWindowAddress++;
	/* playSound(soundNum); */
	flag[soundEndFlag] = TRUE;
}

void b3Stop_sound() // 0, 0x00 
{
	checkForEnd = FALSE;
	stop_midi();
}

boolean b3CharIsIn(char testChar, char* testString)
{
	int i;

	for (i = 0; i < strlen(testString); i++) {
		if (testString[i] == testChar) return TRUE;
	}

	return FALSE;
}

void b3ProcessString(char* stringPointer, byte stringBank, char* outputString)
{
#define TEMP_SIZE 80
#define NUM_STRING_SIZE 80
#define INPUT_BUFFER_SIZE 10
	int i, j, strPos = 0, tempNum, widthNum, count, sprintfLength;
	char* temp, messagePointer;
	byte tempBank, logicFileBank;
	char inputString[INPUT_BUFFER_SIZE];
	LOGICFile logicFile;

	outputString[0] = 0;

	for (i = 0, j = 0; i == 0 || inputString[i - 1] != '\0'; i = (i + 1) % INPUT_BUFFER_SIZE, j++) {
		if (i == 0)
		{
			copyStringFromBanked(stringPointer, inputString, j, INPUT_BUFFER_SIZE, stringBank, FALSE);
		}

		if (inputString[i] == '%') {
			i++;
			switch (inputString[i++]) {
				/* %% isn't actually supported */
				//case '%': sprintf(outputString, "%s%%", outputString); break;
			case 'v':
				tempNum = getNum(inputString, &i, stringBank);
				if (inputString[i + 1] == '|') {
					i += 2;
					temp = (char*)banked_alloc(NUM_STRING_SIZE, &tempBank);
					widthNum = getNum(inputString, &i, stringBank);
					sprintfLength = sprintfBanked(temp, tempBank, "%d", var[tempNum]);
					for (count = sprintfLength; count < widthNum; count++) {
						sprintf(outputString, "%s0", outputString);
					}
					sprintf(outputString, "%s%d", outputString, var[tempNum]);
					banked_dealloc((byte*)temp, &tempBank);
				}
				else
					sprintf(outputString, "%s%d", outputString, var[tempNum]);
				break;
			case 'm':
				tempNum = getNum(inputString, &i, stringBank);
				getLogicFile(&logicFile, currentLog);
				messagePointer = getMessagePointer(currentLog, tempNum - 1);
				sprintfBanked(outputString, logicFile.codeBank, "%s%s", outputString,
					logics[currentLog].data->messages[tempNum - 1]);
				break;
			case 'g':
				tempNum = getNum(inputString, &i, stringBank);
				getLogicFile(&logicFile, currentLog);
				messagePointer = getMessagePointer(0, tempNum - 1);
				sprintfBanked(outputString, "%s%s", outputString, messagePointer);
				break;
			case 'w':
				tempNum = getNum(inputString, &i, stringBank);
				sprintf(outputString, "%s%s", outputString, wordText[tempNum]);
				break;
			case 's':
				tempNum = getNum(inputString, &i, stringBank);
				sprintf(outputString, "%s%s", outputString, string[tempNum]);
				break;
			default: /* ignore the second character */
				break;
			}
		}
		else {
			sprintf(outputString, "%s%c", outputString, inputString[i]);
		}
	}

	/* Recursive part to make sure all % formatting codes are dealt with */
	if (b3CharIsIn('%', outputString)) {
		temp = (char*)banked_alloc(TEMP_SIZE, &tempBank);
		strcpyBanked(temp, outputString, tempBank);
		b3ProcessString(temp, tempBank, outputString);

		banked_dealloc((byte*)temp, tempBank);
	}
}

void b3Print() // 1, 00 
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (*_codeWindowAddress++) - 1);

	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	b3ProcessString(messagePointer, 0, tempString);
	printInBoxBig(tempString, -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	show_mouse(NULL);
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);
}

void b3Print_v() // 1, 0x80 
{
	char* tempString = (char*) & GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (var[*_codeWindowAddress++]) - 1);

	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	b3ProcessString(messagePointer, 0, tempString);
	//printf("Warning Print In Bigbox Not Implemented Implement This");
	//printInBoxBig2(tempString, -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);

}

void b3Display() // 3, 0x00 
{
	int row, col, messNum;
	char* tempString = (char*) & GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	col = *_codeWindowAddress++;
	row = *_codeWindowAddress++;
	messNum = *_codeWindowAddress++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	b3ProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	/*lprintf("info: display() %s, fg: %d bg: %d row: %d col: %d",
	   tempString, agi_fg, agi_bg, row, col);*/
}

void b3Display_v() // 3, 0xE0 
{
	int row, col, messNum;
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	col = var[*_codeWindowAddress++];
	row = var[*_codeWindowAddress++];
	messNum = var[*_codeWindowAddress++];
	//drawString(picture, logics[currentLog].data->messages[messNum-1],
	//   row*8, col*8, agi_fg, agi_bg);

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	b3ProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	/*lprintf("info: display.v() %s, foreground: %d background: %d",
	   tempString, agi_fg, agi_bg);*/
}

void b3Clear_lines() // 3, 0x00 
{
	int boxColour, startLine, endLine;

	startLine = *_codeWindowAddress++;
	endLine = *_codeWindowAddress++;
	boxColour = *_codeWindowAddress++;
	if ((screenMode == AGI_GRAPHICS) && (boxColour > 0)) boxColour = 15;
	boxColour++;
	show_mouse(NULL);
	rectfill(agi_screen, 0, startLine * 16, 639, (endLine * 16) + 15, boxColour);
	show_mouse(screen);
}

void b3Text_screen() // 0, 0x00 
{
	screenMode = AGI_TEXT;
	/* Do something else here */
	inputLineDisplayed = FALSE;
	statusLineDisplayed = FALSE;
	clear(screen);
}

void b3Graphics() // 0, 0x00 
{
	screenMode = AGI_GRAPHICS;
	/* Do something else here */
	inputLineDisplayed = TRUE;
	statusLineDisplayed = TRUE;
	okToShowPic = TRUE;
	clear(screen);
}

void b3Set_cursor_char() // 1, 0x00 
{
	char* temp = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	byte msgNo = (*_codeWindowAddress++) - 1;
	char* messagePointer = getMessagePointer(currentLog, msgNo);
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

#ifdef VERBOSE_STRING_CHECK
	printf("Your msgNo is %d\n", msgNo);
#endif // VERBOSE_STRING_CHECK

	b3ProcessString(messagePointer, logicFile.messageBank, temp);
	cursorChar = temp[0];

#ifdef VERBOSE_STRING_CHECK
	printf("Your cursor char is %c\n", cursorChar);
#endif
}

void b3Set_text_attribute() // 2, 0x00 
{
	agi_fg = (*_codeWindowAddress++) + 1;
	agi_bg = (*_codeWindowAddress++) + 1;
}

void b3Shake_screen() // 1, 0x00 
{
	*_codeWindowAddress++;  /* Ignore this for now. */
}

void b3Configure_screen() // 3, 0x00 
{
	min_print_line = *_codeWindowAddress++;
	user_input_line = *_codeWindowAddress++;
	status_line_num = *_codeWindowAddress++;
}

void b3Status_line_on() // 0, 0x00 
{
	statusLineDisplayed = TRUE;
}

void b3Status_line_off() // 0, 0x00 
{
	statusLineDisplayed = FALSE;
}

void b3Set_string() // 2, 0x00 
{
	int stringNum, messNum;
	char* messagePointer;
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

	stringNum = *_codeWindowAddress++;
	messNum = *_codeWindowAddress++;
	messagePointer = getMessagePointer(currentLog, messNum - 1);

	strcpyBanked(string[stringNum - 1], messagePointer, logicFile.messageBank);
}

void b3Get_string() // 5, 0x00 
{
	int strNum, messNum, row, col, l;
	char* messagePointer;

	strNum = *_codeWindowAddress++;
	messNum = *_codeWindowAddress++;
	col = *_codeWindowAddress++;
	row = *_codeWindowAddress++;
	l = *_codeWindowAddress++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	getString(messagePointer, string[strNum], row, col, l);
}

void b3Word_to_string() // 2, 0x00 
{
	int stringNum, wordNum;

	stringNum = *_codeWindowAddress++;
	wordNum = *_codeWindowAddress++;
	strcpy(string[stringNum], wordText[wordNum]);
}

void b3Parse() // 1, 0x00 
{
	int stringNum;

	stringNum = *_codeWindowAddress++;
	lookupWords(string[stringNum]);
}

void b3Get_num() // 2, 0x40 
{
	int messNum, varNum;
	char temp[80];
	char* messagePointer;

	messNum = *_codeWindowAddress++;
	varNum = *_codeWindowAddress++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	getString(messagePointer, temp, 1, 23, 3);
	var[varNum] = atoi(temp);
}

void b3Prevent_input() // 0, 0x00 
{
	inputLineDisplayed = FALSE;
	/* Do something else here */
}

void b3Accept_input() // 0, 0x00 
{
	inputLineDisplayed = TRUE;
	/* Do something else here */
}

void b3Set_key() // 3, 0x00 
{
	int asciiCode, scanCode, eventCode;
	char* tempStr = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];

	asciiCode = *_codeWindowAddress++;
	scanCode = *_codeWindowAddress++;
	eventCode = *_codeWindowAddress++;


	/* Ignore cases which have both values set for now. They seem to behave
	** differently than normal and often specify controllers that have
	** already been defined.
	*/
	if (scanCode && asciiCode) return;

	if (scanCode) {
		events[eventCode].type = SCAN_KEY_EVENT;
		events[eventCode].eventID = scanCode;
		events[eventCode].asciiValue = asciiCode;
		events[eventCode].scanCodeValue = scanCode;
		events[eventCode].activated = FALSE;
	}
	else if (asciiCode) {
		events[eventCode].type = ASCII_KEY_EVENT;
		events[eventCode].eventID = asciiCode;
		events[eventCode].asciiValue = asciiCode;
		events[eventCode].scanCodeValue = scanCode;
		events[eventCode].activated = FALSE;
	}
}

void b3Add_to_pic() // 7, 0x00 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;

	viewNum = *_codeWindowAddress++;
	loopNum = *_codeWindowAddress++;
	celNum = *_codeWindowAddress++;
	x = *_codeWindowAddress++;
	y = *_codeWindowAddress++;
	priNum = *_codeWindowAddress++;
	baseCol = *_codeWindowAddress++;

	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
}

void b3Add_to_pic_v() // 7, 0xFE 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;

	viewNum = var[*_codeWindowAddress++];
	loopNum = var[*_codeWindowAddress++];
	celNum = var[*_codeWindowAddress++];
	x = var[*_codeWindowAddress++];
	y = var[*_codeWindowAddress++];
	priNum = var[*_codeWindowAddress++];
	baseCol = var[*_codeWindowAddress++];

	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
}

void b3Status() // 0, 0x00 
{
	/* Inventory */
	// set text mode
	// if flag 13 is set then allow selection and store selection in var[25]
	var[25] = 255;
}

void b3Save_game() // 0, 0x00 
{
	/* Not supported yet */
}

void b3Restore_game() // 0, 0x00 
{
	/* Not supported yet */
}


void b3Restart_game() // 0, 0x00 
{
	int i;

	/* Not supported yet */
	for (i = 0; i < 256; i++) {
		flag[i] = FALSE;
		var[i] = 0;
	}
	var[24] = 0x29;
	var[26] = 3;
	var[8] = 255;     /* Number of free 256 byte pages of memory */

	newRoomNum = 0;
	hasEnteredNewRoom = TRUE;
}

void b3Show_obj() // 1, 0x00 
{
	int objectNum;

	objectNum = *_codeWindowAddress++;
	/* Not supported yet */
}

void b3Random_num() // 3, 0x20  random() renamed to avoid clash
{
	int startValue, endValue;

	startValue = *_codeWindowAddress++;
	endValue = *_codeWindowAddress++;
	var[*_codeWindowAddress++] = (rand() % ((endValue - startValue) + 1)) + startValue;
}

void b3Program_control() // 0, 0x00 
{
	controlMode = PROGRAM_CONTROL;
}

void b3Player_control() // 0, 0x00 
{
	controlMode = PLAYER_CONTROL;
}

void b3Obj_status_v() // 1, 0x80 
{
	int objectNum;

	objectNum = var[*_codeWindowAddress++];
	/* Not supported yet */

	/* showView(viewtab[objectNum].currentView); */
	trampoline_1Int(&bDShowObjectState, objectNum, VIEW_CODE_BANK_4);
}


void b3Quit() // 1, 0x00                     /* 0 args for AGI version 2_089 */
{
	int quitType, ch;

	quitType = ((!oldQuit) ? *_codeWindowAddress++ : 0);
	if (quitType == 1) /* Immediate quit */
		exit(0);
	else { /* Prompt for exit */
		printInBoxBig("Press ENTER to quit.\nPress ESC to keep playing.", -1, -1, 30);
		do {
			ch = (readkey() >> 8);
		} while ((ch != KEY_ESC) && (ch != KEY_ENTER));
		if (ch == KEY_ENTER) exit(0);
		showPicture();
	}
}

void b3Pause() // 0, 0x00 
{
	while (key[KEY_ENTER]) { /* Wait */ }
	printInBoxBig("      Game paused.\nPress ENTER to continue.", -1, -1, 30);
	while (!key[KEY_ENTER]) { /* Wait */ }
	showPicture();
	okToShowPic = TRUE;
}


void b3Echo_line() // 0, 0x00 
{

}


void b3Cancel_line() // 0, 0x00 
{
	/*currentInputStr[0]=0;
	strPos=0;*/
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM04")

void b4Init_joy() // 0, 0x00 
{
	/* Not important at this stage */
}

void b4Version() // 0, 0x00 
{
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	printInBoxBig("MEKA AGI Interpreter\n    Version 1.0", -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	showPicture();
	okToShowPic = TRUE;
}

void b4Script_size() // 1, 0x00 
{
	*_codeWindowAddress++;  /* Ignore the script size. Not important for this interpreter */
}

void b4Set_game_id() // 1, 0x00 
{
	*_codeWindowAddress++;  /* Ignore the game ID. Not important */
}

void b4Log() // 1, 0x00 
{
	*_codeWindowAddress++;  /* Ignore log message. Not important */
}

void b4Set_scan_start() // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	/* currentPoint is set in executeLogic() */
	logicEntry.entryPoint = logicEntry.currentPoint + 1;
	/* Does it return() at this point, or does it execute to the end?? */

	setLogicEntry(&logicEntry, currentLog);
}


void b4Reset_scan_start() // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	logicEntry.entryPoint = 0;

	setLogicEntry(&logicEntry, currentLog);
}

void b4Reposition_to() // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = *_codeWindowAddress++;
	localViewtab.yPos = *_codeWindowAddress++;

	setViewTab(&localViewtab, entryNum);
}

void b4Reposition_to_v() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *_codeWindowAddress++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[*_codeWindowAddress++];
	localViewtab.yPos = var[*_codeWindowAddress++];

	setViewTab(&localViewtab, entryNum);
}

void b4Trace_on() // 0, 0x00 
{
	/* Ignore at this stage */
}

void b4Trace_info() // 3, 0x00 
{
	_codeWindowAddress += 3;  /* Ignore trace information at this stage. */
}

void b4Print_at() // 4, 0x00           /* 3 args for AGI versions before */
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;

	messNum = *_codeWindowAddress++;
	x = *_codeWindowAddress++;
	y = *_codeWindowAddress++;
	l = *_codeWindowAddress++;
	//show_mouse(NULL);
	//temp = create_bitmap(640, 336);
	//blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	//show_mouse(screen);
	//while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }

	//messagePointer = getMessagePointer(currentLog, messNum - 1);

	//b3ProcessString(messagePointer, 0, tempString);
	//printInBoxBig(tempString, x, y, l);
	//while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	//while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	//show_mouse(NULL);
	//blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	//show_mouse(screen);
	//destroy_bitmap(temp);
}

void b4Print_at_v() // 4, 0x80         /* 2_440 (maybe laterz) */
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;

	messNum = var[*_codeWindowAddress++];
	x = *_codeWindowAddress++;
	y = *_codeWindowAddress++;
	l = *_codeWindowAddress++;
	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	b3ProcessString(messagePointer, 0, tempString);
	printInBoxBig(tempString, x, y, l);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	show_mouse(NULL);
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);
}

void b4Discard_view_v() // 1, 0x80 
{
	trampoline_1Int(&b9DiscardView, var[*_codeWindowAddress++], VIEW_CODE_BANK_1);
}

void b4Clear_text_rect() // 5, 0x00 
{
	int x1, y1, x2, y2, boxColour;

	x1 = *_codeWindowAddress++;
	y1 = *_codeWindowAddress++;
	x2 = *_codeWindowAddress++;
	y2 = *_codeWindowAddress++;
	boxColour = *_codeWindowAddress++;
	if ((screenMode == AGI_GRAPHICS) && (boxColour > 0)) boxColour = 15;
	if (screenMode == AGI_TEXT) boxColour = 0;
	show_mouse(NULL);
	rectfill(agi_screen, x1 * 16, y1 * 16, (x2 * 16) + 15, (y2 * 16) + 15, boxColour);
	show_mouse(screen);
}

void b4Set_upper_left() // 2, 0x00    (x, y) ??
{
	_codeWindowAddress += 2;
}

void b4WaitKeyRelease()
{
	while (keypressed()) { /* Wait */ }
}

int menuEvent0() { b4WaitKeyRelease(); events[0].activated = 1; return D_O_K; }
int menuEvent1() { b4WaitKeyRelease(); events[1].activated = 1; return D_O_K; }
int menuEvent2() { b4WaitKeyRelease(); events[2].activated = 1; return D_O_K; }
int menuEvent3() { b4WaitKeyRelease(); events[3].activated = 1; return D_O_K; }
int menuEvent4() { b4WaitKeyRelease(); events[4].activated = 1; return D_O_K; }
int menuEvent5() { b4WaitKeyRelease(); events[5].activated = 1; return D_O_K; }
int menuEvent6() { b4WaitKeyRelease(); events[6].activated = 1; return D_O_K; }
int menuEvent7() { b4WaitKeyRelease(); events[7].activated = 1; return D_O_K; }
int menuEvent8() { b4WaitKeyRelease(); events[8].activated = 1; return D_O_K; }
int menuEvent9() { b4WaitKeyRelease(); events[9].activated = 1; return D_O_K; }
int menuEvent10() { b4WaitKeyRelease(); events[10].activated = 1; return D_O_K; }
int menuEvent11() { b4WaitKeyRelease(); events[11].activated = 1; return D_O_K; }
int menuEvent12() { b4WaitKeyRelease(); events[12].activated = 1; return D_O_K; }
int menuEvent13() { b4WaitKeyRelease(); events[13].activated = 1; return D_O_K; }
int menuEvent14() { b4WaitKeyRelease(); events[14].activated = 1; return D_O_K; }
int menuEvent15() { b4WaitKeyRelease(); events[15].activated = 1; return D_O_K; }
int menuEvent16() { b4WaitKeyRelease(); events[16].activated = 1; return D_O_K; }
int menuEvent17() { b4WaitKeyRelease(); events[17].activated = 1; return D_O_K; }
int menuEvent18() { b4WaitKeyRelease(); events[18].activated = 1; return D_O_K; }
int menuEvent19() { b4WaitKeyRelease(); events[19].activated = 1; return D_O_K; }
int menuEvent20() { b4WaitKeyRelease(); events[20].activated = 1; return D_O_K; }
int menuEvent21() { b4WaitKeyRelease(); events[21].activated = 1; return D_O_K; }
int menuEvent22() { b4WaitKeyRelease(); events[22].activated = 1; return D_O_K; }
int menuEvent23() { b4WaitKeyRelease(); events[23].activated = 1; return D_O_K; }
int menuEvent24() { b4WaitKeyRelease(); events[24].activated = 1; return D_O_K; }
int menuEvent25() { b4WaitKeyRelease(); events[25].activated = 1; return D_O_K; }
int menuEvent26() { b4WaitKeyRelease(); events[26].activated = 1; return D_O_K; }
int menuEvent27() { b4WaitKeyRelease(); events[27].activated = 1; return D_O_K; }
int menuEvent28() { b4WaitKeyRelease(); events[28].activated = 1; return D_O_K; }
int menuEvent29() { b4WaitKeyRelease(); events[29].activated = 1; return D_O_K; }
int menuEvent30() { b4WaitKeyRelease(); events[30].activated = 1; return D_O_K; }
int menuEvent31() { b4WaitKeyRelease(); events[31].activated = 1; return D_O_K; }
int menuEvent32() { b4WaitKeyRelease(); events[32].activated = 1; return D_O_K; }
int menuEvent33() { b4WaitKeyRelease(); events[33].activated = 1; return D_O_K; }
int menuEvent34() { b4WaitKeyRelease(); events[34].activated = 1; return D_O_K; }
int menuEvent35() { b4WaitKeyRelease(); events[35].activated = 1; return D_O_K; }
int menuEvent36() { b4WaitKeyRelease(); events[36].activated = 1; return D_O_K; }
int menuEvent37() { b4WaitKeyRelease(); events[37].activated = 1; return D_O_K; }
int menuEvent38() { b4WaitKeyRelease(); events[38].activated = 1; return D_O_K; }
int menuEvent39() { b4WaitKeyRelease(); events[39].activated = 1; return D_O_K; }
int menuEvent40() { b4WaitKeyRelease(); events[40].activated = 1; return D_O_K; }
int menuEvent41() { b4WaitKeyRelease(); events[41].activated = 1; return D_O_K; }
int menuEvent42() { b4WaitKeyRelease(); events[42].activated = 1; return D_O_K; }
int menuEvent43() { b4WaitKeyRelease(); events[43].activated = 1; return D_O_K; }
int menuEvent44() { b4WaitKeyRelease(); events[44].activated = 1; return D_O_K; }
int menuEvent45() { b4WaitKeyRelease(); events[45].activated = 1; return D_O_K; }
int menuEvent46() { b4WaitKeyRelease(); events[46].activated = 1; return D_O_K; }
int menuEvent47() { b4WaitKeyRelease(); events[47].activated = 1; return D_O_K; }
int menuEvent48() { b4WaitKeyRelease(); events[48].activated = 1; return D_O_K; }
int menuEvent49() { b4WaitKeyRelease(); events[49].activated = 1; return D_O_K; }

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

void b4Set_menu() // 1, 0x00 
{
	int messNum, startOffset;
	char* messData;

	MENU newMenu;
	LOGICFile currentLogicFile;

	if (numOfMenus == 0)
	{
		menuChildInit();
	}

	getLogicFile(&currentLogicFile, currentLog);

	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = 0;
	newMenu.menuTextBank = currentLogicFile.messageBank;

	messNum = *_codeWindowAddress++;

	/* Create new menu and allocate space for MAX_MENU_SIZE items */
	newMenu.text = getMessagePointer(currentLog, messNum - 1);

#ifdef VERBOSE_MENU
	printf("The result is %p \n", newMenu.text);
#endif // VERBOSE_MENU

	newMenu.proc = NULL;

	setMenu(&newMenu, numOfMenus);
	numOfMenus++;

	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = NULL;
	newMenu.text = NULL;
	newMenu.menuTextBank = 0;

	/* Mark end of menu */
	setMenu(&newMenu, numOfMenus);
}

void b4Set_menu_item() // 2, 0x00 
{
	int messNum, controllerNum, i;
	MENU childMenu;
	LOGICFile currentLogicFile;

	getLogicFile(&currentLogicFile, currentLog);

	messNum = *_codeWindowAddress++;
	controllerNum = *_codeWindowAddress++;

	if (events[controllerNum].type == NO_EVENT) {
		events[controllerNum].type = MENU_EVENT;
	}
	events[controllerNum].activated = 0;

	childMenu.text = getMessagePointer(currentLog, messNum - 1);
	childMenu.proc = menuFunctions[controllerNum];
	childMenu.menuTextBank = currentLogicFile.messageBank;

	setMenuChild(&childMenu, numOfMenus - 1);


#ifdef VERBOSE_MENU_DUMP
	testMenus();
#endif // VERBOSE_MENU


}

void b4Submit_menu() // 0, 0x00 
{

}

void b4Enable_item() // 1, 0x00 
{
	_codeWindowAddress++;
}

void b4Disable_item() // 1, 0x00 
{
	*_codeWindowAddress++;
}

void b4Menu_input() // 0, 0x00 
{
	do_menu(the_menu, 10, 20);
}

void b4Show_obj_v() // 1, 0x01 
{
	int objectNum;

	objectNum = var[*_codeWindowAddress++];
	/* Not supported yet */
}

void b4Open_dialogue() // 0, 0x00 
{

}

void b4Close_dialogue() // 0, 0x00 
{

}

void b4Mul_n() // 2, 0x80 
{
	var[*_codeWindowAddress++] *= *_codeWindowAddress++;
}

void b4Mul_v() // 2, 0xC0 
{
	var[*_codeWindowAddress++] *= var[*_codeWindowAddress++];
}

void b4Div_n() // 2, 0x80 
{
	var[*_codeWindowAddress++] /= *_codeWindowAddress++;
}

void b4Div_v() // 2, 0xC0 
{
	var[*_codeWindowAddress++] /= var[*_codeWindowAddress++];
}

void b4Close_window() // 0, 0x00 
{

}


#pragma code-name (pop)
#pragma code-name (push, "BANKRAM05")
boolean b5instructionHandler(byte code, int* currentLog, byte logNum, byte bank)
{
	//int clockVal;
	//clock_t before = clock();

	switch (code)
	{
	case 0: /* return */
		return FALSE;
		break;
	case 1: trampoline_0(&b1Increment, bank); break;
	case 2: trampoline_0(&b1Decrement, bank); break;
	case 3: trampoline_0(&b1Assignn, bank); break;
	case 4: trampoline_0(&b1Assignv, bank); break;
	case 5: trampoline_0(&b1Addn, bank); break;
	case 6: trampoline_0(&b1Addv, bank); break;
	case 7: trampoline_0(&b1Subn, bank); break;
	case 8: trampoline_0(&b1Subv, bank); break;
	case 9: trampoline_0(&b1Lindirectv, bank); break;
	case 10: trampoline_0(&b1Rindirect, bank); break;
	case 11: trampoline_0(&b1Lindirectn, bank); break;
	case 12: trampoline_0(&b1Set, bank); break;
	case 13: trampoline_0(&b1Reset, bank); break;
	case 14: trampoline_0(&b1Toggle, bank); break;
	case 15: trampoline_0(&b1Set_v, bank); break;
	case 16: trampoline_0(&b1Reset_v, bank); break;
	case 17: trampoline_0(&b1Toggle_v, bank); break;
	case 18:
		trampoline_0(&b1New_room, bank);
		exitAllLogics = TRUE;
		return FALSE;
		break;
	case 19:
		trampoline_0(&b1New_room_v, bank);
		exitAllLogics = TRUE;
		return FALSE;
		break;
	case 20: trampoline_0(&b1Load_logics, bank); break;
	case 21: trampoline_0(&b1Load_logics_v, bank); break;
	case 22:
		trampoline_0(&b1Call, bank);
		/* The currentLog variable needs to be restored */
		*currentLog = logNum;
		if (exitAllLogics) return FALSE;
#ifdef DEBUG
		sprintf(debugString, "LOGIC.%d:       ", currentLog);
		drawBigString(screen, debugString, 0, 384, 0, 7);
#endif
		break;
	case 23:
		trampoline_0(&b1Call_v, bank);
		/* The currentLog variable needs to be restored */
		*currentLog = logNum;
		if (exitAllLogics) return FALSE;
#ifdef DEBUG
		sprintf(debugString, "LOGIC.%d:       ", currentLog);
		drawBigString(screen, debugString, 0, 384, 0, 7);
#endif
		break;
	case 24: trampoline_0(&b1Load_pic, bank); break;
	case 25: trampoline_0(&b1Draw_pic, bank); break;
	case 26: trampoline_0(&b1Show_pic, bank); break;
	case 27: trampoline_0(&b1Discard_pic, bank); break;
	case 28: trampoline_0(&b1Overlay_pic, bank); break;
	case 29: trampoline_0(&b1Show_pri_screen, bank); break;
	case 30: trampoline_0(&b1Load_view, bank); break;
	case 31: trampoline_0(&b1Load_view_v, bank); break;
	case 32: trampoline_0(&b1Discard_view, bank); break;
	case 33: trampoline_0(&b1Animate_obj, bank); break;
	case 34: trampoline_0(&b1Unanimate_all, bank); break;
	case 35: trampoline_0(&b1Draw, bank); break;
	case 36: trampoline_0(&b1Erase, bank); break;
	case 37: trampoline_0(&b2Position, bank); break;
	case 38: trampoline_0(&b2Position_v, bank); break;
	case 39: trampoline_0(&b2Get_posn, bank); break;
	case 40: trampoline_0(&b2Reposition, bank); break;
	case 41: trampoline_0(&b2Set_view, bank); break;
	case 42: trampoline_0(&b2Set_view_v, bank); break;
	case 43: trampoline_0(&b2Set_loop, bank); break;
	case 44: trampoline_0(&b2Set_loop_v, bank); break;
	case 45: trampoline_0(&b2Fix_loop, bank); break;
	case 46: trampoline_0(&b2Release_loop, bank); break;
	case 47: trampoline_0(&b2Set_cel, bank); break;
	case 48: trampoline_0(&b2Set_cel_v, bank); break;
	case 49: trampoline_0(&b2Last_cel, bank); break;
	case 50: trampoline_0(&b2Current_cel, bank); break;
	case 51: trampoline_0(&b2Current_loop, bank); break;
	case 52: trampoline_0(&b2Current_view, bank); break;
	case 53: trampoline_0(&b2Number_of_loops, bank); break;
	case 54: trampoline_0(&b2Set_priority, bank); break;
	case 55: trampoline_0(&b2Set_priority_v, bank); break;
	case 56: trampoline_0(&b2Release_priority, bank); break;
	case 57: trampoline_0(&b2Get_priority, bank); break;
	case 58: trampoline_0(&b2Stop_update, bank); break;
	case 59: trampoline_0(&b2Start_update, bank); break;
	case 60: trampoline_0(&b2Force_update, bank); break;
	case 61: trampoline_0(&b2Ignore_horizon, bank); break;
	case 62: trampoline_0(&b2Observe_horizon, bank); break;
	case 63: trampoline_0(&b2Set_horizon, bank); break;
	case 64: trampoline_0(&b2Object_on_water, bank); break;
	case 65: trampoline_0(&b2Object_on_land, bank); break;
	case 66: trampoline_0(&b2Object_on_anything, bank); break;
	case 67: trampoline_0(&b2Ignore_objs, bank); break;
	case 68: trampoline_0(&b2Observe_objs, bank); break;
	case 69: trampoline_0(&b2Distance, bank); break;
	case 70: trampoline_0(&b2Stop_cycling, bank); break;
	case 71: trampoline_0(&b2Start_cycling, bank); break;
	case 72: trampoline_0(&b2Normal_cycle, bank); break;
	case 73: trampoline_0(&b2End_of_loop, bank); break;
	case 74: trampoline_0(&b2Reverse_cycle, bank); break;
	case 75: trampoline_0(&b2Reverse_loop, bank); break;
	case 76: trampoline_0(&b2Cycle_time, bank); break;
	case 77: trampoline_0(&b2Stop_motion, bank); break;
	case 78: trampoline_0(&b2Start_motion, bank); break;
	case 79: trampoline_0(&b2Step_size, bank); break;
	case 80: trampoline_0(&b2Step_time, bank); break;
	case 81: trampoline_0(&b2Move_obj, bank); break;
	case 82: trampoline_0(&b2Move_obj_v, bank); break;
	case 83: trampoline_0(&b2Follow_ego, bank); break;
	case 84: trampoline_0(&b2Wander, bank); break;
	case 85: trampoline_0(&b2Normal_motion, bank); break;
	case 86: trampoline_0(&b2Set_dir, bank); break;
	case 87: trampoline_0(&b2Get_dir, bank); break;
	case 88: trampoline_0(&b2Ignore_blocks, bank); break;
	case 89: trampoline_0(&b2Observe_blocks, bank); break;
	case 90: trampoline_0(&b2Block, bank); break;
	case 91: trampoline_0(&b2Unblock, bank); break;
	case 92: trampoline_0(&b3Get, bank); break;
	case 93: trampoline_0(&b3Get_v, bank); break;
	case 94: trampoline_0(&b3Drop, bank); break;
	case 95: trampoline_0(&b3Put, bank); break;
	case 96: trampoline_0(&b3Put_v, bank); break;
	case 97: trampoline_0(&b3Get_room_v, bank); break;
	case 98: trampoline_0(&b3Load_sound, bank); break;
	case 99: trampoline_0(&b3Play_sound, bank); break;
	case 100: trampoline_0(&b3Stop_sound, bank); break;
	case 101: trampoline_0(&b3Print, bank); break;
	case 102: trampoline_0(&b3Print_v, bank); break;
	case 103: trampoline_0(&b3Display, bank); break;
	case 104: trampoline_0(&b3Display_v, bank); break;
	case 105: trampoline_0(&b3Clear_lines, bank); break;
	case 106: trampoline_0(&b3Text_screen, bank); break;
	case 107: trampoline_0(&b3Graphics, bank); break;
	case 108: trampoline_0(&b3Set_cursor_char, bank); break;
	case 109: trampoline_0(&b3Set_text_attribute, bank); break;
	case 110: trampoline_0(&b3Shake_screen, bank); break;
	case 111: trampoline_0(&b3Configure_screen, bank); break;
	case 112: trampoline_0(&b3Status_line_on, bank); break;
	case 113: trampoline_0(&b3Status_line_off, bank); break;
	case 114: trampoline_0(&b3Set_string, bank); break;
	case 115: trampoline_0(&b3Get_string, bank); break;
	case 116: trampoline_0(&b3Word_to_string, bank); break;
	case 117: trampoline_0(&b3Parse, bank); break;
	case 118: trampoline_0(&b3Get_num, bank); break;
	case 119: trampoline_0(&b3Prevent_input, bank); break;
	case 120: trampoline_0(&b3Accept_input, bank); break;
	case 121: trampoline_0(&b3Set_key, bank); break;
	case 122: trampoline_0(&b3Add_to_pic, bank); break;
	case 123: trampoline_0(&b3Add_to_pic_v, bank); break;
	case 124: trampoline_0(&b3Status, bank); break;
	case 125: trampoline_0(&b3Save_game, bank); break;
	case 126: trampoline_0(&b3Restore_game, bank); break;
	case 127: break;
	case 128: trampoline_0(&b3Restart_game, bank); break;
	case 129: trampoline_0(&b3Show_obj, bank); break;
	case 130: trampoline_0(&b3Random_num, bank); break;
	case 131: trampoline_0(&b3Program_control, bank); break;
	case 132: trampoline_0(&b3Player_control, bank); break;
	case 133: trampoline_0(&b3Obj_status_v, bank); break;
	case 134: trampoline_0(&b3Quit, bank); break;
	case 135: break;
	case 136: trampoline_0(&b3Pause, bank); break;
	case 137: trampoline_0(&b3Echo_line, bank); break;
	case 138: trampoline_0(&b3Cancel_line, bank); break;
	case 139: trampoline_0(&b4Init_joy, bank); break;
	case 140: break;
	case 141: trampoline_0(&b4Version, bank); break;
	case 142: trampoline_0(&b4Script_size, bank); break;
	case 143: trampoline_0(&b4Set_game_id, bank); break;
	case 144: trampoline_0(&b4Log, bank); break;
	case 145: trampoline_0(&b4Set_scan_start, bank); break;
	case 146: trampoline_0(&b4Reset_scan_start, bank); break;
	case 147: trampoline_0(&b4Reposition_to, bank); break;
	case 148: trampoline_0(&b4Reposition_to_v, bank); break;
	case 149: trampoline_0(&b4Trace_on, bank); break;
	case 150: trampoline_0(&b4Trace_info, bank); break;
	case 151: trampoline_0(&b4Print_at, bank); break;
	case 152: trampoline_0(&b4Print_at_v, bank); break;
	case 153: trampoline_0(&b4Discard_view_v, bank); break;
	case 154: trampoline_0(&b4Clear_text_rect, bank); break;
	case 155: trampoline_0(&b4Set_upper_left, bank); break;
	case 156: trampoline_0(&b4Set_menu, bank); break;
	case 157: trampoline_0(&b4Set_menu_item, bank); break;
	case 158: trampoline_0(&b4Submit_menu, bank); break;
	case 159: trampoline_0(&b4Enable_item, bank); break;
	case 160: trampoline_0(&b4Disable_item, bank); break;
	case 161: trampoline_0(&b4Menu_input, bank); break;
	case 162: trampoline_0(&b4Show_obj_v, bank); break;
	case 163: trampoline_0(&b4Open_dialogue, bank); break;
	case 164: trampoline_0(&b4Close_dialogue, bank); break;
	case 165: trampoline_0(&b4Mul_n, bank); break;
	case 166: trampoline_0(&b4Mul_v, bank); break;
	case 167: trampoline_0(&b4Div_n, bank); break;
	case 168: trampoline_0(&b4Div_v, bank); break;
	case 169: trampoline_0(&b4Close_window, bank); break;
	case 170:  break;
	case 171:  break;
	case 172:  break;
	case 173:  break;
	case 174:  break;
	case 175:  break;
	case 176:  break;
	case 177:  break;
	case 178:  break;
	case 179:  break;
	case 180:  break;
	case 181:  break;
	}

	//clockVal = (unsigned int)((clock() - before) * 1000 / CLOCKS_PER_SEC);

	/*if (clockVal >= 1 && clockVal != 16 && code != 23)
	{
		printf("Load Timer %u Code %u \n", clockVal, code);
	}*/

	return TRUE;
}

int ifLogicHandlers(byte ch, byte bank)
{

	int result;

#ifdef VERBOSE_LOGIC_EXEC
	printf("If Check %d d1 %d, %d", ch, *_codeWindowAddress, (*_codeWindowAddress + 1));
#endif // VERBOSE_LOGIC_EXEC

	switch (ch) {
	case 0: result = FALSE; break; /* Should never happen */
	case 1: result = trampoline_0Retbool(&b1Equaln, bank); break;
	case 2: result = trampoline_0Retbool(&b1Equalv, bank); break;
	case 3: result = trampoline_0Retbool(&b1Lessn, bank); break;
	case 4: result = trampoline_0Retbool(&b1Lessv, bank); break;
	case 5: result = trampoline_0Retbool(&b1Greatern, bank); break;
	case 6: result = trampoline_0Retbool(&b1Greaterv, bank); break;
	case 7: result = trampoline_0Retbool(&b1Isset, bank); break;
	case 8: result = trampoline_0Retbool(&b1Issetv, bank); break;
	case 9: result = trampoline_0Retbool(&b1Has, bank); break;
	case 10: result = trampoline_0Retbool(&b1Obj_in_room, bank); break;
	case 11: result = trampoline_0Retbool(&b1Posn, bank); break;
	case 12: result = trampoline_0Retbool(&b1Controller, bank); break;
	case 13: result = trampoline_0Retbool(&b1Have_key, bank); break;
	case 14: result = trampoline_0Retbool(&b1Said, bank); break;
	case 15: result = trampoline_0Retbool(&b1Compare_strings, bank); break;
	case 16: result = trampoline_0Retbool(&b1Obj_in_box, bank); break;
	case 17: result = trampoline_0Retbool(&b1Center_posn, bank); break;
	case 18: result = trampoline_0Retbool(&b1Right_posn, bank); break;
	default:
		////lprintf("catastrophe: Illegal test [%d], logic %d, posn %d.",
			//ch, currentLog, logics[currentLog].currentPoint);
		result = FALSE;
		break; /* Should never happen */
	}

#ifdef VERBOSE_LOGIC_EXEC
	printf(" And the result is %d \n", result);
#endif // VERBOSE_LOGIC_EXEC
	return result;
}

#pragma code-name (pop)

byte getBankBasedOnCode(byte code)
{
	if (code <= HIGHEST_BANK1_FUNC)
	{
		return 1;
	}
	else if (code <= HIGHEST_BANK2_FUNC)
	{
		return 2;
	}
	else if (code <= HIGHEST_BANK3_FUNC)
	{
		return 3;
	}
	else if (code <= HIGHEST_BANK4_FUNC)
	{
		return 4;
	}
	return RAM_BANK;
}

/***************************************************************************
** ifHandler
***************************************************************************/
void ifHandler(byte** data, byte codeBank)
{
	int ch;
	boolean stillProcessing = TRUE, testVal, notMode = FALSE, orMode = FALSE;
	byte b1, b2;
	short int disp, dummy;
	char debugString[80];
	byte previousBank = RAM_BANK;
	byte codeWindow[CODE_WINDOW_SIZE];
	byte** ppCodeWindowAddress;
	byte ifHandlerBank;

	ppCodeWindowAddress = &_codeWindowAddress;

	RAM_BANK = codeBank;

	while (stillProcessing) {
		ch = *_codeWindowAddress++;

#ifdef DEBUG
		if (ch <= 18) {
			sprintf(debugString, "%s [%x]           ", testCommands[ch].commandName, ch);
			drawBigString(screen, debugString, 0, 400, 0, 7);
			if ((readkey() & 0xff) == 'q') closedown();
		}
#endif

#ifdef VERBOSE_LOGIC_EXEC
		printf("-- %d %d\n", printCounter + 1, ch);
#endif // VERBOSE_LOGIC_EXEC
		printCounter++;
		switch (ch) {
		case 0xff: /* Closing if bracket. Expression must be true. */
#ifdef DEBUG
			drawBigString(screen, "test is true             ", 0, 400, 0, 7);
			if ((readkey() & 0xff) == 'q') closedown();
#endif
			* data += 2;
			return;
		case 0xfd: /* Not mode toggle */
			notMode = (notMode ? FALSE : TRUE);
			break;
		case 0xfc:
			if (orMode) {
				/* If we have reached the closing OR bracket, then the
				** test for the whole expression must be false. */
				stillProcessing = FALSE;
			}
			else {
				orMode = TRUE;
			}
			break;
		default:
			memcpy(&codeWindow[0], *data, CODE_WINDOW_SIZE);
			_codeWindowAddress = &codeWindow[0];
			RAM_BANK = IF_LOGIC_HANDLERS_BANK;
			ifHandlerBank = getBankBasedOnCode(ch);

			testVal = ifLogicHandlers(ch, ifHandlerBank);

			RAM_BANK = previousBank;

#ifdef VERBOSE_LOGIC_EXEC

			printf("Data was %p trying to add %p ", data, _codeWindowAddress - &codeWindow[0]);
#endif // VERBOSE
			* data += (_codeWindowAddress - &codeWindow[0]);

#ifdef VERBOSE_LOGIC_EXEC
			printf("Data is %p %u \n", data, *data);
#endif
			if (notMode) testVal = (testVal ? FALSE : TRUE);
			notMode = 0;
			if (testVal) {
				if (orMode) {
					/* Find the closing OR. It can't just search for 0xfc
					** because this could be a parameter for one of the test
					** commands rather than being the closing OR. We therefore
					** have to jump over each command as we find it. */
					while (TRUE) {
						ch = *_codeWindowAddress++;
						if (ch == 0xfc) break;
						if (ch > 0xfc) continue;
						if (ch == 0x0e) { /* said() has variable number of args */
							ch = *_codeWindowAddress++;

							*data += (ch << 1);
						}
						else {
							*data += testCommands[ch].numArgs;
						}
					}
				}
			}
			else {
				if (!orMode) stillProcessing = FALSE;
			}
			break;
}
		RAM_BANK = previousBank;

	}

#ifdef DEBUG
	drawBigString(screen, "test is false            ", 0, 400, 0, 7);
	if ((readkey() & 0xff) == 'q') closedown();
#endif

	/* Test is false. */
	while (TRUE) {
		ch = *_codeWindowAddress++;
		if (ch == 0xff) {
			b1 = *_codeWindowAddress++;
			b2 = *_codeWindowAddress++;
			disp = (b2 << 8) | b1;  /* Should be signed 16 bit */
			*data += disp;
			break;
		}
		if (ch >= 0xfc) continue;
		if (ch == 0x0e) {
			ch = *_codeWindowAddress++;
			*data += (ch << 1);
		}
		else {
			*data += testCommands[ch].numArgs;
		}
	}
}

/***************************************************************************
** executeLogic
**
** Purpose: To execute the logic code for the logic with the given number.
***************************************************************************/
void executeLogic(int logNum)
{
	byte previousRamBank = RAM_BANK;
	boolean discardAfterward = FALSE, stillExecuting = TRUE;
	byte* code, * endPos, * startPos;
	byte codeAtTimeOfLastBankSwitch;
	LOGICEntry currentLogic;
	LOGICFile currentLogicFile;
	byte codeWindow[CODE_WINDOW_SIZE];
	byte instructionCodeBank;
	boolean lastCodeWasNonWindow = FALSE;
	unsigned int clockVal;
	unsigned int b1, b2, b3, bWhole, bInner;

	//Temp

	short int disp;
	char debugString[80];
	int i, dummy;

	currentLog = logNum;

	RAM_BANK = LOGIC_ENTRY_BANK;
	currentLogic = logics[logNum];

	RAM_BANK = LOGIC_FILE_BANK;
	currentLogicFile = *currentLogic.data;

#ifdef VERBOSE_SCRIPT_START
	printf("ex s. %d counter %lu\n", logNum, opCounter);
#endif // VERBOSE_SCRIPT_START


#ifdef DEBUG
	sprintf(debugString, "LOGIC.%d:       ", currentLog);
	drawBigString(screen, debugString, 0, 384, 0, 7);
#endif

	/* Load logic file temporarily in order to execute it if the logic is
	** not already in memory. */
	if (!currentLogic.loaded) {
		discardAfterward = TRUE;

		RAM_BANK = LOGIC_CODE_BANK;



		b8LoadLogicFile(logNum);

		RAM_BANK = LOGIC_ENTRY_BANK;
		currentLogic = logics[logNum];

		RAM_BANK = LOGIC_FILE_BANK;
		currentLogicFile = *currentLogic.data;
	}
#ifdef DEBUG
	debugString[0] = 0;
	for (i = 0; i < 10; i++)
		sprintf(debugString, "%s %x", debugString, currentLogicFile->logicCode[i]);
	drawBigString(screen, debugString, 0, 416, 0, 7);
#endif
	/* Set up position to start executing code from. */
	//currentLogic.currentPoint = currentLogic.entryPoint;
	startPos = currentLogicFile.logicCode;
	code = startPos + currentLogic.entryPoint;
	endPos = startPos + currentLogicFile.codeSize;

#ifdef DEBUG
	drawBigString(screen, "Push a key to advance a step", 0, 400, 0, 7);
	if ((readkey() & 0xff) == 'q') closedown();
#endif

	RAM_BANK = currentLogicFile.codeBank;

	bWhole = RDTIM;
	
	printf("\n");

	printf("--LogNum-- %d\n", logNum);

	while ((code < endPos) && stillExecuting) {
		bInner = RDTIM;
		b1 = RDTIM;

		printf(" %u ", *code);

		memcpy(&codeWindow[0], code, CODE_WINDOW_SIZE);

		_codeWindowAddress = &codeWindow[0] + 1;

		/* Emergency exit */
		if (key[KEY_F12]) {
			////lprintf("info: Exiting MEKA due to F12, logic: %d, posn: %d",
				//logNum, currentLogic.currentPoint);
			exit(0);
		}

		currentLogic.currentPoint = (code - startPos);

		clockVal = RDTIM - b1;
		//printf("Load Timer 1 %u (%u - %u) Code %u \n", clockVal, RDTIM, b1, code);

		b2 = RDTIM;

#ifdef DEBUG
		debugString[0] = 0;
		for (i = 0; i < 10; i++)
			sprintf(debugString, "%s %x", debugString, code[i]);
		drawBigString(screen, debugString, 0, 416, 0, 7);

		if (*code < 0xfc) {
			sprintf(debugString, "(%d) %s [%x]           ", currentLogic.currentPoint, agiCommands[*code].commandName, *code);
			drawBigString(screen, debugString, 0, 400, 0, 7);
			if ((readkey() & 0xff) == 'q') closedown();
		}
#endif  
#ifdef VERBOSE_LOGIC_EXEC
		printf("\n The code is %d, on bank %d address, %p log num %d\n", *code, RAM_BANK, code, logNum);
#endif // VERBOSE
		codeAtTimeOfLastBankSwitch = *code;
		instructionCodeBank = getBankBasedOnCode(codeAtTimeOfLastBankSwitch);

#ifdef VERBOSE_PRINT_COUNTER
		printf("%d %d %d\n", printCounter + 1, codeAtTimeOfLastBankSwitch, currentLog);
#endif // VERBOSE_PRINT_COUNTER
#ifdef VERBOSE_LOGIC_EXEC
		printf("Bank is now %d to execute code %d \n", RAM_BANK, codeAtTimeOfLastBankSwitch);
#endif // VERBOSE 
		printCounter++;



		if (*code < 0xfe)
		{
			code++;
			RAM_BANK = INSTRUCTION_HANDLER_BANK;
			stillExecuting = b5instructionHandler(codeAtTimeOfLastBankSwitch, &currentLog, logNum, instructionCodeBank);
			RAM_BANK = currentLogicFile.codeBank;

			clockVal = RDTIM - b2;
			//printf("Load Timer 2a %u (%u - %u) Code %u \n", clockVal, RDTIM, b2, codeAtTimeOfLastBankSwitch);
		}
		else {

			switch (codeAtTimeOfLastBankSwitch) {
			case 0xfe: /* Unconditional branch: else, goto. */
				code++;
#ifdef DEBUG
				sprintf(debugString, "(%d) else                           ", currentLogic.currentPoint);
				drawBigString(screen, debugString, 0, 400, 0, 7);
				if ((readkey() & 0xff) == 'q') closedown();
#endif
				lastCodeWasNonWindow = TRUE;
				b1 = *code++;
				b2 = *code++;
				disp = (b2 << 8) | b1;  /* Should be signed 16 bit */
				code += disp;
				break;

			case 0xff: /* Conditional branch: if */
				code++;
#ifdef DEBUG
				sprintf(debugString, "(%d) if                             ", currentLogic.currentPoint);
				drawBigString(screen, debugString, 0, 400, 0, 7);
				if ((readkey() & 0xff) == 'q') closedown();
#endif
				lastCodeWasNonWindow = TRUE;
				ifHandler(&code, currentLogicFile.codeBank);
				break;

			default:    /* Error has occurred */
				////lprintf("catastrophe: Illegal action [%d], logic %d, posn %d.",
					//*(code - 1), logNum, currentLogic.currentPoint);
				break;
	}

			clockVal = RDTIM - b2;
			//printf("Load Timer 2b %u Code (%u - %u) %u \n", clockVal, RDTIM, b2, codeAtTimeOfLastBankSwitch);
}

		RAM_BANK = currentLogicFile.codeBank;

		b3 = RDTIM;

		if (!lastCodeWasNonWindow)
		{
#ifdef VERBOSE_LOGIC_EXEC
			printf("Now jumping (%p - %p - 1) = %p \n", _codeWindowAddress, &codeWindow[0], (_codeWindowAddress - &codeWindow[0] - 1));
#endif // VERBOSE

			code += (_codeWindowAddress - &codeWindow[0]) - 1;
		}

		lastCodeWasNonWindow = FALSE;

		opCounter++;

		clockVal = (RDTIM - b3);
		//printf("Load Timer 3 %u (%u - %u) Code %u \n", clockVal, RDTIM, b1, codeAtTimeOfLastBankSwitch);
		clockVal = (RDTIM - bInner);
		//printf("Load Timer Inner %u (%u - %u) \n", clockVal, RDTIM, bInner);
	}
	printf("\n");

	clockVal = (RDTIM - bWhole);
	printf("Load Timer wHOLE %u (%u - %u) \n", clockVal, RDTIM, bWhole);

	RAM_BANK = previousRamBank;
}

