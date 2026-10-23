#include "timer.h"

#pragma code-name (push, "BANKRAM06")

timer_proc _timerProc;
uint32_t previousVSyncCounter;
static byte partialVSyncFrames;
extern byte* var;

void b6InitTimer(timer_proc timerProc)
{
	_timerProc = timerProc;
	b6CopyVSyncCounter32(&previousVSyncCounter);
	partialVSyncFrames = 0;
}

void b6CheckTimer(uint32_t currentVsyncCounter)
{
	uint32_t elapsedFrames = currentVsyncCounter - previousVSyncCounter;
	uint32_t elapsedSeconds;
	unsigned int remainder;

	previousVSyncCounter = currentVsyncCounter;
	if (elapsedFrames < 60U)
	{
		remainder = partialVSyncFrames + (byte)elapsedFrames;
		if (remainder < 60)
		{
			partialVSyncFrames = (byte)remainder;
			return;
		}
		partialVSyncFrames = (byte)(remainder - 60);
		elapsedSeconds = 1;
	}
	else
	{
		elapsedSeconds = elapsedFrames / 60U;
		remainder = partialVSyncFrames + (byte)(elapsedFrames % 60U);
		if (remainder >= 60)
		{
			elapsedSeconds++;
			remainder -= 60;
		}
		partialVSyncFrames = (byte)remainder;
	}

	if (_timerProc != NULL)
		_timerProc(elapsedSeconds);
}

void b6Timing_proc(uint32_t elapsedSeconds)
{
	uint32_t totalSeconds = elapsedSeconds + var[11];
	totalSeconds += ((uint32_t)var[12] * 60U);
	totalSeconds += ((uint32_t)var[13] * 60U * 60U);
	totalSeconds += ((uint32_t)var[14] * 24U * 60U * 60U);

	/* cc65 unsigned int is 16 bits; a day must be computed as unsigned long. */
	var[14] = (byte)(totalSeconds / (24UL * 60U * 60U));
	totalSeconds %= (24UL * 60U * 60U);
	var[13] = (byte)(totalSeconds / (60U * 60U));
	totalSeconds %= (60U * 60U);
	var[12] = (byte)(totalSeconds / 60U);
	var[11] = (byte)(totalSeconds % 60U);
}

#pragma code-name (pop)
