#pragma once
#include "debugHelper.h"

extern boolean* flag;
extern byte* var;
extern byte codeBank;
extern byte newRoomNum;
extern boolean hasEnteredNewRoom, exitAllLogics;
extern int currentLog;

long opCounter = 1;
long opStopAt = 0;
long opExitAt = 0;
long opStartPrintingAt = 0x561A0;
boolean opStopEvery = FALSE;
int _clockBefore = 0;

long pixelCounter = 1;
long pixelStartPrintingAt = 1;
long pixelStopAt = -1;
long pixelFreezeAt = -1;

unsigned long queueAction = 0;

//#define CHECK_MEM;

//void stopAtQueue()
//{
//	byte previousRAMBank = RAM_BANK; 
//
//	RAM_BANK = 0x2A;
//	if (*((byte*)0xA7EE) == 69)
//	{
//		printf("The queue action number is %lu\n", queueAction);
//		asm("stp");
//		asm("lda #$EA");
//	}
//
//	RAM_BANK = previousRAMBank;
//}

void stopAtPixel()
{
	if (pixelCounter >= 0x25D9)
	{
		asm("stp"); //Two pointless nops follow in order to make it clear where we have stopped
		asm("nop");
		asm("nop");
	}
}

void stopAtQueueAction()
{
	if (queueAction >= 66)
	{
		asm("stp"); //Two pointless nops follow in order to make it clear where we have stopped
		asm("nop");
		asm("nop");
	}
}

void stopAtFunc()
{
	if (opCounter >= 51)
	{
		asm("stp"); //Two pointless nops follow in order to make it clear where we have stopped
		asm("nop");
		asm("nop");
	}
}


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
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0 && opStartPrintingAt != 0)
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
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("the result is false\n");
	}
}

void debugPrintTrue()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("the result is true\n");
	}
}

void debugPrintNot()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
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
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void debugGreaterThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
	}
}

void debugLessThan_8N()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("Checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] < logDebugVal2);
	}
}

void debugGreaterThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
	}
}

void debugLessThan_8V()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
	}
}

void debugEqualN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d (%d) is equal to %d and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
	}
}

void debugEqualV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("checking that %d (%d) is equal to %d (%d) and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
	}
}

void debugInc()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
	}
}

void debugDec()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("decrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] - 1);
	}
}

void debugAddN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("add var %d (%d) to %d which is %d", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] + logDebugVal2);
	}
}

void debugAddV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("add var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] + var[logDebugVal2]);
	}
}

void debugSubN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("sub var %d (%d) to %d which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
	}
}

void debugSubV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0) {
		printf("sub var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
	}
}

void debugAssignN()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugAssignV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("assign var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], logDebugVal2);
	}
}

void debugIndirect()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugIndirectV()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}


void debugPostCheckVar()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
	}
}

void debugPostCheckFlag()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void codeJumpDebug()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("b1 is %d b2 is %d, Shift %u and the jump result is %u.\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8), (logDebugVal2 << 8) | logDebugVal1);
	}
}

void debugNewRoom()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("Attempting to enter new room %d\n", logDebugVal1);
	}
}

void debugExitAllLogics()
{
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0)
	{
		printf("----------Exit Debug: Attempting to enter new room %d. Has entered new Room: %d. Has exited all logics %d\n", newRoomNum, hasEnteredNewRoom, exitAllLogics);
	}
}

void debugPrintCurrentCodeState(byte* code)
{
	byte codeValue;
	memCpyBanked(&codeValue, code, codeBank, 1);
	if (opCounter >= opStartPrintingAt && opStartPrintingAt != 0) {
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


	printf("\n%lu: attempting To Draw At %d, %d. The address of pixel counter is %p \n", pixelCounter, logDebugVal1, logDebugVal2, &pixelCounter);
}

void b5DebugPixelDraw()
{
	int* expectedDrawAddress = (STARTING_BYTE + logDebugVal1) + (logDebugVal2 * BYTES_PER_ROW);

	printf("%lu: We expect to draw at %p, we draw at %p. Color: %p. Result %d. %d,%d\n", pixelCounter, expectedDrawAddress, drawWhere, toDraw, expectedDrawAddress == drawWhere, logDebugVal1, logDebugVal2);

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

void b5DebugFloodQueueRetrieve()
{
	printf("%lu : retrieved %d\n", queueAction, logDebugVal1);
}

void b5DebugFloodQueueStore()
{
	printf("%lu : stored %d\n", queueAction, logDebugVal1);
}

void b5DebugPixelDrawAddress()
{
	int result;
	*((byte*)&result) = logDebugVal1;
	*((byte*)&result + 1) = logDebugVal2;
	printf("Drawing at %p\n", result);
}

#pragma code-name (pop);





