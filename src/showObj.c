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
byte b11AllocateShowObjSlots(SpriteAllocationSize width, SpriteAllocationSize height,
	byte requestedSlots, VeraSpriteAddress* spriteAddresses)
{
	byte i;
	if (!spriteAddresses)
	{
		return 0;
	}
	for (i = 0; i < MAX_SHOW_OBJ_SPLIT_SPRITES; ++i)
		spriteAddresses[i] = 0;
	if (requestedSlots == 0 || requestedSlots > MAX_SHOW_OBJ_SPLIT_SPRITES)
		return 0;
	for (i = 0; i < requestedSlots; ++i)
	{
		spriteAddresses[i] = bDFindFreeVramBlock(width, height);
		if (!spriteAddresses[i])
		{
			break;
		}
	}
	return i;
}

void b11ReleaseShowObjSlots(SpriteAllocationSize width, SpriteAllocationSize height,
	byte slotCount, VeraSpriteAddress* spriteAddresses)
{
	byte i;
	if (!spriteAddresses || slotCount > MAX_SHOW_OBJ_SPLIT_SPRITES)
	{
		return;
	}
	for (i = 0; i < slotCount; ++i)
	{
		if (spriteAddresses[i])
		{
			bDDeleteAllocation(spriteAddresses[i], width, height);
		}
	}
}

void b11ShowObj(byte objNum)
{
	View localView;
	Loop localLoop;
	Cel localCel;
	SpriteAllocationSize spriteAllocationWidth, spriteAllocationHeight;
	VeraSpriteAddress spriteAddress[MAX_SPRITES_ROW_OR_COLUMN_SIZE];
	PaletteGetResult paletteGetResult;
	PaletteGetResult refreshedPalette;
	byte i, ch, allocatedSlots = 0;
	boolean outOfSpriteMemory, viewWasLoaded;

	getLoadedView(&localView, objNum);
	viewWasLoaded = localView.loaded;
	paletteGetResult = b9LoadViewFile(objNum);
	getLoadedView(&localView, objNum);
	if (!localView.loaded)
	{
		return;
	}

	getLoadedLoop(&localView, &localLoop, 0);   /* show.obj always uses loop 0 */
	getLoadedCel(&localLoop, &localCel, 0);     /* and cel 0 */
	if (localLoop.palette == PALETTE_NOT_SET)
	{
		localLoop.palette = bECreateSpritePalette(localCel.transparency, &refreshedPalette);
		if (refreshedPalette == FailToAllocate) return;
		/* This palette belongs to the retained view, beyond the close-up. */
		setLoadedLoop(&localView, &localLoop, 0);
	}
	spriteAllocationWidth = bEGetSpriteAllocateSize(localLoop.allocationWidth);
	spriteAllocationHeight = bEGetSpriteAllocateSize(localLoop.allocationHeight);
	outOfSpriteMemory = localView.maxVeraSlots == 0 ||
		localView.maxVeraSlots > MAX_SPRITES_ROW_OR_COLUMN_SIZE ||
		localView.maxVeraSlots > MAX_SHOW_OBJ_SPLIT_SPRITES;
	if (!outOfSpriteMemory)
	{
		allocatedSlots = b11AllocateShowObjSlots(spriteAllocationWidth,
			spriteAllocationHeight, localView.maxVeraSlots, spriteAddress);
		outOfSpriteMemory = allocatedSlots != localView.maxVeraSlots;
		for (i = 0; i < allocatedSlots; ++i)
			b11ShowObjSpriteAddressShifted[i] = spriteAddress[i] >> 5;
	}

	if (!outOfSpriteMemory)
	{
		if (localView.maxVeraSlots > 1)
			*((byte*)SPLIT_COUNTER) = 1;

		for (i = 0; i < localView.maxVeraSlots; ++i)
		{
			/* Clear the slot while the foreground owns the VERA ports. */
			b6EnterForegroundVera();
			SET_VERA_ADDRESS_ZP(spriteAddress[i], VERA_ADDRESS, VERA_ADDRESS_HIGH);
			bEClearVeraSprite(localLoop.allocationWidth, localLoop.allocationHeight);
			b6LeaveForegroundVera();

			if (localView.maxVeraSlots > 1)
			{
				*((byte*)(SPLIT_CEL_BANK)) = localCel.splitCelBank;
				*((byte***)(SPLIT_CEL_SEGMENTS)) = localCel.splitCelPointers;
			}
			b9CelToVera(&localCel, localLoop.celsBank, spriteAddress[i],
				MAX_PRIORITY, spriteAllocationWidth / 2, 0, 0, MAX_PRIORITY,
				localView.maxVeraSlots);

			if (localView.maxVeraSlots > 1)
				(*((byte*)SPLIT_COUNTER))++;
		}

		b11ShowObjX = PICTURE_WIDTH - localCel.width;
		b11ShowObjY = 204 - localCel.height;
		b11ShowObjSprAttr7 = localLoop.allocationHeight << 6 |
			localLoop.allocationWidth << 4 | localLoop.palette;
		b11VeraSlots = localView.maxVeraSlots;
		b6SetAndWaitForIrqState(SHOW_OBJ);
	}

	b3DisplayMessageBox(localView.description, localView.codeBlockBank,
		AUTO_CALC_ROW, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER,
		DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);
	do
	{
		GET_IN(ch);
	} while (ch != KEY_ESC && ch != KEY_ENTER);

	b3ClearLastPlacedText();
	b6SetAndWaitForIrqState(CLEAR_OBJ);
	b11ReleaseShowObjSlots(spriteAllocationWidth, spriteAllocationHeight,
		allocatedSlots, spriteAddress);

	if (paletteGetResult == Allocated)
		bFRemoveLastPalette();
	if (!viewWasLoaded)
		b9DiscardView(objNum);
}

#pragma code-name (pop)
