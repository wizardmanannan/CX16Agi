#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "agifiles.h"
#include "controllers.h"
#include "parser.h"
#include "teleport.h"

extern char b7CurrentInputStr[41];
extern byte b1ControllerBits[NO_CONTROLLER_BITS];
void b7PollKeyboard(void);

static byte variables[256];
static boolean flags[256];
static int nextKey;
static boolean keyQueued;
static char lastMessage[80];

byte* var = variables;
boolean* flag = flags;
int controlMode = 1;
int dirnOfEgo;
byte horizon;
int numLogics;
AGIFilePosType logdir[NO_DIRECTORY_ENTRYS];
boolean inputLineDisplayed;

int teleportInputTestReadKey(void)
{
    if (!keyQueued) return 0;
    keyQueued = FALSE;
    return nextKey;
}

static void press(int key)
{
    nextKey = key;
    keyQueued = TRUE;
    b7PollKeyboard();
}

static void type(const char* text)
{
    while (*text) {
        if (*text == ' ') press(KEY_SPACE);
        else if (*text >= '0' && *text <= '9') press((byte)*text);
        else press(KEY_LOWER_A + (*text - 'a'));
        ++text;
    }
}

static boolean jumpPressed(void)
{
    return (b1ControllerBits[7 / 8] & (1 << (7 % 8))) != 0;
}

void b10GetLogicDirectory(AGIFilePosType* result, AGIFilePosType* location)
{
    *result = *location;
}

void getViewTab(ViewTable* result, byte index)
{
    (void)result;
    assert(index == 0);
}

void setViewTab(ViewTable* value, byte index)
{
    (void)value;
    assert(index == 0);
}

boolean b5IsDebuggingEnabled(void) { return FALSE; }
void bDbgShowPriority(void) {}
void trampolineDebug(void (*callback)()) { (void)callback; }

int b12FindSynonymNum(char* userWord, byte userWordBank)
{
    (void)userWord;
    (void)userWordBank;
    return -1;
}

void b3DisplayMessageBox(char* message, byte bank, byte row, byte col,
                         byte palette, byte width, boolean wrap)
{
    (void)bank; (void)row; (void)col; (void)palette; (void)width; (void)wrap;
    strncpy(lastMessage, message, sizeof lastMessage - 1);
    lastMessage[sizeof lastMessage - 1] = '\0';
}

void b5WaitOnSpecificKeys(byte* keys, byte length)
{
    assert(length == 2 && keys[0] == KEY_ENTER && keys[1] == KEY_ESC);
}

void b3ClearLastPlacedText(void) {}

int main(void)
{
    unsigned i;

    b1InitControllers();
    b1AssociateController('0', 0, 7);
    inputLineDisplayed = TRUE;

    press(KEY_0);
    assert(!b7CurrentInputStr[0] && jumpPressed());

    type("teleport ");
    press(KEY_0);
    assert(!jumpPressed());
    assert(strcmp(b7CurrentInputStr, "teleport 0") == 0);
    type(" 0 0");
    assert(strcmp(b7CurrentInputStr, "teleport 0 0 0") == 0);

    teleportPending = 1;
    press(KEY_ENTER);
    assert(!teleportPending && strstr(lastMessage, "canceled"));

    type("teleporter ");
    press(KEY_0);
    assert(jumpPressed());
    assert(strcmp(b7CurrentInputStr, "teleporter ") == 0);

    for (i = 0; i < sizeof b1ControllerBits; ++i) b1ControllerBits[i] = 0;
    puts("PASS: zero stays Jump normally and is text in teleport input");
    return 0;
}
