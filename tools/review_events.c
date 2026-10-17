#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "review_events.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define EVENTS_MAX_BYTES (32u * 1024u * 1024u)
#define EVENTS_MAX_RECORDS 16384u
#define EVENTS_MAX_LINE 2048u
#define EVENTS_HEADER "review-events-v1\n"
#define EVENTS_ID_CAP REVIEW_EVENTS_IDENTIFIER_CAP
#define EVENTS_REVISION_CAP REVIEW_EVENTS_REVISION_CAP

typedef struct {
    uint64_t sequence;
    uint64_t expected_version;
    uint64_t version;
    char event_id[EVENTS_ID_CAP];
    char task_id[EVENTS_ID_CAP];
    char run_id[EVENTS_ID_CAP];
    char revision[EVENTS_REVISION_CAP];
    ReviewTaskState task_state;
    ReviewProcessState process_state;
} EventRecord;

typedef struct {
    EventRecord *records;
    size_t count;
    size_t capacity;
    size_t *event_slots;
    size_t *task_slots;
    size_t slot_count;
} EventLedger;

static void set_error(char *error, size_t capacity, const char *format, ...)
{
    if (!error || !capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, capacity, format, args);
    va_end(args);
}

static bool valid_identifier(const char *text)
{
    if (!text || !*text || strlen(text) >= EVENTS_ID_CAP) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < 0x20 || *at > 0x7e || *at == '|') return false;
    return true;
}

static bool valid_revision(const char *text)
{
    if (!text) return false;
    size_t length = strlen(text);
    if (length != 40 && length != 64) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

static bool parse_u64(const char *text, uint64_t *value)
{
    if (!text || !*text || (text[0] == '0' && text[1])) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < '0' || *at > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end || parsed > UINT64_MAX) return false;
    *value = (uint64_t)parsed;
    return true;
}

static const char *task_state_name(ReviewTaskState state)
{
    switch (state) {
    case REVIEW_TASK_QUEUED: return "queued";
    case REVIEW_TASK_RUNNING: return "running";
    case REVIEW_TASK_SUCCEEDED: return "succeeded";
    case REVIEW_TASK_FAILED: return "failed";
    case REVIEW_TASK_TIMED_OUT: return "timed_out";
    }
    return NULL;
}

static bool parse_task_state(const char *text, ReviewTaskState *state)
{
    static const char *const names[] = { "queued", "running", "succeeded", "failed", "timed_out" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!strcmp(text, names[i])) {
            *state = (ReviewTaskState)i;
            return true;
        }
    }
    return false;
}

static const char *process_state_name(ReviewProcessState state)
{
    switch (state) {
    case REVIEW_PROCESS_NOT_STARTED: return "not_started";
    case REVIEW_PROCESS_RUNNING: return "running";
    case REVIEW_PROCESS_UNKNOWN: return "unknown";
    case REVIEW_PROCESS_EXITED: return "exited";
    }
    return NULL;
}

static bool parse_process_state(const char *text, ReviewProcessState *state)
{
    static const char *const names[] = { "not_started", "running", "unknown", "exited" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!strcmp(text, names[i])) {
            *state = (ReviewProcessState)i;
            return true;
        }
    }
    return false;
}

