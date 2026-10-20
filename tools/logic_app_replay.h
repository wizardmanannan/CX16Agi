#ifndef CX16AGI_LOGIC_APP_REPLAY_H
#define CX16AGI_LOGIC_APP_REPLAY_H

#include <stdint.h>

/* Build names and case-sensitive loader names for every executable image. */
#define LOGIC_APP_REPLAY_IMAGES(X) \
    X("agi.cx16", "agi.cx16") \
    X("agi.cx16.INIT", "agi.cx16.init") \
    X("agi.cx16.01", "agi.cx16.01") \
    X("agi.cx16.02", "agi.cx16.02") \
    X("agi.cx16.03", "agi.cx16.03") \
    X("agi.cx16.04", "agi.cx16.04") \
    X("agi.cx16.05", "agi.cx16.05") \
    X("agi.cx16.06", "agi.cx16.06") \
    X("agi.cx16.07", "agi.cx16.07") \
    X("agi.cx16.08", "agi.cx16.08") \
    X("agi.cx16.09", "agi.cx16.09") \
    X("agi.cx16.0A", "agi.cx16.0A") \
    X("agi.cx16.0B", "agi.cx16.0B") \
    X("agi.cx16.0C", "agi.cx16.0C") \
    X("agi.cx16.0D", "agi.cx16.0D") \
    X("agi.cx16.0E", "agi.cx16.0E") \
    X("agi.cx16.0F", "agi.cx16.0F") \
    X("agi.cx16.10", "agi.cx16.10") \
    X("agi.cx16.11", "agi.cx16.11") \
    X("agi.cx16.12", "agi.cx16.12") \
    X("agi.cx16.DEBUG", "agi.cx16.debug") \
    X("agi.cx16.SPRITEDEBUG", "agi.cx16.spritedebug")
#define LOGIC_APP_REPLAY_DEFAULT_PROGRAM "obj/cx16/logic-app-replay/agi.cx16"

#define LOGIC_APP_REPLAY_INPUT_MAX 64u
#define LOGIC_APP_REPLAY_INPUT_RECORD_SIZE 5u
#define LOGIC_APP_REPLAY_OBJECT_MAX 20u
#define LOGIC_APP_REPLAY_OBJECT_RECORD_SIZE 14u
#define LOGIC_APP_REPLAY_INVENTORY_MAX 255u
#define LOGIC_APP_REPLAY_INVENTORY_RECORD_SIZE 2u
#define LOGIC_APP_REPLAY_MESSAGE_MAX 16u
#define LOGIC_APP_REPLAY_MESSAGE_TEXT_MAX 255u
#define LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE \
    (3u + LOGIC_APP_REPLAY_MESSAGE_TEXT_MAX)
#define LOGIC_APP_REPLAY_TRACE_MAX 32u
#define LOGIC_APP_REPLAY_TRACE_RECORD_SIZE 25u
#define LOGIC_APP_REPLAY_SNAPSHOT_VERSION 4u
#define LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX 8u
#define LOGIC_APP_REPLAY_CONTEXT_FRAME_RECORD_SIZE 12u
#define LOGIC_APP_REPLAY_RNG_ALGORITHM "scummvm-randomsource-v1"
#define LOGIC_APP_REPLAY_INPUT_WAIT_LIMIT 256u
#define LOGIC_APP_REPLAY_SEMANTIC_KEY_UP 0x80u
#define LOGIC_APP_REPLAY_SEMANTIC_KEY_DOWN 0x81u
#define LOGIC_APP_REPLAY_SEMANTIC_KEY_LEFT 0x82u
#define LOGIC_APP_REPLAY_SEMANTIC_KEY_RIGHT 0x83u
#define LOGIC_APP_REPLAY_FAULT_TRIGGER_FRAME 6u
#define LOGIC_APP_REPLAY_FAULT_TRIGGER_PETSCII 0xCCu
#define LOGIC_APP_REPLAY_OUTPUT_FILE "replay.out"
#define LOGIC_APP_REPLAY_DONE_FILE "replay.done"

#define LOGIC_APP_REPLAY_OUTPUT_PREFIX_SIZE 64u
#define LOGIC_APP_REPLAY_VARS_OFFSET 64u
#define LOGIC_APP_REPLAY_FLAGS_OFFSET 320u
#define LOGIC_APP_REPLAY_LOADED_LOGICS_OFFSET 576u
#define LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET 608u
#define LOGIC_APP_REPLAY_OBJECTS_OFFSET 609u
#define LOGIC_APP_REPLAY_INVENTORY_OFFSET 889u
#define LOGIC_APP_REPLAY_MESSAGES_OFFSET \
    (LOGIC_APP_REPLAY_INVENTORY_OFFSET + \
     LOGIC_APP_REPLAY_INVENTORY_MAX * LOGIC_APP_REPLAY_INVENTORY_RECORD_SIZE)
#define LOGIC_APP_REPLAY_TRACE_OFFSET \
    (LOGIC_APP_REPLAY_MESSAGES_OFFSET + \
     LOGIC_APP_REPLAY_MESSAGE_MAX * LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE)
#define LOGIC_APP_REPLAY_PARSER_OFFSET \
    (LOGIC_APP_REPLAY_TRACE_OFFSET + \
     LOGIC_APP_REPLAY_TRACE_MAX * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE)
