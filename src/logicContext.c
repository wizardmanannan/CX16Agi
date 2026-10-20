#include "logicContext.h"
#include <stddef.h>
#if LOGIC_CONTEXT_TRACE
#include "memoryManager.h"
#endif

#define ZP_CODE_PTR (*(volatile word *)0x00A9)
#define ZP_ENTRY_PTR (*(volatile word *)0x00E9)
#define ZP_FILE_PTR (*(volatile word *)0x00E7)
#define RAM_BANK_REGISTER (*(volatile byte *)0x0000)

extern volatile word startPos;
extern volatile word endPos;
extern volatile byte codeBank;
extern volatile word cwCurrentCode;
extern volatile byte codeWindowInvalid;
extern LOGICEntry assmLogicEntry;
extern LOGICFile assmLogicFile;
extern int currentLog;
extern boolean exitAllLogics;

#ifndef LOGIC_CONTEXT_TRACE
#define LOGIC_CONTEXT_TRACE 0
#endif

#ifdef LOGIC_CONTEXT_VERIFY
volatile byte logicContextVerifyFailed;
volatile byte logicContextVerifyField;
volatile byte logicContextVerifyOffset;
volatile word logicContextVerifyPc;
volatile unsigned long logicContextVerifyExpected;
volatile unsigned long logicContextVerifyActual;
#endif
#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
volatile byte logicContextHardwareSp;
volatile word logicContextSoftwareSp;
#endif

/* First 14+metadata bytes are the serialized caller context used by the
   assembly copier. Diagnostic bookkeeping follows that payload. */
typedef struct {
    word pc;
    word filePointer;
    word entryPointer;
    int currentLog;
    byte ramBank;
    LOGICEntry entry;
    LOGICFile file;
    word start;
    word end;
    byte codeBank;
    byte abandoned;
#if LOGIC_CONTEXT_TRACE
    byte windowInvalid;
#endif
#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
    byte hardwareSp;
    word softwareSp;
    byte depth;
    word generation;
#endif
#if LOGIC_CONTEXT_TRACE
    byte callOpcode;
    byte calledLogic;
    word callSitePc;
#endif
} LogicContextFrame;

/* Compile-time layout checks protect the assembly serialization contract. */
typedef char LogicContextLayoutCheck[
    sizeof(LOGICEntry) == 9 && sizeof(LOGICFile) == 9 &&
    offsetof(LogicContextFrame, pc) == 0 &&
    offsetof(LogicContextFrame, filePointer) == 2 &&
    offsetof(LogicContextFrame, currentLog) == 6 &&
    offsetof(LogicContextFrame, ramBank) == 8 &&
    offsetof(LogicContextFrame, entry) == 9 &&
    offsetof(LogicContextFrame, file) == 9 + sizeof(LOGICEntry) &&
    offsetof(LogicContextFrame, start) == 9 + sizeof(LOGICEntry) + sizeof(LOGICFile) &&
    offsetof(LogicContextFrame, codeBank) == 13 + sizeof(LOGICEntry) + sizeof(LOGICFile) &&
    offsetof(LogicContextFrame, abandoned) == 14 + sizeof(LOGICEntry) + sizeof(LOGICFile)
    ? 1 : -1];

extern void __fastcall__ logicContextCaptureCaller(void *state);
extern void __fastcall__ logicContextRestoreCaller(const void *state);


#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
static LogicContextFrame frames[LOGIC_CONTEXT_MAX_DEPTH];
#ifdef LOGIC_CONTEXT_VERIFY
/* This independently declared snapshot intentionally does not share the
   restore-frame type or its field list. */
typedef struct {
    word start;
    word end;
    word pc;
    word entryPointer;
    word filePointer;
    int currentLog;
    byte codeBank;
    byte ramBank;
    byte hardwareSp;
    word softwareSp;
    byte depth;
    word generation;
    byte abandoned;
    LOGICEntry entry;
    LOGICFile file;
} LogicContextShadow;
static LogicContextShadow shadowFrames[LOGIC_CONTEXT_MAX_DEPTH];
static LogicContextShadow actualState;
#endif
static byte contextDepth;
#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
static word contextGeneration;
#endif
static byte contextLastError;
#if LOGIC_CONTEXT_TRACE
static byte contextHardwareStackHighWater;
static unsigned int contextSoftwareStackHighWater;
static byte contextHardwareStackBase;
static word contextSoftwareStackBase;
#endif
#ifdef LOGIC_CONTEXT_VERIFY
static byte verifierDepth;
static word verifierGeneration;
static byte contextMismatchField;
static byte contextMismatchOffset;
static byte contextMismatchLogicValue;
static byte contextMismatchOpcodeValue;
static word contextMismatchPc;
static unsigned long contextMismatchExpected;
static unsigned long contextMismatchActual;
static byte contextTestDepthPending;
static byte contextTestGenerationPending;
static byte contextTestDepthRestore;
static byte contextTestGenerationRestore;
#ifdef LOGIC_CONTEXT_FAULT_INJECTION
static byte contextTestOmitMetadataSave;
#endif
#endif


