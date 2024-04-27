/**************************************************************************
** picture.c
**
** Routines to load AGI picture resources, discard them, draw them and
** show the various screens.
**
** (c) Lance Ewing, 1997 - Modified (6 Jan 98)
**************************************************************************/

#include "picture.h"

#define PIC_DEFAULT 15
#define PRI_DEFAULT 4

//#define VERBOSE
//#define VERBOSE_REL_DRAW
//#define TEST_QUEUE
//#define VERBOSE_FLOOD
//#define TEST_ROUND
//#define VERBOSE_DRAW_LINE
//#define TEST_OK_TO_FILL
//#define VERBOSE_X_CORNER
//#define VERBOSE_ABS_LINE
boolean okToShowPic = FALSE;
PictureFile* loadedPictures = (PictureFile*)&BANK_RAM[PICTURE_START];
int screenMode;
int min_print_line = 1, user_input_line = 23, status_line_num = 0;
boolean statusLineDisplayed = FALSE, inputLineDisplayed = FALSE;

BITMAP* picture;
BITMAP* priority;
BITMAP* control;
BITMAP* agi_screen;      /* This is a subbitmap of the screen */

boolean picDrawEnabled = FALSE, priDrawEnabled = FALSE;
/* Not sure what the default patCode value is */
byte picColour = 0, priColour = 0, patCode, patNum;


/* QUEUE DEFINITIONS */
#define QEMPTY 0xFF
int* bitmapWidthPreMult = &BANK_RAM[BITMAP_WIDTH_PREMULT_START];

#ifdef VERBOSE_FLOOD
extern long pixelCounter;
extern long pixelStartPrintingAt;
#endif // VERBOSE_FLOOD


/**************************************************************************
** pset
**
** Draws a pixel in each screen depending on whether drawing in that
** screen is enabled or not.
**************************************************************************/
#define PSET(x, y) \
    if (picDrawEnabled) { \
if ((x) <= 159 && (y) <= 167) {  \
             toDraw = picColour << 4 | picColour; \
             drawWhere = (STARTING_BYTE + x) + (bitmapWidthPreMult[y]);  \
			SET_VERA_ADDRESS_ABSOLUTE(drawWhere, ADDRESSSEL0, 0) \
			 WRITE_BYTE_VAR_TO_ASSM(toDraw, VERA_data0); \
            \
           } \
    }


void getLoadedPicture(PictureFile* returnedloadedPicture, byte loadedPictureNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = PICTURE_BANK;

	*returnedloadedPicture = loadedPictures[loadedPictureNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedPicture(PictureFile* loadedPicture, byte loadedPictureNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = PICTURE_BANK;

	loadedPictures[loadedPictureNumber] = *loadedPicture;

	RAM_BANK = previousRamBank;
}

#pragma code-name (push, "BANKRAMFLOOD")

#pragma wrapped-call (push, trampoline, FIRST_FLOOD_BANK)
extern void bFloodAgiFill(byte x, byte y);
#pragma wrapped-call (pop)

extern byte goNoFurtherLeft;
extern byte goNoFurtherRight;

#ifdef TEST_OK_TO_FILL
#pragma wrapped-call (push, trampoline, FIRST_FLOOD_BANK)
void testOkToFill()
{
	priDrawEnabled = FALSE;
	picDrawEnabled = FALSE;
	if (bFloodOkToFill((byte*)0x1680))
	{
		printf("fail pri false pic false\n");
	}

	picColour = DEFAULT_COLOR;
	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (bFloodOkToFill((byte*)0x1680))
	{
		printf("fail color default test\n");
	}

	picColour = 3;
	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x1680))
	{
		printf("fail pri disabled pic enabled (left border)\n");
	}

	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x171F))
	{
		printf("fail pri disabled pic enabled (right border)\n");
	}

	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x1681))
	{
		printf("fail pri disabled pic enabled (non border)\n");
	}

	picColour = 3;
	priDrawEnabled = TRUE;
	picDrawEnabled = FALSE;
	if (bFloodOkToFill((byte*)0x1680))
	{
		printf("fail pri enabled pic disabled (left border)\n");
	}

	picColour = 3;
	priDrawEnabled = TRUE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x1680))
	{
		printf("fail both enabled (left border)\n");
	}

	picColour = 3;
	priDrawEnabled = TRUE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x171F))
	{
		printf("fail both enabled (right border)\n");
	}

	b11PSet(0, 0);
	picColour = 3;
	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;

	if (bFloodOkToFill((byte*)0x1680))
	{
		printf("fail both enabled\n");
	}

	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (!bFloodOkToFill((byte*)0x7F7F)) //One before boundary
	{
		printf("fail bound check x before\n");
	}

	priDrawEnabled = FALSE;
	picDrawEnabled = TRUE;
	if (bFloodOkToFill((byte*)0x7F80)) //One over boundary
	{
		printf("fail bound check x after\n");
	}
	
	exit(0);
}
#pragma wrapped-call (pop)
#endif // TEST_OK_TO_FILL


