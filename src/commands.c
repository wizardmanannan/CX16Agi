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


#include <cx16.h>

#include "commands.h"
#include "general.h"
#include "logic.h"
#include "memoryManager.h"
#include "view.h"
#include "stub.h"
#include "helpers.h"
#include "parser.h"

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
//#define VERBOSE_GOTO
//#define VERBOSE_ROOM_CHANGE
//#define VERBOSE_MESSAGE_PRINT

#pragma rodata-name (push, "BANKRAM04")
const char B4_QUIT_MESSAGE[] = "Press ENTER to quit. Press ESC to keep playing.";
const char B4_PAUSE_MESSAGE[] = "      Game paused.\nPress ENTER to continue.";
const char B4_MEKA_MESSAGE[] = "MEKA AGI Interpreter\n    Version 1.0";
const char B4_VERSION_MESSAGE[] = "MEKA AGI Interpreter\n    Version 1.0";
#pragma rodata-name (pop)

extern byte* var;
extern boolean* flag;

extern int newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern byte horizon;
extern int controlMode;

//extern int picFNum;   // Just a debug variable. Delete at some stage!!

int currentLog, agi_bg = 1, agi_fg = 16;
extern char cursorChar;
boolean oldQuit = FALSE;


int numOfMenus = 0;


#pragma bss-name (push, "BANKRAM05")
MENU the_menu[MAX_MENUS];
MENU the_menuChildren[MAX_MENU_CHILDREN];
#pragma bss-name (pop)


int printCounter = 1;
byte lastRoom = 0;

void executeLogic(LOGICEntry* logicEntry, int logNum);

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

#pragma wrapped-call (push, trampoline, MENU_BANK)
#pragma code-name (push, "BANKRAM05");
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

