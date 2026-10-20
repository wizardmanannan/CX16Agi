#include "logic_trace.h"
#include "logic_app_replay.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr,
        "usage:\n"
        "  %s recent TRACE [--count N] [--kind write|call|return|mark] "
        "[--logic N] [--field N]\n"
        "  %s last-writer TRACE LOGIC FIELD [--space variable|flag]\n"
        "  %s check TRACE [LOGIC FIELD]\n",
        program, program, program);
}

static int parse_number(const char *text, unsigned long maximum,
                        unsigned long *value)
{
    char *end;
    unsigned long parsed;
    if (!text || !*text || *text == '-')
        return -1;
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || *end != '\0' || parsed > maximum)
        return -1;
    *value = parsed;
    return 0;
}

static int parse_kind(const char *text, uint8_t *kind)
{
    if (strcmp(text, "write") == 0)
        *kind = LOGIC_TRACE_KIND_WRITE;
    else if (strcmp(text, "call") == 0)
        *kind = LOGIC_TRACE_KIND_CALL;
    else if (strcmp(text, "return") == 0)
        *kind = LOGIC_TRACE_KIND_RETURN;
    else if (strcmp(text, "mark") == 0)
        *kind = LOGIC_TRACE_KIND_MARK;
    else
        return -1;
    return 0;
}

static const char *kind_name(uint8_t kind)
{
    switch (kind) {
    case LOGIC_TRACE_KIND_WRITE: return "write";
    case LOGIC_TRACE_KIND_CALL: return "call";
    case LOGIC_TRACE_KIND_RETURN: return "return";
    case LOGIC_TRACE_KIND_MARK: return "mark";
    default: return "invalid";
    }
}

static uint8_t write_space(const LogicTraceEvent *event)
{
    if (event->reason) return event->reason;
    /* Older assembly producers encode the destination in the AGI opcode. */
    if (event->opcode >= 12 && event->opcode <= 17)
        return LOGIC_TRACE_WRITE_REASON_FLAG;
    if ((event->opcode >= 1 && event->opcode <= 11) || event->opcode == 130)
        return LOGIC_TRACE_WRITE_REASON_VARIABLE;
    return LOGIC_TRACE_WRITE_REASON_UNSPECIFIED;
}

static void print_event(const LogicTraceEvent *event)
{
    printf("seq=%lu tick=%lu kind=%s logic=%u pc=%04X opcode=%02X "
           "depth=%u bank=%u field=%u before=%08lX after=%08lX reason=%u\n",
           (unsigned long)event->sequence, (unsigned long)event->tick,
           kind_name(event->kind), (unsigned int)event->logic,
           (unsigned int)event->logical_pc, (unsigned int)event->opcode,
           (unsigned int)event->depth, (unsigned int)event->bank,
           (unsigned int)event->field, (unsigned long)event->before,
           (unsigned long)event->after, (unsigned int)event->reason);
}

