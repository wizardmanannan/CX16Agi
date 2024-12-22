#include "spriteGarbage.h"

#pragma bss-name (push, "BANKRAM0A")
byte bAViewTableQuickLookupLow[VIEW_TABLE_SIZE];
byte bAViewTableMetdataQuickLookupLow[VIEW_TABLE_SIZE];

byte bAViewTableQuickLookupHigh[VIEW_TABLE_SIZE];
byte bAViewTableMetdataQuickLookupHigh[VIEW_TABLE_SIZE];

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

	for (i = 0; i < VIEW_TABLE_SIZE; i++)
	{
		viewTable = &viewtab[i];
		bAViewTableQuickLookupLow[i] = (byte) ((unsigned int)viewTable & (unsigned int) 0xFF);
		bAViewTableQuickLookupHigh[i] = (byte)((unsigned int)viewTable >> (unsigned int) 8);

		//printf("%p %p %p %p %p\n", &viewtab[i], viewTable, sizeof(ViewTable), (unsigned int)viewTable & (unsigned int)0xFF, (unsigned int)viewTable >> (unsigned int)8);
	
		metadata = &viewTableMetadata[i];
		bAViewTableMetdataQuickLookupLow[i] = (unsigned int)metadata & (unsigned int)0xFF;
		bAViewTableMetdataQuickLookupHigh[i] = (unsigned int)metadata >> (unsigned int) 8;

		//printf("viewTab i %d %x %x %x %x \n", i, &viewTableMetadata[i], metadata, (unsigned int)metadata & (unsigned int)0xFF, (unsigned int)metadata >> (unsigned int)8);
	}

	for (i = 0; i < NO_VIEWS; i++)
	{
		view = &loadedViews[i];
		bAViewQuickLookupLow[i] = (int)view & 0xFF;
		bAViewQuickLookupHigh[i] = (int)view >> 8;
	}
}
#pragma code-name (pop)

