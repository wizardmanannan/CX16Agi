#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_paths.h"
#include "test_run_from.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static const char child_marker[] = "RUN-FROM-CHILD-UNIQUE-DIAGNOSTIC-8f71c0";
static const char early_marker[] = "RUN-FROM-TAIL-EARLY-MARKER-c2401b";
static const char end_marker[] = "RUN-FROM-TAIL-END-MARKER-70af39";
static pid_t unrelated_process = -1;

static bool fixture_root_is_empty(void)
{
    DIR *directory = opendir(suite_fixture_directory);
    if (!directory) return false;
    bool empty = true;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            empty = false;
            break;
        }
    }
    if (closedir(directory)) empty = false;
    return empty;
}

static int runner_child_main(const char *executable, const char *mode)
{
    suite_test_paths_init("run-from-child");
    char log[4096];
    suite_test_path(log, sizeof(log), suite_fixture_directory, "run.log");
    const char *child_mode;
    int timeout = 5;
    if (!strcmp(mode, "exit")) child_mode = "--emit-failure";
    else if (!strcmp(mode, "signal")) child_mode = "--emit-signal";
    else if (!strcmp(mode, "large")) child_mode = "--emit-large-failure";
    else if (!strcmp(mode, "cwd")) child_mode = "--emit-cwd";
    else if (!strcmp(mode, "timeout")) {
        child_mode = "--emit-timeout";
        timeout = 1;
    } else {
        fprintf(stderr, "Unknown runner child mode: %s\n", mode);
        return EXIT_FAILURE;
    }
    char *child_argv[] = { (char *)executable, (char *)child_mode, NULL };
    run_from(child_argv, log, timeout, suite_fixture_directory);

    if (!strcmp(mode, "cwd")) {
        FILE *file = fopen(log, "rb");
        CHECK(file, "Cannot read child working-directory witness");
        char actual[4096];
        CHECK(fgets(actual, sizeof(actual), file), "Missing child working-directory witness");
        CHECK(!fclose(file), "Cannot close child working-directory witness");
        actual[strcspn(actual, "\r\n")] = '\0';
        CHECK(!strcmp(actual, suite_fixture_directory), "Runner child did not use its requested working directory");
        return EXIT_SUCCESS;
    }
    return EXIT_SUCCESS;
}

static char *capture_runner_failure(const char *executable, const char *mode,
                                   size_t *captured_length, bool *overflow)
{
    int output[2];
    CHECK(!pipe(output), "Cannot create runner diagnostic pipe");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork runner diagnostic child");
    if (!child) {
        close(output[0]);
        if (dup2(output[1], STDERR_FILENO) < 0) _Exit(127);
        close(output[1]);
        execlp(executable, executable, "--run-failure", mode, (char *)NULL);
        perror("Cannot start runner diagnostic child");
        _Exit(127);
    }
    close(output[1]);

    size_t capacity = 16384, used = 0;
    *overflow = false;
    char *diagnostics = malloc(capacity);
    CHECK(diagnostics, "Out of memory capturing runner diagnostics");
    char buffer[1024];
    for (;;) {
        ssize_t count = read(output[0], buffer, sizeof(buffer));
        if (!count) break;
        CHECK(count > 0 || errno == EINTR, "Cannot read runner diagnostics");
        if (count < 0) continue;
        size_t amount = (size_t)count;
        if (used + amount < capacity) {
            memcpy(diagnostics + used, buffer, amount);
            used += amount;
        } else {
            *overflow = true;
        }
    }
    close(output[0]);
    diagnostics[used] = '\0';
    *captured_length = used;

    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for runner diagnostic child");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_FAILURE) {
        fprintf(stderr, "Runner failure child had unexpected wait status %d; diagnostics:\n%s", status, diagnostics);
        exit(EXIT_FAILURE);
    }
    return diagnostics;
}

static void stop_unrelated_process(void)
{
    if (unrelated_process <= 0) return;
    if (kill(unrelated_process, SIGTERM) && errno != ESRCH) {
        perror("Cannot stop owned unrelated-process sentinel");
        _Exit(EXIT_FAILURE);
    }
    while (waitpid(unrelated_process, NULL, 0) < 0) {
        if (errno != EINTR && errno != ECHILD) {
            perror("Cannot reap owned unrelated-process sentinel");
            _Exit(EXIT_FAILURE);
        }
        if (errno == ECHILD) break;
    }
    unrelated_process = -1;
}

