#include "showObj.h"

extern unsigned int b11ShowObjSpriteAddressShifted;
extern byte b11ShowObjSprAttr7;
extern int b11ShowObjX;
extern byte b11ShowObjY;

byte trap = FALSE;

#pragma code-name (push, "BANKRAM11")
void b11ShowObj(byte objNum)
{
   View localView;
   Loop localLoop;
   Cel localCel;
   SpriteAllocationSize spriteAllocationWidth, spriteAllocationHeight;
   VeraSpriteAddress spriteAddress;
   PaletteGetResult paletteGetResult;
   byte palette = 0;

   ;trap = TRUE;

   b9LoadViewFile(objNum);

   getLoadedView(&localView, objNum);
   getLoadedLoop(&localView, &localLoop, 0);
   getLoadedCel(&localLoop, &localCel, 0);

   b11ShowObjSprAttr7 = localCel.width;

   spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
   spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);
   spriteAddress = bDFindFreeVramBlock(spriteAllocationWidth, spriteAllocationHeight);

   //printf("you have a width of %d and a height of %d and have allocated %lx, the cel is on %p bank %p\n", spriteAllocationWidth, spriteAllocationHeight, spriteAddress, localCel.bmp, localCel.bitmapBank);
   //asm("stp");


   if(spriteAddress)
   {

    // if(localLoop.palette)
    // {
    //    palette = bFGetPalette(id, paletteGetResult);
    // }

    asm("sei");
    b9CelToVera(&localCel, localLoop.celsBank, spriteAddress, MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY);
    REENABLE_INTERRUPTS();

    b11ShowObjSpriteAddressShifted = spriteAddress >> 5;
    b11ShowObjX = PICTURE_WIDTH - localCel.width;
    b11ShowObjY = PICTURE_HEIGHT / 2 + localCel.height / 2;
    //printf("you are displaying it at %d %d width %d height %d. The trans color is %d with palette %d\n", b11ShowObjX, b11ShowObjY, localCel.width, localCel.height, localCel.transparency, localLoop.palette);

    // printf("the description is %p on bank %p\n", localView.description, localView.codeBlockBank);
    // asm("stp");

    b11ShowObjSprAttr7 = localLoop.allocationHeight << 6 | localLoop.allocationWidth << 4 | localLoop.palette;

    b6SetAndWaitForIrqState(SHOW_OBJ);
    
    while(TRUE);

    bDDeleteAllocation(spriteAddress, spriteAllocationWidth, spriteAllocationHeight);
   }

   b9DiscardView(objNum);
}

#pragma code-name (pop)