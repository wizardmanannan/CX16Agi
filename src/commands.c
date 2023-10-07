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
#define VERBOSE_MESSAGE_PRINT
#define TEXTBOX_PALETTE_NUMBER 1
#define DISPLAY_PALETTE_NUMBER 2

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

int printCounter = 1;
byte lastRoom = 0;

void executeLogic(LOGICEntry* logicEntry, int logNum);

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

void trampolineProcessString(char* stringPointer, byte stringBank, char* outputString);

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

	for (i = 1; i < strlen(result) && *result == ' '; i++) //Skip tabbing
	{
		result = (char*)logicFile.messages[messageNo + i];
	}
	
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
void b3ProcessString(char* stringPointer, byte stringBank, char* outputString);
void trampolineProcessString(char* stringPointer, byte stringBank, char* outputString)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = PROCESS_STRING_BANK;

	b3ProcessString(stringPointer, stringBank, outputString);

	RAM_BANK = previousRamBank;
}

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
	return (objects[loadAndIncWinCode()].roomNum == 255);
}

boolean b1Obj_in_room() // 2, 0x40 
{
	int objNum, varNum;

	objNum = loadAndIncWinCode();
	varNum = var[loadAndIncWinCode()];
	return (objects[objNum].roomNum == varNum);
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
	int eventNum = loadAndIncWinCode(), retVal = 0;

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

	s1 = loadAndIncWinCode();
	s2 = loadAndIncWinCode();
	if (strcmp(string[s1], string[s2]) == 0) return TRUE;
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
	trampoline_1Int(&b6LoadLogicFile, loadAndIncWinCode(), LOGIC_CODE_BANK);

	return;
}

void b2Load_logics_v() // 1, 0x80 
{
	trampoline_1Int(&b6LoadLogicFile, var[loadAndIncWinCode()], LOGIC_CODE_BANK);

	return;
}



void b2Load_pic() // 1, 0x80 
{
	trampoline_1Int(&b11LoadPictureFile, var[loadAndIncWinCode()], PICTURE_CODE_BANK);
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

    drawPicTrampoline(loadedPicture.data, loadedPicture.size, TRUE, pNum);

	return;
}

void b2Show_pic() // 0, 0x00 
{
	okToShowPic = TRUE;   /* Says draw picture with next object update */
	/*stretch_blit(picture, working_screen, 0, 0, 160, 168, 0, 20, 640, 336);*/
	trampoline_0(&b11ShowPicture, PICTURE_CODE_BANK);

	return;
}

void b2Discard_pic() // 1, 0x80 
{
	trampoline_1Int(&b11DiscardPictureFile,var[loadAndIncWinCode()], PICTURE_CODE_BANK);

	return;
}

void b2Overlay_pic() // 1, 0x80 
{
	int pNum;
	PictureFile loadedPicture;
	pNum = var[loadAndIncWinCode()];

	getLoadedPicture(&loadedPicture, pNum);

	drawPicTrampoline(loadedPicture.data, loadedPicture.size, FALSE, pNum);

	return;
}

void b2Show_pri_screen() // 0, 0x00 
{
	return;
}

/************************** VIEW ACTION COMMANDS **************************/

void b2Load_view() // 1, 0x00 
{
	trampoline_1Int(&b9LoadViewFile, (loadAndIncWinCode()), VIEW_CODE_BANK_1);
	return;
}

void b2Load_view_v() // 1, 0x80 
{
	trampoline_1Int(&b9LoadViewFile, var[loadAndIncWinCode()], VIEW_CODE_BANK_1);
	return;
}

void b2Discard_view() // 1, 0x00 
{
	trampoline_1Int(&b9DiscardView, loadAndIncWinCode(), VIEW_CODE_BANK_1);
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

	setViewTab(&localViewTab, entryNum);

	getViewTab(&localViewTab, entryNum);
	return;
}

void b2Unanimate_all() // 0, 0x00 
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
	return;
}

