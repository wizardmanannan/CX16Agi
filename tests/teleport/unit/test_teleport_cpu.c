/* The production teleport assembly on a simulated 65C02: grammar, tours,
   room entry and placement, with the production collision routines.
   Only VERA reads, message boxes, room entry and files are replaced. */
#include "../support/teleport_sim.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sim65.h>

byte terrain[168][160];

byte __fastcall__ testPriority(unsigned int xy)
{
    byte x = (byte)xy, y = xy >> 8;
    assert(x < 160 && y < 168);
    ++pixelReads;
    return terrain[y][x];
}

/* A whole priority row, one byte per X. */
byte* testRowBuffer;
unsigned rowReads;
void __fastcall__ testReadRow(byte y)
{
    assert(y < 168);
    ++rowReads;
    memcpy(testRowBuffer, terrain[y], 160);
}

/* A synthetic game: VOL.0 holds hand-assembled room logics for the
   trigger reader. Every room uses the quiet logic unless a test points it
   at another. */
static FILE* volume;
static unsigned long volumeEnd, quietLogic;

static unsigned long logic(const byte* code, unsigned length)
{
    unsigned long at = volumeEnd;
    byte header[7];
    static const byte messages[3] = { 0, 0, 0 };
    unsigned size = 2 + length + sizeof messages;
    header[0] = 0x12; header[1] = 0x34; header[2] = 0;
    header[3] = (byte)size; header[4] = (byte)(size >> 8);
    header[5] = (byte)length; header[6] = (byte)(length >> 8);
    assert(fwrite(header, 1, 7, volume) == 7);
    assert(fwrite(code, 1, length, volume) == length);
    assert(fwrite(messages, 1, 3, volume) == 3);
    volumeEnd += 7 + length + 3;
    assert(!fflush(volume));
    return at;
}

static void reset(void)
{
    unsigned i;
    memset(viewtab, 0, sizeof viewtab);
    memset(terrain, 4, sizeof terrain);
    memset(testFlags, 0, sizeof testFlags);
    memset(testVars, 0, sizeof testVars);
    memset(b9PreComputedPriority, 8, sizeof b9PreComputedPriority);
    memset(b7Directions, 0, sizeof b7Directions);
    viewtab[0].xsize = 8; viewtab[0].ysize = 12;
    viewtab[0].xPos = 80; viewtab[0].yPos = 100;
    viewtab[0].stepTime = 9; viewtab[0].stepTimeCount = 5;
    viewtab[0].flags = ANIMATED | DRAWN | UPDATE | MOTION;
    var[0] = 1; horizon = 36; controlMode = dirnOfEgo = 0;
    teleportPending = teleportTour = 0; testJiffies = 0;
    pixelReads = notices = waits = clears = transitions = 0;
    lastMessage[0] = 0;
    numLogics = 256;
    for (i = 0; i < 256; ++i) {
        logdir[i].filePos = quietLogic; picdir[i].filePos = 0; logdir[i].fileNum = 0;
    }
    logdir[0].filePos = EMPTY;
}

static boolean command(const char* text) { return b7TeleportCommand(text); }

/* One interpreter cycle, as meka.c runs the hooks. */
/* One interpreter cycle's teleport calls, as in meka.c. */
static void cycle(void)
{
    flag[5] = FALSE;
    if (teleportPending | teleportTour) b6TeleportCycle();
    if (teleportPending) b6TeleportAfterLogic();
}

static void go(const char* text)
{
    assert(command(text));
    assert(teleportPending);
    cycle();
    assert(!teleportPending);
}

static void at(byte x, byte y)
{
    if (viewtab[0].xPos != x || viewtab[0].yPos != y)
        printf("ego at (%u,%u), expected (%u,%u)\n", viewtab[0].xPos, viewtab[0].yPos, x, y);
    assert(viewtab[0].xPos == x && viewtab[0].yPos == y);
}

static void placed(byte x, byte y)
{
    unsigned i;
    at(x, y);
    assert(viewtab[0].previousX == x && viewtab[0].previousY == y);
    assert(!viewtab[0].direction && viewtab[0].staleCounter == 1);
    assert(!var[2] && !var[6] && !dirnOfEgo);
    assert(viewtab[0].stepTime == 9 && viewtab[0].stepTimeCount == 5);
    for (i = 0; i < 9; ++i) assert(!b7Directions[i]);
}

