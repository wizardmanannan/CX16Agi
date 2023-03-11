#pragma once
#include "debugHelper.h"

extern boolean* flag;
extern byte* var;

long opCounter = 1;
long stopAt = 716;
long exitAt = 2500;
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
	printf("Op %lu, %d\n", opCounter, toPrint);

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
	printf("The result is false\n");
}

void debugPrintTrue()
{
	printf("The result is true\n");
}

void debugPrintNot()
{
	printf("The result is inverted by not\n");
}

void debugPrintOrMode()
{
	printf("Or Mode Started\n");
}


extern byte logDebugVal1;
extern byte logDebugVal2;

void debugIsSet()
{
	printf("Checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
}

void debugGreaterThan_8()
{
	printf("Checking that %d is > %d and the result should be %d\n", logDebugVal1, logDebugVal2, logDebugVal1 > logDebugVal2);
}

void debugLessThan_8()
{
	printf("Checking that %d is < %d and the result should be %d\n", logDebugVal1, logDebugVal2, logDebugVal1 < logDebugVal2);
}

void debugEqual()
{
	printf("Checking that %d is equal to %d and it % d\n", logDebugVal1, logDebugVal2, logDebugVal1 == logDebugVal2);
}

void debugInc()
{
	printf("Incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
}

void debugDec()
{
	printf("Decrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] - 1);
}

void debugAddN()
{
	printf("Add var %d (%d) to %d which is", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] + logDebugVal2);
}

void debugAddV()
{
	printf("Add var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] + var[logDebugVal2]);
}

void debugSubN()
{
	printf("Sub var %d (%d) to %d which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
}

void debugSubV()
{
	printf("Sub var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
}

void debugAssignN()
{
	printf("Assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
}

void debugAssignV()
{
	printf("Assign var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
}

void debugIndirect()
{
	printf("Indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
}

void debugIndirectV()
{
	printf("Indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
}


void debugPostCheckVar()
{
	printf("Post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
}

void debugPostCheckFlag()
{
	printf("Post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);

}

void codeJumpDebug()
{
	printf("b1 is %d b2 is %d and the jump result is %u\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8) | logDebugVal1);
}
#pragma code-name (pop);
void debugPrintCurrentCodeState(byte* code)
{
	printf("The code is now %u and the address is %p\n", *code, code);
}




