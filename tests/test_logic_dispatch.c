#include "logicTypes.h"
#include "logicContext.h"

#include <string.h>

#ifndef LOGIC_CONTEXT_VERIFY
/* Run the same behavioral cases with all verifier code compiled out. */
#define logicContextVerifyFailed 0
#endif

#define LOGIC_COUNT 3

volatile byte test_result;
word test_step_count;
LOGICEntry assmLogicEntry;
LOGICFile assmLogicFile;
byte exitAllLogics, hasEnteredNewRoom;
int newRoomNum, currentLog;
byte test_restart_backend_calls;
byte numInputWords;
word inputWords[16];
byte logicEntryAddressesLow[256], logicEntryAddressesHigh[256];
byte b6RandomNumbers[256];
extern const byte logicContextCodeBytes;
LOGICEntry *logics[256];

static LOGICFile logic_files[LOGIC_COUNT];
static LOGICEntry logic_entries[LOGIC_COUNT];

LOGICEntry expected_entry_a, expected_entry_b;
LOGICFile expected_file_a, expected_file_b;
LOGICEntry observed_entry_a, observed_entry_b;
LOGICFile observed_file_a, observed_file_b;
byte expected_count_a, expected_count_b;
byte observed_count_a, observed_count_b;
unsigned int test_context_storage_bytes;
unsigned int test_context_hardware_stack_bytes;
unsigned int test_context_software_stack_bytes;
unsigned int test_logic_entry_size;
unsigned int test_logic_file_size;
unsigned int test_trace_event_size;
word test_diag_a_window;

word expected_a_start, expected_a_end, expected_a_pc;
word expected_a_entry_ptr, expected_a_file_ptr;
byte expected_a_code_bank, expected_a_ram_bank, expected_a_window_invalid;
byte expected_a_window_first;
word observed_a_start, observed_a_end, observed_a_pc;
word observed_a_entry_ptr, observed_a_file_ptr;
byte observed_a_code_bank, observed_a_ram_bank, observed_a_window_invalid;
byte observed_a_window_first;

word expected_b_start, expected_b_end, expected_b_pc;
word expected_b_entry_ptr, expected_b_file_ptr;
byte expected_b_code_bank, expected_b_ram_bank, expected_b_window_invalid;
byte expected_b_window_first;
word observed_b_start, observed_b_end, observed_b_pc;
word observed_b_entry_ptr, observed_b_file_ptr;
byte observed_b_code_bank, observed_b_ram_bank, observed_b_window_invalid;
byte observed_b_window_first;

word top_start, top_end, top_pc, top_entry_ptr, top_file_ptr;
byte top_code_bank, top_ram_bank, top_window_invalid, top_window_first;
LOGICEntry top_entry;
LOGICFile top_file;

extern void test_nested_dispatch(void);
extern void test_snapshot_machine_state(void);
extern void test_capture_machine_state(void);
extern void logicContextTestCorruptDepth(void);
extern void logicContextTestCorruptGeneration(void);
extern void logicContextTestCorruptAbandonment(void);
extern void logicContextTestClearMismatch(void);
extern word startPos, endPos, cwCurrentCode;
extern byte codeBank, codeWindowInvalid;

static byte *golden_ram(void)
{
    return (byte *)0x0400;
}

static void init_logic_tables(void)
{
    static const word starts[LOGIC_COUNT] = {0xA000, 0xA100, 0xA200};
    static const byte banks[LOGIC_COUNT] = {40, 41, 42};
    static const word entries[LOGIC_COUNT] = {4, 7, 9};
    byte i;
    for (i = 0; i < LOGIC_COUNT; ++i) {
        logic_files[i].codeSize = 80;
        logic_files[i].logicCode = (byte *)starts[i];
        logic_files[i].numMessages = 0;
        logic_files[i].messages = (byte **)0;
        logic_files[i].codeBank = banks[i];
        logic_files[i].messageBank = 0;

        logic_entries[i].loaded = TRUE;
        logic_entries[i].entryPoint = entries[i];
        logic_entries[i].currentPoint = 0;
        logic_entries[i].data = &logic_files[i];
        logic_entries[i].dataBank = banks[i];
        logic_entries[i].isLogicZeroOrDependency = FALSE;
    }
}

