#include "logic_replay_runner.h"
#include "../tests/x16_replay_runner.h"

#include <string.h>
#include <stdio.h>
#include "logic_app_replay.h"

int logic_replay_run(const LogicReplayManifest *manifest,
                     const LogicReplayEvent *events, size_t event_count,
                     const char *resource_dir, uint32_t timeout_ms,
                     unsigned fault_flags, LogicReplayResult *result)
{
    if (!manifest || !result || (!events && event_count))
        return -1;
    return logic_replay_x16_run(manifest, events, event_count, resource_dir,
                                timeout_ms, fault_flags, result);
}

int logic_replay_run_ticks(const LogicReplayManifest *manifest,
                           const LogicReplayEvent *events, size_t event_count,
                           const char *resource_dir, uint32_t tick_count,
                           uint32_t timeout_ms, unsigned fault_flags,
                           LogicReplayResult *result)
{
    if (!manifest || !result || (!events && event_count))
        return -1;
    return logic_replay_x16_run_ticks(manifest, events, event_count,
                                      resource_dir, tick_count, timeout_ms,
                                      fault_flags, result);
}

const char *logic_replay_outcome_name(LogicReplayOutcome outcome)
{
    static const char *const names[] = {
        "PASS", "MISMATCH", "CRASH", "TIMEOUT", "RUNNER_ERROR"
    };
    if ((unsigned)outcome >= sizeof names / sizeof names[0])
        return "UNKNOWN";
    return names[outcome];
}

