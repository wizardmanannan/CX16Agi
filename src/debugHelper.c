#pragma once
#include "debugHelper.h"
#define PRINT_ALL_SCRIPTS 256

extern boolean* flag;
extern byte* var;
extern byte codeBank;
extern byte newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern int currentLog;

byte debugBank;

#pragma bss-name (push, "BANKRAMDEBUG")
long opCounter = 1;
long opStopAt = -1;
long opExitAt = -1;
long opStartPrintingAt = 0;
int opPrintOnlyOnScript = PRINT_ALL_SCRIPTS;
boolean opStopEvery = FALSE;
int _clockBefore = 0;

long pixelCounter = 1;
long pixelStartPrintingAt = 0;
long pixelStopAt = -1;
long pixelFreezeAt = -1;

unsigned long queueAction = 0;

#pragma bss-name (pop)

#pragma rodata-name (push, "BANKRAMDEBUG")
const char bDbgPrintMessage[] = "%d. F0 is %d\n";
const char bDbgRemainingMemoryMessage[] = "Your remaining memory is approx: %d \n";
const char bDbgFalseResultMessage[] = "the result is false\n";
const char bDbgTrueResultMessage[] = "the result is true\n";
const char bDbgInvertedResultMessage[] = "the result is inverted by not\n";
const char bDbgIsSetMessage[] = "checking that %d is set and it %d\n";
const char bDbgGreaterThan_8N_Message[] = "checking that %d (%d) is > %d and the result should be %d\n";
const char bDbgLessThan_8N_Message[] = "checking that %d is < %d and the result should be %d\n";
const char bDbgGreaterThan_8V_Message[] = "checking that %d (%d) is > %d (%d) and the result should be %d\n";
const char bDbgLessThan_8V_Message[] = "checking that %d (%d) is < %d (%d) and the result should be %d\n";
const char bDbgEqualN_Message[] = "checking that %d (%d) is equal to %d and it %d\n";
const char bDbgEqualV_Message[] = "checking that %d (%d) is equal to %d (%d) and it %d\n";
const char bDbgIncrementingMessage[] = "incrementing var %d(%d) to %d\n";
const char bDbgDecrementingMessage[] = "decrementing var %d to %d\n";
const char bDbgAddN_Message[] = "add var %d (%d) to %d";
const char bDbgAddV_Message[] = "add var %d (%d) to %d\n";
const char bDbgSubN_Message[] = "sub var %d (%d) to %d which is %d\n";
const char bDbgSubV_Message[] = "sub var %d (%d) to %d (%d) which is %d\n";
const char bDbgAssignN_Message[] = "assign var %d (%d) to %d\n";
const char bDbgAssignV_Message[] = "assign var %d (%d) to %d (%d)\n";
const char bDbgIndirectMessage[] = "indir %d (%d) value %d\n";
const char bDbgScanStartMessage[] = "lognum: %d scan start: zp_code ((%p) + cwCurrentcode (%p)) - startpos (%p) = %p. actual %p";
const char bDbgResetScanStartMessage[] = "resetting scan for logic %d. it %s reset.\n";
const char bDbgIndirectV_Message[] = "indir V %d (%d) value %d (%d)\n";
const char bDbgPostCheckVarMessage[] = "post check var %d (%d)\n";
const char bDbgPostCheckFlagMessage[] = "post check flag %d (%d)\n";
const char bDbgCodeJumpMessage[] = "b1 is %d b2 is %d, shift %u and the jump result is %u.\n";
const char bDbgNewRoomMessage[] = "attempting to enter new room %d\n";
const char bDbgExitAllLogicsMessage[] = "----------Exit Debug: Attempting to enter new room %d. Has entered new Room: %d. Has exited all logics %d\n";
const char bDbgScriptStartMessage[] = "ex s. %d counter op %lu, var 0 is %d\n";
const char bDbgRoomChangeMessage[] = "We are at %d, counter %lu\n";
#pragma rodata-name (pop)

#pragma rodata-name (push, "BANKRAM05")
const char b5InitializingDebug[] = "debug now initializing on bank %d\n";
#pragma rodata-name (pop)

#pragma rodata-name (push, "BANKRAM06")
char b6DebugFileName[] = "agi.cx16.debug";
#pragma rodata-name (pop)

#pragma code-name (push, "BANKRAM05");
extern boolean b5IsDebuggingEnabled();

void b5InitializeDebugging()
{
#define NO_READ_SIZES 4
	byte lfn;
	byte i;
	int bytesRead;
	int readSizes[4] = { LOCAL_WORK_AREA_SIZE, 100, 10, 1 };
	byte* debugBankPtr;

	if (b5IsDebuggingEnabled())
	{
		lfn = b6Cbm_openForSeeking(b6DebugFileName);

		debugBankPtr = b10BankedAlloc(LARGE_SIZE, &debugBank);

		printf(b5InitializingDebug, debugBank);
		for (i = 0; i < NO_READ_SIZES; i++)
		{
			while (cbm_read(lfn, GOLDEN_RAM_WORK_AREA, readSizes[i]))
			{
				memCpyBanked(debugBankPtr, GOLDEN_RAM_WORK_AREA, debugBank, readSizes[i]);

				debugBankPtr += readSizes[i];
			}
		}
		cbm_close(lfn);
	}
}
#pragma code-name (pop);

#pragma code-name (push, "BANKRAMDEBUG");
void bDbgDebugOn()
{
	opStartPrintingAt = 1;
}