void b5SetMenu(MENU* menu, byte menuNo)
{
#ifdef VERBOSE_MENU
	printf("-- Adding menu %p at position %d dp %p flags %d proc %p address %p \n", menu, menuNo, menu->dp, menu->flags, menu->proc, menu->text);
#endif // VERBOSE_MENU

	the_menu[menuNo] = *menu;
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
#pragma wrapped-call (pop)
#pragma code-name (pop);

char* getMessagePointer(byte logicFileNo, byte messageNo)
{
	byte previousBank = RAM_BANK;
	char* result;
	int i;

	LOGICFile logicFile;
	getLogicFile(&logicFile, logicFileNo);

	RAM_BANK = logicFile.messageBank;

	result = (char*)logicFile.messages[messageNo];

	//for (i = 1; i < strlen(result) && *result == ' '; i++) //Skip tabbing
	//{
	//	result = (char*)logicFile.messages[messageNo + i];
	//}

#ifdef VERBOSE_MESSAGE_PRINT
	printf("Attempting to print message %d from address %p, from logic %d. The length is %d\n", messageNo, result, logicFileNo, strlen(result));
#endif // VERBOSE_MESSAGE_PRINT
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

#define PROCESS_STRING_BANK 3

#pragma code-name (push, "BANKRAM0F");
/****************************************************************************
** addLogLine
****************************************************************************/
//void b1AddLogLine(char* message)
//{
//	FILE* fp;
//
//	if ((fp = fopen("log.txt", "a")) == NULL) {
//#ifdef VERBOSE_LOGIC_EXEC
//		fprintf(stderr, "Error opening log file.");
//#endif // VERBOSE
//		return;
//	}
//
//	fprintf(fp, "%s\n", message);
//
//	fclose(fp);
//}

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

boolean b1Has() // 1, 0x00 
{
	objectType objectType;

	bFGetObject(loadAndIncWinCode(), &objectType);

	return (objectType.roomNum == 255);
}

boolean b1Obj_in_room() // 2, 0x40 
{
	int objNum, varNum;
	objectType objectType;

	objNum = loadAndIncWinCode();
	bFGetObject(objNum, &objectType);

	varNum = var[loadAndIncWinCode()];
	return (objectType.roomNum == varNum);
}

boolean b1Posn() // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;

	objNum = loadAndIncWinCode();
	getViewTab(&localViewtab, objNum);

	x1 = loadAndIncWinCode();
	y1 = loadAndIncWinCode();
	x2 = loadAndIncWinCode();
	y2 = loadAndIncWinCode();

	return ((localViewtab.xPos >= x1) && (localViewtab.yPos >= y1)
		&& (localViewtab.xPos <= x2) && (localViewtab.yPos <= y2));
}

boolean b1Controller() // 1, 0x00 
{
	EventType event;
	int eventNum = loadAndIncWinCode(), retVal = 0;

	//b7GetEvent(&event, eventNum);

	///* Some events can be activated by menu input or key input. */

	///* Following code detects key presses at the current time */
	//switch (event.type) {
	//case ASCII_KEY_EVENT:
	//	if (event.activated) {
	//		event.activated = FALSE;
	//		return TRUE;
	//	}
	//	return b7GetAsciiState(event.eventID);
	//case SCAN_KEY_EVENT:
	//	if (event.activated) {
	//		event.activated = FALSE;
	//		return TRUE;
	//	}
	//	if ((event.eventID < 59) &&
	//		(b7GetAsciiState(0) == 0)) return FALSE;   /* ALT Combinations */
	//	return (b7GetKeyState(event.eventID));
	//case MENU_EVENT:
	//	retVal = event.activated;
	//	event.activated = 0;
	//	return (retVal);
	//default:
	return FALSE;
}

boolean b1Have_key() // 0, 0x00
{
	byte key;

	key = var[19];

	if (!key)
	{
		GET_IN(key);
		var[19] = key;
	}

	return key;
}

boolean b1Said()
{
	int numOfArgs, wordNum, argValue;
	boolean wordsMatch = TRUE;
	byte argLo, argHi;

	numOfArgs = loadAndIncWinCode();

	if ((flag[2] == 0) || (flag[4] == 1)) {  /* Not valid input waiting */
		incCodeBy(numOfArgs * 2); /* Jump over arguments */
		return FALSE;
	}

	/* Needs to deal with ANYWORD and ROL */
	for (wordNum = 0; wordNum < numOfArgs; wordNum++) {
		argLo = loadAndIncWinCode();
		argHi = loadAndIncWinCode();
		argValue = (argLo + (argHi << 8));
		if (argValue == 9999) break; /* Should always be last argument */
		if (argValue == 1) continue; /* Word comparison does not matter */
		if (b7GetInputWord(wordNum) != argValue) wordsMatch = FALSE;
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

	s1 = loadAndIncWinCode();
	s2 = loadAndIncWinCode();
	if (b7Strcmp(string[s1], string[s2]) == 0) return TRUE;
	return FALSE;
}

boolean b1Obj_in_box() // 5, 0x00 
{
	int objNum, x1, y1, x2, y2;
	ViewTable localViewtab;
	objNum = loadAndIncWinCode();
	x1 = loadAndIncWinCode();
	y1 = loadAndIncWinCode();
	x2 = loadAndIncWinCode();
	y2 = loadAndIncWinCode();

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
	objNum = loadAndIncWinCode();
	x1 = loadAndIncWinCode();
	y1 = loadAndIncWinCode();
	x2 = loadAndIncWinCode();
	y2 = loadAndIncWinCode();

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

	objNum = loadAndIncWinCode();

	getViewTab(&localViewtab, objNum);

	x1 = loadAndIncWinCode();
	y1 = loadAndIncWinCode();
	x2 = loadAndIncWinCode();
	y2 = loadAndIncWinCode();

	return (((localViewtab.xPos + localViewtab.xsize - 1) >= x1) &&
		(localViewtab.yPos >= y1) &&
		((localViewtab.xPos + localViewtab.xsize - 1) <= x2) &&
		(localViewtab.yPos <= y2));
}



/* ACTION COMMANDS */


#pragma code-name (pop)
#pragma code-name (push, "BANKRAM02")

void b2Load_logics() // 1, 0x00 
{
	b6LoadLogicFile(loadAndIncWinCode());

	return;
}

void b2Load_logics_v() // 1, 0x80 
{
	b6LoadLogicFile(var[loadAndIncWinCode()]);

	return;
}



void b2Load_pic() // 1, 0x80 
{
	b6LoadPictureFile(var[loadAndIncWinCode()]);
	return;
}

void b2Draw_pic() // 1, 0x80 
{
	int pNum;
	int pLen;
	PictureFile loadedPicture;
	pNum = var[loadAndIncWinCode()];

	getLoadedPicture(&loadedPicture, pNum);

	//picFNum = pNum;  // Debugging. Delete at some stage!!!

	b11DrawPic(loadedPicture.data, loadedPicture.size, TRUE, pNum);
	bAResetSpriteMemory(TRUE);

	return;
}

void b2Show_pic() // 0, 0x00 
{
	/*stretch_blit(picture, working_screen, 0, 0, 160, 168, 0, 20, 640, 336);*/
	b6ShowPicture();

	return;
}

void b2Discard_pic() // 1, 0x80 
{
	b6DiscardPictureFile(var[loadAndIncWinCode()]);

	return;
}

void b2Overlay_pic() // 1, 0x80 
{
	int pNum;
	PictureFile loadedPicture;
	pNum = var[loadAndIncWinCode()];

	getLoadedPicture(&loadedPicture, pNum);

	b11DrawPic(loadedPicture.data, loadedPicture.size, FALSE, pNum);
	bAResetSpriteMemory(TRUE);

	return;
}

void b2Show_pri_screen() // 0, 0x00 
{
	return;
}

/************************** VIEW ACTION COMMANDS **************************/

void b2Load_view() // 1, 0x00 
{
	b9LoadViewFile(loadAndIncWinCode());
	return;
}

void b2Load_view_v() // 1, 0x80 
{
	b9LoadViewFile(var[loadAndIncWinCode()]);
	return;
}

void b2Discard_view() // 1, 0x00 
{
	b9DiscardView(loadAndIncWinCode());
	return;
}

void b2Animate_obj() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewTab;

	entryNum = loadAndIncWinCode();
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

	localViewTab.repositioned = FALSE;
	localViewTab.stopped = FALSE;
	localViewTab.noAdvance = FALSE;

	setViewTab(&localViewTab, entryNum);

	getViewTab(&localViewTab, entryNum);




	return;
}

void b2Unanimate_all() // 0, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	/* Mark all objects as unanimated and not drawn */
	for (entryNum = 0; entryNum < VIEW_TABLE_SIZE; entryNum++)
	{
		getViewTab(&localViewtab, entryNum);

		localViewtab.flags &= ~(ANIMATED | DRAWN);

		setViewTab(&localViewtab, entryNum);
	}
	return;
}

#pragma wrapped-call (push, trampoline, POSITION_HELPERS_BANK)
extern void b9FindPosition(ViewTable* localViewTab, byte entryNum);
#pragma wrapped-call (pop)

