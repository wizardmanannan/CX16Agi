/* Run production teleport C and production collision assembly on a 65C02.
   Only VERA reads, view storage, and message/keyboard services are replaced. */
#include "support/teleport_host.h"
#include <assert.h>
#include <stddef.h>

AGIFilePosType logdir[256];
int numLogics = 2, dirnOfEgo, controlMode;
byte horizon = 36;
static byte variables[256];
static boolean flags[256];
byte* var = variables;
boolean* flag = flags;
byte b7Directions[9], b9PreComputedPriority[256];
ViewTable viewtab[VIEW_TABLE_SIZE];
static byte terrain[168][160];
static unsigned int pixelReads;
byte offsetOfFlags = offsetof(ViewTable, flags);
byte offsetOfXPos = offsetof(ViewTable, xPos);
byte offsetOfYPos = offsetof(ViewTable, yPos);
byte offsetOfXSize = offsetof(ViewTable, xsize);
byte offsetOfPrevY = offsetof(ViewTable, previousY);
byte offsetOfPriority = offsetof(ViewTable, priority);
byte sizeOfViewTab = sizeof(ViewTable);

byte __fastcall__ testPriority(unsigned int xy)
{
    byte x = (byte)xy, y = xy >> 8;
    assert(x < 160 && y < 168);
    ++pixelReads;
    return terrain[y][x];
}
void b10GetLogicDirectory(AGIFilePosType* dest, AGIFilePosType* source) { *dest = *source; }
void getViewTab(ViewTable* dest, byte index) { *dest = viewtab[index]; }
void setViewTab(ViewTable* source, byte index) { viewtab[index] = *source; }
void memsetBanked(void* dest, int value, size_t length, byte bank)
{ assert(bank == 7); memset(dest, value, length); }
void b3DisplayMessageBox(char* m, byte b, byte r, byte c, byte p, byte w, boolean wrap)
{ assert(m && b == 7 && r == 255 && c == 255 && p == 1 && w == 32 && wrap); }
void b5WaitOnSpecificKeys(byte* keys, byte length)
{ assert(keys && length == 2); }
void b3ClearLastPlacedText(void) {}

static void reset(void)
{
    memset(viewtab, 0, sizeof viewtab);
    memset(terrain, 4, sizeof terrain);
    memset(flags, 0, sizeof flags);
    memset(variables, 0, sizeof variables);
    memset(b9PreComputedPriority, 8, sizeof b9PreComputedPriority);
    viewtab[0].xsize = 8; viewtab[0].ysize = 12;
    viewtab[0].xPos = 80; viewtab[0].yPos = 100;
    viewtab[0].flags = ANIMATED | DRAWN | UPDATE | MOTION;
    var[0] = 1; teleportPending = 0; pixelReads = 0;
}
static void blocked(const char* command)
{
    ViewTable before;
    byte beforeVar2 = var[2], beforeVar6 = var[6];
    int beforeDirection = dirnOfEgo, beforeControl = controlMode;
    byte beforeDirections[9];
    before = viewtab[0];
    memcpy(beforeDirections, b7Directions, sizeof beforeDirections);
    flag[0] = TRUE;
    assert(b7TeleportCommand(command));
    b6BeginTeleport();
    assert(!teleportPending);
    assert(!memcmp(&before, &viewtab[0], sizeof before));
    assert(flag[0] && !flag[3]);
    assert(var[2] == beforeVar2 && var[6] == beforeVar6);
    assert(dirnOfEgo == beforeDirection && controlMode == beforeControl);
    assert(!memcmp(beforeDirections, b7Directions, sizeof beforeDirections));
    assert(pixelReads <= 168 * 8);
}
static void max_path(void)
{
    reset();
    viewtab[0].xsize = viewtab[0].ysize = 1;
    viewtab[0].xPos = viewtab[0].yPos = 0;
    viewtab[0].flags |= IGNOREHORIZON;
    assert(b7TeleportCommand("teleport 1 159 167"));
    b6BeginTeleport();
    assert(!teleportPending);
    assert(viewtab[0].xPos == 159 && viewtab[0].yPos == 167);
    assert(pixelReads == 169); /* 168 probes plus the final flag publication. */
}
int main(void)
{
    reset();
    memset(terrain, 0, sizeof terrain);
    blocked("teleport 1 90 110"); /* Previously an unbounded spiral. */
    assert(pixelReads == 8);
    reset();
    memset(terrain[120], 0, 160);
    blocked("teleport 1 90 130"); /* Open destination behind a south wall. */
    reset();
    terrain[100][88] = 0;
    blocked("teleport 1 90 100"); /* Entire baseline matters. */
    reset();
    viewtab[1].flags = ANIMATED | DRAWN;
    viewtab[1].xPos = 90; viewtab[1].yPos = 110; viewtab[1].xsize = 8;
    blocked("teleport 1 90 110");
    reset();
    viewtab[0].flags |= ONLAND;
    memset(terrain[110], 3, 160);
    blocked("teleport 1 90 110");
    max_path();
    reset();
    terrain[100][80] = 2;
    assert(b7TeleportCommand("teleport 1 90 110"));
    b6BeginTeleport();
    assert(viewtab[0].xPos == 90 && viewtab[0].yPos == 110);
    assert(!flag[3]); /* Probing intermediate trigger lines has no side effects. */
    reset();
    terrain[110][90] = 2;
    assert(b7TeleportCommand("teleport 1 90 110")); b6BeginTeleport();
    assert(viewtab[0].xPos == 90 && viewtab[0].yPos == 110 && flag[3]);
    puts("PASS: real 65C02 collision code: wall, baseline, object, water and flag regressions");
    return 0;
}