void b5GetLogicFile(LOGICFile *destination, byte logic_no)
{
    if (logic_no < LOGIC_COUNT)
        *destination = logic_files[logic_no];
}

void b5GetLogicEntry(LOGICEntry *destination, byte logic_no)
{
    if (logic_no < LOGIC_COUNT)
        *destination = logic_entries[logic_no];
}

void b5SetLogicEntry(LOGICEntry *source, byte logic_no)
{
    if (logic_no < LOGIC_COUNT)
        logic_entries[logic_no] = *source;
}

void b6LoadLogicFile(byte logic_no, boolean load_dependencies)
{
    (void)logic_no;
    (void)load_dependencies;
}

/* The restart command backend signals the production command loop to abandon
   the current logic chain; the interpreter dispatch itself is not stubbed. */
void b4Restart_game(void)
{
    ++test_restart_backend_calls;
    exitAllLogics = TRUE;
    hasEnteredNewRoom = TRUE;
    newRoomNum = 0;
}

void test_snapshot_call(void)
{
    if ((byte)currentLog == 0) {
        expected_entry_a = assmLogicEntry;
        expected_file_a = assmLogicFile;
        ++expected_count_a;
    } else if ((byte)currentLog == 1) {
        expected_entry_b = assmLogicEntry;
        expected_file_b = assmLogicFile;
        ++expected_count_b;
    }
    test_snapshot_machine_state();
}

void test_capture_nested_return(void)
{
    if ((byte)currentLog == 0) {
        observed_entry_a = assmLogicEntry;
        observed_file_a = assmLogicFile;
        ++observed_count_a;
    } else if ((byte)currentLog == 1) {
        observed_entry_b = assmLogicEntry;
        observed_file_b = assmLogicFile;
        ++observed_count_b;
    }
    test_capture_machine_state();
}

#define CHECK(condition, result) \
    do { if (!(condition)) { test_result = (result); goto finished; } } while (0)

