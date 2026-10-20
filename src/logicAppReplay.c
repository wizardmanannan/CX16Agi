#if defined(LOGIC_APP_REPLAY_TARGET)

#include "../tools/logic_app_replay.h"

#include <string.h>

#include "agifiles.h"
#include "graphics.h"
#include "irq.h"
#include "logic.h"
#include "logicContext.h"
#include "memoryManager.h"
#include "object.h"
#include "random.h"
#include "view.h"

extern int currentLog;
extern int numLogics, numPictures, numViews, numSounds;
extern int numInputWords, inputWords[10];
extern int numWords;
extern word startPos;
extern byte mainLoopCounter;
extern void b6ReplayStartApplication(unsigned int seed);
extern void b6ReplayRunInterpreterCycle(void);
extern byte b6ReplayMainLoopStepReady(unsigned int lastVsync);
extern volatile word cwCurrentCode;
extern volatile byte logicContextOpcode;
extern volatile byte codeBank;

volatile uint8_t logicAppReplaySeed[2];
volatile uint8_t logicAppReplayEventCount;
volatile uint8_t logicAppReplayEvents[LOGIC_APP_REPLAY_INPUT_MAX]
                                     [LOGIC_APP_REPLAY_INPUT_RECORD_SIZE];
volatile uint8_t logicAppReplayFaultFlags;
volatile uint8_t logicAppReplayRunRequested;
volatile uint32_t logicAppReplayTickLimit;
volatile uint8_t logicAppReplayReady;
volatile uint16_t logicAppReplayOutputLength;
volatile uint8_t logicAppReplayOutput[LOGIC_APP_REPLAY_OUTPUT_SIZE];
volatile uint8_t logicAppReplayRandomIndex;
volatile uint32_t logicAppReplayRandomCalls;
static uint32_t replayRandomState;

uint8_t __fastcall__ logicAppReplayRandomNumber(uint8_t max)
{
    uint32_t scaled;
    uint16_t divisor = (uint16_t)max + 1u;
    replayRandomState ^= replayRandomState >> 13;
    replayRandomState ^= replayRandomState << 21;
    replayRandomState ^= replayRandomState >> 11;
    scaled = replayRandomState * 0xDEADBF03UL;
    ++logicAppReplayRandomCalls;
    ++logicAppReplayRandomIndex;
    return (uint8_t)(scaled % divisor);
}

static uint32_t replayTick;
static uint8_t nextInputEvent;
static uint8_t inputWasMissed;
static uint8_t messageCount;
static uint16_t messageDropped;
static uint8_t messageOverflow;
static uint8_t inputWaitExhausted;
static uint8_t faultTriggered;
static uint8_t semanticFaultPhase;
static uint8_t lastInputKey;
static uint16_t inputWaitPolls;
static uint16_t capacityFlags;
static uint32_t lastInputFrame;
static uint32_t inputWaitTick;
static uint32_t replayTickLimit;
static uint32_t replayElapsedVsync;
static uint32_t replayScheduleFrame;
static uint32_t replayScheduleMilliseconds;
static uint8_t replayTimerMilliseconds;
static uint16_t lastSnapshotVsync;
static uint16_t lastCycleVsync;
static uint32_t lastCycleMilliseconds;

#pragma wrapped-call (push, trampoline, IRQ_BANK)
extern void b6Timing_proc(void);
#pragma wrapped-call (pop)

/* Input and AGI timers share a virtual 60 Hz clock. Rendering still waits
   for real IRQs, but its cost cannot move a recorded key to another cycle. */
void logicAppReplayAdvanceMilliseconds(uint32_t milliseconds)
{
    replayScheduleMilliseconds += milliseconds;
    replayScheduleFrame = replayScheduleMilliseconds * 3UL / 50UL;
    while (milliseconds) {
        --milliseconds;
        if (++replayTimerMilliseconds == 50) {
            replayTimerMilliseconds = 0;
            b6Timing_proc();
        }
    }
}

void logicAppReplayAdvanceWait(unsigned int frames)
{
    logicAppReplayAdvanceMilliseconds(((uint32_t)frames * 50UL + 2UL) / 3UL);
}

void logicAppReplayResetCycleClock(void)
{
    /* Closing a blocking message starts a fresh main-loop delay. */
    lastCycleMilliseconds = replayScheduleMilliseconds;
}

#pragma code-name (push, "BANKRAM06")
#pragma wrapped-call (push, trampoline, IRQ_BANK)
static void advanceReplayCycleClock(void)
{
    uint32_t next = lastCycleMilliseconds - lastCycleMilliseconds % 25UL +
        (var[10] ? (uint32_t)var[10] * 50UL : 25UL);
    /* Main-loop polls take 10 ms; time spent inside the previous logic
       already counts toward its next 25 ms scheduling quantum. */
    do {
        logicAppReplayAdvanceMilliseconds(10UL);
    } while (replayScheduleMilliseconds < next);
    lastCycleMilliseconds = replayScheduleMilliseconds;
}
#pragma wrapped-call (pop)
#pragma code-name (pop)

