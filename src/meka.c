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
#include "general.h"
#include "timer.h"
#include "agifiles.h"
#include "logic.h"
#include "commands.h"
#include "view.h"
#include "stub.h"
#include "memoryManager.h"
#include "lruCache.h"
#include "debugHelper.h"
//#include "object.h"
//#include "words.h"
#include "picture.h"
#include "irq.h"
#include "textLayer.h"
#include "loadingScreen.h"
#include "structMetadata.h"
#include "floatDivision.h"
//#include "parser.h"
//#include "sound.h"

boolean stillRunning = TRUE, hasEnteredNewRoom = FALSE, exitAllLogics = FALSE;
byte* var = (byte*)&GOLDEN_RAM[VARS_AREA_START];
boolean* flag = &GOLDEN_RAM[FLAGS_AREA_START];
char string[12][40];
byte horizon;

#define  PLAYER_CONTROL   0
#define  PROGRAM_CONTROL  1
#define DEBUG 1
//#define VERBOSE

const unsigned int TIMER_WAIT_MS = 0;

volatile int counter;              /* Used for timer control */
volatile int hund;                 /* Used for interpreters clock */

int controlMode = PLAYER_CONTROL;    /* player.control or program.control */
int dirnOfEgo, newRoomNum, score;

extern int picFNum;    // Debugging. Delete at some stage!!

#pragma code-name (push, "BANKRAM06")
void b6AdjustEgoPosition()
{
    ViewTable localViewtab;

    getViewTab(&localViewtab, 0);

    switch (var[2]) {
    case 1:
        localViewtab.yPos = 167;
        break;
    case 2:
        localViewtab.xPos = 0;
        break;
    case 3:
        localViewtab.yPos = 37;  //Note: This is default horizon + 1 
        break;
    case 4:
        localViewtab.xPos = 160 - (localViewtab.xsize);
        break;
    }

    setViewTab(&localViewtab, 0);

    // Might need to stop motion of ego 
}

void b6DiscardResources()
{
    int i;
    for (i = 0; i < 256; i++) b9DiscardView(i);
    for (i = 0; i < 256; i++) b6DiscardPictureFile(i);
    for (i = 0; i < 256; i++) discardSoundFile(i);
}

void b6Clear()
{
    b6InitLayer1Mapbase();
    bEClearSpriteAttributes();
    b6ClearPicture();
}

/***************************************************************************
** new_room
**
** This performs the necessary actions for the AGI new_room command. For
** some reason this needs to be executed right at the end of the AGI cycle
** and not when it is first encountered. I have included new_room in the
** main module for this reason and also because it is one of the most
** important of the AGI commands.
***************************************************************************/
void b6NewRoom()
{
     b9ResetViews();
    //stop_update_all();
    //unanimate_all();
    b6DiscardResources();
    controlMode = PLAYER_CONTROL;
    //unblock();
    horizon = 36;
    var[1] = var[0];
    var[0] = newRoomNum;
    var[4] = 0;
    var[5] = 0;
    var[9] = 0;
    var[16] = 0;
    b6AdjustEgoPosition();
    var[2] = 0;
    flag[2] = 0;
    flag[5] = 1;
    score = var[3];

    memset(directions, 0, 9);
    /* rectfill(screen, 0, 20+(22*16), 639, 463, 0); */   /* Clear screen */
    b6Clear();
#ifdef VERBOSE
    printf("New room code called");
#endif // VERBOSE
}

/***************************************************************************
** updateStatusLine
**
** The status line shows the score and sound at the top of the screen.
***************************************************************************/
void b6UpdateStatusLine()
{
    //char scoreStr[256], soundStr[256];

    //if (statusLineDisplayed) {
    //    sprintf(scoreStr, "Score: %d of %d", var[3], var[7]);
    //    sprintf(soundStr, "Sound:%-3s", (flag[9] ? "on" : "off"));
    //    drawBigString(screen, scoreStr, 16, 0, 8, 1);
    //    drawBigString(screen, soundStr, 496, 0, 8, 1);
    //}
    //else {
    //    rectfill(screen, 0, 0, 639, 15, 0);   /* Clear status line */
    //}
}

