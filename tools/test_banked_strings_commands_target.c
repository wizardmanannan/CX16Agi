/* Test caller for get.num with its string in allocator-selected banked RAM. */
#include "helpers.h"
#include "logic.h"
#include "memoryManager.h"
#include <cx16.h>

extern byte *var;
extern int currentLog;

#pragma wrapped-call (push, trampoline, LOGIC_BANK)
extern void b5SetLogicEntry(LOGICEntry *logicEntry, byte logicFileNo);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, LRU_CACHE_LOGIC_BANK)
extern void b4Get_num(void);
#pragma wrapped-call (pop)

static LOGICFile testGetNumLogicFile;
static LOGICEntry testGetNumLogicEntry;
static byte *testGetNumMessages[1];

void testRunGetNum(void)
{
	byte previousBank = RAM_BANK;
	testGetNumMessages[0] = (byte *)0xA100;
	testGetNumLogicFile.numMessages = 1;
	testGetNumLogicFile.messages = testGetNumMessages;
	testGetNumLogicFile.messageBank = 0x24;
	testGetNumLogicEntry.loaded = TRUE;
	testGetNumLogicEntry.data = &testGetNumLogicFile;
	currentLog = 0;
	b5SetLogicEntry(&testGetNumLogicEntry, 0);
	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	RAM_BANK = previousBank;
	var = (byte *)0x0800;
	b4Get_num();
}