static int load_trace(const char *path, LogicTraceEvent **events_out,
                      size_t *count_out, int validate_pairing)
{
    FILE *file = NULL;
    LogicTraceEvent *events = NULL;
    LogicTraceCallFrame *call_stack = NULL;
    size_t count = 0;
    size_t expected_count;
    size_t bad_index = 0;
    uint8_t header[LOGIC_TRACE_HEADER_SIZE];
    uint8_t record[LOGIC_TRACE_RECORD_SIZE];
    size_t amount;
    uint32_t declared_count = 0;
    int trailing;
    int application = 0, incomplete = 0;
    int result = -1;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open trace: %s\n", path);
        goto done;
    }
    if (fread(header, 1, sizeof header, file) != sizeof header) {
        fprintf(stderr, "truncated trace header: %s\n", path);
        goto done;
    }
    if (!memcmp(header, "X16R", 4)) {
        uint8_t prefix[LOGIC_APP_REPLAY_OUTPUT_PREFIX_SIZE];
        application = 1;
        memcpy(prefix, header, sizeof header);
        if (fread(prefix + sizeof header, 1, sizeof prefix - sizeof header, file) !=
                sizeof prefix - sizeof header ||
            prefix[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION || prefix[5] != 1 ||
            prefix[41] > LOGIC_APP_REPLAY_TRACE_MAX ||
            prefix[20] > LOGIC_APP_REPLAY_CONTEXT_FRAME_MAX ||
            fseek(file, 0, SEEK_END) || ftell(file) != LOGIC_APP_REPLAY_OUTPUT_SIZE ||
            fseek(file, LOGIC_APP_REPLAY_TRACE_OFFSET, SEEK_SET)) {
            fprintf(stderr, "unsupported or truncated application checkpoint: %s\n", path);
            goto done;
        }
        declared_count = prefix[41];
        incomplete = prefix[20] || prefix[42] || prefix[43] || prefix[44] ||
                     prefix[45] || (prefix[58] & LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED);
    } else if (logic_trace_header_decode(header, sizeof header, &declared_count) != 0) {
        fprintf(stderr, "unsupported or malformed trace header: %s\n", path);
        goto done;
    }
    expected_count = (size_t)declared_count;
    if ((uint32_t)expected_count != declared_count ||
        expected_count > (size_t)-1 / sizeof *events) {
        fprintf(stderr, "trace is too large: %s\n", path);
        goto done;
    }
    if (expected_count != 0) {
        events = (LogicTraceEvent *)malloc(expected_count * sizeof *events);
        if (!events) {
            fprintf(stderr, "out of memory reading trace: %s\n", path);
            goto done;
        }
    }
    while (count < expected_count) {
        amount = fread(record, 1, sizeof record, file);
        if (amount != sizeof record) {
            if (ferror(file))
                fprintf(stderr, "error reading trace record %lu: %s\n",
                        (unsigned long)count, path);
            else
                fprintf(stderr, "truncated trace record %lu: %s\n",
                        (unsigned long)count, path);
            goto done;
        }
        if (logic_trace_event_decode(record, sizeof record, &events[count]) != 0) {
            fprintf(stderr, "invalid trace record %lu: %s\n",
                    (unsigned long)count, path);
            goto done;
        }
        ++count;
    }
    if (application && fseek(file, 0, SEEK_END)) goto done;
    trailing = fgetc(file);
    if (trailing != EOF) {
        fprintf(stderr, "trace has data beyond its declared record count: %s\n",
                path);
        goto done;
    }
    if (ferror(file)) {
        fprintf(stderr, "error reading trace: %s\n", path);
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        fprintf(stderr, "error closing trace: %s\n", path);
        goto done;
    }
    file = NULL;

    if (validate_pairing) {
        if (incomplete) {
            fprintf(stderr, "incomplete trace history: bounded or active application context prevents full pairing validation: %s\n", path);
            goto done;
        }
        if (expected_count > (size_t)-1 / sizeof *call_stack) {
            fprintf(stderr, "trace is too large to validate: %s\n", path);
            goto done;
        }
        call_stack = (LogicTraceCallFrame *)malloc(
            (expected_count ? expected_count : 1) * sizeof *call_stack);
        if (!call_stack) {
            fprintf(stderr, "out of memory validating trace: %s\n", path);
            goto done;
        }
        if (logic_trace_validate(events, count, call_stack, expected_count,
                                 &bad_index) != 0) {
            if (bad_index == count)
                fprintf(stderr, "unmatched call at end of trace: %s\n", path);
            else
                fprintf(stderr, "invalid trace sequence or call/return at record "
                                "%lu: %s\n", (unsigned long)bad_index, path);
            goto done;
        }
    }
    *events_out = events;
    *count_out = count;
    events = NULL;
    result = 0;

done:
    if (file)
        (void)fclose(file);
    free(call_stack);
    free(events);
    return result;
}

static int command_recent(int argc, char **argv)
{
    LogicTraceEvent *events = NULL;
    LogicTraceEvent *selected = NULL;
    LogicTraceFilter filter;
    size_t count = 0;
    size_t limit = 20;
    size_t selected_count = 0;
    size_t i;
    int arg;
    unsigned long parsed;
    memset(&filter, 0, sizeof filter);
    if (argc < 3 || load_trace(argv[2], &events, &count, 0) != 0)
        return 1;
    for (arg = 3; arg < argc; ++arg) {
        if (strcmp(argv[arg], "--count") == 0 && arg + 1 < argc) {
            if (parse_number(argv[++arg], (unsigned long)-1, &parsed) != 0) {
                fprintf(stderr, "invalid count\n");
                free(events);
                return 1;
            }
            limit = (size_t)parsed;
        } else if (strcmp(argv[arg], "--kind") == 0 && arg + 1 < argc) {
            if (parse_kind(argv[++arg], &filter.kind) != 0) {
                fprintf(stderr, "invalid kind\n");
                free(events);
                return 1;
            }
            filter.flags |= LOGIC_TRACE_FILTER_KIND;
        } else if (strcmp(argv[arg], "--logic") == 0 && arg + 1 < argc) {
            if (parse_number(argv[++arg], UINT8_MAX, &parsed) != 0) {
                fprintf(stderr, "invalid logic number\n");
                free(events);
                return 1;
            }
            filter.logic = (uint8_t)parsed;
            filter.flags |= LOGIC_TRACE_FILTER_LOGIC;
        } else if (strcmp(argv[arg], "--field") == 0 && arg + 1 < argc) {
            if (parse_number(argv[++arg], UINT8_MAX, &parsed) != 0) {
                fprintf(stderr, "invalid field number\n");
                free(events);
                return 1;
            }
            filter.field = (uint8_t)parsed;
            filter.flags |= LOGIC_TRACE_FILTER_FIELD;
        } else {
            usage(argv[0]);
            free(events);
            return 2;
        }
    }
    if (limit > count)
        limit = count;
    if (limit > (size_t)-1 / sizeof *selected) {
        fprintf(stderr, "requested event count is too large\n");
        free(events);
        return 1;
    }
    if (limit != 0) {
        selected = (LogicTraceEvent *)malloc(limit * sizeof *selected);
        if (!selected) {
            fprintf(stderr, "out of memory selecting recent events\n");
            free(events);
            return 1;
        }
    }
    i = count;
    while (i != 0 && selected_count < limit) {
        const LogicTraceEvent *event = &events[--i];
        if (logic_trace_event_matches(event, &filter))
            selected[selected_count++] = *event;
    }
    while (selected_count != 0)
        print_event(&selected[--selected_count]);
    free(selected);
    free(events);
    return 0;
}

