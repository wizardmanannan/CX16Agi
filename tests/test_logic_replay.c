#define _POSIX_C_SOURCE 200809L

#include "logic_replay.h"
#include "logic_replay_runner.h"
#include "logic_reduce.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "logic replay regression:%d: %s\n", __LINE__, #condition); \
        failed = 1; \
        goto cleanup; \
    } \
} while (0)

static int same_event(const LogicReplayEvent *a, const LogicReplayEvent *b)
{
    return a->sequence == b->sequence && a->tick == b->tick &&
           a->kind == b->kind && a->logic == b->logic &&
           a->logical_pc == b->logical_pc && a->opcode == b->opcode &&
           a->depth == b->depth && a->bank == b->bank &&
           a->field == b->field && a->before == b->before &&
           a->after == b->after && a->reason == b->reason;
}

static int same_signature(const LogicFailureSignature *a,
                          const LogicFailureSignature *b)
{
    return a->kind == b->kind && a->logic == b->logic &&
           a->logical_pc == b->logical_pc && a->opcode == b->opcode &&
           a->reason == b->reason && a->code == b->code;
}

typedef struct MarkerOracleRuns {
    unsigned runs;
    unsigned invalid_candidates;
} MarkerOracleRuns;

typedef struct InvalidResultOracleRuns {
    unsigned runs;
} InvalidResultOracleRuns;

/* Reducer unit oracle only; this does not emulate CX16 execution. */
static int marker_oracle(void *context, const LogicReplayManifest *manifest,
                         const LogicReplayEvent *events, size_t count,
                         uint32_t timeout_ms,
                         LogicFailureSignature *signature)
{
    size_t i;
    MarkerOracleRuns *runs = (MarkerOracleRuns *)context;
    (void)manifest;
    if (!timeout_ms || !signature)
        return -1;
    ++runs->runs;
    if (logic_replay_trace_validate(events, count) != 0)
        ++runs->invalid_candidates;
    for (i = 0; i < count; ++i) {
        if (events[i].kind == LOGIC_TRACE_KIND_MARK &&
            events[i].reason == LOGIC_REPLAY_REASON_RNG &&
            events[i].opcode == 0xAA) {
            signature->kind = LOGIC_TRACE_KIND_MARK;
            signature->logic = 7;
            signature->logical_pc = 0x1234;
            signature->opcode = 0xAA;
            signature->reason = LOGIC_REPLAY_REASON_RNG;
            signature->code = 19;
            return 0;
        }
    }
    return 1;
}

static int invalid_candidate_result_oracle(
    void *context, const LogicReplayManifest *manifest,
    const LogicReplayEvent *events, size_t count, uint32_t timeout_ms,
    LogicFailureSignature *signature)
{
    InvalidResultOracleRuns *runs = (InvalidResultOracleRuns *)context;
    (void)manifest;
    (void)events;
    (void)count;
    if (!timeout_ms || !signature)
        return -1;
    if (++runs->runs > 2)
        return 2; /* The documented oracle results are 0, 1, and -1. */
    signature->kind = LOGIC_TRACE_KIND_MARK;
    signature->logic = 7;
    signature->logical_pc = 0x1234;
    signature->opcode = 0xAA;
    signature->reason = LOGIC_REPLAY_REASON_RNG;
    signature->code = 19;
    return 0;
}

typedef struct LiveReplayOracle {
    const char *resource_directory;
    unsigned fault_flags;
} LiveReplayOracle;

/* These bytes are written as replay resources and loaded unchanged by X16. */
static void make_replay_logic_code(uint8_t *code, size_t size, unsigned entry,
                                   unsigned increments, unsigned variable,
                                   unsigned child, unsigned child_calls,
                                   int call_v, unsigned after_variable,
                                   int set_flag)
{
    unsigned i, at = entry;
    memset(code, 0xEA, size);
    for (i = 0; i < increments; ++i) {
        code[at++] = 1;
        code[at++] = (uint8_t)variable;
    }
    for (i = 0; child != 0xFF && i < child_calls; ++i) {
        code[at++] = call_v && i == 0 ? 23 : 22;
        code[at++] = call_v && i == 0 ? 100 : (uint8_t)child;
    }
    if (after_variable != 0xFF) {
        code[at++] = 1;
        code[at++] = (uint8_t)after_variable;
    }
    if (set_flag) {
        code[at++] = 12;
        code[at++] = 0;
    }
    code[at] = 0;
}

