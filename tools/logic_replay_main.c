#define _POSIX_C_SOURCE 200809L

#include "logic_replay.h"
#include "logic_replay_runner.h"
#include "logic_oracle.h"
#include "logic_reduce.h"
#include "logic_app_replay.h"
#include "../tests/x16_replay_runner.h"

#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;
    if (!text || !*text || text[0] == '-')
        return -1;
    parsed = strtoul(text, &end, 10);
    if (*end || parsed > UINT32_MAX)
        return -1;
    *value = (uint32_t)parsed;
    return 0;
}

static int copy_text(char *destination, size_t size, const char *source)
{
    size_t length = strlen(source);
    if (length >= size)
        return -1;
    memcpy(destination, source, length + 1);
    return 0;
}

static uint16_t get_u16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void make_snapshot(const LogicReplayManifest *manifest,
                          const LogicReplayResult *run, uint32_t tick,
                          const uint8_t manifest_hash[32],
                          const uint8_t input_hash[32],
                          LogicOracleSnapshot *snapshot)
{
    size_t i;
    memset(snapshot, 0, sizeof *snapshot);
    snapshot->checkpoint_index = tick - 1;
    snapshot->tick = tick;
    snapshot->agi_version = (uint16_t)strtoul(manifest->agi_version, NULL, 0);
    snapshot->timing_profile = LOGIC_ORACLE_TIMING_AGI_TICK;
    snapshot->game_id[0] = '\0';
    (void)copy_text(snapshot->game_id, sizeof snapshot->game_id, manifest->engine);
    (void)snprintf(snapshot->checkpoint, sizeof snapshot->checkpoint,
                   "tick-%" PRIu32, tick);
    (void)copy_text(snapshot->provenance, sizeof snapshot->provenance,
                    "CX16 x16emu production interpreter");
    memcpy(snapshot->manifest_sha256, manifest_hash, sizeof snapshot->manifest_sha256);
    memcpy(snapshot->input_sha256, input_hash, sizeof snapshot->input_sha256);
    snapshot->rng_seed = manifest->rng_seed;
    snapshot->rng_call_count = run->rng_call_count;
    snapshot->rng_final_state = run->rng_final_state;
    (void)copy_text(snapshot->rng_algorithm, sizeof snapshot->rng_algorithm,
                    manifest->rng_algorithm);
    snapshot->tick_numerator = manifest->tick_numerator;
    snapshot->tick_denominator = manifest->tick_denominator;
    memcpy(snapshot->variables, run->variables, sizeof snapshot->variables);
    memcpy(snapshot->flags, run->flags, sizeof snapshot->flags);
    snapshot->room = run->room;
    snapshot->object_count = run->object_count;
    if (run->outcome == LOGIC_REPLAY_OUTCOME_MISMATCH) {
        snapshot->has_failure = 1;
        snapshot->failure_kind = run->signature.kind;
        snapshot->failure_logic = run->signature.logic;
        snapshot->failure_logical_pc = run->signature.logical_pc;
        snapshot->failure_opcode = run->signature.opcode;
        snapshot->failure_reason = run->signature.reason;
        snapshot->failure_code = run->signature.code;
    }
    for (i = 0; i < run->object_count; ++i) {
        snapshot->objects[i].id = run->objects[i].id;
        snapshot->objects[i].x = run->objects[i].x;
        snapshot->objects[i].y = run->objects[i].y;
        snapshot->objects[i].view = run->objects[i].view;
        snapshot->objects[i].loop = run->objects[i].loop;
        snapshot->objects[i].cel = run->objects[i].cel;
        snapshot->objects[i].motion = run->objects[i].motion;
    }
}

