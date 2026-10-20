#ifndef _LOGIC_CONTEXT_H_
#define _LOGIC_CONTEXT_H_

#include "logicTypes.h"
#ifndef LOGIC_CONTEXT_TRACE
#define LOGIC_CONTEXT_TRACE 0
#endif
#include "../tools/logic_trace.h"

#define LOGIC_CONTEXT_MAX_DEPTH 8

/* A saved frame owns the caller's code range/PC and code bank, entry/file
 * pointers and copied LOGICEntry/LOGICFile metadata, currentLog, RAM_BANK,
 * code-window cache state, hardware/software stack positions, and context
 * depth/generation/abandoned state. The verifier's LogicContextShadow keeps
 * an intentionally independent duplicate field list; do not derive it from
 * the restore-frame type. Normal return restores the saved execution bank.
 * Abandonment leaves the new room/restart execution bank in place; banked
 * opcode adapters separately restore the bank required by their own banked
 * RTS continuation before returning to it. */

enum {
    LOGIC_CONTEXT_OK = 0,
    LOGIC_CONTEXT_OVERFLOW = 1,
    LOGIC_CONTEXT_UNDERFLOW = 2,
    LOGIC_CONTEXT_VERIFY_MISMATCH = 3
};

enum {
    LOGIC_CONTEXT_FIELD_START = 1,
    LOGIC_CONTEXT_FIELD_END,
    LOGIC_CONTEXT_FIELD_PC,
    LOGIC_CONTEXT_FIELD_CODE_BANK,
    LOGIC_CONTEXT_FIELD_ENTRY_POINTER,
    LOGIC_CONTEXT_FIELD_FILE_POINTER,
    LOGIC_CONTEXT_FIELD_ENTRY,
    LOGIC_CONTEXT_FIELD_FILE,
    LOGIC_CONTEXT_FIELD_CURRENT_LOG = LOGIC_TRACE_FIELD_CURRENT_LOG,
    LOGIC_CONTEXT_FIELD_RAM_BANK,
    LOGIC_CONTEXT_FIELD_CODE_WINDOW_OFFSET,
    LOGIC_CONTEXT_FIELD_CODE_WINDOW_INVALID,
    LOGIC_CONTEXT_FIELD_HARDWARE_SP,
    LOGIC_CONTEXT_FIELD_SOFTWARE_SP,
    LOGIC_CONTEXT_FIELD_DEPTH,
    LOGIC_CONTEXT_FIELD_GENERATION,
    LOGIC_CONTEXT_FIELD_ABANDONED
};

/* Call ownership contract:
 * Caller: code bounds/PC/bank, entry/file pointers and values, current logic,
 * RAM bank, and balanced hardware/software stacks. Normal return restores it.
 * Shared: game variables, flags, objects, resources and effects survive calls.
 * Derived: the code window is invalidated and rebuilt from restored PC/bank.
 * Room/restart abandon caller state; they still must return through the bank
 * containing the opcode continuation (callLogicFromBank2), then unwind.
 * The verifier's independently declared shadow must not share the save list.
 */
byte logicContextEnter(void);
byte logicContextLeave(void);
void logicContextAbandon(void);

byte logicContextDepth(void);
word logicContextGeneration(void);
byte logicContextError(void);
byte logicContextMismatchField(void);
byte logicContextMismatchOffset(void);
byte logicContextMismatchLogic(void);
byte logicContextMismatchOpcode(void);
word logicContextMismatchPc(void);
unsigned long logicContextMismatchExpected(void);
unsigned long logicContextMismatchActual(void);
unsigned int logicContextStorageBytes(void);
unsigned int logicContextHardwareStackBytes(void);
unsigned int logicContextSoftwareStackBytes(void);

#if !LOGIC_CONTEXT_TRACE && !defined(LOGIC_CONTEXT_VERIFY)
/* Release callers need no emitted diagnostic stubs. */
extern const volatile byte logicContextDepthValue, logicContextErrorValue;
#define logicContextDepth() logicContextDepthValue
#define logicContextError() logicContextErrorValue
#define logicContextGeneration() 0U
#define logicContextMismatchField() 0U
#define logicContextMismatchOffset() 0U
#define logicContextMismatchLogic() 0U
#define logicContextMismatchOpcode() 0U
#define logicContextMismatchPc() 0U
#define logicContextMismatchExpected() 0UL
#define logicContextMismatchActual() 0UL
#define logicContextHardwareStackBytes() 0U
#define logicContextSoftwareStackBytes() 0U
#define logicContextStorageBytes() \
    (LOGIC_CONTEXT_MAX_DEPTH * (15U + sizeof(LOGICEntry) + sizeof(LOGICFile)) + 2U)
#endif

#if LOGIC_CONTEXT_TRACE
extern uint32_t logicContextTick;
/* Pointer-free view of one active saved caller frame. logicalPc is the
 * caller return location relative to its saved logic's start address. */
typedef struct LogicContextReplayFrame {
    byte depth;
    byte callerLogic;
    byte calledLogic;
    byte callOpcode;
    word logicalPc;
    byte codeBank;
    byte ramBank;
    word generation;
    byte abandoned;
    byte windowInvalid;
} LogicContextReplayFrame;

byte logicContextReplayFrameGet(byte index,
                                LogicContextReplayFrame *snapshot);
byte logicContextTraceCount(void);
uint32_t logicContextTraceDropped(void);
byte logicContextTraceGet(byte index, LogicTraceEvent *event);
void logicContextRecordPendingWrite(void);
void logicContextWriteVariable(byte field, byte value);
void logicContextWriteFlag(byte field, boolean value);
extern volatile byte logicContextOpcode;
extern volatile word logicContextInstructionPc;
extern volatile byte logicContextWriteField;
extern volatile byte logicContextWriteBefore;
extern volatile byte logicContextWriteAfter;
#endif

/* Independent verifier output; diagnostics are compiled out unless enabled. */
#ifdef LOGIC_CONTEXT_VERIFY
extern volatile byte logicContextVerifyFailed;
extern volatile byte logicContextVerifyField;
extern volatile byte logicContextVerifyOffset;
extern volatile word logicContextVerifyPc;
extern volatile unsigned long logicContextVerifyExpected;
extern volatile unsigned long logicContextVerifyActual;
#endif
#if defined(LOGIC_CONTEXT_VERIFY) && defined(LOGIC_CONTEXT_FAULT_INJECTION)
void logicContextTestOmitMetadataSave(void);
#endif
extern volatile byte logicContextHardwareSp;
extern volatile word logicContextSoftwareSp;

#endif
