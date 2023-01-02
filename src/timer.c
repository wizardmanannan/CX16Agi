#include "timer.h"
timer_proc _timerProc;
int _intervalMs = 0;
clock_t _before;


#pragma code-name (push, "BANKRAM0F")
void initTimer(timer_proc timerProc)
{
	_timerProc = timerProc;
	_before = clock();
}

void checkTimer(int intervalMs)
{
	clock_t difference = clock() - _before;
	int msec = difference * 1000 / CLOCKS_PER_SEC;

	if (msec > intervalMs)
	{
		if (_timerProc != NULL)
		{
			trampoline_0(_timerProc, MEKA_BANK);
		}
		_before = clock();
	}
}
#pragma code-name (pop)
