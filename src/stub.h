#ifndef _STUB_H
#define _STUB_H
#include "general.h"
#include "memoryManager.h"
#include <cx16.h>

//A temporary file so that references to yet to be imported files can resolved

//allgero
void stop_midi();
void show_mouse(BITMAP* bmp);
void nosound();
extern void stretch_sprite(BITMAP* bmp, BITMAP* sprite, int x, int y, int w, int h);
extern void clear_to_color(BITMAP* bitmap, int color);
extern void stretch_blit(BITMAP* s, BITMAP* d, int s_x, int s_y, int s_w, int s_h, int d_x, int d_y, int d_w, int d_h);

int keypressed();
extern BITMAP* create_bitmap(int width, int height);
extern void rect(BITMAP* bmp, int x1, int y1, int x2, int y2, int color);


typedef struct MENU
{
	char* text;                   /* menu item text */
	byte menuTextBank;
	int (*proc)(void);            /* callback function */
	int flags;                    /* flags about the menu state */
	void* dp;                     /* any data the menu might require */
} MENU;

int readkey();

#define DIGI_VOICES           32       /* Theoretical maximums: */
#define MIDI_VOICES           32       /* actual drivers may not be */
#define MIDI_TRACKS           32       /* able to handle this many */

extern BITMAP* screen;
void clear(BITMAP* bitmap);
void rectfill(BITMAP* bmp, int x1, int y1, int x2, int y2, int color);
void blit(BITMAP* source, BITMAP* dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
void clear_keybuf();
BITMAP* create_bitmap(int width, int height);

typedef struct MIDI                    /* a midi file */
{
	int divisions;                      /* number of ticks per quarter note */
	struct {
		unsigned char* data;             /* MIDI message stream */
		int len;                         /* length of the track data */
	} track[MIDI_TRACKS];
} MIDI;

void destroy_bitmap(BITMAP* bitmap);
int do_menu(MENU* menu, int x, int y);

//Agi Codes
#define MAX_NAME_LENGTH    20
#define NUM_TEST_COMMANDS  19
#define NUM_AGI_COMMANDS   182

typedef struct {
	char commandName[MAX_NAME_LENGTH];
	int numArgs;
	int argTypeMask;
} agiCommandType;


extern agiCommandType testCommands[];

//Picture
//#define  AGI_GRAPHICS  0
//#define  AGI_TEXT      1
//
//
//extern BITMAP * priority, * control, * agi_screen, * working_screen;
//
//extern int screenMode;

//Sound

void loadSoundFile(int soundNum);

typedef struct {
	int loaded;
	MIDI* data;
} SoundFile;

extern boolean checkForEnd;
void discardSoundFile(int soundNum);
void initSound();

extern SoundFile loadedSounds[];
extern boolean checkForEnd;
extern int soundEndFlag;

//View

//Logic 
extern byte directions[9];


#define SCANCODE_TO_KEY(c)       (((c)<<8) + (int)key_ascii_table[c])
#define SCANCODE_TO_CAPS(c)      (((c)<<8) + (int)key_capslock_table[c])
#define SCANCODE_TO_SHIFT(c)     (((c)<<8) + (int)key_shift_table[c])
#define SCANCODE_TO_CONTROL(c)   (((c)<<8) + (int)key_control_table[c])
#define SCANCODE_TO_ALTGR(c)     (((c)<<8) + (int)key_altgr_table[c])
#define SCANCODE_TO_ALT(c)       ((c)<<8)

#define KB_SHIFT_FLAG         0x0001
#define KB_CTRL_FLAG          0x0002
#define KB_ALT_FLAG           0x0004
#define KB_LWIN_FLAG          0x0008
#define KB_RWIN_FLAG          0x0010
#define KB_MENU_FLAG          0x0020
#define KB_SCROLOCK_FLAG      0x0100
#define KB_NUMLOCK_FLAG       0x0200
#define KB_CAPSLOCK_FLAG      0x0400
#define KB_INALTSEQ_FLAG      0x0800
#define KB_ACCENT1_FLAG       0x1000
#define KB_ACCENT1_S_FLAG     0x2000
#define KB_ACCENT2_FLAG       0x4000
#define KB_ACCENT2_S_FLAG     0x8000

#define KB_NORMAL             1
#define KB_EXTENDED           2

#define KEY_ESC               1     /* keyboard scan codes  */
#define KEY_1                 2 
#define KEY_2                 3 
#define KEY_3                 4
#define KEY_4                 5
#define KEY_5                 6
#define KEY_6                 7
#define KEY_7                 8
#define KEY_8                 9
#define KEY_9                 10
#define KEY_0                 11
#define KEY_MINUS             12
#define KEY_EQUALS            13
#define KEY_BACKSPACE         14
#define KEY_TAB               15 
#define KEY_Q                 16
#define KEY_W                 17
#define KEY_E                 18
#define KEY_R                 19
#define KEY_T                 20
#define KEY_Y                 21
#define KEY_U                 22
#define KEY_I                 23
#define KEY_O                 24
#define KEY_P                 25
#define KEY_OPENBRACE         26
#define KEY_CLOSEBRACE        27
#define KEY_ENTER             28
#define KEY_CONTROL           29
#define KEY_LCONTROL          29
#define KEY_A                 30
#define KEY_S                 31
#define KEY_D                 32
#define KEY_F                 33
#define KEY_G                 34
#define KEY_H                 35
#define KEY_J                 36
#define KEY_K                 37
#define KEY_L                 38
#define KEY_COLON             39
#define KEY_QUOTE             40
#define KEY_TILDE             41
#define KEY_LSHIFT            42
#define KEY_BACKSLASH         43
#define KEY_Z                 44
#define KEY_X                 45
#define KEY_C                 46
#define KEY_V                 47
#define KEY_B                 48
#define KEY_N                 49
#define KEY_M                 50
#define KEY_COMMA             51
#define KEY_STOP              52
#define KEY_SLASH             53
#define KEY_RSHIFT            54
#define KEY_ASTERISK          55
#define KEY_ALT               56
#define KEY_SPACE             57
#define KEY_CAPSLOCK          58
#define KEY_F1                59
#define KEY_F2                60
#define KEY_F3                61
#define KEY_F4                62
#define KEY_F5                63
#define KEY_F6                64
#define KEY_F7                65
#define KEY_F8                66
#define KEY_F9                67
#define KEY_F10               68
#define KEY_NUMLOCK           69
#define KEY_SCRLOCK           70
#define KEY_HOME              71
#define KEY_UP                72
#define KEY_PGUP              73
#define KEY_MINUS_PAD         74
#define KEY_LEFT              75
#define KEY_5_PAD             76
#define KEY_RIGHT             77
#define KEY_PLUS_PAD          78
#define KEY_END               79
#define KEY_DOWN              80
#define KEY_PGDN              81
#define KEY_INSERT            82
#define KEY_DEL               83
#define KEY_PRTSCR            84
#define KEY_F11               87
#define KEY_F12               88
#define KEY_LWIN              91
#define KEY_RWIN              92
#define KEY_MENU              93
#define KEY_PAD               100
#define KEY_RCONTROL          120
#define KEY_ALTGR             121
#define KEY_SLASH2            122
#define KEY_PAUSE             123

#define SEND_MESSAGE(d, msg, c)  (d)->proc(msg, d, c)

/* bits for the flags field */
#define D_EXIT          1        /* object makes the dialog exit */
#define D_SELECTED      2        /* object is selected */
#define D_GOTFOCUS      4        /* object has the input focus */
#define D_GOTMOUSE      8        /* mouse is on top of object */
#define D_HIDDEN        16       /* object is not visible */
#define D_DISABLED      32       /* object is visible but inactive */
#define D_INTERNAL      64       /* reserved for internal use */
#define D_USER          128      /* from here on is free for your own use */

/* return values for the dialog procedures */
#define D_O_K           0        /* normal exit status */
#define D_CLOSE         1        /* request to close the dialog */
#define D_REDRAW        2        /* request to redraw the dialog */
#define D_WANTFOCUS     4        /* this object wants the input focus */
#define D_USED_CHAR     8        /* object has used the keypress */

/* messages for the dialog procedures */
#define MSG_START       1        /* start the dialog, initialise */
#define MSG_END         2        /* dialog is finished - cleanup */
#define MSG_DRAW        3        /* draw the object */
#define MSG_CLICK       4        /* mouse click on the object */
#define MSG_DCLICK      5        /* double click on the object */
#define MSG_KEY         6        /* keyboard shortcut */
#define MSG_CHAR        7        /* other keyboard input */
#define MSG_XCHAR       8        /* broadcast character to all objects */
#define MSG_WANTFOCUS   9        /* does object want the input focus? */
#define MSG_GOTFOCUS    10       /* got the input focus */
#define MSG_LOSTFOCUS   11       /* lost the input focus */
#define MSG_GOTMOUSE    12       /* mouse on top of object */
#define MSG_LOSTMOUSE   13       /* mouse moved away from object */
#define MSG_IDLE        14       /* update any background stuff */
#define MSG_RADIO       15       /* clear radio buttons */
#define MSG_USER        16       /* from here on are free... */





//Graphics
extern void drawString(BITMAP* scn, char* data, int x, int y, int foreColour, int backColour);
void drawBigString(BITMAP* scn, char* data, int x, int y, int foreColour, int backColour);
extern void drawChar(BITMAP* scn, byte charNum, int x, int y, int foreColour, int backColour);
#endif

//Parser
boolean said(byte** data);

void getString(char* promptStr, char* returnStr, int x, int y, int l);

extern char cursorChar;

#define  NO_EVENT         0
#define  ASCII_KEY_EVENT  1
#define  SCAN_KEY_EVENT   2
#define  MENU_EVENT       3

extern int numInputWords, inputWords[];
extern char wordText[10][80];

extern byte keyState[], asciiState[];
extern int lastKey;


extern byte directions[9];
extern boolean haveKey;

void pollKeyboard();
void initEvents();

void lookupWords(char* inputLine);

//Object
void loadObjectFile();

//Words
void loadWords();
void discardWords();

//Unknown 
extern byte* key;

//Objects
extern int numObjects;

extern void discardObjects();

//Text
void printInBoxBig(char* theString, int x, int y, int lineLen);

void printInBox(char* theString);

void printInBoxBig2(char* theString);
