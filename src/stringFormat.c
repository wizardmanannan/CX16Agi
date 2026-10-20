#include "stringFormat.h"
#include "logic.h"
#include "object.h"

/* Keep assembly offsets tied to the compiler's packed engine ABI. */
typedef char formatObjectLayoutMustMatchAssembly[
    sizeof(objectType) == 5 && offsetof(objectType, name) == 1 &&
    MAX_OBJECTS == 100 ? 1 : -1];
typedef char formatLogicLayoutMustMatchAssembly[
    sizeof(LOGICFile) == 9 && offsetof(LOGICFile, numMessages) == 4 &&
    offsetof(LOGICFile, messages) == 5 && offsetof(LOGICFile, messageBank) == 8 &&
    LOGIC_BANK == 5 ? 1 : -1];
