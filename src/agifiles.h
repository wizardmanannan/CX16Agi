/**************************************************************************
** AGIFILES.H
**************************************************************************/

#ifndef _AGIFILES_H_
#define _AGIFILES_H_

#include <string.h>
#include <cx16.h>
#include "memoryManager.h"
#include "helpers.h"

#define  LOGIC    0
#define  PICTURE  1
#define  VIEW     2
#define  SOUND    3

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

extern AGIFilePosType* logdir;
extern AGIFilePosType* picdir; 
extern AGIFilePosType* viewdir;
extern AGIFilePosType* snddir;

extern int numLogics, numPictures, numViews, numSounds;

void b6InitFiles();
void b6LoadAGIDirs();
void loadAGIFile(int resType, AGIFilePosType* location, AGIFile *AGIData);

#endif  /* _AGIFILES_H_ */
