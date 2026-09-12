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
   byte palette = 0, i, ch;
   boolean outOfSpriteMemory = FALSE;


   paletteGetResult = b9LoadViewFile(objNum);

   getLoadedView(&localView, objNum);
   getLoadedLoop(&localView, &localLoop, 0);
   getLoadedCel(&localLoop, &localCel, 0);

   b11ShowObjSprAttr7 = localCel.width;

   spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
   spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);

   for (i = 0; i < localView.maxVeraSlots && !outOfSpriteMemory; i++)
   {
      spriteAddress[i] = bDFindFreeVramBlock(spriteAllocationWidth, spriteAllocationHeight);
      b11ShowObjSpriteAddressShifted[i] = spriteAddress[i] >> 5;

      outOfSpriteMemory = !spriteAddress[i];

   }

   if (!outOfSpriteMemory)
   {
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

         b9CelToVera(&localCel, localLoop.celsBank, spriteAddress[i], MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY, localView.maxVeraSlots);

         (*((byte*)SPLIT_COUNTER))++;
      }

      b11ShowObjX = PICTURE_WIDTH - localCel.width;
      b11ShowObjY = 204 - localCel.height;

      b11ShowObjSprAttr7 = localLoop.allocationHeight << 6 | localLoop.allocationWidth << 4 | localLoop.palette;

      b11VeraSlots = localView.maxVeraSlots;
      b6SetAndWaitForIrqState(SHOW_OBJ);

      b3DisplayMessageBox(localView.description, localView.codeBlockBank, AUTO_CALC_ROW, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER, DEFAULT_BOX_WIDTH, TRUE);

      do
      {
         GET_IN(ch);                     // Get keyboard input

      } while (ch != KEY_ESC && ch != KEY_ENTER);

      b3ClearLastPlacedText();
      b6SetAndWaitForIrqState(CLEAR_OBJ);

      for (i = 0; i < localView.maxVeraSlots; i++)
      {
         if (spriteAddress[i])
         {
            bDDeleteAllocation(spriteAddress[i], spriteAllocationWidth, spriteAllocationHeight);
         }
      }

      if(paletteGetResult == Allocated) //We only have so many palettes, if we allocate a palette which is not in used by one of the game sprites we should dispose it of
      {
         bFRemoveLastPalette();
      }

      b9DiscardView(objNum);
   }
}

#pragma code-name (pop)