static void rejected(const char* text)
{
    ViewTable before;
    byte directions[9];
    before = viewtab[0];
    memcpy(directions, b7Directions, sizeof directions);
    flag[0] = TRUE;
    go(text);
    assert(!memcmp(&before, &viewtab[0], sizeof before));
    assert(flag[0] && !flag[3] && !var[2] && !var[6]);
    assert(!memcmp(directions, b7Directions, sizeof directions));
    assert(strstr(lastMessage, "rejected"));
}

static void test_layout(void)
{
    assert((unsigned)teleportVtXPos == offsetof(ViewTable, xPos));
    assert((unsigned)teleportVtYPos == offsetof(ViewTable, yPos));
    assert((unsigned)teleportVtXSize == offsetof(ViewTable, xsize));
    assert((unsigned)teleportVtYSize == offsetof(ViewTable, ysize));
    assert((unsigned)teleportVtDirection == offsetof(ViewTable, direction));
    assert((unsigned)teleportVtMotion == offsetof(ViewTable, motion));
    assert((unsigned)teleportVtPriority == offsetof(ViewTable, priority));
    assert((unsigned)teleportVtFlags == offsetof(ViewTable, flags));
    assert((unsigned)teleportVtStale == offsetof(ViewTable, staleCounter));
    assert((unsigned)teleportVtPrevX == offsetof(ViewTable, previousX));
    assert((unsigned)teleportVtPrevY == offsetof(ViewTable, previousY));
    assert((unsigned)teleportVtSize == sizeof(ViewTable));
    puts("PASS: ViewTable offsets used by the assembly match graphics.h");
}

static void test_placement(void)
{
    reset(); memset(terrain, 0, sizeof terrain);
    rejected("teleport 1 90 110"); /* Never an unbounded search. */
    assert(pixelReads == 8);
    reset(); memset(terrain[120], 0, 160);
    go("teleport 1 90 130"); placed(90, 119); /* Stops before the wall. */
    assert(strstr(lastMessage, "nearest reachable"));
    reset(); terrain[100][88] = 0;
    rejected("teleport 1 90 100"); /* The whole baseline counts. */
    reset();
    viewtab[1].flags = ANIMATED | DRAWN;
    viewtab[1].xPos = 90; viewtab[1].yPos = 110; viewtab[1].xsize = 8;
    go("teleport 1 90 110"); placed(81, 101); /* Other objects block. */
    reset(); viewtab[0].flags |= ONLAND; memset(terrain[110], 3, 160);
    go("teleport 1 90 110"); placed(89, 109);
    reset(); memset(terrain[100] + 90, 13, 8);
    go("teleport 1 100 100"); placed(82, 100); /* Foreground hides ego. */
    reset(); memset(terrain[100] + 90, 6, 8);
    go("teleport 1 100 100"); placed(82, 100); /* In front of a wall is not ground. */
    reset(); viewtab[0].xsize = viewtab[0].ysize = 1;
    viewtab[0].xPos = viewtab[0].yPos = 0; viewtab[0].flags |= IGNOREHORIZON;
    go("teleport 1 159 167"); placed(159, 167);
    assert(pixelReads == 337); /* 168 probes of 2 reads, then publication. */
    reset(); terrain[100][80] = 2;
    go("teleport 1 90 110"); placed(90, 110);
    assert(!flag[3]); /* Probing a trigger line has no effect. */
    reset(); terrain[110][90] = 2; /* A trigger line at the target: stop before it. */
    go("teleport 1 90 110"); placed(89, 109); assert(!flag[3]);
    assert(strstr(lastMessage, "nearest reachable"));
    reset(); memset(terrain[110], 3, 160); /* Water at the target, likewise. */
    go("teleport 1 90 110"); placed(89, 109); assert(!flag[0]);
    reset(); memset(terrain[110], 2, 160); memset(terrain[109], 2, 160);
    viewtab[0].yPos = 120; /* Every route crosses the line; land past it. */
    go("teleport 1 80 100"); placed(80, 100); assert(!flag[3]);
    reset(); flag[0] = flag[3] = TRUE;
    go("teleport 1 90 110"); assert(!flag[0] && !flag[3]); /* Old spot's flags go. */
    reset(); b7Directions[3] = 1; go("tp 1 85 105"); placed(85, 105);
    reset(); controlMode = 1; rejected("teleport 1 90 110");
    reset(); viewtab[0].motion = MOVE_TO; rejected("teleport 1 90 110");
    reset(); viewtab[0].flags &= ~DRAWN; rejected("teleport 1 90 110");
    reset(); viewtab[0].xsize = 0; rejected("teleport 1 90 110");
    reset(); viewtab[0].xsize = 161; rejected("teleport 1 90 110");
    reset(); viewtab[0].ysize = 169; rejected("teleport 1 90 110");
    reset(); viewtab[0].xPos = 65535u; rejected("teleport 1 90 110");
    reset(); viewtab[0].xPos = 336; rejected("teleport 1 90 110");
    reset(); memset(terrain, 1, sizeof terrain); rejected("teleport 1 90 110");
    viewtab[0].flags |= IGNOREBLOCKS; go("teleport 1 90 110"); placed(90, 110);
    puts("PASS: placement: walls, baselines, objects, water, scenery, triggers, control, atomic rejection");
}

