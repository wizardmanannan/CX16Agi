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

void b7CheckTimer(unsigned int intervalMs)
{
	clock_t difference = clock() - _before;

	if ((unsigned int) difference > intervalMs)
	{
		if (_timerProc != NULL)
		{
			_timerProc();
		}
		_before = clock();
	}
}

#pragma code-name (pop)