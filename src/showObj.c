#include "showObj.h"

extern unsigned int b11ShowObjSpriteAddressShifted[MAX_SPRITES_ROW_OR_COLUMN_SIZE];
extern byte b11ShowObjSprAttr7;
extern int b11ShowObjX;
extern byte b11ShowObjY;
extern byte b11VeraSlots;


#pragma code-name (push, "BANKRAM11")
/* AGI show.obj close-up.
 *
 * Loads view objNum, allocates VERA sprite VRAM for loop 0 / cel 0
 * (one block per Vera slot if the cel is split), blits the pixel data,
 * then asks the IRQ to program sprite attributes at $1FFE0
 * (b11ShowObjIrqHandler). Split slots are a horizontal strip: each extra
 * slot is SPR_SIZE_64 to the right of the previous. Parked at the bottom
 * centre of the picture area.
 *
 * After the description box and Enter/Esc, CLEAR_OBJ deactivates those
 * sprite slots; allocations, view, and any extra palette are then released.
 */
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


   /* May claim a free palette; see the Allocated check at the end. */
   paletteGetResult = b9LoadViewFile(objNum);

   getLoadedView(&localView, objNum);
   getLoadedLoop(&localView, &localLoop, 0);   /* show.obj always uses loop 0 */
   getLoadedCel(&localLoop, &localCel, 0);     /* and cel 0 */

   /* AGI allocation size -> VERA sprite size enum for the allocator. */
   spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
   spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);

   /* One VRAM block per Vera slot. Raw address is kept for free;
    * >>5 form is what the IRQ writes into sprite attrs 0–1.
    * A failed alloc sets outOfSpriteMemory and leaves later entries 0.
    */
   for (i = 0; i < localView.maxVeraSlots && !outOfSpriteMemory; i++)
   {
      spriteAddress[i] = bDFindFreeVramBlock(spriteAllocationWidth, spriteAllocationHeight);
      b11ShowObjSpriteAddressShifted[i] = spriteAddress[i] >> 5;

      outOfSpriteMemory = !spriteAddress[i];

   }

   if (!outOfSpriteMemory)
   {
      /* Split-cel blit helper expects SPLIT_COUNTER starting at 1. */
      if (localView.maxVeraSlots > 1)
      {
         *((byte*)SPLIT_COUNTER) = 1;
      }

      for (i = 0; i < localView.maxVeraSlots; i++)
      {
         asm("sei");

         /* Clear this slot's VRAM before writing the cel pixels. */
         SET_VERA_ADDRESS_ZP(spriteAddress[i], VERA_ADDRESS, VERA_ADDRESS_HIGH);
         bEClearVeraSprite(localLoop.allocationWidth, localLoop.allocationHeight);
         REENABLE_INTERRUPTS();

         if (localView.maxVeraSlots > 1)
         {
            /* Bank + segment table for the current slice of a split cel. */
            *((byte*)(SPLIT_CEL_BANK)) = localCel.splitCelBank;
            *((byte***)(SPLIT_CEL_SEGMENTS)) = localCel.splitCelPointers;
         }

         /* Blit this slot into the VRAM just reserved.
          * MAX_PRIORITY so the close-up sits above the picture.
          */
         b9CelToVera(&localCel, localLoop.celsBank, spriteAddress[i], MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY, localView.maxVeraSlots);

         if (localView.maxVeraSlots > 1)
         {
            (*((byte*)SPLIT_COUNTER))++;
         }
      }

      /* Left edge of slot 0. The IRQ adds SPR_SIZE_64 per extra slot,
       * so a split object is a left-to-right strip of 64-pixel sprites.
       * Y is the bottom of the picture area (204-based). Bottom centre.
       */
      b11ShowObjX = PICTURE_WIDTH - localCel.width;
      b11ShowObjY = 204 - localCel.height;

      /* Attr 7 written by the IRQ: H-size << 6 | W-size << 4 | palette. */
      b11ShowObjSprAttr7 = localLoop.allocationHeight << 6 | localLoop.allocationWidth << 4 | localLoop.palette;

      /* Slot count. The IRQ ASLs this so it can CPY against a byte index
       * into the 16-bit _b11ShowObjSpriteAddressShifted table.
       */
      b11VeraSlots = localView.maxVeraSlots;
      b6SetAndWaitForIrqState(SHOW_OBJ);
   }

   /* Shown even if sprite allocation failed. */
   b3DisplayMessageBox(localView.description, localView.codeBlockBank, AUTO_CALC_ROW, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER, DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);

   do
   {
      GET_IN(ch);                     // Get keyboard input

   } while (ch != KEY_ESC && ch != KEY_ENTER);

   b3ClearLastPlacedText();
   /* Zeros the enable byte of up to MAX_SHOW_OBJ_SPLIT_SPRITES
    * attributes at $1FFE0. Harmless if they were never programmed.
    */
   b6SetAndWaitForIrqState(CLEAR_OBJ); //Doesn't matter is we clear the sprite attributes if we didn't display anything due to out of memory; this just deactivates sprites, which will have no effect if they were never displayed


   /* Free any slot that actually received an address.
    * Outside the success path: a multi-slot object can allocate some
    * blocks and then fail.
    */
   for (i = 0; i < localView.maxVeraSlots; i++)  //We might have enough memory to allocate one part of the object but not the other; this is why this is not inside the if (!outOfSpriteMemory)
   {
      if (spriteAddress[i])
      {
         bDDeleteAllocation(spriteAddress[i], spriteAllocationWidth, spriteAllocationHeight);
      }
   }

   /* Give back a palette claimed only for this view. */
   if (paletteGetResult == Allocated) //We only have so many palettes, if we allocate a palette which is not in used by one of the game sprites we should dispose it of
   {
      bFRemoveLastPalette();
   }

   b9DiscardView(objNum);
}

#pragma code-name (pop)