#include "fill.h"
#pragma code-name (push, "BANKRAM08")
/*
    Fills current scanline and pushes adjacent scanlines onto the stack.
*/

void b8ScanAndFill(uint8_t x, uint8_t y)
{
    //static uint8_t lx, rx;

    //// Inline can_fill logic at the start to avoid unnecessary function calls
    //if (asm_can_fill(x, y) == false) {
    //    return;
    //}

    //lx = x;
    //rx = x;

    //// Inline can_fill logic for left expansion
    //while (lx != 0) {
    //    if (asm_can_fill(lx - 1, y) == false) {
    //        break;
    //    }
    //    --lx;
    //}

    //// Inline can_fill logic for right expansion
    //while (rx != 159) {
    //    if (asm_can_fill(rx + 1, y) == false) {
    //        break;
    //    }
    //    ++rx;
    //}

    //// pset_hline(lx, rx, y);
    //if (vis_enabled)
    //    asm_plot_vis_hline_fast((lx << 1), (rx << 1) + 2, y + STATUSBAR_OFFSET, vis_colour);
    //if (pri_enabled)
    //    asm_plot_pri_hline_fast((lx << 1), (rx << 1) + 2, y + STATUSBAR_OFFSET, pri_colour);

    //// if (y != 167) {
    ////     push(lx, rx, y + 1, 1); // push below
    //// }
    //// if (y != 0) {
    ////     push(lx, rx, y - 1, -1); // push above
    //// }
    //push(lx, rx, y + 1); // push below
    //push(lx, rx, y - 1); // push above
}

#pragma code-name (pop)