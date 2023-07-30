#pragma once
#include "debugHelper.h"

extern boolean* flag;
extern byte* var;
extern byte codeBank;
extern byte newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern int currentLog;
extern int pixelCounter;

long opCounter = 1;
long stopAt = 0;
long exitAt = -1;
long startPrintingAt = -1;
boolean stopEvery = FALSE;
int _clockBefore = 0;

//#define CHECK_MEM;

void stopAtFunc()
{
	if (opCounter >= 51)
	{
		asm("stp");
		asm("nop");
		asm("nop");
	}
}


#pragma code-name (push, "BANKRAM05");
void b5CheckMemory()
{
#ifdef CHECK_MEM
	int i;
	byte* mem = (byte*) 1;
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
	if (opCounter >= startPrintingAt && startPrintingAt != -1 && startPrintingAt != -1)
	{
		if (clockVal > _clockBefore)
		{
			time = clockVal - _clockBefore;
		}
		else
		{
			time = _clockBefore - clockVal;
		}



		printf("op %lu, %d, var 0 is %d. Time taken %d. The RAM Bank is %p\n", opCounter, toPrint, var[0], time, *((byte*)(0x1d4b)));
		_clockBefore = clockVal;
#ifdef CHECK_MEM
		b5CheckMemory();
#endif
	}
	if (stopEvery)
	{
		asm("stp");
	}

	if (opCounter == stopAt)
	{
		asm("stp");
		asm("nop");
		return;
	}

	if (opCounter == exitAt)
	{
		exit(0);
	}

	opCounter++;
}

void debugPrintFalse()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("the result is false\n");
	}
}

void debugPrintTrue()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("the result is true\n");
	}
}

void debugPrintNot()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
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

void debugIsSet()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void debugGreaterThan_8N()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
	}
}

void debugLessThan_8N()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("Checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] < logDebugVal2);
	}
}

void debugGreaterThan_8V()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
	}
}

void debugLessThan_8V()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
	}
}

void debugEqualN()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d (%d) is equal to %d and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
	}
}

void debugEqualV()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("checking that %d (%d) is equal to %d (%d) and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
	}
}

void debugInc()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
	}
}

void debugDec()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("decrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] - 1);
	}
}

void debugAddN()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("add var %d (%d) to %d which is %d", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] + logDebugVal2);
	}
}

void debugAddV()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("add var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] + var[logDebugVal2]);
	}
}

void debugSubN()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("sub var %d (%d) to %d which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
	}
}

void debugSubV()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1) {
		printf("sub var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
	}
}

void debugAssignN()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugAssignV()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("assign var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], logDebugVal2);
	}
}

void debugIndirect()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugIndirectV()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}


void debugPostCheckVar()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
	}
}

void debugPostCheckFlag()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void codeJumpDebug()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("b1 is %d b2 is %d, Shift %u and the jump result is %u.\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8), (logDebugVal2 << 8) | logDebugVal1);
	}
}

void debugNewRoom()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("Attempting to enter new room %d\n", logDebugVal1);
	}
}

void debugExitAllLogics()
{
	if (opCounter >= startPrintingAt && startPrintingAt != -1)
	{
		printf("----------Exit Debug: Attempting to enter new room %d. Has entered new Room: %d. Has exited all logics %d\n", newRoomNum, hasEnteredNewRoom, exitAllLogics);
	}
}

void debugPrintCurrentCodeState(byte* code)
{
	byte codeValue;
	memCpyBanked(&codeValue, code, codeBank, 1);
	if (opCounter >= startPrintingAt && startPrintingAt != -1) {
		printf("the code is now %u and the address is %p\n", codeValue, code);
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


int lastPixelCounter;
boolean pixelDrawn = FALSE;
void b5DebugPrePixelDraw()
{
	pixelDrawn = FALSE;
	printf("\n%d: attempting To Draw At %d, %d. The address of pixel counter is %p \n", pixelCounter, logDebugVal1, logDebugVal2, &pixelCounter);
}

void b5DebugPixelDraw()
{
	int* localDrawWhere;
	byte localToDraw;
	int* expectedDrawAddress = (STARTING_BYTE + logDebugVal1) + (logDebugVal2 * BYTES_PER_ROW);

	memCpyBanked((byte*) &localDrawWhere, (byte*)&drawWhere, PICTURE_CODE_BANK, 2);
	memCpyBanked(&localToDraw, &toDraw, PICTURE_CODE_BANK, 1);

	printf("%d: We expect to draw at %p, we draw at %p. Color: %p. Result %d. %d,%d\n", pixelCounter, expectedDrawAddress, localDrawWhere, localToDraw, expectedDrawAddress == localDrawWhere, logDebugVal1, logDebugVal2);

	pixelDrawn = TRUE;
}

void b5CheckPixelDrawn()
{
	if (!pixelDrawn)
	{
		printf("draw warning: %d: %d,%d wasn't drawn\n", logDebugVal1, logDebugVal2);
	}
	else
	{
		printf("Pixel drawn %d, %d \n", logDebugVal1, logDebugVal2);
	}
}

void b5LineDrawDebug()
{
	printf("draw line %d,%d %d,%d\n", logDebugVal1, logDebugVal2, logDebugVal3, logDebugVal4);
}

unsigned long queueAction = 0;
void b5DebugFloodQueueRetrieve()
{
	printf("%lu : retrieved %d\n", queueAction++, logDebugVal1);
}

void b5DebugFloodQueueStore()
{
	printf("%lu : stored %d\n", queueAction++, logDebugVal1);
}

#pragma code-name (pop);





