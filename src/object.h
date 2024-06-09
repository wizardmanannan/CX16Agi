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

extern objectType bBObjects[MAX_OBJECTS];
extern int bFNumObjects;

#pragma wrapped-call (push, trampoline, OBJECT_BANK)
void bFGetObject(byte objNum, objectType* objectType);
void bFSetObject(byte objNum, objectType* objectType);
void bFLoadObjectFile();
#pragma wrapped-call (pop)

#endif /* _OBJECT_H_ */