void b2Draw() // 1, 0x00 
{
	int entryNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	localViewtab.flags |= (DRAWN | UPDATE);   /* Not sure about update */



	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, localViewtab.currentCel, VIEW_CODE_BANK_1);

	trampoline_1Int(&bADrawObject, entryNum, VIEW_CODE_BANK_2);

	setViewTab(&localViewtab, entryNum);
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

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	setViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_view_v() // 2, 0x40 
{
	int entryNum, viewNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	viewNum = var[loadAndIncWinCode()];

	getViewTab(&localViewtab, entryNum);

	trampolineViewUpdater1Int(&b9AddViewToTable, &localViewtab, viewNum, VIEW_CODE_BANK_1);

	getViewTab(&localViewtab, entryNum);
	return;
}

void b2Set_loop() // 2, 0x00 
{
	int entryNum, loopNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	loopNum = loadAndIncWinCode();

	getViewTab(&localViewtab, entryNum);
	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, 0, VIEW_CODE_BANK_1);
   
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

	trampolineViewUpdater1Int(&b9SetLoop, &localViewtab, loopNum, VIEW_CODE_BANK_1);
	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, loopNum, VIEW_CODE_BANK_1);

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

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

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

	trampolineViewUpdater1Int(&b9SetCel, &localViewtab, celNum, VIEW_CODE_BANK_1);

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
	/* Do immediate update here. Call update(entryNum) */

	trampoline_1Int(&bAUpdateObj, entryNum, VIEW_CODE_BANK_1);
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

	localViewtab.param1 = loadAndIncWinCode();
	localViewtab.param2 = loadAndIncWinCode();
	localViewtab.param3 = localViewtab.stepSize;  /* Save stepsize */
	stepVal = loadAndIncWinCode();
	if (stepVal > 0) localViewtab.stepSize = stepVal;
	localViewtab.param4 = loadAndIncWinCode();
	localViewtab.motion = 3;
	localViewtab.flags |= MOTION;

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
	int entryNum, stepVal, flagNum;
	ViewTable localViewtab;

	entryNum = loadAndIncWinCode();
	getViewTab(&localViewtab, entryNum);

	stepVal = loadAndIncWinCode();
	flagNum = loadAndIncWinCode();
	localViewtab.param1 = localViewtab.stepSize;
	/* Might need to put 'if (stepVal != 0)' */
	//localViewtab.stepSize = stepVal;
	localViewtab.param2 = flagNum;
	localViewtab.motion = 2;
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
	localViewtab.flags |= MOTION;

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
	objects[loadAndIncWinCode()].roomNum = 255;
	return;
}

void b3Get_v() // 1, 0x80 
{
	objects[var[loadAndIncWinCode()]].roomNum = 255;
	return;
}

void b3Drop() // 1, 0x00 
{
	objects[loadAndIncWinCode()].roomNum = 0;
	return;
}


void b3Put() // 2, 0x00 
{
	int objNum, room;

	objNum = loadAndIncWinCode();
	room = loadAndIncWinCode();
	objects[objNum].roomNum = room;
	return;
}

void b3Put_v() // 2, 0x40 
{
	int objNum, room;

	objNum = loadAndIncWinCode();
	room = var[loadAndIncWinCode()];
	objects[objNum].roomNum = room;
	return;
}

void b3Get_room_v() // 2, 0xC0 
{
	int objNum, room;

	objNum = var[loadAndIncWinCode()];
	var[loadAndIncWinCode()] = objects[objNum].roomNum;
	return;
}

void b3Load_sound() // 1, 0x00 
{
	int soundNum;

	soundNum = loadAndIncWinCode();
	loadSoundFile(soundNum);
	return;
}

void b3Play_sound() // 2, 00  sound() renamed to avoid clash
{
	int soundNum;

	soundNum = loadAndIncWinCode();
	soundEndFlag = loadAndIncWinCode();
	/* playSound(soundNum); */
	flag[soundEndFlag] = TRUE;
	return;
}