static void test_room_entry(void)
{
    ViewTable before;
    reset();
    var[1] = 17; var[2] = 1; var[6] = 1; dirnOfEgo = 1;
    assert(command("teleport 2 90 110") && teleportPending == 2);
    flag[5] = FALSE;
    b6BeginTeleport();
    assert(transitions == 1 && var[0] == 2 && var[1] == 1 && !var[2] && !var[6] && !dirnOfEgo);
    assert(teleportPending == 2);
    at(90, 110); /* Parked at the target while the room initializes. */
    b6FinishTeleport(); /* After the room script has run. */
    assert(!teleportPending); placed(90, 110);
    assert(command("teleport 3 80 100"));
    b6BeginTeleport();
    var[0] = 4; /* The room script sent ego elsewhere. */
    before = viewtab[0];
    b6FinishTeleport();
    assert(!teleportPending && !memcmp(&before, &viewtab[0], sizeof before));
    assert(strstr(lastMessage, "different room"));
    reset(); flag[5] = TRUE;
    terrain[100][80] = 0; /* The old room's coordinates mean nothing here. */
    assert(command("tp 2 100 110")); b6BeginTeleport(); b6FinishTeleport(); placed(100, 110);
    reset(); memset(terrain[110] + 100, 13, 8);
    assert(command("tp 2 100 110")); b6BeginTeleport(); b6FinishTeleport(); placed(100, 109);
    assert(strstr(lastMessage, "nearest reachable"));
    reset(); memset(terrain, 0, sizeof terrain);
    assert(command("tp 2 100 110")); b6BeginTeleport(); b6FinishTeleport();
    assert(var[0] == 2 && strstr(lastMessage, "rejected")); /* Room changes; no landing. */
    puts("PASS: room entry through new.room, placement after initialization, redirects");
}

static void test_automatic_landing(void)
{
    reset();
    assert(command("teleport 1") && teleportPending == 1 && teleportX == TELEPORT_AUTO);
    cycle(); /* Already in room 1: ego stays put. */
    assert(!teleportPending && !notices); at(80, 100);
    assert(command("tp 2")); cycle();
    assert(transitions == 1 && var[0] == 2 && !notices);
    placed(76, (36 + 168) / 2); /* The middle of the walkable area. */
    reset(); horizon = 100;
    assert(command("tp 2")); cycle(); placed(76, (100 + 168) / 2);
    reset(); memset(terrain[102] + 60, 0, 40);
    assert(command("tp 2")); cycle();
    assert(var[0] == 2 && viewtab[0].yPos != 102 && !notices); /* No adjustment notice. */
    puts("PASS: optional X Y: automatic landing, same-room no-op, no notice");
}

static void usage(const char* text)
{
    unsigned before = notices;
    byte pending = teleportPending;
    assert(command(text));
    if (notices != before + 1 || !strstr(lastMessage, "Use:")) printf("not rejected: \"%s\"\n", text);
    assert(notices == before + 1 && strstr(lastMessage, "Use:") && waits);
    assert(teleportPending == pending && !teleportTour);
}