#ifdef TEST_QUEUE
void testQueue()
{
	int testAmount = 40103;
	byte testVal;
	unsigned int i;
	printf("The address of i %p and the address of testVal is %p\n", &i, &testVal);

	for (i = 0; i <= testAmount; i++)
	{
		bFloodQstore(i);
	}


	for (i = 0; i <= testAmount; i++)
	{
		testVal = bFloodQretrieve();
		if ((byte)i != testVal)
		{
			asm("stp");
			asm("lda #4"); //This is a deliberately pointless instruction. It is there so we can see where we have stopped in the debugger
		}
	}

	asm("stp");
	asm("lda #$19");
}
#endif // TEST_QUEUE

#pragma code-name (pop)

#pragma code-name (push, "BANKRAM04")


#define plotPatternPoint() \
   if (patCode & 0x20) { \
      if ((splatterMap[bitPos>>3] >> (7-(bitPos&7))) & 1) PSET(x1, y1); \
      bitPos++; \
      if (bitPos == 0xff) bitPos=0; \
   } else PSET(x1, y1)

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
/**************************************************************************
** plotPattern
**
** Draws pixels, circles, squares, or splatter brush patterns depending
** on the pattern code.
**************************************************************************/
void b4PlotPattern(byte x, byte y)
{
	static char circles[][15] = { /* agi circle bitmaps */
	  {0x80},
	  {0xfc},
	  {0x5f, 0xf4},
	  {0x66, 0xff, 0xf6, 0x60},
	  {0x23, 0xbf, 0xff, 0xff, 0xee, 0x20},
	  {0x31, 0xe7, 0x9e, 0xff, 0xff, 0xde, 0x79, 0xe3, 0x00},
	  {0x38, 0xf9, 0xf3, 0xef, 0xff, 0xff, 0xff, 0xfe, 0xf9, 0xf3, 0xe3, 0x80},
	  {0x18, 0x3c, 0x7e, 0x7e, 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x7e,
	   0x7e, 0x3c, 0x18}
	};

	static byte splatterMap[32] = { /* splatter brush bitmaps */
	  0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2,
	  0x82, 0x09, 0x0a, 0x22, 0x12, 0x10, 0x42, 0x14,
	  0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10,
	  0x22, 0xa8, 0x14, 0x24, 0x00, 0x50, 0x24, 0x04
	};

	static byte splatterStart[128] = { /* starting bit position */
	  0x00, 0x18, 0x30, 0xc4, 0xdc, 0x65, 0xeb, 0x48,
	  0x60, 0xbd, 0x89, 0x05, 0x0a, 0xf4, 0x7d, 0x7d,
	  0x85, 0xb0, 0x8e, 0x95, 0x1f, 0x22, 0x0d, 0xdf,
	  0x2a, 0x78, 0xd5, 0x73, 0x1c, 0xb4, 0x40, 0xa1,
	  0xb9, 0x3c, 0xca, 0x58, 0x92, 0x34, 0xcc, 0xce,
	  0xd7, 0x42, 0x90, 0x0f, 0x8b, 0x7f, 0x32, 0xed,
	  0x5c, 0x9d, 0xc8, 0x99, 0xad, 0x4e, 0x56, 0xa6,
	  0xf7, 0x68, 0xb7, 0x25, 0x82, 0x37, 0x3a, 0x51,
	  0x69, 0x26, 0x38, 0x52, 0x9e, 0x9a, 0x4f, 0xa7,
	  0x43, 0x10, 0x80, 0xee, 0x3d, 0x59, 0x35, 0xcf,
	  0x79, 0x74, 0xb5, 0xa2, 0xb1, 0x96, 0x23, 0xe0,
	  0xbe, 0x05, 0xf5, 0x6e, 0x19, 0xc5, 0x66, 0x49,
	  0xf0, 0xd1, 0x54, 0xa9, 0x70, 0x4b, 0xa4, 0xe2,
	  0xe6, 0xe5, 0xab, 0xe4, 0xd2, 0xaa, 0x4c, 0xe3,
	  0x06, 0x6f, 0xc6, 0x4a, 0xa4, 0x75, 0x97, 0xe1
	};

	int circlePos = 0;
	byte x1, y1, penSize, bitPos = splatterStart[patNum];

	penSize = (patCode & 7);

	/* Don't know exactly what happens at the borders, but it can definitely
	** cause problems if it isn't right. */
	/*
	if (x<((penSize/2)+1)) x=((penSize/2)+1);
	else if (x>160-((penSize/2)+1)) x=160-((penSize/2)+1);
	if (y<penSize) y = penSize;
	else if (y>=168-penSize) y=167-penSize;
	*/
	if (x < penSize) x = penSize - 1;
	if (y < penSize) y = penSize;
	//else if (y>=168-penSize) y=167-penSize;

	for (y1 = y - penSize; y1 <= y + penSize; y1++) {
		for (x1 = x - (penSize + 1) / 2; x1 <= x + penSize / 2; x1++) {
			if (patCode & 0x10) { /* Square */
				plotPatternPoint();
			}
			else { /* Circle */
				if ((circles[patCode & 7][circlePos >> 3] >> (7 - (circlePos & 7))) & 1) {
					plotPatternPoint();
				}
				circlePos++;
			}
		}
	}

}
#pragma wrapped-call (pop)


