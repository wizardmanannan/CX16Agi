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
//#define VERBOSE_LOGIC_EXEC
//#define VERBOSE_SCRIPT_START
//#define VERBOSE_PRINT_COUNTER;
//#define VERBOSE_MENU
//#define VERBOSE_MENU_DUMP
//#define VERBOSE_MESSAGE_TEXT
#define VERBOSE_ROOM_CHANGE

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
byte lastRoom = 0;

long opCounter = 0;
int printCounter = 1;

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

boolean b1Equaln(byte** data) // 2, 0x80 
{
	int varVal, value, variable;

	variable = *(*data)++;
	varVal = var[variable];
	value = *(*data)++;

	return (varVal == value);
}

boolean b1Equalv(byte** data) // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*(*data)++];
	varVal2 = var[*(*data)++];
	return (varVal1 == varVal2);
}

boolean b1Lessn(byte** data) // 2, 0x80 
{
	int varVal, value;

	varVal = var[*(*data)++];
	value = *(*data)++;
	return (varVal < value);
}

boolean b1Lessv(byte** data) // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*(*data)++];
	varVal2 = var[*(*data)++];
	return (varVal1 < varVal2);
}

boolean b1Greatern(byte** data) // 2, 0x80 
{
	int varVal, value;

	varVal = var[*(*data)++];
	value = *(*data)++;

	return (varVal > value);
}

boolean b1Greaterv(byte** data) // 2, 0xC0 
{
	int varVal1, varVal2;

	varVal1 = var[*(*data)++];
	varVal2 = var[*(*data)++];
	return (varVal1 > varVal2);
}

boolean b1Isset(byte** data) // 1, 0x00 
{
	int flagNo = *(*data)++;

	if (flagNo == 220 && currentLog == 46)
	{
		printf("Checking whether %d is set and it is %d \n", flagNo, flag[flagNo]);
	}
	return (flag[flagNo]);
}

boolean b1Issetv(byte** data) // 1, 0x80 
{
	return (flag[var[*(*data)++]]);
}

boolean b1Has(byte** data) // 1, 0x00 
{
	return (objects[*(*data)++].roomNum == 255);
}

boolean b1Obj_in_room(byte** data) // 2, 0x40 
{
	int objNum, varNum;

	objNum = *(*data)++;
	varNum = var[*(*data)++];
	return (objects[objNum].roomNum == varNum);
}

boolean b1Posn(byte** data) // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;

	objNum = *(*data)++;
	getViewTab(&localViewtab, objNum);

	x1 = *(*data)++;
	y1 = *(*data)++;
	x2 = *(*data)++;
	y2 = *(*data)++;

	return ((localViewtab.xPos >= x1) && (localViewtab.yPos >= y1)
		&& (localViewtab.xPos <= x2) && (localViewtab.yPos <= y2));
}

boolean b1Controller(byte** data) // 1, 0x00 
{
	int eventNum = *(*data)++, retVal = 0;

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

boolean b1Said(byte** data)
{
	int numOfArgs, wordNum, argValue;
	boolean wordsMatch = TRUE;
	byte argLo, argHi;

	numOfArgs = *(*data)++;

	if ((flag[2] == 0) || (flag[4] == 1)) {  /* Not valid input waiting */
		*data += (numOfArgs * 2); /* Jump over arguments */
		return FALSE;
	}

	/* Needs to deal with ANYWORD and ROL */
	for (wordNum = 0; wordNum < numOfArgs; wordNum++) {
		argLo = *(*data)++;
		argHi = *(*data)++;
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

boolean b1Compare_strings(byte** data) // 2, 0x00 
{
	int s1, s2;

	s1 = *(*data)++;
	s2 = *(*data)++;
	if (strcmp(string[s1], string[s2]) == 0) return TRUE;
	return FALSE;
}

boolean b1Obj_in_box(byte** data) // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;
	objNum = *(*data)++;
	x1 = *(*data)++;
	y1 = *(*data)++;
	x2 = *(*data)++;
	y2 = *(*data)++;

	getViewTab(&localViewtab, objNum);

	return ((localViewtab.xPos >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + localViewtab.xsize - 1) <= x2) &&
		(localViewtab.yPos <= y2));
}

boolean b1Center_posn(byte** data) // 5, 0x00 }
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;
	objNum = *(*data)++;
	x1 = *(*data)++;
	y1 = *(*data)++;
	x2 = *(*data)++;
	y2 = *(*data)++;

	getViewTab(&localViewtab, objNum);

	return (((localViewtab.xPos + (localViewtab.xsize / 2)) >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + (localViewtab.xsize / 2)) <= x2) &&
		(localViewtab.yPos <= y2));
}

boolean b1Right_posn(byte** data) // 5, 0x00
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;

	objNum = *(*data)++;

	getViewTab(&localViewtab, objNum);

	x1 = *(*data)++;
	y1 = *(*data)++;
	x2 = *(*data)++;
	y2 = *(*data)++;

	return (((localViewtab.xPos + localViewtab.xsize - 1) >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + localViewtab.xsize - 1) <= x2) &&
		(localViewtab.yPos <= y2));
}



/* ACTION COMMANDS */

void b1Increment(byte** data) // 1, 0x80 
{
	if (var[*(*data)] < 0xFF)
		var[*(*data)]++;
	(*data)++;

	/* var[*(*data)++]++;  This one doesn't check bounds */
}

void b1Decrement(byte** data) // 1, 0x80 
{
	if (var[*(*data)] > 0)
		var[*(*data)]--;
	(*data)++;

	/* var[*(*data)++]--;  This one doesn't check bounds */
}

void b1Assignn(byte** data) // 2, 0x80 
{
	int varNum, value;

	varNum = *(*data)++;
	value = *(*data)++;
	var[varNum] = value;
}

void b1Assignv(byte** data) // 2, 0xC0 
{
	int var1, var2;

	var1 = *(*data)++;
	var2 = *(*data)++;
	var[var1] = var[var2];
}

void b1Addn(byte** data) // 2, 0x80 
{
	int varNum, value;

	varNum = *(*data)++;
	value = *(*data)++;
	var[varNum] += value;
}

void b1Addv(byte** data) // 2, 0xC0 
{
	int var1, var2;

	var1 = *(*data)++;
	var2 = *(*data)++;
	var[var1] += var[var2];
}

void b1Subn(byte** data) // 2, 0x80 
{
	int varNum, value;

	varNum = *(*data)++;
	value = *(*data)++;
	var[varNum] -= value;
}

void b1Subv(byte** data) // 2, 0xC0 
{
	int var1, var2;

	var1 = *(*data)++;
	var2 = *(*data)++;
	var[var1] -= var[var2];
}

void b1Lindirectv(byte** data) // 2, 0xC0 
{
	int var1, var2;

	var1 = *(*data)++;
	var2 = *(*data)++;
	var[var[var1]] = var[var2];
}

void b1Rindirect(byte** data) // 2, 0xC0 
{
	int var1, var2;

	var1 = *(*data)++;
	var2 = *(*data)++;
	var[var1] = var[var[var2]];
}