#ifdef LOGIC_CONTEXT_VERIFY
/* Fault injection for the permanent verifier regression in the test build. */
void logicContextTestCorruptDepth(void)
{
    contextTestDepthPending = TRUE;
}

void logicContextTestCorruptGeneration(void)
{
    contextTestGenerationPending = TRUE;
}

void logicContextTestCorruptAbandonment(void)
{
    if (contextDepth)
        frames[contextDepth - 1].abandoned = TRUE;
}

void logicContextTestClearMismatch(void)
{
    logicContextVerifyFailed = FALSE;
    logicContextVerifyField = 0;
    logicContextVerifyOffset = 0;
    logicContextVerifyPc = 0;
    logicContextVerifyExpected = 0;
    logicContextVerifyActual = 0;
    contextMismatchField = 0;
    contextMismatchOffset = 0;
    contextMismatchLogicValue = 0;
    contextMismatchOpcodeValue = 0;
    contextMismatchPc = 0;
    contextMismatchExpected = 0;
    contextMismatchActual = 0;
    contextTestDepthPending = FALSE;
    contextTestGenerationPending = FALSE;
    contextTestDepthRestore = FALSE;
    contextTestGenerationRestore = FALSE;
    contextLastError = LOGIC_CONTEXT_OK;
}

#if defined(LOGIC_CONTEXT_FAULT_INJECTION)
void logicContextTestOmitMetadataSave(void)
{
    contextTestOmitMetadataSave = TRUE;
}
#endif
#endif

#if LOGIC_CONTEXT_TRACE
#define LOGIC_CONTEXT_TRACE_CAPACITY 64
#define LOGIC_CONTEXT_TRACE_UNINITIALIZED 0
#define LOGIC_CONTEXT_TRACE_READY 1
#define LOGIC_CONTEXT_TRACE_UNAVAILABLE 2
#define LOGIC_CONTEXT_TRACE_STORAGE_BYTES \
    (LOGIC_CONTEXT_TRACE_CAPACITY * sizeof(LogicTraceEvent))
#if defined(LOGIC_APP_REPLAY_TARGET)
static LogicTraceEvent *contextTraceEvents;
static byte contextTraceBank;
#else
static LogicTraceEvent contextTraceEvents[LOGIC_CONTEXT_TRACE_CAPACITY];
#endif
static LogicTraceRing contextTraceRing;
static byte contextTraceInitialized;
volatile byte logicContextOpcode;
volatile word logicContextInstructionPc;
volatile byte logicContextRequestedLogic;
volatile byte logicContextWriteField;
volatile byte logicContextWriteBefore;
volatile byte logicContextWriteAfter;

uint32_t logicContextTick;