static int command_last_writer(int argc, char **argv)
{
    LogicTraceEvent *events = NULL;
    LogicTraceEvent event;
    size_t count = 0;
    unsigned long logic;
    unsigned long field;
    uint8_t space = LOGIC_TRACE_WRITE_REASON_UNSPECIFIED;
    int found;
    if ((argc != 5 && argc != 7) ||
        parse_number(argv[3], UINT8_MAX, &logic) != 0 ||
        parse_number(argv[4], UINT8_MAX, &field) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc == 7) {
        if (!strcmp(argv[5], "--space") && !strcmp(argv[6], "variable"))
            space = LOGIC_TRACE_WRITE_REASON_VARIABLE;
        else if (!strcmp(argv[5], "--space") && !strcmp(argv[6], "flag"))
            space = LOGIC_TRACE_WRITE_REASON_FLAG;
        else { usage(argv[0]); return 2; }
    }
    if (load_trace(argv[2], &events, &count, 0) != 0)
        return 1;
    found = 1;
    while (count) {
        const LogicTraceEvent *candidate = &events[--count];
        if (candidate->kind == LOGIC_TRACE_KIND_WRITE &&
            candidate->logic == logic && candidate->field == field &&
            (!space || write_space(candidate) == space)) {
            event = *candidate;
            found = 0;
            break;
        }
    }
    free(events);
    if (found != 0) {
        fprintf(stderr, "no writer for logic %lu field %lu\n", logic, field);
        return 1;
    }
    print_event(&event);
    return 0;
}

static int find_latest_write(const LogicTraceEvent *events, size_t count,
                             LogicTraceEvent *event)
{
    while (count != 0) {
        const LogicTraceEvent *candidate = &events[--count];
        if (candidate->kind == LOGIC_TRACE_KIND_WRITE) {
            *event = *candidate;
            return 0;
        }
    }
    return 1;
}

static int command_check(int argc, char **argv)
{
    LogicTraceEvent *events = NULL;
    LogicTraceEvent event;
    size_t count = 0;
    unsigned long logic = 0;
    unsigned long field = 0;
    int found;
    if (argc != 3 && argc != 5) {
        usage(argv[0]);
        return 2;
    }
    if (argc == 5 &&
        (parse_number(argv[3], UINT8_MAX, &logic) != 0 ||
         parse_number(argv[4], UINT8_MAX, &field) != 0)) {
        usage(argv[0]);
        return 2;
    }
    if (load_trace(argv[2], &events, &count, 1) != 0)
        return 1;
    printf("trace order and call/return pairing consistent (%lu events)\n",
           (unsigned long)count);
    if (argc == 5)
        found = logic_trace_find_last_writer(events, count, (uint8_t)logic,
                                             (uint8_t)field, &event);
    else
        found = find_latest_write(events, count, &event);
    if (found == 0) {
        fputs("last writer: ", stdout);
        print_event(&event);
    } else if (argc == 5) {
        fprintf(stderr, "no writer for logic %lu field %lu\n", logic, field);
        free(events);
        return 1;
    } else {
        puts("last writer: none");
    }
    free(events);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "recent") == 0)
        return command_recent(argc, argv);
    if (strcmp(argv[1], "last-writer") == 0)
        return command_last_writer(argc, argv);
    if (strcmp(argv[1], "check") == 0)
        return command_check(argc, argv);
    usage(argv[0]);
    return 2;
}
