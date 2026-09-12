#include "showObj.h"

extern unsigned int b11ShowObjSpriteAddressShifted[MAX_SPRITES_ROW_OR_COLUMN_SIZE];
extern byte b11ShowObjSprAttr7;
extern int b11ShowObjX;
extern byte b11ShowObjY;
extern byte b11VeraSlots;


boolean trap = FALSE;
#pragma code-name (push, "BANKRAM11")
void b11ShowObj(byte objNum)
{
   View localView;
   Loop localLoop;
   Cel localCel;
   SpriteAllocationSize spriteAllocationWidth, spriteAllocationHeight;
   VeraSpriteAddress spriteAddress[MAX_SPRITES_ROW_OR_COLUMN_SIZE];
   PaletteGetResult paletteGetResult;
   byte palette = 0, i;
   boolean outOfSpriteMemory = FALSE;


   b9LoadViewFile(objNum);

   getLoadedView(&localView, objNum);
   getLoadedLoop(&localView, &localLoop, 0);
   getLoadedCel(&localLoop, &localCel, 0);

   //printf("the view description address is %p on bank %p\n", localView.description, localView.codeBlockBank, localLoop.celsBank);
   asm("stp");

   b11ShowObjSprAttr7 = localCel.width;

   spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
   spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);

   for (i = 0; i < localView.maxVeraSlots && !outOfSpriteMemory; i++)
   {
      spriteAddress[i] = bDFindFreeVramBlock(spriteAllocationWidth, spriteAllocationHeight);
      b11ShowObjSpriteAddressShifted[i] = spriteAddress[i] >> 5;

      outOfSpriteMemory = !spriteAddress[i];

   }

   //printf("you have a width of %d and a height of %d and have allocated %lx, the cel is on %p bank %p\n", spriteAllocationWidth, spriteAllocationHeight, spriteAddress, localCel.bmp, localCel.bitmapBank);



   if (!outOfSpriteMemory)
   {

      // if(localLoop.palette)
      // {
      //    palette = bFGetPalette(id, paletteGetResult);
      // }

      *((byte*)SPLIT_COUNTER) = 1;
      for (i = 0; i < localView.maxVeraSlots; i++)
      {
         asm("sei");

         trap = TRUE;

         SET_VERA_ADDRESS_ZP(spriteAddress[i], VERA_ADDRESS, VERA_ADDRESS_HIGH);
         bEClearVeraSprite(localLoop.allocationWidth, localLoop.allocationHeight);
         REENABLE_INTERRUPTS();

         if (localView.maxVeraSlots > 1)
         {
            *((byte*)(SPLIT_CEL_BANK)) = localCel.splitCelBank;

            *((byte***)(SPLIT_CEL_SEGMENTS)) = localCel.splitCelPointers;
         }

         printf("the split cel pointers is %p on bank %p\n", localCel.splitCelPointers, localCel.splitCelBank);

         b9CelToVera(&localCel, localLoop.celsBank, spriteAddress[i], MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY, localView.maxVeraSlots);

         //printf("you are drawing to %lx the data is on %p bank %p\n", spriteAddress[i], localCel.splitCelPointers, localCel.splitCelBank);

         (*((byte*)SPLIT_COUNTER))++;
      }
      // asm("stp");
      // asm("nop");

      b11ShowObjX = PICTURE_WIDTH - localCel.width;
      b11ShowObjY = PICTURE_HEIGHT / 2 + localCel.height / 2;
      //printf("you are displaying it at %d %d width %d height %d. The trans color is %d with palette %d\n", b11ShowObjX, b11ShowObjY, localCel.width, localCel.height, localCel.transparency, localLoop.palette);

      //printf("the description is %p on bank %p\n", localView.description, localView.codeBlockBank);
      //asm("stp");

      b11ShowObjSprAttr7 = localLoop.allocationHeight << 6 | localLoop.allocationWidth << 4 | localLoop.palette;

      b11VeraSlots = localView.maxVeraSlots;
      b6SetAndWaitForIrqState(SHOW_OBJ);

      b3DisplayMessageBox(localView.description, localView.codeBlockBank, 14, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER, DEFAULT_BOX_WIDTH, TRUE);

   }

   for (i = 0; i < localView.maxVeraSlots; i++)
   {
      if (spriteAddress[i])
      {
         bDDeleteAllocation(spriteAddress[i], spriteAllocationWidth, spriteAllocationHeight);
      }
   }

   b9DiscardView(objNum);
}

#pragma code-name (pop)