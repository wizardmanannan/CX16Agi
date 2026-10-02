#ifndef _TIMER_H_
#define _TIMER_H_
#include "irq.h"
typedef void (*timer_proc)(uint32_t elapsedSeconds);
void b6InitTimer(timer_proc timerProc);
void b6CheckTimer(uint32_t currentVsyncCounter);
void b6Timing_proc(uint32_t elapsedSeconds);
#endif /* _TIMER_H_ */