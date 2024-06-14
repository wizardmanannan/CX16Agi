#include "stub.h"

//A temporary file so that references to yet to be imported files can resolved

//Allegro
void nosound()
{

}

void clear_to_color(BITMAP* bitmap, int color)
{
}

void stretch_blit(BITMAP* s, BITMAP* d, int s_x, int s_y, int s_w, int s_h, int d_x, int d_y, int d_w, int d_h)
{

}

void show_mouse(BITMAP* bmp) {

}

void blit(BITMAP* source, BITMAP* dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
}

void clear_keybuf()
{

}

BITMAP* create_bitmap(int width, int height)
{

}

void destroy_bitmap(BITMAP* bitmap)
{
}

int do_menu(MENU* menu, int x, int y)
{
	return 0;
}

void clear(BITMAP* bitmap)
{
}

void rectfill(BITMAP* bmp, int x1, int y1, int x2, int y2, int color)
{
}

//Graphics
void drawString(BITMAP* scn, char* data, int x, int y, int foreColour, int backColour)
{

}

void drawChar(BITMAP* scn, byte charNum, int x, int y, int foreColour, int backColour)
{

}

void drawBigString(BITMAP* scn, char* data, int x, int y, int foreColour, int backColour)
{

}

//allgero
BITMAP* screen;
void rect(BITMAP* bmp, int x1, int y1, int x2, int y2, int color)
{

}

void stretch_sprite(BITMAP* bmp, BITMAP* sprite, int x, int y, int w, int h)
{

}

//Sound
int soundEndFlag;

boolean checkForEnd = FALSE;
extern SoundFile loadedSounds[];
extern boolean checkForEnd;
extern int soundEndFlag;

void stop_midi()
{
}


void initSound() {

}

void discardSoundFile(int soundNum)
{

}


void loadSoundFile(int soundNum) {

}

//Text
void printInBoxBig2(char* theString) {

}

void printInBoxBig(char* theString, int x, int y, int lineLen)
{
}

void printInBox(char* theString) {

}

//unknown
byte* key;