static void test_grammar(void)
{
    static const char* const unrelated[] = {
        "", "t", "tele", "telepor", "look", "teleporter 1 2 3", "teleportation",
        " teleport 1 2 3", "TELEPORT 1 2 3", "teleport\t1 2 3", "tpx 1", "tp1", "t p 1"
    };
    static const char* const invalid[] = {
        "teleport", "teleport ", "teleport 1 2", "teleport 0 1 1", "teleport 256 1 1",
        "teleport 65536 1 1", "teleport 1 160 1", "teleport 1 1 168", "teleport 1 256 1",
        "teleport 1 -1 1", "teleport +1 1 1", "teleport 1 1 -1", "teleport 1 1 1 extra",
        "teleport 1 1 1x", "teleport 1.0 1 1", "teleport 1 999999999999999999999999 1",
        "teleport 1 \t1 1", "teleport 1 1 1\n", "teleport 1 \xff 1", "tp:", "tp:5:", "tp::1",
        "tp:5:1,,2", "tp:5:1,", "tp:256:1,2", "tp:x:1,2", "tp:5:0 0 0,1", "tp 0",
        "tp:1,2,3,4,5,6,7,8,9", "tp a b c d e f g h i j k l m", "tp abcdefghijklmnop",
        "tp :", "tp a,b", "teleport:5:1:2", "tp 5 kings", "tp kings 5", "tp kings 5 5 5",
        "tp 1 5 kings", "tp 1 2 3 4", "tp 255 1 1"
    };
    unsigned i;
    static const char* const typing[] = { "tp", "tp ", "tp:", "teleport", "teleport ", "teleport:", "tp:5:a," };
    reset(); numLogics = 255;
    for (i = 0; i < sizeof unrelated / sizeof unrelated[0]; ++i) {
        assert(!command(unrelated[i]));
        assert(!b7TeleportInputPrefix(unrelated[i])); /* keys stay game keys */
    }
    for (i = 0; i < sizeof typing / sizeof typing[0]; ++i) assert(b7TeleportInputPrefix(typing[i]));
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) usage(invalid[i]);
    assert(!transitions);
    assert(command("teleport  001  080  100 ") && teleportPending == 1);
    assert(teleportX == 80 && teleportY == 100);
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) usage(invalid[i]); /* Queue kept. */
    assert(command("tp 254 159 167") && teleportPending == 254 && teleportX == 159 && teleportY == 167);
    assert(command("tp stop") && !teleportPending && strstr(lastMessage, "canceled"));
    assert(command("tp 3")); assert(command("teleport 0 0 0") && !teleportPending);
    assert(strstr(lastMessage, "canceled"));
    assert(command("tp   stop  ") && strstr(lastMessage, "canceled"));
    logdir[7].filePos = EMPTY; usage("tp 7"); usage("tp:7,1"); logdir[7].filePos = 0;
    numLogics = 0; usage("tp 1");
    numLogics = 256; assert(command("tp 255") && teleportPending == 255);
    assert(command("teleport:5:2") && teleportPending == 2 && !teleportTour);
    assert(command("tp:2 10 120, 3 ,4") && teleportPending == 2 && teleportTour);
    assert(teleportX == 10 && teleportY == 120);
    assert(!strcmp(lastMessage, "Teleport tour, 5 seconds per stop: 2, 3, 4"));
    assert(command("tp:0:1,2,3,4,5,6,7,8") && teleportTour);
    assert(!strcmp(lastMessage, "Teleport tour, 0 seconds per stop: 1, 2, 3, 4, 5, 6, 7, 8"));
    assert(command("tp:255:9,10") && bFTeleportDelay == 255);
    assert(!strcmp(lastMessage, "Teleport tour, 255 seconds per stop: 9, 10"));
    assert(command("tp:100:9,10") && !strcmp(lastMessage, "Teleport tour, 100 seconds per stop: 9, 10"));
    assert(command("tp stop") && !teleportTour && !teleportPending);
    puts("PASS: grammar: tp alias, colon tours, delays, limits, numbers, cancellation");
}

static void test_tour(void)
{
    unsigned before;
    reset(); var[0] = 3;
    assert(command("tp:2:1,2 90 110,3"));
    assert(teleportTour && teleportPending == 1);
    cycle(); assert(var[0] == 1 && !teleportPending && teleportTour);
    testJiffies = 119; cycle(); assert(!teleportPending);
    testJiffies = 120; cycle(); assert(teleportPending == 2);
    cycle(); assert(var[0] == 2 && teleportTour); placed(90, 110);
    before = notices; controlMode = 1; /* Stop 3 cannot place: silently. */
    testJiffies = 239; cycle(); assert(!teleportPending); /* Arrival was at 120. */
    testJiffies = 240; cycle(); assert(teleportPending == 3);
    cycle(); assert(var[0] == 3 && !teleportPending && !teleportTour && notices == before);
    controlMode = 0;
    testJiffies = 65500u; assert(command("tp:1:1,2")); cycle(); /* Clock wraps. */
    assert(var[0] == 1 && teleportTour);
    testJiffies = 65500u + 59; cycle(); assert(!teleportPending);
    testJiffies = (65500u + 60) & 0xffff; cycle(); assert(teleportPending == 2);
    cycle(); assert(!teleportTour);
    assert(command("tp:9:1,2")); cycle(); assert(teleportTour);
    assert(command("tp 3") && !teleportTour && teleportPending == 3); /* Replaced. */
    assert(command("tp:9:1,2")); cycle();
    usage("tp 1 2"); assert(!teleportTour); /* Any command ends a tour. */
    puts("PASS: tours: delays after arrival, quiet failures, clock wrap, replacement");
}