static int live_replay_oracle(void *context,
                              const LogicReplayManifest *manifest,
                              const LogicReplayEvent *events, size_t count,
                              uint32_t timeout_ms,
                              LogicFailureSignature *signature)
{
    LiveReplayOracle *oracle = (LiveReplayOracle *)context;
    LogicReplayResult result;
    if (!timeout_ms || logic_replay_run(manifest, events, count,
                                        oracle->resource_directory,
                                        timeout_ms, oracle->fault_flags,
                                        &result) != 0)
        return -1;
    if (result.outcome == LOGIC_REPLAY_OUTCOME_MISMATCH) {
        *signature = result.signature;
        return 0;
    }
    return 1;
}

static size_t count_input_events(const LogicReplayEvent *events, size_t count)
{
    size_t i, result = 0;
    for (i = 0; i < count; ++i)
        if (events[i].kind == LOGIC_TRACE_KIND_MARK &&
            (events[i].reason == LOGIC_REPLAY_REASON_INPUT_DOWN ||
             events[i].reason == LOGIC_REPLAY_REASON_INPUT_UP))
            ++result;
    return result;
}

static int write_bytes(const char *path, const void *bytes, size_t size)
{
    FILE *file = fopen(path, "wb");
    int result = -1;
    if (!file)
        return -1;
    if (fwrite(bytes, 1, size, file) == size)
        result = 0;
    if (fclose(file) != 0)
        result = -1;
    return result;
}

static int write_u16_at(const char *path, long offset, uint16_t value)
{
    FILE *file = fopen(path, "r+b");
    int result = -1;
    if (!file)
        return -1;
    if (fseek(file, offset, SEEK_SET) == 0 &&
        fputc(value & 0xFF, file) != EOF &&
        fputc(value >> 8, file) != EOF)
        result = 0;
    if (fclose(file) != 0)
        result = -1;
    return result;
}

static int truncate_tail(const char *path, size_t amount)
{
    struct stat status;
    int fd, result = -1;
    if (stat(path, &status) != 0 || status.st_size < 0 ||
        (uintmax_t)status.st_size < amount)
        return -1;
    fd = open(path, O_WRONLY);
    if (fd < 0)
        return -1;
    if (ftruncate(fd, status.st_size - (off_t)amount) == 0)
        result = 0;
    if (close(fd) != 0)
        result = -1;
    return result;
}

static int trace_first_event_is_little_endian(const char *path)
{
    static const unsigned char expected[8] = {0,0,0,0,1,0,0,0};
    unsigned char bytes[sizeof expected];
    FILE *file = fopen(path, "rb");
    int result = -1;
    if (!file)
        return -1;
    if (fseek(file, LOGIC_REPLAY_TRACE_HEADER_SIZE, SEEK_SET) == 0 &&
        fread(bytes, 1, sizeof bytes, file) == sizeof bytes &&
        memcmp(bytes, expected, sizeof bytes) == 0)
        result = 0;
    if (fclose(file) != 0)
        result = -1;
    return result;
}

static int unlink_if_present(const char *path)
{
    if (!path[0] || unlink(path) == 0 || errno == ENOENT)
        return 0;
    fprintf(stderr, "logic replay regression: cannot remove %s: %s\n",
            path, strerror(errno));
    return -1;
}