static uint64_t identifier_hash(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at) {
        hash ^= *at;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static size_t *index_slot(EventLedger *ledger, bool by_task, const char *key)
{
    size_t *slots = by_task ? ledger->task_slots : ledger->event_slots;
    size_t position = (size_t)identifier_hash(key) & (ledger->slot_count - 1);
    for (size_t probed = 0; probed < ledger->slot_count; ++probed) {
        size_t *slot = &slots[position];
        if (!*slot) return slot;
        const EventRecord *record = &ledger->records[*slot - 1];
        const char *stored = by_task ? record->task_id : record->event_id;
        if (!strcmp(stored, key)) return slot;
        position = (position + 1) & (ledger->slot_count - 1);
    }
    return NULL;
}

static bool initialize_indexes(EventLedger *ledger, size_t capacity)
{
    size_t slots = 8;
    if (capacity > EVENTS_MAX_RECORDS) return false;
    while (slots < capacity * 2) {
        if (slots > SIZE_MAX / 2) return false;
        slots *= 2;
    }
    ledger->records = calloc(capacity ? capacity : 1, sizeof(*ledger->records));
    ledger->event_slots = calloc(slots, sizeof(*ledger->event_slots));
    ledger->task_slots = calloc(slots, sizeof(*ledger->task_slots));
    if (!ledger->records || !ledger->event_slots || !ledger->task_slots) return false;
    ledger->capacity = capacity;
    ledger->slot_count = slots;
    return true;
}

static void free_ledger(EventLedger *ledger)
{
    free(ledger->records);
    free(ledger->event_slots);
    free(ledger->task_slots);
    memset(ledger, 0, sizeof(*ledger));
}

static int split_fields(char *line, char **fields, size_t capacity)
{
    size_t count = 0;
    if (!*line) return -1;
    fields[count++] = line;
    for (char *at = line; *at; ++at) {
        if (*at != '|') continue;
        *at = '\0';
        if (count == capacity || !*fields[count - 1] || !at[1]) return -1;
        fields[count++] = at + 1;
    }
    if (!*fields[count - 1]) return -1;
    return (int)count;
}

static bool valid_transition(const EventRecord *previous, const EventRecord *next)
{
    if (!previous)
        return next->expected_version == 0 && next->task_state == REVIEW_TASK_QUEUED &&
               next->process_state == REVIEW_PROCESS_NOT_STARTED;
    if (previous->task_state == REVIEW_TASK_QUEUED &&
        previous->process_state == REVIEW_PROCESS_NOT_STARTED)
        return next->task_state == REVIEW_TASK_RUNNING && next->process_state == REVIEW_PROCESS_RUNNING;
    if (previous->task_state == REVIEW_TASK_RUNNING &&
        previous->process_state == REVIEW_PROCESS_RUNNING) {
        return (next->task_state == REVIEW_TASK_TIMED_OUT && next->process_state == REVIEW_PROCESS_UNKNOWN) ||
               (next->task_state == REVIEW_TASK_SUCCEEDED && next->process_state == REVIEW_PROCESS_EXITED) ||
               (next->task_state == REVIEW_TASK_FAILED && next->process_state == REVIEW_PROCESS_EXITED);
    }
    if (previous->task_state == REVIEW_TASK_TIMED_OUT &&
        previous->process_state == REVIEW_PROCESS_UNKNOWN)
        return next->task_state == REVIEW_TASK_TIMED_OUT && next->process_state == REVIEW_PROCESS_EXITED;
    return false;
}

static bool parse_record(char *line, EventRecord *record)
{
    char *fields[9];
    int count = split_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
    return count == 9 && !strcmp(fields[0], "event") &&
           parse_u64(fields[1], &record->sequence) && record->sequence > 0 &&
           valid_identifier(fields[2]) && valid_identifier(fields[3]) &&
           valid_identifier(fields[4]) && valid_revision(fields[5]) &&
           parse_u64(fields[6], &record->expected_version) && record->expected_version < UINT64_MAX &&
           parse_task_state(fields[7], &record->task_state) &&
           parse_process_state(fields[8], &record->process_state) &&
           snprintf(record->event_id, sizeof(record->event_id), "%s", fields[2]) < (int)sizeof(record->event_id) &&
           snprintf(record->task_id, sizeof(record->task_id), "%s", fields[3]) < (int)sizeof(record->task_id) &&
           snprintf(record->run_id, sizeof(record->run_id), "%s", fields[4]) < (int)sizeof(record->run_id) &&
           snprintf(record->revision, sizeof(record->revision), "%s", fields[5]) < (int)sizeof(record->revision);
}

static bool add_record(EventLedger *ledger, EventRecord *record)
{
    if (ledger->count >= ledger->capacity) return false;
    size_t *event = index_slot(ledger, false, record->event_id);
    size_t *task = index_slot(ledger, true, record->task_id);
    if (!event || !task || *event) return false;
    size_t index = ledger->count++;
    ledger->records[index] = *record;
    *event = index + 1;
    *task = index + 1;
    return true;
}

static bool parse_snapshot(char *data, size_t length, EventLedger *ledger,
                           char *error, size_t error_capacity)
{
    if (!length) return initialize_indexes(ledger, 1);
    if (memchr(data, '\0', length) || length < sizeof(EVENTS_HEADER) - 1 ||
        memcmp(data, EVENTS_HEADER, sizeof(EVENTS_HEADER) - 1)) {
        set_error(error, error_capacity, "review events ledger has an invalid header");
        return false;
    }
    size_t expected_records = 0;
    for (size_t i = sizeof(EVENTS_HEADER) - 1; i < length; ++i)
        if (data[i] == '\n') ++expected_records;
    if (expected_records > EVENTS_MAX_RECORDS) {
        set_error(error, error_capacity, "review events ledger has too many records");
        return false;
    }
    size_t capacity = expected_records < EVENTS_MAX_RECORDS ? expected_records + 1 : expected_records;
    if (!initialize_indexes(ledger, capacity)) {
        set_error(error, error_capacity, "cannot allocate review events ledger index");
        return false;
    }
    char *cursor = data + sizeof(EVENTS_HEADER) - 1;
    char *limit = data + length;
    while (cursor < limit) {
        char *newline = memchr(cursor, '\n', (size_t)(limit - cursor));
        if (!newline || newline == cursor || (size_t)(newline - cursor) > EVENTS_MAX_LINE) {
            set_error(error, error_capacity, "review events ledger contains a malformed record");
            return false;
        }
        *newline = '\0';
        EventRecord record = { 0 };
        if (!parse_record(cursor, &record) || record.sequence != ledger->count + 1) {
            set_error(error, error_capacity, "review events ledger contains a malformed or out-of-order event");
            return false;
        }
        size_t *prior_event = index_slot(ledger, false, record.event_id);
        size_t *prior_task = index_slot(ledger, true, record.task_id);
        if (!prior_event || !prior_task || *prior_event) {
            set_error(error, error_capacity, "review events ledger contains a duplicate event id");
            return false;
        }
        const EventRecord *previous = *prior_task ? &ledger->records[*prior_task - 1] : NULL;
        if ((previous && (strcmp(previous->run_id, record.run_id) ||
                          strcmp(previous->revision, record.revision) ||
                          previous->version != record.expected_version)) ||
            !valid_transition(previous, &record)) {
            set_error(error, error_capacity, "review events ledger contains a stale or invalid task transition");
            return false;
        }
        record.version = record.expected_version + 1;
        if (!add_record(ledger, &record)) {
            set_error(error, error_capacity, "review events ledger index is full");
            return false;
        }
        cursor = newline + 1;
    }
    return true;
}

static bool read_ledger(int descriptor, bool repair_tail, char **data_out, size_t *length_out,
                        char *error, size_t error_capacity)
{
    struct stat info;
    if (fstat(descriptor, &info) || !S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_size < 0 ||
        (uintmax_t)info.st_size > EVENTS_MAX_BYTES) {
        set_error(error, error_capacity, "review events ledger is unsafe or too large");
        return false;
    }
    size_t length = (size_t)info.st_size;
    char *data = malloc(length + 1);
    if (!data) {
        set_error(error, error_capacity, "cannot allocate review events ledger input");
        return false;
    }
    size_t used = 0;
    while (used < length) {
        ssize_t amount = pread(descriptor, data + used, length - used, (off_t)used);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            free(data);
            set_error(error, error_capacity, "cannot read review events ledger");
            return false;
        }
        used += (size_t)amount;
    }
    if (length && data[length - 1] != '\n') {
        if (!repair_tail) {
            free(data);
            set_error(error, error_capacity, "review events ledger has an interrupted final append");
            return false;
        }
        size_t boundary = length;
        while (boundary && data[boundary - 1] != '\n') --boundary;
        if (!boundary && length < sizeof(EVENTS_HEADER) - 1 &&
            !memcmp(data, EVENTS_HEADER, length)) boundary = 0;
        else if (!boundary) {
            free(data);
            set_error(error, error_capacity, "review events ledger has an unrecoverable partial header");
            return false;
        }
        if (ftruncate(descriptor, (off_t)boundary) || fsync(descriptor)) {
            free(data);
            set_error(error, error_capacity, "cannot recover interrupted review events append");
            return false;
        }
        length = boundary;
    }
    data[length] = '\0';
    *data_out = data;
    *length_out = length;
    return true;
}

