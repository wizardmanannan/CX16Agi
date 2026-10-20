#include <ascii_charmap.h>
#include "stringFormat.h"
#include <cx16.h>
#include "helpers.h"
#include "logic.h"
#include "object.h"

extern int numInputWords, currentLog;
extern char b7WordText[10][80], string[12][40];
extern char* getMessagePointer(byte logic, byte message);

/* Resident workspace: formatting runs synchronously, outside the display IRQ. */
const char* formatSource;
byte formatSourceBank;
char* formatDestination;
byte formatDestinationBank;
size_t formatRemaining;
byte formatBuffered, formatCode, formatIndex, formatInvalid;
char formatBuffer[50];
const char* formatText;
byte formatTextBank, formatRecursive;
extern void formatExpandAsm(void);

#pragma code-name (push, "CODE")
/* Metadata access keeps the engine's wrapped-call ABI in one place. */
void formatResolve(void)
{
    LOGICFile file;
    byte previousBank = RAM_BANK;
    byte logic;
    formatText = NULL;
    formatRecursive = 0;
    if (formatInvalid) return;
    switch (formatCode) {
    case '0':
        if (!formatIndex) break;
        RAM_BANK = OBJECT_BANK;
        if (formatIndex <= bDNumObjects && formatIndex <= MAX_OBJECTS)
            formatText = bDObjects[formatIndex - 1].name;
        formatTextBank = OBJECT_BANK;
        break;
    case 'w':
        if (formatIndex && formatIndex <= numInputWords && formatIndex <= 10)
            formatText = b7WordText[formatIndex - 1];
        formatTextBank = PARSER_BANK;
        break;
    case 's':
        if (formatIndex < 12) formatText = string[formatIndex];
        formatTextBank = PARSER_BANK;
        formatRecursive = 1;
        break;
    case 'g':
    case 'm':
        if (!formatIndex) break;
        logic = formatCode == 'g' ? 0 : (byte)currentLog;
        b5GetLogicFile(&file, logic);
        if (file.messages && formatIndex <= file.numMessages)
            formatText = getMessagePointer(logic, formatIndex - 1);
        formatTextBank = file.messageBank;
        formatRecursive = formatCode == 'm';
        break;
    }
    RAM_BANK = previousBank;
}

void formatFlush(void)
{
    if (!formatBuffered) return;
    memCpyBanked((byte*)formatDestination, (byte*)formatBuffer,
                formatDestinationBank, formatBuffered);
    formatDestination += formatBuffered;
    formatBuffered = 0;
}

void expandFormatCodes(const char* src, byte srcBank, char* dst, byte dstBank, size_t dstMax)
{
    byte previousBank = RAM_BANK;
    char zero = 0;
    if (!dstMax) return;
    formatSource = src;
    formatSourceBank = srcBank;
    formatDestination = dst;
    formatDestinationBank = dstBank;
    formatRemaining = dstMax - 1;
    formatBuffered = 0;
    formatExpandAsm();
    formatFlush();
    memCpyBanked((byte*)formatDestination, (byte*)&zero, dstBank, 1);
    RAM_BANK = previousBank;
}
#pragma code-name (pop)
