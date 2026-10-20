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


#endif
