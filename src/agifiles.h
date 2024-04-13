/**************************************************************************
** AGIFILES.H
**************************************************************************/

#ifndef _AGIFILES_H_
#define _AGIFILES_H_

#include "memoryManager.h"
#include "helpers.h"

#ifdef  __CX16__
#include <cx16.h>
#include "helpers.h"
#endif

#define  LOGIC    0
#define  PICTURE  1
#define  VIEW     2
#define  SOUND    3

#define NO_RESOURCES 255

#define  EMPTY  0xFFFFF   /* Empty DIR entry */

typedef struct {          /* AGI data file structure */
   unsigned int totalSize;
   unsigned int codeSize;
   byte* code;
   byte codeBank;
   byte messageBank;
   unsigned int noMessages;
   byte** messagePointers;
   byte* messageData;
} AGIFile;

extern AGIFilePosType logdir[NO_RESOURCES];
extern AGIFilePosType picdir[NO_RESOURCES];
extern AGIFilePosType viewdir[NO_RESOURCES];
extern AGIFilePosType snddir[NO_RESOURCES];

extern int numLogics, numPictures, numViews, numSounds;

#pragma wrapped-call (push, trampoline, FILE_LOADER_HELPERS)
void b6InitFiles();
void b6LoadAGIDirs();
void b6LoadAGIFile(int resType, AGIFilePosType* location, AGIFile *AGIData);
#pragma wrapped-call (pop)

#endif  /* _AGIFILES_H_ */
