#include "timer.h"

#pragma code-name (push, "BANKRAM06")

timer_proc _timerProc;
unsigned int previousVSyncCounter;

void b6InitTimer(timer_proc timerProc)
{
	_timerProc = timerProc;
	previousVSyncCounter = vSyncCounter;
}

void b6CheckTimer()
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

void b6PauseTimer()
{
	boolean areInteruptsAlreadyDisabled = isInteruptDisabled();
	
	if(!areInteruptsAlreadyDisabled)
	{
		asm("sei");
	}
	
	timerEnabled = FALSE;

	if(!areInteruptsAlreadyDisabled)
	{
		REENABLE_INTERRUPTS();
	}
}

void b6ResumeTimer(boolean setKq3TimerFlag)
{
	boolean areInteruptsAlreadyDisabled = isInteruptDisabled();
	
	if(!areInteruptsAlreadyDisabled)
	{
		asm("sei");
	}
	timerEnabled = TRUE;

	if(isKq3 && setKq3TimerFlag)
	{
	flag[76] = TRUE; //Forced timer to be redrawn with the 'updateTimer' flag
	}

	if(!areInteruptsAlreadyDisabled)
	{
		REENABLE_INTERRUPTS();
	}
}

#pragma code-name (pop)