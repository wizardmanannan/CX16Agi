#include "loadingScreen.h"
#define LOADING_TEXT "loading . . . ."
#define LOADING_BOX_SIZE 18
#pragma code-name (push, "BANKRAM06")
boolean loadingScreenDisplayed = FALSE;

void b6DisplayLoadingScreen()
{
	char* loadingText = LOADING_TEXT;

	if (!loadingScreenDisplayed)
	{
		b6SetAndWaitForIrqStateAsm(BLANK_SCREEN);
		b6InitLayer1Mapbase();

		b6SetAndWaitForIrqStateAsm(TEXT_ONLY);
		b3DisplayMessageBox(&loadingText[0], 0, MAX_ROWS_DOWN / 2 - FIRST_ROW, MAX_CHAR_ACROSS / 2 - (LOADING_BOX_SIZE / 2), TEXTBOX_PALETTE_NUMBER, LOADING_BOX_SIZE);
		loadingScreenDisplayed = TRUE;
	}
}

void b6DismissLoadingScreen()
{
	if (loadingScreenDisplayed)
	{
		b6SetAndWaitForIrqStateAsm(BLANK_SCREEN);
		b6InitLayer1Mapbase();
		b6SetAndWaitForIrqStateAsm(NORMAL);
		loadingScreenDisplayed = FALSE;
	}
}
#pragma code-name (pop)