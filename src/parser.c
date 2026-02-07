/***************************************************************************
** parser.c
**
** These functions are related to the 'said' parser.
**
** (c) 1997 Lance Ewing - Initial code (27 Aug 97)
***************************************************************************/

#define  PLAYER_CONTROL   0
#define  PROGRAM_CONTROL  1

#include "parser.h"

extern boolean* flag;
extern byte* var;
extern char string[12][40];
extern int dirnOfEgo;
extern int controlMode;

int lastKey;
int numInputWords;
char cursorChar = '_';

#define MAX_INPUT_STRING_LENGTH 40 //Includes terminator 

//#define VERBOSE_DEBUG_LOOKUP_WORDS
#pragma rodata-name (push, "BANKRAM07")
char SHOW_PRIORITY[] = {0X73, 0X68, 0X6F, 0X77, 0X20, 0X70, 0X72, 0X69, 0X6F, 0X72, 0X69, 0X74, 0X79 };
#pragma rodata-name (pop)
#pragma bss-name (push, "BANKRAM07")
int b7InputWords[10];
char b7WordText[10][80], b7CurrentInputStr[MAX_INPUT_STRING_LENGTH + 1], strPos = 0, b7OutputString[80], b7Temp[256];
char string[12][40];
char b7LookupWordsBuffer[125];
//boolean wordsAreWaiting=FALSE;

