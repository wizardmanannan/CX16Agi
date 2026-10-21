/* Reference model of src/teleport/search/teleportScan.s, written from its contract rather
   than its code, for the differential test in test_teleport_rooms.c. Query
   words are the grammar's offsets into the command line. */
#include "teleport_sim.h"
#include <string.h>

byte refHits[12];
static const byte* refLine;
static const byte key[] = "Avis Durgan";
static byte keyPos, textLength;
static char text[16];

static void endWord(void)
{
    byte q, length;
    const byte* query;
    boolean match;

    if (textLength && textLength < 16) {
        for (q = 0; q < bFTeleportWords; ++q) {
            length = bFTeleportWordLength[q];
            query = refLine + bFTeleportWordStart[q];
            if (textLength == length) match = !memcmp(text, query, length);
            else if (textLength == length + 1)
                match = text[length] == 's' && !memcmp(text, query, length);
            else if (length == textLength + 1)
                match = query[textLength] == 's' && !memcmp(text, query, textLength);
            else match = FALSE;
            if (match && refHits[q] < 255) ++refHits[q];
        }
    }
    textLength = 0;
}

void refScanReset(const char* line)
{
    refLine = (const byte*)line;
    keyPos = textLength = 0;
    memset(refHits, 0, sizeof refHits);
}

void refScan(const byte* data, unsigned length)
{
    byte c;
    while (length--) {
        c = *data++ ^ key[keyPos];
        keyPos = (byte)((keyPos + 1) % 11);
        if (c == '\'') continue;
        if (c >= 'A' && c <= 'Z') c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            if (textLength < 16) text[textLength++] = (char)c;
        } else endWord();
    }
}

void refScanEnd(void) { endWord(); }