void bDbgShowPriority()
{
	unsigned long i, j;
	byte priByte;

	asm("sei");
	SET_VERA_ADDRESS(BITMAP_START, 0, 1);
	SET_VERA_ADDRESS(PRIORITY_START, 1, 1);

	for (i = 0; i < PICTURE_HEIGHT; i++)
	{
		for (j = 0; j < BITMAP_WIDTH / 2; j++)
		{
			if (j < PICTURE_WIDTH / 2)
			{
				READ_BYTE_VAR_FROM_ASSM(priByte, VERA_data1);
				WRITE_BYTE_VAR_TO_ASSM(priByte, VERA_data0);
			}
			else
			{
				READ_BYTE_VAR_FROM_ASSM(priByte, VERA_data0); //Skip Over accords for bitmap being larger than priority
			}
		}
	}
	asm("cli");
}
void bDbgCheckMemory()
{
#ifdef CHECK_MEM
	int i;
	byte* mem = (byte*)1;
	for (i = 10; i < 100000 && i >= 0 && mem; i = i + 100)
	{
		mem = (byte*)malloc(i);
		if (!mem)
		{
			printf(bDbgRemainingMemoryMessage, i);
			i = -1;
		}
		else
		{
			free((byte*)mem);
		}
	}

#endif // CHECK_MEM
}

extern boolean* flag;
void bDbgDebugPrint(byte toPrint)
{
	int time;
	int clockVal = (int)clock();

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		if (clockVal > _clockBefore)
		{
			time = clockVal - _clockBefore;
		}
		else
		{
			time = _clockBefore - clockVal;
		}


		printf(bDbgPrintMessage, toPrint, flag[0]);
		_clockBefore = clockVal;
#ifdef CHECK_MEM
		bDbgCheckMemory();
#endif
	}
	if (opStopEvery)
	{
		asm("stp");
	}

	if (opCounter == opStopAt)
	{
		asm("stp");
		asm("nop"); //A pointless no op follows in order to make it clear in the debugger that this is the point we have stopped
		return;
	}

	if (opCounter == opExitAt)
	{
		exit(0);
	}

	opCounter++;
}

void bDbgPrintFalse()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgFalseResultMessage);
	}
}

void bDbgPrintTrue()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgTrueResultMessage);
	}
}

void bDbgPrintNot()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgInvertedResultMessage);
	}
}

void bDbgPrintOrMode()
{
	//printf("or mode started\n");
}


extern byte logDebugVal1;
extern byte logDebugVal2;
extern byte logDebugVal3;
extern byte logDebugVal4;
extern byte logDebugVal5;
extern byte logDebugVal6;

void bDbgIsSet()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgIsSetMessage, logDebugVal1, flag[logDebugVal1]);
	}
}

void bDbgGreaterThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgGreaterThan_8N_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
	}
}

void bDbgLessThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgLessThan_8N_Message, logDebugVal1, logDebugVal2, logDebugVal1 < logDebugVal2);
	}
}

void bDbgGreaterThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgGreaterThan_8V_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
	}
}

void bDbgLessThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgLessThan_8V_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
	}
}

void bDbgEqualN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgEqualN_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
	}
}

void bDbgEqualV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgEqualV_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
	}
}

void bDbgInc()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgIncrementingMessage, logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
	}
}

void bDbgDec()
{
	byte actual = var[logDebugVal1];

	if (actual)
	{
		actual--;
	}

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgDecrementingMessage, logDebugVal1, actual);
	}
}

void bDbgAddN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgAddN_Message, logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void bDbgAddV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgAddV_Message, logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void bDbgSubN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgSubN_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
	}
}

void bDbgSubV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog)) {
		printf(bDbgSubV_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
	}
}

void bDbgAssignN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgAssignN_Message, logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void bDbgAssignV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgAssignV_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}

void bDbgIndirect()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgIndirectMessage, logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void bDbgScanStart()
{
	LOGICEntry logicEntry;

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		int zpCode = (int)logDebugVal1 + ((int)*(&logDebugVal1 + 1) << 8);
		int cwCurrentCode = (int)logDebugVal3 + ((int)*(&logDebugVal3 + 1) << 8);
		int startPos = (int)logDebugVal5 + ((int)*(&logDebugVal5 + 1) << 8);

		getLogicEntry(&logicEntry, currentLog);

		printf(bDbgScanStartMessage, currentLog, (int)zpCode, (int)cwCurrentCode, (int)startPos, (int)(zpCode + cwCurrentCode) - startPos, logicEntry.entryPoint);
	}
}

void bDbgResetScanStart()
{
	LOGICEntry logicEntry;

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		getLogicEntry(&logicEntry, currentLog);

		printf(bDbgResetScanStartMessage, currentLog, logicEntry.entryPoint == 0 ? "" : "not");
	}
}

void bDbgIndirectV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgIndirectV_Message, logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}


void bDbgPostCheckVar()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgPostCheckVarMessage, logDebugVal1, var[logDebugVal1]);
	}
}

void bDbgPostCheckFlag()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgPostCheckFlagMessage, logDebugVal1, flag[logDebugVal1]);
	}
}

void bDbgCodeJump()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgCodeJumpMessage, logDebugVal1, logDebugVal2, (logDebugVal2 << 8), (logDebugVal2 << 8) | logDebugVal1);
	}
}

void bDbgNewRoom()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgNewRoomMessage, logDebugVal1);
	}
}

void bDbgExitAllLogics()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf(bDbgExitAllLogicsMessage, newRoomNum, hasEnteredNewRoom, exitAllLogics);
	}
}

void bDbgPrintScriptStart()
{
	printf(bDbgScriptStartMessage, currentLog, opCounter, var[0]);
}

void bDbgPrintRoomChange()
{
	printf(bDbgRoomChangeMessage, var[0], opCounter);
}
#pragma code-name (pop);