void b2Draw() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	if (!(localViewtab.flags & DRAWN))
	{
		localViewtab.previousX = localViewtab.xPos;
		localViewtab.previousY = localViewtab.yPos;


		localViewtab.flags |= (DRAWN | UPDATE);   /* Not sure about update */


		b9FindPosition(&localViewtab, entryNum);

		//bAFindPosition(entryNum, &localViewtab);

		localViewtab.noAdvance = FALSE;

		setViewTab(&localViewtab, entryNum);
	}
	return;
}

void b2Erase() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);
	localViewtab.flags &= ~DRAWN;
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Position() // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = loadAndIncWinCode();
	localViewtab.yPos = loadAndIncWinCode();
	localViewtab.previousX = localViewtab.xPos;
	localViewtab.previousY = localViewtab.yPos;

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
	return;
}

void b2Position_v() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[loadAndIncWinCode()];
	localViewtab.yPos = var[loadAndIncWinCode()];
	localViewtab.previousX = localViewtab.xPos;
	localViewtab.previousY = localViewtab.yPos;

	setViewTab(&localViewtab, entryNum);
	/* Need to check that it hasn't been draw()n yet. */
	return;
}

void b2Get_posn() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);


	var[loadAndIncWinCode()] = localViewtab.xPos;
	var[loadAndIncWinCode()] = localViewtab.yPos;

	return;
}

void b2Reposition() // 3, 0x60 
{
	int entryNum, dx, dy;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	dx = (signed char)var[loadAndIncWinCode()];
	dy = (signed char)var[loadAndIncWinCode()];
	localViewtab.xPos += dx;
	localViewtab.yPos += dy;
	localViewtab.repositioned = TRUE;

	setViewTab(&localViewtab, entryNum);
	return;
}


void b2Set_view() // 2, 0x00 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	viewNum = loadAndIncWinCode();

	getViewTab(&localViewtab, entryNum);

	b9SetView(viewNum, entryNum);

	return;
}

void b2Set_view_v() // 2, 0x40 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	viewNum = var[loadAndIncWinCode()];

	getViewTab(&localViewtab, entryNum);

	b9SetView(viewNum, entryNum);

	return;
}

void b2Set_loop() // 2, 0x00 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	loopNum = loadAndIncWinCode();

	getViewTab(&localViewtab, entryNum);
	b9SetLoop(&localViewtab, entryNum, loopNum);

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_loop_v() // 2, 0x40 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	loopNum = var[loadAndIncWinCode()];

	b9SetLoop(&localViewtab, entryNum, loopNum);

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Fix_loop() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= FIXLOOP;

	setViewTab(&localViewtab, entryNum);

	return;
}

void b2Release_loop() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXLOOP;
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_cel() // 2, 0x00 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	celNum = loadAndIncWinCode();

	getViewTab(&localViewtab, entryNum);

	b9SetCel(&localViewtab, entryNum, celNum);


	localViewtab.noAdvance = FALSE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_cel_v() // 2, 0x40 
{
	int entryNum, celNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	celNum = var[loadAndIncWinCode()];

	getViewTab(&localViewtab, entryNum);

	localViewtab.noAdvance = FALSE;

	b9SetCel(&localViewtab, entryNum, celNum);

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Last_cel() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);
	varNum = loadAndIncWinCode();

	var[varNum] = localViewtab.numberOfCels - 1;
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Current_cel() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);
	varNum = loadAndIncWinCode();

	var[varNum] = localViewtab.currentCel;
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Current_loop() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	varNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	var[varNum] = localViewtab.currentLoop;
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Current_view() // 2, 0x40 
{
	int entryNum, varNum, i;
	byte* mem;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	varNum = loadAndIncWinCode();
	var[varNum] = localViewtab.currentView;

	setViewTab(&localViewtab, entryNum);

	return;
}

void b2Number_of_loops() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	varNum = loadAndIncWinCode();
	var[varNum] = localViewtab.numberOfLoops;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_priority() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = loadAndIncWinCode();
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_priority_v() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.priority = var[loadAndIncWinCode()];
	localViewtab.flags |= FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Release_priority() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~FIXEDPRIORITY;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Get_priority() // 2, 0x40 
{
	int entryNum, varNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);


	varNum = loadAndIncWinCode();
	var[varNum] = localViewtab.priority;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Stop_update() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~UPDATE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Start_update() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= UPDATE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Force_update() // 1, 0x00 
{
	int entryNum;

	entryNum = loadAndIncWinCode();
	
	//Will happen automatically the next vblank. The param is ignored

	return;
}

void b2Ignore_horizon() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);


	localViewtab.flags |= IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Observe_horizon() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREHORIZON;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_horizon() // 1, 0x00 
{
	horizon = loadAndIncWinCode();
	return;
}

void b2Object_on_water() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONWATER;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Object_on_land() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= ONLAND;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Object_on_anything() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~(ONWATER | ONLAND);

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Ignore_objs() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Observe_objs() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREOBJECTS;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Distance() // 3, 0x20 
{
	int o1, o2, varNum, x1, y1, x2, y2;
	ViewTable localViewtab1, localViewtab2;

	o1 = loadAndIncWinCode();
	o2 = loadAndIncWinCode();

	getViewTab(&localViewtab1, o1);
	getViewTab(&localViewtab2, o2);

	varNum = loadAndIncWinCode();
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
	return;
}

