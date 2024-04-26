/***************************************************************************
** meka.c
**
** Working name for an AGI interpreter based on the work done by J.Moller,
** L.Ewing, and P.Kelly, thus the name M.E.K.A. (A is Adventure). These
** three formed the AGI group and did the bulk of the initial investigation
** but we have since been grateful to the others who have joined the team
** in a practical way.
**
** (c) Lance Ewing, 1998.
***************************************************************************/
//#include "general.h"
//#include "timer.h"
//#include "agifiles.h"
//#include "logic.h"
//#include "commands.h"
//#include "view.h"
//#include "stub.h"
#include "memoryManager.h"
//#include "lruCache.h"
//#include "debugHelper.h"
////#include "object.h"
////#include "words.h"
//#include "picture.h"
//#include "irq.h"
//#include "textLayer.h"
//#include "loadingScreen.h"
//#include "structMetadata.h"
//#include "floatDivision.h"
//#include "parser.h"
//#include "sound.h"

void main()
{
    memoryMangerInit();
}

void main2()
{
    //AGIFile AGIData;
    //BITMAP *temp = create_bitmap(640, 32);
    //char string1[80], string2[80];

    //allegro_init();
    //install_keyboard();
    //initFiles();
    ////loadAGIFile(PICTURE, picdir[129], &AGIData);
    //loadPictureFile(3);
    //initPicture();
    ////drawPic(AGIData.data, AGIData.size, TRUE);
    //initAGIScreen();
    //initPalette();
    //install_timer();
    //initSound();
    //picFNum = 3;
    //drawPic(loadedPictures[3].data, loadedPictures[3].size, TRUE);
    //loadViewFile(0);
    //addToPic(0, 0, 0, 55, 50, 7, 0);
    //discardView(0);
    //showPic();

    ////drawBigChar(temp, 'A', 0, 0, 7, 15);
    ////drawBigString(temp, "This is a test", 0, 0, 7, 15);
    ////blit(temp, agi_screen, 0, 0, 0, 0, 16*14, 16);
    ////getch();
    //remove_keyboard();
    //printInBoxBig("The quick brown fox jumps over the lazy dog.", -1, -1, 30);
    //loadSoundFile(1);
    //playSound(1);
    //getch();
    //closePicture();
    //allegro_exit();
    //strcpy(string1, "Variable 1: %v1|2 %%");
    //processString(string1, string2);
}
#pragma code-name (push, "BANKRAM07")
void Dummy() {};
#pragma code-name (pop)