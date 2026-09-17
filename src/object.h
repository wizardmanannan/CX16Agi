/***************************************************************************
** object.h
***************************************************************************/

#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "memoryManager.h"
#include <cbm.h>
#include "agifiles.h"
#include "irq.h"
#include "textLayer.h"


#define MAX_OBJECTS 150
#define OBJ_NAME_CACHE_SIZE MAX_OBJECTS * 10
#define HAS_OBJ 255
#define INVENTORY_PALETTE_NUMBER 1
#define FIRST_OBJECT_ROW 1

typedef struct {
	byte roomNum;
	char* name;
} objectType;

extern objectType bDObjects[MAX_OBJECTS];
extern int bDNumObjects;

#pragma wrapped-call (push, trampoline, OBJECT_BANK)
void bDGetObject(byte objNum, objectType* objectType);
void bDSetObject(byte objNum, objectType* objectType);
void bDLoadObjectFile();
void bDDisplayInventory(boolean showObject);

#pragma wrapped-call (pop)

#endif /* _OBJECT_H_ */
