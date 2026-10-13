/* Compiled-C caller for the production helpers-bank smoke case. */
#include "helpers.h"

#pragma wrapped-call (push, trampoline, HELPERS_BANK)
extern int b5Multiply(int a, int b);
#pragma wrapped-call (pop)

int testHarnessSmokeResult;

void testRunHarnessSmoke(void)
{
	testHarnessSmokeResult = b5Multiply(257, 3);
}