int main(void)
{
    byte *ram = golden_ram();

    test_result = 0;
    test_step_count = 0;
    test_diag_a_window = 0;
    expected_count_a = expected_count_b = 0;
    observed_count_a = observed_count_b = 0;
    init_logic_tables();
    memset(ram, 0, 512);

    /* The bridge enters A with a sentinel caller frame, then real opcode
       dispatch runs A -> B -> C and returns through the production callLogic. */
    memset(&assmLogicEntry, 0xA5, sizeof assmLogicEntry);
    memset(&assmLogicFile, 0x5A, sizeof assmLogicFile);
    top_entry = assmLogicEntry;
    top_file = assmLogicFile;
    ram[100] = 1;
#ifdef LOGIC_DISPATCH_BANK2_RETURN_ONLY
    /* This focused case dispatches a call.v from a real bank-2 handler into
       a child whose new.room abandons both production interpreter frames. */
    {
        volatile byte *logic_a = (volatile byte *)0xA000;
        volatile byte *logic_b = (volatile byte *)0xA100;
        volatile byte *ram_bank_register = (volatile byte *)0;
        byte previous_bank = *ram_bank_register;
        byte i;
        *ram_bank_register = 40;
        for (i = 0; i < 80; ++i)
            logic_a[i] = 0;
        logic_a[4] = 23; /* call.v logic in variable 100 */
        logic_a[5] = 100;
        *ram_bank_register = 41;
        for (i = 0; i < 80; ++i)
            logic_b[i] = 0;
        logic_b[7] = 18; /* new.room 9 */
        logic_b[8] = 9;
        *ram_bank_register = previous_bank;
    }
    exitAllLogics = FALSE;
    hasEnteredNewRoom = FALSE;
    newRoomNum = 0;
    observed_a_ram_bank = 0xFF;
    test_nested_dispatch();
    CHECK(hasEnteredNewRoom == TRUE && exitAllLogics == TRUE &&
          newRoomNum == 9,
          68);
    CHECK(observed_count_a == 1 && observed_a_ram_bank == 2, 69);
    CHECK(top_ram_bank == 6, 70);
    CHECK(logicContextDepth() == 0 &&
          logicContextError() == LOGIC_CONTEXT_OK &&
          logicContextVerifyFailed == FALSE,
          71);
    goto finished;
#elif defined(LOGIC_DISPATCH_DIRECT_ONLY)
    /* Run the isolated direct A -> B -> A case in its own fresh emulator
       instance, independent of the deeper A -> B -> C -> B -> A case. */
    {
        volatile byte *window = (volatile byte *)0xA100;
        volatile byte *ram_bank_register = (volatile byte *)0;
        byte previous_bank = *ram_bank_register;
        byte i;
        *ram_bank_register = 41;
        for (i = 0; i < 80; ++i)
            window[i] = 0;
        window[7] = 1; /* increment variable 1 */
        window[8] = 1;
        window[9] = 0;
        *ram_bank_register = previous_bank;
    }
    test_nested_dispatch();
    test_context_storage_bytes = logicContextStorageBytes();
    test_context_hardware_stack_bytes = logicContextHardwareStackBytes();
    test_context_software_stack_bytes = logicContextSoftwareStackBytes();
    test_logic_entry_size = sizeof(LOGICEntry);
    test_logic_file_size = sizeof(LOGICFile);
    test_trace_event_size = sizeof(LogicTraceEvent);
    test_diag_a_window = (word)observed_a_window_first |
                         ((word)observed_a_window_invalid << 8);
    CHECK(ram[0] == 26 && ram[1] == 1 && ram[3] == 1, 64);
    CHECK(logic_entries[0].entryPoint == 61 &&
          logic_entries[0].data == &logic_files[0] &&
          logic_entries[0].dataBank == 40, 72);
    CHECK(expected_count_a == 1 && observed_count_a == 1 &&
          observed_a_pc == expected_a_pc && observed_a_code_bank == 40 &&
          observed_a_window_first == 100 && observed_a_window_invalid == 0,
          65);
    CHECK(top_ram_bank == 0x39 && logicContextDepth() == 0 &&
          logicContextError() == LOGIC_CONTEXT_OK,
          66);
    goto finished;
#else
    test_nested_dispatch();
    test_diag_a_window = (word)observed_a_window_first |
                         ((word)observed_a_window_invalid << 8);

    CHECK(expected_count_b == 2, 1);
    CHECK(observed_count_b == 2, 12);
    CHECK(memcmp(&expected_entry_b, &observed_entry_b, sizeof(LOGICEntry)) == 0, 2);
    CHECK(memcmp(&expected_file_b, &observed_file_b, sizeof(LOGICFile)) == 0, 3);
    CHECK(expected_count_a == 1 && observed_count_a == 1, 4);
    CHECK(memcmp(&expected_entry_a, &observed_entry_a, sizeof(LOGICEntry)) == 0, 5);
    CHECK(memcmp(&expected_file_a, &observed_file_a, sizeof(LOGICFile)) == 0, 6);

    CHECK(observed_b_start == 0xA100 && observed_b_end == 0xA150 &&
          observed_b_pc == expected_b_pc && observed_b_code_bank == 41 &&
          observed_b_entry_ptr == expected_b_entry_ptr &&
          observed_b_file_ptr == expected_b_file_ptr &&
          observed_b_window_first == 2 && observed_b_window_invalid == 0,
          7);
    CHECK(observed_a_start == 0xA000 && observed_a_end == 0xA050, 8);
    CHECK(observed_a_pc == expected_a_pc, 50);
    CHECK(observed_a_code_bank == 40, 51);
    CHECK(observed_a_entry_ptr == expected_a_entry_ptr, 52);
    CHECK(observed_a_file_ptr == expected_a_file_ptr, 53);
    CHECK(observed_a_window_first == 100, 54);
    CHECK(observed_a_window_invalid == 0, 55);

    CHECK(memcmp(&assmLogicEntry, &top_entry, sizeof top_entry) == 0 &&
          memcmp(&assmLogicFile, &top_file, sizeof top_file) == 0,
          9);
    CHECK(top_start == 0xA300, 10);
    CHECK(ram[0] == 26 && ram[1] == 24 && ram[2] == 2 &&
          ram[3] == 1 && ram[4] == 1 && ram[100] == 1 && ram[257] == 1,
          11);
    CHECK(logic_entries[0].entryPoint == 61 &&
          logic_entries[0].data == &logic_files[0] &&
          logic_entries[0].dataBank == 40, 72);
    logic_entries[0].entryPoint = 4;
    CHECK(logicContextDepth() == 0, 13);
    CHECK(logicContextError() == LOGIC_CONTEXT_OK, 14);
    CHECK(logicContextVerifyFailed == FALSE, 15);
    CHECK(top_end == 0xA380, 16);
    CHECK(top_pc == 0xA302, 17);
    CHECK(top_entry_ptr == 0x3412 && top_file_ptr == 0x5678, 18);
    CHECK(top_code_bank == 43, 19);
    CHECK(top_ram_bank == 0x39, 20);
    CHECK(top_window_invalid == 0, 21);
#if LOGIC_CONTEXT_TRACE
    {
        LogicTraceEvent event;
        CHECK(logicContextTraceGet(27, &event) &&
              event.logic == 0 && event.logical_pc == 56, 25);
        CHECK(logicContextTraceCount() == 64, 22);
        CHECK(logicContextTraceDropped() == 0, 23);
        CHECK(logicContextTraceGet(0, &event) &&
              event.kind == LOGIC_TRACE_KIND_CALL && event.logic == 0 &&
              event.depth == 1,
              24);
        CHECK(logicContextTraceGet(27, &event) &&
              event.kind == LOGIC_TRACE_KIND_CALL && event.logic == 0 &&
              event.opcode == 23 && event.depth == 2,
              25);
        CHECK(logicContextTraceGet(52, &event) &&
              event.kind == LOGIC_TRACE_KIND_CALL && event.logic == 1 &&
              event.logical_pc == 55 &&
              event.opcode == 22 && event.depth == 3,
              26);
        CHECK(logicContextTraceGet(61, &event) &&
              event.kind == LOGIC_TRACE_KIND_RETURN && event.logic == 0 &&
              event.depth == 2,
              27);
        CHECK(logicContextTraceGet(63, &event) &&
              event.kind == LOGIC_TRACE_KIND_RETURN && event.logic == 0 &&
              event.depth == 1,
              28);
    }
    CHECK(logicContextTraceDropped() == 0, 29);
#endif
    test_context_storage_bytes = logicContextStorageBytes();
    test_context_hardware_stack_bytes = logicContextHardwareStackBytes();
    test_context_software_stack_bytes = logicContextSoftwareStackBytes();
    test_logic_entry_size = sizeof(LOGICEntry);
    test_logic_file_size = sizeof(LOGICFile);
    test_trace_event_size = sizeof(LogicTraceEvent);
#if !defined(LOGIC_CONTEXT_VERIFY) && !LOGIC_CONTEXT_TRACE
    /* Release frames contain caller state, not diagnostic bookkeeping. */
    CHECK(test_context_storage_bytes <= 266, 74);
    CHECK((word)&logicContextCodeBytes <= 260, 82);
#endif
#if LOGIC_CONTEXT_TRACE
    CHECK(test_context_hardware_stack_bytes > 0, 30);
    CHECK(test_context_software_stack_bytes == 0, 31);
#endif

#ifdef LOGIC_CONTEXT_VERIFY
    /* Fault injection verifies that independent shadow counters catch
       corruption which ordinary nested dispatch cannot naturally produce. */
    CHECK(logicContextEnter() == TRUE, 43);
    logicContextTestCorruptDepth();
    CHECK(logicContextLeave() == TRUE, 44);
    CHECK(logicContextDepth() == 0 && logicContextVerifyFailed == TRUE &&
          logicContextMismatchField() == LOGIC_CONTEXT_FIELD_DEPTH,
          45);
    logicContextTestClearMismatch();
    CHECK(logicContextEnter() == TRUE, 46);
    logicContextTestCorruptGeneration();
    CHECK(logicContextLeave() == TRUE, 47);
    CHECK(logicContextDepth() == 0 && logicContextVerifyFailed == TRUE &&
          logicContextMismatchField() == LOGIC_CONTEXT_FIELD_GENERATION,
          48);
    logicContextTestClearMismatch();
    CHECK(logicContextEnter() == TRUE, 75);
    logicContextTestCorruptAbandonment();
    CHECK(logicContextLeave() == TRUE, 76);
    CHECK(logicContextDepth() == 0 && logicContextVerifyFailed == TRUE &&
          logicContextMismatchField() == LOGIC_CONTEXT_FIELD_ABANDONED,
          77);
    logicContextTestClearMismatch();
#endif

    /* Exercise the real new-room opcode through A -> B. Its backend is a
       stub, but the command must retain writes and abandon both frames. */
    {
        volatile byte *window = (volatile byte *)0xA100;
        volatile byte *ram_bank_register = (volatile byte *)0;
        byte previous_bank = *ram_bank_register;
        byte i;
        *ram_bank_register = 41;
        for (i = 0; i < 80; ++i)
            window[i] = 0;
        window[7] = 1;  /* increment variable 1 */
        window[8] = 1;
        window[9] = 18; /* new_room 9 */
        window[10] = 9;
        *ram_bank_register = previous_bank;
    }
    memset(ram, 0, 512);
    ram[100] = 1;
    exitAllLogics = FALSE;
    hasEnteredNewRoom = FALSE;
    newRoomNum = 0;
    observed_count_a = 0;
    observed_a_ram_bank = 0xFF;
    test_nested_dispatch();
    CHECK(ram[0] == 26 && ram[1] == 1 && ram[3] == 0, 39);
    CHECK(hasEnteredNewRoom == TRUE && exitAllLogics == TRUE &&
          newRoomNum == 9, 40);
    CHECK(observed_count_a == 1 && observed_a_ram_bank == 2, 20);
    CHECK(startPos == 0xA100 && endPos == 0xA150 && codeBank == 41 &&
          cwCurrentCode == 0 && codeWindowInvalid == TRUE,
          41);
    CHECK(top_ram_bank == 6, 59);
    CHECK(logicContextDepth() == 0, 56);
    if (logicContextError() != LOGIC_CONTEXT_OK) {
        test_result = (byte)(99 + logicContextMismatchField());
        goto finished;
    }
    CHECK(logicContextVerifyFailed == FALSE, 58);

    exitAllLogics = FALSE;

    /* Restart from a nested logic must skip that logic's following actions
       and unwind the entire production dispatch chain. */
    {
        volatile byte *window = (volatile byte *)0xA100;
        volatile byte *ram_bank_register = (volatile byte *)0;
        byte previous_bank = *ram_bank_register;
        byte i;
        *ram_bank_register = 41;
        for (i = 0; i < 80; ++i)
            window[i] = 0;
        window[7] = 0x80; /* restart_game */
        window[8] = 1;    /* increment */
        window[9] = 2;    /* variable 2 must not change after restart */
        window[10] = 0;   /* end */
        *ram_bank_register = previous_bank;
    }
    memset(ram, 0, 512);
    ram[100] = 1;
    exitAllLogics = FALSE;
    hasEnteredNewRoom = FALSE;
    newRoomNum = 99;
    test_restart_backend_calls = 0;
    test_nested_dispatch();
    CHECK(exitAllLogics == TRUE && hasEnteredNewRoom == TRUE &&
          newRoomNum == 0, 60);
    CHECK(test_restart_backend_calls == 1, 85);
    CHECK(ram[2] == 0 && ram[3] == 0, 61);
    CHECK(logicContextDepth() == 0 && logicContextError() == LOGIC_CONTEXT_OK,
          62);
    CHECK(top_ram_bank == 6 && logicContextVerifyFailed == FALSE, 63);
    exitAllLogics = FALSE;

    /* toggle.v must change the indexed flag, preserving its index variable. */
    {
        volatile byte *window = (volatile byte *)0xA000;
        volatile byte *bank = (volatile byte *)0;
        byte previous = *bank;
        init_logic_tables();
        *bank = 40;
        window[4] = 17;
        window[5] = 240;
        window[6] = 0;
        *bank = previous;
    }
    memset(ram, 0, 512);
    ram[257 + 255] = 0;
    ram[240] = 255;
    ram[255] = 99;
    test_nested_dispatch();
    test_diag_a_window = ram[257 + 255] | ((word)ram[240] << 8);
    CHECK(ram[257 + 255] == 1 && ram[240] == 255 && ram[255] == 99, 78);
    test_nested_dispatch();
    CHECK(ram[257 + 255] == 0 && ram[240] == 255 && ram[255] == 99, 79);

    {
        static const byte random_code[] = {
            0x82, 3, 4, 240, 0x82, 10, 10, 241, 0x82, 0, 255, 242, 0
        };
        volatile byte *window = (volatile byte *)0xA000;
        volatile byte *bank = (volatile byte *)0;
        byte previous = *bank;
        byte i;
        memset(b6RandomNumbers, 255, sizeof b6RandomNumbers);
        *bank = 40;
        for (i = 0; i < sizeof random_code; ++i) window[4 + i] = random_code[i];
        *bank = previous;
        test_nested_dispatch();
        test_diag_a_window = ram[240] | ((word)ram[241] << 8);
        CHECK(ram[240] == 4 && ram[241] == 10 && ram[242] == 255, 80);
        memset(b6RandomNumbers, 254, sizeof b6RandomNumbers);
        test_nested_dispatch();
        CHECK(ram[240] == 3 && ram[241] == 10 && ram[242] == 254, 81);
    }

    /* add.v must resume at its immediately following production opcode. */
    {
        static const byte add_v_code[] = {
            6, 10, 11,       /* add.v v10, v11 */
            3, 0, 77,         /* assign.n v0, 77 */
            0                 /* return */
        };
        volatile byte *window = (volatile byte *)0xA000;
        volatile byte *bank = (volatile byte *)0;
        byte previous = *bank;
        byte i;
        init_logic_tables();
        *bank = 40;
        for (i = 0; i < 80; ++i)
            window[i] = 0;
        for (i = 0; i < sizeof add_v_code; ++i)
            window[4 + i] = add_v_code[i];
        *bank = previous;

        memset(ram, 0, 512);
        ram[10] = 3;
        ram[11] = 4;
        exitAllLogics = FALSE;
        hasEnteredNewRoom = FALSE;
        test_nested_dispatch();
        CHECK(ram[10] == 7 && ram[0] == 77, 83);
    }

    /* new.room.v must store its variable value before abandoning the caller. */
    {
        volatile byte *window = (volatile byte *)0xA000;
        volatile byte *bank = (volatile byte *)0;
        byte previous = *bank;
        byte i;
        init_logic_tables();
        *bank = 40;
        for (i = 0; i < 80; ++i)
            window[i] = 0;
        window[4] = 19; /* new.room.v v243 */
        window[5] = 243;
        *bank = previous;

        memset(ram, 0, 512);
        ram[243] = 9;
        exitAllLogics = FALSE;
        hasEnteredNewRoom = FALSE;
        newRoomNum = 0x7777;
        test_nested_dispatch();
        CHECK(exitAllLogics == TRUE && hasEnteredNewRoom == TRUE &&
              newRoomNum == 9, 84);
    }

    CHECK(logicContextLeave() == FALSE, 32);
    CHECK(logicContextError() == LOGIC_CONTEXT_UNDERFLOW, 33);

    {
        byte i;
        for (i = 0; i < LOGIC_CONTEXT_MAX_DEPTH; ++i)
            CHECK(logicContextEnter() == TRUE, 34);
        CHECK(logicContextEnter() == FALSE, 35);
        CHECK(logicContextDepth() == LOGIC_CONTEXT_MAX_DEPTH &&
              logicContextError() == LOGIC_CONTEXT_OVERFLOW, 36);
        for (i = 0; i < LOGIC_CONTEXT_MAX_DEPTH; ++i)
            CHECK(logicContextLeave() == TRUE, 37);
        CHECK(logicContextDepth() == 0 &&
              logicContextError() == LOGIC_CONTEXT_OVERFLOW, 38);
    }
#endif

finished:
    asm("jmp $fffd");
    for (;;) { }
}
