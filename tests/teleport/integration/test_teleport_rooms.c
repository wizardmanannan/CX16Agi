/* Room-name search in the production assembly on a simulated 65C02, reading
   real game files: known KQ1/KQ3 names, the tour plan text, and the 65C02
   scanner against an independent C model over every room's message text. */
#include "../support/teleport_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sim65.h>

void __fastcall__ testScanFloor(const byte* data, byte length);
static unsigned long cycles(void)
{
    peripherals.counter.select = COUNTER_SELECT_CLOCKCYCLE_COUNTER;
    peripherals.counter.latch = 0;
    return peripherals.counter.value32[0];
}

void testSetLine(const char* line);
void testScanReset(void);
void __fastcall__ testScan(const byte* data, byte length);
void testScanEnd(void);
void refScanReset(const char* line);
void refScan(const byte* data, unsigned length);
void refScanEnd(void);
extern byte refHits[12], refTypos[12], refPairs[12];
void refTriggersBegin(byte* boxes, byte room);
void refTriggerLogic(FILE* file, unsigned length);
byte refTriggersEnd(void);
extern byte refCalls[4], refCallCount, refTriggersPartial;
byte __fastcall__ b1TeleportTriggers(byte* zones);
byte __fastcall__ testPriority(unsigned int xy) { (void)xy; assert(!"no placement here"); return 0; }
byte* testRowBuffer;
void __fastcall__ testReadRow(byte y) { (void)y; assert(!"no placement here"); }

static void search(const char* text)
{
    unsigned before = clears;
    assert(b7TeleportCommand(text));
    assert(!testFileOpen());
    assert(clears > before); /* "searching rooms..." is removed again */
}

static void game(const char* path)
{
    testLoadGame(path);
    var[0] = 1;
    teleportPending = teleportTour = 0;
}

static void test_kq1(const char* path)
{
    game(path);
    search("tp castle"); assert(teleportPending == 2 && teleportX == TELEPORT_AUTO);
    search("tp cast"); assert(teleportPending == 2); /* the last word is a prefix */
    search("tp castel"); assert(teleportPending == 2); /* swapped letters: one typo */
    search("tp dragn"); assert(teleportPending == 51); /* a missing letter */
    search("tp bean stalk"); assert(teleportPending == 6); /* joined: "beanstalk" */
    search("tp moat 80 120"); assert(teleportPending == 1 && teleportX == 80 && teleportY == 120);
    search("tp king edward"); assert(teleportPending == 53);
    /* Two throne rooms: the leprechaun king's mentions "throne" more. */
    search("tp throne room"); assert(teleportPending == 77);
    search("tp:3:kings room,moat,castle");
    assert(teleportTour && teleportPending == 53);
    assert(!strcmp(lastMessage, "Teleport tour, 3 seconds per stop: kings room=53, moat=1, castle=2"));
    search("tp:dragon,9,goat"); assert(teleportTour && teleportPending == 51);
    assert(!strcmp(lastMessage, "Teleport tour, 5 seconds per stop: dragon=51, 9, goat=11"));
    search("tp xyzzyplugh");
    assert(!teleportTour && !strcmp(lastMessage, "Teleport: no room matches xyzzyplugh"));
    search("tp:castle,  kings   room ,xyzzy");
    assert(!strcmp(lastMessage, "Teleport: no room matches xyzzy"));
    printf("PASS: %s: KQ1 names, tour plan, unknown names (%u opens, %u seeks)\n", path, fileOpens, fileSeeks);
}

static void test_kq3(const char* path)
{
    game(path);
    search("tp kitchen"); assert(teleportPending == 6);
    search("tp kitchn"); assert(teleportPending == 6); /* a missing letter */
    search("tp:1:wizard,spider"); assert(teleportPending == 4);
    assert(!strcmp(lastMessage, "Teleport tour, 1 seconds per stop: wizard=4, spider=14"));
    printf("PASS: %s: KQ3 names\n", path);
}

/* Each room's message text, streamed in pseudo-random chunk sizes through
   both scanners; every word count must agree. */
static char line[] = "tp kings room moat castel bean stalk a the dargon goat sir grahm"; /* typos, a 2-gram */
static unsigned seed = 1;

