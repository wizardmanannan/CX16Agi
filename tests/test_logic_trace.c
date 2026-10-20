#define _POSIX_C_SOURCE 200809L
#include "logic_trace.h"
#include "logic_app_replay.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition, message) \
    do { if (!(condition)) { fprintf(stderr, "%s\n", message); return 1; } } while (0)

static LogicTraceEvent event(uint8_t kind, uint8_t logic, uint8_t depth,
                             uint8_t field, uint32_t tick)
{
    LogicTraceEvent result;
    memset(&result, 0, sizeof result);
    result.kind = kind;
    result.logic = logic;
    result.depth = depth;
    result.field = field;
    result.tick = tick;
    return result;
}

static int check_failure_history(const char *cli, int intentional)
{
    char directory[] = "obj/cx16/logic-trace-test-XXXXXX";
    char path[256], output[256], text[2048];
    uint8_t bytes[LOGIC_TRACE_HEADER_SIZE + 3 * LOGIC_TRACE_RECORD_SIZE];
    LogicTraceEvent call = event(LOGIC_TRACE_KIND_CALL, 1, 1, 0, 1);
    LogicTraceEvent write = event(LOGIC_TRACE_KIND_WRITE, 1, 1, 7, 1);
    LogicTraceEvent flag = event(LOGIC_TRACE_KIND_WRITE, 1, 1, 7, 1);
    FILE *file;
    int result = 1;
    if (!mkdtemp(directory)) return 1;
    snprintf(path, sizeof path, "%s/history.ltrc", directory);
    snprintf(output, sizeof output, "%s/output", directory);
    write.sequence = 1;
    write.after = 9;
    write.reason = 1;
    write.opcode = 39;
    flag.sequence = 2;
    flag.reason = 2;
    flag.opcode = 73;
    flag.before = 1;
    if (logic_trace_header_encode(3, bytes, LOGIC_TRACE_HEADER_SIZE) ||
        logic_trace_event_encode(&call, bytes + LOGIC_TRACE_HEADER_SIZE,
                                 LOGIC_TRACE_RECORD_SIZE) ||
        logic_trace_event_encode(&write, bytes + LOGIC_TRACE_HEADER_SIZE +
                                 LOGIC_TRACE_RECORD_SIZE, LOGIC_TRACE_RECORD_SIZE) ||
        logic_trace_event_encode(&flag, bytes + LOGIC_TRACE_HEADER_SIZE +
                                 2 * LOGIC_TRACE_RECORD_SIZE, LOGIC_TRACE_RECORD_SIZE))
        goto cleanup;
    file = fopen(path, "wb");
    if (!file) goto cleanup;
    int written = fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes;
    if (fclose(file) || !written) goto cleanup;
    if (intentional) { result = 2; goto cleanup; }
    for (unsigned format = 0; format < 3; ++format) {
        if (format == 2) {
            /* Legacy assembly writes identify their space by AGI opcode. */
            bytes[LOGIC_TRACE_HEADER_SIZE + LOGIC_TRACE_RECORD_SIZE + 12] = 3;
            bytes[LOGIC_TRACE_HEADER_SIZE + LOGIC_TRACE_RECORD_SIZE + 24] = 0;
            bytes[LOGIC_TRACE_HEADER_SIZE + 2 * LOGIC_TRACE_RECORD_SIZE + 12] = 12;
            bytes[LOGIC_TRACE_HEADER_SIZE + 2 * LOGIC_TRACE_RECORD_SIZE + 24] = 0;
        }
        if (format) {
            uint8_t checkpoint[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
            memcpy(checkpoint, "X16R", 4);
            checkpoint[4] = LOGIC_APP_REPLAY_SNAPSHOT_VERSION;
            checkpoint[5] = 1;
            checkpoint[20] = 1;
            checkpoint[41] = 3;
            checkpoint[42] = 1; /* Ring eviction makes full pairing inconclusive. */
            memcpy(checkpoint + LOGIC_APP_REPLAY_TRACE_OFFSET,
                   bytes + LOGIC_TRACE_HEADER_SIZE, 3 * LOGIC_TRACE_RECORD_SIZE);
            file = fopen(path, "wb");
            if (!file) goto cleanup;
            written = fwrite(checkpoint, 1, sizeof checkpoint, file) == sizeof checkpoint;
            if (fclose(file) || !written) goto cleanup;
        }
        for (unsigned command = 0; command < 5; ++command) {
            int status, fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0600);
            if (fd < 0) goto cleanup;
            pid_t child = fork();
            if (!child) {
                if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0)
                    _exit(126);
                close(fd);
                if (command >= 3)
                    execl(cli, cli, "last-writer", path, "1", "7", "--space",
                          command == 3 ? "variable" : "flag", (char *)NULL);
                else if (command == 1)
                    execl(cli, cli, "last-writer", path, "1", "7", (char *)NULL);
                else
                    execl(cli, cli, command == 0 ? "recent" : "check", path, (char *)NULL);
                _exit(127);
            }
            close(fd);
            if (child < 0 || waitpid(child, &status, 0) != child || !WIFEXITED(status))
                goto cleanup;
            file = fopen(output, "rb");
            if (!file) goto cleanup;
            size_t length = fread(text, 1, sizeof text - 1, file);
            text[length] = 0;
            if (fclose(file)) goto cleanup;
            if ((command != 2 && (WEXITSTATUS(status) ||
                    !strstr(text, command == 0 || command == 3 ?
                        "after=00000009" : "after=00000000"))) ||
                (command == 2 && (!WEXITSTATUS(status) ||
                    !strstr(text, format ? "incomplete trace history" : "unmatched call")))) {
                fprintf(stderr, "failure history CLI regression: command=%u status=%d output=%s\n",
                        command, WEXITSTATUS(status), text);
                goto cleanup;
            }
        }
    }
    result = 0;