int main(int argc, char **argv)
{
    char directory[] = "obj/cx16/logic-replay-test-XXXXXX";
    char resource_path[256], manifest_path[256], trace_path[256];
    char live_resource_paths[3][256] = {{0}};
    LogicReplayManifest manifest, decoded, live_manifest;
    LogicReplayEvent events[9], invalid[2], *loaded = NULL, *minimized = NULL;
    LogicReplayEvent live_events[8], *live_minimized = NULL;
    LogicFailureSignature failure, expected_failure, expected_live_failure;
    LogicFailureSignature live_failure;
    LogicReductionOptions options;
    LogicReductionReport report;
    LogicReductionOptions live_options;
    LogicReductionReport live_report;
    LogicReplayResult clean_result, clean_repeat_result;
    LogicReplayResult faulty_result, minimized_result;
    LiveReplayOracle live_oracle;
    size_t loaded_count = 0, minimized_count = 0;
    size_t live_minimized_count = 0;
    MarkerOracleRuns oracle_runs = {0, 0};
    InvalidResultOracleRuns invalid_result_runs = {0};
    unsigned char digest[LOGIC_REPLAY_SHA256_SIZE];
    static const unsigned char abc_sha256[LOGIC_REPLAY_SHA256_SIZE] = {
        0xBA,0x78,0x16,0xBF,0x8F,0x01,0xCF,0xEA,
        0x41,0x41,0x40,0xDE,0x5D,0xAE,0x22,0x23,
        0xB0,0x03,0x61,0xA3,0x96,0x17,0x7A,0x9C,
        0xB4,0x10,0xFF,0x61,0xF2,0x00,0x15,0xAD
    };
    int failed = 0;
    int cleanup_failed = 0;
    int intentional_failure = 0;
    int directory_created = 0;
    size_t i;
    int fail_after_fixtures = argc == 2 &&
                              strcmp(argv[1], "--fail-after-fixtures") == 0;

    if (argc != 1 && !fail_after_fixtures) {
        fprintf(stderr, "usage: %s [--fail-after-fixtures]\n", argv[0]);
        return 2;
    }

    memset(&manifest, 0, sizeof manifest);
    memset(&decoded, 0, sizeof decoded);
    memset(&live_manifest, 0, sizeof live_manifest);
    memset(events, 0, sizeof events);
    memset(live_events, 0, sizeof live_events);
    memset(&options, 0, sizeof options);
    memset(&report, 0, sizeof report);
    memset(&live_options, 0, sizeof live_options);
    memset(&live_report, 0, sizeof live_report);
    memset(&clean_result, 0, sizeof clean_result);
    memset(&clean_repeat_result, 0, sizeof clean_repeat_result);
    memset(&faulty_result, 0, sizeof faulty_result);
    memset(&minimized_result, 0, sizeof minimized_result);
    memset(&failure, 0, sizeof failure);
    memset(&expected_failure, 0, sizeof expected_failure);
    memset(&live_failure, 0, sizeof live_failure);
    memset(&live_oracle, 0, sizeof live_oracle);
    resource_path[0] = manifest_path[0] = trace_path[0] = '\0';

    CHECK(mkdtemp(directory) != NULL);
    directory_created = 1;
    snprintf(resource_path, sizeof resource_path, "%s/LOGIC.0", directory);
    snprintf(manifest_path, sizeof manifest_path, "%s/run.lrm", directory);
    snprintf(trace_path, sizeof trace_path, "%s/run.lrt", directory);
    CHECK(write_bytes(resource_path, "abc", 3) == 0);
    CHECK(logic_replay_sha256_file(resource_path, digest) == 0);
    CHECK(memcmp(digest, abc_sha256, sizeof digest) == 0);

    manifest.version = LOGIC_REPLAY_MANIFEST_VERSION;
    strcpy(manifest.engine, "CX16-interpreter");
    strcpy(manifest.build, "t3code-f508eb73");
    strcpy(manifest.agi_version, "3");
    manifest.resource_count = 1;
    strcpy(manifest.resources[0].name, "LOGIC.0");
    memcpy(manifest.resources[0].sha256, digest, sizeof digest);
    manifest.checkpoint_length = 4;
    manifest.initial_checkpoint[0] = 0x01;
    manifest.initial_checkpoint[1] = 0x23;
    manifest.initial_checkpoint[2] = 0x45;
    manifest.initial_checkpoint[3] = 0x67;
    manifest.rng_seed = UINT64_C(0x123456789ABCDEF0);
    strcpy(manifest.rng_algorithm, "xorshift32-v1");
    manifest.tick_numerator = 60;
    manifest.tick_denominator = 1;
    CHECK(logic_replay_manifest_validate(&manifest) == 0);
    CHECK(logic_replay_manifest_write(manifest_path, &manifest) == 0);
    CHECK(logic_replay_manifest_read(manifest_path, &decoded) == 0);
    CHECK(logic_replay_manifest_matches(&manifest, &decoded));
    CHECK(logic_replay_manifest_verify_resources(&decoded, directory) == 0);
    CHECK(write_bytes(resource_path, "abd", 3) == 0);
    CHECK(logic_replay_manifest_verify_resources(&decoded, directory) != 0);
    CHECK(write_bytes(resource_path, "abc", 3) == 0);

    events[0].sequence = 0;
    events[0].tick = 1;
    events[0].kind = LOGIC_TRACE_KIND_CALL;
    events[0].logic = 9;
    events[0].depth = 1;
    events[1].sequence = 1;
    events[1].tick = 1;
    events[1].kind = LOGIC_TRACE_KIND_MARK;
    events[1].reason = LOGIC_REPLAY_REASON_INPUT_DOWN;
    events[1].field = 42;
    events[1].before = 0;
    events[1].after = 1;
    events[2].sequence = 2;
    events[2].tick = 2;
    events[2].kind = LOGIC_TRACE_KIND_MARK;
    events[2].reason = LOGIC_REPLAY_REASON_TIME;
    events[2].before = 1;
    events[2].after = 2;
    events[3].sequence = 3;
    events[3].tick = 3;
    events[3].kind = LOGIC_TRACE_KIND_MARK;
    events[3].reason = LOGIC_REPLAY_REASON_TIME;
    events[3].before = 1;
    events[3].after = 2;
    events[4].sequence = 4;
    events[4].tick = 4;
    events[4].kind = LOGIC_TRACE_KIND_MARK;
    events[4].reason = LOGIC_REPLAY_REASON_RNG;
    events[4].logic = 7;
    events[4].opcode = 0xAA;
    events[4].after = 0x87654321;
    events[5].sequence = 5;
    events[5].tick = 5;
    events[5].kind = LOGIC_TRACE_KIND_MARK;
    events[5].reason = LOGIC_REPLAY_REASON_TIME;
    events[5].after = 4;
    events[6].sequence = 6;
    events[6].tick = 6;
    events[6].kind = LOGIC_TRACE_KIND_MARK;
    events[6].reason = LOGIC_REPLAY_REASON_RNG;
    events[6].after = 0x13579BDF;
    events[7].sequence = 7;
    events[7].tick = 7;
    events[7].kind = LOGIC_TRACE_KIND_MARK;
    events[7].reason = LOGIC_REPLAY_REASON_TIME;
    events[7].after = 6;
    events[8].sequence = 8;
    events[8].tick = 7;
    events[8].kind = LOGIC_TRACE_KIND_RETURN;
    events[8].logic = 9;
    events[8].depth = 1;

    events[7].kind = LOGIC_TRACE_KIND_MARK;
    events[7].reason = LOGIC_REPLAY_REASON_INPUT_UP;
    events[7].field = 42;
    events[7].before = 1;
    events[7].after = 0;

    CHECK(logic_replay_trace_validate(events, 9) == 0);
    CHECK(logic_replay_trace_write(trace_path, events, 9) == 0);
    if (fail_after_fixtures) {
        fprintf(stderr, "logic replay cleanup probe: intentional failure\n");
        failed = 1;
        intentional_failure = 1;
        goto cleanup;
    }
    CHECK(logic_replay_trace_read(trace_path, &loaded, &loaded_count) == 0);
    CHECK(loaded_count == 9);
    for (i = 0; i < loaded_count; ++i)
        CHECK(same_event(&loaded[i], &events[i]));
    logic_replay_events_free(loaded);
    loaded = NULL;
    CHECK(trace_first_event_is_little_endian(trace_path) == 0);

    memset(invalid, 0, sizeof invalid);
    invalid[0] = events[7];
    invalid[0].sequence = 1;
    CHECK(logic_replay_trace_validate(invalid, 1) != 0);
    invalid[0] = events[1];
    invalid[0].sequence = 1;
    CHECK(logic_replay_trace_validate(invalid, 1) != 0);
    invalid[0] = events[0];
    invalid[0].sequence = 1;
    CHECK(logic_replay_trace_validate(invalid, 1) != 0);
    invalid[0] = events[5];
    invalid[0].sequence = 0;
    invalid[1] = events[6];
    invalid[1].sequence = 2;
    CHECK(logic_replay_trace_validate(invalid, 2) != 0);
    invalid[0] = events[3];
    invalid[0].sequence = 1;
    invalid[1] = events[5];
    invalid[1].sequence = 2;
    invalid[1].tick = 0;
    CHECK(logic_replay_trace_validate(invalid, 2) != 0);

    CHECK(write_u16_at(manifest_path, 8,
                       LOGIC_REPLAY_MANIFEST_VERSION + 1) == 0);
    CHECK(logic_replay_manifest_read(manifest_path, &decoded) != 0);
    CHECK(logic_replay_manifest_write(manifest_path, &manifest) == 0);
    CHECK(write_u16_at(manifest_path,
                       LOGIC_REPLAY_MANIFEST_RECORD_LENGTH_OFFSET,
                       LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE - 1) == 0);
    CHECK(logic_replay_manifest_read(manifest_path, &decoded) != 0);
    CHECK(logic_replay_manifest_write(manifest_path, &manifest) == 0);

    CHECK(write_u16_at(trace_path, 4, LOGIC_REPLAY_TRACE_VERSION + 1) == 0);
    CHECK(logic_replay_trace_read(trace_path, &loaded, &loaded_count) != 0);
    CHECK(logic_replay_trace_write(trace_path, events, 9) == 0);
    CHECK(write_u16_at(trace_path,
                       LOGIC_REPLAY_TRACE_RECORD_LENGTH_OFFSET,
                       LOGIC_REPLAY_EVENT_RECORD_SIZE - 1) == 0);
    CHECK(logic_replay_trace_read(trace_path, &loaded, &loaded_count) != 0);
    CHECK(logic_replay_trace_write(trace_path, events, 9) == 0);
    CHECK(truncate_tail(trace_path, 1) == 0);
    CHECK(logic_replay_trace_read(trace_path, &loaded, &loaded_count) != 0);

cleanup:
    logic_replay_events_free(loaded);
    logic_replay_events_free(minimized);
    logic_replay_events_free(live_minimized);
    if (unlink_if_present(resource_path) != 0) cleanup_failed = 1;
    if (unlink_if_present(live_resource_paths[1]) != 0) cleanup_failed = 1;
    if (unlink_if_present(live_resource_paths[2]) != 0) cleanup_failed = 1;
    if (unlink_if_present(manifest_path) != 0) cleanup_failed = 1;
    if (unlink_if_present(trace_path) != 0) cleanup_failed = 1;
    if (directory_created && rmdir(directory) != 0) {
        fprintf(stderr, "logic replay regression: cannot remove %s: %s\n",
                directory, strerror(errno));
        cleanup_failed = 1;
    }
    if (cleanup_failed)
        return 1;
    if (intentional_failure)
        return 2;
    if (failed)
        return 1;
    puts("logic replay serialization and reduction checks passed");
    return 0;
}
