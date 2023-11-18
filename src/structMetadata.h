#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "view.h"
#include <stddef.h>

//This used for stored struct sizes of offsets which are used in assembler. That way when struct sizes/offsets change the assembler won't break. 

//Cel
byte sizeofCell = sizeof(Cel);

byte offsetOfBmp = offsetof(struct Cel, bmp);
byte offsetOfBmpBank = offsetof(struct Cel, bitmapBank);
byte offsetOfCelHeight = offsetof(struct Cel, height);
byte offsetOfCelTrans = offsetof(struct Cel, transparency);
#endif
