/**************************************************************************
** GENERAL.H
**
** Contains general definitions used by most C files.
**************************************************************************/

#ifndef _GENERAL_H_
#define _GENERAL_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define C_STACK 0x22

#ifndef TRUE
#define  TRUE    1
#endif
#ifndef FALSE
#define  FALSE   0
#endif

#define FILE_DEVICE 8
#define FILE_OPEN_ADDRESS 2

/* MENU data */
#define SEQUENTIAL_LFN 2

#define GAMEID_MAX_LENGTH 6

extern char gameId[GAMEID_MAX_LENGTH + 1];

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

#define VERA_ADDRESS_SIZE 3

#define JIFFY_CALL_FREQ 16 

extern boolean* flag;

extern int currentLog;

#define DEFAULT_PRIORITY_BASE 48

#endif  /* _GENERAL_H_ */
