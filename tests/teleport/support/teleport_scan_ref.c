/* Reference model of src/teleport/search/teleportScan.s, written from its contract rather
   than its code, for the differential test in test_teleport_rooms.c. Query
   words are the grammar's offsets into the command line; the last word of
   each stop is also a prefix; words of 5+ letters allow one typo, 9+ two
   (Damerau: changed, missing, extra, swapped neighbours), a wrong first
   letter counts two. */
#include "teleport_sim.h"
#include <string.h>

byte refHits[12], refTypos[12];
static const byte* refLine;
static const byte key[] = "Avis Durgan";
static byte keyPos, textLength;
static char text[16];
static byte last[12]; /* last word of its stop */

/* Edits (at most left) to turn q[0..m) into t[0..n); a prefix may stop early. */
static int edit(const char* q, int m, const char* t, int n, int left, int prefix)
{
    while (m && n && *q == *t) { ++q; ++t; --m; --n; }
    if (!m) return prefix || n <= left;
    if (!n) return m <= left;
    if (!left--) return 0;
    if (edit(q + 1, m - 1, t + 1, n - 1, left, prefix)) return 1;
    if (edit(q + 1, m - 1, t, n, left, prefix)) return 1;
    if (edit(q, m, t + 1, n - 1, left, prefix)) return 1;
    return m > 1 && n > 1 && q[1] == t[0] && q[0] == t[1] && edit(q + 2, m - 2, t + 2, n - 2, left, prefix);
}

/* Typos (1 or 2) for query q against the current word, or 0 if none fit. */
static byte typos(byte q)
{
    byte length = bFTeleportWordLength[q], budget = length >= 9 ? 2 : length >= 5, k;
    const char* query = (const char*)refLine + bFTeleportWordStart[q];
    int diff = (int)length - (int)textLength;
    if (!budget || diff > budget || (!last[q] && -diff > budget)) return 0;
    for (k = query[0] == text[0] ? 1 : 2; k <= budget; ++k)
        if (edit(query, length, text, textLength, k, last[q])) return k;
    return 0;
}

static void endWord(void)
{
    byte q, length, t;
    const byte* query;
    boolean match;

    if (textLength && textLength < 16) {
        for (q = 0; q < bFTeleportWords; ++q) {
            length = bFTeleportWordLength[q];
            query = refLine + bFTeleportWordStart[q];
            if (last[q] && textLength >= length) match = !memcmp(text, query, length);
            else if (textLength == length) match = !memcmp(text, query, length);
            else if (textLength == length + 1)
                match = text[length] == 's' && !memcmp(text, query, length);
            else if (length == textLength + 1)
                match = query[textLength] == 's' && !memcmp(text, query, textLength);
            else match = FALSE;
            t = match ? 0 : typos(q);
            if (match || t) {
                if (t < refTypos[q]) refTypos[q] = t;
                if (refHits[q] < 255) ++refHits[q];
            }
        }
    }
    textLength = 0;
}

void refScanReset(const char* line)
{
    byte s;
    refLine = (const byte*)line;
    keyPos = textLength = 0;
    memset(refHits, 0, sizeof refHits);
    memset(refTypos, 0xFF, sizeof refTypos);
    memset(last, 0, sizeof last);
    for (s = 0; s < bFTeleportStopCount; ++s)
        if (bFTeleportStopFirst[s + 1] > bFTeleportStopFirst[s]) last[bFTeleportStopFirst[s + 1] - 1] = 1;
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