long b4GetVeraPictureAddress(int x, int y)
{
	return (STARTING_BYTE + x) + (bitmapWidthPreMult[y]);
}
#pragma code-name (pop)
#pragma code-name (push, "BANKRAM11")

#define ROUND_THRESHOLD_POS ((unsigned int) 0x7FBE)
#define ROUND_THRESHOLD_NEG ((unsigned int) 0x8041)
int b11Round(fix32 aNumber, boolean isPos)
{
	if (isPos)
	{
#ifdef TEST_ROUND
		printf("%lu Pos True %d result %d\n", aNumber, getMantissa(aNumber), getMantissa(aNumber) < ROUND_THRESHOLD_POS ? floor_fix_32(aNumber) : ceil_fix_32(aNumber));
		printf("%u < %u = %d\n", getMantissa(aNumber), ROUND_THRESHOLD_POS, getMantissa(aNumber) < ROUND_THRESHOLD_POS);
		printf("The address of aNumber is %p", &aNumber);
#endif
		return b1GetMantissa(aNumber) < ROUND_THRESHOLD_POS ? b1FloorFix32(aNumber) : b1CeilFix32(aNumber);
	}
	else
	{
#ifdef TEST_ROUND
		printf("%lu Neg True %d result %p %d < %d\n", aNumber, getMantissa(aNumber), getMantissa(aNumber) <= ROUND_THRESHOLD_NEG ? floor_fix_32(aNumber) : ceil_fix_32(aNumber), getMantissa(aNumber), ROUND_THRESHOLD_POS);
		printf("%u < %u = %d\n", getMantissa(aNumber), ROUND_THRESHOLD_POS, getMantissa(aNumber) < ROUND_THRESHOLD_POS);
#endif
		return b1GetMantissa(aNumber) <= ROUND_THRESHOLD_NEG ? b1FloorFix32(aNumber) : b1CeilFix32(aNumber);
	}
}