void b2Stop_cycling() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~CYCLING;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Start_cycling() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= CYCLING;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Normal_cycle() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleStatus = 0;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2End_of_loop() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = loadAndIncWinCode();
	localViewtab.cycleStatus = 1;
	localViewtab.flags |= (UPDATE | CYCLING);
	localViewtab.noAdvance = TRUE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Reverse_cycle() // 1, 0x00
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);
	/* Store the other parameters here */

	localViewtab.cycleStatus = 3;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Reverse_loop() // 2, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = loadAndIncWinCode();
	localViewtab.cycleStatus = 2;
	localViewtab.flags |= (UPDATE | CYCLING);

	localViewtab.noAdvance = TRUE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Cycle_time() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.cycleTime = var[loadAndIncWinCode()];
	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Stop_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~MOTION;
	localViewtab.direction = 0;
	localViewtab.motion = 0;
	localViewtab.staleCounter = TRUE;

	if (entryNum == 0)
	{
		var[6] = 0;
		controlMode = PROGRAM_CONTROL;
	}

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Start_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= MOTION;
	localViewtab.motion = 0;        /* Not sure about this */

	if (entryNum == 0)
	{
		var[6] = 0;
		controlMode = PLAYER_CONTROL;
	}

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Step_size() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepSize = var[loadAndIncWinCode()];

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Step_time() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.stepTime = var[loadAndIncWinCode()];

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Move_obj() // 5, 0x00 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	b9StartMoveObj(&localViewtab, entryNum, loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode());

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Move_obj_v() // 5, 0x70 
{
	int entryNum;
	byte stepVal;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.param1 = var[loadAndIncWinCode()];
	localViewtab.param2 = var[loadAndIncWinCode()];
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = var[loadAndIncWinCode()];
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = loadAndIncWinCode();
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Follow_ego() // 3, 0x00 
{
	int entryNum, dist, flagNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	dist = loadAndIncWinCode();
	flagNum = loadAndIncWinCode();
	localViewtab.param1 = dist > localViewtab.stepSize ? dist : localViewtab.stepSize;
	/* Might need to put 'if (stepVal != 0)' */
	//localViewtab.stepSize = stepVal;
	localViewtab.param2 = flagNum;
	localViewtab.motion = 2;
	localViewtab.param3 = -1;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Wander() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);
	localViewtab.motion = 1;
	localViewtab.flags |= MOTION | UPDATE;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Normal_motion() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.motion = 0;
	localViewtab.flags |= MOTION;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_dir() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.direction = var[loadAndIncWinCode()];

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Get_dir() // 2, 0x40 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	var[loadAndIncWinCode()] = localViewtab.direction;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Ignore_blocks() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Observe_blocks() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags &= ~IGNOREBLOCKS;

	setViewTab(&localViewtab, entryNum);
	return;
}

//void b3Block() // 4, 0x00 
//{
//	/* Is this used anywhere? - Not implemented at this stage */
//	*data += 4;
//}
//
//void b3Unblock() // 0, 0x00 
//{
//
//}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM03")

void b3Get() // 1, 00 
{
	objectType objectType;
	byte objNum = loadAndIncWinCode();

	bFGetObject(objNum, &objectType);

	objectType.roomNum = 255;

	bFSetObject(objNum, &objectType);

	return;
}

void b3Get_v() // 1, 0x80 
{
	objectType objectType;
	byte objNum = var[loadAndIncWinCode()];

	bFGetObject(objNum, &objectType);

	objectType.roomNum = 255;

	bFSetObject(objNum, &objectType);

	return;
}

void b3Drop() // 1, 0x00 
{
	objectType objectType;
	byte objNum = loadAndIncWinCode();

	bFGetObject(objNum, &objectType);

	objectType.roomNum = 0;

	bFSetObject(objNum, &objectType);
	return;
}


void b3Put() // 2, 0x00 
{
	int objNum, room;
	objectType objectType;

	objNum = loadAndIncWinCode();
	room = loadAndIncWinCode();

	bFGetObject(objNum, &objectType);

	objectType.roomNum = room;

	bFSetObject(objNum, &objectType);
	return;
}

void b3Put_v() // 2, 0x00 
{
	int objNum, room;
	objectType objectType;

	objNum = loadAndIncWinCode();
	room = var[loadAndIncWinCode()];

	bFGetObject(objNum, &objectType);

	objectType.roomNum = room;

	bFSetObject(objNum, &objectType);
	return;
}

void b3Get_room_v() // 2, 0xC0 
{
	int objNum, room;
	objectType objectType;

	objNum = var[loadAndIncWinCode()];

	bFGetObject(objNum, &objectType);

	var[loadAndIncWinCode()] = objectType.roomNum;
	return;
}

void b3Load_sound() // 1, 0x00 
{
	int soundNum;

	soundNum = loadAndIncWinCode();
	b1LoadSoundFile(soundNum);
	return;
}

void b3Play_sound() // 2, 00  sound() renamed to avoid clash
{
	int soundNum;

	soundNum = loadAndIncWinCode();
	soundEndFlag = loadAndIncWinCode();
	b1PlaySound(soundNum, soundEndFlag);
	return;
}


void b3Stop_sound() // 0, 0x00 
{
	return;
}

boolean b3CharIsIn(char testChar, char* testString)
{
	int i;

	for (i = 0; i < strlen(testString); i++) {
		if (testString[i] == testChar) return TRUE;
	}

	return FALSE;
}

