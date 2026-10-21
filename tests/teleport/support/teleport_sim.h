#ifndef TELEPORT_SIM_H
#define TELEPORT_SIM_H
/* Production teleport and view types for the sim65 tests, with the CX16
   hardware headers replaced. Include first. */
#define _MEMORYMANAGER_H_
#define _HELPERS_H_
#define _IRQ_H_
#define _VIEW_H_
#define _TEXTLAYER_H_
#define NO_DIRECTORY_ENTRYS 256
/* Bank numbers from memoryManager.h (sim65 memory is flat; the trampoline
   ignores them). */
#define CONTROLLER_BANK 0x1
#define DEBUG_INIT_BANK 5
#define DEPENDENCY_RESOLVER_BANK 0x4
#define DIRECTORY_BANK 0x10
#define EXTRA_SMALL_FIRST_BANK 0x14
#define FILE_LOADER_HELPERS 0x6
#define FLOAT_BANK 0x12
#define GRAPHICS_BANK 6
#define HELPERS_BANK 5
#define IRQ_BANK 6
#define LARGE_FIRST_BANK 0x2D
#define LINE_DRAW_BANK 8
#define LOADED_VIEW_BANK 0x11
#define LOADING_SCREEN_CODE_BANK 6
#define LOGIC_BANK 0x5
#define LOGIC_CODE_BANK 0x6
#define LOGIC_ENTRY_ADDRESSES_BANK 8
#define LRU_CACHE_LOGIC_BANK 0x4
#define MEDIUM_FIRST_BANK 0x21
#define MEKA_BANK 0x6
#define MEMORY_MANAGEMENT_BANK 0x10
#define MENU_BANK 0xA
#define OBJECT_BANK 0xD
#define OBJ_SHOW_BANK 0x11
#define PALETTE_MANAGER_BANK 0xF
#define PARSER_BANK 0x7
#define PICTURE_CODE_BANK 0x11
#define PICTURE_CODE_OVERFLOW_BANK 0x8
#define PICTURE_DATA_BANK 0xC
#define POSITION_HELPERS_BANK 0x9
#define RANDOM_BANK 0x6
#define SMALL_FIRST_BANK 0x15
#define SOUND_BANK 0xB
#define SPRITE_MEMORY_MANAGER_BANK 0xD
#define SPRITE_METADATA_BANK 0xE
#define SPRITE_UPDATED_BANK 0xE
#define STRING_BANK 0x7
#define TEXT_CODE_BANK 3
#define TINY_FIRST_BANK  0x13
#define VIEWTAB_BANK 9
#define WORD_BANK 0x12

#define VIEW_TABLE_SIZE 20
#define ANIMATED 0x0040
#define DRAWN 0x0001
#define UPDATE 0x0010
#define MOTION 0x0080
#define IGNOREHORIZON 0x0008
#define IGNOREBLOCKS 0x0002
#define FIXEDPRIORITY 0x0004
#define ONWATER 0x0100
#define ONLAND 0x0800
/* textLayer.h (replaced above): message boxes start below the status rows. */
#define FIRST_TEXT_ROW 4
#include "general.h"
void trampoline(void);
#include "graphics.h"
#include "agifiles.h"
#include "movement.h"
#include "teleport/teleport.h"
/* Entry points the tests drive directly (meka.c reaches them through
   b6TeleportCycle and b6TeleportAfterLogic). */
void b6BeginTeleport(void);
void b6FinishTeleport(void);
void bFTeleportTourTick(void);
#include <assert.h>

/* teleport_services.c: the engine around the teleport modules. */
extern byte testVars[256];
extern boolean testFlags[256];
extern unsigned int testJiffies;
extern byte* var;
extern boolean* flag;
extern ViewTable viewtab[VIEW_TABLE_SIZE];
extern byte horizon;
extern int numLogics, controlMode, dirnOfEgo, newRoomNum;
extern byte b7Directions[9], b9PreComputedPriority[256];
extern unsigned pixelReads, notices, waits, clears, transitions;
extern unsigned fileOpens, fileSeeks;
extern char lastMessage[128];
extern const char* testGame;
/* Points the file services at a game directory; loads LOGDIR and PICDIR. */
void testLoadGame(const char* game);
boolean testFileOpen(void);
extern byte testNoMacptr;       /* nonzero: the device has no block read */

/* State exported by teleportGrammar.s. */
extern byte bFTeleportStopCount, bFTeleportStopRoom[8], bFTeleportStopX[8];
extern byte bFTeleportStopY[8], bFTeleportStopFirst[9], bFTeleportWords;
extern byte bFTeleportWordStart[12], bFTeleportWordLength[12];
extern byte bFTeleportDelay, bFTeleportTourForm, bFTeleportHits[12];
/* ViewTable offsets the assembly assumes (teleportState.s). */
extern char teleportVtXPos[], teleportVtYPos[], teleportVtXSize[], teleportVtYSize[];
extern char teleportVtDirection[], teleportVtMotion[], teleportVtFlags[];
extern char teleportVtStale[], teleportVtPrevX[], teleportVtPrevY[], teleportVtSize[];
boolean b9CanBeHere(ViewTable* localViewtab, byte entryNum); /* collision.inc */
#endif
