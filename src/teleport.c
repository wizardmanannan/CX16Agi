#include "teleport.h"

#if ENABLE_TELEPORT
#include "agifiles.h"
#include "parser.h"
#include "view.h"
#include "textLayer.h"

extern int dirnOfEgo, controlMode;
extern byte horizon;
/* These routines execute in bank 9, with b9TryTeleport. */
extern boolean b9Collide(ViewTable* ego, byte entryNum);
extern boolean b9CanBeHere(ViewTable* ego, byte entryNum);

/* Unbanked: shared by input (bank 7) and the cycle (bank 6). */
byte teleportPending;
static byte teleportX, teleportY;

#pragma rodata-name (push, "BANKRAM07")
/* ASCII bytes: cc65's target execution charset is PETSCII. */
static const char teleportWord[] = {116,101,108,101,112,111,114,116,0};
#ifdef __CC65__
#include <ascii_charmap.h>
#endif
static const char waitingMessage[] = "Walk through a normal exit. Teleport waits for the next room entry.";
static const char blockedMessage[] = "Teleport rejected: blocked path or scripted movement.";
static const char wrongRoomMessage[] = "Teleport canceled: entered a different room.";
static const char canceledMessage[] = "Teleport canceled.";
#ifdef __CC65__
#include <cbm_petscii_charmap.h>
#endif
#pragma rodata-name (pop)
#pragma code-name (push, "BANKRAM07")
void b7TeleportNotice(byte reason)
{
    const char* message;
    byte keys[2];

    switch (reason) {
    case TELEPORT_WAITING: message = waitingMessage; break;
    case TELEPORT_BLOCKED: message = blockedMessage; break;
    case TELEPORT_WRONG_ROOM: message = wrongRoomMessage; break;
    default: message = canceledMessage; break;
    }
    keys[0] = KEY_ENTER;
    keys[1] = KEY_ESC;
    b3DisplayMessageBox((char*)message, STRING_BANK, AUTO_CALC_ROW, AUTO_CALC_COLUMN,
                       TEXTBOX_PALETTE_NUMBER, DEFAULT_BOX_WIDTH, TRUE);
    b5WaitOnSpecificKeys(keys, 2);
    b3ClearLastPlacedText();
}

boolean b7TeleportCommand(const char* input)
{
    byte args[3], i;
    unsigned int value;
    AGIFilePosType location;

    if (strncmp(input, teleportWord, 8) || (input[8] && input[8] != 32))
        return FALSE;
    input += 8;
    /* Consume malformed debug commands too; never send them to said(). */
    for (i = 0; i < 3; ++i) {
        if (*input != 32) return TRUE;
        while (*input == 32) ++input;
        if (*input < 48 || *input > 57) return TRUE;
        value = 0;
        do {
            value = value * 10 + (*input++ - 48);
            if (value > 255) return TRUE;
        } while (*input >= 48 && *input <= 57);
        args[i] = (byte)value;
    }
    while (*input == 32) ++input;
    if (!*input && !args[0] && !args[1] && !args[2]) {
        teleportPending = 0;
        b7TeleportNotice(TELEPORT_CANCELED);
        return TRUE;
    }
    if (*input || !args[0] || args[0] >= numLogics ||
        args[1] >= 160 || args[2] >= 168) return TRUE;
    b10GetLogicDirectory(&location, &logdir[args[0]]);
    if (location.filePos == EMPTY) return TRUE;

    teleportX = args[1];
    teleportY = args[2];
    /* Room zero is invalid, so the room itself doubles as the pending flag. */
    teleportPending = args[0];
    if (teleportPending != var[0]) b7TeleportNotice(TELEPORT_WAITING);
    return TRUE;
}
#pragma code-name (pop)

#pragma code-name (push, "BANKRAM06")
void b6BeginTeleport(void)
{
    /* Cross-room requests wait for an actual script-driven room transition.
       Never fabricate the previous room, crossing direction, or new.room. */
    if (teleportPending == var[0]) b6FinishTeleport();
}

void b6FinishTeleport(void)
{
    /* Called for the next completed normal room entry, or immediately for a
       same-room request. A redirect consumes the request without placement. */
    if (var[0] != teleportPending) {
        teleportPending = 0;
        b7TeleportNotice(TELEPORT_WRONG_ROOM);
        return;
    }
    teleportPending = 0;
    if (!b9TryTeleport()) b7TeleportNotice(TELEPORT_BLOCKED);
}
#pragma code-name (pop)

#pragma code-name (push, "BANKRAM09")
/* Every probe uses a local copy. entryNum=1 suppresses CanBeHere's ego
   water/special flag writes; collision exclusion still uses ego's index 0. */
static boolean b9TeleportPositionValid(ViewTable* ego)
{
    if (ego->xPos > 160 - ego->xsize || ego->yPos >= 168 ||
        ego->yPos + 1 < ego->ysize ||
        (!(ego->flags & IGNOREHORIZON) && ego->yPos <= horizon)) return FALSE;
    return !b9Collide(ego, 0) && b9CanBeHere(ego, 1);
}

boolean b9TryTeleport(void)
{
    ViewTable ego;

    getViewTab(&ego, 0);
    if (controlMode != 0 || ego.motion != NORMAL_MOTION ||
        (ego.flags & (ANIMATED | DRAWN | UPDATE | MOTION)) !=
                     (ANIMATED | DRAWN | UPDATE | MOTION) ||
        !ego.xsize || ego.xsize > 160 || !ego.ysize || ego.ysize > 168)
        return FALSE;

    /* A monotone eight-direction path: at most 168 position checks including
       the origin. No spiral search, map allocation, or collision bypass. */
    for (;;) {
        if (!b9TeleportPositionValid(&ego)) return FALSE;
        if (ego.xPos == teleportX && ego.yPos == teleportY) break;
        ego.previousX = ego.xPos;
        ego.previousY = ego.yPos;
        if (ego.xPos < teleportX) ++ego.xPos;
        else if (ego.xPos > teleportX) --ego.xPos;
        if (ego.yPos < teleportY) ++ego.yPos;
        else if (ego.yPos > teleportY) --ego.yPos;
    }

    /* Publish environment flags only at the accepted destination. */
    flag[0] = flag[3] = FALSE;
    b9CanBeHere(&ego, 0);
    ego.previousX = ego.xPos;
    ego.previousY = ego.yPos;
    ego.direction = 0;
    ego.staleCounter = 1;
    setViewTab(&ego, 0);
    var[2] = var[6] = 0;
    dirnOfEgo = 0;
    memsetBanked(b7Directions, 0, 9, STRING_BANK);
    return TRUE;
}
#pragma code-name (pop)
#endif
