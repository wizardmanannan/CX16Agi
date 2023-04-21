#include "timer.h"

#pragma code-name (push, "BANKRAM07")

timer_proc _timerProc;
int _intervalMs = 0;
clock_t _before;

void b7InitTimer(timer_proc timerProc)
{
	_timerProc = timerProc;
	_before = clock();
}

void b7CheckTimer(int intervalMs)
{
	clock_t difference = clock() - _before;
	unsigned int msec = difference * 1000 / CLOCKS_PER_SEC;

	if (msec > intervalMs)
	{
		if (_timerProc != NULL)
		{
			_timerProc();
		}
		_before = clock();
	}
}

#pragma code-name (pop)