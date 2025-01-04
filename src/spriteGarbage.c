#include "spriteGarbage.h"

#pragma bss-name (push, "BANKRAM0E")
byte bAViewTableMetdataQuickLookupLow[VIEW_TABLE_SIZE];
byte bAViewTableMetdataQuickLookupHigh[VIEW_TABLE_SIZE];
#pragma bss-name (pop)

#pragma bss-name (push, "BANKRAM09")
byte bAViewTableQuickLookupLow[VIEW_TABLE_SIZE];
byte bAViewTableQuickLookupHigh[VIEW_TABLE_SIZE];
#pragma bss-name (pop)

#pragma bss-name (push, "BANKRAM11")
byte bAViewQuickLookupLow[NO_VIEWS];
byte bAViewQuickLookupHigh[NO_VIEWS];
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM0A")
void bAGarbageCollectorInit()
{
	int i;
	ViewTable* viewTable;
	ViewTableMetadata* metadata;
	View* view;
	byte tempLow, tempHigh;
	
	SGC_LAST_LOCATION_GC_CHECKED = 0; //This means we start searching from 1
	for (i = 0; i < VIEW_TABLE_SIZE; i++)
	{
		viewTable = &viewtab[i];
		tempLow = (byte) ((unsigned int)viewTable & (unsigned int) 0xFF);
		tempHigh = (byte)((unsigned int)viewTable >> (unsigned int) 8);

		memCpyBanked(&bAViewTableQuickLookupLow[i], &tempLow, VIEWTAB_BANK, 1);
		memCpyBanked(&bAViewTableQuickLookupHigh[i], &tempHigh, VIEWTAB_BANK, 1);

		//printf("%p %p %p %p %p\n", &viewtab[i], viewTable, sizeof(ViewTable), (unsigned int)viewTable & (unsigned int)0xFF, (unsigned int)viewTable >> (unsigned int)8);
	
		metadata = &viewTableMetadata[i];
		tempLow = (unsigned int)metadata & (unsigned int)0xFF;
		tempHigh = (unsigned int)metadata >> (unsigned int) 8;

		memCpyBanked(&bAViewTableMetdataQuickLookupLow[i], &tempLow, SPRITE_METADATA_BANK, 1);
		memCpyBanked(&bAViewTableMetdataQuickLookupHigh[i], &tempHigh, SPRITE_METADATA_BANK, 1);
	}

	for (i = 0; i < NO_VIEWS; i++)
	{
		view = &loadedViews[i];
		tempLow = (int)view & 0xFF;
		tempHigh = (int)view >> 8;

		memCpyBanked(&bAViewQuickLookupLow[i], &tempLow, LOADED_VIEW_BANK, 1);
		memCpyBanked(&bAViewQuickLookupHigh[i], &tempHigh, LOADED_VIEW_BANK, 1);
	}
}
#pragma code-name (pop)

