#include "fill.h"
#pragma code-name (push, "BANKRAM08")
/*
    Fills current scanline and pushes adjacent scanlines onto the stack.
*/
extern boolean picDrawEnabled, priDrawEnabled, picColour, priColour;
extern boolean b8AsmCanFill(uint8_t x, uint8_t y);

extern void b8AsmPlotVisHLineFast(unsigned short x0, unsigned short x1, unsigned char y, unsigned char color);

boolean enableStop = FALSE;
int drawCounter = 0;

//#define VERBOSE_FILL

void b8ScanAndFill(uint8_t x, uint8_t y)
{
    static uint8_t lx, rx;

    //printf("in. trying to fill %d, %d\n", x,y);

    // Inline can_fill logic at the start to avoid unnecessary function calls
    if (b8AsmCanFill(x, y) == false) {
#ifdef VERBOSE_FILL
        printf("blocked on %d %d\n", x, y);
#endif // VERBOSE_FILL
        return;
    }

#ifdef VERBOSE_FILL
    printf("can fill true %d %d\n", x, y);
#endif

    lx = x;
    rx = x;

    //printf("at 1\n");

    // Inline can_fill logic for left expansion
    while (lx != 0) {
        if (b8AsmCanFill(lx - 1, y) == false) {
            break;
        }
        --lx;
    }

    //printf("at 2\n");

    // Inline can_fill logic for right expansion
    while (rx != 159) {
        if (b8AsmCanFill(rx + 1, y) == false) {
            
#ifdef VERBOSE_FILL
            printf("stopping at %d\n", rx);
#endif
            break;
        }
        ++rx;
        //printf("l2 rx %d\n", rx);
    }

    //printf("at 3. x0 %d x1 %d y %d color %d\n", lx, rx + 1, y, picColour);

    // pset_hline(lx, rx, y);
    if (picDrawEnabled)
#ifdef VERBOSE_FILL
        printf("%d drawing a line %d, %d to %d %d\n",drawCounter++, lx, y, rx, y);
#endif
        
        if (drawCounter == 84)
        {
            enableStop = TRUE;
        }
    
        b8AsmPlotVisHLineFast(lx,  rx, y, picColour);
        enableStop = FALSE;

    //printf("at 4\n");

   /* if (priDrawEnabled)
        asm_plot_pri_hline_fast((lx << 1), (rx << 1) + 2, y + STARTING_BYTE, priColour);*/

    // if (y != 167) {
    //     push(lx, rx, y + 1, 1); // push below
    // }
    // if (y != 0) {
    //     push(lx, rx, y - 1, -1); // push above
    // }

        if (y < PICTURE_HEIGHT)
        {
            b8Push(lx, rx, y + 1); // push below
        }

        if (y > 0)
        {
            b8Push(lx, rx, y - 1); // push above
        }
}

#pragma code-name (pop)