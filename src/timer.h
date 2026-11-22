#ifndef _TIMER_H_
#define _TIMER_H_
#include "irq.h"
#include "memoryManager.h"
typedef void (*timer_proc)();
void b6InitTimer(timer_proc timerProc);
void b6CheckTimer();

extern boolean timerEnabled;

#pragma wrapped-call (push, trampoline, TIMER_BANK)
extern void b6PauseTimer();
extern void b6ResumeTimer(boolean setKq3TimerFlag);
#pragma wrapped-call (pop)

#endif /* _TIMER_H_ */