static int open_ledger(const char *path, bool write, int *descriptor,
                       char *error, size_t error_capacity)
{
    if (!path || !*path) {
        set_error(error, error_capacity, "review events ledger path is required");
        return -1;
    }
    int flags = (write ? O_RDWR | O_CREAT : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW;
    int fd = open(path, flags, 0600);
    if (fd < 0) {
        set_error(error, error_capacity, "cannot open review events ledger: %s", strerror(errno));
        return -1;
    }
    struct stat info;
    if (fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
        (write && fchmod(fd, 0600))) {
        int saved_errno = errno;
        close(fd);
        set_error(error, error_capacity, "review events ledger must be a private regular file%s%s",
                  saved_errno ? ": " : "", saved_errno ? strerror(saved_errno) : "");
        return -1;
    }
    if (flock(fd, write ? LOCK_EX : LOCK_SH)) {
        int saved_errno = errno;
        close(fd);
        set_error(error, error_capacity, "cannot lock review events ledger: %s", strerror(saved_errno));
        return -1;
    }
    *descriptor = fd;
    return 0;
}

static void close_ledger(int descriptor)
{
    (void)flock(descriptor, LOCK_UN);
    (void)close(descriptor);
}

static bool event_matches(const EventRecord *record, const ReviewEventsUpdate *update)
{
    return !strcmp(record->event_id, update->event_id) &&
           !strcmp(record->task_id, update->task_id) &&
           !strcmp(record->run_id, update->run_id) &&
           !strcmp(record->revision, update->revision) &&
           record->expected_version == update->expected_version &&
           record->task_state == update->task_state && record->process_state == update->process_state;
}

static bool update_is_valid(const ReviewEventsUpdate *update)
{
    return update && valid_identifier(update->event_id) && valid_identifier(update->task_id) &&
           valid_identifier(update->run_id) && valid_revision(update->revision) &&
           update->expected_version < UINT64_MAX && task_state_name(update->task_state) &&
           process_state_name(update->process_state);
}

static bool sync_parent_directory(const char *path)
{
    char parent[4096];
    size_t length = strlen(path);
    if (length >= sizeof(parent)) return false;
    memcpy(parent, path, length + 1);
    char *slash = strrchr(parent, '/');
    if (!slash) strcpy(parent, ".");
    else if (slash == parent) slash[1] = '\0';
    else *slash = '\0';
    int directory = open(parent, O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (directory < 0) return false;
    bool ok = fsync(directory) == 0;
    close(directory);
    return ok;
}

static bool write_all(int descriptor, const char *data, size_t length)
{
    size_t written = 0;
    while (written < length) {
        ssize_t amount = write(descriptor, data + written, length - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) return false;
        written += (size_t)amount;
    }
    return true;
}

int review_events_record(const char *ledger_path, const ReviewEventsUpdate *update,
                         uint64_t *version, char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!update_is_valid(update)) {
        set_error(error, error_capacity, "review event identity or state is invalid");
        return REVIEW_EVENTS_ERROR;
    }
    int descriptor;
    if (open_ledger(ledger_path, true, &descriptor, error, error_capacity)) return REVIEW_EVENTS_ERROR;
    char *data = NULL;
    size_t length = 0;
    EventLedger ledger = { 0 };
    bool ok = read_ledger(descriptor, true, &data, &length, error, error_capacity) &&
              parse_snapshot(data, length, &ledger, error, error_capacity);
    if (!ok) {
        free(data);
        free_ledger(&ledger);
        close_ledger(descriptor);
        return REVIEW_EVENTS_ERROR;
    }

    size_t *prior_event_slot = index_slot(&ledger, false, update->event_id);
    if (!prior_event_slot) {
        set_error(error, error_capacity, "review events ledger index is full");
        ok = false;
    } else if (*prior_event_slot) {
        const EventRecord *prior = &ledger.records[*prior_event_slot - 1];
        if (!event_matches(prior, update)) {
            set_error(error, error_capacity, "event id was already used for a different update");
            ok = false;
        } else if (!sync_parent_directory(ledger_path)) {
            set_error(error, error_capacity,
                      "review event was written but its ledger directory could not be synced; retry the same event id");
            ok = false;
        } else if (version) {
            *version = prior->version;
        }
        int result = ok ? REVIEW_EVENTS_DUPLICATE : REVIEW_EVENTS_ERROR;
        free(data);
        free_ledger(&ledger);
        close_ledger(descriptor);
        return result;
    }

    size_t *task_slot = index_slot(&ledger, true, update->task_id);
    if (!task_slot) {
        set_error(error, error_capacity, "review events ledger index is full");
        ok = false;
    }
    const EventRecord *previous = ok && *task_slot ? &ledger.records[*task_slot - 1] : NULL;
    if (ok && previous && (strcmp(previous->run_id, update->run_id) ||
                           strcmp(previous->revision, update->revision))) {
        set_error(error, error_capacity, "task update is bound to a stale run or revision");
        ok = false;
    }
    if (ok && previous && update->expected_version != previous->version) {
        set_error(error, error_capacity, "stale task version: expected %" PRIu64 ", current is %" PRIu64,
                  update->expected_version, previous->version);
        ok = false;
    }
    EventRecord next = { 0 };
    if (ok) {
        next.expected_version = update->expected_version;
        next.version = update->expected_version + 1;
        next.sequence = ledger.count + 1;
        next.task_state = update->task_state;
        next.process_state = update->process_state;
        strcpy(next.event_id, update->event_id);
        strcpy(next.task_id, update->task_id);
        strcpy(next.run_id, update->run_id);
        strcpy(next.revision, update->revision);
        if (!valid_transition(previous, &next)) {
            bool redundant = previous && previous->task_state == next.task_state &&
                             previous->process_state == next.process_state;
            set_error(error, error_capacity, redundant ? "duplicate task state update" :
                      "invalid task or process state transition");
            ok = false;
        }
    }
    if (ok && (ledger.count >= EVENTS_MAX_RECORDS || ledger.count >= ledger.capacity)) {
        set_error(error, error_capacity, "review events ledger has reached its record limit");
        ok = false;
    }
    if (ok && length > EVENTS_MAX_BYTES - EVENTS_MAX_LINE) {
        set_error(error, error_capacity, "review events ledger has reached its size limit");
        ok = false;
    }

    char row[EVENTS_MAX_LINE + 1];
    int row_length = 0;
    if (ok) {
        row_length = snprintf(row, sizeof(row), "event|%" PRIu64 "|%s|%s|%s|%s|%" PRIu64 "|%s|%s\n",
                              next.sequence, next.event_id, next.task_id, next.run_id, next.revision,
                              next.expected_version, task_state_name(next.task_state),
                              process_state_name(next.process_state));
        if (row_length < 0 || row_length >= (int)sizeof(row)) {
            set_error(error, error_capacity, "review event record is too long");
            ok = false;
        }
    }
    if (ok) {
        off_t original_size = (off_t)length;
        bool first_record = !length;
        if (lseek(descriptor, 0, SEEK_END) != original_size ||
            (first_record && !write_all(descriptor, EVENTS_HEADER, sizeof(EVENTS_HEADER) - 1)) ||
            !write_all(descriptor, row, (size_t)row_length) || fsync(descriptor)) {
            int saved_errno = errno;
            bool rolled_back = ftruncate(descriptor, original_size) == 0 && fsync(descriptor) == 0;
            set_error(error, error_capacity, rolled_back ? "cannot append durable review event: %s" :
                      "review event append durability is uncertain; retry the same event id: %s",
                      strerror(saved_errno));
            ok = false;
        } else if (first_record && !sync_parent_directory(ledger_path)) {
            set_error(error, error_capacity,
                      "review event was written but its ledger directory could not be synced; retry the same event id");
            ok = false;
        }
    }
    if (ok && version) *version = next.version;
    free(data);
    free_ledger(&ledger);
    close_ledger(descriptor);
    return ok ? REVIEW_EVENTS_APPLIED : REVIEW_EVENTS_ERROR;
}

int review_events_get(const char *ledger_path, const char *task_id, ReviewEventsTask *task,
                      char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!valid_identifier(task_id) || !task) {
        set_error(error, error_capacity, "review task id and result are required");
        return REVIEW_EVENTS_ERROR;
    }
    int descriptor;
    if (open_ledger(ledger_path, false, &descriptor, error, error_capacity)) return REVIEW_EVENTS_ERROR;
    char *data = NULL;
    size_t length = 0;
    EventLedger ledger = { 0 };
    bool ok = read_ledger(descriptor, false, &data, &length, error, error_capacity) &&
              parse_snapshot(data, length, &ledger, error, error_capacity);
    if (ok) {
        size_t *slot = index_slot(&ledger, true, task_id);
        if (!slot || !*slot) {
            set_error(error, error_capacity, "review task was not found in ledger");
            ok = false;
        } else {
            const EventRecord *record = &ledger.records[*slot - 1];
            memset(task, 0, sizeof(*task));
            strcpy(task->task_id, record->task_id);
            strcpy(task->run_id, record->run_id);
            strcpy(task->revision, record->revision);
            strcpy(task->last_event_id, record->event_id);
            task->version = record->version;
            task->sequence = record->sequence;
            task->task_state = record->task_state;
            task->process_state = record->process_state;
        }
    }
    free(data);
    free_ledger(&ledger);
    close_ledger(descriptor);
    return ok ? 0 : REVIEW_EVENTS_ERROR;
}

