#include "showObj.h"

#pragma code-name (push, "BANKRAM11")
void b11ShowObj(byte objNum)
{
   View localView;
   Loop localLoop;
   Cel localCel;
   SpriteAllocationSize spriteAllocationWidth, spriteAllocationHeight;
   VeraSpriteAddress spriteAddress;

   b9LoadViewFile(objNum);

   getLoadedView(&localView, objNum);
   getLoadedLoop(&localView, &localLoop, 0);
   getLoadedCel(&localLoop, &localCel, 0);

   spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
   spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);
   spriteAddress = bDFindFreeVramBlock(spriteAllocationWidth, spriteAllocationHeight);

   printf("you have a width of %d and a height of %d and have allocated %p\n", spriteAllocationWidth, spriteAllocationHeight, &spriteAddress);

   if(spriteAddress)
   {
    b9CelToVera(&localCel, localLoop.celsBank, spriteAddress, MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY);

    asm("stp");

    bDDeleteAllocation(spriteAddress, spriteAllocationWidth, spriteAllocationHeight);
   }

   b9DiscardView(objNum);
}

#pragma code-name (pop)