static void recordTraceEvent(byte kind, byte logic, word pc, byte opcode,
                             byte depth, byte bank, byte field,
                             unsigned long before, unsigned long after,
                             byte reason)
{
    LogicTraceEvent event;
#if defined(LOGIC_APP_REPLAY_TARGET)
    byte previousRamBank = RAM_BANK_REGISTER;
    if (contextTraceInitialized == LOGIC_CONTEXT_TRACE_UNAVAILABLE) {
        RAM_BANK_REGISTER = previousRamBank;
        return;
    }
    if (contextTraceInitialized == LOGIC_CONTEXT_TRACE_UNINITIALIZED) {
        contextTraceEvents = (LogicTraceEvent *)b10BankedAlloc(
            LOGIC_CONTEXT_TRACE_STORAGE_BYTES, &contextTraceBank);
        if (!contextTraceEvents) {
            contextTraceInitialized = LOGIC_CONTEXT_TRACE_UNAVAILABLE;
            RAM_BANK_REGISTER = previousRamBank;
            return;
        }
        RAM_BANK_REGISTER = contextTraceBank;
        if (logic_trace_ring_init(&contextTraceRing, contextTraceEvents,
                                  LOGIC_CONTEXT_TRACE_CAPACITY) != 0) {
            contextTraceInitialized = LOGIC_CONTEXT_TRACE_UNAVAILABLE;
            RAM_BANK_REGISTER = previousRamBank;
            return;
        }
        contextTraceInitialized = LOGIC_CONTEXT_TRACE_READY;
    } else {
        RAM_BANK_REGISTER = contextTraceBank;
    }
#else
    if (contextTraceInitialized == LOGIC_CONTEXT_TRACE_UNAVAILABLE)
        return;
    if (contextTraceInitialized == LOGIC_CONTEXT_TRACE_UNINITIALIZED) {
        if (logic_trace_ring_init(&contextTraceRing, contextTraceEvents,
                                  LOGIC_CONTEXT_TRACE_CAPACITY) != 0) {
            contextTraceInitialized = LOGIC_CONTEXT_TRACE_UNAVAILABLE;
            return;
        }
        contextTraceInitialized = LOGIC_CONTEXT_TRACE_READY;
    }
#endif
    event.sequence = 0;
    event.tick = logicContextTick;
    event.kind = kind;
    event.logic = logic;
    event.logical_pc = pc;
    event.opcode = opcode;
    event.depth = depth;
    event.bank = bank;
    event.field = field;
    event.before = (uint32_t)before;
    event.after = (uint32_t)after;
    event.reason = reason;
    (void)logic_trace_record(&contextTraceRing, &event);
#if defined(LOGIC_APP_REPLAY_TARGET)
    RAM_BANK_REGISTER = previousRamBank;
#endif
}

byte logicContextTraceCount(void)
{
    return contextTraceInitialized == LOGIC_CONTEXT_TRACE_READY
        ? (byte)logic_trace_ring_count(&contextTraceRing) : 0;
}

uint32_t logicContextTraceDropped(void)
{
#if defined(LOGIC_APP_REPLAY_TARGET)
    if (contextTraceInitialized == LOGIC_CONTEXT_TRACE_UNAVAILABLE)
        return 1;
#endif
    return logic_trace_ring_dropped(&contextTraceRing);
}

byte logicContextTraceGet(byte index, LogicTraceEvent *event)
{
#if defined(LOGIC_APP_REPLAY_TARGET)
    byte previousRamBank = RAM_BANK_REGISTER;
#endif
    byte result;
    if (contextTraceInitialized != LOGIC_CONTEXT_TRACE_READY)
        return FALSE;
#if defined(LOGIC_APP_REPLAY_TARGET)
    RAM_BANK_REGISTER = contextTraceBank;
#endif
    result = logic_trace_ring_get(&contextTraceRing, index, event) == 0;
#if defined(LOGIC_APP_REPLAY_TARGET)
    RAM_BANK_REGISTER = previousRamBank;
#endif
    return result;
}

void logicContextRecordPendingWrite(void)
{
    recordTraceEvent(LOGIC_TRACE_KIND_WRITE, (byte)currentLog,
                     logicContextInstructionPc, logicContextOpcode,
                     contextDepth, codeBank, logicContextWriteField,
                     logicContextWriteBefore, logicContextWriteAfter,
                     LOGIC_TRACE_WRITE_REASON_UNSPECIFIED);
}

void logicContextWriteVariable(byte field, byte value)
{
    byte before = GOLDEN_RAM[VARS_AREA_START + field];
    GOLDEN_RAM[VARS_AREA_START + field] = value;
    recordTraceEvent(LOGIC_TRACE_KIND_WRITE, (byte)currentLog,
                     logicContextInstructionPc, logicContextOpcode,
                     contextDepth, codeBank, field, before, value,
                     LOGIC_TRACE_WRITE_REASON_VARIABLE);
}

void logicContextWriteFlag(byte field, boolean value)
{
    boolean before = GOLDEN_RAM[FLAGS_AREA_START + field];
    GOLDEN_RAM[FLAGS_AREA_START + field] = value;
    recordTraceEvent(LOGIC_TRACE_KIND_WRITE, (byte)currentLog,
                     logicContextInstructionPc, logicContextOpcode,
                     contextDepth, codeBank, field, before, value,
                     LOGIC_TRACE_WRITE_REASON_FLAG);
}
#endif

