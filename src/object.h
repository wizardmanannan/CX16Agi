/***************************************************************************
** object.h
***************************************************************************/

#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "memoryManager.h"
#include <cbm.h>
#include "agifiles.h"


#define MAX_OBJECTS 255
#define OBJ_NAME_CACHE_SIZE MAX_OBJECTS * 10

typedef struct {
	byte roomNum;
	char* name;
} objectType;

extern objectType bDObjects[MAX_OBJECTS];
extern int bFNumObjects;

#pragma wrapped-call (push, trampoline, OBJECT_BANK)
void bDGetObject(byte objNum, objectType* objectType);
void bDSetObject(byte objNum, objectType* objectType);
void bDLoadObjectFile();
#pragma wrapped-call (pop)

#endif /* _OBJECT_H_ */
