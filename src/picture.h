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
#include "loadingScreen.h"
#include "graphics.h"
#include "floatDivision.h"

#define DEFAULT_COLOR 0xF

#define  AGI_GRAPHICS  0
#define  AGI_TEXT      1

#define MULT_HALF_POINT 128

typedef struct {
	int loaded; //0
	unsigned int size; //2
	byte* data; //4
	byte bank; //6
} PictureFile;

extern PictureFile* loadedPictures;

extern int screenMode;
extern int min_print_line, user_input_line, status_line_num;
extern boolean statusLineDisplayed, inputLineDisplayed;
extern BITMAP* picture, * priority, * control, * agi_screen, * working_screen;

extern void b6DisableAndWaitForVsync();

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
extern void b4ClearBackground();
extern void b4ClearPicture();
#pragma wrapped-call (pop);

#pragma wrapped-call (push, trampoline, PICTURE_CODE_BANK)
void b11DrawPic(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum);
#pragma wrapped-call (pop)
#pragma wrapped-call (push, trampoline, MEKA_BANK)
extern void b6InitPictures();
void b6LoadPictureFile(int picFileNum);
void b6ShowPicture();
void b6DiscardPictureFile(int picFileNum);
void b6ShowPicture();
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
extern long b4GetVeraPictureAddress(int x, int y);
#pragma wrapped-call (pop)

#pragma wrapped-call(push, trampoline, LINE_DRAW_BANK)
extern long b8GetVeraPictureAddress(byte x, byte y);
#pragma wrapped-call (pop)


extern void getLoadedPicture(PictureFile* returnedloadedPicture, byte loadedPictureNumber);


#endif  /* _PICTURE_H_ */