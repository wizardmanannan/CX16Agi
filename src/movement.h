#ifndef _MOVEMENT_H_
#define _MOVEMENT_H_

#include "general.h"
#include "view.h"

typedef enum {
	NORMAL_MOTION = 0,
	WANDER = 1,
	FOLLOW = 2,
	MOVE_TO = 3
} Motion;

void bADetermineMovement(byte entryNum);

#pragma wrapped-call (push, trampoline, VIEWTAB_BANK)
extern void b9StartMoveObj(ViewTable* localViewTab, byte entryNum, byte x, byte y, byte stepSize, byte completionFlag);
#pragma wrapped-call (pop)

#endif