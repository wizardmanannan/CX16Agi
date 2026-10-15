/* Compiled caller keeps target integer widths in the production clock path. */
#include "helpers.h"

#pragma wrapped-call (push, trampoline, MEKA_BANK)
#include "timer.h"
#pragma wrapped-call (pop)

extern byte *var;
byte testTimerClock[4];
uint32_t testTimerElapsed;

void testRunTimerClock(void)
{
	byte *previousVar = var;
	byte i;
	var = (byte *)0x0800;
	for (i = 0; i < 4; ++i) var[11 + i] = testTimerClock[i];
	b6Timing_proc(testTimerElapsed);
	for (i = 0; i < 4; ++i) testTimerClock[i] = var[11 + i];
	var = previousVar;
}
