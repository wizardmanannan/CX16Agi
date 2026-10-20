#include "logicTypes.h"
#include "logicContext.h"

#define REPLAY_MAX_EVENTS 64
#define REPLAY_EVENT_BYTES 25
#define REPLAY_OBJECT_BYTES 9

/* Host-written replay request. Keep these exported names in sync with the
   labels consumed by tests/x16_replay_runner.c. */
volatile word replay_event_count;
volatile byte replay_events[REPLAY_MAX_EVENTS][REPLAY_EVENT_BYTES];
volatile byte replay_rng_seed[8];
volatile unsigned long replay_tick_numerator;
volatile unsigned long replay_tick_denominator;
volatile unsigned long replay_tick_count;
volatile byte replay_fault_flags;
volatile word replay_checkpoint_length;
volatile byte replay_checkpoint[4096];
volatile word replay_resource_sizes[3];

volatile byte replay_output[19];
volatile byte replay_variables[256];
volatile byte replay_flags[256];
volatile word replay_room;
volatile word replay_object_count;
volatile byte replay_objects[REPLAY_OBJECT_BYTES * 256];
volatile unsigned long replay_rng_call_count;
volatile unsigned long replay_rng_final_state;
volatile unsigned long replay_ticks_executed;

/* Production commandLoop.s owns the dispatch path; this fixture supplies
   only the small resource table and deterministic platform edges. */
byte exitAllLogics, hasEnteredNewRoom;
int newRoomNum, currentLog;
volatile byte test_result;
word test_step_count;
byte numInputWords;
word inputWords[16];
byte logicEntryAddressesLow[256], logicEntryAddressesHigh[256];
LOGICEntry *logics[256];
LOGICEntry assmLogicEntry;
LOGICFile assmLogicFile;

static LOGICFile logic_files[3];
static LOGICEntry logic_entries[3];
static byte resource_present[3];
static byte event_consumed[REPLAY_MAX_EVENTS];
static byte fault_pending;
static byte replay_invalid;
static byte replay_key_down;
static byte replay_current_key;
static unsigned long replay_current_tick;
byte replay_rng_minimum;
byte replay_rng_maximum;
static unsigned long rng_state;
static unsigned long rng_calls_this_tick;
static unsigned long expected_rng_calls_this_tick;

extern void replay_dispatch_logic_zero(void);
extern volatile byte logicContextVerifyFailed;

static volatile byte *const golden_vars = (volatile byte *)0x0400;
static volatile byte *const golden_flags = (volatile byte *)0x0501;

static unsigned long event_u32(byte index, byte offset)
{
    unsigned long value = replay_events[index][offset];
    value |= (unsigned long)replay_events[index][offset + 1] << 8;
    value |= (unsigned long)replay_events[index][offset + 2] << 16;
    value |= (unsigned long)replay_events[index][offset + 3] << 24;
    return value;
}

static byte event_reason(byte index)
{
    return replay_events[index][24];
}

static byte event_field(byte index)
{
    return replay_events[index][15];
}

static void set_agi_clock(unsigned long seconds)
{
    golden_vars[11] = (byte)(seconds % 60UL);
    seconds /= 60UL;
    golden_vars[12] = (byte)(seconds % 60UL);
    seconds /= 60UL;
    golden_vars[13] = (byte)(seconds % 24UL);
    seconds /= 24UL;
    golden_vars[14] = (byte)seconds;
}

static unsigned long agi_clock_seconds(void)
{
    unsigned long total = golden_vars[14];
    total = total * 24UL + golden_vars[13];
    total = total * 60UL + golden_vars[12];
    return total * 60UL + golden_vars[11];
}

static void put_u32(volatile byte *destination, unsigned long value)
{
    destination[0] = (byte)value;
    destination[1] = (byte)(value >> 8);
    destination[2] = (byte)(value >> 16);
    destination[3] = (byte)(value >> 24);
}

static void initialize_logic_resources(void)
{
    static const word code_addresses[3] = {0xA000, 0xA100, 0xA200};
    static const byte code_banks[3] = {40, 41, 42};
    static const word raw_entries[3] = {4, 7, 9};
    byte raw_mode;
    byte logic;

    raw_mode = replay_checkpoint_length >= 4 &&
        replay_checkpoint[0] == 0x4C && replay_checkpoint[1] == 0x43 &&
        replay_checkpoint[2] == 0x58 && replay_checkpoint[3] == 0x31;
    for (logic = 0; logic < 3; ++logic) {
        LOGICFile *file = &logic_files[logic];
        LOGICEntry *entry = &logic_entries[logic];
        file->codeSize = replay_resource_sizes[logic];
        file->logicCode = (byte *)code_addresses[logic];
        file->numMessages = 0;
        file->messages = (byte **)0;
        file->codeBank = code_banks[logic];
        file->messageBank = 0;
        entry->loaded = TRUE;
        entry->entryPoint = raw_mode ? raw_entries[logic] : 0;
        entry->currentPoint = 0;
        entry->data = file;
        entry->dataBank = code_banks[logic];
        entry->isLogicZeroOrDependency = FALSE;
        resource_present[logic] = replay_resource_sizes[logic] != 0;
        logicEntryAddressesLow[logic] = (byte)code_addresses[logic];
        logicEntryAddressesHigh[logic] = (byte)(code_addresses[logic] >> 8);
        logics[logic] = entry;
    }
}