void b3Stop_sound() // 0, 0x00 
{
	checkForEnd = FALSE;
	stop_midi();
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

void b3ProcessString(char* stringPointer, byte stringBank, char* outputString)
{
#define TEMP_SIZE 80
#define NUM_STRING_SIZE 80
#define INPUT_BUFFER_SIZE 10
	
}

void b3Print() // 1, 00 
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (loadAndIncWinCode()) - 1);

	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	//b3ProcessString(messagePointer, 0, tempString);
	printInBoxBig(tempString, -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	show_mouse(NULL);
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);
	return;
}

void b3Print_v() // 1, 0x80 
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;

	char* messagePointer = getMessagePointer(currentLog, (var[loadAndIncWinCode()]) - 1);

	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	trampolineProcessString(messagePointer, 0, tempString);
	//printf("Warning Print In Bigbox Not Implemented Implement This");
	//printInBoxBig2(tempString, -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);

	return;
}

void b3Display() // 3, 0x00 
{
	int row, col, messNum, i;
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;
	
	LOGICFile logicFile;
	getLogicFile(&logicFile, currentLog);

	row = loadAndIncWinCode();
	col = loadAndIncWinCode();
	messNum = loadAndIncWinCode();

	messagePointer = getMessagePointer(currentLog, messNum - 1);

#ifdef VERBOSE_MESSAGE_PRINT
	printf("Attempting to print %d from script %d. The length is %d. The message pointer is %p\n", messNum - 1, currentLog, strLenBanked(messagePointer, logicFile.messageBank), messagePointer);
#endif
	//trampolineProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	b3DisplayMessageBox(messagePointer, logicFile.messageBank, row, col, DISPLAY_PALETTE_NUMBER, 0);

	return;
}

void b3Display_v() // 3, 0xE0 
{
	int row, col, messNum;
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	char* messagePointer;

	col = var[loadAndIncWinCode()];
	row = var[loadAndIncWinCode()];
	messNum = var[loadAndIncWinCode()];
	//drawString(picture, logics[currentLog].data->messages[messNum-1],
	//   row*8, col*8, agi_fg, agi_bg);

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	trampolineProcessString(messagePointer, 0, tempString);
	drawBigString(screen, tempString, row * 16, 20 + (col * 16), agi_fg, agi_bg);
	/*lprintf("info: display.v() %s, foreground: %d background: %d",
	   tempString, agi_fg, agi_bg);*/
	return;
}

void b3Clear_lines() // 3, 0x00 
{
	int boxColour,startLine, endLine;
	byte i, j;
	startLine = loadAndIncWinCode();
	endLine = loadAndIncWinCode();
	boxColour = loadAndIncWinCode();
	
	show_mouse(NULL);

	b3FillChar(startLine, endLine, DISPLAY_PALETTE_NUMBER, TRANSPARENT_CHAR);
	
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
	clear(screen);
	return;
}

void b4Graphics() // 0, 0x00 
{
	screenMode = AGI_GRAPHICS;
	/* Do something else here */
	inputLineDisplayed = TRUE;
	statusLineDisplayed = TRUE;
	okToShowPic = TRUE;
	clear(screen);
	return;
}

void b4Set_cursor_char() // 1, 0x00 
{
	char* temp = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	byte msgNo = loadAndIncWinCode() - 1;
	char* messagePointer = getMessagePointer(currentLog, msgNo);
	LOGICFile logicFile;

	getLogicFile(&logicFile, currentLog);

#ifdef VERBOSE_STRING_CHECK
	printf("Your msgNo is %d\n", msgNo);
#endif // VERBOSE_STRING_CHECK


	trampolineProcessString(messagePointer, logicFile.messageBank, temp);
	cursorChar = temp[0];

#ifdef VERBOSE_STRING_CHECK
	printf("Your cursor char is %c\n", cursorChar);
#endif

	return;
}