static void differential(const char* path)
{
    static byte order[256], chunk[128], header[7];
    unsigned count = 0, i, j, rooms = 0, words = 0, typed = 0, close = 0;
    unsigned long bytes = 0;
    unsigned long position = 0, start, left, scanCycles = 0, floorCycles = 0;
    byte n, current = 255, size, q;
    FILE* file = NULL;
    char name[128];

    game(path);
    bFTeleportWords = 0; /* the query words of the line above */
    for (i = 3; line[i]; ) {
        bFTeleportWordStart[bFTeleportWords] = (byte)i;
        while (line[i] && line[i] != ' ') ++i;
        bFTeleportWordLength[bFTeleportWords] = (byte)(i - bFTeleportWordStart[bFTeleportWords]);
        ++bFTeleportWords;
        while (line[i] == ' ') ++i;
    }
    assert(bFTeleportWords == 12);
    bFTeleportStopCount = 4; /* stops of three words: each third word is also a prefix */
    for (i = 0; i <= 4; ++i) bFTeleportStopFirst[i] = (byte)(3 * i);
    testSetLine(line);
    for (i = 1; i < (unsigned)numLogics; ++i)
        if (logdir[i].filePos != EMPTY && picdir[i].filePos != EMPTY) order[count++] = (byte)i;
    for (i = 1; i < count; ++i) /* by volume and offset, to read forward only */
        for (j = i; j && (logdir[order[j - 1]].fileNum > logdir[order[j]].fileNum ||
             (logdir[order[j - 1]].fileNum == logdir[order[j]].fileNum &&
              logdir[order[j - 1]].filePos > logdir[order[j]].filePos)); --j) {
            n = order[j]; order[j] = order[j - 1]; order[j - 1] = n;
        }
    for (i = 0; i < count; ++i) {
        AGIFilePosType* entry = &logdir[order[i]];
        if (entry->fileNum != current) {
            if (file) fclose(file);
            sprintf(name, "%s/VOL.%u", path, entry->fileNum);
            file = fopen(name, "rb"); assert(file);
            current = entry->fileNum; position = 0;
        }
        for (start = entry->filePos; position < start; position += n) {
            n = start - position > sizeof chunk ? sizeof chunk : (byte)(start - position);
            assert(fread(chunk, 1, n, file) == n);
        }
        assert(fread(header, 1, 7, file) == 7); position += 7;
        assert(header[0] == 0x12 && header[1] == 0x34);
        for (start = position + (header[5] | header[6] << 8); position < start; position += n) {
            n = start - position > sizeof chunk ? sizeof chunk : (byte)(start - position);
            assert(fread(chunk, 1, n, file) == n);
        }
        assert(fread(chunk, 1, 3, file) == 3); position += 3;
        left = (header[3] | header[4] << 8) - (header[5] | header[6] << 8) - 5u - chunk[0] * 2u;
        for (start = position + chunk[0] * 2u; position < start; position += n) {
            n = start - position > sizeof chunk ? sizeof chunk : (byte)(start - position);
            assert(fread(chunk, 1, n, file) == n);
        }
        testScanReset(); refScanReset(line);
        while (left) {
            seed = seed * 25173 + 13849;
            size = (byte)(1 + (seed >> 8) % 128);
            if (size > left) size = (byte)left;
            assert(fread(chunk, 1, size, file) == size);
            position += size; left -= size; bytes += size;
            start = cycles(); testScan(chunk, size); scanCycles += cycles() - start;
            start = cycles(); testScanFloor(chunk, size); floorCycles += cycles() - start;
            refScan(chunk, size);
        }
        testScanEnd(); refScanEnd();
        for (q = 0; q < 12; ++q) {
            if (bFTeleportHits[q] != refHits[q])
                printf("room %u word %u: 65C02 %u, model %u\n", order[i], q, bFTeleportHits[q], refHits[q]);
            assert(bFTeleportHits[q] == refHits[q]);
            if (bFTeleportTypos[q] != refTypos[q])
                printf("room %u word %u: 65C02 %u typos, model %u\n", order[i], q, bFTeleportTypos[q], refTypos[q]);
            assert(bFTeleportTypos[q] == refTypos[q]);
            if (bFTeleportPairs[q] != refPairs[q])
                printf("room %u word %u: 65C02 pair %u, model %u\n", order[i], q, bFTeleportPairs[q], refPairs[q]);
            assert(bFTeleportPairs[q] == refPairs[q]);
            close += refPairs[q] < 3;
            typed += refTypos[q] == 1 || refTypos[q] == 2;
            words += refHits[q];
        }
        ++rooms;
    }
    if (file) fclose(file);
    printf("PASS: %s: 65C02 scanner equals the C model in %u rooms (%lu bytes, %u matches, %u words matched with typos, %u close pairs; %lu cycles/byte, decrypt-and-classify floor %lu)\n",
           path, rooms, bytes, words, typed, close, scanCycles / bytes, floorCycles / bytes);
}