#pragma wrapped-call (push, trampoline, TEXT_CODE_BANK)
//Helper function not a command. Note there is an identical function on bank 4
void b3PrintMessageInTextbox(byte messNum, byte x, byte y, byte length)
{
#define NO_KEYS_TO_WAIT 2

	char* messagePointer;
	byte timeoutFlagVal = var[PRINT_TIMEOUT];
	unsigned int waitTicks;
	unsigned int vSyncToContinueAt;
	LOGICFile logicFile;
	byte keysToWait[NO_KEYS_TO_WAIT] = { KEY_ESC, KEY_ENTER };

	getLogicFile(&logicFile, currentLog);

#ifdef  VERBOSE_MESSAGE_PRINT
	printf("Attempting to display message %d at %d,%d, length %d\n", messNum - 1, x, y, length);
#endif
#ifdef  VERBOSE_MESSAGE_PRINT
	printf("The bank is %d\n", logicFile.messageBank);
#endif


	show_mouse(NULL);
	show_mouse(screen);

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	b3DisplayMessageBox(messagePointer, logicFile.messageBank, y, x, TEXTBOX_PALETTE_NUMBER, length);

	if (timeoutFlagVal)
	{
		waitTicks = timeoutFlagVal * 30;  // The timeout value is given in half seconds and the TotalTicks in 1/60ths of a second.
		vSyncToContinueAt = vSyncCounter + waitTicks;

		while (vSyncCounter != vSyncToContinueAt);
	}
	else
	{
		b5WaitOnSpecificKeys(keysToWait, NO_KEYS_TO_WAIT);
	}

	b3ClearLastPlacedText();

	show_mouse(NULL);

	show_mouse(screen);
	return;
}
#pragma wrapped-call (pop)

void b3Print() // 1, 00 
{
	b3PrintMessageInTextbox(loadAndIncWinCode(), DEFAULT_TEXTBOX_X, DEFAULT_TEXTBOX_Y, DEFAULT_BOX_WIDTH);
}

void b3Print_v() // 1, 0x80 
{
	b3PrintMessageInTextbox(var[loadAndIncWinCode()], DEFAULT_TEXTBOX_X, DEFAULT_TEXTBOX_Y, DEFAULT_BOX_WIDTH);
}

//A helper function not a command
void b3DisplayWithoutTextbox(byte row, byte col, byte messNum)
{
	int i;
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	LOGICFile logicFile;
	getLogicFile(&logicFile, currentLog);

	messagePointer = getMessagePointer(currentLog, messNum - 1);

#ifdef VERBOSE_MESSAGE_PRINT
	printf("attempting to print %d from script %d. The length is %d. The message pointer is %p and the bank is %p\n", messNum, currentLog, strLenBanked(messagePointer, logicFile.messageBank), messagePointer, logicFile.messageBank);
	printf("the messages live at %p on bank %p\n", logicFile.messages, logicFile.messageBank);
#endif
	//b3ProcessString(messagePointer, 0, tempString);
	b3DisplayMessageBox(messagePointer, logicFile.messageBank, row, col, b3SetTextColor(_currentForegroundColour, _currentBackgroundColour), 0);
	return;
}

void b3Display() // 3, 0x00 
{
	b3DisplayWithoutTextbox(loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode());
}

void b3Display_v() // 3, 0xE0 
{
	b3DisplayWithoutTextbox(var[loadAndIncWinCode()], var[loadAndIncWinCode()], var[loadAndIncWinCode()]);
}

void b3Clear_lines() // 3, 0x00 
{
	int boxColour, startLine, endLine;
	byte i, j;
	startLine = loadAndIncWinCode();
	endLine = loadAndIncWinCode();
	boxColour = loadAndIncWinCode();

	show_mouse(NULL);

	b3FillChar(startLine, endLine, 0, TRANSPARENT_CHAR); //When clearing the palette is not important as entry 0 is always transparent

	show_mouse(screen);
}


#pragma code-name (pop)
#pragma code-name (push, "BANKRAM04")

void b4Text_screen() // 0, 0x00 
{
	screenMode = AGI_TEXT;
	/* Do something else here */
	inputLineDisplayed = FALSE;
	statusLineDisplayed = FALSE;
	b6SetAndWaitForIrqState(TEXT_ONLY);

	return;
}

void b4Graphics() // 0, 0x00 
{
	screenMode = AGI_GRAPHICS;
	/* Do something else here */
	inputLineDisplayed = TRUE;
	statusLineDisplayed = TRUE;
	b6SetAndWaitForIrqState(DISPLAY_GRAPHICS);

	return;
}

void b4Set_cursor_char() // 1, 0x00 
{
	char* b7Temp = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	byte msgNo = loadAndIncWinCode() - 1;
	char* messagePointer = getMessagePointer(currentLog, msgNo);
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

#ifdef VERBOSE_STRING_CHECK
	printf("Your msgNo is %d\n", msgNo);
#endif // VERBOSE_STRING_CHECK


	//b3ProcessString(messagePointer, logicFile.messageBank, temp);

	memCpyBanked((byte*)&cursorChar, (byte*)b7Temp, STRING_BANK, 1);

#ifdef VERBOSE_STRING_CHECK
	printf("Your cursor char is %c\n", cursorChar);
#endif

	return;
}

void b4Set_text_attribute() // 2, 0x00 
{
	agi_fg = loadAndIncWinCode();
	agi_bg = loadAndIncWinCode();

#ifdef VERBOSE_MESSAGE_PRINT
	printf("Set foreground: %d background: %d\n", agi_fg, agi_bg);
#endif // VERBOSE_MESSAGE_PRINT

	b3SetTextColor(agi_fg, agi_bg);

	return;
}

//void b4Shake_screen() // 1, 0x00 
//{
//	(*data)++;  /* Ignore this for now. */
//}