static void updateReplayClock(void)
{
    uint16_t now = b6ReadVSyncCounter();
    replayElapsedVsync += (uint16_t)(now - lastSnapshotVsync);
    lastSnapshotVsync = now;
}

static void put16(uint16_t offset, uint16_t value)
{
    logicAppReplayOutput[offset] = (uint8_t)value;
    logicAppReplayOutput[offset + 1] = (uint8_t)(value >> 8);
}

static void put32(uint16_t offset, uint32_t value)
{
    uint8_t i;
    for (i = 0; i < 4; ++i)
        logicAppReplayOutput[offset + i] = (uint8_t)(value >> (i * 8));
}

static void putRaw(uint16_t offset, const uint8_t *source, uint16_t size)
{
    uint16_t i;
    for (i = 0; i < size; ++i)
        logicAppReplayOutput[offset + i] = source[i];
}

static void writeTrace(uint16_t offset, const LogicTraceEvent *event)
{
    put32(offset, event->sequence);
    put32(offset + 4, event->tick);
    logicAppReplayOutput[offset + 8] = event->kind;
    logicAppReplayOutput[offset + 9] = event->logic;
    logicAppReplayOutput[offset + 10] = (uint8_t)event->logical_pc;
    logicAppReplayOutput[offset + 11] = (uint8_t)(event->logical_pc >> 8);
    logicAppReplayOutput[offset + 12] = event->opcode;
    logicAppReplayOutput[offset + 13] = event->depth;
    logicAppReplayOutput[offset + 14] = event->bank;
    logicAppReplayOutput[offset + 15] = event->field;
    put32(offset + 16, event->before);
    put32(offset + 20, event->after);
    logicAppReplayOutput[offset + 24] = event->reason;
}

static uint16_t captureLoadedLogics(void)
{
    uint16_t loaded = 0;
    uint16_t i;
    for (i = 0; i < 256; ++i) {
        LOGICEntry entry;
        b5GetLogicEntry(&entry, (byte)i);
        if (entry.loaded) {
            logicAppReplayOutput[LOGIC_APP_REPLAY_LOADED_LOGICS_OFFSET + i / 8] |=
                (uint8_t)(1u << (i & 7));
            ++loaded;
        }
    }
    return loaded;
}

static uint8_t captureObjects(void)
{
    uint8_t i;
    uint16_t offset = LOGIC_APP_REPLAY_OBJECTS_OFFSET;
    for (i = 0; i < LOGIC_APP_REPLAY_OBJECT_MAX; ++i) {
        ViewTable view;
        getViewTab(&view, i);
        if (view.entryNum != i)
            capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_OBJECT_ENTRY_MISMATCH;
        logicAppReplayOutput[offset] = i;
        logicAppReplayOutput[offset + 1] = (uint8_t)view.flags;
        logicAppReplayOutput[offset + 2] = (uint8_t)(view.flags >> 8);
        logicAppReplayOutput[offset + 3] = (uint8_t)view.xPos;
        logicAppReplayOutput[offset + 4] = (uint8_t)(view.xPos >> 8);
        logicAppReplayOutput[offset + 5] = (uint8_t)view.yPos;
        logicAppReplayOutput[offset + 6] = (uint8_t)(view.yPos >> 8);
        logicAppReplayOutput[offset + 7] = view.currentView;
        logicAppReplayOutput[offset + 8] = view.currentLoop;
        logicAppReplayOutput[offset + 9] = view.currentCel;
        logicAppReplayOutput[offset + 10] = view.direction;
        logicAppReplayOutput[offset + 11] = view.motion;
        logicAppReplayOutput[offset + 12] =
            view.cycleStatus | (view.noAdvance ? 0x80u : 0u);
        logicAppReplayOutput[offset + 13] = view.param2;
        offset += LOGIC_APP_REPLAY_OBJECT_RECORD_SIZE;
    }
    logicAppReplayOutput[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET] =
        LOGIC_APP_REPLAY_OBJECT_MAX;
    return LOGIC_APP_REPLAY_OBJECT_MAX;
}

static uint16_t captureInventory(void)
{
    uint16_t i;
    uint16_t count;
    uint16_t offset = LOGIC_APP_REPLAY_INVENTORY_OFFSET;
    count = (uint16_t)bDGetObjectCount();
    if (count > LOGIC_APP_REPLAY_INVENTORY_MAX) {
        capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_INVENTORY_TRUNCATED;
        count = LOGIC_APP_REPLAY_INVENTORY_MAX;
    }
    for (i = 0; i < count; ++i) {
        objectType object;
        bDGetObject((byte)i, &object);
        logicAppReplayOutput[offset++] = (uint8_t)i;
        logicAppReplayOutput[offset++] = object.roomNum;
    }
    return count;
}

