/**************************************************************************
** picture.c
**
** Routines to load AGI picture resources, discard them, draw them and
** show the various screens.
**
** (c) Lance Ewing, 1997 - Modified (6 Jan 98)
**************************************************************************/

#include "picture.h"
//#define TEST_LINE_DRAW
//#define TEST_PRIORITY_DRAW_LINES;

#pragma wrapped-call (push, trampoline, LINE_DRAW_BANK)
extern void b8DrawLine(unsigned short x1, unsigned char y1, unsigned short x2, unsigned char y2);

#ifdef TEST_PRIORITY_DRAW_LINES
extern void b8TestAsmPlotPriHLineFast();
#endif // TEST_PRIORITY_DRAW_LINES

#pragma wrapped-call (pop)

#define PIC_DEFAULT 15
#define PRI_DEFAULT 4

//#define VERBOSE
//#define VERBOSE_REL_DRAW
//#define VERBOSE_DRAW_LINE
//#define VERBOSE_X_CORNER
//#define VERBOSE_ABS_LINE
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

#define PICTURE_DATA_ZP (byte**) 0xFE; //This must not conflict with any from _b8DrawLine, that is why it is set so high. Must match DATA zp in _b8AsmFloodFillSections (fillAsm.s)

#ifdef TEST_LINE_DRAW

#pragma code-name (push, "BANKRAM08")
#pragma wrapped-call (push, trampoline, LINE_DRAW_BANK)
void b8TestDrawLine()
{
	int i = 0;
#define LOOP_AMOUNT 10000
	picDrawEnabled = TRUE;
	priDrawEnabled = FALSE;
	picColour = 5;
	b8DrawLine(159, 0, 159, 167);
	while (1) {}
}
#pragma wrapped-call (pop)
#pragma code-name (pop)

#endif // VERBOSE_LINE_DRAW


/* QUEUE DEFINITIONS */
#define QEMPTY 0xFF

#pragma rodata-name (push, "BANKRAM11")
const char B11_UNKNOWN_PIC[] = "Unknown picture code : %X\n";
const char B11_THE_BUFFER_STATUS[] = "";
const char B11_LOADED_PIC[] = "Loaded picture data %p, bank %d, size %d\n";
#pragma rodata-name (pop)

#pragma rodata-name (push, "BANKRAM04")
const char B4_CIRCLES[][15] = { /* agi circle bitmaps */
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

const byte B4_SPLATTER_MAP[32] = { /* splatter brush bitmaps */
  0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2,
  0x82, 0x09, 0x0a, 0x22, 0x12, 0x10, 0x42, 0x14,
  0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10,
  0x22, 0xa8, 0x14, 0x24, 0x00, 0x50, 0x24, 0x04
};

const byte B4_SPLATTER_START[128] = { /* starting bit position */
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
#pragma rodata-name (pop)

#pragma wrapped-call (push, trampoline, LINE_DRAW_BANK)
extern void b8DrawPixel(byte x, byte y);
extern byte b8AsmFloodFillSections(BufferStatus* bufferStatus, boolean* cleanPic);
extern byte b8AsmFloodFillSectionsVisOnly(BufferStatus* bufferStatus, boolean* cleanPic);
#pragma wrapped-call (pop)

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

#pragma code-name (pop)

#pragma code-name (push, "BANKRAM04")


#define plotPatternPoint() \
   if (patCode & 0x20) { \
      if ((B4_SPLATTER_MAP[bitPos>>3] >> (7-(bitPos&7))) & 1) b8DrawPixel(x1, y1); \
      bitPos++; \
      if (bitPos == 0xff) bitPos=0; \
   } else b8DrawPixel(x1, y1)

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
/**************************************************************************
** plotPattern
**
** Draws pixels, circles, squares, or splatter brush patterns depending
** on the pattern code.
**************************************************************************/
void b4PlotPattern(byte x, byte y)
{

	int circlePos = 0;
	byte x1, y1, penSize, bitPos = B4_SPLATTER_START[patNum];

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
				if ((B4_CIRCLES[patCode & 7][circlePos >> 3] >> (7 - (circlePos & 7))) & 1) {
					plotPatternPoint();
				}
				circlePos++;
			}
		}
	}

}
#pragma wrapped-call (pop)
#pragma code-name (pop)
#pragma code-name (push, "BANKRAM11")
int xCounter = 0;
extern void b8AsmFloodFill(uint8_t x, uint8_t y);
#pragma code-name (pop)
#pragma code-name (push, "BANKRAM11")
unsigned long lineDrawCounter = 0;

extern long pixelCounter;
extern long pixelStartPrintingAt;