byte b7KeyState[256], b7AsciiState[256];
char b7LastLine[80];
EventType b7Events[256];  /* controller(), set.key(), set.menu.item() */
byte b7Directions[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM07")
int b7Strcmp(char const* _Str1, char const* _Str2)
{
	return strcmp(_Str1, _Str2);
}


void b7GetEvent(EventType* event, byte eventNumber)
{
	*event = b7Events[eventNumber];
}

void b7ActivateEvent(byte eventNumber)
{
	b7Events[eventNumber].activated = 1;
}

void b7SetEvent(EventType* event, byte eventNumber)
{
	b7Events[eventNumber] = *event;
}

byte b7GetAsciiState(byte number)
{
	return b7AsciiState[number];
}

byte b7GetKeyState(byte number)
{
	return b7KeyState[number];
}

byte b7GetInputWord(byte number)
{
	return b7InputWords[number];
}

char* b7GetInternalStringPtr(byte number, size_t* length)
{
	*length = strlen(string[number]);
	return string[number];
}

void b7LookupWords(char* inputLine);

void b7ProcessString(char* stringPointer, byte stringBank, char* b7OutputString, byte outputStringBank)
{
#define TEMP_SIZE 80
#define NUM_STRING_SIZE 80
#define INPUT_BUFFER_SIZE 10

}

void b7InitEvents()
{
	int eventNum;

	for (eventNum = 0; eventNum < 256; eventNum++) {
		b7Events[eventNum].activated = FALSE;
		b7Events[eventNum].type = NO_EVENT;
	}
}

void b7HandleDirection(int dirn)
{
	ViewTable viewTab;

	getViewTab(&viewTab, 0);

	if (viewTab.flags & MOTION) {
		if (b7Directions[dirn] == 0) {
			memset(b7Directions, 0, 9);
			b7Directions[dirn] = 1;
			viewTab.direction = dirn;
			var[6] = dirnOfEgo = dirn;
		}
		else {
			memset(b7Directions, 0, 9);
			viewTab.direction = 0;
			var[6] = dirnOfEgo = 0;
			viewTab.staleCounter = 1;
		}

		setViewTab(&viewTab, 0);
	}
}

void b7GetString(char* promptStr, byte promptStringBank, char* returnStr, byte returnStrBank, int x, int y, int l)
{
	int ch, gx, gy;
	boolean stillInputing = TRUE;

	gx = (x - 1) * 16;
	gy = ((y - 1) * 16) + 20;
	b7ProcessString(promptStr, promptStringBank, b7Temp, PARSER_BANK);
	sprintf(b7OutputString, "%s%s%c", b7Temp, b7CurrentInputStr, cursorChar);

	do {
		GET_IN(ch);
		if (ch)
		{
			if ((ch >> 8) == 0x1C) ch |= 0x0D; /* Handle keypad ENTER */
			switch (ch & 0xff) {
			case 0:     /* Ignore these when building input string */
			case 0x09:
			case 0x1B:
				break;
			case 0x0D:  /* ENTER */
				strcpy(returnStr, b7CurrentInputStr);
				while (key[KEY_ENTER]) { /* Wait until ENTER released */ }
				return;
			case 0x08: /* Backspace */
				if (strPos > 0) {
					strPos--;
					b7CurrentInputStr[strPos] = 0;
				}
				break;
			case 0x0A: break;
			default:
				if (strPos == l) break;
				b7CurrentInputStr[strPos] = (ch & 0xff);
				strPos++;
				b7CurrentInputStr[strPos] = 0;
				sprintf(b7OutputString, "%s%s%c", b7Temp, b7CurrentInputStr, cursorChar);
				break;
			}
		}
	} while (ch);
}

/* used by get.string and get.num */
void b7GetInternalString(char* promptStr, byte promptStringBank, byte stringNumber, int x, int y, int l)
{
	char* inputString = string[stringNumber];
	b7GetString(promptStr, promptStringBank, inputString, PARSER_BANK, x, y, l);
}

#pragma wrapped-call (push, trampoline, DEBUG_INIT_BANK)
extern boolean b5IsDebuggingEnabled();
#pragma wrapped-call (pop)
extern void bDbgShowPriority();
extern byte debugBank;
/***************************************************************************
** pollKeyboard
***************************************************************************/
void b7PollKeyboard()
{
	static char strPos = 0;
	int ch, dummy, gx, gy;

	var[19] = 0;

	/* Clear keyboard buffers */
	memset(b7KeyState, 0, 256);
	memset(b7AsciiState, 0, 256);
	//b1ProcessString(temp, PARSER_BANK, outputString );
	gx = 0;
	gy = ((user_input_line - 1) * 16) + 20;
	if (inputLineDisplayed) {
		sprintf(b7OutputString, "%s%s%c", b7Temp, b7CurrentInputStr, cursorChar);
	}

	do {
		GET_IN(ch);

		if (ch) {
			var[19] = ch;
			lastKey = ch;  /* Store key value for have.key() cmd */
			if (ch == 0x1C) ch |= 0x0D; /* Handle keypad ENTER */
			b7KeyState[ch] = 1;     /* Mark scancode as activated */
			/* if ((ch & 0x00) != 0x00) asciiState[ch & 0xff] = 1; */
			b7AsciiState[ch] = 1;

			//if ((ch >> 8) == KEY_F11) saveSnapShot();

			/* Handle arrow keys */
			if (controlMode == PLAYER_CONTROL) {
				switch (ch) {
				case KEY_UP: b7HandleDirection(1); return;
				case KEY_PGUP: b7HandleDirection(2); return;
				case KEY_RIGHT: b7HandleDirection(3); return;
				case KEY_PGDN: b7HandleDirection(4); return;
				case KEY_DOWN: b7HandleDirection(5); return;
				case KEY_END: b7HandleDirection(6); return;
				case KEY_LEFT: b7HandleDirection(7); return;
				case KEY_HOME: b7HandleDirection(8); return;
				}
			}

			if (inputLineDisplayed) {
				switch (ch & 0xff) {
				case KEY_TAB:  /* Ignore these when building input string */
				case KEY_ESC:
					/* closedown(); */
					break;
				case KEY_ENTER:
					if (b5IsDebuggingEnabled() && !strcmp(b7CurrentInputStr, SHOW_PRIORITY))
					{
						trampolineDebug(bDbgShowPriority);
					}
					b7LookupWords(b7CurrentInputStr);
					b7CurrentInputStr[0] = 0;
					strPos = 0;
					sprintf(b7OutputString, "%s%s%c", b7Temp, b7CurrentInputStr, cursorChar);
					strcpy(b7LastLine, b7CurrentInputStr);
					break;
				case KEY_BACK_SPACE:   /* Backspace */
					if (strPos > 0) {
						strPos--;
						b7CurrentInputStr[strPos] = 0;
						sprintf(b7OutputString, "%s%s%c ", b7Temp, b7CurrentInputStr, cursorChar);
						//drawBigString(screen, " ", (strPos*16), 448, 7, 0);
					}
					else
						return;
					break;
				default:
					if (strlen(b7CurrentInputStr) < MAX_INPUT_STRING_LENGTH && (ch >= KEY_CAP_A && ch <= KEY_CAP_Z || ch >= KEY_LOWER_A && ch <= KEY_LOWER_Z || ch >= KEY_1 && ch <= KEY_9 || ch == SPACE))
					{
						if (ch >= KEY_CAP_A && ch <= KEY_CAP_Z)
						{
							ch -= KEY_CAP_A - KEY_LOWER_A;
							ch += ASCII_DIFF;
						}
						else if (ch >= KEY_LOWER_A && ch <= KEY_LOWER_Z)
						{
							ch += ASCII_DIFF;
						}
						
						b7CurrentInputStr[strPos] = (ch);
						strPos++;
						b7CurrentInputStr[strPos] = 0;
						sprintf(b7OutputString, "%s%s%c", b7Temp, b7CurrentInputStr, cursorChar);
					}
					break;
				}
			}
		}
	} while (ch);
}

/***************************************************************************
** stripExtraChars
**
** Purpose: This function strips out all the punctuation and unneeded
** spaces from the user input, and converts all the characters to
** lowercase.
***************************************************************************/
void b7StripExtraChars(char* userInput)
{
	char tempString[41] = "";
	int strPos, tempPos = 0;
	boolean lastCharSpace = FALSE;

	for (strPos = 0; strPos < strlen(userInput); strPos++) {
		switch (userInput[strPos]) {
		case ' ':
			if (!lastCharSpace) tempString[tempPos++] = ' ';
			lastCharSpace = TRUE;
			break;
		case ',':
		case '.':
		case '?':
		case '!':
		case '(':
		case ')':
		case ';':
		case ':':
		case '[':
		case ']':
		case '{':
		case '}':
		case '\'':
		case '`':
		case '-':
		case '"':
			/* Ignore */
			break;
		default:
			lastCharSpace = FALSE;
			tempString[tempPos++] = userInput[strPos];
			break;
		}
	}

	tempString[tempPos] = 0;
	strcpy(userInput, tempString);
}

char** b7TokenizeWords(char* inputLine, char** tokens)
{
	char* token;

	for (token = strtok(inputLine, " "); token; token = strtok(0, " ")) {
		*tokens++ = token;
	}

	return tokens - 1;
}


extern long opStartPrintingAt;
/***************************************************************************
** lookupWords
**
** Purpose: To convert the users input string into a list of synonym
** numbers as used in WORDS.TOK. This functions is called when the ENTER
** key is hit in the normal flow of game play (as opposed to using the menu,
** browsing the inventory, etc).
***************************************************************************/
void b7LookupWords(char* inputLine)
{
	int synNum;
	boolean allWordsFound = TRUE;
	char* userInput = b7Temp;
	char** start = (char**)b7LookupWordsBuffer, **end, **originalEnd;
	char* strBuf = (char*) start + MAX_WORD_SIZE * sizeof(char*) + sizeof(char**);
	byte stringLength;
	
#ifdef VERBOSE_DEBUG_LOOKUP_WORDS
	byte i;
#endif

	strcpy(userInput, inputLine);
	b7StripExtraChars(userInput);

	end = b7TokenizeWords(userInput, start);
	originalEnd = end;

#ifdef VERBOSE_DEBUG_LOOKUP_WORDS
	printf("start %p end %p\n", start, end);
#endif

	strcpy(userInput, inputLine); //We want to remove the terminators added by strtok afterwards

	numInputWords = 0;

	stringLength = strlen(inputLine);
	while (start <= end && allWordsFound)
	{
		memcpy(strBuf, *start, stringLength);
		strBuf[stringLength] = '\0';

#ifdef VERBOSE_DEBUG_LOOKUP_WORDS
		printf("s. %p e. %p sBuf %p\n", start, end, strBuf);
		asm("stp");
#endif
		
		   switch (synNum = b12FindSynonymNum(strBuf, PARSER_BANK)) {
			case -1: /* Word not found */
				if (start == end)
				{
					var[9] = numInputWords + 1;
					allWordsFound = FALSE;
				}
				stringLength = *end - *start;
				*end--;
				break;
			default:
				start = end + 1;
				end = originalEnd;

				if (start <= end)
				{
					stringLength = strlen(*start);
				}
				
#ifdef VERBOSE_DEBUG_LOOKUP_WORDS
				printf("2 s. %p e. %p sBuf %p. stringLength is %d\n", start, end, strBuf, stringLength);
				asm("stp");
#endif
				if (synNum)
				{
					b7InputWords[numInputWords] = synNum;
					strcpy(b7WordText[numInputWords], strBuf);
					numInputWords++;
				}
				break;
			}
	}

#ifdef VERBOSE_DEBUG_LOOKUP_WORDS
	for (i = 0; i < numInputWords; i++)
	{
		printf("%d \n", b7InputWords[i]);
	}
#endif

	if (strlen(inputLine)) {
		flag[2] = TRUE;  /* The user has entered an input line */
		flag[4] = FALSE; /* Said command has not yet accepted the line */
	}
}

/***************************************************************************
** said
**
** Purpose: To execute the said() test command. Each two-byte argument is
** compared with the relevant words entered by the user. If there is an
** exact match, it returns TRUE, otherwise it returns false.
***************************************************************************/
boolean b7Said(byte** data)
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

		if (b7InputWords[wordNum] != argValue) wordsMatch = FALSE;
	}

	if ((numInputWords != numOfArgs) && (argValue != 9999)) return FALSE;

	if (wordsMatch) {
		flag[4] = TRUE;    /* said() accepted input */
		numInputWords = 0;
		flag[2] = FALSE;   /* not sure about this one */
	}

	return (wordsMatch);
}

#pragma code-name (pop)