static int snapshot_command(int argc, char **argv)
{
    const char *game_dir = NULL, *manifest_path = NULL;
    const char *trace_path = NULL, *snapshot_path = NULL;
    LogicReplayManifest manifest;
    LogicReplayEvent *events = NULL;
    LogicOracleSnapshot *snapshots = NULL;
    size_t event_count = 0, event_index = 0;
    uint8_t manifest_hash[32], input_hash[32];
    uint32_t tick_count = 0;
    unsigned fault_flags = LOGIC_REPLAY_FAULT_NONE;
    uint32_t tick;
    int result = 1;

    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--test-fault")) {
            if (++i >= argc || strcmp(argv[i], "omit-metadata-save") != 0 ||
                fault_flags != LOGIC_REPLAY_FAULT_NONE)
                goto usage;
            fault_flags = LOGIC_REPLAY_FAULT_OMIT_METADATA_SAVE;
            continue;
        }
        if (++i >= argc)
            goto usage;
        if (!strcmp(argv[i - 1], "--game-dir"))
            game_dir = argv[i];
        else if (!strcmp(argv[i - 1], "--manifest"))
            manifest_path = argv[i];
        else if (!strcmp(argv[i - 1], "--input-trace"))
            trace_path = argv[i];
        else if (!strcmp(argv[i - 1], "--snapshot"))
            snapshot_path = argv[i];
        else if (!strcmp(argv[i - 1], "--ticks")) {
            if (parse_u32(argv[i], &tick_count) != 0)
                goto usage;
        } else {
            goto usage;
        }
    }
    if (!game_dir || !manifest_path || !trace_path || !snapshot_path ||
        !tick_count || tick_count > LOGIC_ORACLE_MAX_REFERENCE_TICKS)
        goto usage;
    if (logic_replay_manifest_read(manifest_path, &manifest) != 0 ||
        logic_replay_manifest_verify_resources(&manifest, game_dir) != 0 ||
        logic_replay_sha256_file(manifest_path, manifest_hash) != 0 ||
        logic_replay_sha256_file(trace_path, input_hash) != 0 ||
        logic_replay_trace_read(trace_path, &events, &event_count) != 0) {
        fprintf(stderr, "logic-replay: invalid manifest, resources, or input trace\n");
        goto done;
    }
    {
        char *end;
        unsigned long version = strtoul(manifest.agi_version, &end, 0);
        if (!manifest.agi_version[0] || *end || !version || version > UINT16_MAX) {
            fprintf(stderr, "logic-replay: manifest AGI version is not numeric\n");
            goto done;
        }
    }
    snapshots = (LogicOracleSnapshot *)calloc(tick_count, sizeof *snapshots);
    if (!snapshots)
        goto done;

    /* A clean boot for each prefix gives canonical tick checkpoints without
       relying on host-specific save-state restoration. */
    for (tick = 1; tick <= tick_count; ++tick) {
        LogicReplayResult run;
        memset(&run, 0, sizeof run);
        while (event_index < event_count && events[event_index].tick <= tick)
            ++event_index;
        if (logic_replay_run_ticks(&manifest, events, event_index, game_dir,
                                   tick, 60000, fault_flags, &run) != 0 ||
            run.outcome == LOGIC_REPLAY_OUTCOME_CRASH ||
            run.outcome == LOGIC_REPLAY_OUTCOME_TIMEOUT ||
            run.outcome == LOGIC_REPLAY_OUTCOME_RUNNER_ERROR) {
            fprintf(stderr, "logic-replay: candidate run failed at tick %" PRIu32
                    ": %s\n", tick, run.detail);
            goto done;
        }
        make_snapshot(&manifest, &run, tick, manifest_hash, input_hash,
                      &snapshots[tick - 1]);
        if (run.outcome == LOGIC_REPLAY_OUTCOME_MISMATCH)
            fprintf(stderr, "logic-replay: candidate mismatch at tick %" PRIu32
                    " (%s) signature=%u:%u:%04x:%u:%u:%08" PRIx32 "\n",
                    tick, run.detail, run.signature.kind, run.signature.logic,
                    run.signature.logical_pc, run.signature.opcode,
                    run.signature.reason, run.signature.code);
    }
    if (logic_oracle_snapshot_write(snapshot_path, snapshots, tick_count) != 0) {
        fprintf(stderr, "logic-replay: could not write candidate snapshots\n");
        goto done;
    }
    printf("candidate checkpoints=%" PRIu32 " ticks=%" PRIu32
           " events=%zu snapshot=%s\n", tick_count, tick_count,
           event_count, snapshot_path);
    result = 0;
    goto done;

usage:
    fprintf(stderr, "usage: logic-replay snapshot --game-dir DIR --manifest FILE"
            " --input-trace FILE --ticks N --snapshot FILE"
            " [--test-fault omit-metadata-save]\n");
done:
    free(snapshots);
    logic_replay_events_free(events);
    return result;
}

static int validate_command(int argc, char **argv)
{
    LogicReplayManifest manifest;
    LogicReplayEvent *events = NULL;
    size_t event_count = 0;
    int result = 1;
    if (argc != 5) {
        fprintf(stderr, "usage: logic-replay validate MANIFEST TRACE RESOURCE_DIR\n");
        return 2;
    }
    if (logic_replay_manifest_read(argv[2], &manifest) != 0) {
        fprintf(stderr, "logic-replay: invalid or unsupported manifest\n");
        goto done;
    }
    if (logic_replay_manifest_verify_resources(&manifest, argv[4]) != 0) {
        fprintf(stderr, "logic-replay: resource hash mismatch or unreadable resource\n");
        goto done;
    }
    if (logic_replay_trace_read(argv[3], &events, &event_count) != 0) {
        fprintf(stderr, "logic-replay: invalid or unsupported trace\n");
        goto done;
    }
    printf("manifest_version=%u events=%zu tick=%" PRIu32 "/%" PRIu32
           " resources=%u replay=not-run\n",
           manifest.version, event_count, manifest.tick_numerator,
           manifest.tick_denominator, manifest.resource_count);
    result = 0;
done:
    logic_replay_events_free(events);
    return result;
}