#ifdef TEST_ROUND
void testRound()
{
	int result;
	result = round(0x0F7FBE, TRUE);

	if (result != 0x10)
	{
		printf("Fail Round 1 Pos (Equal). Expected %p got %p \n", 0x10, result);
	}

	result = round(0x0F7F7C, TRUE);

	if (result != 0x0F)
	{
		printf("Fail Round 2 Pos (Less). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8000, TRUE);

	if (result != 0x10)
	{
		printf("Fail Round 3 Pos (Greater). Expected %p got %p \n", 0x10, result);
	}

	result = round(0x0F8041, FALSE);

	if (result != 0xF)
	{
		printf("Fail Round 1 Neg (Equal). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8000, FALSE);

	if (result != 0xF)
	{
		printf("Fail Round 2 Neg (Less). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8083, FALSE);

	if (result != 0x10)
	{
		printf("Fail Round 3 Neg (Greater). Expected %p got %p \n", 0x10, result);
	}
}
#endif // TEST_ROUND


extern void b11FillClean();
/**************************************************************************
** fill
**
** Agi flood fill.  (drawing action 0xF8)
**************************************************************************/
byte b11FloodFill(byte** data, BufferStatus* bufferStatus, boolean* cleanPic)
{
	byte x1, y1;
	byte picColorOld = picColour;

	picColour = 0xE;

	//b11PSet(90, 43);
	picColour = picColorOld;

	for (;;) {
		GET_NEXT(x1);
		if (x1 >= 0xF0) return x1;

		GET_NEXT(y1);
		if (y1 >= 0xF0) return y1;

		if (*cleanPic && picDrawEnabled)
		{
			b11FillClean();
			*cleanPic = FALSE;
		}
		else
		{
			bFloodAgiFill(x1, y1);
		}
	}
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM06")
/**************************************************************************
** initPicture
**
** Purpose: To initialize allegro and create the picture and priority
** bitmaps. This function gets called once at the start of an AGI program.
**************************************************************************/
void b6InitPicture()
{
	int i;
	int* tempbitmapWidthPreMult = (int*)GOLDEN_RAM_WORK_AREA;

	for (i = 0; i < PICTURE_HEIGHT; i++)
	{
		tempbitmapWidthPreMult[i] = i * BYTES_PER_ROW;
	}

	memCpyBanked(&bitmapWidthPreMult[0], &tempbitmapWidthPreMult[0], PICTURE_CODE_BANK, PICTURE_HEIGHT * 2);
	memCpyBanked(&bitmapWidthPreMult[0], &tempbitmapWidthPreMult[0], PICTURE_CODE_OVERFLOW_BANK, PICTURE_HEIGHT * 2);

	for (i = FIRST_FLOOD_BANK; i <= LAST_FLOOD_BANK; i++)
	{
		memCpyBanked(&bitmapWidthPreMult[0], &tempbitmapWidthPreMult[0], i, PICTURE_HEIGHT * 2);
	}
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM11")
/**************************************************************************
** priPSet
**
** Draws a pixel in the priority screen.
**************************************************************************/
void b11PriPSet(word x, word y)
{
	if (x > 159) return;
	if (y > 167) return;
	//priority->line[y][x] = priColour;
}

unsigned long lineDrawCounter = 0;

extern long pixelCounter;
extern long pixelStartPrintingAt;

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
extern void b4DrawStraightLineAlongX(byte x1, byte x2, byte y);
extern void b4DrawStraightLineAlongY(byte x1, byte x2, byte y);
#pragma wrapped-call (pop)


void b11Drawline(byte x1, byte y1, byte x2, byte y2)
{
	int height, width, startX, startY;
	boolean xIsPos = TRUE, yIsPos = TRUE;
	fix32 x, y, addX, addY;
	word temp;
	long drawWhere;
	byte toDraw;
	
#ifdef VERBOSE_DRAW_LINE
	if (pixelCounter >= pixelStartPrintingAt)
	{
		printf("drawing %d:%d %d:%d. add line draw counter is %p value is %lu\n", x1, y1, x2, y2, &lineDrawCounter, lineDrawCounter);
	}
#endif // VERBOSE_DRAW_LINE

	if (x1 > x2)
	{
		xIsPos = FALSE;
	}

	if (y1 > y2)
	{
		yIsPos = FALSE;
	}

	if (x1 == x2)
	{
		if (yIsPos)
		{
			b4DrawStraightLineAlongY(y1, y2, x1);
		}
		else
		{
			b4DrawStraightLineAlongY(y2, y1, x1);
		}
	}
	else if (y1 == y2)
	{
		if (xIsPos)
		{		
			b4DrawStraightLineAlongX(x1, x2, y1);
		}
		else
		{
			b4DrawStraightLineAlongX(x2, x1, y1);
		}
	}
	else
	{
		height = (y2 - y1);

#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			printf("Height %d - %d = %d\n", y2, y1, height);
		}
#endif

		width = (x2 - x1);



#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			printf("Width %d - %d = %d \n", x2, x1, width);
		}
#endif

		addX = height == 0 ? height : b1Div(abs(width), abs(height));

#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			printf("add x div (abs(w: %d), abs(h %d) = %lx \n", abs(width), abs(height), DIV(abs(width), abs(height)));
		}
#endif

#ifdef VERBOSE_DRAW_LINE
		if (!xIsPos)
		{
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("x is neg \n");
			}

		}
#endif // VERBOSE_DRAW_LINE

		addY = width == 0 ? width : b1Div(abs(height), abs(width));
#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			printf("add y div(abs(h: %d), abs(w %d) = %lx \n", abs(height), abs(width), DIV(abs(height), abs(width)));
		}
#endif

#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			if (!yIsPos)
			{
				printf("y is neg \n");
			}
		}
#endif

#ifdef VERBOSE_DRAW_LINE
		if (pixelCounter >= pixelStartPrintingAt)
		{
			printf("divide addy %d / %d result: %lx. Address %p\n ", height, width, addY, &addY);
		}
#endif // VERBOSE

		if (abs(width) > abs(height)) {
			y = b1FpFromInt(y1);


#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("convert top y %d to fix32 %lx\n ", y1, y);
			}
#endif // VERBOSE

			addX = (width == 0 ? 0 : b1FpFromInt(1));

#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("convert top width (%d) to fix32 %lx\n ", width, addX);
			}
#endif // VERBOSE

			for (x = b1FpFromInt(x1); xIsPos ? x < b1FpFromInt(x2) : x > b1FpFromInt(x2); xIsPos ? x += addX : x -= addX) {
#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					/*		printf("x is %lx\n", x);*/
					printf("psettop in loop %lx, %d (isPos), %lx, %d (isPos)  round %d %d\n", x, xIsPos, y, yIsPos, round(x, xIsPos), round(y, yIsPos));
				}
#endif // VERBOSE

				PSET(b11Round(x, xIsPos), b11Round(y, yIsPos));

#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					printf("add y top %lx + %lx = %lx. yIsPos %d\n", y, addY, yIsPos ? y + addY : y - addY, yIsPos);
				}
#endif
				yIsPos ? y += addY : y -= addY;

#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					printf("add x top %lx + %lx = %lx, %lx != %lx (%d). xIsPos %d\n", x, addX, xIsPos ? x + addX : x - addX, xIsPos ? x + addX : x - addX, fp_fromInt(x2), xIsPos ? x + addX != fp_fromInt(x2) : x - addX != fp_fromInt(x2), xIsPos);
				}
#endif
			}

