#ifndef CX16AGI_LOGIC_ORACLE_H
#define CX16AGI_LOGIC_ORACLE_H

#include <stddef.h>
#include <stdint.h>

#define LOGIC_ORACLE_ID_SIZE 64
#define LOGIC_ORACLE_PROVENANCE_SIZE 128
#define LOGIC_ORACLE_MESSAGE_TEXT_SIZE 256
#define LOGIC_ORACLE_SHA256_SIZE 32
#define LOGIC_ORACLE_RNG_ALGORITHM_SIZE 32
#define LOGIC_ORACLE_MAX_OBJECTS 256
#define LOGIC_ORACLE_MAX_MESSAGES 256
#define LOGIC_ORACLE_MAX_REFERENCE_TICKS 1024
#define LOGIC_ORACLE_HEADER_SIZE 14
#define LOGIC_ORACLE_SNAPSHOT_VERSION 6
#define LOGIC_ORACLE_TRACE_VERSION 1
#define LOGIC_ORACLE_TRACE_RECORD_SIZE 25
#define LOGIC_ORACLE_SNAPSHOT_RECORD_SIZE \
    (4 + 4 + 4 + 1 + 2 + 1 + 1 + LOGIC_ORACLE_ID_SIZE + LOGIC_ORACLE_ID_SIZE + \
     LOGIC_ORACLE_PROVENANCE_SIZE + LOGIC_ORACLE_SHA256_SIZE * 2 + 8 + \
     LOGIC_ORACLE_RNG_ALGORITHM_SIZE + 8 + 8 + 256 + 256 + 2 + 2 + \
     (LOGIC_ORACLE_MAX_OBJECTS * 9) + 2 + \
     (LOGIC_ORACLE_MAX_MESSAGES * (2 + LOGIC_ORACLE_MESSAGE_TEXT_SIZE)) + \
     1 + 2 + 1 + 1 + 1 + 2 + 1 + 1 + 4 + 1 + 2 + 256)

enum {
    /* Tick equality is diagnostic only; timing_profile must still be compatible. */
    LOGIC_ORACLE_TIMING_AGI_TICK = 1,
    LOGIC_ORACLE_TIMING_FRAME_STEP = 2,
    LOGIC_ORACLE_TIMING_WALL_MILLISECONDS = 3
};

typedef struct LogicOracleObject {
    uint8_t id;
    int16_t x;
    int16_t y;
    uint8_t view;
    uint8_t loop;
    uint8_t cel;
    uint8_t motion;
} LogicOracleObject;

typedef struct LogicOracleMessage {
    uint8_t logic;
    uint8_t id;
    char text[LOGIC_ORACLE_MESSAGE_TEXT_SIZE];
} LogicOracleMessage;

typedef struct LogicOracleSnapshot {
    uint32_t checkpoint_index;
    uint32_t tick;
    uint32_t trace_sequence;
    uint8_t has_trace;
    uint16_t agi_version;
    uint8_t timing_profile;
    uint8_t control_flow_reliable;
    char game_id[LOGIC_ORACLE_ID_SIZE];
    char checkpoint[LOGIC_ORACLE_ID_SIZE];
    char provenance[LOGIC_ORACLE_PROVENANCE_SIZE];
    /* SHA-256 of the manifest bytes and serialized LTRC delivery stream. */
    uint8_t manifest_sha256[LOGIC_ORACLE_SHA256_SIZE];
    uint8_t input_sha256[LOGIC_ORACLE_SHA256_SIZE];
    uint64_t rng_seed;
    uint32_t rng_call_count;
    uint32_t rng_final_state;
    char rng_algorithm[LOGIC_ORACLE_RNG_ALGORITHM_SIZE];
    uint32_t tick_numerator;
    uint32_t tick_denominator;
    uint8_t variables[256];
    uint8_t flags[256];
    uint16_t room;
    uint16_t object_count;
    LogicOracleObject objects[LOGIC_ORACLE_MAX_OBJECTS];
    /* Ordered print events, including repeated source messages. */
    uint16_t message_count;
    LogicOracleMessage messages[LOGIC_ORACLE_MAX_MESSAGES];
    uint8_t logic;
    uint16_t logical_pc;
    /* Optional stable candidate failure identity; contains no host pointers. */
    uint8_t has_failure;
    uint8_t failure_kind;
    uint8_t failure_logic;
    uint16_t failure_logical_pc;
    uint8_t failure_opcode;
    uint8_t failure_reason;
    uint32_t failure_code;
    /* Inventory item id indexes room locations; unknown differs from empty. */
    uint8_t inventory_known;
    uint16_t inventory_count;
    uint8_t inventory_rooms[256];
} LogicOracleSnapshot;

