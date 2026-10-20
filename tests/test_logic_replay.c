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

    decoded = manifest;
    decoded.resources[0].sha256[0] ^= 0xFF;
    CHECK(!logic_replay_manifest_matches(&manifest, &decoded));
    options.max_attempts = 128;
    options.timeout_ms = 10000;
    options.agreement_runs = 2;
    CHECK(logic_reduce_trace(&manifest, events, 9, marker_oracle, &oracle_runs,
                             &options, &minimized, &minimized_count,
                             &failure, &report) == 0);
    expected_failure.kind = LOGIC_TRACE_KIND_MARK;
    expected_failure.logic = 7;
    expected_failure.logical_pc = 0x1234;
    expected_failure.opcode = 0xAA;
    expected_failure.reason = LOGIC_REPLAY_REASON_RNG;
    expected_failure.code = 19;
    CHECK(report.original_count == 9 && report.minimized_count == 1);
    CHECK(minimized_count == 1 && minimized[0].opcode == 0xAA);
    for (i = 0; i < minimized_count; ++i)
        CHECK(minimized[i].sequence == i);
    CHECK(report.attempts <= options.max_attempts);
    CHECK(report.repeated_run_agreement == 1);
    CHECK(same_signature(&failure, &expected_failure));
    CHECK(oracle_runs.runs == report.oracle_runs);
    CHECK(oracle_runs.invalid_candidates == 0);
    CHECK(logic_replay_trace_validate(minimized, minimized_count) == 0);

    logic_replay_events_free(minimized);
    minimized = NULL;
    options.max_attempts = 1;
    CHECK(logic_reduce_trace(&manifest, events, 9,
                             invalid_candidate_result_oracle,
                             &invalid_result_runs, &options, &minimized,
                             &minimized_count, &failure, &report) == -1);
    CHECK(invalid_result_runs.runs == 3 && report.oracle_runs == 3);
    CHECK(report.attempts == 1 && !report.limit_reached);
    CHECK(minimized == NULL && minimized_count == 0);

    options.max_attempts = 1;
    CHECK(logic_reduce_trace(&manifest, events, 9, marker_oracle, &oracle_runs,
                             &options, &minimized, &minimized_count,
                             &failure, &report) == 0);
    CHECK(report.attempts == 1 && report.limit_reached == 1);
    CHECK(minimized_count == report.minimized_count && minimized_count > 1);
    CHECK(logic_replay_trace_validate(minimized, minimized_count) == 0);
    CHECK(oracle_runs.invalid_candidates == 0);

    /* The replay case uses the same raw logic bytes the emulator loads. */
    live_manifest = manifest;
    live_manifest.resource_count = 3;
    live_manifest.checkpoint_length = 20;
    memcpy(live_manifest.initial_checkpoint, "LCX1", 4);
    live_manifest.initial_checkpoint[4] = 0x00;
    live_manifest.initial_checkpoint[5] = 0xA3;
    live_manifest.initial_checkpoint[6] = 0x80;
    live_manifest.initial_checkpoint[7] = 0xA3;
    live_manifest.initial_checkpoint[8] = 0x02;
    live_manifest.initial_checkpoint[9] = 0xA3;
    live_manifest.initial_checkpoint[10] = 0x12;
    live_manifest.initial_checkpoint[11] = 0x34;
    live_manifest.initial_checkpoint[12] = 0x78;
    live_manifest.initial_checkpoint[13] = 0x56;
    live_manifest.initial_checkpoint[14] = 43;
    live_manifest.initial_checkpoint[15] = 39;
    live_manifest.initial_checkpoint[16] = 2;
    live_manifest.initial_checkpoint[17] = 0;
    live_manifest.initial_checkpoint[18] = 0;
    live_manifest.initial_checkpoint[19] = 0;
    strcpy(live_manifest.rng_algorithm, "xorshift32-v1");
    live_manifest.rng_seed = UINT64_C(0x123456789ABCDEF0);
    live_manifest.tick_numerator = 60;
    live_manifest.tick_denominator = 1;
    {
        uint8_t code[3][80];
        static const char *const names[3] = {
            "LOGIC.0", "LOGIC.1", "LOGIC.2"
        };
        unsigned i;
        make_replay_logic_code(code[0], sizeof code[0], 4, 26, 0, 1, 1, 1, 3, 0);
        make_replay_logic_code(code[1], sizeof code[1], 7, 24, 1, 2, 2, 0, 4, 0);
        make_replay_logic_code(code[2], sizeof code[2], 9, 1, 2, 0xFF, 0, 0, 0xFF, 1);
        for (i = 0; i < 3; ++i) {
            CHECK(snprintf(live_resource_paths[i], sizeof live_resource_paths[i],
                           "%s/%s", directory, names[i]) > 0);
            CHECK(write_bytes(live_resource_paths[i], code[i], sizeof code[i]) == 0);
            strcpy(live_manifest.resources[i].name, names[i]);
            CHECK(logic_replay_sha256_file(live_resource_paths[i],
                                           live_manifest.resources[i].sha256) == 0);
        }
    }
    CHECK(logic_replay_manifest_validate(&live_manifest) == 0);
    CHECK(logic_replay_manifest_verify_resources(&live_manifest, directory) == 0);

    live_events[0].sequence = 0;
    live_events[0].tick = 1;
    live_events[0].kind = LOGIC_TRACE_KIND_MARK;
    live_events[0].reason = LOGIC_REPLAY_REASON_INPUT_DOWN;
    live_events[0].field = 13;
    live_events[0].before = 0;
    live_events[0].after = 1;
    live_events[1] = live_events[0];
    live_events[1].sequence = 1;
    live_events[1].tick = 2;
    live_events[1].reason = LOGIC_REPLAY_REASON_INPUT_UP;
    live_events[1].before = 1;
    live_events[1].after = 0;
    live_events[2] = live_events[0];
    live_events[2].sequence = 2;
    live_events[2].tick = 3;
    live_events[2].field = 14;
    live_events[3] = live_events[1];
    live_events[3].sequence = 3;
    live_events[3].tick = 4;
    live_events[3].field = 14;
    live_events[4] = live_events[0];
    live_events[4].sequence = 4;
    live_events[4].tick = 5;
    live_events[4].field = 15;
    live_events[5] = live_events[1];
    live_events[5].sequence = 5;
    live_events[5].tick = 6;
    live_events[5].field = 15;
    live_events[6].sequence = 6;
    live_events[6].tick = 7;
    live_events[6].kind = LOGIC_TRACE_KIND_MARK;
    live_events[6].reason = LOGIC_REPLAY_REASON_TIME;
    live_events[6].before = 0;
    live_events[6].after = 1;
    live_events[7].sequence = 7;
    live_events[7].tick = 7;
    live_events[7].kind = LOGIC_TRACE_KIND_MARK;
    live_events[7].reason = LOGIC_REPLAY_REASON_TIME;
    live_events[7].before = 1;
    live_events[7].after = 6;
    CHECK(logic_replay_trace_validate(live_events, 8) == 0);

    CHECK(logic_replay_run(&live_manifest, live_events, 8, directory,
                           60000, LOGIC_REPLAY_FAULT_NONE,
                           &clean_result) == 0);
    if (clean_result.outcome != LOGIC_REPLAY_OUTCOME_PASS)
        fprintf(stderr, "logic replay live result: %s (%s), events=%lu/%u ticks=%lu/%lu rng=%lu/%08lx signature=%u:%u:%04x:%u:%u:%08lx\n",
                logic_replay_outcome_name(clean_result.outcome),
                clean_result.detail,
                (unsigned long)clean_result.events_consumed, 8u,
                (unsigned long)clean_result.ticks_executed,
                (unsigned long)clean_result.tick_count,
                (unsigned long)clean_result.rng_call_count,
                (unsigned long)clean_result.rng_final_state,
                clean_result.signature.kind, clean_result.signature.logic,
                clean_result.signature.logical_pc, clean_result.signature.opcode,
                clean_result.signature.reason,
                (unsigned long)clean_result.signature.code);
    CHECK(clean_result.outcome == LOGIC_REPLAY_OUTCOME_PASS);
    CHECK(clean_result.events_consumed == 8);
    CHECK(clean_result.semantic_hash != 0);
    CHECK(clean_result.tick_count == 7 && clean_result.ticks_executed == 7);
    CHECK(clean_result.rng_call_count == 0 &&
          clean_result.rng_final_state == UINT32_C(0x9abcdef0));
    CHECK(logic_replay_run(&live_manifest, live_events, 8, directory,
                           60000, LOGIC_REPLAY_FAULT_NONE,
                           &clean_repeat_result) == 0);
    CHECK(clean_repeat_result.outcome == LOGIC_REPLAY_OUTCOME_PASS);
    CHECK(clean_repeat_result.events_consumed == clean_result.events_consumed &&
          clean_repeat_result.tick_count == clean_result.tick_count &&
          clean_repeat_result.ticks_executed == clean_result.ticks_executed &&
          clean_repeat_result.rng_call_count == clean_result.rng_call_count &&
          clean_repeat_result.rng_final_state == clean_result.rng_final_state &&
          clean_repeat_result.semantic_hash == clean_result.semantic_hash &&
          clean_repeat_result.room == clean_result.room &&
          clean_repeat_result.object_count == clean_result.object_count &&
          memcmp(clean_repeat_result.variables, clean_result.variables,
                 sizeof clean_result.variables) == 0 &&
          memcmp(clean_repeat_result.flags, clean_result.flags,
                 sizeof clean_result.flags) == 0 &&
          memcmp(clean_repeat_result.objects, clean_result.objects,
                 clean_result.object_count * sizeof clean_result.objects[0]) == 0);
    CHECK(logic_replay_run(&live_manifest, live_events, 8, directory,
                           60000, LOGIC_REPLAY_FAULT_OMIT_METADATA_SAVE,
                           &faulty_result) == 0);
    CHECK(faulty_result.outcome == LOGIC_REPLAY_OUTCOME_MISMATCH);
    expected_live_failure.kind = LOGIC_TRACE_KIND_MARK;
    expected_live_failure.logic = 1;
    expected_live_failure.logical_pc = 0xA138;
    expected_live_failure.opcode = 22;
    expected_live_failure.reason = 5; /* context mismatch */
    expected_live_failure.code = 0x00000701; /* entryPoint low byte */
    if (!same_signature(&faulty_result.signature, &expected_live_failure))
        fprintf(stderr, "live context failure signature=%u:%u:%04x:%u:%u:%08x\n",
                faulty_result.signature.kind, faulty_result.signature.logic,
                faulty_result.signature.logical_pc, faulty_result.signature.opcode,
                faulty_result.signature.reason, faulty_result.signature.code);
    CHECK(same_signature(&faulty_result.signature, &expected_live_failure));
    CHECK(faulty_result.events_consumed == 8);

    live_oracle.resource_directory = directory;
    live_oracle.fault_flags = LOGIC_REPLAY_FAULT_OMIT_METADATA_SAVE;
    live_options.max_attempts = 32;
    live_options.timeout_ms = 60000;
    live_options.agreement_runs = 2;
    CHECK(logic_reduce_trace(&live_manifest, live_events, 8,
                             live_replay_oracle, &live_oracle, &live_options,
                             &live_minimized, &live_minimized_count,
                             &live_failure, &live_report) == 0);
    CHECK(live_report.repeated_run_agreement == 1);
    CHECK(live_report.original_count == 8 &&
          live_report.minimized_count == live_minimized_count);
    CHECK(live_minimized_count < 8);
    CHECK(count_input_events(live_minimized, live_minimized_count) <
          count_input_events(live_events, 8));
    CHECK(logic_replay_trace_validate(live_minimized, live_minimized_count) == 0);
    for (i = 0; i < live_minimized_count; ++i)
        CHECK(live_minimized[i].sequence == i);
    CHECK(same_signature(&live_failure, &expected_live_failure));
    CHECK(logic_replay_run(&live_manifest, live_minimized, live_minimized_count,
                           directory, 60000,
                           LOGIC_REPLAY_FAULT_OMIT_METADATA_SAVE,
                           &minimized_result) == 0);
    CHECK(minimized_result.outcome == LOGIC_REPLAY_OUTCOME_MISMATCH);
    CHECK(minimized_result.events_consumed == live_minimized_count);
    CHECK(same_signature(&minimized_result.signature, &faulty_result.signature));
    logic_replay_events_free(live_minimized);
    live_minimized = NULL;

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
