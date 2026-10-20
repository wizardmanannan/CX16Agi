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

static int app_inputs_from_trace(const LogicReplayEvent *events,
                                 size_t event_count, uint32_t tick_count,
                                 LogicAppReplayInput input[])
{
    size_t i;
    if (event_count > LOGIC_APP_REPLAY_INPUT_MAX || tick_count > 255u)
        return -1;
    for (i = 0; i < event_count; ++i) {
        if (events[i].kind != LOGIC_TRACE_KIND_MARK || events[i].logic != 0 ||
            (events[i].reason != LOGIC_REPLAY_REASON_SEMANTIC_KEYPRESS &&
             events[i].reason != LOGIC_REPLAY_REASON_INPUT_DOWN) ||
            !events[i].tick || events[i].tick > tick_count ||
            events[i].tick > UINT16_MAX / 6u)
            return -1;
        input[i].frame = events[i].tick * 6u;
        input[i].petscii = events[i].field;
    }
    return 0;
}

static int verify_app_image_identity(const LogicReplayManifest *manifest)
{
#define IMAGE_TARGET(source, target) target,
    static const char *const images[] = {
        LOGIC_APP_REPLAY_IMAGES(IMAGE_TARGET)
    };
#undef IMAGE_TARGET
    const LogicReplayResource *main_image = NULL;
    const char *program = getenv("CX16_LOGIC_APP_PRG");
    uint8_t digest[LOGIC_REPLAY_SHA256_SIZE];
    for (size_t i = 0; i < sizeof images / sizeof images[0]; ++i) {
        const LogicReplayResource *image = NULL;
        for (size_t j = 0; j < manifest->resource_count; ++j)
            if (!strcmp(manifest->resources[j].name, images[i])) {
                image = &manifest->resources[j];
                break;
            }
        if (!image) {
            fprintf(stderr, "logic-replay: missing full-app application image identity: %s\n",
                    images[i]);
            return -1;
        }
        if (i == 0) main_image = image;
    }
    if (!program || !*program) program = LOGIC_APP_REPLAY_DEFAULT_PROGRAM;
    if (logic_replay_sha256_file(program, digest) ||
        memcmp(digest, main_image->sha256, sizeof digest)) {
        fprintf(stderr, "logic-replay: full-app executable differs from manifest: %s\n", program);
        return -1;
    }
    return 0;
}

static int read_app_replay_inputs(const char *game_dir,
                                  const char *manifest_path,
                                  const char *trace_path, uint32_t tick_count,
                                  LogicReplayManifest *manifest,
                                  LogicReplayEvent **events,
                                  size_t *event_count,
                                  LogicAppReplayInput input[],
                                  uint16_t *seed)
{
    if (logic_replay_manifest_read(manifest_path, manifest) != 0) {
        fprintf(stderr,
                "logic-replay: invalid full-app manifest, resource set, or semantic input trace\n");
        return -1;
    }
    if (manifest->checkpoint_length) {
        fprintf(stderr,
                "logic-replay: full-app initial checkpoint restore is unsupported\n");
        return -1;
    }
    if (strcmp(manifest->rng_algorithm, LOGIC_APP_REPLAY_RNG_ALGORITHM)) {
        fprintf(stderr, "logic-replay: unsupported full-app RNG algorithm\n");
        return -1;
    }
    if (verify_app_image_identity(manifest))
        return -1;
    if (logic_replay_manifest_verify_resources(manifest, game_dir) != 0 ||
        logic_replay_trace_read(trace_path, events, event_count) != 0 ||
        manifest->tick_numerator != 10 || manifest->tick_denominator != 1 ||
        manifest->rng_seed > UINT16_MAX ||
        app_inputs_from_trace(*events, *event_count, tick_count, input) != 0) {
        fprintf(stderr,
                "logic-replay: invalid full-app manifest, resource set, or semantic input trace\n");
        return -1;
    }
    *seed = (uint16_t)manifest->rng_seed;
    return 0;
}

static int write_raw_checkpoint(const char *path,
                                const uint8_t checkpoint[LOGIC_APP_REPLAY_OUTPUT_SIZE])
{
    FILE *file = fopen(path, "wb");
    int write_ok;
    if (!file)
        return -1;
    write_ok = fwrite(checkpoint, 1, LOGIC_APP_REPLAY_OUTPUT_SIZE, file) ==
               LOGIC_APP_REPLAY_OUTPUT_SIZE;
    if (fclose(file) != 0)
        write_ok = 0;
    return write_ok ? 0 : -1;
}

