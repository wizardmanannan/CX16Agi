/* Production teleport.c with mocked hardware/engine services. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NDEBUG
#error These tests require assertions
#endif

AGIFilePosType logdir[NO_DIRECTORY_ENTRYS];
int numLogics, dirnOfEgo, controlMode;
byte horizon;
static byte variables[256];
static boolean flags[256];
byte* var = variables;
boolean* flag = flags;
byte b7Directions[9];
static ViewTable ego;
static byte terrain[168][160];
static int obstacleX, obstacleY;
static unsigned transitions, directoryReads, viewReads, viewWrites, probes, collisions;
static unsigned notices;
static char lastNotice[128];

void b10GetLogicDirectory(AGIFilePosType* result, AGIFilePosType* location)
{
    assert(location >= logdir && location < logdir + numLogics);
    ++directoryReads;
    *result = *location;
}
void getViewTab(ViewTable* result, byte index)
{
    assert(index == 0); ++viewReads; *result = ego;
}
void setViewTab(ViewTable* value, byte index)
{
    assert(index == 0); ++viewWrites; ego = *value;
}
void memsetBanked(void* dest, int value, size_t length, byte bank)
{
    assert(dest == b7Directions && value == 0 && length == 9 && bank == STRING_BANK);
    memset(dest, value, length);
}
/* This must never be called by teleport: only real game scripts may enter rooms. */
void b6NewRoom(void) { ++transitions; assert(!"synthetic room transition"); }
void b3DisplayMessageBox(char* message, byte bank, byte row, byte col,
                        byte palette, byte width, boolean wrap)
{
    assert(bank == STRING_BANK && row == 255 && col == 255);
    assert(palette == 1 && width == 32 && wrap);
    assert(strlen(message) < sizeof lastNotice);
    strcpy(lastNotice, message); ++notices;
}
void b5WaitOnSpecificKeys(byte* keys, byte length)
{
    assert(length == 2 && keys[0] == KEY_ENTER && keys[1] == KEY_ESC);
}
void b3ClearLastPlacedText(void) {}
boolean b9Collide(ViewTable* candidate, byte entry)
{
    assert(entry == 0);
    ++collisions;
    return obstacleY == candidate->yPos && obstacleX >= candidate->xPos &&
           obstacleX < candidate->xPos + candidate->xsize;
}
/* Model the collision-service contract, including CanBeHere's side effects.
   The production 65C02 routine is exercised separately by the CPU tests. */
