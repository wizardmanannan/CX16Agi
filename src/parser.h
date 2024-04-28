/***************************************************************************
** parser.h
***************************************************************************/

#include <string.h>
#include <stdio.h>

#include "general.h"
#include "words.h"
#include "view.h"
#include "picture.h"
#include "words.h"

#ifndef _PARSER_H_
#define _PARSER_H_

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
extern byte directions[9];
extern int lastKey;

#pragma wrapped-call (push, trampoline, WORD_BANK)
void b7ProcessString(char* stringPointer, byte stringBank, char* b7OutputString, byte outputStringBank);
void b7GetString(char* promptStr, byte promptStringBank, char* returnStr, byte returnStrBank, int x, int y, int l);
void b7GetEvent(EventType* event, byte eventNumber);
void b7SetEvent(EventType* event, byte eventNumber);
byte b7GetAsciiState(byte number);
void b7GetEvent(EventType* event, byte eventNumber);
byte b7GetKeyState(byte number);
void b7InitEvents();
void b7PollKeyboard();
byte b7GetInputWord(byte number);
void b7ActivateEvent(byte eventNumber);
#pragma wrapped-call (pop)

#endif  /* _PARSER_H_ */