typedef struct LogicOracleTraceEvent {
    uint32_t sequence;
    uint32_t tick;
    uint8_t kind;
    uint8_t logic;
    uint16_t logical_pc;
    uint8_t opcode;
    uint8_t depth;
    uint8_t bank;
    uint8_t field;
    uint32_t before;
    uint32_t after;
    uint8_t reason;
} LogicOracleTraceEvent;

typedef enum LogicOracleStatus {
    LOGIC_ORACLE_MATCH = 0,
    LOGIC_ORACLE_DIVERGED = 1,
    LOGIC_ORACLE_INCOMPATIBLE = 2,
    LOGIC_ORACLE_INVALID = 3
} LogicOracleStatus;

typedef enum LogicOraclePolicy {
    LOGIC_ORACLE_POLICY_DETERMINISTIC = 0,
    LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC = 1
} LogicOraclePolicy;

typedef struct LogicOracleResult {
    LogicOracleStatus status;
    uint32_t checkpoint_index;
    uint32_t reference_trace_sequence;
    uint32_t candidate_trace_sequence;
    uint8_t reference_has_trace;
    uint8_t candidate_has_trace;
    int64_t reference_value;
    int64_t candidate_value;
    char checkpoint[LOGIC_ORACLE_ID_SIZE];
    char field[48];
    char reference_text[LOGIC_ORACLE_MESSAGE_TEXT_SIZE];
    char candidate_text[LOGIC_ORACLE_MESSAGE_TEXT_SIZE];
    char reference_provenance[LOGIC_ORACLE_PROVENANCE_SIZE];
    char candidate_provenance[LOGIC_ORACLE_PROVENANCE_SIZE];
    char detail[128];
} LogicOracleResult;

/* Both policies require matching game/version, timing rate, manifest, input
   schedule, and RNG seed. Deterministic comparison also requires matching RNG
   algorithm. Both policies require the same timing profile and checkpoint
   boundary. Cross-engine semantic comparison permits an RNG algorithm
   mismatch, provided rates and sample tick numbers match. An elapsed wall
   sample cannot substitute for a completed interpreter cycle. All gameplay
   fields remain compared;
   RNG call/state telemetry is skipped only when algorithms differ. Logic/PC
   compare only when both captures mark them reliable. */
LogicOracleStatus logic_oracle_compare(const LogicOracleSnapshot *reference,
                                      const LogicOracleSnapshot *candidate,
                                      LogicOracleResult *result);
LogicOracleStatus logic_oracle_compare_with_policy(
    const LogicOracleSnapshot *reference,
    const LogicOracleSnapshot *candidate,
    LogicOraclePolicy policy, LogicOracleResult *result);
LogicOracleStatus logic_oracle_compare_streams(
    const LogicOracleSnapshot *reference, size_t reference_count,
    const LogicOracleSnapshot *candidate, size_t candidate_count,
    LogicOracleResult *result);
LogicOracleStatus logic_oracle_compare_streams_with_policy(
    const LogicOracleSnapshot *reference, size_t reference_count,
    const LogicOracleSnapshot *candidate, size_t candidate_count,
    LogicOraclePolicy policy, LogicOracleResult *result);

int logic_oracle_snapshot_write(const char *path,
                                const LogicOracleSnapshot *snapshots,
                                size_t count);
int logic_oracle_snapshot_read(const char *path,
                               LogicOracleSnapshot *snapshots,
                               size_t capacity, size_t *count);
int logic_oracle_snapshot_load(const char *path,
                               LogicOracleSnapshot **snapshots,
                               size_t *count);
int logic_oracle_trace_write(const char *path,
                             const LogicOracleTraceEvent *events,
                             size_t count);
int logic_oracle_trace_read(const char *path,
                            LogicOracleTraceEvent *events,
                            size_t capacity, size_t *count);
int logic_oracle_trace_validate(const char *path);
int logic_oracle_trace_find(const char *path, uint32_t sequence,
                            LogicOracleTraceEvent *event);
const char *logic_oracle_status_name(LogicOracleStatus status);

/* LORS/LORT files use a 14-byte LE header: magic[4], version:u16,
   record_length:u32, record_count:u32. Snapshot record lengths are fixed;
   trace records serialize the shared 25-byte event schema field by field. */

#endif
