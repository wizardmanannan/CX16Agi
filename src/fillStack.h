#ifndef FILLSTACK_H
#define FILLSTACK_H
/*
    Custom stack data structure for the flood fill algorithm.
*/
#include <stdint.h>
#include <stdbool.h>

// After testing, the highest required I have found has been about 160
#define MAX_STACK_SIZE 160

// Span structure
typedef struct {
    uint8_t lx, rx, y;
} Span;

extern uint8_t fill_stack_pointer; // also used by pixel.c

void push(uint8_t lx, uint8_t rx, uint8_t y);
bool pop(uint8_t* lx, uint8_t* rx, uint8_t* y);

#endif // FILLSTACK_H