/* Rooms with pictures, by volume and offset, so files are read forward only. */
static byte order[256];
static unsigned rooms(void)
{
    unsigned count = 0, i, j;
    byte n;
    for (i = 1; i < (unsigned)numLogics; ++i)
        if (logdir[i].filePos != EMPTY && picdir[i].filePos != EMPTY) order[count++] = (byte)i;
    for (i = 1; i < count; ++i)
        for (j = i; j && (logdir[order[j - 1]].fileNum > logdir[order[j]].fileNum ||
             (logdir[order[j - 1]].fileNum == logdir[order[j]].fileNum &&
              logdir[order[j - 1]].filePos > logdir[order[j]].filePos)); --j) {
            n = order[j]; order[j] = order[j - 1]; order[j - 1] = n;
        }
    return count;
}

/* Opens logic n at its code; *length = code bytes. Reads forward only: sim65
   has no seek, and a read into sim65 memory is cheap. */
static FILE* openLogic(const char* path, byte n, unsigned* length)
{
    static byte chunk[256], header[7];
    char name[128];
    unsigned long left = logdir[n].filePos;
    size_t step;
    FILE* file;
    sprintf(name, "%s/VOL.%u", path, logdir[n].fileNum);
    file = fopen(name, "rb"); assert(file);
    while (left) {
        step = left > sizeof chunk ? sizeof chunk : (size_t)left;
        assert(fread(chunk, 1, step, file) == step);
        left -= step;
    }
    assert(fread(header, 1, 7, file) == 7 && header[0] == 0x12 && header[1] == 0x34);
    *length = header[5] | header[6] << 8;
    return file;
}

/* The 65C02 trigger reader against the C model, for every room of a game,
   in a fresh-arrival state (var 0 = room, flag 5 set). */
static void triggers(const char* path)
{
    static byte machine[1 + 4 * 63], model[1 + 4 * 63];
    unsigned room, i, rooms = 0, boxes = 0, dropped = 0, calls = 0, partials = 0, length;
    byte partial;
    FILE* file;
    game(path);
    for (room = 1; room < (unsigned)numLogics; ++room) {
        if (logdir[room].filePos == EMPTY || picdir[room].filePos == EMPTY) continue;
        memset(testVars, 0, sizeof testVars);
        memset(testFlags, 0, sizeof testFlags);
        var[0] = (byte)room; flag[5] = TRUE;
        for (i = 1; i < 16; ++i) { /* drawn objects, so distance() boxes are compared too */
            viewtab[i].flags = DRAWN; viewtab[i].xPos = 9 * i; viewtab[i].yPos = 50 + 7 * i;
        }
        partial = b1TeleportTriggers(machine);
        assert(!testFileOpen());
        refTriggersBegin(model, (byte)room);
        file = openLogic(path, (byte)room, &length);
        refTriggerLogic(file, length);
        fclose(file);
        for (i = 0; i < refCallCount; ++i) {
            if (logdir[refCalls[i]].filePos == EMPTY) continue;
            file = openLogic(path, refCalls[i], &length);
            refTriggerLogic(file, length);
            fclose(file);
        }
        refTriggersEnd();
        if (memcmp(machine, model, 1 + 4 * model[0]))
            printf("room %u: 65C02 %u boxes, model %u\n", room, machine[0], model[0]);
        assert(!memcmp(machine, model, 1 + 4 * model[0]));
        assert(!partial == !refTriggersPartial);
        if (partial) { printf("room %u: not every trigger seen\n", room); ++partials; }
        for (i = 0; i < model[0]; ++i) {
            if (model[2 + 4 * i] == 0xFF) ++dropped; /* a position box that guarded nothing */
            else ++boxes;
        }
        calls += refCallCount;
        ++rooms;
    }
    printf("PASS: %s: trigger boxes equal the C model in %u rooms (%u armed, %u position boxes dropped, %u calls followed, %u partial)\n",
           path, rooms, boxes, dropped, calls, partials);
}

/* One game per run (sim65 caps cycles): game directory, then kq1, kq3 or any. */
int main(int argc, char** argv)
{
    assert(argc == 3);
    if (!strcmp(argv[2], "kq1")) {
        test_kq1(argv[1]);
        testNoMacptr = 1; /* byte-by-byte KERNAL reads give the same answers */
        test_kq1(argv[1]);
        testNoMacptr = 0;
    } else if (!strcmp(argv[2], "kq3")) test_kq3(argv[1]);
    differential(argv[1]);
    triggers(argv[1]);
    return 0;
}
