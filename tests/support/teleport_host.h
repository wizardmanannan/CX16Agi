#ifndef TELEPORT_HOST_H
#define TELEPORT_HOST_H

/* Replace only hardware-facing headers/services. Keep the production data
   types, ViewTable layout, resource declarations and teleport API. This file
   is force-included by the host test build, never by the CX16 build. */
#define _MEMORYMANAGER_H_
#define _HELPERS_H_
#define _IRQ_H_
#define _PARSER_H_
#define _VIEW_H_
#define _TEXTLAYER_H_
#define NO_DIRECTORY_ENTRYS 256
#define STRING_BANK 7
#define POSITION_HELPERS_BANK 9
#define VIEWTAB_BANK 9
#define GRAPHICS_BANK 6
#define FILE_LOADER_HELPERS 6
#define ANIMATED 0x0040
#define DRAWN 0x0001
#define UPDATE 0x0010
#define MOTION 0x0080
#define IGNOREHORIZON 0x0008
#define IGNOREBLOCKS 0x0002
#define FIXEDPRIORITY 0x0004
#define ONWATER 0x0100
#define ONLAND 0x0800
#define KEY_ENTER 13
#define KEY_ESC 27
#define AUTO_CALC_ROW 255
#define AUTO_CALC_COLUMN 255
#define TEXTBOX_PALETTE_NUMBER 1
#define DEFAULT_BOX_WIDTH 32

#include "general.h"
void trampoline(void);
#include "graphics.h"
#include "agifiles.h"
#include "movement.h"
#include "teleport.h"

extern byte* var;
extern byte b7Directions[9];
void b10GetLogicDirectory(AGIFilePosType* result, AGIFilePosType* location);
void getViewTab(ViewTable* result, byte index);
void setViewTab(ViewTable* value, byte index);
void memsetBanked(void* dest, int value, size_t length, byte bank);
void b3DisplayMessageBox(char* message, byte bank, byte row, byte col,
                        byte palette, byte width, boolean wrap);
void b5WaitOnSpecificKeys(byte* keys, byte length);
void b3ClearLastPlacedText(void);

#endif
