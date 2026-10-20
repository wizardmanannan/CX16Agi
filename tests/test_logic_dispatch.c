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
