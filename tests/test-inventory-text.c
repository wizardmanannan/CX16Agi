/* Production inventory composition with bank/keyboard I/O at native seams. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "object.h"

static byte work[LOCAL_WORK_AREA_SIZE];
static byte* writePointer;
static BufferStatus* testBufferStatusPointer;
static char* namePointer;
#undef GOLDEN_RAM_WORK_AREA
#define GOLDEN_RAM_WORK_AREA work
#undef ZP_TMP_10
#define ZP_TMP_10 (&writePointer)
#undef ZP_TMP_12
#define ZP_TMP_12 (&testBufferStatusPointer)
#undef ZP_TMP_13
#define ZP_TMP_13 (&namePointer)
int cbm_read(byte channel, void* buffer, unsigned int count);
void cbm_close(byte channel);
#include "../src/object.c"

byte RAM_BANK, _assmByte, b3TextModeTileByte;
byte bCSplitBuffer[SPLIT_BUFFER_SIZE];
byte variables[256];
byte* var = variables;
boolean inputLineDisplayed;
static unsigned int emitted, pages, itemsOnPage, totalItems, inputPolls;
static boolean seen[MAX_OBJECTS];
static char names[MAX_OBJECTS][256];
static boolean longNames, selecting;

void native_asm(const char* instruction, ...)
{
    if (strcmp(instruction, "jsr %w") == 0) {
        assert(++inputPolls <= 4);
        if (selecting && totalItems == MAX_OBJECTS) {
            static const byte keys[] = { KEY_PGDN, KEY_PGUP, KEY_PGDN, KEY_ENTER };
            _assmByte = keys[inputPolls - 1];
        } else _assmByte = pages < (totalItems + 51) / 52 ? KEY_PGDN : selecting ? KEY_ENTER : KEY_ESC;
    }
}
void b6SetBackgroundColour(unsigned int color) { (void)color; }
void b6TextMode(void) {}
void b6GraphicsMode(void) {}
void b6SetAndWaitForIrqState(IRQ_COMMAND state) { (void)state; }
void* memCpyBanked(byte* destination, byte* source, byte bank, size_t length)
{
    (void)bank;
    return memcpy(destination, source, length);
}
void b5FlushBuffer(BufferStatus* status)
{
    unsigned int offset = status->bufferCounter * LOCAL_WORK_AREA_SIZE;
    assert(offset + sizeof(work) <= sizeof(bCSplitBuffer));
    memcpy(status->bankedData + offset, work, sizeof(work));
    status->bufferCounter++;
}
void bDWriteNext(byte ch)
{
    assert(++emitted < sizeof(bCSplitBuffer));
    if (writePointer == work + sizeof(work)) {
        b5FlushBuffer(testBufferStatusPointer);
        writePointer = work;
    }
    *writePointer++ = ch;
}
void bDDisplayInventoryInner(byte length)
{
    unsigned int id = (namePointer[0] - '0') * 10 + namePointer[1] - '0';
    assert(id < totalItems && length == (longNames ? 20 : 2));
    seen[id] = TRUE;
    itemsOnPage++;
    while (length-- && *namePointer) bDWriteNext(*namePointer++);
}
byte bDPadWordsWithSpaces(byte number, byte previousLength)
{
    (void)number; (void)previousLength;
    return 0; /* Spacing does not affect item or padding-row capacity. */
}
void b3DisplayMessageBox(char* text, byte bank, byte row, byte column,
                       byte palette, byte width, boolean wrap, byte firstRow)
{
    const char* p;
    unsigned int rows = 1;
    (void)row; (void)column; (void)palette; (void)width; (void)wrap;
    assert(bank == SPLIT_BANK && firstRow == FIRST_OBJECT_ROW);
    assert(memchr(text, 0, sizeof(bCSplitBuffer)));
    for (p = text; *p; p++) if (*p == '\n') rows++;
    assert(rows <= MAX_OBJECT_ROWS && itemsOnPage <= 52);
    pages++;
    itemsOnPage = emitted = 0;
}
static void inventoryCase(unsigned int count, boolean longLabels, boolean choose)
{
    unsigned int i;
    memset(seen, 0, sizeof(seen));
    pages = emitted = itemsOnPage = inputPolls = 0;
    totalItems = count;
    longNames = longLabels;
    selecting = choose;
    bDNumObjects = count;
    for (i = 0; i < count; i++) {
        snprintf(names[i], sizeof(names[i]), "%02u", i);
        if (longNames) {
            memset(names[i] + 2, 'x', 253);
            names[i][255] = 0;
        }
        bDObjects[i].name = names[i];
        bDObjects[i].roomNum = HAS_OBJ;
        bDObjects[i].objectNum = i;
        bDObjectNameLengths[i] = longNames ? 255 : 2;
    }
    var[25] = 0xee;
    bDDisplayInventory(choose);
    assert(pages == (choose && count == MAX_OBJECTS ? 4 : count ? (count + 51) / 52 : 1));
    for (i = 0; i < count; i++) {
        assert(seen[i] && bDObjects[i].roomNum == HAS_OBJ);
        assert(bDObjectNameLengths[i] == (longNames ? 255 : 2));
    }
    if (choose) assert(var[25] == (count ? (count - 1) / 52 * 52 : 0xff));
}
int main(void)
{
    inventoryCase(0, FALSE, FALSE);
    inventoryCase(0, FALSE, TRUE);
    inventoryCase(1, FALSE, FALSE);
    inventoryCase(52, FALSE, FALSE);
    inventoryCase(54, FALSE, FALSE);
    inventoryCase(61, FALSE, FALSE);
    inventoryCase(MAX_OBJECTS, FALSE, FALSE);
    inventoryCase(MAX_OBJECTS, TRUE, TRUE);
    puts("inventory text capacity cases passed");
    return 0;
}
