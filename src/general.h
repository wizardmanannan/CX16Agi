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

//KernalFunctions

#define SCREEN_SET_CHAR_SET 0xFF62
//Args
#define ISO 1

#endif  /* _GENERAL_H_ */
