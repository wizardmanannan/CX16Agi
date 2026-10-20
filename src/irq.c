#include "irq.h"
#include <stdint.h>
#if defined(LOGIC_APP_REPLAY_TARGET)
#include "../tools/logic_app_replay.h"
#endif

#pragma code-name (push, "BANKRAM06")
uint16_t b6ReadVSyncCounter(void)
{
	volatile byte *counter = (volatile byte *)&vSyncCounter;
	byte high_before, low, high_after;
	do {
		high_before = counter[1];
		low = counter[0];
		high_after = counter[1];
	} while (high_before != high_after);
	return (uint16_t)(((uint16_t)high_before << 8) | low);
}

void b6WaitForVSyncFrames(unsigned int frames)
{
	uint16_t duration = (uint16_t)frames;
	uint16_t start;
	if (!duration)
		return;
#if defined(LOGIC_APP_REPLAY_TARGET)
	logicAppReplayAdvanceWait(duration);
#endif
	start = b6ReadVSyncCounter();
	for (;;) {
		uint16_t now = b6ReadVSyncCounter();
		if ((uint16_t)(now - start) >= duration)
			return;
		b6SetAndWaitForIrqState(DONT_CHANGE);
	}
}

void b6SetAndWaitForIrqState(IRQ_COMMAND state)
{
	b6SetAndWaitForIrqStateAsm(state);
}
#pragma code-name (pop)
