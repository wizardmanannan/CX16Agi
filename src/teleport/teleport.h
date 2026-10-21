#ifndef TELEPORT_H
#define TELEPORT_H

#include "../general.h"

/* Teleport is written in 65C02 assembly (the modules under src/teleport;
   see docs/teleport/README.md). This header is its C-visible surface. Build
   with ENABLE_TELEPORT=0 for both C and assembly to omit it entirely. */
#ifndef ENABLE_TELEPORT
#define ENABLE_TELEPORT 1
#endif

#if ENABLE_TELEPORT
#include "../memoryManager.h"
#define TELEPORT_BANK 0xF
#define TELEPORT_AUTO 255

/* teleportState.s: the request being carried out. */
extern byte teleportPending;    /* destination room; 0 = none */
extern byte teleportX, teleportY;
extern byte teleportTour;       /* nonzero while a tour runs */

/* Bank 7, from the parser: is the line a teleport command, and run it. */
boolean b7TeleportInputPrefix(const char* input);
boolean b7TeleportCommand(const char* input);
/* Bank 6, from the interpreter cycle: before the logics while a request or
   tour is active, and after them while a request is. */
void b6TeleportCycle(void);
void b6TeleportAfterLogic(void);
#endif
#endif