void b1Lindirectn(byte** data) // 2, 0x80 
{
	int varNum, value;

	varNum = *(*data)++;
	value = *(*data)++;
	var[var[varNum]] = value;
}

void b1Set(byte** data) // 1, 0x00 
{
	flag[*(*data)++] = TRUE;
}

void b1Reset(byte** data) // 1, 0x00 
{
	flag[*(*data)++] = FALSE;
}

void b1Toggle(byte** data) // 1, 0x00 
{
	int f = *(*data)++;

	flag[f] = (flag[f] ? FALSE : TRUE);
}

void b1Set_v(byte** data) // 1, 0x80 
{
	flag[var[*(*data)++]] = TRUE;
}

void b1Reset_v(byte** data) // 1, 0x80 
{
	flag[var[*(*data)++]] = FALSE;
}

void b1Toggle_v(byte** data) // 1, 0x80 
{
	int f = var[*(*data)++];

	flag[f] = (flag[f] ? FALSE : TRUE);
}

void b1New_room(byte** data) // 1, 0x00 
{
	/* This function is handled in meka.c */
	newRoomNum = *(*data)++;
	hasEnteredNewRoom = TRUE;
}

void b1New_room_v(byte** data) // 1, 0x80 
{
	/* This function is handled in meka.c */
	newRoomNum = var[*(*data)++];
	hasEnteredNewRoom = TRUE;
}

void b1Load_logics(byte** data) // 1, 0x00 
{
	trampoline_1Int(&b8LoadLogicFile, *(*data)++, LOGIC_CODE_BANK);
}

void b1Load_logics_v(byte** data) // 1, 0x80 
{
	trampoline_1Int(&b8LoadLogicFile, var[*(*data)++], LOGIC_CODE_BANK);
}

void b1Call(byte** data) // 1, 0x00 
{
	executeLogic(*(*data)++);
}

void b1Call_v(byte** data) // 1, 0x80 
{
	executeLogic(var[*(*data)++]);
}

void b1Load_pic(byte** data) // 1, 0x80 
{
	loadPictureFile(var[*(*data)++]);
}

void b1Draw_pic(byte** data) // 1, 0x80 
{
	int pNum;

	pNum = var[*(*data)++];
	//picFNum = pNum;  // Debugging. Delete at some stage!!!
	drawPic(loadedPictures[pNum].data, loadedPictures[pNum].size, TRUE);
}

void b1Show_pic(byte** data) // 0, 0x00 
{
	okToShowPic = TRUE;   /* Says draw picture with next object update */
	/*stretch_blit(picture, working_screen, 0, 0, 160, 168, 0, 20, 640, 336);*/
	showPicture();
}

void b1Discard_pic(byte** data) // 1, 0x80 
{
	discardPictureFile(var[*(*data)++]);
}

void b1Overlay_pic(byte** data) // 1, 0x80 
{
	int pNum;

	pNum = var[*(*data)++];
	drawPic(loadedPictures[pNum].data, loadedPictures[pNum].size, FALSE);
}

void b1Show_pri_screen(byte** data) // 0, 0x00 
{
	//showPriority();
	showDebugPri();
	//getch();
	//while (!keypressed()) { /* Wait for key */ }
}

/************************** VIEW ACTION COMMANDS **************************/

void b1Load_view(byte** data) // 1, 0x00 
{
	trampoline_1Int(&b9LoadViewFile, (*(*data)++), VIEW_CODE_BANK_1);
}

void b1Load_view_v(byte** data) // 1, 0x80 
{
	trampoline_1Int(&b9LoadViewFile, var[*(*data)++], VIEW_CODE_BANK_1);
}

void b1Discard_view(byte** data) // 1, 0x00 
{
	trampoline_1Int(&b9DiscardView, *(*data)++, VIEW_CODE_BANK_1);
}

void b1Animate_obj(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewTab;

	entryNum = *(*data)++;
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

void b1Unanimate_all(byte** data) // 0, 0x00 
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

void b1Draw(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= (DRAWN | UPDATE);   /* Not sure about update */



	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, localViewtab.currentCel, VIEW_CODE_BANK_1);

	trampoline_1Int(&bADrawObject, entryNum, VIEW_CODE_BANK_2);

	setViewTab(&localViewtab, entryNum);
}

void b1Erase(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags &= ~DRAWN;

	setViewTab(&localViewtab, entryNum);
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM02")

void b2Position(byte** data) // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = *(*data)++;
	localViewtab.yPos = *(*data)++;

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
}

void b2Position_v(byte** data) // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[*(*data)++];
	localViewtab.yPos = var[*(*data)++];

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
}

void b2Get_posn(byte** data) // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	var[*(*data)++] = localViewtab.xPos;
	var[*(*data)++] = localViewtab.yPos;
}

void b2Reposition(byte** data) // 3, 0x60 
{
	int entryNum, dx, dy;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	dx = (signed char)var[*(*data)++];
	dy = (signed char)var[*(*data)++];
	localViewtab.xPos += dx;
	localViewtab.yPos += dy;

	setViewTab(&localViewtab, entryNum);
}


void b2Set_view(byte** data) // 2, 0x00 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	viewNum = *(*data)++;

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_view_v(byte** data) // 2, 0x40 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	viewNum = var[*(*data)++];

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	getViewTab(&localViewtab, entryNum);
}

void b2Set_loop(byte** data) // 2, 0x00 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	loopNum = *(*data)++;

	getViewTab(&localViewtab, entryNum);
	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, 0, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_loop_v(byte** data) // 2, 0x40 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	loopNum = var[*(*data)++];

	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, loopNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Fix_loop(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= FIXLOOP;

	setViewTab(&localViewtab, entryNum);

}

void b2Release_loop(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXLOOP;
	setViewTab(&localViewtab, entryNum);
}

void b2Set_cel(byte** data) // 2, 0x00 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	celNum = *(*data)++;

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Set_cel_v(byte** data) // 2, 0x40 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	celNum = var[*(*data)++];

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
}

void b2Last_cel(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);
	varNum = *(*data)++;

	var[varNum] = localViewtab.numberOfCels - 1;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_cel(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);
	varNum = *(*data)++;

	var[varNum] = localViewtab.currentCel;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_loop(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	varNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	var[varNum] = localViewtab.currentLoop;
	setViewTab(&localViewtab, entryNum);
}

void b2Current_view(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	varNum = *(*data)++;
	var[varNum] = localViewtab.currentView;

	setViewTab(&localViewtab, entryNum);
}

