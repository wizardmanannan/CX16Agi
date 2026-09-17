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
		b3InitLayer1Mapbase(TILE_BYTE_2);

		b6SetAndWaitForIrqStateAsm(TEXT_ONLY);
		b3DisplayMessageBox((char*)B6_LOADING_TEXT, 0, MAX_ROWS_DOWN / 2 - FIRST_TEXT_ROW, MAX_CHAR_ACROSS / 2 - (LOADING_BOX_SIZE / 2), TEXTBOX_PALETTE_NUMBER, LOADING_BOX_SIZE, FALSE, FIRST_TEXT_ROW);
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
		//bBUpdateObjects();//To Do: Refresh the screen after loading
		b6SetAndWaitForIrqStateAsm(BLANK_SCREEN);
		b3InitLayer1Mapbase(TILE_BYTE_2);
		b6SetAndWaitForIrqStateAsm(NORMAL);
		loadingScreenDisplayed = FALSE;
	}
}
#pragma code-name (pop)