#define LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET \
    (LOGIC_APP_REPLAY_PARSER_OFFSET + 22u)
#define LOGIC_APP_REPLAY_LOGIC0_SIZE_OFFSET \
    (LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET + 8u)
#define LOGIC_APP_REPLAY_LOGIC0_BANK_OFFSET \
    (LOGIC_APP_REPLAY_LOGIC0_SIZE_OFFSET + 2u)
#define LOGIC_APP_REPLAY_WORD_COUNT_OFFSET \
    (LOGIC_APP_REPLAY_LOGIC0_BANK_OFFSET + 1u)
#define LOGIC_APP_REPLAY_BASE_OUTPUT_SIZE \
    (LOGIC_APP_REPLAY_WORD_COUNT_OFFSET + 2u)
#define LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET \
    LOGIC_APP_REPLAY_BASE_OUTPUT_SIZE
#define LOGIC_APP_REPLAY_CONTEXT_FRAMES_OFFSET \
    (LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET + 1u)
#define LOGIC_APP_REPLAY_RNG_SEED_OFFSET \
    (LOGIC_APP_REPLAY_CONTEXT_FRAMES_OFFSET + \
     LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX * \
         LOGIC_APP_REPLAY_CONTEXT_FRAME_RECORD_SIZE)
#define LOGIC_APP_REPLAY_RNG_STATE_OFFSET \
    (LOGIC_APP_REPLAY_RNG_SEED_OFFSET + 2u)
#define LOGIC_APP_REPLAY_RNG_STATE_SIZE 4u
#define LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET \
    (LOGIC_APP_REPLAY_RNG_STATE_OFFSET + LOGIC_APP_REPLAY_RNG_STATE_SIZE)
#define LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET \
    (LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET + 2u)
#define LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET \
    (LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET + 2u)
#define LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET \
    (LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET + 4u)
#define LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET \
    (LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET + 1u)
#define LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET \
    (LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET + 4u)
#define LOGIC_APP_REPLAY_OUTPUT_SIZE \
    (LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET + 4u)

enum {
    LOGIC_APP_REPLAY_STATUS_INPUT_MISSED = 1u << 0,
    LOGIC_APP_REPLAY_STATUS_MESSAGE_OVERFLOW = 1u << 1,
    LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED = 1u << 2,
    LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED = 1u << 3,
    LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY = 1u << 4,
    LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED = 1u << 5,
    LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED = 1u << 6
};

enum {
    LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED = 1u << 0,
    LOGIC_APP_REPLAY_CAPACITY_MESSAGE_EVICTED = 1u << 1,
    LOGIC_APP_REPLAY_CAPACITY_MESSAGE_TEXT_TRUNCATED = 1u << 2,
    LOGIC_APP_REPLAY_CAPACITY_INVENTORY_TRUNCATED = 1u << 3,
    LOGIC_APP_REPLAY_CAPACITY_CONTEXT_TRUNCATED = 1u << 4,
    LOGIC_APP_REPLAY_CAPACITY_INPUT_COUNT_TRUNCATED = 1u << 5,
    LOGIC_APP_REPLAY_CAPACITY_MESSAGE_DROP_COUNT_SATURATED = 1u << 6,
    LOGIC_APP_REPLAY_CAPACITY_OBJECT_ENTRY_MISMATCH = 1u << 7,
    LOGIC_APP_REPLAY_CAPACITY_TICK_LIMIT_UNSUPPORTED = 1u << 8
};

enum {
    LOGIC_APP_REPLAY_FAULT_OMIT_METADATA_SAVE = 1u,
    LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE = 2u
};

/* Test-target request block. Input events contain VSYNC frame offset:u32le
 * and target PETSCII:u8. Host corpus traces use ASCII/control plus the four
 * semantic arrow tokens above, translated to PETSCII at the X16 boundary. */
#if defined(LOGIC_APP_REPLAY_TARGET)
extern volatile uint8_t logicAppReplaySeed[2];
extern volatile uint8_t logicAppReplayEventCount;
extern volatile uint8_t logicAppReplayEvents[LOGIC_APP_REPLAY_INPUT_MAX]
                                             [LOGIC_APP_REPLAY_INPUT_RECORD_SIZE];
extern volatile uint8_t logicAppReplayFaultFlags;
extern volatile uint8_t logicAppReplayRunRequested;
extern volatile uint32_t logicAppReplayTickLimit;
extern volatile uint8_t logicAppReplayReady;
extern volatile uint16_t logicAppReplayOutputLength;
extern volatile uint8_t logicAppReplayOutput[LOGIC_APP_REPLAY_OUTPUT_SIZE];
extern volatile uint8_t logicAppReplayRandomIndex;
extern volatile uint32_t logicAppReplayRandomCalls;

void logicAppReplayBootBody(void);
void logicAppReplayStepBody(void);
void logicAppReplayRunTarget(void);
void logicAppReplayAdvanceWait(unsigned int frames);
void logicAppReplayAdvanceMilliseconds(uint32_t milliseconds);
void logicAppReplayResetCycleClock(void);
int logicAppReplayReadInput(int *output);
uint8_t __fastcall__ logicAppReplayRandomNumber(uint8_t max);
void logicAppReplayRecordMessage(uint8_t logic, uint8_t message,
                                 const char *text, uint8_t bank);
#endif

#endif