void b5GetLogicFile(LOGICFile *destination, byte logic_no)
{
    if (logic_no < 3 && resource_present[logic_no]) {
#if defined(LOGIC_CONTEXT_VERIFY) && defined(LOGIC_CONTEXT_FAULT_INJECTION)
        if (fault_pending && logic_no == 2 && currentLog == 1) {
            logicContextTestOmitMetadataSave();
            fault_pending = FALSE;
        }
#endif
        *destination = logic_files[logic_no];
    }
}

void b5GetLogicEntry(LOGICEntry *destination, byte logic_no)
{
    if (logic_no < 3 && resource_present[logic_no])
        *destination = logic_entries[logic_no];
}

void b5SetLogicEntry(LOGICEntry *source, byte logic_no)
{
    if (logic_no < 3 && resource_present[logic_no])
        logic_entries[logic_no] = *source;
}

void b6LoadLogicFile(byte logic_no, boolean load_dependencies)
{
    (void)logic_no;
    (void)load_dependencies;
}

void b4Restart_game(void)
{
    exitAllLogics = TRUE;
    hasEnteredNewRoom = TRUE;
    newRoomNum = 0;
}

void test_snapshot_call(void)
{
}

void test_capture_nested_return(void)
{
}

/* The random opcode still enters through production b4Random_num. Only its
   platform random source is replaced by the manifest-seeded deterministic
   stream required for repeatable replay. */
void replay_rng_set_bounds(byte minimum, byte maximum)
{
    replay_rng_minimum = minimum;
    replay_rng_maximum = maximum;
}

byte replay_rng_next(void)
{
    byte i;
    unsigned long product;
    unsigned long range;
    byte value;
    byte matched = FALSE;

    rng_state ^= rng_state >> 13;
    rng_state ^= rng_state << 21;
    rng_state ^= rng_state >> 11;
    product = rng_state * 0xDEADBF03UL;
    ++replay_rng_call_count;
    ++rng_calls_this_tick;
    if (replay_rng_maximum < replay_rng_minimum) {
        replay_invalid = TRUE;
        value = replay_rng_minimum;
    } else {
        range = (unsigned long)replay_rng_maximum - replay_rng_minimum + 1UL;
        value = (byte)(replay_rng_minimum + product % range);
    }

    for (i = 0; i < replay_event_count; ++i) {
        if (!event_consumed[i] &&
            event_u32(i, 4) == replay_current_tick &&
            event_reason(i) == 3) {
            event_consumed[i] = TRUE;
            matched = TRUE;
            break;
        }
    }
    if (!matched)
        replay_invalid = TRUE;
    return value;
}

static void initialize_runtime(void)
{
    unsigned int i;

    for (i = 0; i < 256; ++i) {
        golden_vars[i] = 0;
        golden_flags[i] = 0;
        replay_variables[i] = 0;
        replay_flags[i] = 0;
    }
    for (i = 0; i < sizeof event_consumed; ++i)
        event_consumed[i] = FALSE;
    for (i = 0; i < 19; ++i)
        replay_output[i] = 0;
    for (i = 0; i < sizeof replay_objects; ++i)
        replay_objects[i] = 0;

    /* These are the AGI defaults applied by both the production startup and
       the ScummVM comparison fixture. */
    golden_flags[2] = FALSE;
    golden_flags[4] = FALSE;
    golden_flags[5] = TRUE;
    golden_flags[9] = TRUE;
    golden_flags[11] = TRUE;
    golden_vars[19] = 0;
    if (replay_checkpoint_length >= 4 &&
        replay_checkpoint[0] == 0x4C && replay_checkpoint[1] == 0x43 &&
        replay_checkpoint[2] == 0x58 && replay_checkpoint[3] == 0x31)
        golden_vars[100] = 1;
    replay_key_down = FALSE;
    replay_current_key = 0;
    replay_current_tick = 0;
    replay_rng_minimum = 0;
    replay_rng_maximum = 0;
    replay_rng_call_count = 0;
    replay_rng_final_state = 0;
    replay_ticks_executed = 0;
    rng_calls_this_tick = 0;
    expected_rng_calls_this_tick = 0;
    rng_state = (unsigned long)replay_rng_seed[0] |
                ((unsigned long)replay_rng_seed[1] << 8) |
                ((unsigned long)replay_rng_seed[2] << 16) |
                ((unsigned long)replay_rng_seed[3] << 24);
    replay_rng_final_state = rng_state;
    fault_pending = (replay_fault_flags & 1) != 0;
    replay_invalid = FALSE;
    test_result = 0;
    test_step_count = 0;
    exitAllLogics = FALSE;
    hasEnteredNewRoom = FALSE;
    newRoomNum = 0;
    currentLog = 0;
    numInputWords = 0;
    for (i = 0; i < 16; ++i)
        inputWords[i] = 0;

    initialize_logic_resources();
    assmLogicEntry.loaded = TRUE;
    assmLogicEntry.entryPoint = 0;
    assmLogicEntry.currentPoint = 0;
    assmLogicEntry.data = (LOGICFile *)0;
    assmLogicEntry.dataBank = 0;
    assmLogicEntry.isLogicZeroOrDependency = FALSE;
    assmLogicFile.codeSize = 0;
    assmLogicFile.logicCode = (byte *)0;
    assmLogicFile.numMessages = 0;
    assmLogicFile.messages = (byte **)0;
    assmLogicFile.codeBank = 0;
    assmLogicFile.messageBank = 0;
}

