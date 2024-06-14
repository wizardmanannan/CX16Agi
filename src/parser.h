/***************************************************************************
** parser.h
***************************************************************************/

#ifndef _PARSER_H_
#define _PARSER_H_

#include <string.h>
#include <stdio.h>

#include "general.h"
#include "words.h"
#include "view.h"
#include "picture.h"
#include "words.h"
#include "kernal.h"
#include "keyboard.h"

#define  NO_EVENT         0
#define  ASCII_KEY_EVENT  1
#define  SCAN_KEY_EVENT   2
#define  MENU_EVENT       3

/* An event as tested by the controller() test command is either a key or
** a menu item. */
typedef struct {
	byte type;     /* either key or menu item */
	byte eventID;  /* either scancode or menu item ID */
	byte asciiValue;
	byte scanCodeValue;
	boolean activated;
} EventType;

extern EventType b7Events[256];

extern int numInputWords, b7InputWords[];
extern char b7WordText[10][80];
extern boolean haveKey;
extern byte b7KeyState[256], b7AsciiState[256];
extern byte b7Directions[9];
extern int lastKey;

#pragma wrapped-call (push, trampoline, STRING_BANK)
extern void b7LookupWords(char* inputLine);
extern int b7Strcmp(char const* _Str1,char const* _Str2);
extern void b7ProcessString(char* stringPointer, byte stringBank, char* b7OutputString, byte outputStringBank);
extern char* b7GetInternalStringPtr(byte number, size_t* length);
extern void b7GetInternalString(char* promptStr, byte promptStringBank, byte stringNumber, int x, int y, int l);
void b7GetString(char* promptStr, byte promptStringBank, char* returnStr, byte returnStrBank, int x, int y, int l);
extern void b7GetEvent(EventType* event, byte eventNumber);
extern void b7SetEvent(EventType* event, byte eventNumber);
extern byte b7GetAsciiState(byte number);
extern void b7GetEvent(EventType* event, byte eventNumber);
extern byte b7GetKeyState(byte number);
extern void b7InitEvents();
extern void b7PollKeyboard();
extern byte b7GetInputWord(byte number);
extern void b7ActivateEvent(byte eventNumber);

extern char string[12][40];
#pragma wrapped-call (pop)

#endif  /* _PARSER_H_ */