void b4Configure_screen() // 3, 0x00 
{
	min_print_line = loadAndIncWinCode();
	user_input_line = loadAndIncWinCode();
	status_line_num = loadAndIncWinCode();
	return;
}

void b4Status_line_on() // 0, 0x00 
{
	statusLineDisplayed = TRUE;
	return;
}

void b4Status_line_off() // 0, 0x00 
{
	statusLineDisplayed = FALSE;
	return;
}

void b4Set_string() // 2, 0x00 
{
	int stringNum, messNum;
	char* messagePointer;
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

	stringNum = loadAndIncWinCode();
	messNum = loadAndIncWinCode();
	messagePointer = getMessagePointer(currentLog, messNum - 1);

	/*printf("attempting to %p on bank %p to %p. the length is %d\n", string[stringNum - 1], messagePointer, logicFile.messageBank, strLenBanked(messagePointer, logicFile.messageBank));

	strcpyBanked(string[stringNum - 1], messagePointer, logicFile.messageBank);

	asm("stp");*/

	return;
}

void b4Get_string() // 5, 0x00 
{
	int strNum, messNum, row, col, l;
	char* messagePointer;
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

	strNum = loadAndIncWinCode();
	messNum = loadAndIncWinCode();
	col = loadAndIncWinCode();
	row = loadAndIncWinCode();
	l = loadAndIncWinCode();

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	b7GetInternalString(messagePointer, logicFile.messageBank, strNum, row, col, l);
	return;
}

void b4Word_to_string() // 2, 0x00 
{
	int stringNum, wordNum;
	char* stringPtr;
	size_t length;

	stringNum = loadAndIncWinCode();
	wordNum = loadAndIncWinCode();

	stringPtr = b7GetInternalStringPtr(stringNum, &length);

	memCpyBanked((byte*)&b7WordText[wordNum], (byte*)stringPtr, STRING_BANK, length);
	return;
}

void b4Parse() // 1, 0x00 
{
	int stringNum;
	size_t length;
	char* stringToParse;

	stringToParse = b7GetInternalStringPtr(stringNum, &length);

	stringNum = loadAndIncWinCode();
	b7LookupWords(stringToParse);
	return;
}

void b4Get_num() // 2, 0x40 
{
	int messNum, varNum;
	char* b7Temp;
	char* messagePointer;
	LOGICFile logicFile;
	byte tempBank;

	b7Temp = (char*)b10BankedAlloc(80, &tempBank);

	getLogicFile(&logicFile, currentLog);

	messNum = loadAndIncWinCode();
	varNum = loadAndIncWinCode();

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	b7GetString(messagePointer, logicFile.messageBank, b7Temp, tempBank, 1, 23, 3);
	var[varNum] = atoi(b7Temp);

	b10BankedDealloc((byte*)b7Temp, tempBank);

	return;
}

void b4Prevent_input() // 0, 0x00 
{
	inputLineDisplayed = FALSE;
	/* Do something else here */
	return;
}

void b4Accept_input() // 0, 0x00 
{
	inputLineDisplayed = TRUE;
	/* Do something else here */
	return;
}

void b4Set_key() // 3, 0x00 
{
	int asciiCode, scanCode, eventCode;
	char* tempStr = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	EventType event;

	asciiCode = loadAndIncWinCode();
	scanCode = loadAndIncWinCode();
	eventCode = loadAndIncWinCode();

	b7GetEvent(&event, eventCode);

	/* Ignore cases which have both values set for now. They seem to behave
	** differently than normal and often specify controllers that have
	** already been defined.
	*/
	if (scanCode && asciiCode) return;

	if (scanCode) {
		event.type = SCAN_KEY_EVENT;
		event.eventID = scanCode;
		event.asciiValue = asciiCode;
		event.scanCodeValue = scanCode;
		event.activated = FALSE;
	}
	else if (asciiCode) {
		event.type = ASCII_KEY_EVENT;
		event.eventID = asciiCode;
		event.asciiValue = asciiCode;
		event.scanCodeValue = scanCode;
		event.activated = FALSE;
	}
}

void b4Add_to_pic() // 7, 0x00 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;

	viewNum = loadAndIncWinCode();
	loopNum = loadAndIncWinCode();
	celNum = loadAndIncWinCode();
	x = loadAndIncWinCode();
	y = loadAndIncWinCode();
	priNum = loadAndIncWinCode();
	baseCol = loadAndIncWinCode();

	//printf("viewNum %d, loopNum %d, celNum %d, x %d, y %d priNum %d baseCol %d", viewNum, loopNum, celNum, x, y, priNum, baseCol);

	b9AddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
	//TODO: Update previous x and y and x and y. See Agile animated object 1264


	return;
}

void b4Add_to_pic_v() // 7, 0xFE 
{
	int viewNum, loopNum, celNum, x, y, priNum, baseCol;
	viewNum = var[loadAndIncWinCode()];
	loopNum = var[loadAndIncWinCode()];
	celNum = var[loadAndIncWinCode()];
	x = var[loadAndIncWinCode()];
	y = var[loadAndIncWinCode()];
	priNum = var[loadAndIncWinCode()];
	baseCol = var[loadAndIncWinCode()];

	b9AddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
	return;
}

void b4Status() // 0, 0x00 
{
	/* Inventory */
	// set text mode
	// if flag 13 is set then allow selection and store selection in var[25]
	var[25] = 255;
}

//void b4Save_game() // 0, 0x00 
//{
//	/* Not supported yet */
//}
//
//void b4Restore_game() // 0, 0x00 
//{
//	/* Not supported yet */
//}

