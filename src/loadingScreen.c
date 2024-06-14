#include "loadingScreen.h"
#define LOADING_BOX_SIZE 18

#pragma rodata(push, "BANKRAM06");
const char B6_LOADING_TEXT[] = "loading . . . .";
#pragma rodata(pop);

#pragma code-name (push, "BANKRAM06")
boolean loadingScreenDisplayed = FALSE;

void b6DisplayLoadingScreen()
{
	if (!loadingScreenDisplayed)
	{
		b6SetAndWaitForIrqStateAsm(BLANK_SCREEN);
		b3InitLayer1Mapbase();

		b6SetAndWaitForIrqStateAsm(TEXT_ONLY);
		b3DisplayMessageBox((char*)B6_LOADING_TEXT, 0, MAX_ROWS_DOWN / 2 - FIRST_ROW, MAX_CHAR_ACROSS / 2 - (LOADING_BOX_SIZE / 2), TEXTBOX_PALETTE_NUMBER, LOADING_BOX_SIZE);
		loadingScreenDisplayed = TRUE;
	}
}

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_3)
extern void bBUpdateObjects();
#pragma wrapped-call (pop)

void b6DismissLoadingScreen()
{
	if (loadingScreenDisplayed)
	{
		bBUpdateObjects(); //This is a little unorthodox but if we don't do this objects that should /should not display wait until the VBLANK after
		b6SetAndWaitForIrqStateAsm(BLANK_SCREEN);
		b3InitLayer1Mapbase();
		b6SetAndWaitForIrqStateAsm(NORMAL);
		loadingScreenDisplayed = FALSE;
	}
}
#pragma code-name (pop)