#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("pset top out of loop %d,%d\n", x2, y2);
			}
#endif
			PSET(x2, y2);
		}
		else {
			x = b1FpFromInt(x1);
#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("%d convert bottom x to fix32 %d\n ", x1, x);
			}
#endif // VERBOSE


			addY = (height == 0 ? 0 : b1FpFromInt(1));


#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("convert top height (%d) to fix32 %lx\n ", height, addY);
			}
#endif // VERBOSE



			for (y = b1FpFromInt(y1); y != b1FpFromInt(y2); yIsPos ? y += addY : y -= addY) {

#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					printf("pset bottom in loop %lx, %d (isPos), %lx, %d (isPos)  round %d %d\n", x, xIsPos, y, yIsPos, round(x, xIsPos), round(y, yIsPos));
				}
#endif // VERBOSE
				PSET(b11Round(x, xIsPos), b11Round(y, yIsPos));


#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					printf("add x bottom %lx + %lx = %lx. xIsPos %d\n", x, addX, xIsPos ? x + addX : x - addX, xIsPos);
				}
#endif
				xIsPos ? x += addX : x -= addX;

#ifdef VERBOSE_DRAW_LINE
				if (pixelCounter >= pixelStartPrintingAt)
				{
					printf("add y bottom %lx + %lx = %lx, %lx != %lx (%d). yIsPos %d\n", y, addY, yIsPos ? y + addY : y - addY, yIsPos ? y + addY : y - addY, fp_fromInt(y2), yIsPos ? y + addY != fp_fromInt(y2) : y - addY != fp_fromInt(y2), yIsPos);
				}
#endif
			}

#ifdef VERBOSE_DRAW_LINE
			if (pixelCounter >= pixelStartPrintingAt)
			{
				printf("pset bottom out loop %d,%d\n", x2, y2);
			}
#endif
			PSET(x2, y2);
		}
	}
#ifdef VERBOSE_DRAW_LINE
	lineDrawCounter++;
#endif // VERBOSE_DRAW_LINE
}

