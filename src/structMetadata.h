#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "view.h"
#include <stddef.h>

//This used for stored struct sizes of offsets which are used in assembler. That way when struct sizes/offsets change the assembler won't break. 

//Cel
byte sizeofCel = sizeof(Cel);

byte offsetOfBmp = offsetof(struct Cel, bmp);
byte offsetOfBmpBank = offsetof(struct Cel, bitmapBank);
byte offsetOfCelTrans = offsetof(struct Cel, transparency);
byte offsetOfCelWidth = offsetof(struct Cel, width);
byte offsetOfCelHeight = offsetof(struct Cel, height);
byte offsetOfSplitCelPointers = offsetof(struct Cel, splitCelPointers);
byte offsetOfSplitCelBank = offsetof(struct Cel, splitCelBank);
byte offsetOfSplitSegments = offsetof(struct Cel, splitSegments);
byte offsetOfFlipped = offsetof(struct Cel, flipped);

//ViewTab
byte offsetOfXPos = offsetof(struct ViewTable, xPos);
byte offsetOfYPos = offsetof(struct ViewTable, yPos);
byte offsetOfPriority = offsetof(struct ViewTable, priority);
#endif