boolean b9CanBeHere(ViewTable* candidate, byte entry)
{
    unsigned x;
    boolean water = TRUE, special = FALSE, valid = TRUE;
    assert(entry <= 1 && candidate->yPos < 168);
    assert(candidate->xPos + candidate->xsize <= 160);
    ++probes;
    if (!(candidate->flags & FIXEDPRIORITY)) candidate->priority = 8;
    if (candidate->priority == 15) water = FALSE;
    else {
        for (x = candidate->xPos; x < candidate->xPos + candidate->xsize; ++x) {
            byte p = terrain[candidate->yPos][x];
            if (p != 3) water = FALSE;
            if (p == 2) special = TRUE;
            if (p == 0 || (p == 1 && !(candidate->flags & IGNOREBLOCKS))) valid = FALSE;
        }
        if (water && (candidate->flags & ONLAND)) valid = FALSE;
        if (!water && (candidate->flags & ONWATER)) valid = FALSE;
    }
    if (entry == 0) {
        if (water) flag[0] = TRUE;
        if (special) flag[3] = TRUE;
    }
    return valid;
}
static void reset(int count)
{
    unsigned i;
    numLogics = count; teleportPending = 0;
    transitions = directoryReads = viewReads = viewWrites = probes = collisions = notices = 0;
    dirnOfEgo = controlMode = 0; horizon = 36;
    obstacleX = obstacleY = -1;
    memset(variables, 0, sizeof variables);
    memset(flags, 0, sizeof flags);
    memset(b7Directions, 0, sizeof b7Directions);
    memset(terrain, 4, sizeof terrain);
    memset(&ego, 0, sizeof ego);
    ego.xsize = 8; ego.ysize = 12; ego.xPos = 80; ego.yPos = 100;
    ego.flags = ANIMATED | DRAWN | UPDATE | MOTION;
    ego.stepTime = 9; ego.stepTimeCount = 5;
    var[0] = 1;
    for (i = 0; i < 256; ++i) {
        logdir[i].filePos = EMPTY; logdir[i].fileNum = 0;
    }
    if (count > 1) logdir[1].filePos = 0;
}
static boolean command(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    boolean handled;
    assert(copy); memcpy(copy, text, length);
    handled = b7TeleportCommand(copy);
    assert(!memcmp(text, copy, length)); free(copy);
    return handled;
}
static void queue(unsigned room, unsigned x, unsigned y)
{
    char text[64];
    snprintf(text, sizeof text, "teleport %u %u %u", room, x, y);
    assert(command(text));
}
static void accepted(unsigned x, unsigned y)
{
    unsigned before = viewWrites, i;
    b6BeginTeleport();
    assert(!teleportPending && viewWrites == before + 1);
    assert(ego.xPos == x && ego.yPos == y);
    assert(ego.previousX == x && ego.previousY == y);
    assert(!ego.direction && !var[2] && !var[6] && !dirnOfEgo);
    assert(ego.stepTime == 9 && ego.stepTimeCount == 5); /* No collision grace frame. */
    for (i = 0; i < 9; ++i) assert(!b7Directions[i]);
}
static void rejected(void)
{
    ViewTable before = ego;
    byte varsBefore[256], directionsBefore[9];
    boolean flagsBefore[256];
    int directionBefore = dirnOfEgo, controlBefore = controlMode;
    unsigned writesBefore = viewWrites, noticesBefore = notices;
    memcpy(varsBefore, var, sizeof varsBefore);
    memcpy(flagsBefore, flag, sizeof flagsBefore);
    memcpy(directionsBefore, b7Directions, sizeof directionsBefore);
    b6BeginTeleport();
    assert(!teleportPending && viewWrites == writesBefore && notices == noticesBefore + 1);
    assert(strstr(lastNotice, "rejected"));
    assert(!memcmp(&ego, &before, sizeof ego));
    assert(!memcmp(var, varsBefore, sizeof varsBefore));
    assert(!memcmp(flag, flagsBefore, sizeof flagsBefore));
    assert(!memcmp(b7Directions, directionsBefore, sizeof directionsBefore));
    assert(dirnOfEgo == directionBefore && controlMode == controlBefore);
}
static void test_input(void)
{
    static const char* const unrelated[] = {
        "", "t", "tele", "telepor", "look", "teleporter 1 2 3",
        "teleportation", " teleport 1 2 3", "TELEPORT 1 2 3", "teleport\t1 2 3"
    };
    static const char* const invalid[] = {
        "teleport", "teleport ", "teleport 1", "teleport 1 2",
        "teleport 0 1 1", "teleport 256 1 1", "teleport 65536 1 1",
        "teleport 1 160 1", "teleport 1 1 168", "teleport 1 256 1",
        "teleport 1 -1 1", "teleport +1 1 1", "teleport 1 1 -1",
        "teleport 1 1 1 extra", "teleport 1 1 1x", "teleport 1.0 1 1",
        "teleport 1 999999999999999999999999 1", "teleport 2 1 1",
        "teleport 1 \t1 1", "teleport 1 1 1\n", "teleport 1 \xff 1"
    };
    unsigned i;
    reset(256);
    for (i = 0; i < sizeof unrelated / sizeof unrelated[0]; ++i) assert(!command(unrelated[i]));
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        assert(command(invalid[i])); assert(!teleportPending);
    }
    assert(!transitions && !viewReads && !viewWrites);
    assert(command("teleport  001  080  100 ")); accepted(80, 100);
    queue(1, 90, 110);
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        assert(command(invalid[i])); assert(teleportPending == 1);
    }
    accepted(90, 110);
    queue(1, 100, 120); queue(1, 110, 130); accepted(110, 130);
    queue(1, 120, 140);
    assert(command("teleport 0 0 0") && !teleportPending);
    assert(strstr(lastNotice, "canceled"));
    puts("PASS: parsing, numeric overflow, queued requests, explicit cancellation");
}
static void test_bounds(void)
{
    unsigned n, x, y, before;
    reset(256);
    for (n = 1; n < 256; ++n) logdir[n].filePos = 0;
    for (n = 1; n < 256; ++n) {
        var[0] = (byte)n; queue(n, 80, 100); accepted(80, 100);
    }
    for (x = 0; x < 256; ++x) for (y = 0; y < 256; ++y) {
        ego.xPos = 80; ego.yPos = 100;
        queue(255, x, y);
        assert((teleportPending != 0) == (x < 160 && y < 168));
        if (teleportPending) {
            before = collisions;
            if (x <= 152 && y > 36) accepted(x, y); else rejected();
            assert(collisions - before <= 168);
        }
    }
    reset(0); queue(1, 1, 1); assert(!teleportPending && !directoryReads);
    reset(1); queue(1, 1, 1); assert(!teleportPending && !directoryReads);
    reset(255); queue(255, 1, 1); assert(!teleportPending && !directoryReads);
    reset(256); ego.xsize = ego.ysize = 1; ego.xPos = ego.yPos = 0;
    ego.flags |= IGNOREHORIZON;
    queue(1, 159, 167); accepted(159, 167);
    assert(collisions == 168 && probes == 169); /* Includes final flag publication. */
    puts("PASS: all room bytes, all 65,536 X/Y byte pairs, 168-check maximum");
}
static void test_no_synthetic_entry(void)
{
    ViewTable before;
    unsigned writesBefore;
    reset(256); logdir[2].filePos = 0;
    var[1] = 17; var[2] = 1; var[6] = 1; dirnOfEgo = 1;
    before = ego;
    queue(2, 90, 110);
    assert(strstr(lastNotice, "normal exit"));
    b6BeginTeleport();
    assert(!transitions && var[0] == 1 && var[1] == 17 && var[2] == 1);
    assert(var[6] == 1 && dirnOfEgo == 1 && teleportPending == 2);
    assert(!memcmp(&ego, &before, sizeof ego));
    /* Simulate the game's completed normal entry, not teleport's new.room. */
    var[1] = 1; var[0] = 2; flag[5] = TRUE;
    b6FinishTeleport();
    assert(!teleportPending && ego.xPos == 90 && ego.yPos == 110);
    assert(var[1] == 1 && flag[5] && !transitions);
    queue(1, 80, 100);
    var[1] = 2; var[0] = 3; /* Different exit or scripted redirect. */
    before = ego; writesBefore = viewWrites;
    b6FinishTeleport();
    assert(!teleportPending && viewWrites == writesBefore);
    assert(!memcmp(&ego, &before, sizeof ego));
    assert(var[1] == 2 && var[0] == 3 && strstr(lastNotice, "different room"));
    puts("PASS: normal-entry-only travel, preserved direction/previous room, redirects");
}
static void test_blocked_paths(void)
{
    unsigned x;
    reset(256); memset(terrain, 0, sizeof terrain);
    queue(1, 90, 110); rejected();
    assert(probes == 1); /* Fully blocked scene must return, never spiral. */
    reset(256);
    for (x = 0; x < 160; ++x) terrain[120][x] = 0;
    queue(1, 90, 130); rejected(); /* Free endpoint across a solid wall. */
    reset(256); terrain[100][88] = 0; /* Right end of baseline, not ego origin. */
    queue(1, 90, 100); rejected();
    reset(256); obstacleX = 90; obstacleY = 110;
    queue(1, 90, 110); rejected();
    reset(256); ego.flags |= ONLAND;
    memset(terrain[110], 3, 160); queue(1, 90, 110); rejected();
    reset(256); ego.flags |= ONWATER;
    queue(1, 90, 110); rejected();
    reset(256); memset(terrain, 1, sizeof terrain);
    queue(1, 90, 110); rejected();
    ego.flags |= IGNOREBLOCKS; queue(1, 90, 110); accepted(90, 110);
    reset(256); terrain[100][80] = 2; flag[0] = TRUE;
    queue(1, 90, 110); accepted(90, 110);
    assert(!flag[0] && !flag[3]); /* Intermediate special pixels are probes only. */
    reset(256); memset(terrain[110], 3, 160);
    queue(1, 90, 110); accepted(90, 110); assert(flag[0] && !flag[3]);
    reset(256); terrain[110][90] = 2;
    queue(1, 90, 110); accepted(90, 110); assert(!flag[0] && flag[3]);
    reset(256); controlMode = 1; queue(1, 90, 110); rejected();
    reset(256); ego.motion = MOVE_TO; queue(1, 90, 110); rejected();
    reset(256); ego.flags &= ~DRAWN; queue(1, 90, 110); rejected();
    reset(256); ego.xsize = 0; queue(1, 90, 110); rejected();
    reset(256); ego.xsize = 161; queue(1, 90, 110); rejected();
    reset(256); ego.ysize = 169; queue(1, 90, 110); rejected();
    reset(256); ego.xPos = 65535; queue(1, 90, 110); rejected();
    puts("PASS: walls, full baseline, objects, water, horizon, script control, atomic rejection");
}
static FILE* open_resource(const char* game, const char* upper, const char* lower)
{
    char path[4096]; FILE* file;
    int length = snprintf(path, sizeof path, "%s/%s", game, upper);
    assert(length >= 0 && (size_t)length < sizeof path);
    file = fopen(path, "rb");
    if (!file) {
        length = snprintf(path, sizeof path, "%s/%s", game, lower);
        assert(length >= 0 && (size_t)length < sizeof path); file = fopen(path, "rb");
    }
    if (!file) { perror(path); exit(EXIT_FAILURE); }
    return file;
}
static void test_game(const char* game)
{
    byte data[769]; size_t length; unsigned room, valid = 0;
    FILE* file = open_resource(game, "LOGDIR", "logdir");
    length = fread(data, 1, sizeof data, file); assert(!ferror(file)); fclose(file);
    assert(length && length <= 768 && length % 3 == 0); reset((int)(length / 3));
    for (room = 0; room < (unsigned)numLogics; ++room) {
        byte* entry = data + room * 3;
        unsigned long offset = ((unsigned long)(entry[0] & 15) << 16)
                             | ((unsigned long)entry[1] << 8) | entry[2];
        logdir[room].filePos = offset; logdir[room].fileNum = entry[0] >> 4;
        if (offset != EMPTY) {
            char upper[16], lower[16];
            snprintf(upper, sizeof upper, "VOL.%u", logdir[room].fileNum);
            snprintf(lower, sizeof lower, "vol.%u", logdir[room].fileNum);
            file = open_resource(game, upper, lower);
            assert(fseek(file, (long)offset, SEEK_SET) == 0);
            assert(fgetc(file) == 0x12 && fgetc(file) == 0x34); fclose(file);
        }
    }
    for (room = 1; room < 256; ++room) {
        var[0] = 0; queue(room, 80, 100);
        assert((teleportPending != 0) ==
               (room < (unsigned)numLogics && logdir[room].filePos != EMPTY));
        if (teleportPending) {
            b6BeginTeleport(); assert(var[0] == 0 && !transitions);
            var[0] = (byte)room; /* Model a normal room entry. */
            b6FinishTeleport(); assert(!teleportPending); ++valid;
        }
    }
    printf("PASS: %s: %u resource destinations; no synthetic transitions\n", game, valid);
}
int main(int argc, char** argv)
{
    int i;
    test_no_synthetic_entry(); test_input(); test_bounds(); test_blocked_paths();
    for (i = 1; i < argc; ++i) test_game(argv[i]);
    return 0;
}
