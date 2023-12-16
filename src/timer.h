#ifndef _TIMER_H_
#define _TIMER_H_
#include "irq.h"
typedef void (*timer_proc)();
void b6InitTimer(timer_proc timerProc);
void b6CheckTimer(int intervalMs);
#endif /* _TIMER_H_ */