#if LOGIC_CONTEXT_TRACE
static void captureState(LogicContextFrame *state)
{
    logicContextCaptureCaller(state);
    state->windowInvalid = codeWindowInvalid;
}
#else
#define captureState logicContextCaptureCaller
#endif

#ifdef LOGIC_CONTEXT_VERIFY
static void captureShadow(LogicContextShadow *state)
{
    state->start = startPos;
    state->end = endPos;
    state->pc = ZP_CODE_PTR + cwCurrentCode;
    state->entryPointer = ZP_ENTRY_PTR;
    state->filePointer = ZP_FILE_PTR;
    state->currentLog = currentLog;
    state->codeBank = codeBank;
    state->ramBank = RAM_BANK_REGISTER;
    state->entry = assmLogicEntry;
    state->file = assmLogicFile;
}

static void captureShadowStack(LogicContextShadow *state)
{
    state->hardwareSp = logicContextHardwareSp;
    state->softwareSp = logicContextSoftwareSp;
}
#endif

#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
static void captureStackState(LogicContextFrame *state)
{
    state->hardwareSp = logicContextHardwareSp;
    state->softwareSp = logicContextSoftwareSp;
}
#endif

#if defined(LOGIC_CONTEXT_VERIFY) && defined(LOGIC_CONTEXT_FAULT_INJECTION)
static void restoreState(const LogicContextFrame *state)
{
    LOGICEntry childEntry;
    childEntry = assmLogicEntry;
    logicContextRestoreCaller(state);
    if (contextTestOmitMetadataSave) {
        contextTestOmitMetadataSave = FALSE;
        assmLogicEntry = childEntry;
    }
}
#else
#define restoreState logicContextRestoreCaller
#endif

byte logicContextEnter(void)
{
    LogicContextFrame *frame;

    if (contextDepth >= LOGIC_CONTEXT_MAX_DEPTH) {
        contextLastError = LOGIC_CONTEXT_OVERFLOW;
        exitAllLogics = TRUE;
        return FALSE;
    }

    frame = &frames[contextDepth];
    captureState(frame);
#ifdef LOGIC_CONTEXT_VERIFY
    captureShadow(&shadowFrames[contextDepth]);
#endif
#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
    frame->depth = contextDepth;
    frame->generation = ++contextGeneration;
#endif
    frame->abandoned = FALSE;
#if LOGIC_CONTEXT_TRACE
    frame->callOpcode = logicContextOpcode;
    frame->calledLogic = logicContextRequestedLogic;
    frame->callSitePc = logicContextInstructionPc;
#endif
#if LOGIC_CONTEXT_TRACE || defined(LOGIC_CONTEXT_VERIFY)
    captureStackState(frame);
#endif
#if LOGIC_CONTEXT_TRACE
    if (contextDepth == 0) {
        contextHardwareStackBase = frame->hardwareSp;
        contextSoftwareStackBase = frame->softwareSp;
        contextHardwareStackHighWater = 0;
        contextSoftwareStackHighWater = 0;
    } else {
        byte hardwareBytes = (byte)(contextHardwareStackBase - frame->hardwareSp);
        word softwareBytes = frame->softwareSp >= contextSoftwareStackBase
            ? (word)(frame->softwareSp - contextSoftwareStackBase)
            : (word)(contextSoftwareStackBase - frame->softwareSp);
        if (hardwareBytes > contextHardwareStackHighWater)
            contextHardwareStackHighWater = hardwareBytes;
        if (softwareBytes > contextSoftwareStackHighWater)
            contextSoftwareStackHighWater = softwareBytes;
    }
#endif
#ifdef LOGIC_CONTEXT_VERIFY
    shadowFrames[contextDepth].depth = verifierDepth;
    ++verifierGeneration;
    shadowFrames[contextDepth].generation = verifierGeneration;
    shadowFrames[contextDepth].abandoned = FALSE;
    captureShadowStack(&shadowFrames[contextDepth]);
#endif
    ++contextDepth;
#ifdef LOGIC_CONTEXT_VERIFY
    ++verifierDepth;
#endif
#if LOGIC_CONTEXT_TRACE
    recordTraceEvent(LOGIC_TRACE_KIND_CALL, (byte)frame->currentLog,
                     frame->callSitePc,
                     frame->callOpcode, contextDepth, frame->codeBank,
                     LOGIC_CONTEXT_FIELD_CURRENT_LOG,
                     (unsigned int)frame->currentLog, frame->calledLogic, 0);
#endif
    contextLastError = LOGIC_CONTEXT_OK;
    return TRUE;
}

