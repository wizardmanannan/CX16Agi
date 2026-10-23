#define _POSIX_C_SOURCE 200809L
#include "review_events.h"
#include "test_paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char ledger[4096];
static const char revision_a[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char revision_b[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
static const char *events_cli;

static int record_event(const char *event_id, const char *task_id, const char *run_id,
                        const char *revision, uint64_t expected, ReviewTaskState task_state,
                        ReviewProcessState process_state, uint64_t *version, char *error,
                        size_t error_capacity)
{
    ReviewEventsUpdate update = {
        .event_id = event_id,
        .task_id = task_id,
        .run_id = run_id,
        .revision = revision,
        .expected_version = expected,
        .task_state = task_state,
        .process_state = process_state
    };
    return review_events_record(ledger, &update, version, error, error_capacity);
}

static void read_task(const char *task_id, ReviewEventsTask *task)
{
    char error[256] = { 0 };
    CHECK(review_events_get(ledger, task_id, task, error, sizeof(error)) == 0,
          "Cannot read review event task state");
}

static int concurrent_update(const char *event_id, ReviewTaskState state, ReviewProcessState process)
{
    char error[256] = { 0 };
    return record_event(event_id, "race-task", "race-run", revision_a, 2, state, process,
                        NULL, error, sizeof(error));
}

static void test_concurrent_compare_and_append(void)
{
    uint64_t version;
    char error[256] = { 0 };
    CHECK(record_event("race-queued", "race-task", "race-run", revision_a, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 1,
          "Cannot queue concurrent ledger task");
    CHECK(record_event("race-started", "race-task", "race-run", revision_a, 1,
                       REVIEW_TASK_RUNNING, REVIEW_PROCESS_RUNNING,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 2,
          "Cannot start concurrent ledger task");

    pid_t first = fork();
    CHECK(first >= 0, "Cannot fork first concurrent ledger writer");
    if (!first) _Exit(concurrent_update("race-timeout", REVIEW_TASK_TIMED_OUT,
                                         REVIEW_PROCESS_UNKNOWN) == REVIEW_EVENTS_APPLIED ? 0 : 1);
    pid_t second = fork();
    CHECK(second >= 0, "Cannot fork second concurrent ledger writer");
    if (!second) _Exit(concurrent_update("race-failed", REVIEW_TASK_FAILED,
                                          REVIEW_PROCESS_EXITED) == REVIEW_EVENTS_APPLIED ? 0 : 1);
    int first_status, second_status;
    CHECK(waitpid(first, &first_status, 0) == first && waitpid(second, &second_status, 0) == second,
          "Cannot wait for concurrent ledger writers");
    int winners = WIFEXITED(first_status) && !WEXITSTATUS(first_status);
    winners += WIFEXITED(second_status) && !WEXITSTATUS(second_status);
    CHECK(winners == 1, "Concurrent stale ledger updates were not serialized");
    ReviewEventsTask task;
    read_task("race-task", &task);
    CHECK(task.version == 3 &&
          ((task.task_state == REVIEW_TASK_TIMED_OUT && task.process_state == REVIEW_PROCESS_UNKNOWN) ||
           (task.task_state == REVIEW_TASK_FAILED && task.process_state == REVIEW_PROCESS_EXITED)),
          "Concurrent ledger winner was not durably recorded as one complete event");
}

static int run_cli(char *const args[], char *output, size_t capacity)
{
    int descriptors[2];
    CHECK(!pipe(descriptors), "Cannot create review events CLI output pipe");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork review events CLI");
    if (!child) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0 || dup2(descriptors[1], STDERR_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        execv(events_cli, args);
        _Exit(127);
    }
    close(descriptors[1]);
    size_t used = 0;
    while (used + 1 < capacity) {
        ssize_t amount = read(descriptors[0], output + used, capacity - used - 1);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) break;
        used += (size_t)amount;
    }
    output[used] = '\0';
    CHECK(!close(descriptors[0]), "Cannot close review events CLI output pipe");
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for review events CLI");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static void test_live_process_timeout_lifecycle(void)
{
    int started[2], release[2];
    CHECK(!pipe(started) && !pipe(release), "Cannot create live process lifecycle pipes");
    pid_t process = fork();
    CHECK(process >= 0, "Cannot fork live review task process");
    if (!process) {
        close(started[0]);
        close(release[1]);
        char ready = 'R';
        if (write(started[1], &ready, 1) != 1) _Exit(2);
        close(started[1]);
        char finish;
        ssize_t amount;
        do { amount = read(release[0], &finish, 1); } while (amount < 0 && errno == EINTR);
        close(release[0]);
        _Exit(amount == 1 && finish == 'X' ? 0 : 3);
    }
    close(started[1]);
    close(release[0]);
    char ready = 0;
    CHECK(read(started[0], &ready, 1) == 1 && ready == 'R', "Live review task did not start");
    close(started[0]);

    char error[256] = { 0 };
    uint64_t version = 0;
    int queued = record_event("live-queued", "live-task", "live-run", revision_a, 0,
                              REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                              &version, error, sizeof(error));
    int running = queued == REVIEW_EVENTS_APPLIED && record_event(
        "live-started", "live-task", "live-run", revision_a, 1,
        REVIEW_TASK_RUNNING, REVIEW_PROCESS_RUNNING, &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED;
    int timed_out = running && record_event("live-timeout", "live-task", "live-run", revision_a, 2,
        REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_UNKNOWN, &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED;
    ReviewEventsTask task = { 0 };
    int read_ok = !review_events_get(ledger, "live-task", &task, error, sizeof(error));
    int status = 0;
    pid_t observed = waitpid(process, &status, WNOHANG);

    char finish = 'X';
    ssize_t released;
    do { released = write(release[1], &finish, 1); } while (released < 0 && errno == EINTR);
    close(release[1]);
    int child_status = 0;
    CHECK(waitpid(process, &child_status, 0) == process, "Cannot collect live review task process");
    int exited = WIFEXITED(child_status) && !WEXITSTATUS(child_status);
    int reaped = timed_out && exited && record_event("live-exited", "live-task", "live-run", revision_a, 3,
        REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_EXITED, &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED;
    read_ok = read_ok && !review_events_get(ledger, "live-task", &task, error, sizeof(error));

    CHECK(released == 1 && observed == 0 && queued == REVIEW_EVENTS_APPLIED && running && timed_out,
          "Live task lifecycle did not reach timeout while its process remained alive");
    CHECK(read_ok && task.task_state == REVIEW_TASK_TIMED_OUT && task.process_state == REVIEW_PROCESS_EXITED &&
          task.version == 4 && reaped,
          "Live task timeout and later observed process exit were not recorded separately");
}

static void test_events_cli(void)
{
    char output[1024];
    char *record[] = { (char *)events_cli, (char *)"record", (char *)"--ledger", ledger,
        (char *)"--event", (char *)"cli-queued", (char *)"--task", (char *)"cli-task",
        (char *)"--run", (char *)"cli-run", (char *)"--revision", (char *)revision_b,
        (char *)"--expected-version", (char *)"0", (char *)"--task-state", (char *)"queued",
        (char *)"--process-state", (char *)"not_started", NULL };
    CHECK(run_cli(record, output, sizeof(output)) == 0 && strstr(output, "result|applied") &&
          strstr(output, "version|1"), "Review events CLI did not report its applied ledger version");
    CHECK(run_cli(record, output, sizeof(output)) == 0 && strstr(output, "result|duplicate") &&
          strstr(output, "version|1"), "Review events CLI did not report an idempotent replay");
    char *get[] = { (char *)events_cli, (char *)"get", (char *)"--ledger", ledger,
        (char *)"--task", (char *)"cli-task", NULL };
    CHECK(run_cli(get, output, sizeof(output)) == 0 && strstr(output, "task_id|cli-task") &&
          strstr(output, "run_id|cli-run") && strstr(output, "revision|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") &&
          strstr(output, "version|1") && strstr(output, "task_state|queued") &&
          strstr(output, "process_state|not_started"),
          "Review events CLI did not publish the task's revision-bound state");
}

static void test_orchestration_identifier_shape(void)
{
    static const char task_id[] =
        "node:delegated-task:command%3Amcp%3A77ad7755-9eeb-4d01-830c-a2c8e2d2e0e7"
        "%3Adelegate-task%3Aquality-core-complete-r3-20261018";
    static const char run_id[] =
        "thread:node%3Adelegated-task%3A77ad7755-9eeb-4d01-830c-a2c8e2d2e0e7"
        "%3Aturn%3A20261018T103000Z%3Arun-0007";
    static const char queued_event[] =
        "node:event%3Acommand%3Amcp%3A77ad7755-9eeb-4d01-830c-a2c8e2d2e0e7%3Aqueued";
    static const char running_event[] =
        "node:event%3Acommand%3Amcp%3A77ad7755-9eeb-4d01-830c-a2c8e2d2e0e7%3Arunning";
    uint64_t version = 0;
    char error[256] = { 0 };
    CHECK(record_event(queued_event, task_id, run_id, revision_a, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 1,
          "Review events rejected opaque orchestration task, run, or event identifiers");
    CHECK(record_event(running_event, task_id, run_id, revision_a, 1,
                       REVIEW_TASK_RUNNING, REVIEW_PROCESS_RUNNING,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 2,
          "Review events could not advance a task with encoded orchestration identifiers");
    ReviewEventsTask task;
    read_task(task_id, &task);
    CHECK(!strcmp(task.task_id, task_id) && !strcmp(task.run_id, run_id) &&
          !strcmp(task.last_event_id, running_event) && task.version == 2 &&
          task.task_state == REVIEW_TASK_RUNNING,
          "Review events did not preserve the complete orchestration identity in its task state");
}

static void test_parent_directory_durability_retry(void)
{
    char directory[4096], path[4096];
    suite_test_path(directory, sizeof(directory), suite_fixture_directory, "events-durability-parent");
    CHECK(!mkdir(directory, 0700), "Cannot create event durability fixture directory");
    suite_test_path(path, sizeof(path), directory, "ledger");
    CHECK(!chmod(directory, 0300), "Cannot remove read permission from event durability fixture");

    ReviewEventsUpdate update = {
        .event_id = "durability-event",
        .task_id = "durability-task",
        .run_id = "durability-run",
        .revision = revision_a,
        .expected_version = 0,
        .task_state = REVIEW_TASK_QUEUED,
        .process_state = REVIEW_PROCESS_NOT_STARTED
    };
    char first_error[256] = { 0 }, retry_error[256] = { 0 };
    uint64_t version = 0;
    int first = review_events_record(path, &update, &version, first_error, sizeof(first_error));
    int retry = review_events_record(path, &update, &version, retry_error, sizeof(retry_error));

    int restored = chmod(directory, 0700);
    CHECK(!restored, "Cannot restore event durability fixture directory permissions");
    CHECK(first == REVIEW_EVENTS_ERROR && strstr(first_error, "ledger directory could not be synced"),
          "Event append did not report a parent-directory sync failure");
    CHECK(retry == REVIEW_EVENTS_ERROR && strstr(retry_error, "ledger directory could not be synced"),
          "Idempotent retry hid the unresolved parent-directory durability failure");
    CHECK(review_events_record(path, &update, &version, retry_error, sizeof(retry_error)) ==
          REVIEW_EVENTS_DUPLICATE && version == 1,
          "Event retry did not recover idempotently after parent-directory sync became available");
}

int main(int argc, char **argv)
{
    CHECK(argc == 2, "Expected the review events CLI executable path");
    events_cli = argv[1];
    suite_test_paths_init("review-events");
    suite_test_path(ledger, sizeof(ledger), suite_fixture_directory, "review-events.ledger");

    uint64_t version = 0;
    char error[256] = { 0 };
    CHECK(record_event("event-queued", "task-1", "run-1", revision_a, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 1,
          "Review events did not durably create a task");
    CHECK(record_event("event-queued", "task-1", "run-1", revision_a, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_DUPLICATE && version == 1,
          "Review events did not treat an exact replay as idempotent");
    CHECK(record_event("event-queued", "task-1", "run-1", revision_b, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_ERROR,
          "Review events accepted an event id reused for another revision");

    CHECK(record_event("event-started", "task-1", "run-1", revision_a, 1,
                       REVIEW_TASK_RUNNING, REVIEW_PROCESS_RUNNING,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 2,
          "Review events did not start the task");
    CHECK(record_event("event-timeout", "task-1", "run-1", revision_a, 2,
                       REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_UNKNOWN,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 3,
          "Review events did not record task timeout");

    ReviewEventsTask task;
    read_task("task-1", &task);
    CHECK(task.task_state == REVIEW_TASK_TIMED_OUT && task.process_state == REVIEW_PROCESS_UNKNOWN,
          "A task timeout was treated as proof that its process died");
    CHECK(record_event("stale-result", "task-1", "run-1", revision_a, 2,
                       REVIEW_TASK_FAILED, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_ERROR,
          "Review events accepted a stale task version");
    CHECK(strstr(error, "stale") != NULL, "Review events returned the wrong stale-version error");
    CHECK(record_event("stale-revision", "task-1", "run-1", revision_b, 3,
                       REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_ERROR,
          "Review events accepted an update for a different revision");
    CHECK(record_event("timeout-is-not-success", "task-1", "run-1", revision_a, 3,
                       REVIEW_TASK_SUCCEEDED, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_ERROR,
          "Review events treated timeout as successful process completion");
    CHECK(record_event("timeout-reaped", "task-1", "run-1", revision_a, 3,
                       REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED && version == 4,
          "Review events could not record a later observed process exit");
    read_task("task-1", &task);
    CHECK(task.task_state == REVIEW_TASK_TIMED_OUT && task.process_state == REVIEW_PROCESS_EXITED,
          "A later process exit changed the recorded timeout result");
    CHECK(record_event("duplicate-state", "task-1", "run-1", revision_a, 4,
                       REVIEW_TASK_TIMED_OUT, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_ERROR,
          "Review events accepted a duplicate state update with a new event id");

    CHECK(record_event("task-2-queued", "task-2", "run-2", revision_b, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED,
          "Review events did not accept an independent task and revision");
    CHECK(record_event("task-2-started", "task-2", "run-2", revision_b, 1,
                       REVIEW_TASK_RUNNING, REVIEW_PROCESS_RUNNING,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED,
          "Review events did not start an independent run");
    CHECK(record_event("task-2-passed", "task-2", "run-2", revision_b, 2,
                       REVIEW_TASK_SUCCEEDED, REVIEW_PROCESS_EXITED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED,
          "Review events did not record an observed successful process exit");
    read_task("task-2", &task);
    CHECK(task.task_state == REVIEW_TASK_SUCCEEDED && task.process_state == REVIEW_PROCESS_EXITED,
          "Review events did not persist task, run, and revision identity");

    test_live_process_timeout_lifecycle();
    test_concurrent_compare_and_append();
    test_events_cli();
    test_orchestration_identifier_shape();
    test_parent_directory_durability_retry();

    int descriptor = open(ledger, O_WRONLY | O_APPEND);
    CHECK(descriptor >= 0, "Cannot open review events ledger for crash-tail fixture");
    CHECK(write(descriptor, "partial-record", 14) == 14 && !fsync(descriptor) && !close(descriptor),
          "Cannot create interrupted review events append fixture");
    CHECK(record_event("task-3-queued", "task-3", "run-3", revision_b, 0,
                       REVIEW_TASK_QUEUED, REVIEW_PROCESS_NOT_STARTED,
                       &version, error, sizeof(error)) == REVIEW_EVENTS_APPLIED,
          "Review events did not recover an interrupted final append");
    read_task("task-3", &task);
    CHECK(task.task_state == REVIEW_TASK_QUEUED && task.version == 1,
          "Review events did not preserve a complete event after crash-tail recovery");

    puts("PASS review event ledger is durable, idempotent, versioned, revision-bound, and timeout-safe");
    return EXIT_SUCCESS;
}
