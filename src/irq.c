#include "irq.h"

#pragma code-name (push, "BANKRAM06")
void b6SetAndWaitForIrqState(IRQ_COMMAND state)
{
	b6SetAndWaitForIrqStateAsm(state);
}
#pragma code-name (pop)