void b2Number_of_loops(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	varNum = *(*data)++;
	var[varNum] = localViewtab.numberOfLoops;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_priority(byte** data) // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = *(*data)++;
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_priority_v(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = var[*(*data)++];
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Release_priority(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
}

void b2Get_priority(byte** data) // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	varNum = *(*data)++;
	var[varNum] = localViewtab.priority;

	setViewTab(&localViewtab, entryNum);
}

void b2Stop_update(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~UPDATE;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_update(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= UPDATE;

	setViewTab(&localViewtab, entryNum);
}

void b2Force_update(byte** data) // 1, 0x00 
{
	int entryNum;

	entryNum = *(*data)++;
	/* Do immediate update here. Call update(entryNum) */

	trampoline_1Int(&bAUpdateObj, entryNum, VIEW_CODE_BANK_1);
}

void b2Ignore_horizon(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_horizon(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_horizon(byte** data) // 1, 0x00 
{
	horizon = *(*data)++;
}

void b2Object_on_water(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONWATER;

	setViewTab(&localViewtab, entryNum);
}

void b2Object_on_land(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONLAND;

	setViewTab(&localViewtab, entryNum);
}

void b2Object_on_anything(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~(ONWATER | ONLAND);

	setViewTab(&localViewtab, entryNum);
}

void b2Ignore_objs(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_objs(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
}


void b2Distance(byte** data) // 3, 0x20 
{
	int o1, o2, varNum, x1, y1, x2, y2;
	ViewTable localViewtab1, localViewtab2;

	o1 = *(*data)++;
	o2 = *(*data)++;

	getViewTab(&localViewtab1, o1);
	getViewTab(&localViewtab2, o2);

	varNum = *(*data)++;
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

void b2Stop_cycling(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~CYCLING;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_cycling(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= CYCLING;

	setViewTab(&localViewtab, entryNum);
}

void b2Normal_cycle(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleStatus = 0;

	setViewTab(&localViewtab, entryNum);
}

void b2End_of_loop(byte** data) // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *(*data)++;
	localViewtab.cycleStatus = 1;
	localViewtab.flags |= (UPDATE | CYCLING);

	setViewTab(&localViewtab, entryNum);
}

void b2Reverse_cycle(byte** data) // 1, 0x00
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);
	/* Store the other parameters here */

	localViewtab.cycleStatus = 3;

	setViewTab(&localViewtab, entryNum);
}

void b2Reverse_loop(byte** data) // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *(*data)++;
	localViewtab.cycleStatus = 2;
	localViewtab.flags |= (UPDATE | CYCLING);

	setViewTab(&localViewtab, entryNum);
}

void b2Cycle_time(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleTime = var[*(*data)++];
	setViewTab(&localViewtab, entryNum);
}

void b2Stop_motion(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~MOTION;
	localViewtab.direction = 0;
	localViewtab.motion = 0;

	setViewTab(&localViewtab, entryNum);
}

void b2Start_motion(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= MOTION;
	localViewtab.motion = 0;        /* Not sure about this */

	setViewTab(&localViewtab, entryNum);
}

void b2Step_size(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepSize = var[*(*data)++];

	setViewTab(&localViewtab, entryNum);
}

void b2Step_time(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepTime = var[*(*data)++];

	setViewTab(&localViewtab, entryNum);
}

void b2Move_obj(byte** data) // 5, 0x00 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = *(*data)++;
	localViewtab.param2 = *(*data)++;
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = *(*data)++;
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = *(*data)++;
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Move_obj_v(byte** data) // 5, 0x70 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = var[*(*data)++];
	localViewtab.param2 = var[*(*data)++];
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = var[*(*data)++];
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = *(*data)++;
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Follow_ego(byte** data) // 3, 0x00 
{
	int entryNum, stepVal, flagNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	stepVal = *(*data)++;
	flagNum = *(*data)++;
	localViewtab.param1 = localViewtab.stepSize;
	/* Might need to put 'if (stepVal != 0)' */
	//localViewtab.stepSize = stepVal;
	localViewtab.param2 = flagNum;
	localViewtab.motion = 2;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Wander(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);


	localViewtab.motion = 1;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Normal_motion(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.motion = 0;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
}

void b2Set_dir(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.direction = var[*(*data)++];

	setViewTab(&localViewtab, entryNum);
}

void b2Get_dir(byte** data) // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	var[*(*data)++] = localViewtab.direction;

	setViewTab(&localViewtab, entryNum);
}

void b2Ignore_blocks(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
}

void b2Observe_blocks(byte** data) // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
}

void b2Block(byte** data) // 4, 0x00 
{
	/* Is this used anywhere? - Not implemented at this stage */
	*data += 4;
}

void b2Unblock(byte** data) // 0, 0x00 
{

}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM03")

void b3Get(byte** data) // 1, 00 
{
	objects[*(*data)++].roomNum = 255;
}

void b3Get_v(byte** data) // 1, 0x80 
{
	objects[var[*(*data)++]].roomNum = 255;
}

void b3Drop(byte** data) // 1, 0x00 
{
	objects[*(*data)++].roomNum = 0;
}


void b3Put(byte** data) // 2, 0x00 
{
	int objNum, room;

	objNum = *(*data)++;
	room = *(*data)++;
	objects[objNum].roomNum = room;
}

void b3Put_v(byte** data) // 2, 0x40 
{
	int objNum, room;

	objNum = *(*data)++;
	room = var[*(*data)++];
	objects[objNum].roomNum = room;
}

void b3Get_room_v(byte** data) // 2, 0xC0 
{
	int objNum, room;

	objNum = var[*(*data)++];
	var[*(*data)++] = objects[objNum].roomNum;
}

void b3Load_sound(byte** data) // 1, 0x00 
{
	int soundNum;

	soundNum = *(*data)++;
	loadSoundFile(soundNum);
}

void b3Play_sound(byte** data) // 2, 00  sound() renamed to avoid clash
{
	int soundNum;

	soundNum = *(*data)++;
	soundEndFlag = *(*data)++;
	/* playSound(soundNum); */
	flag[soundEndFlag] = TRUE;
}

void b3Stop_sound(byte** data) // 0, 0x00 
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

void b3Print(byte** data) // 1, 00 
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (*(*data)++) - 1);

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

void b3Print_v(byte** data) // 1, 0x80 
{
	char* tempString = (char*) & GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (var[*(*data)++]) - 1);

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

void b3Display(byte** data) // 3, 0x00 
{
	int row, col, messNum;
	char* tempString = (char*) & GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	col = *(*data)++;
	row = *(*data)++;
	messNum = *(*data)++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	b3ProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	/*lprintf("info: display() %s, fg: %d bg: %d row: %d col: %d",
	   tempString, agi_fg, agi_bg, row, col);*/
}

void b3Display_v(byte** data) // 3, 0xE0 
{
	int row, col, messNum;
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	col = var[*(*data)++];
	row = var[*(*data)++];
	messNum = var[*(*data)++];
	//drawString(picture, logics[currentLog].data->messages[messNum-1],
	//   row*8, col*8, agi_fg, agi_bg);

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	b3ProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	/*lprintf("info: display.v() %s, foreground: %d background: %d",
	   tempString, agi_fg, agi_bg);*/
}

void b3Clear_lines(byte** data) // 3, 0x00 
{
	int boxColour, startLine, endLine;

	startLine = *(*data)++;
	endLine = *(*data)++;
	boxColour = *(*data)++;
	if ((screenMode == AGI_GRAPHICS) && (boxColour > 0)) boxColour = 15;
	boxColour++;
	show_mouse(NULL);
	rectfill(agi_screen, 0, startLine * 16, 639, (endLine * 16) + 15, boxColour);
	show_mouse(screen);
}

void b3Text_screen(byte** data) // 0, 0x00 
{
	screenMode = AGI_TEXT;
	/* Do something else here */
	inputLineDisplayed = FALSE;
	statusLineDisplayed = FALSE;
	clear(screen);
}

void b3Graphics(byte** data) // 0, 0x00 
{
	screenMode = AGI_GRAPHICS;
	/* Do something else here */
	inputLineDisplayed = TRUE;
	statusLineDisplayed = TRUE;
	okToShowPic = TRUE;
	clear(screen);
}

void b3Set_cursor_char(byte** data) // 1, 0x00 
{
	char* temp = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	byte msgNo = (*(*data)++) - 1;
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

void b3Set_text_attribute(byte** data) // 2, 0x00 
{
	agi_fg = (*(*data)++) + 1;
	agi_bg = (*(*data)++) + 1;
}

void b3Shake_screen(byte** data) // 1, 0x00 
{
	(*data)++;  /* Ignore this for now. */
}

void b3Configure_screen(byte** data) // 3, 0x00 
{
	min_print_line = *(*data)++;
	user_input_line = *(*data)++;
	status_line_num = *(*data)++;
}

void b3Status_line_on(byte** data) // 0, 0x00 
{
	statusLineDisplayed = TRUE;
}

void b3Status_line_off(byte** data) // 0, 0x00 
{
	statusLineDisplayed = FALSE;
}

void b3Set_string(byte** data) // 2, 0x00 
{
	int stringNum, messNum;
	char* messagePointer;
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

	stringNum = *(*data)++;
	messNum = *(*data)++;
	messagePointer = getMessagePointer(currentLog, messNum - 1);

	strcpyBanked(string[stringNum - 1], messagePointer, logicFile.messageBank);
}

void b3Get_string(byte** data) // 5, 0x00 
{
	int strNum, messNum, row, col, l;
	char* messagePointer;

	strNum = *(*data)++;
	messNum = *(*data)++;
	col = *(*data)++;
	row = *(*data)++;
	l = *(*data)++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	getString(messagePointer, string[strNum], row, col, l);
}

void b3Word_to_string(byte** data) // 2, 0x00 
{
	int stringNum, wordNum;

	stringNum = *(*data)++;
	wordNum = *(*data)++;
	strcpy(string[stringNum], wordText[wordNum]);
}

void b3Parse(byte** data) // 1, 0x00 
{
	int stringNum;

	stringNum = *(*data)++;
	lookupWords(string[stringNum]);
}

void b3Get_num(byte** data) // 2, 0x40 
{
	int messNum, varNum;
	char temp[80];
	char* messagePointer;

	messNum = *(*data)++;
	varNum = *(*data)++;

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	getString(messagePointer, temp, 1, 23, 3);
	var[varNum] = atoi(temp);
}

void b3Prevent_input(byte** data) // 0, 0x00 
{
	inputLineDisplayed = FALSE;
	/* Do something else here */
}

void b3Accept_input(byte** data) // 0, 0x00 
{
	inputLineDisplayed = TRUE;
	/* Do something else here */
}

void b3Set_key(byte** data) // 3, 0x00 
{
	int asciiCode, scanCode, eventCode;
	char* tempStr = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];

	asciiCode = *(*data)++;
	scanCode = *(*data)++;
	eventCode = *(*data)++;


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

void b3Add_to_pic(byte** data) // 7, 0x00 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;

	viewNum = *(*data)++;
	loopNum = *(*data)++;
	celNum = *(*data)++;
	x = *(*data)++;
	y = *(*data)++;
	priNum = *(*data)++;
	baseCol = *(*data)++;

	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
}

void b3Add_to_pic_v(byte** data) // 7, 0xFE 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;

	viewNum = var[*(*data)++];
	loopNum = var[*(*data)++];
	celNum = var[*(*data)++];
	x = var[*(*data)++];
	y = var[*(*data)++];
	priNum = var[*(*data)++];
	baseCol = var[*(*data)++];

	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
}