void b4Set_text_attribute() // 2, 0x00 
{
	agi_fg = (loadAndIncWinCode()) + 1;
	agi_bg = (loadAndIncWinCode()) + 1;

#ifdef VERBOSE_MESSAGE_PRINT
	printf("Set foreground: %d background: %d\n", agi_fg, agi_bg);
#endif // VERBOSE_MESSAGE_PRINT
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

	strcpyBanked(string[stringNum - 1], messagePointer, logicFile.messageBank);
	return;
}

void b4Get_string() // 5, 0x00 
{
	int strNum, messNum, row, col, l;
	char* messagePointer;

	strNum = loadAndIncWinCode();
	messNum = loadAndIncWinCode();
	col = loadAndIncWinCode();
	row = loadAndIncWinCode();
	l = loadAndIncWinCode();

	messagePointer = getMessagePointer(currentLog, messNum - 1);

	getString(messagePointer, string[strNum], row, col, l);
	return;
}

void b4Word_to_string() // 2, 0x00 
{
	int stringNum, wordNum;

	stringNum = loadAndIncWinCode();
	wordNum = loadAndIncWinCode();
	strcpy(string[stringNum], wordText[wordNum]);
	return;
}

void b4Parse() // 1, 0x00 
{
	int stringNum;

	stringNum = loadAndIncWinCode();
	lookupWords(string[stringNum]);
	return;
}

