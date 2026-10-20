/* Production text-palette identity: every legal foreground/background pair. */
#include <assert.h>
#include <stdio.h>
#include "textLayer.h"
byte _assmByte;
long _assmLong;
static int expectedId;
void native_asm(const char* instruction, ...) { (void)instruction; }
byte bFGetPalette(int id, PaletteGetResult* result)
{
    if (id != expectedId) fprintf(stderr, "text palette ID: got %d, expected %d\n", id, expectedId);
    assert(id == expectedId);
    *result = AlreadyAllocated;
    return 4;
}
unsigned int b6SetPaletteToInt(byte reference) { return reference; }
int main(void)
{
    byte foreground, background;
    for (foreground = 0; foreground < 16; foreground++) {
        for (background = 0; background < 16; background++) {
            expectedId = BASE_TEXT_ID + foreground * 16 + background;
            assert(b3SetTextColor(foreground, background) == 4);
            assert(b3CurrentForegroundColour == foreground && b3CurrentBackgroundColour == background);
        }
    }
    puts("text palette identity cases passed");
    return 0;
}
