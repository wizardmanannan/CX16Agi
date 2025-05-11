#ifndef _STUB_H
#define _STUB_H
#include "general.h"
#include "memoryManager.h"
#include <cx16.h>

//A temporary file so that references to yet to be imported files can resolved

void show_mouse(BITMAP* bmp);


typedef struct MENU
{
	char* text;                   /* menu item text */
	byte menuTextBank;
	int (*proc)(void);            /* callback function */
	int flags;                    /* flags about the menu state */
	void* dp;                     /* any data the menu might require */
} MENU;

#define DIGI_VOICES           32       /* Theoretical maximums: */
#define MIDI_VOICES           32       /* actual drivers may not be */
#define MIDI_TRACKS           32       /* able to handle this many */

extern BITMAP* screen;

typedef struct MIDI                    /* a midi file */
{
	int divisions;                      /* number of ticks per quarter note */
	struct {
		unsigned char* data;             /* MIDI message stream */
		int len;                         /* length of the track data */
	} track[MIDI_TRACKS];
} MIDI;

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

//Picture
//#define  AGI_GRAPHICS  0
//#define  AGI_TEXT      1
//
//
//extern BITMAP * priority, * control, * agi_screen, * working_screen;
//
//extern int screenMode;

//View

//Logic 
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

#endif

#define  NO_EVENT         0
#define  ASCII_KEY_EVENT  1
#define  SCAN_KEY_EVENT   2
#define  MENU_EVENT       3

//Unknown 
extern byte* key;


//Text
void printInBoxBig(char* theString, int x, int y, int lineLen);

void printInBox(char* theString);

void printInBoxBig2(char* theString);
