/**************************************************************************
** picture.c
**
** Routines to load AGI picture resources, discard them, draw them and
** show the various screens.
**
** (c) Lance Ewing, 1997 - Modified (6 Jan 98)
**************************************************************************/

#include "picture.h"

#define PIC_DEFAULT 16
#define PRI_DEFAULT 4
//#define VERBOSE
//#define VERBOSE_REL_DRAW
//#define VERBOSE_PLOT

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

#define QMAX 10 //TODO:Fix
#define QEMPTY 0xFF

word buf[QMAX + 1];
word rpos = QMAX, spos = 0;

int* bitmapWidthPreMult = &BANK_RAM[BITMAP_WIDTH_PREMULT_START];

extern void b11PSet(byte x, byte y);

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

    loadedPictures[loadedPictureNumber]  = *loadedPicture;

    RAM_BANK = previousRamBank;
}

#pragma code-name (push, "BANKRAM11")
boolean b11Empty()
{
    return (rpos == spos);
}

void b11Qstore(word q)
{
    if (spos + 1 == rpos || (spos + 1 == QMAX && !rpos)) {
        nosound();
        return;
    }
    buf[spos] = q;
    spos++;
    if (spos == QMAX) spos = 0;  /* loop back */
}

word b11Qretrieve()
{
    if (rpos == QMAX) rpos = 0;  /* loop back */
    if (rpos == spos) {
        return QEMPTY;
    }
    rpos++;
    return buf[rpos - 1];
}

#define SWIDTH   640  /* Screen resolution */
#define SHEIGHT  480
#define PWIDTH   160  /* Picture resolution */
#define PHEIGHT  168
#define VWIDTH   640  /* Viewport size */
#define VHEIGHT  336

/**************************************************************************
** initPicture
**
** Purpose: To initialize allegro and create the picture and priority
** bitmaps. This function gets called once at the start of an AGI program.
**************************************************************************/
void b11InitPicture()
{
    int i;
    for (i = 0; i < PICTURE_HEIGHT; i++)
    {
        bitmapWidthPreMult[i] = i * BYTES_PER_ROW;
    }
}

/**************************************************************************
** initAGIScreen
**
** Purpose: Sets the screen mode to 640x480x256. This can be called a
** number of times during the program, e.g. when switching back from a
** text mode.
**************************************************************************/
void b11InitAGIScreen()
{
}

/**************************************************************************
** clearPicture
**
** Purpose: To clear the picture and priority bitmaps so that they are
** ready for drawing another PICTURE.
**************************************************************************/
void b11ClearPicture()
{
    trampoline_0(&b7ClearBackground, IRQ_BANK);
    clear_to_color(priority, PRI_DEFAULT);
    clear_to_color(control, PRI_DEFAULT);
}

/**************************************************************************
** priPSet
**
** Draws a pixel in the priority screen.
**************************************************************************/
void b11PriPSet(word x, word y)
{
    if (x > 159) return;
    if (y > 167) return;
    priority->line[y][x] = priColour;
}

/**************************************************************************
** pset
**
** Draws a pixel in each screen depending on whether drawing in that
** screen is enabled or not.
**************************************************************************/
#define PSET(x, y) \
    if (picDrawEnabled) { \
if ((x) <= 159 && (y) <= 167) {  \
             b11PSet(x, y); \
           } \
    } 

/**************************************************************************
** picGetPixel
**
** Get colour at x,y on the picture page.
**************************************************************************/
byte b11PicGetPixel(word x, word y)
{
    if (x > 159) return(PIC_DEFAULT);
    if (y > 167) return(PIC_DEFAULT);
    return (picture->line[y][x]);
}

/**************************************************************************
** priGetPixel
**
** Get colour at x,y on the priority page.
**************************************************************************/
byte b11PriGetPixel(word x, word y)
{
    if (x > 159) return(PRI_DEFAULT);
    if (y > 167) return(PRI_DEFAULT);
    return (priority->line[y][x]);
}

///**************************************************************************
//** round
//**
//** Rounds a float to the closest int. Takes into actions which direction
//** the current line is being drawn when it has a 50:50 decision about
//** where to put a pixel.
//**************************************************************************/
int b11Round(fix16 aNumber, fix16 dirn)
{
    fix16 pointFive = int_to_fix16(1 << (FIX16_SHIFT - 1)); // 0.5 in fixed point representation

    if (fix16_to_int_round_down(dirn) < 0)
        return fix16_to_int_round_down(aNumber - pointFive <= 0 ? aNumber : fix16_add(aNumber, int_to_fix16(1)));

    return fix16_to_int_round_down(aNumber - pointFive < 0 ? aNumber : fix16_add(aNumber, int_to_fix16(1)));
}

