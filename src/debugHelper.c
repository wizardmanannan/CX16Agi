#pragma once
#include "debugHelper.h"
#define PRINT_ALL_SCRIPTS 256

extern boolean* flag;
extern byte* var;
extern byte codeBank;
extern byte newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern int currentLog;

long opCounter = 1;
long opStopAt = 0;
long opExitAt = 0;
long opStartPrintingAt = 0;
int opPrintOnlyOnScript = 102;
boolean opStopEvery = FALSE;
int _clockBefore = 0;

long pixelCounter = 1;
long pixelStartPrintingAt = 0;
long pixelStopAt = -1;
long pixelFreezeAt = -1;

unsigned long queueAction = 0;




#pragma code-name (push, "BANKRAM05");
void b5CheckMemory()
{
#ifdef CHECK_MEM
	int i;
	byte* mem = (byte*)1;
	for (i = 10; i < 100000 && i >= 0 && mem; i = i + 100)
	{
		mem = (byte*)malloc(i);
		if (!mem)
		{
			printf("Your remaining memory is approx: %d \n", i);
			i = -1;
		}
		else
		{
			free((byte*)mem);
		}
	}

#endif // CHECK_MEM
}

void debugPrint(byte toPrint)
{
	int time;
	int clockVal = (int)clock();

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		if (clockVal > _clockBefore)
		{
			time = clockVal - _clockBefore;
		}
		else
		{
			time = _clockBefore - clockVal;
		}


		printf("op %lu, %d\n", opCounter, toPrint);
		_clockBefore = clockVal;
#ifdef CHECK_MEM
		b5CheckMemory();
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

void debugPrintFalse()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("the result is false\n");
	}
}

void debugPrintTrue()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("the result is true\n");
	}
}

void debugPrintNot()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("the result is inverted by not\n");
	}
}

void debugPrintOrMode()
{
	//printf("or mode started\n");
}


extern byte logDebugVal1;
extern byte logDebugVal2;
extern byte logDebugVal3;
extern byte logDebugVal4;
extern byte logDebugVal5;
extern byte logDebugVal6;

void debugIsSet()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void debugGreaterThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
	}
}

void debugLessThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, logDebugVal2, logDebugVal1 < logDebugVal2);
	}
}

void debugGreaterThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
	}
}

void debugLessThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
	}
}

void debugEqualN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d (%d) is equal to %d and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
	}
}

void debugEqualV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("checking that %d (%d) is equal to %d (%d) and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
	}
}

void debugInc()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
	}
}

void debugDec()
{
	byte actual = var[logDebugVal1];

	if (actual)
	{
		actual--;
	}

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("decrementing var %d to %d\n", logDebugVal1, actual);
	}
}

void debugAddN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("add var %d (%d) to %d", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugAddV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("add var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugSubN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("sub var %d (%d) to %d which is %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
	}
}

void debugSubV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog)) {
		printf("sub var %d (%d) to %d (%d) which is %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
	}
}

void debugAssignN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugAssignV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("assign var %d (%d) to %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}

void debugIndirect()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void b5DebugScanStart()
{
	LOGICEntry logicEntry;

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		int zpCode = (int)logDebugVal1 + ((int)*(&logDebugVal1 + 1) << 8);
		int cwCurrentCode = (int)logDebugVal3 + ((int)*(&logDebugVal3 + 1) << 8);
		int startPos = (int)logDebugVal5 + ((int)*(&logDebugVal5 + 1) << 8);

		getLogicEntry(&logicEntry, currentLog);

		printf("lognum: %d scan start: zp_code ((%p) + cwCurrentcode (%p)) - startpos (%p) = %p. actual %p", currentLog, (int)zpCode, (int)cwCurrentCode, (int)startPos, (int)(zpCode + cwCurrentCode) - startPos, logicEntry.entryPoint);
	}
}

void b5DebugResetScanStart()
{
	LOGICEntry logicEntry;

	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		getLogicEntry(&logicEntry, currentLog);

		printf("resetting scan for logic %d. it %s reset.\n", currentLog, logicEntry.entryPoint == 0 ? "" : "not");
	}
}

void debugIndirectV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}


void debugPostCheckVar()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
	}
}

void debugPostCheckFlag()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void codeJumpDebug()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("b1 is %d b2 is %d, shift %u and the jump result is %u.\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8), (logDebugVal2 << 8) | logDebugVal1);
	}
}

void debugNewRoom()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("attempting to enter new room %d\n", logDebugVal1);
	}
}

void debugExitAllLogics()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && (opPrintOnlyOnScript == PRINT_ALL_SCRIPTS || opPrintOnlyOnScript == currentLog))
	{
		printf("----------Exit Debug: Attempting to enter new room %d. Has entered new Room: %d. Has exited all logics %d\n", newRoomNum, hasEnteredNewRoom, exitAllLogics);
	}
}

void b5DebugPrintScriptStart()
{
	printf("ex s. %d counter op %lu, var 0 is %d\n", currentLog, opCounter, var[0]);
}

void b5DebugPrintRoomChange()
{
	printf("We are at %d, counter %lu\n", var[0], opCounter);
}
#pragma code-name (pop);





