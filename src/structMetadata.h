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
byte offsetOfCurrentView = offsetof(struct ViewTable, currentView);
byte offsetOfCurrentLoop = offsetof(struct ViewTable, currentLoop);
byte offsetOfFlags = offsetof(struct ViewTable, flags);
byte offsetOfDirection = offsetof(struct ViewTable, direction);
byte offsetOfParam1 = offsetof(struct ViewTable, param1);
byte offsetOfParam2 = offsetof(struct ViewTable, param2);
byte offsetOfMotion = offsetof(struct ViewTable, motion);

//View Metadata
byte offsetOfloopsVeraAddressesPointers = offsetof(struct ViewTableMetadata, loopsVeraAddressesPointers);
byte offsetOfViewMetadataBank = offsetof(struct ViewTableMetadata, viewTableMetadataBank);
byte offsetOfBackBuffers = offsetof(struct ViewTableMetadata, backBuffers);
byte offsetOfIsOnBackBuffer = offsetof(struct ViewTableMetadata, isOnBackBuffer);
//View
byte offsetOfNumberOfLoops = offsetof(struct View, numberOfLoops);
byte offsetOfMaxCels = offsetof(struct View, maxCels);
byte offsetOfMaxVeraSlots = offsetof(struct View, maxVeraSlots);
byte sizeOfView = sizeof(View);

#endif