void b4Get_num() // 2, 0x40 
{
	int messNum, varNum;
	char temp[80];
	char* messagePointer;

	messNum = loadAndIncWinCode();
	varNum = loadAndIncWinCode();

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	getString(messagePointer, temp, 1, 23, 3);
	var[varNum] = atoi(temp);
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

	asciiCode = loadAndIncWinCode();
	scanCode = loadAndIncWinCode();
	eventCode = loadAndIncWinCode();

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
	
	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
	


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

	trampolineAddToPic(viewNum, loopNum, celNum, x, y, priNum, baseCol);
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

void b4Random_num() // 3, 0x20  random() renamed to avoid clash
{
	int startValue, endValue;

	startValue = loadAndIncWinCode();
	endValue = loadAndIncWinCode();
	var[loadAndIncWinCode()] = 4;  (rand() % ((endValue - startValue) + 1)) + startValue;
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
	trampoline_1Int(&bDShowObjectState, objectNum, VIEW_CODE_BANK_4);
	return;
}


void b4Quit() // 1, 0x00                     /* 0 args for AGI version 2_089 */
{
	int quitType, ch;

	quitType = ((!oldQuit) ? loadAndIncWinCode() : 0);
	if (quitType == 1) /* Immediate quit */
		exit(0);
	else { /* Prompt for exit */
		printInBoxBig("Press ENTER to quit.\nPress ESC to keep playing.", -1, -1, 30);
		do {
			ch = (readkey() >> 8);
		} while ((ch != KEY_ESC) && (ch != KEY_ENTER));
		if (ch == KEY_ENTER) exit(0);
		trampoline_0(&b11ShowPicture, PICTURE_CODE_BANK);
	}
	return;
}

void b4Pause() // 0, 0x00 
{
	while (key[KEY_ENTER]) { /* Wait */ }
	printInBoxBig("      Game paused.\nPress ENTER to continue.", -1, -1, 30);
	while (!key[KEY_ENTER]) { /* Wait */ }
	trampoline_0(&b11ShowPicture, PICTURE_CODE_BANK);
	okToShowPic = TRUE;
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
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }
	printInBoxBig("MEKA AGI Interpreter\n    Version 1.0", -1, -1, 30);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	trampoline_0(&b11ShowPicture, PICTURE_CODE_BANK);
	okToShowPic = TRUE;
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

void b4Set_scan_start() // 0, 0x00 
{
	LOGICEntry logicEntry;

	getLogicEntry(&logicEntry, currentLog);

	/* currentPoint is set in executeLogic() */
	logicEntry.entryPoint = logicEntry.currentPoint + 1;
	/* Does it return() at this point, or does it execute to the end?? */

	setLogicEntry(&logicEntry, currentLog);
	return;
}


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
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;
	int timeoutFlagVal = var[PRINT_TIMEOUT];
	byte waitTicks;
	byte vSyncToContinueAt;

	LOGICFile logicFile;
	getLogicFile(&logicFile, currentLog);

#ifdef  VERBOSE_MESSAGE_PRINT
	printf("The bank is %d\n", logicFile.messageBank);
#endif

	messNum = loadAndIncWinCode();
	x = loadAndIncWinCode();
	y = loadAndIncWinCode();
	l = loadAndIncWinCode();

#ifdef  VERBOSE_MESSAGE_PRINT
	printf("Attempting to display message %d at %d,%d, length %d\n", messNum - 1, x, y, l);
#endif

	show_mouse(NULL);
	show_mouse(screen);
	
	//while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ } //TODO: When keyboard control added put in wait

	messagePointer = getMessagePointer(currentLog, messNum - 1);



    trampolineDisplayMessageBox(messagePointer, logicFile.messageBank, x, y, TEXTBOX_PALETTE_NUMBER, l);

	if (timeoutFlagVal)
	{
		waitTicks = timeoutFlagVal * 30;  // The timeout value is given in half seconds and the TotalTicks in 1/60ths of a second.
		vSyncToContinueAt = vSyncCounter + waitTicks;

		while (vSyncCounter != vSyncToContinueAt);
		
		//trampolinefillChar(y, )
	}
	else
	{
		//TODO: Wait for key press
			//while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	//while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	}
	//printInBoxBig(tempString, x, y, l);

	show_mouse(NULL);

	show_mouse(screen);
	return;
}

void b4Print_at_v() // 4, 0x80         /* 2_440 (maybe laterz) */
{
	char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp;
	int messNum, x, y, l;
	char* messagePointer;

	messNum = var[loadAndIncWinCode()];
	x = loadAndIncWinCode();
	y = loadAndIncWinCode();
	l = loadAndIncWinCode();
	show_mouse(NULL);
	temp = create_bitmap(640, 336);
	blit(agi_screen, temp, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	while (key[KEY_ENTER] || key[KEY_ESC]) { /* Wait */ }

	messagePointer = getMessagePointer(currentLog, messNum - 1);
	trampolineProcessString(messagePointer, 0, tempString);
	printInBoxBig(tempString, x, y, l);
	while (!key[KEY_ENTER] && !key[KEY_ESC]) { /* Wait */ }
	while (key[KEY_ENTER] || key[KEY_ESC]) { clear_keybuf(); }
	show_mouse(NULL);
	blit(temp, agi_screen, 0, 0, 0, 0, 640, 336);
	show_mouse(screen);
	destroy_bitmap(temp);
	return;
}

void b4Discard_view_v() // 1, 0x80 
{
	trampoline_1Int(&b9DiscardView, var[loadAndIncWinCode()], VIEW_CODE_BANK_1);
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
	rectfill(agi_screen, x1 * 16, y1 * 16, (x2 * 16) + 15, (y2 * 16) + 15, boxColour);
	show_mouse(screen);
	return;
}

//void b4Set_upper_left() // 2, 0x00    (x, y) ??
//{
//	*data += 2;
//}

void b4WaitKeyRelease()
{
	while (keypressed()) { /* Wait */ }
	return;
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

	messNum = loadAndIncWinCode();

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

	return;
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM05")

void b5Set_menu_item() // 2, 0x00 
{
	int messNum, controllerNum, i;
	MENU childMenu;
	LOGICFile currentLogicFile;

	getLogicFile(&currentLogicFile, currentLog);


	messNum = loadAndIncWinCode();
	controllerNum = loadAndIncWinCode();

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
#pragma code-name (pop)



