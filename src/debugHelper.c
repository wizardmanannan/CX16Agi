#pragma once
#include "debugHelper.h"

extern boolean* flag;
extern byte* var;

long opCounter = 1;
long stopAt = 2000;
long exitAt = 2000;
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
	printf("op %lu, %d, var 0 is %d\n", opCounter, toPrint, var[0]);

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
	printf("the result is false\n");
}

void debugPrintTrue()
{
	printf("the result is true\n");
}

void debugPrintNot()
{
	printf("the result is inverted by not\n");
}

void debugPrintOrMode()
{
	//printf("or mode started\n");
}


extern byte logDebugVal1;
extern byte logDebugVal2;

void debugIsSet()
{
	printf("checking that %d is set and it %d\n", logDebugVal1, flag[logDebugVal1]);
}

void debugGreaterThan_8N()
{
	printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] > logDebugVal2);
}

void debugLessThan_8N()
{
	printf("Checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] < logDebugVal2);
}

void debugGreaterThan_8V()
{
	printf("checking that %d (%d) is > %d and the result should be %d\n", logDebugVal1, var[logDebugVal1] , logDebugVal2, var[logDebugVal2], var[logDebugVal1] > var[logDebugVal2]);
}

void debugLessThan_8V()
{
	printf("checking that %d is < %d and the result should be %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] < var[logDebugVal2]);
}

void debugEqualN()
{
	printf("checking that %d (%d) is equal to %d and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] == logDebugVal2);
}

void debugEqualV()
{
	printf("checking that %d (%d) is equal to %d (%d) and it %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] == var[logDebugVal2]);
}

void debugInc()
{
	printf("incrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] + 1);
}

void debugDec()
{
	printf("decrementing var %d(%d) to %d\n", logDebugVal1, var[logDebugVal1], var[logDebugVal1] - 1);
}

void debugAddN()
{
	printf("add var %d (%d) to %d which is", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] + logDebugVal2);
}

void debugAddV()
{
	printf("add var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] + var[logDebugVal2]);
}

void debugSubN()
{
	printf("sub var %d (%d) to %d which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal1] - logDebugVal2);
}

void debugSubV()
{
	printf("sub var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2], var[logDebugVal1] - var[logDebugVal2]);
}

void debugAssignN()
{
	printf("assign var %d (%d) to %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
}

void debugAssignV()
{
	printf("assign var %d (%d) to %d (%d) which is\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
}

void debugIndirect()
{
	printf("indir %d (%d) value %d\n", logDebugVal1, var[logDebugVal1], logDebugVal2);
}

void debugIndirectV()
{
	printf("indir V %d (%d) value %d (%d)\n", logDebugVal1, var[logDebugVal1], logDebugVal2, var[logDebugVal2]);
}


void debugPostCheckVar()
{
	printf("post check var %d (%d)\n", logDebugVal1, var[logDebugVal1]);
}

void debugPostCheckFlag()
{
	printf("post check flag %d (%d)\n", logDebugVal1, flag[logDebugVal1]);

}

void codeJumpDebug()
{
	printf("b1 is %d b2 is %d and the jump result is %u\n", logDebugVal1, logDebugVal2, (logDebugVal2 << 8) | logDebugVal1);
}
#pragma code-name (pop);
void debugPrintCurrentCodeState(byte* code)
{
	printf("the code is now %u and the address is %p\n", *code, code);
}




