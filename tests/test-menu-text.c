#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"

byte RAM_BANK;
int currentLog = 106;
static byte* messages[2];
extern char menuTextBuffer[MENU_TEXT_BUFFER_SIZE];
extern char* nextMenuTextBufferAddr;
extern int numOfMenus;
extern MENU the_menu[];
extern char* bAStoreMessageInBuffer(LOGICFile* file, byte message);

void native_asm(const char* instruction, ...) { (void)instruction; }

void b5GetLogicFile(LOGICFile* result, byte number)
{
	assert(number == 106);
	memset(result, 0, sizeof(*result));
	result->messages = messages;
	result->numMessages = 2;
	result->messageBank = 21;
}

char* getMessagePointer(byte logic, byte message)
{
	assert(logic == 106 && message < 2);
	return (char*)messages[message];
}

size_t strLenBanked(char* text, int bank) { assert(bank == 21); return strlen(text); }

void memCpyBankedBetween(byte* dst, byte dstBank, byte* src, byte srcBank, size_t length)
{
	assert(dstBank == MENU_BANK && srcBank == 21);
	assert(dst >= (byte*)menuTextBuffer);
	assert(length <= (size_t)((byte*)menuTextBuffer + MENU_TEXT_BUFFER_SIZE - dst));
	memcpy(dst, src, length);
}

int main(void)
{
	LOGICFile file;
	char label[256];
	char* saved;
	int i;
	b5GetLogicFile(&file, 106);
	memset(label, 'x', 255);
	label[255] = '\0';
	messages[0] = (byte*)label;
	assert(bAStoreMessageInBuffer(&file, 1) == menuTextBuffer);
	assert(strcmp(menuTextBuffer, label) == 0);
	saved = nextMenuTextBufferAddr;
	assert(bAStoreMessageInBuffer(&file, 1) == NULL);
	assert(nextMenuTextBufferAddr == saved);
	nextMenuTextBufferAddr = menuTextBuffer + MENU_TEXT_BUFFER_SIZE;
	bASetMenu(1);
	assert(numOfMenus == 0);
	nextMenuTextBufferAddr = menuTextBuffer;
	messages[0] = (byte*)"Menu";
	for (i = 0; i < MAX_MENUS; i++) bASetMenu(1);
	assert(numOfMenus == MAX_MENUS);
	assert(the_menu[MAX_MENUS].text == NULL);
	saved = nextMenuTextBufferAddr;
	bASetMenu(1);
	assert(numOfMenus == MAX_MENUS && nextMenuTextBufferAddr == saved);
	puts("menu text tests passed");
	return 0;
}
