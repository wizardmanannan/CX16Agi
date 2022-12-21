#include "timer.h"
timer_proc _timerProc;
int _intervalMs = 0;
clock_t _before;

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
			_timerProc();
		}
		_before = clock();
	}
}
