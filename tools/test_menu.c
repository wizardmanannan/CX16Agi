/* Execute the production menu producers; only bank/keyboard boundaries are stubs. */
#define _MEMORYMANAGER_H_
#define _HELPERS_H_
#define DEPENDENCYRESOLVER_H
#define _PARSER_H_
#define MENU_BANK 0xA
#define TILE_LAYER_WIDTH 64
#define asm(...) ((void)0)
static unsigned key_index;
static unsigned char keys[4];
#define GET_IN(ch) ((ch) = keys[key_index++])
#include "../src/menu.h"
size_t strLenBanked(char *, int);
void memCpyBankedBetween(byte *, byte, byte *, byte, size_t);
#include "../src/menu.c"

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s\n", m); exit(1); } } while (0)
int currentLog;
static char message[600];
static unsigned copies, activations;
char *getMessagePointer(byte logic, byte number) { (void)logic; (void)number; return message; }
void b5GetLogicFile(LOGICFile *file, byte number) { (void)number; memset(file, 0, sizeof(*file)); }
size_t strLenBanked(char *text, int bank) { (void)bank; return strlen(text); }
void memCpyBankedBetween(byte *dest, byte bank, byte *src, byte source_bank, size_t length) {
    (void)bank; (void)source_bank; ++copies; memcpy(dest, src, length);
}
boolean b1SetController(byte controller) { ++activations; return controller != NOT_ASSOCIATED; }

static void reset(void) {
    memset(the_menu, 0, sizeof(the_menu));
    numOfMenus = 0; nextMenuTextBufferAddr = menuTextBuffer; copies = 0;
    bAInitMenuState(); bAMenuChildInit(); strcpy(message, "menu");
    key_index = activations = 0; memset(keys, KEY_ESC, sizeof(keys));
}

int main(void) {
    reset();
    for (unsigned i = 0; i < MAX_MENUS; ++i) bASetMenu(1);
    CHECK(numOfMenus == MAX_MENUS, "Maximum menu count was not accepted");
    unsigned before = copies;
    bASetMenu(1);
    CHECK(numOfMenus == MAX_MENUS && copies == before, "Full menu producer copied or appended another entry");
    reset();
    bASetMenuItem(1, 7);
    CHECK(copies == 0 && numOfMenus == 0, "Child menu without a parent changed storage");
    bASetMenu(1);
    for (unsigned i = 0; i < MAX_MENU_CHILDREN; ++i) bASetMenuItem(1, i);
    before = copies;
    bASetMenuItem(1, 7);
    CHECK(bAMenuChildCount[0] == MAX_MENU_CHILDREN && copies == before,
          "Full child menu consumed text-buffer capacity");
    reset();
    LOGICFile file = {0};
    memset(message, 'X', MENU_TEXT_BUFFER_SIZE); message[MENU_TEXT_BUFFER_SIZE] = 0;
    CHECK(bAStoreMessageInBuffer(&file, 1) == NULL && copies == 0 && nextMenuTextBufferAddr == menuTextBuffer,
          "Oversized menu text copied or advanced its buffer cursor");
    memset(message, 'X', MENU_TEXT_BUFFER_SIZE - 1); message[MENU_TEXT_BUFFER_SIZE - 1] = 0;
    CHECK(bAStoreMessageInBuffer(&file, 1) == menuTextBuffer && copies == 1,
          "Exact-capacity menu text was rejected");
    CHECK(nextMenuTextBufferAddr == menuTextBuffer + MENU_TEXT_BUFFER_SIZE,
          "Exact-capacity menu copy used the wrong length");
    strcpy(message, "x");
    CHECK(bAStoreMessageInBuffer(&file, 1) == NULL && copies == 1,
          "Exhausted menu text buffer accepted another string");
    reset();
    bAGoToNextMenu(-1); bAGoToNextChildMenu(-1);
    CHECK(bAMenuSelected == 0 && bAMenuChildSelected == 0,
          "Empty menu navigation underflowed its selection");
    reset(); bASetMenu(1);
    memset(message, 'X', 256); message[256] = 0;
    bASetMenuItem(1, 7);
    CHECK(bAMenuChildWidth[0] == MENU_BAR_WIDTH,
          "Long menu child width wrapped or escaped the visible bar");
    MENU invalid = {"x", 7};
    bASetMenuChild(&invalid, 255);
    CHECK(bAMenuChildCount[0] == 1, "Invalid parent index changed child storage");
    reset(); bASetMenu(1); bASetMenuItem(1, 7);
    bASetMenuControllerEnabled(7, FALSE);
    keys[0] = KEY_ENTER; keys[1] = KEY_ESC;
    bAShowMenu(TRUE);
    CHECK(activations == 0 && key_index == 2 && !menuShown,
          "Disabled menu item activated a controller or consumed ENTER as selection");
    reset(); bASetMenu(1); bASetMenuItem(1, 7);
    bASetMenuControllerEnabled(7, TRUE); keys[0] = KEY_ENTER;
    bAShowMenu(TRUE);
    CHECK(activations == 1 && key_index == 1 && !menuShown,
          "Enabled menu item failed to activate exactly once");
    reset(); keys[0] = KEY_ENTER; keys[1] = KEY_ESC;
    bAShowMenu(TRUE);
    CHECK(activations == 0 && key_index == 2,
          "Empty menu activated an unpublished child");
    puts("Menu producer checks pass: counts, empty/full parents, text capacity and empty navigation");
    return 0;
}