#ifndef REVIEW_EVENTS_NO_MAIN
static void print_usage(FILE *stream)
{
    fprintf(stream,
        "usage:\n"
        "  review-events record --ledger PATH --event ID --task ID --run ID --revision SHA "
        "--expected-version N --task-state STATE --process-state STATE\n"
        "  review-events get --ledger PATH --task ID\n"
        "The ledger is editable local provenance, not trusted chronology or approval authority.\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }
    if (!strcmp(argv[1], "get")) {
        const char *ledger = NULL, *task_id = NULL;
        bool seen_ledger = false, seen_task = false;
        for (int i = 2; i < argc; ++i) {
            if (i + 1 >= argc) {
                fprintf(stderr, "review-events: missing option value\n");
                return EXIT_FAILURE;
            }
            const char *option = argv[i++], *value = argv[i];
            if (!strcmp(option, "--ledger")) {
                if (seen_ledger) { fprintf(stderr, "review-events: duplicate --ledger\n"); return EXIT_FAILURE; }
                seen_ledger = true;
                ledger = value;
            } else if (!strcmp(option, "--task")) {
                if (seen_task) { fprintf(stderr, "review-events: duplicate --task\n"); return EXIT_FAILURE; }
                seen_task = true;
                task_id = value;
            } else {
                fprintf(stderr, "review-events: unknown option %s\n", option);
                return EXIT_FAILURE;
            }
        }
        if (!seen_ledger || !seen_task) {
            fprintf(stderr, "review-events: get requires --ledger and --task\n");
            return EXIT_FAILURE;
        }
        ReviewEventsTask task;
        char error[512];
        if (review_events_get(ledger, task_id, &task, error, sizeof(error))) {
            fprintf(stderr, "review-events: %s\n", error[0] ? error : "cannot read task state");
            return EXIT_FAILURE;
        }
        printf("review-events-task-v1\n"
               "task_id|%s\nrun_id|%s\nrevision|%s\nlast_event_id|%s\n"
               "version|%" PRIu64 "\nsequence|%" PRIu64 "\ntask_state|%s\nprocess_state|%s\n",
               task.task_id, task.run_id, task.revision, task.last_event_id, task.version,
               task.sequence, task_state_name(task.task_state), process_state_name(task.process_state));
        return EXIT_SUCCESS;
    }
    if (!strcmp(argv[1], "record")) {
        const char *ledger = NULL, *event_id = NULL, *task_id = NULL, *run_id = NULL;
        const char *revision = NULL, *task_state_text = NULL, *process_state_text = NULL;
        uint64_t expected_version = 0;
        bool seen_ledger = false, seen_event = false, seen_task = false, seen_run = false;
        bool seen_revision = false, seen_version = false, seen_task_state = false, seen_process_state = false;
        for (int i = 2; i < argc; ++i) {
            if (i + 1 >= argc) {
                fprintf(stderr, "review-events: missing option value\n");
                return EXIT_FAILURE;
            }
            const char *option = argv[i++], *value = argv[i];
            bool matched = false;
#define EVENT_OPTION(name, flag, destination) do { \
    if (!strcmp(option, name)) { \
        if (flag) { fprintf(stderr, "review-events: duplicate %s\n", name); return EXIT_FAILURE; } \
        flag = true; destination = value; matched = true; \
    } \
} while (0)
            EVENT_OPTION("--ledger", seen_ledger, ledger);
            EVENT_OPTION("--event", seen_event, event_id);
            EVENT_OPTION("--task", seen_task, task_id);
            EVENT_OPTION("--run", seen_run, run_id);
            EVENT_OPTION("--revision", seen_revision, revision);
            EVENT_OPTION("--task-state", seen_task_state, task_state_text);
            EVENT_OPTION("--process-state", seen_process_state, process_state_text);
#undef EVENT_OPTION
            if (matched) continue;
            if (!strcmp(option, "--expected-version")) {
                if (seen_version || !parse_u64(value, &expected_version)) {
                    fprintf(stderr, "review-events: expected-version must be a valid nonnegative integer\n");
                    return EXIT_FAILURE;
                }
                seen_version = true;
            } else {
                fprintf(stderr, "review-events: unknown option %s\n", option);
                return EXIT_FAILURE;
            }
        }
        if (!seen_ledger || !seen_event || !seen_task || !seen_run || !seen_revision || !seen_version ||
            !seen_task_state || !seen_process_state) {
            fprintf(stderr, "review-events: record requires ledger, event, task, run, revision, version, and both states\n");
            return EXIT_FAILURE;
        }
        ReviewEventsUpdate update = {
            .event_id = event_id,
            .task_id = task_id,
            .run_id = run_id,
            .revision = revision,
            .expected_version = expected_version
        };
        if (!parse_task_state(task_state_text, &update.task_state) ||
            !parse_process_state(process_state_text, &update.process_state)) {
            fprintf(stderr, "review-events: task-state or process-state is invalid\n");
            return EXIT_FAILURE;
        }
        char error[512];
        uint64_t version = 0;
        int status = review_events_record(ledger, &update, &version, error, sizeof(error));
        if (status == REVIEW_EVENTS_ERROR) {
            fprintf(stderr, "review-events: %s\n", error[0] ? error : "cannot record event");
            return EXIT_FAILURE;
        }
        printf("review-events-result-v1\nresult|%s\nversion|%" PRIu64 "\n",
               status == REVIEW_EVENTS_DUPLICATE ? "duplicate" : "applied", version);
        return EXIT_SUCCESS;
    }
    print_usage(stderr);
    return EXIT_FAILURE;
}
#endif