void b3Status(byte** data) // 0, 0x00 
{
	/* Inventory */
	// set text mode
	// if flag 13 is set then allow selection and store selection in var[25]
	var[25] = 255;
}

void b3Save_game(byte** data) // 0, 0x00 
{
	/* Not supported yet */
}

void b3Restore_game(byte** data) // 0, 0x00 
{
	/* Not supported yet */
}


void b3Restart_game(byte** data) // 0, 0x00 
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

void b3Show_obj(byte** data) // 1, 0x00 
{
	int objectNum;

	objectNum = *(*data)++;
	/* Not supported yet */
}

void b3Random_num(byte** data) // 3, 0x20  random() renamed to avoid clash
{
	int startValue, endValue;

	startValue = *(*data)++;
	endValue = *(*data)++;
	var[*(*data)++] = (rand() % ((endValue - startValue) + 1)) + startValue;
}

void b3Program_control(byte** data) // 0, 0x00 
{
	controlMode = PROGRAM_CONTROL;
}

void b3Player_control(byte** data) // 0, 0x00 
{
	controlMode = PLAYER_CONTROL;
}

void b3Obj_status_v(byte** data) // 1, 0x80 
{
	int objectNum;

	objectNum = var[*(*data)++];
	/* Not supported yet */

	/* showView(viewtab[objectNum].currentView); */
	trampoline_1Int(&bDShowObjectState, objectNum, VIEW_CODE_BANK_4);
}


void b3Quit(byte** data) // 1, 0x00                     /* 0 args for AGI version 2_089 */
{
	int quitType, ch;

	quitType = ((!oldQuit) ? *(*data)++ : 0);
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

void b3Pause(byte** data) // 0, 0x00 
{
	while (key[KEY_ENTER]) { /* Wait */ }
	printInBoxBig("      Game paused.\nPress ENTER to continue.", -1, -1, 30);
	while (!key[KEY_ENTER]) { /* Wait */ }
	showPicture();
	okToShowPic = TRUE;
}


void b3Echo_line(byte** data) // 0, 0x00 
{

}


void b3Cancel_line(byte** data) // 0, 0x00 
{
	/*currentInputStr[0]=0;
	strPos=0;*/
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM04")

void b4Init_joy(byte** data) // 0, 0x00 
{
	/* Not important at this stage */
}

void b4Version(byte** data) // 0, 0x00 
{
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	printInBoxBig("MEKA AGI Interpreter\n    Version 1.0", -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	showPicture();
	okToShowPic = TRUE;
}

void b4Script_size(byte** data) // 1, 0x00 
{
	(*data)++;  /* Ignore the script size. Not important for this interpreter */
}

void b4Set_game_id(byte** data) // 1, 0x00 
{
	(*data)++;  /* Ignore the game ID. Not important */
}

void b4Log(byte** data) // 1, 0x00 
{
	(*data)++;  /* Ignore log message. Not important */
}

void b4Set_scan_start(byte** data) // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	/* currentPoint is set in executeLogic() */
	logicEntry.entryPoint = logicEntry.currentPoint + 1;
	/* Does it return() at this point, or does it execute to the end?? */

	setLogicEntry(&logicEntry, currentLog);
}


void b4Reset_scan_start(byte** data) // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	logicEntry.entryPoint = 0;

	setLogicEntry(&logicEntry, currentLog);
}

void b4Reposition_to(byte** data) // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = *(*data)++;
	localViewtab.yPos = *(*data)++;

	setViewTab(&localViewtab, entryNum);
}

