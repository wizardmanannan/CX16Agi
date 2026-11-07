/* The engine around the teleport modules, for sim65: view table, priority
   screen, var/flag arrays, room entry, message boxes and the AGI file
   services (real game files through sim65's host I/O). */
#include "teleport_sim.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

byte testVars[256];
boolean testFlags[256];
unsigned int testJiffies;
byte* var = testVars;
boolean* flag = testFlags;
ViewTable viewtab[VIEW_TABLE_SIZE];
byte horizon = 36;
int numLogics, controlMode, dirnOfEgo, newRoomNum;
byte b7Directions[9], b9PreComputedPriority[256];
AGIFilePosType logdir[256], picdir[256];
byte avisDurgan[11] = { 0x41, 0x76, 0x69, 0x73, 0x20, 0x44, 0x75, 0x72, 0x67, 0x61, 0x6E };
unsigned pixelReads, notices, waits, clears, transitions, fileOpens, fileSeeks;
char lastMessage[128];
const char* testGame;
byte offsetOfFlags = offsetof(ViewTable, flags);
byte offsetOfXPos = offsetof(ViewTable, xPos);
byte offsetOfYPos = offsetof(ViewTable, yPos);
byte offsetOfXSize = offsetof(ViewTable, xsize);
byte offsetOfYSize = offsetof(ViewTable, ysize);
byte offsetOfPrevY = offsetof(ViewTable, previousY);
byte offsetOfPriority = offsetof(ViewTable, priority);
byte sizeOfViewTab = sizeof(ViewTable);

void b6NewRoom(void)
{
    ++transitions;
    var[1] = var[0];
    var[0] = (byte)newRoomNum;
    flag[5] = TRUE;
}

void b3DisplayMessageBox(char* message, byte bank, byte row, byte col,
                         byte palette, byte width, boolean wrap, byte firstRow)
{
    assert(bank == 7 || bank == 0xF);
    assert(row == 255 && col == 255 && palette == 1 && width == 32 && wrap);
    assert(firstRow == FIRST_TEXT_ROW);
    assert(strlen(message) < sizeof lastMessage);
    strcpy(lastMessage, message);
    ++notices;
}

void b5WaitOnSpecificKeys(byte* keys, byte length)
{
    assert(length == 2 && keys[0] == 13 && keys[1] == 27);
    ++waits;
}

void b3ClearLastPlacedText(void) { ++clears; }

void* memCpyBanked(byte* dest, byte* src, byte bank, size_t length)
{
    assert(bank == 0x10 && length == 5);
    assert((src >= (byte*)logdir && src < (byte*)(logdir + 256)) ||
           (src >= (byte*)picdir && src < (byte*)(picdir + 256)));
    return memcpy(dest, src, length);
}

/* One VOL file at a time, as on the CX16. sim65 has no lseek, so a seek
   backwards reopens the file; forward seeks read ahead. */
static FILE* volume;
static char volumePath[128];
static unsigned long volumePosition;

byte b6Cbm_openForSeeking(const char* fileName)
{
    /* PETSCII lowercase "vol." has ASCII uppercase codes: VOL.N. */
    assert(!volume && testGame && !memcmp(fileName, "VOL.", 4));
    sprintf(volumePath, "%s/%s", testGame, fileName);
    volume = fopen(volumePath, "rb");
    assert(volume);
    volumePosition = 0;
    ++fileOpens;
    return 2;
}

int8_t b6Cx16_fseek(uint8_t channel, uint32_t offset)
{
    static byte skip[256];
    unsigned long gap;
    assert(channel == 2 && volume);
    ++fileSeeks;
    if (offset < volumePosition) {
        fclose(volume);
        volume = fopen(volumePath, "rb");
        assert(volume);
        volumePosition = 0;
    }
    while (volumePosition < offset) {
        gap = offset - volumePosition;
        if (gap > sizeof skip) gap = sizeof skip;
        assert(fread(skip, 1, (size_t)gap, volume) == gap);
        volumePosition += gap;
    }
    return 0;
}

/* KERNAL MACPTR, BASIN, READST and CLOSE on the open VOL file
   (teleport_cpu.s adapts the register interface). */
byte testNoMacptr;

byte __fastcall__ testRead(byte* buffer, byte size)
{
    size_t n;
    assert(volume && size);
    n = fread(buffer, 1, size, volume);
    volumePosition += n;
    return (byte)n;
}

byte testReadByte(void)
{
    int c;
    assert(volume);
    c = fgetc(volume);
    if (c == EOF) return 0;
    ++volumePosition;
    return (byte)c;
}

byte testReadStatus(void) { return feof(volume) ? 0x40 : 0; }

void __fastcall__ testClose(byte lfn)
{
    assert(lfn == 2 && volume);
    fclose(volume);
    volume = NULL;
}

boolean testFileOpen(void) { return volume != NULL; }

static unsigned loadDirectory(const char* name, AGIFilePosType* directory)
{
    char path[128];
    byte entry[3];
    unsigned count = 0;
    FILE* file;
    memset(directory, 0, sizeof(AGIFilePosType) * 256);
    sprintf(path, "%s/%s", testGame, name);
    file = fopen(path, "rb");
    assert(file);
    while (count < 256 && fread(entry, 1, 3, file) == 3) {
        directory[count].fileNum = entry[0] >> 4;
        directory[count].filePos = ((unsigned long)(entry[0] & 15) << 16) |
                                   ((unsigned long)entry[1] << 8) | entry[2];
        ++count;
    }
    for (; count < 256; ++count) directory[count].filePos = EMPTY;
    fclose(file);
    return count;
}

void testLoadGame(const char* game)
{
    char path[128];
    FILE* file;
    unsigned count = 0;
    byte entry[3];
    testGame = game;
    loadDirectory("PICDIR", picdir);
    loadDirectory("LOGDIR", logdir);
    sprintf(path, "%s/LOGDIR", game);
    file = fopen(path, "rb");
    while (fread(entry, 1, 3, file) == 3) ++count;
    fclose(file);
    numLogics = (int)count;
}