static int full_app_command(int argc, char **argv)
{
    const char *game_dir = NULL, *manifest_path = NULL, *trace_path = NULL;
    const char *output_path = NULL;
    LogicReplayManifest manifest;
    LogicReplayEvent *events = NULL;
    LogicAppReplayInput input[LOGIC_APP_REPLAY_INPUT_MAX];
    size_t event_count = 0;
    uint32_t tick_count = 0, timeout_ms = 120000;
    uint16_t seed = 0;
    uint8_t checkpoint[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    char detail[256];
    int result = 1;
    for (int i = 2; i < argc; ++i) {
        const char *option = argv[i];
        if (++i >= argc)
            goto usage;
        if (!strcmp(option, "--game-dir")) game_dir = argv[i];
        else if (!strcmp(option, "--manifest")) manifest_path = argv[i];
        else if (!strcmp(option, "--input-trace")) trace_path = argv[i];
        else if (!strcmp(option, "--ticks")) {
            if (parse_u32(argv[i], &tick_count) != 0)
                goto usage;
        } else if (!strcmp(option, "--timeout-ms")) {
            if (parse_u32(argv[i], &timeout_ms) != 0)
                goto usage;
        } else if (!strcmp(option, "--output")) output_path = argv[i];
        else goto usage;
    }
    if (!game_dir || !manifest_path || !trace_path || !output_path ||
        !tick_count || tick_count > 255u || !timeout_ms)
        goto usage;
    if (read_app_replay_inputs(game_dir, manifest_path, trace_path, tick_count,
            &manifest, &events, &event_count, input, &seed) != 0)
        goto done;
    if (logic_app_replay_x16_run(game_dir, seed, input, event_count,
            tick_count, timeout_ms, 0, checkpoint, detail, sizeof detail) != 0) {
        fprintf(stderr, "logic-replay: full production application failed: %s\n",
                detail);
        /* Preserve a captured failure without turning it into a passing run. */
        if (!memcmp(checkpoint, "X16R", 4) &&
            checkpoint[4] == LOGIC_APP_REPLAY_SNAPSHOT_VERSION && checkpoint[5] == 1) {
            if (write_raw_checkpoint(output_path, checkpoint))
                fprintf(stderr, "logic-replay: cannot retain failure checkpoint %s\n", output_path);
            else
                fprintf(stderr, "logic-replay: failure checkpoint retained: %s\n", output_path);
        }
        goto done;
    }
    if (write_raw_checkpoint(output_path, checkpoint) != 0) {
        fprintf(stderr, "logic-replay: cannot write full-app checkpoint %s\n",
                output_path);
        goto done;
    }
    printf("full-app replay ticks=%" PRIu32 " events=%zu checkpoint=%s\n",
           tick_count, event_count, output_path);
    result = 0;
    goto done;
usage:
    fprintf(stderr, "usage: logic-replay full-app --game-dir DIR --manifest FILE"
            " --input-trace FILE --ticks N --output FILE [--timeout-ms N]\n");
done:
    logic_replay_events_free(events);
    return result;
}

typedef struct AppFaultOracleContext {
    const char *game_dir;
    uint16_t seed;
    uint32_t tick_count;
    uint32_t context_failure_tick;
    uint8_t fault_flags;
    const char *manifest_path;
    int semantic;
    int has_semantic_failure;
    LogicOracleResult semantic_failure;
} AppFaultOracleContext;

static uint64_t app_milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static int absolute_app_path(char *out, size_t size, const char *path)
{
    char cwd[4096];
    if (path[0] == '/') return copy_text(out, size, path);
    if (!getcwd(cwd, sizeof cwd)) return -1;
    int length = snprintf(out, size, "%s/%s", cwd, path);
    return length < 0 || (size_t)length >= size ? -1 : 0;
}

static int app_reference_run(const char *directory, const char *game_dir,
    const char *manifest_path, uint32_t ticks, uint64_t deadline)
{
    const char *configured = getenv("CX16_SCUMMVM_ORACLE");
    char program[4096], game[4096], manifest[4096], tick_text[16];
    pid_t child;
    int status;
    struct timespec pause = {0, 10000000};
    if (!configured || !*configured) configured = "obj/cx16/logic-oracle-scummvm";
    if (absolute_app_path(program, sizeof program, configured) ||
        absolute_app_path(game, sizeof game, game_dir) ||
        absolute_app_path(manifest, sizeof manifest, manifest_path)) return -1;
    snprintf(tick_text, sizeof tick_text, "%u", (unsigned)ticks);
    child = fork();
    if (child < 0) return -1;
    if (!child) {
        if (chdir(directory)) _exit(126);
        execl(program, program, "--game-dir", game, "--manifest", manifest,
              "--input-trace", "reference.ltrace", "--ticks", tick_text,
              "--snapshot", "reference.lors", "--checkpoint-profile",
              "completed-cycle", (char *)NULL);
        _exit(127);
    }
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) return WIFEXITED(status) && !WEXITSTATUS(status) ? 0 : -1;
        if (waited < 0 && errno != EINTR) return -1;
        if (app_milliseconds() >= deadline) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            return -1;
        }
        nanosleep(&pause, NULL);
    }
}

