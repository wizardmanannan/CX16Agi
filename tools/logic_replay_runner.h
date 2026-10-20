#ifndef CX16AGI_LOGIC_REPLAY_RUNNER_H
#define CX16AGI_LOGIC_REPLAY_RUNNER_H

#include "logic_replay.h"
#include "logic_oracle.h"

#define LOGIC_REPLAY_PROVENANCE_MAX 128
#define LOGIC_REPLAY_DETAIL_MAX 128
#define LOGIC_REPLAY_OBJECT_MAX 256

typedef struct LogicReplayObject {
    uint8_t id;
    int16_t x;
    int16_t y;
    uint8_t view;
    uint8_t loop;
    uint8_t cel;
    uint8_t motion;
} LogicReplayObject;

typedef enum LogicReplayOutcome {
    LOGIC_REPLAY_OUTCOME_PASS = 0,
    LOGIC_REPLAY_OUTCOME_MISMATCH = 1,
    LOGIC_REPLAY_OUTCOME_CRASH = 2,
    LOGIC_REPLAY_OUTCOME_TIMEOUT = 3,
    LOGIC_REPLAY_OUTCOME_RUNNER_ERROR = 4
} LogicReplayOutcome;

enum LogicReplayFaultFlags {
    LOGIC_REPLAY_FAULT_NONE = 0,
    LOGIC_REPLAY_FAULT_OMIT_METADATA_SAVE = 1u
};

typedef struct LogicReplayResult {
    LogicReplayOutcome outcome;
    LogicFailureSignature signature;
    uint32_t semantic_hash;
    uint32_t events_consumed;
    uint32_t tick_count;
    uint32_t ticks_executed;
    uint32_t rng_call_count;
    uint32_t rng_final_state;
    uint8_t variables[256];
    uint8_t flags[256];
    uint16_t room;
    uint16_t object_count;
    LogicReplayObject objects[LOGIC_REPLAY_OBJECT_MAX];
    char provenance[LOGIC_REPLAY_PROVENANCE_MAX];
    char detail[LOGIC_REPLAY_DETAIL_MAX];
} LogicReplayResult;

/* Runs the CX16 production interpreter in the headless X16 emulator. Returns
 * zero when the run completed and result->outcome describes its result; -1
 * means the request or runner setup was invalid. The timeout bounds one run. */
int logic_replay_run(const LogicReplayManifest *manifest,
                     const LogicReplayEvent *events, size_t event_count,
                     const char *resource_dir, uint32_t timeout_ms,
                     unsigned fault_flags, LogicReplayResult *result);

int logic_replay_run_ticks(const LogicReplayManifest *manifest,
                           const LogicReplayEvent *events, size_t event_count,
                           const char *resource_dir, uint32_t tick_count,
                           uint32_t timeout_ms, unsigned fault_flags,
                           LogicReplayResult *result);

/* Converts a complete, successful production-app capture. Rejects truncated
 * capture/identity data; unavailable control-flow remains marked unreliable. */
int logic_app_replay_snapshot_to_oracle(
    const char *game_id, uint16_t agi_version,
    const LogicReplayManifest *manifest,
    const uint8_t manifest_hash[LOGIC_ORACLE_SHA256_SIZE],
    const uint8_t input_hash[LOGIC_ORACLE_SHA256_SIZE],
    const uint8_t *raw, size_t raw_size, LogicOracleSnapshot *snapshot,
    char *detail, size_t detail_size);

const char *logic_replay_outcome_name(LogicReplayOutcome outcome);

#endif
