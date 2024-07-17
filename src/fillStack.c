#include "fillstack.h"

#pragma code-name (push, "BANKRAM08")

#pragma bss-name (push, "BANKRAM08")
uint8_t b8FillStackPointer;
static Span fill_stack[MAX_STACK_SIZE];
#pragma bss-name (pop)

/*
    Push a span onto the stack
*/
void b8Push(uint8_t lx, uint8_t rx, uint8_t y)
{
    printf("called push %d %d %d\n", lx, rx, y);
    if (b8FillStackPointer < MAX_STACK_SIZE) {
    fill_stack[b8FillStackPointer].lx = lx;
    fill_stack[b8FillStackPointer].rx = rx;
    fill_stack[b8FillStackPointer].y = y;
    }
    else
    {
        printf("overflow at %d \n", b8FillStackPointer);
    }
    ++b8FillStackPointer;
}

/*
    Pop a span from the stack
*/
bool b8Pop(uint8_t* lx, uint8_t* rx, uint8_t* y) {
    printf("called pop %d %d %d\n", *lx, *rx, *y);
    if (b8FillStackPointer > 0) {
        Span* item = &fill_stack[--b8FillStackPointer];
        *lx = item->lx;
        *rx = item->rx;
        *y = item->y;
        return true;
    }
    return false;
}

#pragma code-name (pop)