void b4Restart_game() // 0, 0x00 
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
	return;
}

void b4Show_obj() // 1, 0x00 
{
	int objectNum;

	objectNum = loadAndIncWinCode();
	/* Not supported yet */
	return;
}

void b4Program_control() // 0, 0x00 
{
	controlMode = PROGRAM_CONTROL;
	return;
}

void b4Player_control() // 0, 0x00 
{
	controlMode = PLAYER_CONTROL;
	return;
}

void b4Obj_status_v() // 1, 0x80 
{
	int objectNum;

	objectNum = var[loadAndIncWinCode()];
	/* Not supported yet */

	/* showView(viewtab[objectNum].currentView); */
	bDShowObjectState(objectNum);
	return;
}


void b4Quit() // 1, 0x00                     /* 0 args for AGI version 2_089 */
{
	int quitType, ch;

	quitType = ((!oldQuit) ? loadAndIncWinCode() : 0);
	if (quitType == 1) /* Immediate quit */
		exit(0);
	else { /* Prompt for exit */
#define QUIT_BOX_SIZE 15
		//TODO: Fix display of quit message
		b3DisplayMessageBox(B4_QUIT_MESSAGE, 4, MAX_ROWS_DOWN / 2 - FIRST_ROW, MAX_CHAR_ACROSS / 2, TEXTBOX_PALETTE_NUMBER, QUIT_BOX_SIZE);
		do {
			GET_IN(ch);
			ch >> 8;
		} while ((ch != KEY_ESC) && (ch != KEY_ENTER));
		if (ch == KEY_ENTER) exit(0);
		b6ShowPicture();
	}
	return;
}

void b4Pause() // 0, 0x00 
{
#define PAUSE_BOX_SIZE 15
	while (key[KEY_ENTER]) { /* Wait */ }
	b3DisplayMessageBox(B4_PAUSE_MESSAGE, 4, MAX_ROWS_DOWN / 2 - FIRST_ROW, MAX_CHAR_ACROSS / 2, TEXTBOX_PALETTE_NUMBER, PAUSE_BOX_SIZE);
	while (!key[KEY_ENTER]) { /* Wait */ }
	b6ShowPicture();
	return;
}


//void b4Echo_line() // 0, 0x00 
//{
//
//}


//void b4Cancel_line() // 0, 0x00 
//{
//	/*currentInputStr[0]=0;
//	strPos=0;*/
//}

//void b4Init_joy() // 0, 0x00 
//{
//	/* Not important at this stage */
//}

void b4Version() // 0, 0x00 
{
#define VERSION_BOX_SIZE 15
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	b3DisplayMessageBox(B4_VERSION_MESSAGE, 4, MAX_ROWS_DOWN / 2 - FIRST_ROW, MAX_CHAR_ACROSS / 2, TEXTBOX_PALETTE_NUMBER, VERSION_BOX_SIZE);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	b6ShowPicture();
	return;
}

//void b4Script_size() // 1, 0x00 
//{
//	(*data)++;  /* Ignore the script size. Not important for this interpreter */
//}
//
//void b4Set_game_id() // 1, 0x00 
//{
//	(*data)++;  /* Ignore the game ID. Not important */
//}
//
//void b4Log() // 1, 0x00 
//{
//	(*data)++;  /* Ignore log message. Not important */
//}

void b4Reset_scan_start() // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	logicEntry.entryPoint = 0;

	setLogicEntry(&logicEntry, currentLog);
	return;
}

void b4Reposition_to() // 3, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = loadAndIncWinCode();
	localViewtab.yPos = loadAndIncWinCode();
	localViewtab.repositioned = TRUE;

	setViewTab(&localViewtab, entryNum);

	return;
}

void b4Reposition_to_v() // 3, 0x60 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.xPos = var[loadAndIncWinCode()];
	localViewtab.yPos = var[loadAndIncWinCode()];
	localViewtab.repositioned = TRUE;

	setViewTab(&localViewtab, entryNum);
	return;
}

//void b4Trace_on() // 0, 0x00 
//{
//	/* Ignore at this stage */
//}

//void b4Trace_info() // 3, 0x00 
//{
//	*data += 3;  /* Ignore trace information at this stage. */
//}

void b4Print_at() // 4, 0x00           /* 3 args for AGI versions before */
{
	b3PrintMessageInTextbox(loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode());
}

void b4Print_at_v() // 4, 0x80         /* 2_440 (maybe laterz) */
{
	b3PrintMessageInTextbox(var[loadAndIncWinCode()], loadAndIncWinCode(), loadAndIncWinCode(), loadAndIncWinCode());
}

void b4Discard_view_v() // 1, 0x80 
{
	b9DiscardView(var[loadAndIncWinCode()]);
	return;
}

void b4Clear_text_rect() // 5, 0x00 
{
	int x1, y1, x2, y2, boxColour;

	x1 = loadAndIncWinCode();
	y1 = loadAndIncWinCode();
	x2 = loadAndIncWinCode();
	y2 = loadAndIncWinCode();
	boxColour = loadAndIncWinCode();
	if ((screenMode == AGI_GRAPHICS) && (boxColour > 0)) boxColour = 15;
	if (screenMode == AGI_TEXT) boxColour = 0;
	show_mouse(NULL);
	show_mouse(screen);
	return;
}

//void b4Set_upper_left() // 2, 0x00    (x, y) ??
//{
//	*data += 2;
//}

void b4WaitKeyRelease()
{
	byte ch;

	b5WaitOnKey();
	return;
}

