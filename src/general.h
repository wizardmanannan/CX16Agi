#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/**************************************************************************
** GENERAL.H
**
** Contains general definitions used by most C files.
**************************************************************************/

#ifndef _GENERAL_H_
#define _GENERAL_H_

#ifndef TRUE
#define  TRUE    1
#endif
#ifndef FALSE
#define  FALSE   0
#endif

/* MENU data */
#define MAX_MENU_SIZE 20
#define SEQUENTIAL_LFN 2

typedef unsigned char byte;
typedef unsigned short int word;
typedef char boolean;

typedef struct { 
	int w;
	int h;
	int v_w;
	int v_h;
	int color_depth;
	byte** line;
} BITMAP;

typedef struct {          /* DIR entry structure */
	byte fileNum;
	unsigned long filePos;
} AGIFilePosType;

extern byte callC1, callC2;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

typedef enum {
	PRINT_TIMEOUT = 21
} FLAGS;

#define VIEW_TABLE_SIZE  20  // Needs to be here and not in views to prevent circular depedencies
#define VERA_ADDRESS_SIZE 3

#define BANK_SIZE (0xBFFF-0xA000 + 1)
#define BANK_MAX 0xA000
#define BANK_MIN 0xBFFF

#endif  /* _GENERAL_H_ */
