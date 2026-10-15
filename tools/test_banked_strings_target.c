/* Compiled-C callers for the production helpers and text-renderer cases. */
#include "helpers.h"
#include "textLayer.h"

#pragma wrapped-call (push, trampoline, HELPERS_BANK)
extern int b5Multiply(int a, int b);
#pragma wrapped-call (pop)

int testHarnessSmokeResult;

void testRunHarnessSmoke(void)
{
	testHarnessSmokeResult = b5Multiply(257, 3);
}

void testRunTextCleanupRow1(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, 1);
}

void testRunTextCleanupRow4(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
}

void testRunTextCleanupLastRow(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, TILE_LAYER_HEIGHT);
}

static void displayMaximumHeightText(byte firstRow)
{
	char message[TILE_LAYER_HEIGHT * 2 + 1];
	byte rows = TILE_LAYER_HEIGHT - firstRow + 1;
	byte row;
	size_t length = 0;

	for (row = 0; row < rows; ++row)
	{
		message[length++] = 'A';
		if (row + 1 < rows) message[length++] = NEW_LINE;
	}
	message[length] = '\0';
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, firstRow);
}

void testRunTextCleanupMaximumOrigin1(void)
{
	displayMaximumHeightText(1);
}

void testRunTextCleanupMaximumOrigin2(void)
{
	displayMaximumHeightText(2);
}

void testRunTextCleanupMaximumOrigin4(void)
{
	displayMaximumHeightText(FIRST_TEXT_ROW);
}

void testRunTextCleanupClear(void)
{
	b3ClearLastPlacedText();
}
