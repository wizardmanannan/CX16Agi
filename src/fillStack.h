#ifndef FILLSTACK_H
#define FILLSTACK_H
/*
    Custom stack data structure for the flood fill algorithm.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// After testing, the highest required I have found has been about 160
#define MAX_STACK_SIZE 255

// Span structure
typedef struct {
    uint8_t lx, rx, y;
} Span;

extern uint8_t b8FillStackPointer; // also used by pixel.c
extern Span fill_stack[MAX_STACK_SIZE];

void b8Push(uint8_t lx, uint8_t rx, uint8_t y);
bool b8Pop(uint8_t* lx, uint8_t* rx, uint8_t* y);

#endif // FILLSTACK_H