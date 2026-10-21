/* Reference model of src/teleport/search/teleportTriggers.s, written from its contract, for
   the differential test in test_teleport_rooms.c: walks AGI logic code and
   lists the boxes on ego whose condition, and enclosing blocks, might hold
   for the current var[] and flag[], and the logics it calls (up to four,
   each once, not logic 0 or the room), and whether anything was left unseen. Reads code from FILEs the caller
   opens: refTriggerLogic for the room, then for each of refCalls. */
#include "teleport_sim.h"
#include <stdio.h>

#define MAYBE_TRUE 1
#define MAYBE_FALSE 2

static const byte actionArguments[182] = {
    0,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,0,1,1,1,1,0,1,
    1,3,3,3,3,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,1,2,1,1,1,1,1,1,1,1,1,1,1,3,1,1,
    1,2,1,2,2,1,1,2,2,5,5,3,1,1,2,2,1,1,4,0,1,1,1,2,2,2,1,2,0,1,1,3,3,3,0,0,
    1,2,1,3,0,0,2,5,2,1,2,0,0,3,7,7,0,0,0,0,0,1,3,0,0,1,1,0,0,0,0,0,0,0,1,1,
    1,0,0,3,3,0,3,4,4,1,5,2,1,2,0,1,1,0,1,0,0,2,2,2,2,0,1,0,0,0,1,1,0,1,0,4,
    2,0 };
static const byte testArguments[19] = { 0,2,2,2,2,2,2,1,1,1,2,5,1,0,0,2,5,5,5 };

static FILE* code;
static unsigned left, position;
unsigned refTriggerConsumed;     /* code bytes read by the last refTriggerLogic */
byte refCalls[4], refCallCount, refTriggersPartial;
static byte count, *boxes, room;

static void called(byte logic)
{
    byte i;
    if (!logic || logic == room) return;
    for (i = 0; i < refCallCount; ++i) if (refCalls[i] == logic) return;
    if (refCallCount == 4) { refTriggersPartial = 1; return; }
    refCalls[refCallCount++] = logic;
}

void refTriggersBegin(byte* out, byte currentRoom)
{
    boxes = out; count = 0; refCallCount = 0; room = currentRoom; refTriggersPartial = 0;
}

byte refTriggersEnd(void) { boxes[0] = count; return count; }

static int next(void)
{
    if (!left) return -1;
    --left; ++position;
    return fgetc(code);
}

static byte both(byte a, byte b) { return (byte)(((a & b) & MAYBE_TRUE) | ((a | b) & MAYBE_FALSE)); }
static byte either(byte a, byte b) { return (byte)(((a | b) & MAYBE_TRUE) | ((a & b) & MAYBE_FALSE)); }

/* Adds the boxes of one logic's code to the list. */
void refTriggerLogic(FILE* file, unsigned length)
{
    struct { unsigned end, elseEnd; byte active, enclosing, maybeFalse, hasElse; } frame[16];
    byte depth = 0, start, condition, group, inGroup, negate, test, i, value, active;
    byte args[5];
    int c;
    unsigned size, offset;

    code = file; left = length; position = 0;
    for (;;) {
        while (depth && position >= frame[depth - 1].end) {
            if (frame[depth - 1].hasElse) {
                frame[depth - 1].active = frame[depth - 1].enclosing && frame[depth - 1].maybeFalse;
                frame[depth - 1].end = frame[depth - 1].elseEnd;
                frame[depth - 1].hasElse = 0;
            } else --depth;
        }
        if ((c = next()) < 0) goto end;
        if (c == 0xFE) {
            if ((c = next()) < 0) goto done;
            offset = (unsigned)c;
            if ((c = next()) < 0) goto done;
            offset |= (unsigned)c << 8;
            /* An else, unless it would outlast the enclosing block (an exit). */
            if (offset < 0x8000 && depth && position == frame[depth - 1].end
                && (depth == 1 || position + offset <= frame[depth - 2].end)) {
                frame[depth - 1].elseEnd = position + offset;
                frame[depth - 1].hasElse = 1;
            }
            continue;
        }
        if (c == 0x16 || c == 0x17) {
            byte op = (byte)c;
            if ((c = next()) < 0) goto done;
            called(op == 0x16 ? (byte)c : var[c]);
            continue;
        }
        if (c != 0xFF) {
            if (c >= 182) goto done;
            for (i = 0; i < actionArguments[c]; ++i) if (next() < 0) goto done;
            continue;
        }
        start = count; condition = MAYBE_TRUE; inGroup = negate = 0; group = 0;
        for (;;) {
            if ((c = next()) < 0) goto done;
            if (c == 0xFF) break;
            if (c == 0xFC) {
                if (inGroup) condition = both(condition, group);
                else group = MAYBE_FALSE;
                inGroup = !inGroup;
                continue;
            }
            if (c == 0xFD) { negate = 1; continue; }
            if (c == 0x0E) {
                if ((c = next()) < 0) goto done;
                for (i = 0; i < 2 * c; ++i) if (next() < 0) goto done;
                value = MAYBE_FALSE;
            } else {
                if (c >= 19) goto done;
                test = (byte)c;
                for (i = 0; i < testArguments[test]; ++i) {
                    if ((c = next()) < 0) goto done;
                    args[i] = (byte)c;
                }
                switch (test) {
                case 1: value = var[args[0]] == args[1] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 2: value = var[args[0]] == var[args[1]] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 3: value = var[args[0]] < args[1] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 4: value = var[args[0]] < var[args[1]] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 5: value = var[args[0]] > args[1] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 6: value = var[args[0]] > var[args[1]] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 7: value = flag[args[0]] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 8: value = flag[var[args[0]]] ? MAYBE_TRUE : MAYBE_FALSE; break;
                case 12: case 13: value = MAYBE_FALSE; break;
                case 11: case 16: case 17: case 18:
                    if (args[0] || negate) { value = MAYBE_TRUE | MAYBE_FALSE; break; }
                    if (count < 63) {
                        for (i = 0; i < 4; ++i) boxes[1 + 4 * count + i] = args[1 + i];
                        ++count;
                    } else refTriggersPartial = 1;
                    value = MAYBE_TRUE;
                    break;
                default: value = MAYBE_TRUE | MAYBE_FALSE;
                }
            }
            if (negate) {
                negate = 0;
                if (value != (MAYBE_TRUE | MAYBE_FALSE)) value ^= MAYBE_TRUE | MAYBE_FALSE;
            }
            if (inGroup) group = either(group, value);
            else condition = both(condition, value);
        }
        if ((c = next()) < 0) goto done;
        size = (unsigned)c;
        if ((c = next()) < 0) goto done;
        size |= (unsigned)c << 8;
        active = depth ? frame[depth - 1].active : 1;
        if (!(active && (condition & MAYBE_TRUE))) count = start;
        if (depth == 16) goto done;
        frame[depth].end = position + size;
        frame[depth].enclosing = active;
        frame[depth].active = active && (condition & MAYBE_TRUE);
        frame[depth].maybeFalse = (condition & MAYBE_FALSE) != 0;
        frame[depth].hasElse = 0;
        ++depth;
    }
done:
    refTriggersPartial = 1;
end:
    refTriggerConsumed = position;
}
