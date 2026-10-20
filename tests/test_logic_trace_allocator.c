#include "logicContext.h"
#include "memoryManager.h"

#include <string.h>

#define TRACE_ALLOC_TEST_TINY_BANK 1
#define TRACE_ALLOC_TEST_CANARY 2
#define TRACE_ALLOC_TEST_RING 3
#define TRACE_ALLOC_TEST_EVENT 4
#define TRACE_ALLOC_TEST_STORAGE 5
#define TRACE_ALLOC_TEST_SMALL_SLOT 6
#define TRACE_ALLOC_TEST_RUNTIME_BANK 7

#define TRACE_ALLOC_TEST_TRACE_CAPACITY 64u
/* 396 linker-emitted common BSS bytes plus the 1600-byte owned payload,
   including the replay tick and captured hardware/software stack pointers. */
#define TRACE_ALLOC_TEST_EXPECTED_STORAGE 1996u
#define TRACE_ALLOC_TEST_RUNTIME_BANK_VALUE 0x39

typedef char TraceEventSizeCheck[sizeof(LogicTraceEvent) == 25 ? 1 : -1];

typedef byte *(*BankedAllocFunction)(int size, byte *bank);

volatile byte test_result;
volatile unsigned int test_context_storage_bytes;
volatile unsigned int test_context_hardware_stack_bytes;
volatile unsigned int test_context_software_stack_bytes;
volatile unsigned int test_logic_entry_size;
volatile unsigned int test_logic_file_size;
volatile unsigned int test_trace_event_size;
volatile word test_diag_a_window;
volatile byte test_trace_corrupt_slot;

extern byte *b10BankedAlloc(int size, byte *bank);

extern word startPos, endPos, cwCurrentCode;
extern byte codeBank, codeWindowInvalid;
extern LOGICEntry assmLogicEntry;
extern LOGICFile assmLogicFile;
extern int currentLog;
extern boolean exitAllLogics;

word startPos, endPos, cwCurrentCode;
byte codeBank, codeWindowInvalid;
LOGICEntry assmLogicEntry;
LOGICFile assmLogicFile;
int currentLog;
boolean exitAllLogics;
long opCounter;

static volatile byte *tiny_slots[TINY_NO_SEGMENTS];
static byte tiny_banks[TINY_NO_SEGMENTS];
static volatile byte *const ram_bank_register = (volatile byte *)0x0000;

static byte canary_value(byte slot, byte offset)
{
    return (byte)(0x31u + (unsigned int)slot * 13u +
                  (unsigned int)offset * 7u);
}

static byte *allocate_banked(int size, byte *bank)
{
    BankedAllocFunction allocate = b10BankedAlloc;
    return allocate(size, bank);
}

static byte check_tiny_canaries(void)
{
    byte previous_bank = *ram_bank_register;
    byte slot, offset;

    for (slot = 0; slot < TINY_NO_SEGMENTS; ++slot) {
        *ram_bank_register = tiny_banks[slot];
        for (offset = 0; offset < TINY_SIZE; ++offset) {
            if (tiny_slots[slot][offset] != canary_value(slot, offset)) {
                test_trace_corrupt_slot = slot;
                test_diag_a_window = slot;
                *ram_bank_register = previous_bank;
                return FALSE;
            }
        }
    }
    *ram_bank_register = previous_bank;
    return TRUE;
}

int main(void)
{
    byte i, offset, bank;
    byte previous_bank = *ram_bank_register;
    byte *next_small;
    LogicTraceEvent event;

    test_result = 0;
    test_trace_corrupt_slot = 0xFF;
    test_context_storage_bytes = 0;
    test_context_hardware_stack_bytes = 0;
    test_context_software_stack_bytes = 0;
    test_logic_entry_size = sizeof(LOGICEntry);
    test_logic_file_size = sizeof(LOGICFile);
    test_trace_event_size = sizeof(LogicTraceEvent);
    test_diag_a_window = 0;

    /* Seed the actual allocator's bank-10 metadata before reserving every
       bank-$13 tiny slot. The trace test then treats each allocated slot as
       live game data and checks it after production trace writes. */
    *ram_bank_register = MEMORY_MANAGEMENT_BANK;
    b10InitDynamicMemory();
    for (i = 0; i < TINY_NO_SEGMENTS; ++i) {
        tiny_slots[i] = allocate_banked(TINY_SIZE, &bank);
        tiny_banks[i] = bank;
        if (!tiny_slots[i] || bank != TINY_FIRST_BANK ||
            (word)tiny_slots[i] != (word)(BANK_MIN +
                (unsigned int)i * TINY_SIZE)) {
            *ram_bank_register = previous_bank;
            test_result = TRACE_ALLOC_TEST_TINY_BANK;
            return test_result;
        }
        *ram_bank_register = bank;
        for (offset = 0; offset < TINY_SIZE; ++offset)
            tiny_slots[i][offset] = canary_value(i, offset);
        *ram_bank_register = MEMORY_MANAGEMENT_BANK;
    }

    /* These are the same globals written by the instrumented interpreter
       immediately before it records a variable write. */
    currentLog = 17;
    codeBank = 42;
    logicContextTick = 0x12345678UL;
    logicContextInstructionPc = 0x3456;
    logicContextOpcode = 1;
    logicContextWriteField = 9;
    logicContextWriteBefore = 0xA5;
    logicContextWriteAfter = 0x5A;
    *ram_bank_register = TRACE_ALLOC_TEST_RUNTIME_BANK_VALUE;

    for (i = 0; i < 80; ++i)
        logicContextRecordPendingWrite();

    if (*ram_bank_register != TRACE_ALLOC_TEST_RUNTIME_BANK_VALUE) {
        test_result = TRACE_ALLOC_TEST_RUNTIME_BANK;
        return test_result;
    }

    if (!check_tiny_canaries()) {
        test_result = TRACE_ALLOC_TEST_CANARY;
        return test_result;
    }
    if (logicContextTraceCount() != TRACE_ALLOC_TEST_TRACE_CAPACITY ||
        logicContextTraceDropped() != 16) {
        test_result = TRACE_ALLOC_TEST_RING;
        return test_result;
    }
    if (!logicContextTraceGet(0, &event) || event.sequence != 16 ||
        event.tick != 0x12345678UL || event.kind != LOGIC_TRACE_KIND_WRITE ||
        event.logic != 17 || event.logical_pc != 0x3456 || event.opcode != 1 ||
        event.bank != 42 || event.field != 9 || event.before != 0xA5 ||
        event.after != 0x5A) {
        test_result = TRACE_ALLOC_TEST_EVENT;
        return test_result;
    }
    if (*ram_bank_register != TRACE_ALLOC_TEST_RUNTIME_BANK_VALUE) {
        test_result = TRACE_ALLOC_TEST_RUNTIME_BANK;
        return test_result;
    }

    test_context_storage_bytes = logicContextStorageBytes();
    if (test_context_storage_bytes != TRACE_ALLOC_TEST_EXPECTED_STORAGE) {
        test_result = TRACE_ALLOC_TEST_STORAGE;
        return test_result;
    }

    /* A 1600-byte trace reservation consumes exactly one SMALL segment. */
    *ram_bank_register = MEMORY_MANAGEMENT_BANK;
    next_small = allocate_banked(SMALL_SIZE, &bank);
    *ram_bank_register = previous_bank;
    if (!next_small || bank != SMALL_FIRST_BANK ||
        (word)next_small != (word)(BANK_MIN + SMALL_SIZE)) {
        test_result = TRACE_ALLOC_TEST_SMALL_SLOT;
        return test_result;
    }

    return 0;
}
