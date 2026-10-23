#ifndef CX16AGI_REVIEW_EVENTS_H
#define CX16AGI_REVIEW_EVENTS_H

#include <stdint.h>
#include <stddef.h>

#define REVIEW_EVENTS_IDENTIFIER_CAP 512u
#define REVIEW_EVENTS_REVISION_CAP 65u

typedef enum {
    REVIEW_TASK_QUEUED,
    REVIEW_TASK_RUNNING,
    REVIEW_TASK_SUCCEEDED,
    REVIEW_TASK_FAILED,
    REVIEW_TASK_TIMED_OUT
} ReviewTaskState;

typedef enum {
    REVIEW_PROCESS_NOT_STARTED,
    REVIEW_PROCESS_RUNNING,
    REVIEW_PROCESS_UNKNOWN,
    REVIEW_PROCESS_EXITED
} ReviewProcessState;

typedef struct {
    const char *event_id;
    const char *task_id;
    const char *run_id;
    const char *revision;
    uint64_t expected_version;
    ReviewTaskState task_state;
    ReviewProcessState process_state;
} ReviewEventsUpdate;

typedef struct {
    char task_id[REVIEW_EVENTS_IDENTIFIER_CAP];
    char run_id[REVIEW_EVENTS_IDENTIFIER_CAP];
    char revision[REVIEW_EVENTS_REVISION_CAP];
    char last_event_id[REVIEW_EVENTS_IDENTIFIER_CAP];
    uint64_t version;
    uint64_t sequence;
    ReviewTaskState task_state;
    ReviewProcessState process_state;
} ReviewEventsTask;

enum {
    REVIEW_EVENTS_ERROR = -1,
    REVIEW_EVENTS_APPLIED = 0,
    REVIEW_EVENTS_DUPLICATE = 1
};

/*
 * Appends one durable, version-checked event. Replaying its event_id is idempotent.
 * This local ledger is editable provenance; its order is not trusted chronology or approval.
 */
int review_events_record(const char *ledger_path, const ReviewEventsUpdate *update,
                         uint64_t *version, char *error, size_t error_capacity);

/* Reads the latest task state; a timed-out task reports process UNKNOWN until exit is observed. */
int review_events_get(const char *ledger_path, const char *task_id, ReviewEventsTask *task,
                      char *error, size_t error_capacity);

#endif