void b4Reposition_to_v(byte** data) // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = *(*data)++;
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[*(*data)++];
	localViewtab.yPos = var[*(*data)++];

	setViewTab(&localViewtab, entryNum);
}

void b4Trace_on(byte** data) // 0, 0x00 
{
	/* Ignore at this stage */
}

void b4Trace_info(byte** data) // 3, 0x00 
{
	*data += 3;  /* Ignore trace information at this stage. */
}

void b4Print_at(byte** data) // 4, 0x00           /* 3 args for AGI versions before */
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;

	messNum = *(*data)++;
	x = *(*data)++;
	y = *(*data)++;
	l = *(*data)++;
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

void b4Print_at_v(byte** data) // 4, 0x80         /* 2_440 (maybe laterz) */
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;

	messNum = var[*(*data)++];
	x = *(*data)++;
	y = *(*data)++;
	l = *(*data)++;
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

void b4Discard_view_v(byte** data) // 1, 0x80 
{
	trampoline_1Int(&b9DiscardView, var[*(*data)++], VIEW_CODE_BANK_1);
}

void b4Clear_text_rect(byte** data) // 5, 0x00 
{
	int x1, y1, x2, y2, boxColour;

	x1 = *(*data)++;
	y1 = *(*data)++;
	x2 = *(*data)++;
	y2 = *(*data)++;
	boxColour = *(*data)++;
	if ((screenMode == AGI_GRAPHICS) && (boxColour > 0)) boxColour = 15;
	if (screenMode == AGI_TEXT) boxColour = 0;
	show_mouse(NULL);
	rectfill(agi_screen, x1 * 16, y1 * 16, (x2 * 16) + 15, (y2 * 16) + 15, boxColour);
	show_mouse(screen);
}