static void check_failure_case(const char *executable, const char *mode, const char *expected)
{
    size_t length = 0;
    bool overflow = false;
    char *diagnostics = capture_runner_failure(executable, mode, &length, &overflow);
    CHECK(!overflow, "Runner diagnostics exceeded the test capture bound");
    if (!strstr(diagnostics, child_marker) || !strstr(diagnostics, expected)) {
        fprintf(stderr, "Runner %s diagnostics omitted its marker or outcome (%s); captured output:\n%s",
                mode, expected, diagnostics);
        exit(EXIT_FAILURE);
    }
    CHECK(fixture_root_is_empty(), "Runner failure left its owned log or fixture directory behind");
    free(diagnostics);
}

static void check_bounded_tail_case(const char *executable)
{
    size_t length = 0;
    bool overflow = false;
    char *diagnostics = capture_runner_failure(executable, "large", &length, &overflow);
    if (overflow || length > RUN_FROM_LOG_TAIL_LIMIT + 1024 ||
        !strstr(diagnostics, end_marker) || strstr(diagnostics, early_marker) ||
        !strstr(diagnostics, "earlier bytes omitted") || !strstr(diagnostics, "exit status 24")) {
        fprintf(stderr, "Runner did not return a bounded tail with the end marker and exit status; captured output:\n%s",
                diagnostics);
        free(diagnostics);
        exit(EXIT_FAILURE);
    }
    CHECK(fixture_root_is_empty(), "Large child log left its owned fixture behind");
    free(diagnostics);
}

static void check_working_directory(const char *executable)
{
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork working-directory witness");
    if (!child) {
        execlp(executable, executable, "--run-cwd", (char *)NULL);
        perror("Cannot start working-directory witness");
        _Exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for working-directory witness");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Runner did not preserve the requested child working directory");
    CHECK(fixture_root_is_empty(), "Working-directory witness left its owned fixture behind");
}

int main(int argc, char **argv)
{
    if (argc == 2 && (!strcmp(argv[1], "--emit-failure") ||
                      !strcmp(argv[1], "--emit-signal") ||
                      !strcmp(argv[1], "--emit-timeout"))) {
        fprintf(stderr, "%s\n", child_marker);
        fflush(stderr);
        if (!strcmp(argv[1], "--emit-signal")) raise(SIGTERM);
        if (!strcmp(argv[1], "--emit-timeout")) for (;;) pause();
        return 23;
    }
    if (argc == 2 && !strcmp(argv[1], "--emit-large-failure")) {
        fprintf(stderr, "%s\n", early_marker);
        for (int i = 0; i < 32 * 1024; ++i) fputc('x', stderr);
        fprintf(stderr, "\n%s\n", end_marker);
        return 24;
    }
    if (argc == 2 && !strcmp(argv[1], "--emit-cwd")) {
        char cwd[4096];
        CHECK(getcwd(cwd, sizeof(cwd)), "Cannot read child working directory");
        puts(cwd);
        return EXIT_SUCCESS;
    }
    if (argc == 3 && !strcmp(argv[1], "--run-failure"))
        return runner_child_main(argv[0], argv[2]);
    if (argc == 2 && !strcmp(argv[1], "--run-cwd"))
        return runner_child_main(argv[0], "cwd");
    CHECK(argc == 1, "Unexpected runner test argument");

    suite_test_paths_init("run-from-parent");
    CHECK(!atexit(stop_unrelated_process), "Cannot register unrelated-process cleanup");
    CHECK(!setenv("TMPDIR", suite_fixture_directory, 1), "Cannot set runner child fixture root");

    check_working_directory(argv[0]);
    check_failure_case(argv[0], "exit", "exit status 23");
    check_failure_case(argv[0], "signal", "signal 15");
    check_bounded_tail_case(argv[0]);

    unrelated_process = fork();
    CHECK(unrelated_process >= 0, "Cannot fork unrelated-process sentinel");
    if (!unrelated_process) for (;;) pause();
    check_failure_case(argv[0], "timeout", "timed out (1 seconds)");
    CHECK(kill(unrelated_process, 0) == 0, "Runner timeout killed an unrelated owned process");
    stop_unrelated_process();

    puts("PASS child exit, signal, and timeout diagnostics survive owned-log cleanup");
    return EXIT_SUCCESS;
}
