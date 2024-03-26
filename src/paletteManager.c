#include "paletteManager.h"

#pragma bss-name (push, "BANKRAM0E")
int allocatedPaletteOwners[NO_MANAGED_PALETTES];
byte palettesAllocated;
#pragma bss-name (pop)

//#define VERBOSE_PALETTE_MANAGER

#pragma code-name (push, "BANKRAM0E")
byte bEInitPaletteManager()
{
	memset(allocatedPaletteOwners, 0, NO_MANAGED_PALETTES);
	palettesAllocated = 0;
}

byte bEGetPalette(int id, PaletteGetResult* result)
{
	byte i;
	byte allocatedPalette;

#ifdef VERBOSE_PALETTE_MANAGER
	printf("gp1. the id is %d\n", id);
#endif // VERBOSE_PALETTE_MANAGER

	if (palettesAllocated == NO_MANAGED_PALETTES)
	{
		*result = FailToAllocate;

		return 0;
	}

#ifdef VERBOSE_PALETTE_MANAGER
	printf("gp2. the number of available palettes are %d\n", NO_MANAGED_PALETTES - palettesAllocated);
#endif // VERBOSE_PALETTE_MANAGER


	for (i = 0; i < palettesAllocated; i++)
	{
		if (allocatedPaletteOwners[i] == id)
		{
#ifdef VERBOSE_PALETTE_MANAGER
			printf("gp3. we are now returning %d\n",i);
#endif // VERBOSE_PALETTE_MANAGER

			*result = AlreadyAllocated;
			return i + BASE_MANAGED_PALETTE;
		}
	}
	
	*result = Allocated;

#ifdef VERBOSE_PALETTE_MANAGER
	printf("gp4. allocating %d\n", palettesAllocated);
#endif

	allocatedPalette = palettesAllocated++;
	allocatedPaletteOwners[i] = id;

	return allocatedPalette + BASE_MANAGED_PALETTE;
}
#pragma code-name (pop)
