#pragma once
#include "debugHelper.h"

extern boolean* flag;
extern byte* var;
extern byte codeBank;

long opCounter = 1;
long stopAt = 10000;
long exitAt = 20000;
long startPrintingAt = 6000;
boolean stopEvery = FALSE;

void stopAtFunc()
{
	if (opCounter > 1)
	{
		asm("stp");
	}
}


#pragma code-name (push, "BANKRAM05");
void debugPrint(byte toPrint)
{
	if (opCounter >= startPrintingAt)
	{
		printf("op %lu, %d, var 0 is %d\n", opCounter, toPrint, var[0]);
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
	if (opCounter >= startPrintingAt)
	{
		printf("the result is false\n");
	}
}

void debugPrintTrue()
{
	if (opCounter >= startPrintingAt)
	{
		printf("the result is true\n");
	}
}

void debugPrintNot()
{
	if (opCounter >= startPrintingAt)
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

void debugIsSet()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void debugGreaterThan_8N()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
	}
}

void debugLessThan_8N()
{
	if (opCounter >= startPrintingAt)
	{
		printf("Checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] < logDebugVal2);
	}
}

void debugGreaterThan_8V()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
	}
}

void debugLessThan_8V()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
	}
}

void debugEqualN()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d (%d) is equal to %d and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
	}
}

void debugEqualV()
{
	if (opCounter >= startPrintingAt)
	{
		printf("checking that %d (%d) is equal to %d (%d) and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
	}
}

void debugInc()
{
	if (opCounter >= startPrintingAt)
	{
		printf("incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
	}
}

void debugDec()
{
	if (opCounter >= startPrintingAt)
	{
		printf("decrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] - 1);
	}
}

void debugAddN()
{
	if (opCounter >= startPrintingAt)
	{
		printf("add var %d (%d) to %d which is %d", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] + logDebugVal2);
	}
}

void debugAddV()
{
	if (opCounter >= startPrintingAt)
	{
		printf("add var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] + var[logDebugVal2]);
	}
}

void debugSubN()
{
	if (opCounter >= startPrintingAt)
	{
		printf("sub var %d (%d) to %d which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
	}
}

void debugSubV()
{
	if (opCounter >= startPrintingAt) {
		printf("sub var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
	}
}

void debugAssignN()
{
	if (opCounter >= startPrintingAt)
	{
		printf("assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugAssignV()
{
	if (opCounter >= startPrintingAt)
	{
		printf("assign var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], logDebugVal2);
	}
}

void debugIndirect()
{
	if (opCounter >= startPrintingAt)
	{
		printf("indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
	}
}

void debugIndirectV()
{
	if (opCounter >= startPrintingAt)
	{
		printf("indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
	}
}


void debugPostCheckVar()
{
	if (opCounter >= startPrintingAt)
	{
		printf("post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
	}
}

void debugPostCheckFlag()
{
	if (opCounter >= startPrintingAt)
	{
		printf("post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);
	}
}

void codeJumpDebug()
{
	if (opCounter >= startPrintingAt)
	{
		printf("b1 is %d b2 is %d and the jump result is %u\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8) | logDebugVal1);
	}
}

void debugPrintCurrentCodeState(byte* code)
{
	byte codeValue;
	memCpyBanked(&codeValue, code, codeBank, 1);
	if (opCounter >= startPrintingAt) {
		printf("the code is now %u and the address is %p\n", codeValue, code);
	}
}

#pragma code-name (pop);