/**************************************************************************
** okToFill
**************************************************************************/
boolean b11OkToFill(byte x, byte y)
{
    if (!picDrawEnabled && !priDrawEnabled) return FALSE;
    if (picColour == PIC_DEFAULT) return FALSE;
    if (!priDrawEnabled) return (b11PicGetPixel(x, y) == PIC_DEFAULT);
    if (priDrawEnabled && !picDrawEnabled) return (b11PriGetPixel(x, y) == PRI_DEFAULT);
    return (b11PicGetPixel(x, y) == PIC_DEFAULT);
}

/**************************************************************************
** agiFill
**************************************************************************/
void b11AgiFill(word x, word y)
{
    byte x1, y1;
    rpos = spos = 0;

    return;

    b11Qstore(x);
    b11Qstore(y);

    for (;;) {

        x1 = b11Qretrieve();
        y1 = b11Qretrieve();

        if ((x1 == QEMPTY) || (y1 == QEMPTY))
            break;
        else {

            if (b11OkToFill(x1, y1)) {

                PSET(x1, y1);

                if (b11OkToFill(x1, y1 - 1) && (y1 != 0)) {
                    b11Qstore(x1);
                    b11Qstore(y1 - 1);
                }
                if (b11OkToFill(x1 - 1, y1) && (x1 != 0)) {
                    b11Qstore(x1 - 1);
                    b11Qstore(y1);
                }
                if (b11OkToFill(x1 + 1, y1) && (x1 != 159)) {
                    b11Qstore(x1 + 1);
                    b11Qstore(y1);
                }
                if (b11OkToFill(x1, y1 + 1) && (y1 != 167)) {
                    b11Qstore(x1);
                    b11Qstore(y1 + 1);
                }

            }

        }

    }

}

