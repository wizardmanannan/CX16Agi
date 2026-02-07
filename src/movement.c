#include "movement.h"

#pragma code-name (push, "BANKRAM0A")

extern void bAFollowEgo(ViewTable* localViewTab);
extern void bAMoveTo(ViewTable* localViewTab, byte entryNum);

void bADetermineMovement(byte entryNum)
{
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

    if (localViewtab.flags & ANIMATED && localViewtab.flags & UPDATE && localViewtab.flags & DRAWN && (localViewtab.stepTimeCount == 1))
    {
        switch (localViewtab.motion)
        {
        case WANDER:
            bAWander(&localViewtab, entryNum);
            break;

        case FOLLOW:
            bAFollowEgo(&localViewtab);
            break;

        case MOVE_TO:
            bAMoveTo(&localViewtab, entryNum);
            break;
        }
        setViewTab(&localViewtab, entryNum);
    }
}

#pragma code-name (pop)