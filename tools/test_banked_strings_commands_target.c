/* Test caller for get.num with its string in allocator-selected banked RAM. */
#include "helpers.h"
#include "logic.h"
#include "memoryManager.h"
#include "paletteManager.h"
#include <cx16.h>

extern byte *var;
extern int currentLog;

#pragma wrapped-call (push, trampoline, LOGIC_BANK)
extern void b5SetLogicEntry(LOGICEntry *logicEntry, byte logicFileNo);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, LRU_CACHE_LOGIC_BANK)
extern void b4Get_num(void);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, TEXT_CODE_BANK)
extern void b3Display(void);
extern void b3Display_v(void);
#pragma wrapped-call (pop)

static LOGICFile testGetNumLogicFile;
static LOGICEntry testGetNumLogicEntry;
static byte *testGetNumMessages[1];

static LOGICFile testStartupLogicFile;
static LOGICEntry testStartupLogicEntry;
static byte *testStartupMessages[25];

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

void testPrepareKq1StartupDisplay(void)
{
	byte messageNo;

	for (messageNo = 0; messageNo < 25; ++messageNo)
		testStartupMessages[messageNo] = (byte *)(0xA100 + (unsigned int)messageNo * 0x40);
	testStartupLogicFile.codeSize = 0;
	testStartupLogicFile.logicCode = NULL;
	testStartupLogicFile.numMessages = 25;
	testStartupLogicFile.messages = testStartupMessages;
	testStartupLogicFile.codeBank = 0;
	testStartupLogicFile.messageBank = 0x24;
	testStartupLogicEntry.loaded = TRUE;
	testStartupLogicEntry.data = &testStartupLogicFile;
	testStartupLogicEntry.dataBank = 0;
	testStartupLogicEntry.isLogicZeroOrDependency = FALSE;
	currentLog = 0;
	bFInitPaletteManager();
	b5SetLogicEntry(&testStartupLogicEntry, 0);
	var = (byte *)0x0800;
	b3Display();
	b3Display();
}

void testRunKq1StartupFirstCredit(void)
{
	var[1] = 18;
	var[2] = 12;
	var[3] = 1;
	b3Display_v();
}

void testRunKq1StartupRemainingCredits(void)
{
	byte cycle, messageNo;

	for (cycle = 2; cycle <= 4; ++cycle)
	{
		for (messageNo = cycle; messageNo > 0; --messageNo)
		{
			var[1] = (byte)(18 - (cycle - messageNo));
			var[2] = 12;
			var[3] = messageNo;
			b3Display_v();
		}
	}
}

void testPrepareKq1StartupLongDisplay(void)
{
	testStartupMessages[0] = (byte *)0xA100;
	testStartupLogicFile.codeSize = 0;
	testStartupLogicFile.logicCode = NULL;
	testStartupLogicFile.numMessages = 1;
	testStartupLogicFile.messages = testStartupMessages;
	testStartupLogicFile.messageBank = 0x24;
	testStartupLogicFile.codeBank = 0;
	testStartupLogicEntry.loaded = TRUE;
	testStartupLogicEntry.data = &testStartupLogicFile;
	testStartupLogicEntry.dataBank = 0;
	testStartupLogicEntry.isLogicZeroOrDependency = FALSE;
	currentLog = 0;
	bFInitPaletteManager();
	b5SetLogicEntry(&testStartupLogicEntry, 0);
	var = (byte *)0x0800;
}

void testRunKq1StartupLongCredit(void)
{
	var[1] = 0;
	var[2] = 0;
	var[3] = 1;
	b3Display_v();
}