#ifdef LOGIC_CONTEXT_VERIFY
static void mismatch(byte field, byte offset,
                     unsigned long expected, unsigned long actual,
                     word logicalPc)
{
    if (logicContextVerifyFailed)
        return;
    logicContextVerifyFailed = TRUE;
    logicContextVerifyField = field;
    logicContextVerifyOffset = offset;
    logicContextVerifyPc = logicalPc;
    logicContextVerifyExpected = expected;
    logicContextVerifyActual = actual;
    contextMismatchField = field;
    contextMismatchOffset = offset;
    contextMismatchLogicValue = (byte)currentLog;
    contextMismatchPc = logicalPc;
    contextMismatchExpected = expected;
    contextMismatchActual = actual;
    contextLastError = LOGIC_CONTEXT_VERIFY_MISMATCH;
    exitAllLogics = TRUE;
}

static void compareValue(byte field, unsigned long expected,
                         unsigned long actual, word logicalPc)
{
    if (expected != actual)
        mismatch(field, 0, expected, actual, logicalPc);
}

static void compareBytes(byte field, const byte *expected,
                         const byte *actual, unsigned int size,
                         word logicalPc)
{
    unsigned int offset;
    for (offset = 0; offset < size; ++offset) {
        if (expected[offset] != actual[offset]) {
            mismatch(field, (byte)offset, expected[offset], actual[offset], logicalPc);
            return;
        }
    }
}

static void verifyRestoredState(const LogicContextShadow *expected,
                                byte expectedDepth, word expectedGeneration)
{
    captureShadow(&actualState);
    captureShadowStack(&actualState);
    actualState.depth = contextDepth;
    actualState.generation = contextGeneration;

    compareValue(LOGIC_CONTEXT_FIELD_START, expected->start, actualState.start, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_END, expected->end, actualState.end, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_PC, expected->pc, actualState.pc, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CODE_BANK, expected->codeBank, actualState.codeBank, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_ENTRY_POINTER, expected->entryPointer, actualState.entryPointer, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_FILE_POINTER, expected->filePointer, actualState.filePointer, expected->pc);
    compareBytes(LOGIC_CONTEXT_FIELD_ENTRY,
                 (const byte *)&expected->entry,
                 (const byte *)&actualState.entry,
                 sizeof expected->entry, expected->pc);
    compareBytes(LOGIC_CONTEXT_FIELD_FILE,
                 (const byte *)&expected->file,
                 (const byte *)&actualState.file,
                 sizeof expected->file, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CURRENT_LOG, (unsigned int)expected->currentLog,
                 (unsigned int)actualState.currentLog, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_RAM_BANK, expected->ramBank, actualState.ramBank, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CODE_WINDOW_OFFSET, 0, cwCurrentCode, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CODE_WINDOW_INVALID, TRUE, codeWindowInvalid, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_HARDWARE_SP, expected->hardwareSp,
                 actualState.hardwareSp, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_SOFTWARE_SP, expected->softwareSp,
                 actualState.softwareSp, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_DEPTH, expectedDepth, contextDepth, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_GENERATION, expectedGeneration,
                 contextGeneration, expected->pc);
}

static void verifyAbandonedState(const LogicContextShadow *expected,
                                 byte expectedDepth, word expectedGeneration)
{
    captureShadow(&actualState);
    captureShadowStack(&actualState);
    compareValue(LOGIC_CONTEXT_FIELD_CURRENT_LOG,
                 (unsigned int)expected->currentLog,
                 (unsigned int)actualState.currentLog, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CODE_WINDOW_OFFSET, 0, cwCurrentCode,
                 expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_CODE_WINDOW_INVALID, TRUE,
                 codeWindowInvalid, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_HARDWARE_SP, expected->hardwareSp,
                 actualState.hardwareSp, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_SOFTWARE_SP, expected->softwareSp,
                 actualState.softwareSp, expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_DEPTH, expectedDepth, contextDepth,
                 expected->pc);
    compareValue(LOGIC_CONTEXT_FIELD_GENERATION, expectedGeneration,
                 contextGeneration, expected->pc);
}
#endif


#endif