/***************************************************************************
** interpret
**
** The main routine that gets called everytime the timing procedure is
** activated.
***************************************************************************/
void b6Interpret()
{
    ViewTable localViewtab;
    LOGICFile logicFile;
    LOGICEntry logicEntry;
    flag[2] = FALSE;   //The player has issued a command line
    flag[4] = FALSE;   //The 'said' command has accepted the input
    pollKeyboard();
    //if (controlMode == PROGRAM_CONTROL)
    //   dirnOfEgo = var[6];
    //else
    //   var[6] = dirnOfEgo;
    getLogicFile(&logicFile, 0);
    getLogicEntry(&logicEntry, 0);
    getViewTab(&localViewtab, 0);
    localViewtab.direction = var[6];
    setViewTab(&localViewtab, 0);

    bCCalcObjMotion();

    // <<-- Update status line here (score & sound)
    b6UpdateStatusLine();

    do {
        hasEnteredNewRoom = FALSE;
        exitAllLogics = FALSE;

        executeLogic(&logicEntry, 0);

#ifdef VERBOSE
        printf("Back To Meka");
#endif // VERBOSE
        //dirnOfEgo = var[6];
        getViewTab(&localViewtab, 0);
        localViewtab.direction = var[6];
        setViewTab(&localViewtab, 0);
        // <<-- Update status line here (score & sound)
        b6UpdateStatusLine();
        var[5] = 0;
        var[4] = 0;
        flag[5] = 0;
        flag[6] = FALSE;
        flag[12] = FALSE;
        if (!hasEnteredNewRoom) {
            bBUpdateObjects();
        }
        if (hasEnteredNewRoom) b6NewRoom();

    } while (hasEnteredNewRoom);
}

void b6Timing_proc()
{
    counter++;
    hund += 5;
    if (hund >= 100) { //One second has passed
        var[11]++;
        if (var[11] >= 60) {  //One minute has passed
            var[12]++;
            if (var[12] >= 60) { //One hour has passed
                var[13]++;
                if (var[13] >= 24) { //One day has passed 
                    var[14]++;
                    var[13] = 0;
                }
                var[12] = 0;
            }
            var[11] = 0;
        }
        hund = 0;
    }
}

void b6Closedown()
{
    discardObjects();
    discardWords();
}

extern void b6InitGraphics();
extern void b6InitIrq();
extern void b6InitInterpreter();
extern void b6TellMeTheAddressPlease();
void b6Initialise()
{
    int i;

    b6InitTimer(&b6Timing_proc);

    b4InitLruCaches(&b6DiscardLogicFile, &b9DiscardView);

    b6InitFiles();             /* Load resource directories */

    //// <<--  Determine exact version in here
    for (i = 0; i < 255; i++) {  /* Initialize variables and flags */
        var[i] = 0;
        flag[i] = FALSE;
    }
    flag[5] = TRUE;
    var[24] = 0x29;
    var[22] = 3;
    var[26] = 3;
    var[8] = 255;     /* Number of free 256 byte pages of memory */
    var[10] = 2;      /* Normal speed */

    ///* SQ2 patch. I don't know where these are set in the game. */
    ///* var[86] = 1; var[87] = 2; var[88] = 3; */

    b6InitLogics();

#ifdef VERBOSE
    printf("Logics Inited\n");
#endif

    b6InitPicture();
    initSound();

    b9InitViews();
    b9InitObjects();

    loadObjectFile();
    loadWords();
    initEvents();
    b6InitInterpreter();
    b6InitIrq();
    bEInitSpriteMemoryManager();

    asm("sei");
    b6InitGraphics();
    REENABLE_INTERRUPTS();

    horizon = 36;

    ///* Set up timer. The timer controls the interpreter speed. */
    counter = 0;

    b6InitFloatDivision();
    b6TellMeTheAddressPlease();
    bEInitPaletteManager();
}


#pragma code-name (pop)

void main()
{
    int ret, oldCount = 0;
    //chdir("..\\KQ1-2917");
    //chdir("..\\COMPILER\\NEW\\SAMPLE\\TEMPLATE");
    //chdir("\\GAMES\\SIERRA\\MH2");
    //chdir("\\GAMES\\SIERRA\\MH1");
    //chdir("\\GAMES\\SIERRA\\SQ2");
    //chdir("\\GAMES\\SIERRA\\LSL1");
    //chdir("..\\KQ2-2917");

    memoryMangerInit();
    b5CheckMemory();

    RAM_BANK = MEKA_BANK;
    b6Initialise();

    while (TRUE) {
        /* Cycle initiator. Controlled by delay variable (var[10). */
        if (counter >= var[10]) {
#ifdef VERBOSE
            printf("Interpret Runs\n");
#endif // VERBOSE
            b6Interpret();
            counter = 0;
        }
        b6CheckTimer(TIMER_WAIT_MS);
    }

    //chdir("\\HACK\\AGI\\D\\AGI\\MEKA");
    //closedown();
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