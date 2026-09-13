#ifndef TELEPORT_H
#define TELEPORT_H

#include "general.h"

/* Set to 0 for a build with no teleport code or polling overhead. */
#ifndef ENABLE_TELEPORT
#define ENABLE_TELEPORT 1
#endif

#if ENABLE_TELEPORT
#include "memoryManager.h"
enum {
    TELEPORT_WAITING = 1,
    TELEPORT_BLOCKED,
    TELEPORT_WRONG_ROOM,
    TELEPORT_CANCELED
};
/* Zero means idle; otherwise this is the destination room. */
extern byte teleportPending;
/* Called in bank 7; input is the parser's ASCII buffer. */
boolean b7TeleportCommand(const char* input);
/* Called in bank 6, outside the logic interpreter. */
void b6BeginTeleport(void);
void b6FinishTeleport(void);
#pragma wrapped-call (push, trampoline, STRING_BANK)
void b7TeleportNotice(byte reason);
#pragma wrapped-call (pop)
#pragma wrapped-call (push, trampoline, POSITION_HELPERS_BANK)
boolean b9TryTeleport(void);
#pragma wrapped-call (pop)
#endif
#endif
