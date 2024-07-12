#include "fillstack.h"

#pragma code-name (push, "BANKRAM08")

#pragma bss-name (push, "BANKRAM08")
static Span fill_stack[MAX_STACK_SIZE];
uint8_t fill_stack_pointer = 0;
#pragma bss-name (pop)

/*
    Push a span onto the stack
*/
void push(uint8_t lx, uint8_t rx, uint8_t y)
{
    //if (fill_stack_pointer < MAX_STACK_SIZE) {
    fill_stack[fill_stack_pointer].lx = lx;
    fill_stack[fill_stack_pointer].rx = rx;
    fill_stack[fill_stack_pointer].y = y;
    ++fill_stack_pointer;
    //}
}

/*
    Pop a span from the stack
*/
bool pop(uint8_t* lx, uint8_t* rx, uint8_t* y) {
    if (fill_stack_pointer > 0) {
        Span* item = &fill_stack[--fill_stack_pointer];
        *lx = item->lx;
        *rx = item->rx;
        *y = item->y;
        return true;
    }
    return false;
}

#pragma code-name (pop)