/* Room scripts' position triggers (teleportTriggers.s). Boxes are
   (85,105)-(95,115) around the target (90,110); ego is 8 wide. */
#define BOX 0x0B, 0x00, 85, 105, 95, 115
#define BLOCK 0xFF, 0x02, 0x00, 0x01, 0x05 /* end of condition; { v5++ } */
static void lands(const byte* code, unsigned length, byte x, byte y, const char* why)
{
    reset();
    logdir[2].filePos = logic(code, length);
    assert(command("tp 2 90 110"));
    cycle();
    if (viewtab[0].xPos != x || viewtab[0].yPos != y) printf("%s:\n", why);
    placed(x, y);
}

byte __fastcall__ b1TeleportTriggers(byte* zones);

/* The boxes the trigger reader reports for room 2 running `code`, and
   whether it said the walk could not see everything. */
static byte zones[1 + 4 * 63], partial;
static byte armedBoxes(const byte* code, unsigned length)
{
    reset();
    var[0] = 2;
    logdir[2].filePos = logic(code, length);
    memset(zones, 0xEE, sizeof zones);
    partial = b1TeleportTriggers(zones);
    assert(!testFileOpen());
    return zones[0];
}

static void test_quiet_landing(void)
{
    static const byte armed[] = { 0xFF, BOX, BLOCK, 0x00 };
    static const byte flagged[] = { 0xFF, 0x07, 50, BOX, BLOCK, 0x00 };
    static const byte spoken[] = { 0xFF, 0x0E, 0x01, 0x05, 0x00, BOX, BLOCK, 0x00 };
    static const byte negated[] = { 0xFF, 0xFD, BOX, BLOCK, 0x00 };
    static const byte either[] = { 0xFF, 0xFC, BOX, 0x07, 52, 0xFC, BLOCK, 0x00 };
    static const byte room[] = { 0xFF, 0x01, 0x00, 2, BOX, BLOCK, 0x00 };
    static const byte elsewhere[] = { 0xFF, 0x01, 0x00, 3, BOX, BLOCK, 0x00 };
    static const byte other[] = { 0xFF, 0x0B, 0x01, 85, 105, 95, 115, BLOCK, 0x00 };
    /* if (isset(f51)) { v5++ } else { if (box) {} }: f51 may be clear */
    static const byte elseArmed[] = { 0xFF, 0x07, 51, 0xFF, 0x05, 0x00, 0x01, 0x05,
        0xFE, 0x0C, 0x00, 0xFF, BOX, BLOCK, 0x00 };
    /* if (v0 == 2) { v5++ } else { if (box) {} }: in room 2 the else cannot run */
    static const byte elseIdle[] = { 0xFF, 0x01, 0x00, 2, 0xFF, 0x05, 0x00, 0x01, 0x05,
        0xFE, 0x0C, 0x00, 0xFF, BOX, BLOCK, 0x00 };
    /* if (isset(f51)) { if (box) {} } */
    static const byte nestedIdle[] = { 0xFF, 0x07, 51, 0xFF, 0x0C, 0x00, 0xFF, BOX, BLOCK, 0x00 };
    static const byte everywhere[] = { 0xFF, 0x0B, 0x00, 0, 0, 159, 167, BLOCK, 0x00 };
    assert(armedBoxes(armed, sizeof armed) == 1);
    assert(zones[1] == 85 && zones[2] == 105 && zones[3] == 95 && zones[4] == 115);
    assert(!armedBoxes(flagged, sizeof flagged));
    lands(armed, sizeof armed, 90, 104, "an armed box moves the landing out of it");
    assert(strstr(lastMessage, "nearest reachable"));
    lands(flagged, sizeof flagged, 90, 110, "flag 50 is clear: the box cannot fire");
    flag[50] = TRUE;
    logdir[2].filePos = logic(flagged, sizeof flagged);
    assert(command("tp 2 90 110")); var[0] = 1; cycle(); placed(90, 104);
    lands(spoken, sizeof spoken, 90, 110, "said() needs the player");
    lands(negated, sizeof negated, 90, 110, "not(box) fires outside it, not in it");
    lands(either, sizeof either, 90, 104, "or(box, f52): the box alone can fire it");
    lands(room, sizeof room, 90, 104, "v0 == 2 holds in room 2");
    lands(elsewhere, sizeof elsewhere, 90, 110, "v0 == 3 does not");
    lands(other, sizeof other, 90, 110, "another object's box");
    lands(elseArmed, sizeof elseArmed, 90, 104, "f51 clear: the else branch's box is armed");
    lands(elseIdle, sizeof elseIdle, 90, 110, "the else branch cannot run in room 2");
    lands(nestedIdle, sizeof nestedIdle, 90, 110, "f51 clear: the inner box cannot fire");
    reset(); logdir[2].filePos = logic(everywhere, sizeof everywhere);
    assert(command("tp 2 90 110")); cycle(); placed(90, 110);
    assert(strstr(lastMessage, "No quiet spot")); /* Walkable fallback, said so. */
    reset(); logdir[2].filePos = logic(everywhere, sizeof everywhere);
    assert(command("tp:0:2 90 110,3")); cycle(); placed(90, 110);
    assert(!strstr(lastMessage, "No quiet spot")); /* Tours stay silent. */
    reset(); logdir[1].filePos = logic(armed, sizeof armed);
    go("teleport 1 90 110"); placed(84, 104); /* Same room: the path stops short. */
    {   /* Boxes in logics the room calls: call(9), and call.v(v30) with v30 = 9. */
        static const byte calls[] = { 0x16, 9, 0x00 };
        static const byte callsVariable[] = { 0x17, 30, 0x00 };
        unsigned long helper = logic(armed, sizeof armed), caller = logic(calls, sizeof calls);
        unsigned long variableCaller = logic(callsVariable, sizeof callsVariable);
        reset(); logdir[9].filePos = helper; logdir[2].filePos = caller;
        assert(command("tp 2 90 110")); cycle(); placed(90, 104);
        reset(); logdir[9].filePos = helper; logdir[2].filePos = variableCaller; var[30] = 9;
        assert(command("tp 2 90 110")); cycle(); placed(90, 104);
    }
    /* if (f1) { if (f2) { goto past the end } if (box) {} } with f1 and f2
       set: the goto leaves the outer block, so it is an early exit, not an
       else (which f2 would make inactive) - the box after it stays armed. */
    {
        static const byte exits[] = { 0xFF, 0x07, 1, 0xFF, 0x15, 0x00,
            0xFF, 0x07, 2, 0xFF, 0x03, 0x00, 0xFE, 0x20, 0x00,
            0xFF, BOX, BLOCK, 0x00 };
        reset(); flag[1] = flag[2] = TRUE;
        var[0] = 2; logdir[2].filePos = logic(exits, sizeof exits);
        memset(zones, 0xEE, sizeof zones);
        b1TeleportTriggers(zones);
        assert(zones[0] == 1);
    }
    /* get.posn(ego, v40, v41): number tests on v40/v41 are "might be" and
       make a box, kept only if the block does something. */
    {
        /* if (v40 < 50 && v41 > 120) { new.room(5) } */
        static const byte guarded[] = { 0x27, 0, 40, 41, 0xFF, 0x03, 40, 50, 0x05, 41, 120, 0xFF,
            0x02, 0x00, 0x12, 5, 0x00 };
        /* if (v40 < 50) { reposition.to(ego, 60, 100) }: steering, no box */
        static const byte steering[] = { 0x27, 0, 40, 41, 0xFF, 0x03, 40, 50, 0xFF,
            0x04, 0x00, 0x3B, 0, 60, 100, 0x00 };
        /* if (v40 < 50 || f9) { new.room(5) }: not expressible as a box */
        static const byte either[] = { 0x27, 0, 40, 41, 0xFF, 0xFC, 0x03, 40, 50, 0x07, 9, 0xFC, 0xFF,
            0x02, 0x00, 0x12, 5, 0x00 };
        /* if (v40 > 100) { if (box) { v5++ } }: v40 is stale (0); the inner
           box must not be judged dead by it */
        static const byte stale[] = { 0x27, 0, 40, 41, 0xFF, 0x05, 40, 100, 0xFF, 0x0D, 0x00,
            0xFF, BOX, BLOCK, 0x00 };
        assert(armedBoxes(guarded, sizeof guarded) == 1);
        assert(zones[1] == 0 && zones[2] == 121 && zones[3] == 49 && zones[4] == 167);
        assert(armedBoxes(steering, sizeof steering) == 1 && zones[1] == 0xFF && zones[2] == 0xFF);
        assert(!armedBoxes(either, sizeof either));
        assert(armedBoxes(stale, sizeof stale) == 2);
        assert(zones[5] == 85 && zones[6] == 105 && zones[7] == 95 && zones[8] == 115);
    }
    /* distance(ego, o3, v50): v50 < 10 means within 9 of object 3. */
    {
        /* if (v50 < 10) { new.room(5) } */
        static const byte nearby[] = { 0x45, 0, 3, 50, 0xFF, 0x03, 50, 10, 0xFF,
            0x02, 0x00, 0x12, 5, 0x00 };
        /* if (v50 > 10) { new.room(5) }: far away is not a box */
        static const byte away[] = { 0x45, 3, 0, 50, 0xFF, 0x05, 50, 10, 0xFF,
            0x02, 0x00, 0x12, 5, 0x00 };
        armedBoxes(nearby, sizeof nearby); /* object 3 not drawn: no box */
        assert(!zones[0]);
        viewtab[3].flags = DRAWN; viewtab[3].xPos = 100; viewtab[3].yPos = 120;
        var[0] = 2; memset(zones, 0xEE, sizeof zones);
        logdir[2].filePos = logic(nearby, sizeof nearby);
        b1TeleportTriggers(zones);
        assert(zones[0] == 1 && zones[1] == 91 && zones[2] == 111 && zones[3] == 109 && zones[4] == 129);
        teleportPending = 2; b1TeleportTriggers(zones); /* parking: objects not placed */
        assert(!zones[0]); teleportPending = 0;
        logdir[2].filePos = logic(away, sizeof away);
        b1TeleportTriggers(zones); assert(!zones[0]);
    }
    puts("PASS: quiet landings: trigger lines, water and armed script boxes (also in called logics) are avoided");

    /* What the walk cannot see is reported: an unknown test, more boxes or
       calls than kept, ifs nested too deep. Calls already queued are free. */
    {
        static const byte unknown[] = { 0xFF, 0x13, BLOCK, 0x00 };
        static const byte fiveCalls[] = { 0x16, 11, 0x16, 12, 0x16, 13, 0x16, 14, 0x16, 15, 0x00 };
        static const byte fourCalls[] = { 0x16, 11, 0x16, 12, 0x16, 13, 0x16, 14, 0x16, 11, 0x00 };
        static byte many[2 + 6 * 64 + 6], deep[17 * 8 + 1];
        unsigned i, n = 0;
        assert(!armedBoxes(armed, sizeof armed) == 0 && !partial);
        armedBoxes(unknown, sizeof unknown); assert(partial);
        armedBoxes(fourCalls, sizeof fourCalls); assert(!partial);
        armedBoxes(fiveCalls, sizeof fiveCalls); assert(partial);
        many[n++] = 0xFF;
        for (i = 0; i < 64; ++i) { many[n++] = 0x0B; many[n++] = 0; many[n++] = 85; many[n++] = 105; many[n++] = 95; many[n++] = 115; }
        many[n++] = 0xFF; many[n++] = 0x02; many[n++] = 0x00; many[n++] = 0x01; many[n++] = 0x05; many[n++] = 0x00;
        assert(armedBoxes(many, n) == 63 && partial);
        for (i = 0, n = 0; i < 17; ++i) { /* if (isset(f1)) { ... 17 deep */
            deep[n++] = 0xFF; deep[n++] = 0x07; deep[n++] = 1; deep[n++] = 0xFF;
            deep[n++] = (byte)(8 * (16 - i) + 2); deep[n++] = 0x00; deep[n++] = 0x01; deep[n++] = 0x05;
        }
        deep[n++] = 0x00;
        flag[1] = TRUE; armedBoxes(deep, n); assert(partial);
        lands(unknown, sizeof unknown, 90, 110, "an unreadable script still lands, with a warning");
        assert(strstr(lastMessage, "too complex"));
    }
    puts("PASS: unseen triggers: unknown tests, box, call and nesting limits are reported");

    /* Parking before entry: the new room's script runs a cycle before
       placement, so ego waits outside its armed boxes, read as if in that
       room (v0 == 2 holds), with the old room's water and trigger flags gone. */
    reset(); logdir[2].filePos = logic(room, sizeof room);
    flag[0] = flag[3] = TRUE;
    assert(command("tp 2 30 140")); b6BeginTeleport();
    assert(var[0] == 2 && teleportPending == 2 && !flag[0] && !flag[3]);
    at(30, 140); /* The target itself is free. */
    b6FinishTeleport(); placed(30, 140);
    reset(); logdir[2].filePos = logic(room, sizeof room);
    assert(command("tp 2 90 110")); b6BeginTeleport();
    at(4, 160); /* The target is in the box: first free grid point. */
    b6FinishTeleport(); placed(90, 104);
    reset(); logdir[2].filePos = logic(everywhere, sizeof everywhere);
    assert(command("tp 2 90 110")); b6BeginTeleport();
    at(80, 100); /* Nowhere free: ego stays put. */
    /* While parked, the new room's own draw (CanBeHere) must not report the
       unseen picture's water or trigger lines under ego; afterwards it does. */
    reset(); logdir[2].filePos = logic(room, sizeof room);
    assert(command("tp 2 30 140")); b6BeginTeleport();
    memset(terrain[140], 3, 160);
    b9CanBeHere(&viewtab[0], 0); assert(!flag[0]);
    memset(terrain[140], 2, 160);
    b9CanBeHere(&viewtab[0], 0); assert(!flag[3]);
    memset(terrain[140], 4, 160);
    b6FinishTeleport(); placed(30, 140);
    memset(terrain[140], 3, 160);
    b9CanBeHere(&viewtab[0], 0); assert(flag[0]);
    puts("PASS: parking: before entry ego waits outside the new room's armed boxes, unaffected by its water");
}

