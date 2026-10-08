#ifndef TELEPORT_PARSER_INPUT_HOST_H
#define TELEPORT_PARSER_INPUT_HOST_H

/* Host shims for compiling the real parser key path in the native C suite. */
#define _KERNAL_H_
#include "teleport_host.h"
#undef KEY_ENTER
#undef KEY_ESC
#include "controllers.h"

#define MAX_WORD_SIZE 41
#define PARSER_BANK 7
#define SPACE KEY_SPACE

extern byte key[];
extern int user_input_line;
extern boolean inputLineDisplayed;
void trampolineDebug(void (*callback)());

#define NO_EVENT         0
#define ASCII_KEY_EVENT  1
#define SCAN_KEY_EVENT   2
#define MENU_EVENT       3

typedef struct {
    byte type;
    byte eventID;
    byte asciiValue;
    byte scanCodeValue;
    boolean activated;
} EventType;

int teleportInputTestReadKey(void);
int b12FindSynonymNum(char* userWord, byte userWordBank);
#define GET_IN(output) do { (output) = teleportInputTestReadKey(); } while (0)

#endif
