#ifndef CX16AGI_TEST_RUN_FROM_H
#define CX16AGI_TEST_RUN_FROM_H

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RUN_FROM_LOG_TAIL_LIMIT (8 * 1024)

static void run_from_fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void run_from_print_log_tail(const char *log)
{
    if (!log) return;
    FILE *file = fopen(log, "rb");
    if (!file) {
        fprintf(stderr, "Cannot read child log %s: %s\n", log, strerror(errno));
        return;
    }
    if (fseek(file, 0, SEEK_END)) {
        fprintf(stderr, "Cannot seek child log %s: %s\n", log, strerror(errno));
        fclose(file);
        return;
    }
    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "Cannot size child log %s: %s\n", log, strerror(errno));
        fclose(file);
        return;
    }
    long start = length > RUN_FROM_LOG_TAIL_LIMIT ? length - RUN_FROM_LOG_TAIL_LIMIT : 0;
    if (fseek(file, start, SEEK_SET)) {
        fprintf(stderr, "Cannot seek child log %s: %s\n", log, strerror(errno));
        fclose(file);
        return;
    }

    fputs(start ? "--- child output tail (earlier bytes omitted) ---\n" : "--- child output ---\n", stderr);
    size_t emitted = 0;
    char buffer[1024];
    while (emitted < RUN_FROM_LOG_TAIL_LIMIT) {
        size_t capacity = RUN_FROM_LOG_TAIL_LIMIT - emitted;
        if (capacity > sizeof(buffer)) capacity = sizeof(buffer);
        size_t count = fread(buffer, 1, capacity, file);
        if (!count) {
            if (ferror(file)) fprintf(stderr, "Error reading child log %s\n", log);
            break;
        }
        fwrite(buffer, 1, count, stderr);
        emitted += count;
    }
    if (emitted) fputc('\n', stderr);
    fputs("--- end child output ---\n", stderr);
    fclose(file);
}

static void run_from_fail_with_log(const char *log, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    run_from_print_log_tail(log);
    exit(EXIT_FAILURE);
}

/* Use argv directly: paths and game names never become shell commands. */
static void run_from(char *const argv[], const char *log, int timeout, const char *directory)
{
    pid_t pid = fork();
    if (pid < 0) run_from_fail("fork: %s", strerror(errno));
    if (!pid) {
        if (log) {
            FILE *f = fopen(log, "a");
            if (!f) _exit(126);
            dup2(fileno(f), STDOUT_FILENO);
            dup2(fileno(f), STDERR_FILENO);
            fclose(f);
        }
        if (directory && chdir(directory)) {
            perror(directory);
            _exit(126);
        }
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    int status;
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start))
        run_from_fail("Cannot read monotonic clock: %s", strerror(errno));
    for (;;) {
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) run_from_fail("waitpid: %s", strerror(errno));
        if (clock_gettime(CLOCK_MONOTONIC, &now))
            run_from_fail("Cannot read monotonic clock: %s", strerror(errno));
        if (timeout && (now.tv_sec - start.tv_sec > timeout ||
                        (now.tv_sec - start.tv_sec == timeout && now.tv_nsec >= start.tv_nsec))) {
            if (kill(pid, SIGKILL) && errno != ESRCH)
                run_from_fail("Cannot stop timed out child %s: %s", argv[0], strerror(errno));
            while (waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR) run_from_fail("waitpid: %s", strerror(errno));
            }
            run_from_fail_with_log(log, "%s timed out (%d seconds); see %s",
                                   argv[0], timeout, log ? log : "output");
        }
        struct timespec delay = { 0, 100000000 };
        nanosleep(&delay, NULL);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status))
        run_from_fail_with_log(log, "%s failed (exit status %d); see %s",
                               argv[0], WEXITSTATUS(status), log ? log : "output");
    if (WIFSIGNALED(status))
        run_from_fail_with_log(log, "%s terminated by signal %d; see %s",
                               argv[0], WTERMSIG(status), log ? log : "output");
    if (!WIFEXITED(status))
        run_from_fail_with_log(log, "%s failed (wait status %d); see %s",
                               argv[0], status, log ? log : "output");
}

#endif