cleanup:
    if (unlink(path) && access(path, F_OK) == 0) result = 1;
    if (unlink(output) && access(output, F_OK) == 0) result = 1;
    if (rmdir(directory)) result = 1;
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 1;
    int cli_result = check_failure_history(argv[1], argc == 3 &&
                                          !strcmp(argv[2], "--fail-after-fixtures"));
    if (cli_result) return cli_result;
    LogicTraceEvent storage[3];
    LogicTraceRing ring;
    LogicTraceEvent value, decoded, history[6], last_writer;
    LogicTraceCallFrame call_stack[3];
    LogicTraceFilter filter;
    uint8_t header[LOGIC_TRACE_HEADER_SIZE];
    uint8_t record[LOGIC_TRACE_RECORD_SIZE];
    size_t bad_index = 0;
    uint32_t record_count = 0;

    CHECK(logic_trace_ring_init(&ring, storage, 3) == 0, "ring init failed");
    value = event(LOGIC_TRACE_KIND_WRITE, 1, 0, 7, 10);
    value.before = 3;
    value.after = 4;
    CHECK(logic_trace_record(&ring, &value) == 0, "first record failed");
    value = event(LOGIC_TRACE_KIND_CALL, 2, 1, 0, 11);
    value.logical_pc = 0x1234;
    CHECK(logic_trace_record(&ring, &value) == 0, "second record failed");
    value = event(LOGIC_TRACE_KIND_RETURN, 2, 1, 0, 12);
    CHECK(logic_trace_record(&ring, &value) == 0, "third record failed");
    value = event(LOGIC_TRACE_KIND_WRITE, 1, 0, 7, 13);
    value.before = 4;
    value.after = 9;
    CHECK(logic_trace_record(&ring, &value) == 0, "overflow record failed");

    CHECK(logic_trace_ring_count(&ring) == 3, "ring count is not bounded");
    CHECK(logic_trace_ring_dropped(&ring) == 1, "ring overflow count differs");
    CHECK(logic_trace_ring_get(&ring, 0, &decoded) == 0 &&
          decoded.sequence == 1 && decoded.kind == LOGIC_TRACE_KIND_CALL,
          "ring did not evict the oldest event deterministically");
    CHECK(logic_trace_ring_get(&ring, 2, &decoded) == 0 &&
          decoded.sequence == 3 && decoded.after == 9,
          "ring order or assigned sequence differs");

    memset(&filter, 0, sizeof filter);
    filter.flags = LOGIC_TRACE_FILTER_LOGIC;
    filter.logic = 1;
    CHECK(logic_trace_ring_get(&ring, 1, &decoded) == 0 &&
          logic_trace_event_matches(&decoded, &filter) == 0,
          "filter matched the wrong logic");
    filter.flags = LOGIC_TRACE_FILTER_KIND;
    filter.kind = LOGIC_TRACE_KIND_WRITE;
    CHECK(logic_trace_ring_get(&ring, 1, &decoded) == 0 &&
          logic_trace_event_matches(&decoded, &filter) == 0,
          "filter matched the wrong kind");
    CHECK(logic_trace_ring_get(&ring, 2, &decoded) == 0 &&
          logic_trace_event_matches(&decoded, &filter) != 0,
          "filter missed matching event");

    CHECK(logic_trace_header_encode(7, header, sizeof header) == 0,
          "header encode failed");
    CHECK(header[4] == 1 && header[5] == 0 && header[6] == 25 &&
          header[7] == 0 && header[8] == 7 && header[9] == 0 &&
          header[10] == 0 && header[11] == 0,
          "header integers are not explicitly little-endian");
    CHECK(logic_trace_header_decode(header, sizeof header, &record_count) == 0 &&
          record_count == 7,
          "header decode failed");
    CHECK(logic_trace_header_decode(header, sizeof header - 1,
                                    &record_count) != 0,
          "truncated header was accepted");
    header[4] = 2;
    CHECK(logic_trace_header_decode(header, sizeof header, &record_count) != 0,
          "unsupported version was accepted");
    CHECK(logic_trace_header_encode(7, header, sizeof header) == 0,
          "header re-encode failed");
    header[6]++;
    CHECK(logic_trace_header_decode(header, sizeof header, &record_count) != 0,
          "bad record length was accepted");

    value = event(LOGIC_TRACE_KIND_WRITE, 4, 0, 5, 21);
    value.sequence = 0x12345678UL;
    value.logical_pc = 0x9abc;
    value.opcode = 0x77;
    value.bank = 42;
    value.before = 0x10203040UL;
    value.after = 0xa1b2c3d4UL;
    value.reason = 6;
    CHECK(logic_trace_event_encode(&value, record, sizeof record) == 0,
          "event encode failed");
    CHECK(record[0] == 0x78 && record[1] == 0x56 && record[2] == 0x34 &&
          record[3] == 0x12 && record[8] == LOGIC_TRACE_KIND_WRITE &&
          record[10] == 0xbc && record[11] == 0x9a &&
          record[16] == 0x40 && record[19] == 0x10 &&
          record[20] == 0xd4 && record[23] == 0xa1,
          "event bytes are not explicitly little-endian");
    CHECK(logic_trace_event_decode(record, sizeof record, &decoded) == 0 &&
          memcmp(&value, &decoded, sizeof value) == 0,
          "event serialization roundtrip differs");
    CHECK(logic_trace_event_decode(record, sizeof record - 1, &decoded) != 0,
          "truncated event was accepted");

    history[0] = event(LOGIC_TRACE_KIND_WRITE, 1, 0, 7, 30);
    history[0].sequence = 20;
    history[0].before = 0;
    history[0].after = 4;
    history[1] = event(LOGIC_TRACE_KIND_WRITE, 1, 0, 7, 31);
    history[1].sequence = 21;
    history[1].before = 4;
    history[1].after = 9;
    history[2] = event(LOGIC_TRACE_KIND_CALL, 2, 1, 0, 32);
    history[2].sequence = 22;
    history[3] = event(LOGIC_TRACE_KIND_CALL, 3, 2, 0, 33);
    history[3].sequence = 23;
    history[4] = event(LOGIC_TRACE_KIND_RETURN, 3, 2, 0, 34);
    history[4].sequence = 24;
    history[5] = event(LOGIC_TRACE_KIND_RETURN, 2, 1, 0, 35);
    history[5].sequence = 25;
    CHECK(logic_trace_validate(history, 6, call_stack, 3, &bad_index) == 0,
          "valid nested call/return trace was rejected");
    CHECK(logic_trace_find_last_writer(history, 6, 1, 7, &last_writer) == 0 &&
          last_writer.sequence == 21 && last_writer.after == 9,
          "last-writer query returned the wrong event");
    history[3].sequence = 99;
    CHECK(logic_trace_validate(history, 6, call_stack, 3, &bad_index) != 0 &&
          bad_index == 3,
          "out-of-order sequence was accepted");
    history[3].sequence = 23;
    history[4].logic = 2;
    CHECK(logic_trace_validate(history, 6, call_stack, 3, &bad_index) != 0 &&
          bad_index == 4,
          "mismatched return was accepted");

    history[0] = event(LOGIC_TRACE_KIND_CALL, 1, 1, 9, 40);
    history[0].sequence = 0;
    history[0].before = 1;
    history[0].after = 2;
    history[0].logical_pc = 10;
    history[0].opcode = 22;
    history[1] = history[0];
    history[1].sequence = 1;
    history[1].kind = LOGIC_TRACE_KIND_RETURN;
    history[1].before = 3; /* another child of the same caller */
    history[1].after = 1;
    CHECK(logic_trace_validate(history, 2, call_stack, 3, &bad_index) != 0 && bad_index == 1,
          "return from a different child of the same caller was accepted");
    history[1].before = 2;
    history[1].logical_pc = 11;
    CHECK(logic_trace_validate(history, 2, call_stack, 3, &bad_index) != 0 && bad_index == 1,
          "return to a different call site was accepted");
    history[1].logical_pc = 10;
    CHECK(logic_trace_validate(history, 2, call_stack, 3, &bad_index) == 0,
          "production caller/callee transition was rejected");

    puts("logic trace regression passed");
    return 0;
}
