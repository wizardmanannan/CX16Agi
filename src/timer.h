#ifndef _TIMER_H_
#define _TIMER_H_
#include <time.h>
typedef void (*timer_proc)();
void b7InitTimer(timer_proc timerProc);
void b7CheckTimer(int intervalMs);
#endif /* _TIMER_H_ */