/* What a new-room landing costs - cycles, single-pixel and row reads - for
   an open room, a far-off safe island, a room with no quiet spot (all
   water) and one with no walkable spot. The landings are fixed; the cycle
   ceilings catch a return to probing every point (100 to 240 million)
   or to a second pass for walkable spots (3 million). */
static unsigned long cyclesNow(void)
{
    peripherals.counter.select = COUNTER_SELECT_CLOCKCYCLE_COUNTER;
    peripherals.counter.latch = 0;
    return peripherals.counter.value32[0];
}
static void landingCost(const char* name, byte x, byte y, unsigned long ceiling)
{
    unsigned long start, cycles;
    pixelReads = rowReads = 0;
    assert(command("tp 2"));
    teleportX = x; teleportY = y;
    b6BeginTeleport();
    start = cyclesNow();
    b6FinishTeleport();
    cycles = cyclesNow() - start;
    printf("COST %-8s %8lu cycles %5u reads %3u rows -> (%u,%u)\n", name, cycles,
           pixelReads, rowReads, viewtab[0].xPos, viewtab[0].yPos);
    assert(var[0] == 2 && cycles < ceiling);
}
static void test_landing_cost(void)
{
    byte y;
    reset(); landingCost("open", 20, 60, 50000); placed(20, 60);
    reset(); memset(terrain, 0, sizeof terrain);
    for (y = 150; y < 168; ++y) memset(terrain[y] + 130, 4, 30);
    landingCost("island", 20, 60, 2000000); placed(130, 150);
    reset(); memset(terrain, 3, sizeof terrain);
    landingCost("noquiet", 20, 60, 2000000); placed(20, 60);
    assert(strstr(lastMessage, "No quiet spot"));
    reset(); memset(terrain, 0, sizeof terrain);
    landingCost("blocked", 20, 60, 2000000);
    at(20, 60); assert(strstr(lastMessage, "rejected"));
    puts("PASS: landing cost: row-by-row search, same landings, within cycle ceilings");
}

int main(int argc, char** argv)
{
    char path[128];
    assert(argc == 2);
    testGame = argv[1];
    sprintf(path, "%s/VOL.0", argv[1]);
    volume = fopen(path, "wb");
    assert(volume);
    {
        static const byte quiet[] = { 0x00 };
        quietLogic = logic(quiet, sizeof quiet);
    }
    test_layout();
    test_placement();
    test_room_entry();
    test_automatic_landing();
    test_grammar();
    test_tour();
    test_quiet_landing();
    test_landing_cost();
    fclose(volume);
    return 0;
}
