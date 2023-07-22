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
#include "stub.h"
#include "agifiles.h"
#include "memoryManager.h"
#include "fixed.h"
#include "helpers.h"

#define DEFAULT_COLOR 0xF

#define PICTURE_WIDTH   160  /* Picture resolution */
#define PICTURE_HEIGHT  168

#define BITMAP_WIDTH 320
#define BITMAP_HEIGHT 240

#define  AGI_GRAPHICS  0
#define  AGI_TEXT      1

#define STARTING_ROW ((BITMAP_HEIGHT / 2) - (PICTURE_HEIGHT / 2))
#define STARTING_BYTE (STARTING_ROW * BYTES_PER_ROW)
#define BYTES_PER_ROW (BITMAP_WIDTH / 2)
#define MULT_HALF_POINT 128

typedef struct {
   int loaded; //0
   unsigned int size; //2
   byte *data; //4
   byte bank; //6
} PictureFile;

extern PictureFile* loadedPictures;

extern boolean okToShowPic;
extern int screenMode;
extern int min_print_line, user_input_line, status_line_num;
extern boolean statusLineDisplayed, inputLineDisplayed;
extern BITMAP *picture, *priority, *control, *agi_screen, *working_screen;

extern void b11InitPicture();
extern void b11InitPictures();
extern void b6DisableAndWaitForVsync();
extern void b6ClearBackground();

void b11LoadPictureFile(int picFileNum);
void b11ShowPicture();
void b11DiscardPictureFile(int picFileNum);


extern void getLoadedPicture(PictureFile* returnedloadedPicture, byte loadedPictureNumber);

extern void b11Drawline(byte x1, byte y1, byte x2, byte y2);

extern byte toDraw;
extern int* drawWhere;

void drawPicTrampoline(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum);


#endif  /* _PICTURE_H_ */