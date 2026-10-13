/* Compiled-C callers for the production object commands and object accessors. */
#include "helpers.h"
#include "object.h"
#include <cx16.h>

extern byte *var;
extern byte bDObjData[OBJ_NAME_CACHE_SIZE];

int testObjectCount;
int testObjectObservedCount;
byte testObjectVars[256];
byte testObjectIndex;
byte testObjectRoom;
word testObjectName;
byte testObjectSeedRoom;
word testObjectSeedName;
boolean testObjectResult;
byte testObjectObservedRoom;
word testObjectObservedName;

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
	bFNumObjects = testObjectCount;
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
	testObjectObservedCount = bFNumObjects;
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
