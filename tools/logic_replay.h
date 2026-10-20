#ifndef LOGIC_REPLAY_H
#define LOGIC_REPLAY_H

#include <stddef.h>
#include <stdint.h>
#include "logic_trace.h"

#define LOGIC_REPLAY_MANIFEST_VERSION 1
#define LOGIC_REPLAY_TRACE_VERSION LOGIC_TRACE_VERSION
#define LOGIC_REPLAY_SHA256_SIZE 32
#define LOGIC_REPLAY_RESOURCE_NAME_MAX 64
#define LOGIC_REPLAY_ENGINE_MAX 64
#define LOGIC_REPLAY_BUILD_MAX 64
#define LOGIC_REPLAY_AGI_VERSION_MAX 32
#define LOGIC_REPLAY_RNG_ALGORITHM_MAX 32
#define LOGIC_REPLAY_MAX_RESOURCES 128
#define LOGIC_REPLAY_CHECKPOINT_MAX 4096
#define LOGIC_REPLAY_EVENT_RECORD_SIZE LOGIC_TRACE_RECORD_SIZE
#define LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE 98
#define LOGIC_REPLAY_MANIFEST_HEADER_SIZE 20
#define LOGIC_REPLAY_TRACE_HEADER_SIZE LOGIC_TRACE_HEADER_SIZE
#define LOGIC_REPLAY_MANIFEST_RECORD_LENGTH_OFFSET 12
#define LOGIC_REPLAY_TRACE_RECORD_LENGTH_OFFSET 6

/* The file header is 8-byte magic, version, header length, record length,
 * reserved word, and payload length; all integer fields are little-endian. */
typedef struct LogicReplayResource {
    char name[LOGIC_REPLAY_RESOURCE_NAME_MAX];
    uint8_t sha256[LOGIC_REPLAY_SHA256_SIZE];
} LogicReplayResource;

typedef struct LogicReplayManifest {
    uint16_t version;
    char engine[LOGIC_REPLAY_ENGINE_MAX];
    char build[LOGIC_REPLAY_BUILD_MAX];
    char agi_version[LOGIC_REPLAY_AGI_VERSION_MAX];
    uint16_t resource_count;
    LogicReplayResource resources[LOGIC_REPLAY_MAX_RESOURCES];
    uint16_t checkpoint_length;
    uint8_t initial_checkpoint[LOGIC_REPLAY_CHECKPOINT_MAX];
    uint64_t rng_seed;
    char rng_algorithm[LOGIC_REPLAY_RNG_ALGORITHM_MAX];
    /* Interpreter ticks per second as numerator / denominator. */
    uint32_t tick_numerator;
    uint32_t tick_denominator;
} LogicReplayManifest;

enum LogicReplayDeliveryReason {
    LOGIC_REPLAY_REASON_INPUT_DOWN = 1,
    LOGIC_REPLAY_REASON_INPUT_UP = 2,
    LOGIC_REPLAY_REASON_RNG = 3,
    LOGIC_REPLAY_REASON_TIME = 4,
    LOGIC_REPLAY_REASON_CONTEXT_MISMATCH = 5,
    /* One semantic key press delivered by a full-app replay input hook. */
    LOGIC_REPLAY_REASON_SEMANTIC_KEYPRESS = 6,
    LOGIC_REPLAY_REASON_SEMANTIC_MISMATCH = 7
};

/* Delivery records are MARK events; reason identifies input/RNG/time. */
typedef LogicTraceEvent LogicReplayEvent;

typedef struct LogicFailureSignature {
    uint8_t kind;
    uint8_t logic;
    uint16_t logical_pc;
    uint8_t opcode;
    uint8_t reason;
    uint32_t code;
} LogicFailureSignature;

int logic_replay_manifest_validate(const LogicReplayManifest *manifest);
int logic_replay_manifest_write(const char *path,
                                const LogicReplayManifest *manifest);
int logic_replay_manifest_read(const char *path,
                               LogicReplayManifest *manifest);
int logic_replay_manifest_matches(const LogicReplayManifest *expected,
                                  const LogicReplayManifest *actual);
int logic_replay_manifest_verify_resources(const LogicReplayManifest *manifest,
                                           const char *resource_directory);
int logic_replay_sha256_file(const char *path,
                             uint8_t digest[LOGIC_REPLAY_SHA256_SIZE]);

int logic_replay_trace_validate(const LogicReplayEvent *events, size_t count);
int logic_replay_trace_write(const char *path,
                             const LogicReplayEvent *events, size_t count);
int logic_replay_trace_read(const char *path, LogicReplayEvent **events,
                            size_t *count);
void logic_replay_events_free(LogicReplayEvent *events);

#endif