static uint16_t app_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t app_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int logic_app_replay_snapshot_to_oracle(
    const char *game_id, uint16_t agi_version,
    const LogicReplayManifest *manifest,
    const uint8_t manifest_hash[LOGIC_ORACLE_SHA256_SIZE],
    const uint8_t input_hash[LOGIC_ORACLE_SHA256_SIZE],
    const uint8_t *raw, size_t raw_size, LogicOracleSnapshot *snapshot,
    char *detail, size_t detail_size)
{
    unsigned i;
    unsigned inventory_count;
    int bounded_trace;
    unsigned trace_capacity = LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    unsigned trace_status = LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
                            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    uint8_t seen[256] = {0};
#define APP_INVALID(message) do { \
    if (snapshot) memset(snapshot, 0, sizeof *snapshot); \
    if (detail && detail_size) snprintf(detail, detail_size, "%s", message); \
    return -1; \
} while (0)
    if (snapshot) memset(snapshot, 0, sizeof *snapshot);
    if (detail && detail_size) detail[0] = '\0';
    if (!game_id || !agi_version || !manifest || !manifest_hash ||
        !input_hash || !raw || !snapshot || raw_size != LOGIC_APP_REPLAY_OUTPUT_SIZE)
        APP_INVALID("invalid full-app conversion request or capture size");
    if (!memchr(manifest->engine, 0, sizeof manifest->engine) ||
        !game_id[0] || strlen(game_id) >= sizeof snapshot->game_id ||
        strcmp(game_id, manifest->engine) != 0 ||
        !memchr(manifest->rng_algorithm, 0, sizeof manifest->rng_algorithm) ||
        !manifest->rng_algorithm[0] ||
        !manifest->tick_numerator || !manifest->tick_denominator ||
        manifest->rng_seed > 65535u)
        APP_INVALID("invalid full-app game, RNG, or timing identity");
    bounded_trace = app_u32(raw + 42) != 0;
    if (bounded_trace) {
        if (raw[41] != LOGIC_APP_REPLAY_TRACE_MAX ||
            raw[58] != trace_status ||
            app_u16(raw + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET) != trace_capacity)
            APP_INVALID("inconsistent bounded trace-history overwrite");
    } else if ((raw[58] & trace_status) ||
               (app_u16(raw + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET) & trace_capacity)) {
        APP_INVALID("trace-history overwrite flags without dropped events");
    }
    if (memcmp(raw, "X16R", 4) || raw[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION ||
        raw[5] != 1 || (raw[58] & ~trace_status) || raw[14] || raw[21] || raw[22] ||
        (app_u16(raw + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET) & ~trace_capacity) ||
        !app_u32(raw + 6) || app_u32(raw + 6) > 255u ||
        app_u16(raw + 10) > LOGIC_APP_REPLAY_INPUT_MAX ||
        app_u16(raw + 12) > app_u16(raw + 10) ||
        raw[46] > LOGIC_APP_REPLAY_MESSAGE_MAX ||
        raw[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET] > LOGIC_APP_REPLAY_OBJECT_MAX ||
        raw[41] > LOGIC_APP_REPLAY_TRACE_MAX ||
        raw[LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET] > LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX ||
        app_u16(raw + LOGIC_APP_REPLAY_RNG_SEED_OFFSET) != manifest->rng_seed)
        APP_INVALID("invalid, failed, or incomplete full-app capture");
    inventory_count = app_u16(raw + 51);
    if (inventory_count > LOGIC_APP_REPLAY_INVENTORY_MAX || inventory_count > 256u)
        APP_INVALID("inventory count exceeds captured capacity");
    snapshot->checkpoint_index = app_u32(raw + 6) - 1;
    snapshot->tick = app_u32(raw + 6);
    snapshot->agi_version = agi_version;
    snapshot->timing_profile = LOGIC_ORACLE_TIMING_AGI_TICK;
    strcpy(snapshot->game_id, game_id);
    strcpy(snapshot->provenance, "CX16 production application, x16emu");
    snprintf(snapshot->checkpoint, sizeof snapshot->checkpoint,
             "agi-tick-%u", (unsigned)snapshot->tick);
    memcpy(snapshot->manifest_sha256, manifest_hash, LOGIC_ORACLE_SHA256_SIZE);
    memcpy(snapshot->input_sha256, input_hash, LOGIC_ORACLE_SHA256_SIZE);
    snapshot->rng_seed = manifest->rng_seed;
    snapshot->rng_call_count = app_u32(raw + 60);
    snapshot->rng_final_state = app_u32(raw + LOGIC_APP_REPLAY_RNG_STATE_OFFSET);
    strcpy(snapshot->rng_algorithm, manifest->rng_algorithm);
    snapshot->tick_numerator = manifest->tick_numerator;
    snapshot->tick_denominator = manifest->tick_denominator;
    memcpy(snapshot->variables, raw + LOGIC_APP_REPLAY_VARS_OFFSET, 256);
    memcpy(snapshot->flags, raw + LOGIC_APP_REPLAY_FLAGS_OFFSET, 256);
    for (i = 0; i < 256; ++i)
        if (snapshot->flags[i] > 1) APP_INVALID("nonboolean captured flag");
    snapshot->room = snapshot->variables[0];
    if (raw[15] != snapshot->room) APP_INVALID("captured room disagrees with variable zero");
    snapshot->logic = raw[16];
    snapshot->logical_pc = app_u16(raw + 17);
    snapshot->control_flow_reliable = raw[19] == 0 && raw[20] == 0;
    for (i = 0; i < raw[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET]; ++i) {
        const uint8_t *record = raw + LOGIC_APP_REPLAY_OBJECTS_OFFSET +
            i * LOGIC_APP_REPLAY_OBJECT_RECORD_SIZE;
        LogicOracleObject *object;
        if (record[0] >= LOGIC_APP_REPLAY_OBJECT_MAX || seen[record[0]])
            APP_INVALID("invalid or duplicate captured screen-object index");
        seen[record[0]] = 1;
        if (!(app_u16(record + 1) & 0x0041u)) continue;
        object = &snapshot->objects[snapshot->object_count++];
        object->id = record[0];
        object->x = (int16_t)app_u16(record + 3);
        object->y = (int16_t)app_u16(record + 5);
        object->view = record[7];
        object->loop = record[8];
        object->cel = record[9];
        object->motion = record[11];
    }
    memset(seen, 0, sizeof seen);
    snapshot->inventory_known = 1;
    snapshot->inventory_count = (uint16_t)inventory_count;
    for (i = 0; i < inventory_count; ++i) {
        const uint8_t *record = raw + LOGIC_APP_REPLAY_INVENTORY_OFFSET +
            i * LOGIC_APP_REPLAY_INVENTORY_RECORD_SIZE;
        if (record[0] >= inventory_count || seen[record[0]])
            APP_INVALID("invalid or duplicate captured inventory index");
        seen[record[0]] = 1;
        snapshot->inventory_rooms[record[0]] = record[1];
    }
    for (i = 0; i < raw[46]; ++i) {
        const uint8_t *record = raw + LOGIC_APP_REPLAY_MESSAGES_OFFSET +
            i * LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE;
        LogicOracleMessage *message;
        if (memchr(record + 3, 0, record[2]))
            APP_INVALID("embedded zero in captured message text");
        message = &snapshot->messages[snapshot->message_count++];
        message->logic = record[0];
        message->id = record[1];
        memcpy(message->text, record + 3, record[2]);
        message->text[record[2]] = '\0';
    }
    if (bounded_trace && detail && detail_size)
        snprintf(detail, detail_size, "bounded recent trace: %u older events overwritten",
                 (unsigned)app_u32(raw + 42));
    return 0;
#undef APP_INVALID
}