int b4MenuUpdate(byte eventNo)
{
	b4WaitKeyRelease();
	b7ActivateEvent(eventNo);
	return D_O_K;
}

int menuEvent0() { return b4MenuUpdate(0); }
int menuEvent1() { return b4MenuUpdate(1); }
int menuEvent2() { return b4MenuUpdate(2); }
int menuEvent3() { return b4MenuUpdate(3); }
int menuEvent4() { return b4MenuUpdate(4); }
int menuEvent5() { return b4MenuUpdate(5); }
int menuEvent6() { return b4MenuUpdate(6); }
int menuEvent7() { return b4MenuUpdate(7); }
int menuEvent8() { return b4MenuUpdate(8); }
int menuEvent9() { return b4MenuUpdate(9); }
int menuEvent10() { return b4MenuUpdate(10); }
int menuEvent11() { return b4MenuUpdate(11); }
int menuEvent12() { return b4MenuUpdate(12); }
int menuEvent13() { return b4MenuUpdate(13); }
int menuEvent14() { return b4MenuUpdate(14); }
int menuEvent15() { return b4MenuUpdate(15); }
int menuEvent16() { return b4MenuUpdate(16); }
int menuEvent17() { return b4MenuUpdate(17); }
int menuEvent18() { return b4MenuUpdate(18); }
int menuEvent19() { return b4MenuUpdate(19); }
int menuEvent20() { return b4MenuUpdate(20); }
int menuEvent21() { return b4MenuUpdate(21); }
int menuEvent22() { return b4MenuUpdate(22); }
int menuEvent23() { return b4MenuUpdate(23); }
int menuEvent24() { return b4MenuUpdate(24); }
int menuEvent25() { return b4MenuUpdate(25); }
int menuEvent26() { return b4MenuUpdate(26); }
int menuEvent27() { return b4MenuUpdate(27); }
int menuEvent28() { return b4MenuUpdate(28); }
int menuEvent29() { return b4MenuUpdate(29); }
int menuEvent30() { return b4MenuUpdate(30); }
int menuEvent31() { return b4MenuUpdate(31); }
int menuEvent32() { return b4MenuUpdate(32); }
int menuEvent33() { return b4MenuUpdate(33); }
int menuEvent34() { return b4MenuUpdate(34); }
int menuEvent35() { return b4MenuUpdate(35); }
int menuEvent36() { return b4MenuUpdate(36); }
int menuEvent37() { return b4MenuUpdate(37); }
int menuEvent38() { return b4MenuUpdate(38); }
int menuEvent39() { return b4MenuUpdate(39); }
int menuEvent40() { return b4MenuUpdate(40); }
int menuEvent41() { return b4MenuUpdate(41); }
int menuEvent42() { return b4MenuUpdate(42); }
int menuEvent43() { return b4MenuUpdate(43); }
int menuEvent44() { return b4MenuUpdate(44); }
int menuEvent45() { return b4MenuUpdate(45); }
int menuEvent46() { return b4MenuUpdate(46); }
int menuEvent47() { return b4MenuUpdate(47); }
int menuEvent48() { return b4MenuUpdate(48); }
int menuEvent49() { return b4MenuUpdate(49); }

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

	messNum = loadAndIncWinCode();

	//return;

	if (numOfMenus == 0)
	{
		b5MenuChildInit();
	}

	getLogicFile(&currentLogicFile, currentLog);

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

	b5SetMenu(&newMenu, numOfMenus);
	numOfMenus++;

	newMenu.dp = NULL;
	newMenu.flags = 0;
	newMenu.proc = NULL;
	newMenu.text = NULL;
	newMenu.menuTextBank = 0;

	/* Mark end of menu */
	b5SetMenu(&newMenu, numOfMenus);

	return;
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM05")

void b5Set_menu_item() // 2, 0x00 
{
	int messNum, controllerNum, i;
	MENU childMenu;
	LOGICFile currentLogicFile;
	EventType event;

	getLogicFile(&currentLogicFile, currentLog);
	b7GetEvent(&event, controllerNum);

	messNum = loadAndIncWinCode();
	controllerNum = loadAndIncWinCode();

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

//void b4Submit_menu() // 0, 0x00 
//{
//
//}

//void b4Enable_item() // 1, 0x00 
//{
//	(*data)++;
//}
//
//void b4Disable_item() // 1, 0x00 
//{
//	(*data)++;
//}


void b5Menu_input() // 0, 0x00 
{
	do_menu(the_menu, 10, 20);
	return;
}

void b5Show_obj_v() // 1, 0x01 
{
	int objectNum;

	objectNum = var[loadAndIncWinCode()];
	/* Not supported yet */
	return;
}

//void b4Open_dialogue() // 0, 0x00 
//{
//
//}

//void b4Close_dialogue() // 0, 0x00 
//{
//
//}

void b5Mul_n() // 2, 0x80 
{
	var[loadAndIncWinCode()] *= loadAndIncWinCode();
	return;
}

void b5Mul_v() // 2, 0xC0 
{
	var[loadAndIncWinCode()] *= var[loadAndIncWinCode()];
	return;
}

void b5Div_n() // 2, 0x80 
{
	var[loadAndIncWinCode()] /= loadAndIncWinCode();
	return;
}

void b5Div_v() // 2, 0xC0 
{
	var[loadAndIncWinCode()] /= var[loadAndIncWinCode()];
	return;
}

void b5SetPriorityBase()
{
	priorityBase = loadAndIncWinCode();
	bAPopulatePrecomputedPriorityTable();
}

#pragma code-name (pop)