static uint8_t captureTrace(void)
{
    uint8_t available = logicContextTraceCount();
    uint8_t count = available;
    uint8_t first = 0;
    uint8_t i;
    if (count > LOGIC_APP_REPLAY_TRACE_MAX) {
        capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
        first = (uint8_t)(count - LOGIC_APP_REPLAY_TRACE_MAX);
        count = LOGIC_APP_REPLAY_TRACE_MAX;
    }
    for (i = 0; i < count; ++i) {
        LogicTraceEvent event;
        if (!logicContextTraceGet((byte)(first + i), &event))
            return i;
        writeTrace(LOGIC_APP_REPLAY_TRACE_OFFSET +
                   (uint16_t)i * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE, &event);
    }
    return count;
}

static void captureSnapshot(void)
{
    uint16_t i;
    uint16_t loaded;
    uint8_t objects;
    uint16_t inventory;
    uint8_t traceCount;
    uint8_t availableTraceCount;
    uint8_t statusFlags = 0;
    byte previousBank;
    LOGICFile logicZero;
    uint8_t frameCount;
    uint8_t frameIndex;
    uint16_t pc = (uint16_t)(*(volatile uint16_t *)0x00A9 +
                             cwCurrentCode - startPos);

    /* The dispatcher has consumed RETURN at a completed cycle boundary. */
    if (logicContextOpcode == 0 && pc)
        --pc;

    updateReplayClock();

    memset((void *)logicAppReplayOutput, 0,
           LOGIC_APP_REPLAY_MESSAGES_OFFSET);
    memset((void *)(logicAppReplayOutput + LOGIC_APP_REPLAY_TRACE_OFFSET), 0,
           LOGIC_APP_REPLAY_TRACE_MAX * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE);
    memset((void *)(logicAppReplayOutput + LOGIC_APP_REPLAY_PARSER_OFFSET), 0,
           LOGIC_APP_REPLAY_OUTPUT_SIZE - LOGIC_APP_REPLAY_PARSER_OFFSET);
    /* Binary checkpoint magic is ASCII. cc65 maps character constants to
       PETSCII on CX16 (so 'X' would encode as $D8, not ASCII $58). */
    logicAppReplayOutput[0] = 0x58;
    logicAppReplayOutput[1] = 0x31;
    logicAppReplayOutput[2] = 0x36;
    logicAppReplayOutput[3] = 0x52;
    logicAppReplayOutput[4] = LOGIC_APP_REPLAY_SNAPSHOT_VERSION;
    logicAppReplayOutput[5] = logicAppReplayReady ? 1 : 0;
    put32(6, replayTick);
    put16(10, logicAppReplayEventCount);
    put16(12, nextInputEvent);
    logicAppReplayOutput[14] = inputWasMissed;
    logicAppReplayOutput[15] = var[0];
    logicAppReplayOutput[16] = (uint8_t)currentLog;
    put16(17, pc);
    logicAppReplayOutput[19] = logicContextOpcode;
    logicAppReplayOutput[20] = logicContextDepth();
    logicAppReplayOutput[21] = logicContextError();
#ifdef LOGIC_CONTEXT_VERIFY
    logicAppReplayOutput[22] = logicContextVerifyFailed;
    logicAppReplayOutput[23] = logicContextVerifyField;
    logicAppReplayOutput[24] = logicContextVerifyOffset;
    logicAppReplayOutput[25] = logicContextMismatchLogic();
    logicAppReplayOutput[26] = logicContextMismatchOpcode();
    put16(27, logicContextMismatchPc());
    put32(29, logicContextVerifyExpected);
    put32(33, logicContextVerifyActual);
    if (logicContextVerifyFailed)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED;
#endif
    logicAppReplayOutput[37] = logicAppReplayRandomIndex;
    logicAppReplayOutput[38] = mainLoopCounter;
    put32(60, logicAppReplayRandomCalls);
    loaded = captureLoadedLogics();
    put16(39, loaded);
    availableTraceCount = logicContextTraceCount();
    traceCount = captureTrace();
    logicAppReplayOutput[41] = traceCount;
    put32(42, logicContextTraceDropped() +
           (availableTraceCount > traceCount
                ? availableTraceCount - traceCount : 0));
    if (logicContextTraceDropped() || availableTraceCount > traceCount) {
        statusFlags |= LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED;
        capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    }
    logicAppReplayOutput[46] = messageCount;
    put16(47, messageDropped);
    objects = captureObjects();
    put16(49, objects);
    inventory = captureInventory();
    put16(51, inventory);
    put16(53, logicContextStorageBytes());
    logicAppReplayOutput[55] = (uint8_t)logicContextHardwareStackBytes();
    put16(56, logicContextSoftwareStackBytes());
    if (inputWasMissed)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_INPUT_MISSED;
    if (messageOverflow)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_MESSAGE_OVERFLOW;
    if (inputWaitExhausted)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED;
    if (faultTriggered)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED;
    if (capacityFlags)
        statusFlags |= LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    logicAppReplayOutput[58] = statusFlags;
    logicAppReplayOutput[59] = codeBank;

    putRaw(LOGIC_APP_REPLAY_VARS_OFFSET, (const uint8_t *)GOLDEN_RAM, 256);
    putRaw(LOGIC_APP_REPLAY_FLAGS_OFFSET,
           (const uint8_t *)(GOLDEN_RAM + 257), 256);

    previousBank = RAM_BANK;
    RAM_BANK = PARSER_BANK;
    put16(LOGIC_APP_REPLAY_PARSER_OFFSET,
          numInputWords > 0 ? (uint16_t)numInputWords : 0);
    for (i = 0; i < 10; ++i)
        put16(LOGIC_APP_REPLAY_PARSER_OFFSET + 2 + i * 2,
              (uint16_t)inputWords[i]);
    RAM_BANK = WORD_BANK;
    put16(LOGIC_APP_REPLAY_WORD_COUNT_OFFSET,
          numWords > 0 ? (uint16_t)numWords : 0);
    RAM_BANK = previousBank;

    put16(LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET,
          numLogics > 0 ? (uint16_t)numLogics : 0);
    put16(LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET + 2,
          numPictures > 0 ? (uint16_t)numPictures : 0);
    put16(LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET + 4,
          numViews > 0 ? (uint16_t)numViews : 0);
    put16(LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET + 6,
          numSounds > 0 ? (uint16_t)numSounds : 0);
    b5GetLogicFile(&logicZero, 0);
    put16(LOGIC_APP_REPLAY_LOGIC0_SIZE_OFFSET,
          (uint16_t)logicZero.codeSize);
    logicAppReplayOutput[LOGIC_APP_REPLAY_LOGIC0_BANK_OFFSET] =
        logicZero.codeBank;

    frameCount = logicContextDepth();
    if (frameCount > LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX) {
        capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_CONTEXT_TRUNCATED;
        frameCount = LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX;
    }
    logicAppReplayOutput[LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET] =
        frameCount;
#if LOGIC_CONTEXT_TRACE
    for (frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        LogicContextReplayFrame frame;
        uint16_t offset = LOGIC_APP_REPLAY_CONTEXT_FRAMES_OFFSET +
            (uint16_t)frameIndex * LOGIC_APP_REPLAY_CONTEXT_FRAME_RECORD_SIZE;
        if (!logicContextReplayFrameGet(frameIndex, &frame)) {
            capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_CONTEXT_TRUNCATED;
            logicAppReplayOutput[LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET] =
                frameIndex;
            break;
        }
        logicAppReplayOutput[offset] = frame.depth;
        logicAppReplayOutput[offset + 1] = frame.callerLogic;
        logicAppReplayOutput[offset + 2] = frame.calledLogic;
        logicAppReplayOutput[offset + 3] = frame.callOpcode;
        logicAppReplayOutput[offset + 4] = (uint8_t)frame.logicalPc;
        logicAppReplayOutput[offset + 5] = (uint8_t)(frame.logicalPc >> 8);
        logicAppReplayOutput[offset + 6] = frame.codeBank;
        logicAppReplayOutput[offset + 7] = frame.ramBank;
        put16(offset + 8, frame.generation);
        logicAppReplayOutput[offset + 10] = frame.abandoned;
        logicAppReplayOutput[offset + 11] = frame.windowInvalid;
    }
#else
    if (frameCount)
        capacityFlags |= LOGIC_APP_REPLAY_CAPACITY_CONTEXT_TRUNCATED;
    logicAppReplayOutput[LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET] = 0;
#endif
    put16(LOGIC_APP_REPLAY_RNG_SEED_OFFSET,
          (uint16_t)((uint16_t)logicAppReplaySeed[0] |
                     ((uint16_t)logicAppReplaySeed[1] << 8)));
    put32(LOGIC_APP_REPLAY_RNG_STATE_OFFSET, replayRandomState);
    put16(LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET, capacityFlags);
    if (capacityFlags)
        logicAppReplayOutput[58] |=
            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    put16(LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET, inputWaitPolls);
    put32(LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET, lastInputFrame);
    logicAppReplayOutput[LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET] = lastInputKey;
    put32(LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET, replayElapsedVsync);
    put32(LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET, replayScheduleFrame);

    logicAppReplayOutputLength = LOGIC_APP_REPLAY_OUTPUT_SIZE;
}


#endif
