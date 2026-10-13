/* Test caller for compiled production banked-string helpers. */
#include "helpers.h"
#include "logic.h"
#include <cx16.h>

extern int currentLog;

#pragma wrapped-call (push, trampoline, LOGIC_BANK)
extern void b5SetLogicEntry(LOGICEntry *logicEntry, byte logicFileNo);
extern void b5Set_game_id(void);
#pragma wrapped-call (pop)

char *testBankedStringSource;
byte testBankedStringBank;
unsigned int testBankedStringByteLimit;
size_t testBankedStringLength;
boolean testBankedStringTerminated;
char *testGameIdMessage;
byte testGameIdMessageBank;
static LOGICFile testGetNumLogicFile;
static LOGICEntry testGetNumLogicEntry;
static byte *testGetNumMessages[1];

void testRunBankedString(void)
{
	testBankedStringLength = strLenBankedBounded(testBankedStringSource,
		testBankedStringBank, testBankedStringByteLimit, &testBankedStringTerminated);
}

void testRunSetGameId(void)
{
	testGetNumMessages[0] = (byte *)testGameIdMessage;
	testGetNumLogicFile.numMessages = 1;
	testGetNumLogicFile.messages = testGetNumMessages;
	testGetNumLogicFile.messageBank = testGameIdMessageBank;
	testGetNumLogicEntry.loaded = TRUE;
	testGetNumLogicEntry.data = &testGetNumLogicFile;
	currentLog = 0;
	b5SetLogicEntry(&testGetNumLogicEntry, 0);
	b5Set_game_id();
}
