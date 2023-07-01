/***************************************************************************
** picture.h
***************************************************************************/

#ifndef _PICTURE_H_
#define _PICTURE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <ctype.h>

#include "stub.h"
#include "helpers.h"
#include "general.h"
#include "graphics.h"
#include "stub.h"
#include "agifiles.h"
#include "memoryManager.h"

#define DEFAULT_COLOR 0xF

#define PWIDTH   160  /* Picture resolution */
#define PHEIGHT  168

#define BITMAP_WIDTH 320
#define BITMAP_HEIGHT 240

#define  AGI_GRAPHICS  0
#define  AGI_TEXT      1

typedef struct {
   int loaded;
   unsigned int size;
   char *data;
   byte bank;
} PictureFile;

extern PictureFile* loadedPictures;

extern boolean okToShowPic;
extern int screenMode;
extern int min_print_line, user_input_line, status_line_num;
extern boolean statusLineDisplayed, inputLineDisplayed;
extern BITMAP *picture, *priority, *control, *agi_screen, *working_screen;

extern void b11InitPicture();
extern void b11InitPictures();

void b11LoadPictureFile(int picFileNum);
void b11ShowPicture();
void b11DiscardPictureFile(int picFileNum);


void drawPicTrampoline(byte* data, int pLen, boolean okToClearScreen);

#endif  /* _PICTURE_H_ */