/**************************************************************************
** xCorner
**
** Draws an xCorner  (drawing action 0xF5)
**************************************************************************/
byte b11XCorner(byte** data, BufferStatus* bufferStatus)
{
	byte x1, x2, y1, y2;

	GET_NEXT(x1);
	GET_NEXT(y1);

	PSET(x1, y1);

	for (;;) {
		GET_NEXT(x2);
		if (x2 >= 0xF0) return x2;


#ifdef VERBOSE_X_CORNER
		printf("x corner line 1: %d,%d : %d,%d. Address data %p\n", x1, y1, x2, y2, *data);
#endif

		b11Drawline(x1, y1, x2, y1);
		x1 = x2;
		GET_NEXT(y2);
		if (y2 >= 0xF0) return y2;

#ifdef VERBOSE_X_CORNER
		printf("x corner line 2: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif
		b11Drawline(x1, y1, x1, y2);
		y1 = y2;
	}
}

/**************************************************************************
** yCorner
**
** Draws an yCorner  (drawing action 0xF4)
**************************************************************************/
byte b11YCorner(byte** data, BufferStatus* bufferStatus)
{
	byte x1, x2, y1, y2;

	GET_NEXT(x1);
	GET_NEXT(y1);

	PSET(x1, y1);

	for (;;) {
		GET_NEXT(y2);
		if (y2 >= 0xF0) return y2;



#ifdef VERBOSE_Y_CORN
		printf("y corner line 1: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif

		b11Drawline(x1, y1, x1, y2);
		y1 = y2;
		GET_NEXT(x2);
		if (x2 >= 0xF0) return x2;

#ifdef VERBOSE_Y_CORN
		printf("y Corner line 2: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif
		b11Drawline(x1, y1, x2, y1);
		x1 = x2;
	}
}

/**************************************************************************
** relativeDraw
**
** Draws short lines relative to last position.  (drawing action 0xF7)
**************************************************************************/
byte b11RelativeDraw(byte** data, BufferStatus* bufferStatus)
{
	byte x1, y1, disp;
	signed char dx, dy;

	GET_NEXT(x1);
	GET_NEXT(y1);

	PSET(x1, y1);

	for (;;) {
		GET_NEXT(disp);

#ifdef VERBOSE_REL_DRAW
		printf("Disp %u \n", disp);
#endif // VERBOSE

		if (disp >= 0xF0) return disp;
		dx = ((disp & 0xF0) >> 4) & 0x0F;
#ifdef VERBOSE_REL_DRAW
		printf("Prior dx: ((%u & 0xF0) >> 4) & 0x0f : %d\n", disp, dx);
#endif


		dy = (disp & 0x0F);

#ifdef VERBOSE_REL_DRAW
		printf("Prior dy: ( %u & 0x0f): %d\n", disp, dy);
#endif

		if (dx & 0x08)
		{
#ifdef VERBOSE_REL_DRAW
			printf("x neg -1 * (%d & 07) :  %d \n", dx, (-1) * (dx & 0x07));
#endif
			dx = (-1) * (dx & 0x07);
#ifdef VERBOSE_REL_DRAW
			printf("dy is %d\n", dy);
#endif
		}
#ifdef  VERBOSE_REL_DRAW
		else
		{
			printf("x not neg %d \n", dx & 0x08);
		}
#endif //  VERBOSE

		if (dy & 0x08)
		{
#ifdef VERBOSE_REL_DRAW
			printf("y neg -1 * (%d & 07) :  %d \n", dy, (-1) * (dy & 0x07));
#endif
			dy = (-1) * (dy & 0x07);

#ifdef VERBOSE_REL_DRAW
			printf("dy is %d\n", dy);
#endif
		}
#ifdef  VERBOSE_REL_DRAW
		else
		{
			printf("y not neg %d \n", dy & 0x08);
		}
#endif //  VERBOSE

#ifdef VERBOSE_REL_DRAW
		printf("Rel Draw  x1 %u, y1 %u dx: %d, dy: %d, x1 + dx: %u, y1 + dy %u \n", x1, y1, dx, dy, x1 + dx, y1 + dy);
#endif // VERBOSE


#ifdef VERBOSE_REL_DRAW
		printf("rel line: %d,%d : %d,%d\n", x1, y1, x1 + dx, y1 + dy);
#endif

		b11Drawline(x1, y1, x1 + dx, y1 + dy);
		x1 += dx;
		y1 += dy;
	}
}

/**************************************************************************
** absoluteLine
**
** Draws long lines to actual locations (cf. relative) (drawing action 0xF6)
**************************************************************************/
byte b11AbsoluteLine(byte** data, BufferStatus* bufferStatus)
{
	byte x1, y1, x2, y2;

	GET_NEXT(x1);
	GET_NEXT(y1);

	PSET(x1, y1);

	for (;;) {
		GET_NEXT(x2);
		if (x2 >= 0xF0)
		{
			//#ifdef VERBOSE
			//            printf("Absolute Line Break\n");
			//#endif // VERBOSE

			return x2;
		}
		GET_NEXT(y2);
		if (y2 >= 0xF0)
		{
			//#ifdef VERBOSE
			//            printf("Absolute Line Break\n");
			//#endif // VERBOSE
			return y2;
		}
#ifdef VERBOSE_ABS_LINE
		printf("abs line: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif
		b11Drawline(x1, y1, x2, y2);
		x1 = x2;
		y1 = y2;
	}
}

/**************************************************************************
** plotBrush
**
** Plots points and various brush patterns.
**************************************************************************/
byte b11PlotBrush(byte** data, BufferStatus* bufferStatus)
{
	byte x1, y1, store;

	for (;;) {
		if (patCode & 0x20) {
			GET_NEXT(patNum);
			if (patCode >= 0xF0) return patCode;
			patNum = ((patNum >> 1) & 0x7f);
		}
		GET_NEXT(x1);
		if (x1 >= 0xF0) return x1;

		GET_NEXT(y1);
		if (y1 >= 0xF0) return y1;
		b4PlotPattern(x1, y1);
	}
}

/**************************************************************************
** splitPriority
**
** Purpose: To split the priority colours from the control colours. It
** determines the priority information for pixels that are overdrawn by
** control lines by the same method used in Sierras interpreter (at this
** stage). This could change later on.
**************************************************************************/
void b11SplitPriority()
{
	int x, y, dy;
	byte data;
	boolean priFound;

	//for (x = 0; x < 160; x++) {
	//    for (y = 0; y < 168; y++) {
	//        data = priority->line[y][x];
	//        if (data == 3) {
	//            priority->line[y][x] = 4;
	//            control->line[y][x] = data;
	//        }
	//        if (data < 3) {
	//            control->line[y][x] = data;
	//            dy = y + 1;
	//            priFound = FALSE;
	//            while (!priFound && (dy < 168)) {
	//                data = priority->line[dy][x];
	//                /* The following if statement is a hack to fix a problem
	//                ** in KQ1 room 1.
	//                */
	//                if (data == 3) {
	//                    priFound = TRUE;
	//                    priority->line[y][x] = 4;
	//                }
	//                else if (data > 3) {
	//                    priFound = TRUE;
	//                    priority->line[y][x] = data;
	//                }
	//                else
	//                    dy++;
	//            }
	//        }
	//    }
	//}
}

int picFNum = 0;
boolean showPicCalled = FALSE;

/**************************************************************************
** drawPic
**
** Purpose: To draw an AGI picture from the given PICTURE resource data.
**
**  pLen = length of PICTURE data
**************************************************************************/
void b11DrawPic(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum)
{
	unsigned long i;
	byte action, returnedAction = 0;
	boolean stillDrawing = TRUE;
	PictureFile loadedPicture;
	byte* buffer = GOLDEN_RAM_WORK_AREA;
	byte** data = &buffer; //Get_Next Macro works with pointer pointers so need this;
	BufferStatus localBufferStatus;
	BufferStatus* bufferStatus = &localBufferStatus;
	boolean cleanPic = TRUE;

	int** zpPremultTable = (int**)ZP_PTR_TMP_20;
	int** zpFloodQueueStore = (int**)ZP_PTR_TMP_21;
	int** zpFloodQueueServe = (int**)ZP_PTR_TMP_22;
	int* sPosBank = (int*)ZP_PTR_TMP_3;
	int* rPosBank = (int*)ZP_PTR_TMP_4;

	return;

	*zpPremultTable = &bitmapWidthPreMult[0];
	*zpFloodQueueStore = (int*)FLOOD_QUEUE_START;
	*zpFloodQueueServe = (int*)FLOOD_QUEUE_START;
	*sPosBank = FIRST_FLOOD_BANK;
	*rPosBank = FIRST_FLOOD_BANK;

	getLoadedPicture(&loadedPicture, picNum);

#ifdef VERBOSE
	printf("Preparing To Draw %d of size %d\n", picNum, loadedPicture.size);
#endif // VERBOSE

	if (!data)
	{
		printf("Out of memory in picture code");
	}

	localBufferStatus.bank = loadedPicture.bank;
	localBufferStatus.bankedData = loadedPicture.data;
	localBufferStatus.bufferCounter = 0;

	b5RefreshBuffer(bufferStatus);

	if (okToClearScreen) {
		b3InitLayer1Mapbase();
		
		asm("sei");
		b4ClearPicture();
	}
	
	asm("sei");

#ifdef TEST_OK_TO_FILL
	testOkToFill();
#endif

	patCode = 0x00;

#ifdef VERBOSE
	printf("Plotting. . .\n");
#endif // VERBOSE

	do {
		if (!returnedAction)
		{
			GET_NEXT(action);
	    }
		else
		{
			action = returnedAction;
			returnedAction = 0;
		}

#ifdef VERBOSE
		printf("Action: %p \n", action);
#endif // VERBOSE
		switch (action) {
		case 0xFF:
			stillDrawing = 0;
			break;
		case 0xF0: GET_NEXT(picColour);
			picDrawEnabled = TRUE;
			break;
		case 0xF1: picDrawEnabled = FALSE; break;
		case 0xF2: GET_NEXT(priColour);
			priDrawEnabled = TRUE;
			break;
		case 0xF3: priDrawEnabled = FALSE; break;
		case 0xF4: 
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11YCorner(data, bufferStatus); 
			break;
		case 0xF5: 
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11XCorner(data, bufferStatus); 
		break;
		case 0xF6: 
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11AbsoluteLine(data, bufferStatus); 
			break;
		case 0xF7: 
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11RelativeDraw(data, bufferStatus); 
			break;
		case 0xF8: 
			returnedAction = b11FloodFill(data, bufferStatus, &cleanPic);
			break;
		case 0xF9: GET_NEXT(patCode); break;
		case 0xFA: 
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11PlotBrush(data, bufferStatus); break;
		default: 
			printf("Unknown picture code : %X\n", action); 
			printf("The buffer status bank is %p, buffer status banked data is %p and the buffer counter is %d. The loaded picture is %d\n", bufferStatus->bank, bufferStatus->bankedData, bufferStatus->bufferCounter, picNum);
			printf("Loaded picture data %p, bank %d, size %d\n", loadedPicture.data, loadedPicture.bank, loadedPicture.size);
				for (;;);
			exit(0);
		}

		//if (picFNum == 3) {
		//   showPicture();
		//   if ((readkey() >> 8) == KEY_ESC) closedown();
		//}
#ifdef VERBOSE
		printf(" data %p pLen %d data + pLen %p stillDrawing %d \n", data, pLen, data + pLen, stillDrawing);
#endif
	} while ((data < (data + pLen)) && stillDrawing);

	b11SplitPriority();

	REENABLE_INTERRUPTS(); //Loading screen stays on until showPic command

	showPicCalled = FALSE;
}

void b6InitPictures()
{
	int i;
	PictureFile loadedPicture;
	for (i = 0; i < 256; i++) {
		getLoadedPicture(&loadedPicture, i);
		loadedPicture.loaded = FALSE;
		setLoadedPicture(&loadedPicture, i);
	}
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM06")
/***************************************************************************
** loadPictureFile
**
** Purpose: To load the picture resource with the given number.
***************************************************************************/
void b6LoadPictureFile(int picFileNum)
{
	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	PictureFile loadedPicture;

	getLoadedPicture(&loadedPicture, picFileNum);

	getLogicDirectory(&agiFilePosType, &picdir[picFileNum]);


#ifdef VERBOSE
	printf("The address of picdir is %p\n", &picdir[picFileNum]);
	printf("The picture number is %d \n", picFileNum);
	printf("Loading Picture file %d, position %d\n", agiFilePosType.fileNum, agiFilePosType.filePos);
#endif
	b6LoadAGIFile(PICTURE, &agiFilePosType, &tempAGI);


	loadedPicture.size = tempAGI.totalSize;
	loadedPicture.data = tempAGI.code;
	loadedPicture.bank = tempAGI.codeBank;
	loadedPicture.loaded = TRUE;

	setLoadedPicture(&loadedPicture, picFileNum);

#ifdef VERBOSE
	printf("Loaded Picture %d, data %p, bank %d, loaded %d\n", loadedPicture.size, loadedPicture.data, loadedPicture.bank, loadedPicture.loaded);
#endif // VERBOSE
}

void b6DiscardPictureFile(int picFileNum)
{
	PictureFile loadedPicture;

	getLoadedPicture(&loadedPicture, picFileNum);

	if (loadedPicture.loaded) {
		loadedPicture.loaded = FALSE;
		b10BankedDealloc(loadedPicture.data, loadedPicture.bank);
	}

	setLoadedPicture(&loadedPicture, picFileNum);
}

void b6ShowPicture()
{
	if (showPicCalled)
	{
		b3InitLayer1Mapbase();
	}
	showPicCalled = TRUE;
}

#pragma code-name (pop)