#pragma wrapped-call (push, trampoline, PICTURE_CODE_OVERFLOW_BANK)
extern void b4DrawStraightLineAlongX(byte x1, byte x2, byte y);
extern void b4DrawStraightLineAlongY(byte x1, byte x2, byte y);
#pragma wrapped-call (pop)

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

	b8DrawPixel(x1, y1);

	for (;;) {
		GET_NEXT(x2);
		if (x2 >= 0xF0) return x2;


#ifdef VERBOSE_X_CORNER
		printf("x corner line 1: %d,%d : %d,%d. Address data %p\n", x1, y1, x2, y2, *data);
#endif

		b8DrawLine(x1, y1, x2, y1);
		x1 = x2;
		GET_NEXT(y2);
		if (y2 >= 0xF0) return y2;

#ifdef VERBOSE_X_CORNER
		printf("x corner line 2: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif
		b8DrawLine(x1, y1, x1, y2);
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

	b8DrawPixel(x1, y1);

	for (;;) {
		GET_NEXT(y2);
		if (y2 >= 0xF0) return y2;



#ifdef VERBOSE_Y_CORN
		printf("y corner line 1: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif

		b8DrawLine(x1, y1, x1, y2);
		y1 = y2;
		GET_NEXT(x2);
		if (x2 >= 0xF0) return x2;

#ifdef VERBOSE_Y_CORN
		printf("y Corner line 2: %d,%d : %d,%d\n", x1, y1, x2, y2);
#endif
		b8DrawLine(x1, y1, x2, y1);
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

	b8DrawPixel(x1, y1);

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

		b8DrawLine(x1, y1, x1 + dx, y1 + dy);
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

	b8DrawPixel(x1, y1);

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
		b8DrawLine(x1, y1, x2, y2);
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

#pragma wrapped-call (push, trampoline, GRAPHICS_BANK)
extern void b6Clear();
#pragma wrapped-call (pop)
/**************************************************************************
** drawPic
**
** Purpose: To draw an AGI picture from the given PICTURE resource data.
**
**  pLen = length of PICTURE data
**************************************************************************/
extern boolean disableIrq;
void b11DrawPic(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum)
{
	unsigned long i;
	byte action, returnedAction = 0;
	boolean stillDrawing = TRUE;
	PictureFile loadedPicture;
	byte** data;

	BufferStatus localBufferStatus;
	BufferStatus* bufferStatus = &localBufferStatus;
	boolean cleanPic = TRUE;

	picDrawEnabled = FALSE;
	priDrawEnabled = FALSE;

	data = PICTURE_DATA_ZP;
	*data = GOLDEN_RAM_WORK_AREA;
#ifdef TEST_LINE_DRAW
	b8TestDrawLine();
#endif

	getLoadedPicture(&loadedPicture, picNum);

#ifdef VERBOSE
	printf("Preparing To Draw %d of size %d\n", picNum, loadedPicture.size);
#endif // VERBOSE


	localBufferStatus.bank = loadedPicture.bank;
	localBufferStatus.bankedData = loadedPicture.data;
	localBufferStatus.bufferCounter = 0;

	b5RefreshBuffer(bufferStatus);

	if (okToClearScreen) {
		asm("sei");
		b6Clear();
	}
	asm("sei");

#ifdef TEST_PRIORITY_DRAW_LINES
	b8TestAsmPlotPriHLineFast();
#endif // b8TestAsmPlotPriHLineFast

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
		printfSafe("Action: %p \n", action);
		printfSafe("Work area address %p\n", GOLDEN_RAM_WORK_AREA);
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
			if (priDrawEnabled)
			{
				returnedAction = b8AsmFloodFillSections(bufferStatus, &cleanPic);
			}
			else
			{
				returnedAction = b8AsmFloodFillSectionsVisOnly(bufferStatus, &cleanPic);
			}
			break;
		case 0xF9: GET_NEXT(patCode); break;
		case 0xFA:
			if (picDrawEnabled)
			{
				cleanPic = FALSE;
			}
			returnedAction = b11PlotBrush(data, bufferStatus); break;
		default:
			printf(B11_UNKNOWN_PIC, action);
			printf(B11_THE_BUFFER_STATUS, bufferStatus->bank, bufferStatus->bankedData, bufferStatus->bufferCounter, picNum);
			printf(B11_LOADED_PIC, loadedPicture.data, loadedPicture.bank, loadedPicture.size);
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
		asm("sei");
		b3InitLayer1Mapbase();
		asm("cli");
	}
	showPicCalled = TRUE;
}

#pragma code-name (pop)