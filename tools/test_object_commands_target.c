/* Compiled-C callers for the production object commands and object accessors. */
#include "helpers.h"
#include "object.h"
#include <cx16.h>

extern byte *var;
extern byte bDObjData[OBJ_NAME_CACHE_SIZE];
extern byte bDObjectNameLengths[MAX_OBJECTS];

int testObjectCount;
int testObjectObservedCount;
int testObjectCapacity = MAX_OBJECTS;
int testObjectNameCacheSize = OBJ_NAME_CACHE_SIZE;
int testObjectStride = sizeof(objectType);
byte testObjectVars[256];
byte testObjectIndex;
byte testObjectRoom;
word testObjectName;
byte testObjectSeedRoom;
word testObjectSeedName;
int testObjectFileLength;
byte testObjectFileLfn;
byte testObjectFileNullLength;
byte testObjectFileNullBuffer;
boolean testObjectResult;
boolean testObjectParseResult;
int testObjectParsedCount;
byte testObjectParsedRoom;
byte testObjectParsedObjectNum;
byte testObjectParsedNameLength;
word testObjectParsedName;
word testObjectParsedHeader;
byte testObjectObservedRoom;
word testObjectObservedName;

#pragma wrapped-call (push, trampoline, OBJECT_BANK)
extern byte bDLoadFile(int* fileLen, byte* buffer);
#pragma wrapped-call (pop)

void testObjectCommandsReadFile(void)
{
	byte previousBank = RAM_BANK;
	int* fileLen = testObjectFileNullLength ? (int*)0 : &testObjectFileLength;
	byte* buffer = testObjectFileNullBuffer ? (byte*)0 : bDObjData;
	RAM_BANK = OBJECT_BANK;
	testObjectFileLfn = bDLoadFile(fileLen, buffer);
	RAM_BANK = previousBank;
}

void testObjectCommandsLoadObjectFile(void)
{
	byte previousBank = RAM_BANK;
	bDLoadObjectFile();
	RAM_BANK = OBJECT_BANK;
	testObjectObservedCount = bDNumObjects;
	RAM_BANK = previousBank;
}

#pragma wrapped-call (push, trampoline, 0x0F)
extern boolean b1Has(void);
extern boolean b1Obj_in_room(void);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, 0x03)
extern void b3Get(void);
extern void b3Get_v(void);
extern void b3Drop(void);
extern void b3Put(void);
extern void b3Put_v(void);
extern void b3Get_room_v(void);
#pragma wrapped-call (pop)

void testObjectCommandsReset(void)
{
	byte previousBank = RAM_BANK;
	int index;

	memset(testObjectVars, 0, sizeof(testObjectVars));
	var = testObjectVars;
	RAM_BANK = OBJECT_BANK;
	bDNumObjects = testObjectCount;
	for (index = 0; index < MAX_OBJECTS; ++index)
	{
		bDObjects[index].roomNum = 0x66;
		bDObjects[index].name = (char *)0xBEEF;
	}
	for (index = 0; index < 8; ++index)
		bDObjData[index] = (byte)(0xD0 + index);
	RAM_BANK = previousBank;
}

void testObjectCommandsReadCount(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = OBJECT_BANK;
	testObjectObservedCount = bDNumObjects;
	RAM_BANK = previousBank;
}

void testObjectCommandsParseFile(void)
{
	byte previousBank = RAM_BANK;
	testObjectParseResult = bDParseObjectFileData(testObjectFileLength);
	RAM_BANK = OBJECT_BANK;
	testObjectParsedCount = bDNumObjects;
	testObjectParsedRoom = bDObjects[MAX_OBJECTS - 1].roomNum;
	testObjectParsedObjectNum = bDObjects[MAX_OBJECTS - 1].objectNum;
	testObjectParsedName = (word)bDObjects[MAX_OBJECTS - 1].name;
	testObjectParsedNameLength = bDObjectNameLengths[MAX_OBJECTS - 1];
	testObjectParsedHeader = bDObjData[0] | ((word)bDObjData[1] << 8);
	RAM_BANK = previousBank;
}

void testObjectCommandsRead(void)
{
	objectType object;
	object.roomNum = testObjectSeedRoom;
	object.name = (char *)testObjectSeedName;
	testObjectResult = bDGetObject(testObjectIndex, &object);
	testObjectObservedRoom = object.roomNum;
	testObjectObservedName = (word)object.name;
}

void testObjectCommandsWrite(void)
{
	objectType object;
	object.roomNum = testObjectRoom;
	object.name = (char *)testObjectName;
	testObjectResult = bDSetObject(testObjectIndex, &object);
}

void testObjectCommandsInspect(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = OBJECT_BANK;
	testObjectObservedRoom = bDObjects[testObjectIndex].roomNum;
	testObjectObservedName = (word)bDObjects[testObjectIndex].name;
	RAM_BANK = previousBank;
}

void testObjectCommandsRunB1Has(void)
{
	testObjectResult = b1Has();
}

void testObjectCommandsRunB1ObjInRoom(void)
{
	testObjectResult = b1Obj_in_room();
}

void testObjectCommandsRunB3Get(void)
{
	b3Get();
}

void testObjectCommandsRunB3GetVar(void)
{
	b3Get_v();
}

void testObjectCommandsRunB3Drop(void)
{
	b3Drop();
}

void testObjectCommandsRunB3Put(void)
{
	b3Put();
}

void testObjectCommandsRunB3PutVar(void)
{
	b3Put_v();
}

void testObjectCommandsRunB3GetRoomVar(void)
{
	b3Get_room_v();
}