static int app_version(const char *text, uint16_t *version)
{
    char digits[8], *end;
    size_t length = 0;
    unsigned long value;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    for (; *text; ++text) {
        if (*text == '.') continue;
        if (length + 1 >= sizeof digits) return -1;
        digits[length++] = *text;
    }
    digits[length] = 0;
    value = strtoul(digits, &end, 16);
    if (!length || *end || !value || value > UINT16_MAX) return -1;
    *version = (uint16_t)value;
    return 0;
}

static int same_semantic_failure(const LogicOracleResult *a, const LogicOracleResult *b)
{
    return a->checkpoint_index == b->checkpoint_index &&
        !strcmp(a->field, b->field) &&
        a->reference_value == b->reference_value && a->candidate_value == b->candidate_value &&
        !strcmp(a->reference_text, b->reference_text) &&
        !strcmp(a->candidate_text, b->candidate_text);
}

static int full_app_semantic_oracle(void *opaque,
    const LogicReplayManifest *manifest, const LogicReplayEvent *events,
    size_t count, uint32_t timeout_ms, LogicFailureSignature *signature)
{
    AppFaultOracleContext *context = opaque;
    LogicAppReplayInput input[LOGIC_APP_REPLAY_INPUT_MAX];
    LogicReplayEvent expanded[LOGIC_APP_REPLAY_INPUT_MAX * 2];
    uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0}, manifest_hash[32], input_hash[32], reference_hash[32];
    LogicOracleSnapshot candidate, *reference = NULL;
    uint8_t (*series)[LOGIC_APP_REPLAY_OUTPUT_SIZE] = NULL;
    LogicOracleResult comparison;
    uint16_t version;
    size_t reference_count = 0;
    uint64_t deadline = app_milliseconds() + timeout_ms;
    char directory[] = "obj/cx16/app-semantic-reduce-XXXXXX";
    char canonical_path[512], reference_path[512], snapshot_path[512], detail[256];
    int result = -1, fixture = 0;
    uint32_t ticks = context && context->has_semantic_failure
        ? context->semantic_failure.checkpoint_index + 1 : context ? context->tick_count : 0;
    if (!context || !manifest || !signature || !timeout_ms ||
        app_inputs_from_trace(events, count, context->tick_count, input) ||
        app_version(manifest->agi_version, &version)) return -1;
    if (!mkdtemp(directory)) return -1;
    fixture = 1;
    snprintf(canonical_path, sizeof canonical_path, "%s/input.ltrace", directory);
    snprintf(reference_path, sizeof reference_path, "%s/reference.ltrace", directory);
    snprintf(snapshot_path, sizeof snapshot_path, "%s/reference.lors", directory);
    for (size_t i = 0; i < count; ++i) {
        expanded[i * 2] = events[i];
        expanded[i * 2].sequence = (uint32_t)(i * 2);
        expanded[i * 2].reason = LOGIC_REPLAY_REASON_INPUT_DOWN;
        expanded[i * 2].before = 0; expanded[i * 2].after = 1;
        expanded[i * 2 + 1] = expanded[i * 2];
        expanded[i * 2 + 1].sequence++;
        expanded[i * 2 + 1].reason = LOGIC_REPLAY_REASON_INPUT_UP;
        expanded[i * 2 + 1].before = 1; expanded[i * 2 + 1].after = 0;
    }
    if (logic_replay_trace_write(canonical_path, events, count) ||
        logic_replay_trace_write(reference_path, expanded, count * 2) ||
        logic_replay_sha256_file(context->manifest_path, manifest_hash) ||
        logic_replay_sha256_file(canonical_path, input_hash) ||
        logic_replay_sha256_file(reference_path, reference_hash) ||
        app_milliseconds() >= deadline ||
        app_reference_run(directory, context->game_dir, context->manifest_path,
            ticks, deadline) ||
        logic_oracle_snapshot_load(snapshot_path, &reference, &reference_count) ||
        reference_count != ticks) goto done;
    for (size_t i = 0; i < reference_count; ++i) {
        if (memcmp(reference[i].input_sha256, reference_hash, 32)) goto done;
        /* Down/up expansion was constructed from this exact candidate above. */
        memcpy(reference[i].input_sha256, input_hash, 32);
    }
    /* Capture every boundary: a final match can hide earlier corruption. */
    series = calloc(ticks, sizeof *series);
    if (!series) goto done;
    {
        uint64_t now = app_milliseconds();
        if (now >= deadline) goto done;
        int captured = logic_app_replay_x16_series(context->game_dir,
            context->seed, input, count, ticks, (uint32_t)(deadline - now),
            context->fault_flags, series, ticks, detail, sizeof detail);
        const uint8_t *last = series[ticks - 1];
        if (captured && !((context->fault_flags & LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE) &&
                !(last[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED) &&
                last[5] == 1 && get_u16(last + 6) == ticks)) {
            if (last[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED) result = 1;
            else fprintf(stderr, "semantic reduction series failed: %s\n", detail);
            goto done;
        }
    }
    for (uint32_t tick = 1; tick <= ticks; ++tick) {
        uint64_t now = app_milliseconds();
        LogicOracleStatus status;
        if (now >= deadline) goto done;
        memcpy(raw, series[tick - 1], sizeof raw);
        if (context->fault_flags & LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE)
            raw[58] &= (uint8_t)~LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED;
        if (logic_app_replay_snapshot_to_oracle(manifest->engine, version, manifest,
            manifest_hash, input_hash, raw, sizeof raw, &candidate, detail, sizeof detail))
            goto done;
        status = logic_oracle_compare_with_policy(&reference[tick - 1], &candidate,
            LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC, &comparison);
        if (status == LOGIC_ORACLE_DIVERGED) break;
        if (status != LOGIC_ORACLE_MATCH) {
            fprintf(stderr, "semantic reduction comparison invalid: %s\n", comparison.detail);
            goto done;
        }
        if (tick == ticks) { result = 1; goto done; }
    }
    if (!context->has_semantic_failure) {
        context->semantic_failure = comparison;
        context->has_semantic_failure = 1;
    } else if (!same_semantic_failure(&context->semantic_failure, &comparison)) {
        result = 1;
        goto done;
    }
    memset(signature, 0, sizeof *signature);
    signature->kind = LOGIC_TRACE_KIND_MARK;
    signature->reason = LOGIC_REPLAY_REASON_SEMANTIC_MISMATCH;
    signature->code = 1;
    result = 0;
done:
    free(series);
    free(reference);
    if (fixture) {
        static const char *const names[] = {"input.ltrace", "reference.ltrace", "reference.lors", "scummvm.ini"};
        char path[512];
        for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
            snprintf(path, sizeof path, "%s/%s", directory, names[i]);
            if (unlink(path) && errno != ENOENT) result = -1;
        }
        if (rmdir(directory)) result = -1;
    }
    return result;
}

