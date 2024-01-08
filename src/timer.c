#include "timer.h"

#pragma code-name (push, "BANKRAM06")

timer_proc _timerProc;
unsigned int previousVSyncCounter;

void b6InitTimer(timer_proc timerProc)
{
	_timerProc = timerProc;
	previousVSyncCounter = vSyncCounter;
}

void b6CheckTimer(int intervalMs)
{
	if (previousVSyncCounter != vSyncCounter)
	{
		if (_timerProc != NULL)
		{
			_timerProc();
		}
		
		previousVSyncCounter = vSyncCounter;
	}
}

#pragma code-name (pop)