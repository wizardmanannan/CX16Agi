#include "paletteManager.h"

#pragma bss-name (push, "BANKRAM0E")
byte allocatedPaletteOwners[NO_MANAGED_PALETTES];
byte palettesAllocated;
#pragma bss-name (pop)

//#define VERBOSE_PALETTE_MANAGER

#pragma code-name (push, "BANKRAM0E")
byte bEInitPaletteManager()
{
	memset(allocatedPaletteOwners, 0, NO_MANAGED_PALETTES);
	palettesAllocated = 0;
}

byte bEGetPalette(byte id, PaletteGetResult* result)
{
	byte i;

	if (palettesAllocated == NO_MANAGED_PALETTES)
	{
		*result = FailToAllocate;

		return 0;
	}

#ifdef VERBOSE_PALETTE_MANAGER
	printf("The number of available palettes are %d", NO_MANAGED_PALETTES - palettesAllocated);
#endif // VERBOSE_PALETTE_MANAGER


	for (i = 0; i < palettesAllocated; i++)
	{
		if (allocatedPaletteOwners[i] == id)
		{
			*result = AlreadyAllocated;
			return i;
		}
	}
	
	*result = Allocated;

#ifdef VERBOSE_PALETTE_MANAGER
	printf("Allocating %d\n", palettesAllocated);
#endif

	return palettesAllocated++;
}
#pragma code-name (pop)
