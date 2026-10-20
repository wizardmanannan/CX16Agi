#include "stringFormat.h"
#include <cx16.h>
#include "logic.h"
#include "object.h"

/* The assembly object lookup relies on the packed cc65 object ABI. */
typedef char formatObjectLayoutMustMatchAssembly[
    sizeof(objectType) == 5 && offsetof(objectType, name) == 1 && MAX_OBJECTS == 100 ? 1 : -1];
extern byte formatIndex, formatTextBank;

#pragma code-name (push, "CODE")
/* Keep the engine's typed, wrapped-call metadata ABI at this single boundary. */
char* __fastcall__ formatMessage(byte logic)
{
    LOGICFile file;
    char* text = NULL;
    byte previousBank = RAM_BANK;
    b5GetLogicFile(&file, logic);
    formatTextBank = file.messageBank;
    if (file.messages && formatIndex && formatIndex <= file.numMessages) {
        RAM_BANK = file.messageBank;
        text = (char*)file.messages[formatIndex - 1];
    }
    RAM_BANK = previousBank;
    return text;
}
#pragma code-name (pop)