static int full_app_fault_oracle(void *opaque,
                                 const LogicReplayManifest *manifest,
                                 const LogicReplayEvent *events, size_t count,
                                 uint32_t timeout_ms,
                                 LogicFailureSignature *signature)
{
    AppFaultOracleContext *context = (AppFaultOracleContext *)opaque;
    LogicAppReplayInput input[LOGIC_APP_REPLAY_INPUT_MAX];
    uint8_t checkpoint[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    char detail[256] = {0};
    int result;
    uint32_t run_ticks = context && context->context_failure_tick
        ? context->context_failure_tick : context ? context->tick_count : 0;
    if (!context || !manifest || !signature || count > LOGIC_APP_REPLAY_INPUT_MAX ||
        manifest->rng_seed != context->seed ||
        app_inputs_from_trace(events, count, context->tick_count, input) != 0)
        return -1;
    result = logic_app_replay_x16_run(context->game_dir, context->seed,
        input, count, run_ticks, timeout_ms, context->fault_flags,
        checkpoint, detail, sizeof detail);
    if (checkpoint[0] != 'X' || checkpoint[1] != '1' ||
        checkpoint[2] != '6' || checkpoint[3] != 'R' ||
        checkpoint[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION)
        return -1;
    /* Exhausting the input schedule is not a context failure. In particular,
       deleting a prompt response must never preserve an unrelated signature. */
    if (checkpoint[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED)
        return 1;
    if (checkpoint[58] & LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED) {
        /* Injection is optional: the verifier's field/PC signature identifies
           the failure regardless of whether a test fault caused it. */
        if (context->fault_flags &&
            !(checkpoint[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED))
            return -1;
        memset(signature, 0, sizeof *signature);
        signature->kind = LOGIC_TRACE_KIND_MARK;
        signature->logic = checkpoint[25];
        signature->logical_pc = get_u16(checkpoint + 27);
        signature->opcode = checkpoint[26];
        signature->reason = LOGIC_REPLAY_REASON_CONTEXT_MISMATCH;
        signature->code = ((uint32_t)checkpoint[23] << 8) | checkpoint[24];
        if (!context->context_failure_tick)
            context->context_failure_tick = get_u16(checkpoint + 6);
        return 0;
    }
    /* Deleting the trigger is a rejected reduction candidate, rather than
       an oracle error. Require a complete, otherwise valid clean run. */
    if (context->fault_flags &&
        !(checkpoint[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED) &&
        (uint32_t)get_u16(checkpoint + 6) == run_ticks &&
        get_u16(checkpoint + 8) == 0 &&
        checkpoint[38] == (uint8_t)run_ticks &&
        !(checkpoint[58] & (uint8_t)~(LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY)) &&
        !(get_u16(checkpoint + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET) &
            (uint16_t)~LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED))
        return 1;
    if (result != 0)
        fprintf(stderr, "context reduction target rejected: %s (status=%02x field=%u offset=%u)\n",
                detail, checkpoint[58], checkpoint[23], checkpoint[24]);
    return result == 0 ? 1 : -1;
}

static int app_reduce_command(int argc, char **argv)
{
    const char *game_dir = NULL, *manifest_path = NULL, *trace_path = NULL;
    const char *output_path = NULL;
    LogicReplayManifest manifest;
    LogicReplayEvent *events = NULL, *reduced = NULL;
    LogicAppReplayInput input[LOGIC_APP_REPLAY_INPUT_MAX];
    size_t event_count = 0, reduced_count = 0;
    LogicReductionOptions options = {20, 120000, 2};
    LogicReductionReport report;
    LogicFailureSignature failure;
    AppFaultOracleContext context = {0};
    uint32_t tick_count = 0, max_attempts = 20;
    uint16_t seed = 0;
    int result = 1;
    int reduction_status;
    int fault_seen = 0;
    for (int i = 2; i < argc; ++i) {
        const char *option = argv[i];
        if (!strcmp(option, "--failure")) {
            if (++i >= argc) goto usage;
            if (!strcmp(argv[i], "semantic")) context.semantic = 1;
            else if (!strcmp(argv[i], "context")) context.semantic = 0;
            else goto usage;
            continue;
        }
        if (!strcmp(option, "--fault")) {
            if (++i >= argc || fault_seen) goto usage;
            if (!strcmp(argv[i], "omit-metadata-save"))
                fault_seen = LOGIC_APP_REPLAY_FAULT_OMIT_METADATA_SAVE;
            else if (!strcmp(argv[i], "semantic-write"))
                fault_seen = LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE;
            else goto usage;
            continue;
        }
        if (++i >= argc)
            goto usage;
        if (!strcmp(option, "--game-dir")) game_dir = argv[i];
        else if (!strcmp(option, "--manifest")) manifest_path = argv[i];
        else if (!strcmp(option, "--input-trace")) trace_path = argv[i];
        else if (!strcmp(option, "--ticks")) {
            if (parse_u32(argv[i], &tick_count) != 0)
                goto usage;
        } else if (!strcmp(option, "--output-trace")) output_path = argv[i];
        else if (!strcmp(option, "--max-attempts")) {
            if (parse_u32(argv[i], &max_attempts) != 0)
                goto usage;
        } else if (!strcmp(option, "--timeout-ms")) {
            if (parse_u32(argv[i], &options.timeout_ms) != 0)
                goto usage;
        } else goto usage;
    }
    if (!game_dir || !manifest_path || !trace_path ||
        !output_path || !tick_count || tick_count > 255u ||
        !max_attempts || !options.timeout_ms ||
        (context.semantic && fault_seen == LOGIC_APP_REPLAY_FAULT_OMIT_METADATA_SAVE) ||
        (!context.semantic && fault_seen == LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE))
        goto usage;
    options.max_attempts = max_attempts;
    if (read_app_replay_inputs(game_dir, manifest_path, trace_path, tick_count,
            &manifest, &events, &event_count, input, &seed) != 0)
        goto done;
    context.manifest_path = manifest_path;
    context.game_dir = game_dir;
    context.seed = seed;
    context.tick_count = tick_count;
    context.fault_flags = (uint8_t)fault_seen;
    reduction_status = logic_reduce_trace(&manifest, events, event_count,
            context.semantic ? full_app_semantic_oracle : full_app_fault_oracle,
            &context, &options, &reduced, &reduced_count, &failure,
            &report);
    if (reduction_status != 0 || report.limit_reached ||
        !report.repeated_run_agreement || reduced_count >= event_count) {
        fprintf(stderr,
                "logic-replay: full-app failure was not reduced with repeated signature agreement: status=%d limit=%d agreement=%d attempts=%zu runs=%zu elapsed-ms=%u events=%zu->%zu\n",
                reduction_status, report.limit_reached,
                report.repeated_run_agreement, report.attempts,
                report.oracle_runs, report.elapsed_ms, event_count, reduced_count);
        goto done;
    }
    if (logic_replay_trace_write(output_path, reduced, reduced_count) != 0) {
        fprintf(stderr, "logic-replay: cannot write reduced full-app trace %s\n",
                output_path);
        goto done;
    }
    printf("full-app reduction events=%zu->%zu signature=%u:%u:%04x:%u:%u:%08" PRIx32
           " attempts=%zu emulator-runs=%zu elapsed-ms=%" PRIu32 " trace=%s\n",
           event_count, reduced_count, failure.kind, failure.logic,
           failure.logical_pc, failure.opcode, failure.reason, failure.code,
           report.attempts, report.oracle_runs, report.elapsed_ms, output_path);
    if (context.semantic)
        printf("semantic failure checkpoint=%u field=%s reference=%lld candidate=%lld reference-text=%s candidate-text=%s\n",
            context.semantic_failure.checkpoint_index, context.semantic_failure.field,
            (long long)context.semantic_failure.reference_value,
            (long long)context.semantic_failure.candidate_value,
            context.semantic_failure.reference_text, context.semantic_failure.candidate_text);
    result = 0;
    goto done;
usage:
    fprintf(stderr, "usage: logic-replay app-reduce --game-dir DIR --manifest FILE"
            " --input-trace FILE --ticks N --output-trace FILE"
            " [--failure context|semantic] [--fault omit-metadata-save|semantic-write] [--max-attempts N] [--timeout-ms N]\n");
done:
    logic_replay_events_free(events);
    free(reduced);
    return result;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: logic-replay {validate|snapshot|full-app|app-reduce} ...\n");
        return 2;
    }
    if (!strcmp(argv[1], "validate"))
        return validate_command(argc, argv);
    if (!strcmp(argv[1], "snapshot"))
        return snapshot_command(argc, argv);
    if (!strcmp(argv[1], "full-app"))
        return full_app_command(argc, argv);
    if (!strcmp(argv[1], "app-reduce"))
        return app_reduce_command(argc, argv);
    fprintf(stderr, "logic-replay: unknown command '%s'\n", argv[1]);
    return 2;
}