static void apply_event(byte index)
{
    byte reason = event_reason(index);
    unsigned long after = event_u32(index, 20);

    if (replay_events[index][8] != 4 || replay_events[index][9] != 0) {
        replay_invalid = TRUE;
        event_consumed[index] = TRUE;
        return;
    }
    if (reason == 1) { /* key down */
        replay_current_key = event_field(index);
        replay_key_down = TRUE;
        golden_vars[19] = replay_current_key;
        event_consumed[index] = TRUE;
    } else if (reason == 2) { /* key up */
        if (!replay_key_down || replay_current_key != event_field(index))
            replay_invalid = TRUE;
        replay_key_down = FALSE;
        event_consumed[index] = TRUE;
    } else if (reason == 3) { /* checked after the real RNG opcode runs */
        unsigned long before = event_u32(index, 16);
        if (golden_vars[event_field(index)] != (byte)before)
            replay_invalid = TRUE;
        ++expected_rng_calls_this_tick;
    } else if (reason == 4) { /* TIME: after is elapsed seconds */
        if (event_u32(index, 16) != agi_clock_seconds())
            replay_invalid = TRUE;
        set_agi_clock(after);
        event_consumed[index] = TRUE;
    } else {
        replay_invalid = TRUE;
        event_consumed[index] = TRUE;
    }
}

static void record_context_mismatch(void)
{
    word pc;
    byte logic;
    unsigned long code;
    if (!logicContextVerifyFailed || replay_output[0] == 1)
        return;

    pc = logicContextMismatchPc();
    logic = logicContextMismatchLogic();
    code = ((unsigned long)logicContextMismatchField() << 8) |
           logicContextMismatchOffset();
    replay_output[0] = 1;
    replay_output[9] = 4; /* LOGIC_TRACE_KIND_MARK */
    replay_output[10] = logic;
    replay_output[11] = (byte)pc;
    replay_output[12] = (byte)(pc >> 8);
    replay_output[13] = logicContextMismatchOpcode();
    replay_output[14] = 5; /* context mismatch */
    put_u32(replay_output + 15, code);
}

static void run_ticks(void)
{
    unsigned long tick;
    unsigned int i;

    for (tick = 1; tick <= replay_tick_count; ++tick) {
        replay_current_tick = tick;
        rng_calls_this_tick = 0;
        expected_rng_calls_this_tick = 0;
        for (i = 0; i < replay_event_count; ++i)
            if (!event_consumed[i] && event_u32(i, 4) == tick)
                apply_event(i);

        replay_dispatch_logic_zero();
        /* ScummVM's interpretCycle clears VM_FLAG_NEW_ROOM_EXEC after the
           first cycle; the production dispatch fixture supplies that outer
           cycle bookkeeping around the real command loop. */
        golden_flags[5] = FALSE;
        replay_ticks_executed = tick;
        if (rng_calls_this_tick != expected_rng_calls_this_tick)
            replay_invalid = TRUE;
        for (i = 0; i < replay_event_count; ++i) {
            if (event_consumed[i] && event_u32(i, 4) == tick &&
                event_reason(i) == 3 &&
                golden_vars[event_field(i)] != (byte)event_u32(i, 20))
                replay_invalid = TRUE;
        }
        replay_rng_final_state = rng_state;
        record_context_mismatch();
    }
}

static void capture_result(void)
{
    unsigned int i;
    unsigned long consumed = 0;
    for (i = 0; i < replay_event_count; ++i)
        if (event_consumed[i])
            ++consumed;
        else
            replay_invalid = TRUE;

    for (i = 0; i < 256; ++i) {
        replay_variables[i] = golden_vars[i];
        replay_flags[i] = golden_flags[i] ? 1 : 0;
    }
    replay_room = replay_variables[0];
    replay_object_count = 0;
    if (test_result == 99 && replay_output[0] == 0) {
        replay_output[0] = 2;
        replay_output[14] = 1; /* production dispatch step guard */
    }
    if (replay_invalid && replay_output[0] == 0) {
        replay_output[0] = 1;
        replay_output[9] = 4;
        replay_output[14] = 3; /* deterministic replay mismatch */
    }
    put_u32(replay_output + 1, consumed);
    put_u32(replay_output + 5, 0);
    replay_rng_final_state = rng_state;
}

void replay_execute_body(void)
{
    initialize_runtime();
    run_ticks();
    capture_result();
}

int main(void)
{
    /* The host enters replay_execute after cc65 startup has initialized BSS. */
    asm("jmp $fffd");
}