/**************************************************************************
** xCorner
**
** Draws an xCorner  (drawing action 0xF5)
**************************************************************************/
void b11XCorner(byte** data)
{
    byte x1, x2, y1, y2;

    x1 = *((*data)++);
    y1 = *((*data)++);

    PSET(x1, y1);

    for (;;) {
        x2 = *((*data)++);
        if (x2 >= 0xF0) break;

        bresenham_x1 = x1;
        bresenham_x2 = x2;
        bresenham_y1 = y1;
        bresenham_y2 = y2;
        
#ifdef VERBOSE
        printf("x corner line 1: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif
        b11Drawline();
        x1 = x2;
        y2 = *((*data)++);
        if (y2 >= 0xF0) break;

        bresenham_x1 = x1;
        bresenham_x2 = x2;
        bresenham_y1 = y1;
        bresenham_y2 = y2;

#ifdef VERBOSE
        printf("x corner line 2: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif
        b11Drawline();
        y1 = y2;
    }

    (*data)--;
}

/**************************************************************************
** yCorner
**
** Draws an yCorner  (drawing action 0xF4)
**************************************************************************/
void b11YCorner(byte** data)
{
    byte x1, x2, y1, y2;

    x1 = *((*data)++);
    y1 = *((*data)++);

    PSET(x1, y1);

    for (;;) {
        y2 = *((*data)++);
        if (y2 >= 0xF0) break;


        bresenham_x1 = x1;
        bresenham_x2 = x2;
        bresenham_y1 = y1;
        bresenham_y2 = y2;
        
#ifdef VERBOSE
        printf("y corner line 1: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif
        
        b11Drawline();
        y1 = y2;
        x2 = *((*data)++);
        if (x2 >= 0xF0) break;


        bresenham_x1 = x1;
        bresenham_x2 = x2;
        bresenham_y1 = y1;
        bresenham_y2 = y2;

#ifdef VERBOSE
        printf("y Corner line 2: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif
        b11Drawline();
        x1 = x2;
    }

    (*data)--;
}

/**************************************************************************
** relativeDraw
**
** Draws short lines relative to last position.  (drawing action 0xF7)
**************************************************************************/
void b11RelativeDraw(byte** data)
{
    byte x1, y1, disp;
    signed char dx, dy;

    x1 = *((*data)++);
    y1 = *((*data)++);

    PSET(x1, y1);

    for (;;) {
        disp = *((*data)++);

#ifdef VERBOSE_REL_DRAW
        printf("Disp %u \n", disp);
#endif // VERBOSE

        if (disp >= 0xF0) break;
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


        bresenham_x1 = x1;
        bresenham_x2 = x1 + dx;
        bresenham_y1 = y1;
        bresenham_y2 = y1 + dy;

#ifdef VERBOSE
        printf("rel line: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif

        b11Drawline();
        x1 += dx;
        y1 += dy;
    }

    (*data)--;
}

/**************************************************************************
** fill
**
** Agi flood fill.  (drawing action 0xF8)
**************************************************************************/
void b11Fill(byte** data)
{
    byte x1, y1;

    for (;;) {
        if ((x1 = *((*data)++)) >= 0xF0) break;
        if ((y1 = *((*data)++)) >= 0xF0) break;
        b11AgiFill(x1, y1);
    }

    (*data)--;
}

/**************************************************************************
** absoluteLine
**
** Draws long lines to actual locations (cf. relative) (drawing action 0xF6)
**************************************************************************/
void b11AbsoluteLine(byte** data)
{
    byte x1, y1, x2, y2;

    x1 = *((*data)++);
    y1 = *((*data)++);

    PSET(x1, y1);

    for (;;) {
        if ((x2 = *((*data)++)) >= 0xF0)
        {
//#ifdef VERBOSE
//            printf("Absolute Line Break\n");
//#endif // VERBOSE

            break;
        }
        if ((y2 = *((*data)++)) >= 0xF0)
        {
//#ifdef VERBOSE
//            printf("Absolute Line Break\n");
//#endif // VERBOSE
            break;
        }
        bresenham_x1 = x1;
        bresenham_x2 = x2;
        bresenham_y1 = y1;
        bresenham_y2 = y2;
#ifdef VERBOSE
        printf("abs line: %d,%d : %d,%d\n", bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2);
#endif
        b11Drawline();
        x1 = x2;
        y1 = y2;
    }

    (*data)--;
}


#define plotPatternPoint() \
   if (patCode & 0x20) { \
      if ((splatterMap[bitPos>>3] >> (7-(bitPos&7))) & 1) PSET(x1, y1); \
      bitPos++; \
      if (bitPos == 0xff) bitPos=0; \
   } else PSET(x1, y1)

/**************************************************************************
** plotPattern
**
** Draws pixels, circles, squares, or splatter brush patterns depending
** on the pattern code.
**************************************************************************/
void b11PlotPattern(byte x, byte y)
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

    //TODO: Fix Float
    //for (y1 = y - penSize; y1 <= y + penSize; y1++) {
    //    for (x1 = x - (ceil((float)penSize / 2)); x1 <= x + (floor((float)penSize / 2)); x1++) {
    //        if (patCode & 0x10) { /* Square */
    //            plotPatternPoint();
    //        }
    //        else { /* Circle */
    //            if ((circles[patCode & 7][circlePos >> 3] >> (7 - (circlePos & 7))) & 1) {
    //                plotPatternPoint();
    //            }
    //            circlePos++;
    //        }
    //    }
    //}

}


/**************************************************************************
** plotBrush
**
** Plots points and various brush patterns.
**************************************************************************/
void b11PlotBrush(byte** data)
{
    byte x1, y1, store;

    for (;;) {
        if (patCode & 0x20) {
            if ((patNum = *((*data)++)) >= 0xF0) break;
            patNum = ((patNum >> 1) & 0x7f);
        }
        if ((x1 = *((*data)++)) >= 0xF0) break;
        if ((y1 = *((*data)++)) >= 0xF0) break;
        b11PlotPattern(x1, y1);
    }

    (*data)--;
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

    for (x = 0; x < 160; x++) {
        for (y = 0; y < 168; y++) {
            data = priority->line[y][x];
            if (data == 3) {
                priority->line[y][x] = 4;
                control->line[y][x] = data;
            }
            if (data < 3) {
                control->line[y][x] = data;
                dy = y + 1;
                priFound = FALSE;
                while (!priFound && (dy < 168)) {
                    data = priority->line[dy][x];
                    /* The following if statement is a hack to fix a problem
                    ** in KQ1 room 1.
                    */
                    if (data == 3) {
                        priFound = TRUE;
                        priority->line[y][x] = 4;
                    }
                    else if (data > 3) {
                        priFound = TRUE;
                        priority->line[y][x] = data;
                    }
                    else
                        dy++;
                }
            }
        }
    }
}

int picFNum = 0;

/**************************************************************************
** drawPic
**
** Purpose: To draw an AGI picture from the given PICTURE resource data.
**
**  pLen = length of PICTURE data
**************************************************************************/
void b11DrawPic(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum)
{
    byte action;
    boolean stillDrawing = TRUE;
    PictureFile loadedPicture;
    byte* data;
    
    int** zpPtrTemp = (int**) ZP_PTR_TEMP;
    *zpPtrTemp = &bitmapWidthPreMult[0];

    getLoadedPicture(&loadedPicture, picNum);

#ifdef VERBOSE
    printf("Preparing To Draw %d of size %d\n", picNum, loadedPicture.size);
#endif // VERBOSE


    data = (byte*)malloc(loadedPicture.size);

    if (!data)
    {
        printf("Out of memory in picture code");
    }

    memCpyBanked(&data[0], (byte*)loadedPicture.data, loadedPicture.bank, loadedPicture.size);
    
    //asm("sei");

    if (okToClearScreen) b11ClearPicture();

    //asm("cli");

    //trampoline_0(&b7DisableAndWaitForVsync, IRQ_BANK);

    //asm("sei");

    patCode = 0x00;
    
#ifdef VERBOSE
    printf("Plotting. . .\n");
#endif // VERBOSE

    do {
        action = *(data++);
#ifdef VERBOSE
        printf("Action: %p \n", action);
#endif // VERBOSE
        switch (action) {
        case 0xFF: 
            stillDrawing = 0; 
            break;
        case 0xF0: picColour = *(data++);
            picDrawEnabled = TRUE;
            break;
        case 0xF1: picDrawEnabled = FALSE; break;
        case 0xF2: priColour = *(data++);
            priDrawEnabled = TRUE;
            break;
        case 0xF3: priDrawEnabled = FALSE; break;
        case 0xF4: b11YCorner(&data); break;
        case 0xF5: b11XCorner(&data); break;
        case 0xF6: b11AbsoluteLine(&data); break;
        case 0xF7: b11RelativeDraw(&data); break;
        case 0xF8: b11Fill(&data); break;
        case 0xF9: patCode = *(data++); break;
        case 0xFA: b11PlotBrush(&data); break;
        default: printf("Unknown picture code : %X\n", action); exit(0);
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

    //asm("cli");

    //trampoline_0(&b7DisableAndWaitForVsync, IRQ_BANK);

    free(data);
}

void b11InitPictures()
{
    int i;
    PictureFile loadedPicture;
    for (i = 0; i < 256; i++) {
        getLoadedPicture(&loadedPicture, i);
        loadedPicture.loaded = FALSE;
        setLoadedPicture(&loadedPicture, i);
    }
}

/***************************************************************************
** loadPictureFile
**
** Purpose: To load the picture resource with the given number.
***************************************************************************/
void b11LoadPictureFile(int picFileNum)
{
    AGIFile tempAGI;
    AGIFilePosType agiFilePosType;
    PictureFile loadedPicture;

    getLoadedPicture(&loadedPictures, picFileNum);

    getLogicDirectory(&agiFilePosType, &picdir[picFileNum]);

#ifdef VERBOSE
    printf("The address of picdir is %p\n", &picdir[picFileNum]);
    printf("The picture number is %d \n", picFileNum);
    printf("Loading Picture file %d, position %d\n", agiFilePosType.fileNum, agiFilePosType.filePos);
#endif

    loadAGIFileTrampoline(PICTURE, &agiFilePosType, &tempAGI);
    
    loadedPicture.size = tempAGI.totalSize;
    loadedPicture.data = tempAGI.code;
    loadedPicture.bank = tempAGI.codeBank;
    loadedPicture.loaded = TRUE;

    setLoadedPicture(&loadedPicture, picFileNum);

#ifdef VERBOSE
    printf("Loaded Picture %d, data %p, bank %d, loaded %d\n", loadedPicture.size, loadedPicture.data, loadedPicture.bank, loadedPicture.loaded);
#endif // VERBOSE
}

void b11DiscardPictureFile(int picFileNum)
{
    PictureFile loadedPicture;

    getLoadedPicture(&loadedPicture, picFileNum);

    if (loadedPicture.loaded) {
        loadedPicture.loaded = FALSE;
        banked_deallocTrampoline(loadedPicture.data, loadedPicture.bank);
    }

    setLoadedPicture(&loadedPicture, picFileNum);
}

void b11ShowPicture()
{
    //Doesn't need to do much since picture is stored straight in VRAM. Need to investigate whether we need to do this
}

#pragma code-name (pop)

void drawPicTrampoline(byte* bankedData, int pLen, boolean okToClearScreen, byte picNum)
{
    byte previousBank = RAM_BANK;
    RAM_BANK = PICTURE_CODE_BANK;
    
    b11DrawPic(bankedData, pLen, okToClearScreen, picNum);

    RAM_BANK = previousBank;
}