void b4Set_upper_left(byte** data) // 2, 0x00    (x, y) ??
{
	*data += 2;
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

void b4Set_menu(byte** data) // 1, 0x00 
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

	messNum = *(*data)++;

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

void b4Set_menu_item(byte** data) // 2, 0x00 
{
	int messNum, controllerNum, i;
	MENU childMenu;
	LOGICFile currentLogicFile;

	getLogicFile(&currentLogicFile, currentLog);

	messNum = *(*data)++;
	controllerNum = *(*data)++;

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

void b4Submit_menu(byte** data) // 0, 0x00 
{

}

void b4Enable_item(byte** data) // 1, 0x00 
{
	(*data)++;
}

void b4Disable_item(byte** data) // 1, 0x00 
{
	(*data)++;
}

void b4Menu_input(byte** data) // 0, 0x00 
{
	do_menu(the_menu, 10, 20);
}

void b4Show_obj_v(byte** data) // 1, 0x01 
{
	int objectNum;

	objectNum = var[*(*data)++];
	/* Not supported yet */
}

void b4Open_dialogue(byte** data) // 0, 0x00 
{

}

void b4Close_dialogue(byte** data) // 0, 0x00 
{

}

void b4Mul_n(byte** data) // 2, 0x80 
{
	var[*(*data)++] *= *(*data)++;
}

void b4Mul_v(byte** data) // 2, 0xC0 
{
	var[*(*data)++] *= var[*(*data)++];
}

void b4Div_n(byte** data) // 2, 0x80 
{
	var[*(*data)++] /= *(*data)++;
}

void b4Div_v(byte** data) // 2, 0xC0 
{
	var[*(*data)++] /= var[*(*data)++];
}

void b4Close_window(byte** data) // 0, 0x00 
{

}


#pragma code-name (pop)
#pragma code-name (push, "BANKRAM05")
boolean b5instructionHandler(byte code, int* currentLog, byte logNum, byte** ppCodeWindowAddress, byte bank)
{
	switch (code)
	{
	case 0: /* return */
		return FALSE;
		break;
	case 1: trampoline_1pp(&b1Increment, ppCodeWindowAddress, bank); break;
	case 2: trampoline_1pp(&b1Decrement, ppCodeWindowAddress, bank); break;
	case 3: trampoline_1pp(&b1Assignn, ppCodeWindowAddress, bank); break;
	case 4: trampoline_1pp(&b1Assignv, ppCodeWindowAddress, bank); break;
	case 5: trampoline_1pp(&b1Addn, ppCodeWindowAddress, bank); break;
	case 6: trampoline_1pp(&b1Addv, ppCodeWindowAddress, bank); break;
	case 7: trampoline_1pp(&b1Subn, ppCodeWindowAddress, bank); break;
	case 8: trampoline_1pp(&b1Subv, ppCodeWindowAddress, bank); break;
	case 9: trampoline_1pp(&b1Lindirectv, ppCodeWindowAddress, bank); break;
	case 10: trampoline_1pp(&b1Rindirect, ppCodeWindowAddress, bank); break;
	case 11: trampoline_1pp(&b1Lindirectn, ppCodeWindowAddress, bank); break;
	case 12: trampoline_1pp(&b1Set, ppCodeWindowAddress, bank); break;
	case 13: trampoline_1pp(&b1Reset, ppCodeWindowAddress, bank); break;
	case 14: trampoline_1pp(&b1Toggle, ppCodeWindowAddress, bank); break;
	case 15: trampoline_1pp(&b1Set_v, ppCodeWindowAddress, bank); break;
	case 16: trampoline_1pp(&b1Reset_v, ppCodeWindowAddress, bank); break;
	case 17: trampoline_1pp(&b1Toggle_v, ppCodeWindowAddress, bank); break;
	case 18:
		trampoline_1pp(&b1New_room, ppCodeWindowAddress, bank);
		exitAllLogics = TRUE;
		return FALSE;
		break;
	case 19:
		trampoline_1pp(&b1New_room_v, ppCodeWindowAddress, bank);
		exitAllLogics = TRUE;
		return FALSE;
		break;
	case 20: trampoline_1pp(&b1Load_logics, ppCodeWindowAddress, bank); break;
	case 21: trampoline_1pp(&b1Load_logics_v, ppCodeWindowAddress, bank); break;
	case 22:
		trampoline_1pp(&b1Call, ppCodeWindowAddress, bank);
		/* The currentLog variable needs to be restored */
		*currentLog = logNum;
		if (exitAllLogics) return FALSE;
#ifdef DEBUG
		sprintf(debugString, "LOGIC.%d:       ", currentLog);
		drawBigString(screen, debugString, 0, 384, 0, 7);
#endif
		break;
	case 23:
		trampoline_1pp(&b1Call_v, ppCodeWindowAddress, bank);
		/* The currentLog variable needs to be restored */
		*currentLog = logNum;
		if (exitAllLogics) return FALSE;
#ifdef DEBUG
		sprintf(debugString, "LOGIC.%d:       ", currentLog);
		drawBigString(screen, debugString, 0, 384, 0, 7);
#endif
		break;
	case 24: trampoline_1pp(&b1Load_pic, ppCodeWindowAddress, bank); break;
	case 25: trampoline_1pp(&b1Draw_pic, ppCodeWindowAddress, bank); break;
	case 26: trampoline_1pp(&b1Show_pic, ppCodeWindowAddress, bank); break;
	case 27: trampoline_1pp(&b1Discard_pic, ppCodeWindowAddress, bank); break;
	case 28: trampoline_1pp(&b1Overlay_pic, ppCodeWindowAddress, bank); break;
	case 29: trampoline_1pp(&b1Show_pri_screen, ppCodeWindowAddress, bank); break;
	case 30: trampoline_1pp(&b1Load_view, ppCodeWindowAddress, bank); break;
	case 31: trampoline_1pp(&b1Load_view_v, ppCodeWindowAddress, bank); break;
	case 32: trampoline_1pp(&b1Discard_view, ppCodeWindowAddress, bank); break;
	case 33: trampoline_1pp(&b1Animate_obj, ppCodeWindowAddress, bank); break;
	case 34: trampoline_1pp(&b1Unanimate_all, ppCodeWindowAddress, bank); break;
	case 35: trampoline_1pp(&b1Draw, ppCodeWindowAddress, bank); break;
	case 36: trampoline_1pp(&b1Erase, ppCodeWindowAddress, bank); break;
	case 37: trampoline_1pp(&b2Position, ppCodeWindowAddress, bank); break;
	case 38: trampoline_1pp(&b2Position_v, ppCodeWindowAddress, bank); break;
	case 39: trampoline_1pp(&b2Get_posn, ppCodeWindowAddress, bank); break;
	case 40: trampoline_1pp(&b2Reposition, ppCodeWindowAddress, bank); break;
	case 41: trampoline_1pp(&b2Set_view, ppCodeWindowAddress, bank); break;
	case 42: trampoline_1pp(&b2Set_view_v, ppCodeWindowAddress, bank); break;
	case 43: trampoline_1pp(&b2Set_loop, ppCodeWindowAddress, bank); break;
	case 44: trampoline_1pp(&b2Set_loop_v, ppCodeWindowAddress, bank); break;
	case 45: trampoline_1pp(&b2Fix_loop, ppCodeWindowAddress, bank); break;
	case 46: trampoline_1pp(&b2Release_loop, ppCodeWindowAddress, bank); break;
	case 47: trampoline_1pp(&b2Set_cel, ppCodeWindowAddress, bank); break;
	case 48: trampoline_1pp(&b2Set_cel_v, ppCodeWindowAddress, bank); break;
	case 49: trampoline_1pp(&b2Last_cel, ppCodeWindowAddress, bank); break;
	case 50: trampoline_1pp(&b2Current_cel, ppCodeWindowAddress, bank); break;
	case 51: trampoline_1pp(&b2Current_loop, ppCodeWindowAddress, bank); break;
	case 52: trampoline_1pp(&b2Current_view, ppCodeWindowAddress, bank); break;
	case 53: trampoline_1pp(&b2Number_of_loops, ppCodeWindowAddress, bank); break;
	case 54: trampoline_1pp(&b2Set_priority, ppCodeWindowAddress, bank); break;
	case 55: trampoline_1pp(&b2Set_priority_v, ppCodeWindowAddress, bank); break;
	case 56: trampoline_1pp(&b2Release_priority, ppCodeWindowAddress, bank); break;
	case 57: trampoline_1pp(&b2Get_priority, ppCodeWindowAddress, bank); break;
	case 58: trampoline_1pp(&b2Stop_update, ppCodeWindowAddress, bank); break;
	case 59: trampoline_1pp(&b2Start_update, ppCodeWindowAddress, bank); break;
	case 60: trampoline_1pp(&b2Force_update, ppCodeWindowAddress, bank); break;
	case 61: trampoline_1pp(&b2Ignore_horizon, ppCodeWindowAddress, bank); break;
	case 62: trampoline_1pp(&b2Observe_horizon, ppCodeWindowAddress, bank); break;
	case 63: trampoline_1pp(&b2Set_horizon, ppCodeWindowAddress, bank); break;
	case 64: trampoline_1pp(&b2Object_on_water, ppCodeWindowAddress, bank); break;
	case 65: trampoline_1pp(&b2Object_on_land, ppCodeWindowAddress, bank); break;
	case 66: trampoline_1pp(&b2Object_on_anything, ppCodeWindowAddress, bank); break;
	case 67: trampoline_1pp(&b2Ignore_objs, ppCodeWindowAddress, bank); break;
	case 68: trampoline_1pp(&b2Observe_objs, ppCodeWindowAddress, bank); break;
	case 69: trampoline_1pp(&b2Distance, ppCodeWindowAddress, bank); break;
	case 70: trampoline_1pp(&b2Stop_cycling, ppCodeWindowAddress, bank); break;
	case 71: trampoline_1pp(&b2Start_cycling, ppCodeWindowAddress, bank); break;
	case 72: trampoline_1pp(&b2Normal_cycle, ppCodeWindowAddress, bank); break;
	case 73: trampoline_1pp(&b2End_of_loop, ppCodeWindowAddress, bank); break;
	case 74: trampoline_1pp(&b2Reverse_cycle, ppCodeWindowAddress, bank); break;
	case 75: trampoline_1pp(&b2Reverse_loop, ppCodeWindowAddress, bank); break;
	case 76: trampoline_1pp(&b2Cycle_time, ppCodeWindowAddress, bank); break;
	case 77: trampoline_1pp(&b2Stop_motion, ppCodeWindowAddress, bank); break;
	case 78: trampoline_1pp(&b2Start_motion, ppCodeWindowAddress, bank); break;
	case 79: trampoline_1pp(&b2Step_size, ppCodeWindowAddress, bank); break;
	case 80: trampoline_1pp(&b2Step_time, ppCodeWindowAddress, bank); break;
	case 81: trampoline_1pp(&b2Move_obj, ppCodeWindowAddress, bank); break;
	case 82: trampoline_1pp(&b2Move_obj_v, ppCodeWindowAddress, bank); break;
	case 83: trampoline_1pp(&b2Follow_ego, ppCodeWindowAddress, bank); break;
	case 84: trampoline_1pp(&b2Wander, ppCodeWindowAddress, bank); break;
	case 85: trampoline_1pp(&b2Normal_motion, ppCodeWindowAddress, bank); break;
	case 86: trampoline_1pp(&b2Set_dir, ppCodeWindowAddress, bank); break;
	case 87: trampoline_1pp(&b2Get_dir, ppCodeWindowAddress, bank); break;
	case 88: trampoline_1pp(&b2Ignore_blocks, ppCodeWindowAddress, bank); break;
	case 89: trampoline_1pp(&b2Observe_blocks, ppCodeWindowAddress, bank); break;
	case 90: trampoline_1pp(&b2Block, ppCodeWindowAddress, bank); break;
	case 91: trampoline_1pp(&b2Unblock, ppCodeWindowAddress, bank); break;
	case 92: trampoline_1pp(&b3Get, ppCodeWindowAddress, bank); break;
	case 93: trampoline_1pp(&b3Get_v, ppCodeWindowAddress, bank); break;
	case 94: trampoline_1pp(&b3Drop, ppCodeWindowAddress, bank); break;
	case 95: trampoline_1pp(&b3Put, ppCodeWindowAddress, bank); break;
	case 96: trampoline_1pp(&b3Put_v, ppCodeWindowAddress, bank); break;
	case 97: trampoline_1pp(&b3Get_room_v, ppCodeWindowAddress, bank); break;
	case 98: trampoline_1pp(&b3Load_sound, ppCodeWindowAddress, bank); break;
	case 99: trampoline_1pp(&b3Play_sound, ppCodeWindowAddress, bank); break;
	case 100: trampoline_1pp(&b3Stop_sound, ppCodeWindowAddress, bank); break;
	case 101: trampoline_1pp(&b3Print, ppCodeWindowAddress, bank); break;
	case 102: trampoline_1pp(&b3Print_v, ppCodeWindowAddress, bank); break;
	case 103: trampoline_1pp(&b3Display, ppCodeWindowAddress, bank); break;
	case 104: trampoline_1pp(&b3Display_v, ppCodeWindowAddress, bank); break;
	case 105: trampoline_1pp(&b3Clear_lines, ppCodeWindowAddress, bank); break;
	case 106: trampoline_1pp(&b3Text_screen, ppCodeWindowAddress, bank); break;
	case 107: trampoline_1pp(&b3Graphics, ppCodeWindowAddress, bank); break;
	case 108: trampoline_1pp(&b3Set_cursor_char, ppCodeWindowAddress, bank); break;
	case 109: trampoline_1pp(&b3Set_text_attribute, ppCodeWindowAddress, bank); break;
	case 110: trampoline_1pp(&b3Shake_screen, ppCodeWindowAddress, bank); break;
	case 111: trampoline_1pp(&b3Configure_screen, ppCodeWindowAddress, bank); break;
	case 112: trampoline_1pp(&b3Status_line_on, ppCodeWindowAddress, bank); break;
	case 113: trampoline_1pp(&b3Status_line_off, ppCodeWindowAddress, bank); break;
	case 114: trampoline_1pp(&b3Set_string, ppCodeWindowAddress, bank); break;
	case 115: trampoline_1pp(&b3Get_string, ppCodeWindowAddress, bank); break;
	case 116: trampoline_1pp(&b3Word_to_string, ppCodeWindowAddress, bank); break;
	case 117: trampoline_1pp(&b3Parse, ppCodeWindowAddress, bank); break;
	case 118: trampoline_1pp(&b3Get_num, ppCodeWindowAddress, bank); break;
	case 119: trampoline_1pp(&b3Prevent_input, ppCodeWindowAddress, bank); break;
	case 120: trampoline_1pp(&b3Accept_input, ppCodeWindowAddress, bank); break;
	case 121: trampoline_1pp(&b3Set_key, ppCodeWindowAddress, bank); break;
	case 122: trampoline_1pp(&b3Add_to_pic, ppCodeWindowAddress, bank); break;
	case 123: trampoline_1pp(&b3Add_to_pic_v, ppCodeWindowAddress, bank); break;
	case 124: trampoline_1pp(&b3Status, ppCodeWindowAddress, bank); break;
	case 125: trampoline_1pp(&b3Save_game, ppCodeWindowAddress, bank); break;
	case 126: trampoline_1pp(&b3Restore_game, ppCodeWindowAddress, bank); break;
	case 127: break;
	case 128: trampoline_1pp(&b3Restart_game, ppCodeWindowAddress, bank); break;
	case 129: trampoline_1pp(&b3Show_obj, ppCodeWindowAddress, bank); break;
	case 130: trampoline_1pp(&b3Random_num, ppCodeWindowAddress, bank); break;
	case 131: trampoline_1pp(&b3Program_control, ppCodeWindowAddress, bank); break;
	case 132: trampoline_1pp(&b3Player_control, ppCodeWindowAddress, bank); break;
	case 133: trampoline_1pp(&b3Obj_status_v, ppCodeWindowAddress, bank); break;
	case 134: trampoline_1pp(&b3Quit, ppCodeWindowAddress, bank); break;
	case 135: break;
	case 136: trampoline_1pp(&b3Pause, ppCodeWindowAddress, bank); break;
	case 137: trampoline_1pp(&b3Echo_line, ppCodeWindowAddress, bank); break;
	case 138: trampoline_1pp(&b3Cancel_line, ppCodeWindowAddress, bank); break;
	case 139: trampoline_1pp(&b4Init_joy, ppCodeWindowAddress, bank); break;
	case 140: break;
	case 141: trampoline_1pp(&b4Version, ppCodeWindowAddress, bank); break;
	case 142: trampoline_1pp(&b4Script_size, ppCodeWindowAddress, bank); break;
	case 143: trampoline_1pp(&b4Set_game_id, ppCodeWindowAddress, bank); break;
	case 144: trampoline_1pp(&b4Log, ppCodeWindowAddress, bank); break;
	case 145: trampoline_1pp(&b4Set_scan_start, ppCodeWindowAddress, bank); break;
	case 146: trampoline_1pp(&b4Reset_scan_start, ppCodeWindowAddress, bank); break;
	case 147: trampoline_1pp(&b4Reposition_to, ppCodeWindowAddress, bank); break;
	case 148: trampoline_1pp(&b4Reposition_to_v, ppCodeWindowAddress, bank); break;
	case 149: trampoline_1pp(&b4Trace_on, ppCodeWindowAddress, bank); break;
	case 150: trampoline_1pp(&b4Trace_info, ppCodeWindowAddress, bank); break;
	case 151: trampoline_1pp(&b4Print_at, ppCodeWindowAddress, bank); break;
	case 152: trampoline_1pp(&b4Print_at_v, ppCodeWindowAddress, bank); break;
	case 153: trampoline_1pp(&b4Discard_view_v, ppCodeWindowAddress, bank); break;
	case 154: trampoline_1pp(&b4Clear_text_rect, ppCodeWindowAddress, bank); break;
	case 155: trampoline_1pp(&b4Set_upper_left, ppCodeWindowAddress, bank); break;
	case 156: trampoline_1pp(&b4Set_menu, ppCodeWindowAddress, bank); break;
	case 157: trampoline_1pp(&b4Set_menu_item, ppCodeWindowAddress, bank); break;
	case 158: trampoline_1pp(&b4Submit_menu, ppCodeWindowAddress, bank); break;
	case 159: trampoline_1pp(&b4Enable_item, ppCodeWindowAddress, bank); break;
	case 160: trampoline_1pp(&b4Disable_item, ppCodeWindowAddress, bank); break;
	case 161: trampoline_1pp(&b4Menu_input, ppCodeWindowAddress, bank); break;
	case 162: trampoline_1pp(&b4Show_obj_v, ppCodeWindowAddress, bank); break;
	case 163: trampoline_1pp(&b4Open_dialogue, ppCodeWindowAddress, bank); break;
	case 164: trampoline_1pp(&b4Close_dialogue, ppCodeWindowAddress, bank); break;
	case 165: trampoline_1pp(&b4Mul_n, ppCodeWindowAddress, bank); break;
	case 166: trampoline_1pp(&b4Mul_v, ppCodeWindowAddress, bank); break;
	case 167: trampoline_1pp(&b4Div_n, ppCodeWindowAddress, bank); break;
	case 168: trampoline_1pp(&b4Div_v, ppCodeWindowAddress, bank); break;
	case 169: trampoline_1pp(&b4Close_window, ppCodeWindowAddress, bank); break;
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

	return TRUE;
}

int ifLogicHandlers(byte ch, byte** ppCodeWindowAddress, byte bank)
{

	int result;

#ifdef VERBOSE_LOGIC_EXEC
	printf("If Check %d d1 %d, %d", ch, *(*ppCodeWindowAddress), *(*ppCodeWindowAddress + 1));
#endif // VERBOSE_LOGIC_EXEC

	switch (ch) {
	case 0: result = FALSE; break; /* Should never happen */
	case 1: result = trampoline_1pRetbool(&b1Equaln, ppCodeWindowAddress, bank); break;
	case 2: result = trampoline_1pRetbool(&b1Equalv, ppCodeWindowAddress, bank); break;
	case 3: result = trampoline_1pRetbool(&b1Lessn, ppCodeWindowAddress, bank); break;
	case 4: result = trampoline_1pRetbool(&b1Lessv, ppCodeWindowAddress, bank); break;
	case 5: result = trampoline_1pRetbool(&b1Greatern, ppCodeWindowAddress, bank); break;
	case 6: result = trampoline_1pRetbool(&b1Greaterv, ppCodeWindowAddress, bank); break;
	case 7: result = trampoline_1pRetbool(&b1Isset, ppCodeWindowAddress, bank); break;
	case 8: result = trampoline_1pRetbool(&b1Issetv, ppCodeWindowAddress, bank); break;
	case 9: result = trampoline_1pRetbool(&b1Has, ppCodeWindowAddress, bank); break;
	case 10: result = trampoline_1pRetbool(&b1Obj_in_room, ppCodeWindowAddress, bank); break;
	case 11: result = trampoline_1pRetbool(&b1Posn, ppCodeWindowAddress, bank); break;
	case 12: result = trampoline_1pRetbool(&b1Controller, ppCodeWindowAddress, bank); break;
	case 13: result = trampoline_1pRetbool(&b1Have_key, ppCodeWindowAddress, bank); break;
	case 14: result = trampoline_1pRetbool(&b1Said, ppCodeWindowAddress, bank); break;
	case 15: result = trampoline_1pRetbool(&b1Compare_strings, ppCodeWindowAddress, bank); break;
	case 16: result = trampoline_1pRetbool(&b1Obj_in_box, ppCodeWindowAddress, bank); break;
	case 17: result = trampoline_1pRetbool(&b1Center_posn, ppCodeWindowAddress, bank); break;
	case 18: result = trampoline_1pRetbool(&b1Right_posn, ppCodeWindowAddress, bank); break;
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
	byte* codeWindowAddress;
	byte** ppCodeWindowAddress;
	byte ifHandlerBank;

	ppCodeWindowAddress = &codeWindowAddress;

	RAM_BANK = codeBank;

	while (stillProcessing) {
		ch = *(*data)++;

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
			codeWindowAddress = &codeWindow[0];
			RAM_BANK = IF_LOGIC_HANDLERS_BANK;
			ifHandlerBank = getBankBasedOnCode(ch);

			testVal = ifLogicHandlers(ch, ppCodeWindowAddress, ifHandlerBank);

			RAM_BANK = previousBank;

#ifdef VERBOSE_LOGIC_EXEC

			printf("Data was %p trying to add %p ", data, codeWindowAddress - &codeWindow[0]);
#endif // VERBOSE
			* data += (codeWindowAddress - &codeWindow[0]);

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
						ch = *(*data)++;
						if (ch == 0xfc) break;
						if (ch > 0xfc) continue;
						if (ch == 0x0e) { /* said() has variable number of args */
							ch = *(*data)++;

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
		ch = *(*data)++;
		if (ch == 0xff) {
			b1 = *(*data)++;
			b2 = *(*data)++;
			disp = (b2 << 8) | b1;  /* Should be signed 16 bit */
			*data += disp;
			break;
		}
		if (ch >= 0xfc) continue;
		if (ch == 0x0e) {
			ch = *(*data)++;
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
	byte* code, * endPos, * startPos, b1, b2;
	byte codeAtTimeOfLastBankSwitch;
	LOGICEntry currentLogic;
	LOGICFile currentLogicFile;
	byte codeWindow[CODE_WINDOW_SIZE];
	byte* codeWindowAddress;
	byte** ppCodeWindowAddress;
	byte instructionCodeBank;
	boolean lastCodeWasNonWindow = FALSE;

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

#ifdef VERBOSE_ROOM_CHANGE
	if (var[0] != lastRoom)
	{
		printf("We are at %d, counter %lu\n", var[0], opCounter);
}
	lastRoom = var[0];
#endif


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

	while ((code < endPos) && stillExecuting) {

		memcpy(&codeWindow[0], code, CODE_WINDOW_SIZE);

		codeWindowAddress = &codeWindow[0] + 1;
		ppCodeWindowAddress = &codeWindowAddress;

		/* Emergency exit */
		if (key[KEY_F12]) {
			////lprintf("info: Exiting MEKA due to F12, logic: %d, posn: %d",
				//logNum, currentLogic.currentPoint);
			exit(0);
		}

		currentLogic.currentPoint = (code - startPos);

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
			stillExecuting = b5instructionHandler(codeAtTimeOfLastBankSwitch, &currentLog, logNum, ppCodeWindowAddress, instructionCodeBank);
			RAM_BANK = currentLogicFile.codeBank;
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
}

		RAM_BANK = currentLogicFile.codeBank;

		if (!lastCodeWasNonWindow)
		{
#ifdef VERBOSE_LOGIC_EXEC
			printf("Now jumping (%p - %p - 1) = %p \n", codeWindowAddress, &codeWindow[0], (codeWindowAddress - &codeWindow[0] - 1));
#endif // VERBOSE
			code += (codeWindowAddress - &codeWindow[0]) - 1;
		}

		lastCodeWasNonWindow = FALSE;

		opCounter++;
	}

	RAM_BANK = previousRamBank;
}

