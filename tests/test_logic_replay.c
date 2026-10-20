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

