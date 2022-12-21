#ifndef _TIMER_H_
#define _TIMER_H_
#include <time.h>
typedef void (*timer_proc)();
void initTimer(timer_proc timerProc);
void checkTimer(int intervalMs);
#endif /* _TIMER_H_ */