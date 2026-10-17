#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "review_metrics.h"

#include "review_sha256.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#define METRICS_PATH 4096
#define METRICS_LINE 8192
#define METRICS_CAPTURE 65536
#define METRICS_DEFAULT_TIMEOUT_MS UINT64_C(10000)
#define METRICS_MAX_TIMEOUT_MS UINT64_C(300000)

typedef struct {
    char kind[16];
    char commit[65];
    char tree[65];
    char image_sha[65];
    char image_path[METRICS_PATH];
    uint64_t bytes;
} ImageManifest;

typedef struct {
    uint64_t cycles;
    uint64_t latency_ns;
    char toolchain_sha[65];
    char output_sha[65];
} Measurement;

static void set_error(char *error, size_t capacity, const char *format, ...)
{
    if (!error || !capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, capacity, format, args);
    va_end(args);
}

static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (!length || length >= capacity) return false;
    memcpy(destination, source, length + 1);
    return true;
}

static bool is_hex(const char *text, size_t expected)
{
    if (strlen(text) != expected) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (!((*at >= '0' && *at <= '9') || (*at >= 'a' && *at <= 'f'))) return false;
    return true;
}

static bool is_git_sha(const char *text)
{
    size_t length = strlen(text);
    return (length == 40 || length == 64) && is_hex(text, length);
}

static bool is_name(const char *text)
{
    if (!*text || !(((*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z') ||
                     (*text >= '0' && *text <= '9')))) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (!( (*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') ||
               (*at >= '0' && *at <= '9') || *at == '-' || *at == '_' || *at == '.' )) return false;
    return strcmp(text, ".") && strcmp(text, "..");
}

static bool valid_relative_path(const char *path)
{
    if (!*path || path[0] == '/' || path[0] == '-') return false;
    const char *component = path;
    for (const char *at = path;; ++at) {
        if (*at && *at != '/') {
            if ((unsigned char)*at < 0x20 || *at == 0x7f || *at == '|') return false;
            continue;
        }
        size_t length = (size_t)(at - component);
        if (!length || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.')) return false;
        if (!*at) break;
        component = at + 1;
    }
    return true;
}

static bool parse_u64(const char *text, uint64_t *value)
{
    if (!*text) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < '0' || *at > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end || parsed > UINT64_MAX) return false;
    *value = (uint64_t)parsed;
    return true;
}

static int split_fields(char *line, char **fields, size_t capacity)
{
    size_t count = 0;
    fields[count++] = line;
    for (char *at = line; *at; ++at) {
        if (*at != '|') continue;
        *at = '\0';
        if (count == capacity) return -1;
        fields[count++] = at + 1;
    }
    for (size_t i = 0; i < count; ++i) if (!*fields[i]) return -1;
    return (int)count;
}

static uint64_t monotonic_milliseconds(void);
static void stop_owned_runner(pid_t child);

static int run_capture(char *const argv[], char *output, size_t output_capacity)
{
    if (!output || output_capacity < 2) return -1;
    int descriptors[2];
    if (pipe(descriptors)) return -1;
    pid_t child = fork();
    if (child < 0) { close(descriptors[0]); close(descriptors[1]); return -1; }
    if (!child) {
        (void)setpgid(0, 0);
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        execvp(argv[0], argv);
        _Exit(127);
    }
    (void)setpgid(child, child);
    close(descriptors[1]);
    int descriptor_flags = fcntl(descriptors[0], F_GETFL);
    if (descriptor_flags < 0 || fcntl(descriptors[0], F_SETFL, descriptor_flags | O_NONBLOCK)) {
        close(descriptors[0]);
        stop_owned_runner(child);
        return -1;
    }
    size_t used = 0;
    bool eof = false, exited = false;
    int status = 0;
    uint64_t start = monotonic_milliseconds();
    uint64_t deadline = start + METRICS_DEFAULT_TIMEOUT_MS;
    if (!start || deadline < start) {
        close(descriptors[0]);
        stop_owned_runner(child);
        return -1;
    }
    char buffer[4096];
    while (!eof || !exited) {
        uint64_t now = monotonic_milliseconds();
        if (!now || now >= deadline) {
            close(descriptors[0]);
            stop_owned_runner(child);
            return -1;
        }
        int delay = (int)(deadline - now);
        if (delay > 100) delay = 100;
        struct pollfd poll_descriptor = { .fd = descriptors[0], .events = POLLIN | POLLHUP };
        int ready = poll(&poll_descriptor, 1, delay);
        if (ready < 0 && errno != EINTR) {
            close(descriptors[0]);
            stop_owned_runner(child);
            return -1;
        }
        if (ready > 0 && (poll_descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
            for (;;) {
                ssize_t amount = read(descriptors[0], buffer, sizeof(buffer));
                if (amount < 0 && errno == EINTR) continue;
                if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (amount < 0) {
                    close(descriptors[0]);
                    stop_owned_runner(child);
                    return -1;
                }
                if (!amount) { eof = true; break; }
                if ((size_t)amount > output_capacity - 1 - used) {
                    close(descriptors[0]);
                    stop_owned_runner(child);
                    return -1;
                }
                memcpy(output + used, buffer, (size_t)amount);
                used += (size_t)amount;
            }
        }
        if (!exited) {
            pid_t waited = waitpid(child, &status, WNOHANG);
            if (waited == child) exited = true;
            else if (waited < 0 && errno != EINTR) {
                close(descriptors[0]);
                stop_owned_runner(child);
                return -1;
            }
        }
    }
    close(descriptors[0]);
    output[used] = '\0';
    if (!WIFEXITED(status)) return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    return WEXITSTATUS(status);
}

static int run_git(const char *repository, char *const arguments[], char *output, size_t capacity)
{
    char *argv[24];
    size_t count = 0;
    argv[count++] = (char *)"git";
    argv[count++] = (char *)"-C";
    argv[count++] = (char *)repository;
    for (size_t i = 0; arguments[i]; ++i) {
        if (count + 1 >= sizeof(argv) / sizeof(argv[0])) return -1;
        argv[count++] = arguments[i];
    }
    argv[count] = NULL;
    return run_capture(argv, output, capacity);
}

static bool git_value(const char *repository, const char *revision, char *value, size_t capacity)
{
    char *arguments[] = { (char *)"rev-parse", (char *)"--verify", (char *)"--end-of-options",
        (char *)revision, NULL };
    if (run_git(repository, arguments, value, capacity)) return false;
    value[strcspn(value, "\r\n")] = '\0';
    return is_git_sha(value);
}

static bool git_commit_exists(const char *repository, const char *sha)
{
    char revision[80], ignored[8];
    if (snprintf(revision, sizeof(revision), "%s^{commit}", sha) >= (int)sizeof(revision)) return false;
    char *arguments[] = { (char *)"cat-file", (char *)"-e", revision, NULL };
    return run_git(repository, arguments, ignored, sizeof(ignored)) == 0;
}

static bool git_is_ancestor(const char *repository, const char *ancestor, const char *descendant)
{
    char *arguments[] = { (char *)"merge-base", (char *)"--is-ancestor", (char *)ancestor,
        (char *)descendant, NULL };
    char ignored[8];
    return run_git(repository, arguments, ignored, sizeof(ignored)) == 0;
}

static bool repository_is_clean_at(const char *repository, const char *candidate)
{
    char head[80], status[8192];
    if (!git_value(repository, "HEAD", head, sizeof(head)) || strcmp(head, candidate)) return false;
    char *arguments[] = { (char *)"status", (char *)"--porcelain=v1", (char *)"--untracked-files=all", NULL };
    return !run_git(repository, arguments, status, sizeof(status)) && !*status;
}

static bool resolve_regular_file(const char *path, char resolved[METRICS_PATH], struct stat *info)
{
    if (!path || !*path) return false;
    struct stat link_info;
    if (lstat(path, &link_info) || S_ISLNK(link_info.st_mode) || !S_ISREG(link_info.st_mode) ||
        !realpath(path, resolved) || stat(resolved, info) || !S_ISREG(info->st_mode) || info->st_size < 0)
        return false;
    return true;
}

static bool read_image_manifest(const char *manifest_path, const char *image_path,
                                ImageManifest *manifest, char *error, size_t error_capacity)
{
    char resolved_image[METRICS_PATH];
    struct stat info;
    if (!resolve_regular_file(image_path, resolved_image, &info)) {
        set_error(error, error_capacity, "image is missing, unsafe, or not a regular file");
        return false;
    }
    uint64_t image_bytes = (uint64_t)info.st_size;
    char actual_sha[65];
    if (review_sha256_file(resolved_image, actual_sha)) {
        set_error(error, error_capacity, "cannot hash image");
        return false;
    }

    FILE *file = fopen(manifest_path, "rb");
    if (!file) { set_error(error, error_capacity, "cannot open image manifest: %s", strerror(errno)); return false; }
    char *line = NULL;
    size_t allocated = 0;
    ssize_t length;
    size_t stage = 0;
    bool malformed = false;
    const char *keys[] = { "kind", "commit", "tree", "image_sha" };
    while ((length = getline(&line, &allocated, file)) >= 0) {
        if ((size_t)length > METRICS_LINE || !length || line[length - 1] != '\n' ||
            memchr(line, '\0', (size_t)length)) { malformed = true; break; }
        line[--length] = '\0';
        if (strchr(line, '\r') || !*line) { malformed = true; break; }
        if (!stage) {
            malformed = strcmp(line, "review-image-v1") != 0;
            stage = 1;
            if (malformed) break;
            continue;
        }
        char *fields[4];
        int count = split_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        if (count != 2 || stage > 4 || strcmp(fields[0], keys[stage - 1])) { malformed = true; break; }
        if (stage == 1) {
            if (strcmp(fields[1], "release") && strcmp(fields[1], "test")) malformed = true;
            else copy_text(manifest->kind, sizeof(manifest->kind), fields[1]);
        } else if (stage == 2) {
            if (!is_git_sha(fields[1])) malformed = true;
            else copy_text(manifest->commit, sizeof(manifest->commit), fields[1]);
        } else if (stage == 3) {
            if (!is_git_sha(fields[1])) malformed = true;
            else copy_text(manifest->tree, sizeof(manifest->tree), fields[1]);
        } else {
            if (!is_hex(fields[1], 64)) malformed = true;
            else copy_text(manifest->image_sha, sizeof(manifest->image_sha), fields[1]);
        }
        ++stage;
        if (malformed) break;
    }
    if (ferror(file)) malformed = true;
    if (fclose(file)) malformed = true;
    free(line);
    if (malformed || stage != 5 || strcmp(actual_sha, manifest->image_sha)) {
        set_error(error, error_capacity, "image manifest is malformed or its image digest mismatches");
        return false;
    }
    if (!copy_text(manifest->image_path, sizeof(manifest->image_path), resolved_image)) {
        set_error(error, error_capacity, "image path is too long");
        return false;
    }
    manifest->bytes = image_bytes;
    return true;
}

static bool validate_image_identity(const char *repository, const ImageManifest *image,
                                    char *error, size_t error_capacity)
{
    if (!git_commit_exists(repository, image->commit)) {
        set_error(error, error_capacity, "image manifest names a missing Git commit");
        return false;
    }
    char revision[80], tree[80];
    if (snprintf(revision, sizeof(revision), "%s^{tree}", image->commit) >= (int)sizeof(revision) ||
        !git_value(repository, revision, tree, sizeof(tree)) || strcmp(tree, image->tree)) {
        set_error(error, error_capacity, "image tree does not match its real Git commit");
        return false;
    }
    return true;
}

static bool parse_tradeoff(const char *repository, const char *candidate_commit, const char *path,
                           char *error, size_t error_capacity)
{
    if (!valid_relative_path(path)) {
        set_error(error, error_capacity, "tradeoff provenance path must be repository-relative");
        return false;
    }
    char working_path[METRICS_PATH];
    if (snprintf(working_path, sizeof(working_path), "%s/%s", repository, path) >= (int)sizeof(working_path)) {
        set_error(error, error_capacity, "tradeoff provenance path is too long");
        return false;
    }
    struct stat info;
    char resolved[METRICS_PATH];
    if (!resolve_regular_file(working_path, resolved, &info)) {
        set_error(error, error_capacity, "tradeoff proposal is not a tracked regular file");
        return false;
    }
    FILE *file = fopen(resolved, "rb");
    if (!file) { set_error(error, error_capacity, "cannot read tradeoff provenance"); return false; }
    char contents[4096];
    size_t used = fread(contents, 1, sizeof(contents) - 1, file);
    bool read_failed = ferror(file) || !feof(file);
    if (fclose(file)) read_failed = true;
    contents[used] = '\0';
    if (read_failed || !used || contents[used - 1] != '\n') {
        set_error(error, error_capacity, "tradeoff proposal is incomplete");
        return false;
    }
    char revision[METRICS_PATH];
    if (snprintf(revision, sizeof(revision), "%s:%s", candidate_commit, path) >= (int)sizeof(revision)) {
        set_error(error, error_capacity, "tradeoff provenance revision is too long");
        return false;
    }
    char *show_args[] = { (char *)"show", (char *)"--no-ext-diff", revision, NULL };
    char committed[4096];
    if (run_git(repository, show_args, committed, sizeof(committed)) || strcmp(committed, contents)) {
        set_error(error, error_capacity, "tradeoff proposal is not present in the candidate commit");
        return false;
    }

    char *lines[4];
    size_t line_count = 0;
    char *start = contents;
    for (char *at = contents; *at; ++at) {
        if (*at != '\n') continue;
        *at = '\0';
        if (line_count == sizeof(lines) / sizeof(lines[0]) || !*start) {
            set_error(error, error_capacity, "tradeoff proposal has malformed records");
            return false;
        }
        lines[line_count++] = start;
        start = at + 1;
    }
    if (start != contents + used || line_count != 3 || strcmp(lines[0], "review-metrics-tradeoff-v1") ||
        strncmp(lines[1], "review_reference|", 17) || !lines[1][17] ||
        strncmp(lines[2], "reason|", 7) || !lines[2][7]) {
        set_error(error, error_capacity, "tradeoff proposal provenance needs a review reference and reason");
        return false;
    }
    for (size_t i = 1; i < line_count; ++i)
        for (const unsigned char *at = (const unsigned char *)lines[i]; *at; ++at)
            if (*at < 0x20 || *at == 0x7f) {
                set_error(error, error_capacity, "tradeoff proposal provenance contains control characters");
                return false;
            }
    return true;
}

static bool prepare_temp_root(const char *scratch, char root[METRICS_PATH], char *error, size_t error_capacity)
{
    struct stat info;
    if (lstat(scratch, &info) || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) ||
        !realpath(scratch, root)) {
        set_error(error, error_capacity, "scratch directory is missing or unsafe");
        return false;
    }
    char template[METRICS_PATH];
    if (snprintf(template, sizeof(template), "%s/review-metrics-XXXXXX", root) >= (int)sizeof(template) ||
        !mkdtemp(template)) {
        set_error(error, error_capacity, "cannot create review metrics scratch directory: %s", strerror(errno));
        return false;
    }
    strcpy(root, template);
    return true;
}

static bool remove_tree(const char *path)
{
    struct stat info;
    if (lstat(path, &info)) return errno == ENOENT;
    if (!S_ISDIR(info.st_mode)) return !unlink(path) || errno == ENOENT;
    DIR *directory = opendir(path);
    if (!directory) return false;
    bool ok = true;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) { if (errno) ok = false; break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[METRICS_PATH];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            !remove_tree(child)) ok = false;
    }
    if (closedir(directory)) ok = false;
    if (ok && rmdir(path)) ok = false;
    return ok;
}

static uint64_t monotonic_milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec / UINT64_C(1000000);
}

static void stop_owned_runner(pid_t child)
{
    (void)kill(-child, SIGKILL);
    (void)kill(child, SIGKILL);
    int status;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
}

static bool run_metric(const char *runner, const char *input, const char *image,
                       const char *output_path, const char *workload, const char *input_sha,
                       uint64_t timeout_ms, Measurement *measurement,
                       char *error, size_t error_capacity)
{
    int descriptors[2];
    if (pipe(descriptors)) { set_error(error, error_capacity, "cannot create runner output pipe"); return false; }
    pid_t child = fork();
    if (child < 0) { close(descriptors[0]); close(descriptors[1]); set_error(error, error_capacity, "cannot fork metric runner"); return false; }
    if (!child) {
        (void)setpgid(0, 0);
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0 || dup2(descriptors[1], STDERR_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        char *argv[] = { (char *)runner, (char *)"--input", (char *)input, (char *)"--image", (char *)image,
            (char *)"--output", (char *)output_path, (char *)"--workload", (char *)workload, NULL };
        execv(runner, argv);
        _Exit(127);
    }
    (void)setpgid(child, child);
    close(descriptors[1]);
    int descriptor_flags = fcntl(descriptors[0], F_GETFL);
    if (descriptor_flags < 0 || fcntl(descriptors[0], F_SETFL, descriptor_flags | O_NONBLOCK)) {
        close(descriptors[0]);
        stop_owned_runner(child);
        set_error(error, error_capacity, "cannot configure runner output pipe");
        return false;
    }
    char output[METRICS_CAPTURE + 2];
    size_t used = 0;
    bool eof = false, exited = false;
    int status = 0;
    uint64_t start = monotonic_milliseconds();
    uint64_t deadline = start + timeout_ms;
    if (!start || deadline < start) {
        close(descriptors[0]);
        stop_owned_runner(child);
        set_error(error, error_capacity, "cannot establish a monotonic runner deadline");
        return false;
    }
    while (!eof || !exited) {
        uint64_t now = monotonic_milliseconds();
        if (!now || now >= deadline) {
            close(descriptors[0]);
            stop_owned_runner(child);
            set_error(error, error_capacity, "metric runner timed out after %" PRIu64 " ms", timeout_ms);
            return false;
        }
        int delay = (int)(deadline - now);
        if (delay > 100) delay = 100;
        struct pollfd poll_descriptor = { .fd = descriptors[0], .events = POLLIN | POLLHUP };
        int ready = poll(&poll_descriptor, 1, delay);
        if (ready < 0 && errno != EINTR) {
            close(descriptors[0]);
            stop_owned_runner(child);
            set_error(error, error_capacity, "cannot poll metric runner output: %s", strerror(errno));
            return false;
        }
        if (ready > 0 && (poll_descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
            for (;;) {
                ssize_t amount = read(descriptors[0], output + used, sizeof(output) - 1 - used);
                if (amount < 0 && errno == EINTR) continue;
                if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (amount < 0) {
                    close(descriptors[0]);
                    stop_owned_runner(child);
                    set_error(error, error_capacity, "cannot read metric runner output: %s", strerror(errno));
                    return false;
                }
                if (!amount) { eof = true; break; }
                used += (size_t)amount;
                if (used > METRICS_CAPTURE) {
                    close(descriptors[0]);
                    stop_owned_runner(child);
                    set_error(error, error_capacity, "metric runner output exceeds %u bytes", METRICS_CAPTURE);
                    return false;
                }
            }
        }
        if (!exited) {
            pid_t waited = waitpid(child, &status, WNOHANG);
            if (waited == child) exited = true;
            else if (waited < 0 && errno != EINTR) {
                close(descriptors[0]);
                stop_owned_runner(child);
                set_error(error, error_capacity, "cannot wait for metric runner: %s", strerror(errno));
                return false;
            }
        }
    }
    close(descriptors[0]);
    output[used] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        set_error(error, error_capacity, "metric runner did not complete successfully");
        return false;
    }
    if (!used || output[used - 1] != '\n' || memchr(output, '\0', used)) {
        set_error(error, error_capacity, "metric runner returned malformed execution evidence");
        return false;
    }
    output[--used] = '\0';
    char *fields[8];
    int count = split_fields(output, fields, sizeof(fields) / sizeof(fields[0]));
    if (count != 7 || strcmp(fields[0], "review-metric-v2")) {
        set_error(error, error_capacity, "metric runner returned malformed execution evidence");
        return false;
    }
    if (strcmp(fields[1], workload)) {
        set_error(error, error_capacity, "workload identity mismatch between runner and gate");
        return false;
    }
    if (!is_hex(fields[2], 64) || strcmp(fields[2], input_sha)) {
        set_error(error, error_capacity, "input digest mismatch between runner and gate");
        return false;
    }
    if (!is_hex(fields[3], 64) || !copy_text(measurement->toolchain_sha, sizeof(measurement->toolchain_sha), fields[3])) {
        set_error(error, error_capacity, "metric runner toolchain identity is malformed");
        return false;
    }
    if (!parse_u64(fields[4], &measurement->cycles)) {
        set_error(error, error_capacity, "cycle count overflows uint64 or is malformed");
        return false;
    }
    if (!parse_u64(fields[5], &measurement->latency_ns)) {
        set_error(error, error_capacity, "latency count overflows uint64 or is malformed");
        return false;
    }
    if (strcmp(fields[6], "emulator-cycle-counter")) {
        set_error(error, error_capacity, "cycle evidence must identify the emulator cycle counter");
        return false;
    }
    struct stat output_info;
    char resolved_output[METRICS_PATH];
    if (!resolve_regular_file(output_path, resolved_output, &output_info) ||
        review_sha256_file(resolved_output, measurement->output_sha)) {
        set_error(error, error_capacity, "metric runner output is missing or unsafe");
        return false;
    }
    return true;
}

static bool file_digest_and_size(const char *path, char digest[65], uint64_t *size,
                                 char *error, size_t error_capacity)
{
    char resolved[METRICS_PATH];
    struct stat info;
    if (!resolve_regular_file(path, resolved, &info) || review_sha256_file(resolved, digest)) {
        set_error(error, error_capacity, "input or runner is missing, unsafe, or unreadable");
        return false;
    }
    if (size) *size = (uint64_t)info.st_size;
    return true;
}

static bool verify_file_digest(const char *path, const char *expected, const char *label,
                               char *error, size_t error_capacity)
{
    char actual[65], detail[256];
    if (!file_digest_and_size(path, actual, NULL, detail, sizeof(detail))) {
        set_error(error, error_capacity, "%s could not be revalidated after metric execution", label);
        return false;
    }
    if (strcmp(actual, expected)) {
        set_error(error, error_capacity, "%s changed during metric execution", label);
        return false;
    }
    return true;
}

static bool verify_execution_snapshot(const char *repository, const char *input,
                                      const char *input_sha,
                                      const char *baseline_runner, const char *baseline_runner_sha,
                                      const char *candidate_runner, const char *candidate_runner_sha,
                                      const char *baseline_manifest, const char *baseline_manifest_sha,
                                      const char *candidate_manifest, const char *candidate_manifest_sha,
                                      const ImageManifest *baseline, const ImageManifest *candidate,
                                      char *error, size_t error_capacity)
{
    if (!verify_file_digest(input, input_sha, "workload input", error, error_capacity) ||
        !verify_file_digest(baseline_runner, baseline_runner_sha, "baseline runner", error, error_capacity) ||
        !verify_file_digest(candidate_runner, candidate_runner_sha, "candidate runner", error, error_capacity) ||
        !verify_file_digest(baseline_manifest, baseline_manifest_sha, "baseline image manifest", error, error_capacity) ||
        !verify_file_digest(candidate_manifest, candidate_manifest_sha, "candidate image manifest", error, error_capacity) ||
        !verify_file_digest(baseline->image_path, baseline->image_sha, "baseline image", error, error_capacity) ||
        !verify_file_digest(candidate->image_path, candidate->image_sha, "candidate image", error, error_capacity) ||
        !validate_image_identity(repository, baseline, error, error_capacity) ||
        !validate_image_identity(repository, candidate, error, error_capacity)) return false;
    if (!repository_is_clean_at(repository, candidate->commit)) {
        set_error(error, error_capacity, "Git checkout changed during metric execution");
        return false;
    }
    return true;
}

int review_metrics_gate(const ReviewMetricsOptions *options, ReviewMetricsResult *result,
                        char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!options || !result || !options->repository || !options->input_path || !options->workload ||
        !options->baseline_image || !options->baseline_manifest || !options->baseline_runner ||
        !options->candidate_image || !options->candidate_manifest || !options->candidate_runner ||
        !options->proof_kind || !options->scratch_directory) {
        set_error(error, error_capacity, "repository, workload, input, images, runners, proof kind, and scratch directory are required");
        return -1;
    }
    if (!options->max_bytes || !options->max_cycles || !options->max_latency_ns) {
        set_error(error, error_capacity, "both metric budgets and a latency budget are required and must be positive");
        return -1;
    }
    uint64_t runner_timeout_ms = options->runner_timeout_ms ? options->runner_timeout_ms : METRICS_DEFAULT_TIMEOUT_MS;
    if (!runner_timeout_ms || runner_timeout_ms > METRICS_MAX_TIMEOUT_MS) {
        set_error(error, error_capacity, "runner timeout must be between 1 and %" PRIu64 " ms", METRICS_MAX_TIMEOUT_MS);
        return -1;
    }
    if (!is_name(options->workload) || (strcmp(options->proof_kind, "release") && strcmp(options->proof_kind, "test"))) {
        set_error(error, error_capacity, "workload name or proof kind is invalid");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    char repository[METRICS_PATH], input[METRICS_PATH], baseline_runner[METRICS_PATH], candidate_runner[METRICS_PATH];
    struct stat info;
    if (!realpath(options->repository, repository) ||
        !resolve_regular_file(options->input_path, input, &info) ||
        access(options->baseline_runner, X_OK) || access(options->candidate_runner, X_OK) ||
        !resolve_regular_file(options->baseline_runner, baseline_runner, &info) ||
        !resolve_regular_file(options->candidate_runner, candidate_runner, &info)) {
        set_error(error, error_capacity, "repository, input, or runner path is missing or unsafe");
        return -1;
    }
    ImageManifest baseline = { 0 }, candidate = { 0 };
    char baseline_manifest_path[METRICS_PATH], candidate_manifest_path[METRICS_PATH];
    char baseline_manifest_sha[65], candidate_manifest_sha[65];
    if (!resolve_regular_file(options->baseline_manifest, baseline_manifest_path, &info) ||
        !resolve_regular_file(options->candidate_manifest, candidate_manifest_path, &info) ||
        !file_digest_and_size(baseline_manifest_path, baseline_manifest_sha, NULL, error, error_capacity) ||
        !file_digest_and_size(candidate_manifest_path, candidate_manifest_sha, NULL, error, error_capacity)) {
        if (!error || !*error) set_error(error, error_capacity, "image manifest is missing or unsafe");
        return -1;
    }
    if (!read_image_manifest(options->baseline_manifest, options->baseline_image, &baseline, error, error_capacity) ||
        !read_image_manifest(options->candidate_manifest, options->candidate_image, &candidate, error, error_capacity)) return -1;
    if (!verify_file_digest(baseline_manifest_path, baseline_manifest_sha, "baseline image manifest", error, error_capacity) ||
        !verify_file_digest(candidate_manifest_path, candidate_manifest_sha, "candidate image manifest", error, error_capacity)) return -1;
    if (!strcmp(options->proof_kind, "release") &&
        (!strcmp(baseline.kind, "test") || !strcmp(candidate.kind, "test"))) {
        set_error(error, error_capacity, "test image cannot be used for a release-kind budget comparison");
        return -1;
    }
    if (strcmp(baseline.kind, candidate.kind)) {
        set_error(error, error_capacity, "baseline and candidate image kinds differ");
        return -1;
    }
    if (strcmp(options->proof_kind, baseline.kind)) {
        set_error(error, error_capacity, "image kind does not match requested image kind");
        return -1;
    }
    if (!validate_image_identity(repository, &baseline, error, error_capacity) ||
        !validate_image_identity(repository, &candidate, error, error_capacity)) return -1;
    if (strcmp(baseline.commit, candidate.commit) &&
        !git_is_ancestor(repository, baseline.commit, candidate.commit)) {
        set_error(error, error_capacity, "candidate image commit does not descend from baseline commit");
        return -1;
    }
    if (!repository_is_clean_at(repository, candidate.commit)) {
        set_error(error, error_capacity, "Git checkout must be clean at the candidate image commit");
        return -1;
    }
    uint64_t input_size;
    if (!file_digest_and_size(input, result->input_sha256, &input_size, error, error_capacity)) return -1;
    (void)input_size;
    char baseline_runner_sha[65], candidate_runner_sha[65];
    if (!file_digest_and_size(baseline_runner, baseline_runner_sha, NULL, error, error_capacity) ||
        !file_digest_and_size(candidate_runner, candidate_runner_sha, NULL, error, error_capacity)) return -1;
    if (strcmp(baseline_runner_sha, candidate_runner_sha)) {
        set_error(error, error_capacity, "baseline and candidate runner tool pins differ");
        return -1;
    }

    char scratch_root[METRICS_PATH];
    if (!prepare_temp_root(options->scratch_directory, scratch_root, error, error_capacity)) return -1;
    char baseline_output[METRICS_PATH], candidate_output[METRICS_PATH];
    if (snprintf(baseline_output, sizeof(baseline_output), "%s/baseline.output", scratch_root) >= (int)sizeof(baseline_output) ||
        snprintf(candidate_output, sizeof(candidate_output), "%s/candidate.output", scratch_root) >= (int)sizeof(candidate_output)) {
        set_error(error, error_capacity, "metric output path is too long");
        remove_tree(scratch_root);
        return -1;
    }
    Measurement baseline_measurement = { 0 }, candidate_measurement = { 0 };
    bool baseline_measured = run_metric(baseline_runner, input, baseline.image_path, baseline_output,
                                        options->workload, result->input_sha256, runner_timeout_ms,
                                        &baseline_measurement, error, error_capacity);
    bool snapshot_before_candidate = baseline_measured &&
        verify_execution_snapshot(repository, input, result->input_sha256,
                                  baseline_runner, baseline_runner_sha, candidate_runner, candidate_runner_sha,
                                  baseline_manifest_path, baseline_manifest_sha,
                                  candidate_manifest_path, candidate_manifest_sha,
                                  &baseline, &candidate, error, error_capacity);
    bool candidate_measured = snapshot_before_candidate &&
        run_metric(candidate_runner, input, candidate.image_path, candidate_output,
                   options->workload, result->input_sha256, runner_timeout_ms,
                   &candidate_measurement, error, error_capacity);
    bool measured = baseline_measured && snapshot_before_candidate && candidate_measured;
    bool unchanged = measured &&
        verify_execution_snapshot(repository, input, result->input_sha256,
                                  baseline_runner, baseline_runner_sha, candidate_runner, candidate_runner_sha,
                                  baseline_manifest_path, baseline_manifest_sha,
                                  candidate_manifest_path, candidate_manifest_sha,
                                  &baseline, &candidate, error, error_capacity) &&
        verify_file_digest(baseline_output, baseline_measurement.output_sha,
                           "baseline measurement output", error, error_capacity) &&
        verify_file_digest(candidate_output, candidate_measurement.output_sha,
                           "candidate measurement output", error, error_capacity);
    bool cleaned = remove_tree(scratch_root);
    if (!measured || !unchanged || !cleaned) {
        if (!cleaned && (!error || !*error)) set_error(error, error_capacity, "cannot clean metric runner outputs");
        return -1;
    }
    if (!baseline.bytes || !baseline_measurement.cycles || !baseline_measurement.latency_ns ||
        !candidate.bytes || !candidate_measurement.cycles || !candidate_measurement.latency_ns) {
        set_error(error, error_capacity, "baseline bytes, cycles, and latency must be nonzero and candidate metrics must be valid");
        return -1;
    }
    if (strcmp(baseline_measurement.toolchain_sha, candidate_measurement.toolchain_sha)) {
        set_error(error, error_capacity, "measurement toolchain identity mismatch between baseline and candidate");
        return -1;
    }
    if (strcmp(baseline_measurement.output_sha, candidate_measurement.output_sha)) {
        set_error(error, error_capacity, "baseline and candidate outputs are not equivalent");
        return -1;
    }
    if (candidate.bytes > options->max_bytes || candidate_measurement.cycles > options->max_cycles ||
        candidate_measurement.latency_ns > options->max_latency_ns) {
        set_error(error, error_capacity, "candidate exceeds an active byte, cycle, or latency budget");
        return -1;
    }

    bool pareto = candidate.bytes <= baseline.bytes && candidate_measurement.cycles <= baseline_measurement.cycles &&
                  candidate_measurement.latency_ns <= baseline_measurement.latency_ns &&
                  (candidate.bytes < baseline.bytes || candidate_measurement.cycles < baseline_measurement.cycles ||
                   candidate_measurement.latency_ns < baseline_measurement.latency_ns);
    if (!pareto && !options->tradeoff_path) {
        set_error(error, error_capacity, "candidate does not locally Pareto-improve; documented tradeoff proposal provenance is required");
        return -1;
    }
    if (!pareto && !parse_tradeoff(repository, candidate.commit, options->tradeoff_path, error, error_capacity)) return -1;

    copy_text(result->baseline_commit, sizeof(result->baseline_commit), baseline.commit);
    copy_text(result->baseline_tree, sizeof(result->baseline_tree), baseline.tree);
    copy_text(result->candidate_commit, sizeof(result->candidate_commit), candidate.commit);
    copy_text(result->candidate_tree, sizeof(result->candidate_tree), candidate.tree);
    copy_text(result->runner_sha256, sizeof(result->runner_sha256), baseline_runner_sha);
    copy_text(result->toolchain_sha256, sizeof(result->toolchain_sha256), baseline_measurement.toolchain_sha);
    copy_text(result->baseline_image_sha256, sizeof(result->baseline_image_sha256), baseline.image_sha);
    copy_text(result->candidate_image_sha256, sizeof(result->candidate_image_sha256), candidate.image_sha);
    copy_text(result->baseline_output_sha256, sizeof(result->baseline_output_sha256), baseline_measurement.output_sha);
    copy_text(result->candidate_output_sha256, sizeof(result->candidate_output_sha256), candidate_measurement.output_sha);
    result->baseline_bytes = baseline.bytes;
    result->baseline_cycles = baseline_measurement.cycles;
    result->baseline_latency_ns = baseline_measurement.latency_ns;
    result->candidate_bytes = candidate.bytes;
    result->candidate_cycles = candidate_measurement.cycles;
    result->candidate_latency_ns = candidate_measurement.latency_ns;
    result->max_bytes = options->max_bytes;
    result->max_cycles = options->max_cycles;
    result->max_latency_ns = options->max_latency_ns;
    result->runner_timeout_ms = runner_timeout_ms;
    result->local_pareto_improvement = pareto;
    return 0;
}

static bool parse_budget(const char *text, uint64_t *value)
{
    return parse_u64(text, value) && *value > 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "gate")) {
        fprintf(stderr, "usage: review_metrics gate --repo DIR --input FILE --workload ID --baseline-image FILE --baseline-manifest FILE --candidate-image FILE --candidate-manifest FILE --baseline-runner EXE --candidate-runner EXE --proof-kind release|test --max-bytes N --max-cycles N --max-latency-ns N --scratch-dir DIR [--runner-timeout-ms N] [--tradeoff REPO_PATH]\n");
        return EXIT_FAILURE;
    }
    ReviewMetricsOptions options = { 0 };
    bool seen_repo = false, seen_input = false, seen_workload = false, seen_bi = false, seen_bm = false;
    bool seen_ci = false, seen_cm = false, seen_br = false, seen_cr = false, seen_kind = false;
    bool seen_bytes = false, seen_cycles = false, seen_latency = false, seen_timeout = false;
    bool seen_scratch = false, seen_tradeoff = false;
    for (int i = 2; i < argc; ++i) {
        if (i + 1 >= argc) { fprintf(stderr, "review-metrics: missing option value\n"); return EXIT_FAILURE; }
        const char *option = argv[i++], *value = argv[i];
#define TAKE(flag, field) do { if (flag) { fprintf(stderr, "review-metrics: duplicate %s\n", option); return EXIT_FAILURE; } flag = true; options.field = value; } while (0)
        if (!strcmp(option, "--repo")) TAKE(seen_repo, repository);
        else if (!strcmp(option, "--input")) TAKE(seen_input, input_path);
        else if (!strcmp(option, "--workload")) TAKE(seen_workload, workload);
        else if (!strcmp(option, "--baseline-image")) TAKE(seen_bi, baseline_image);
        else if (!strcmp(option, "--baseline-manifest")) TAKE(seen_bm, baseline_manifest);
        else if (!strcmp(option, "--candidate-image")) TAKE(seen_ci, candidate_image);
        else if (!strcmp(option, "--candidate-manifest")) TAKE(seen_cm, candidate_manifest);
        else if (!strcmp(option, "--baseline-runner")) TAKE(seen_br, baseline_runner);
        else if (!strcmp(option, "--candidate-runner")) TAKE(seen_cr, candidate_runner);
        else if (!strcmp(option, "--proof-kind")) TAKE(seen_kind, proof_kind);
        else if (!strcmp(option, "--scratch-dir")) TAKE(seen_scratch, scratch_directory);
        else if (!strcmp(option, "--tradeoff")) TAKE(seen_tradeoff, tradeoff_path);
        else if (!strcmp(option, "--max-bytes")) {
            if (seen_bytes || !parse_budget(value, &options.max_bytes)) { fprintf(stderr, "review-metrics: max-bytes gate must be positive\n"); return EXIT_FAILURE; }
            seen_bytes = true;
        } else if (!strcmp(option, "--max-cycles")) {
            if (seen_cycles || !parse_budget(value, &options.max_cycles)) { fprintf(stderr, "review-metrics: max-cycles gate must be positive\n"); return EXIT_FAILURE; }
            seen_cycles = true;
        } else if (!strcmp(option, "--max-latency-ns")) {
            if (seen_latency || !parse_budget(value, &options.max_latency_ns)) { fprintf(stderr, "review-metrics: max-latency-ns gate must be positive\n"); return EXIT_FAILURE; }
            seen_latency = true;
        } else if (!strcmp(option, "--runner-timeout-ms")) {
            if (seen_timeout || !parse_budget(value, &options.runner_timeout_ms) ||
                options.runner_timeout_ms > METRICS_MAX_TIMEOUT_MS) {
                fprintf(stderr, "review-metrics: runner-timeout-ms must be between 1 and %" PRIu64 "\n",
                        METRICS_MAX_TIMEOUT_MS);
                return EXIT_FAILURE;
            }
            seen_timeout = true;
        } else { fprintf(stderr, "review-metrics: unknown option %s\n", option); return EXIT_FAILURE; }
#undef TAKE
    }
    if (!seen_repo || !seen_input || !seen_workload || !seen_bi || !seen_bm || !seen_ci || !seen_cm ||
        !seen_br || !seen_cr || !seen_kind || !seen_scratch || !seen_bytes || !seen_cycles || !seen_latency) {
        fprintf(stderr, "review-metrics: both metric budgets and a latency budget are required; all identity inputs are also required\n");
        return EXIT_FAILURE;
    }
    ReviewMetricsResult result;
    char error[1024];
    if (review_metrics_gate(&options, &result, error, sizeof(error))) {
        fprintf(stderr, "review-metrics: %s\n", error[0] ? error : "metric gate failed");
        return EXIT_FAILURE;
    }
    printf("review-metrics-result-v2\n"
           "workload|%s\ninput_sha256|%s\nrunner_sha256|%s\nrunner_reported_toolchain_sha256|%s\n"
           "image_kind|%s\nrunner_timeout_ms|%" PRIu64 "\n"
           "max_bytes|%" PRIu64 "\nmax_cycles|%" PRIu64 "\nmax_latency_ns|%" PRIu64 "\n"
           "baseline_commit|%s\nbaseline_tree|%s\nbaseline_bytes|%" PRIu64 "\nbaseline_cycles|%" PRIu64 "\nbaseline_latency_ns|%" PRIu64 "\n"
           "baseline_image_sha256|%s\nbaseline_output_sha256|%s\n"
           "candidate_commit|%s\ncandidate_tree|%s\ncandidate_bytes|%" PRIu64 "\ncandidate_cycles|%" PRIu64 "\ncandidate_latency_ns|%" PRIu64 "\n"
           "candidate_image_sha256|%s\ncandidate_output_sha256|%s\n"
           "decision|%s\nscope|local comparison only; image manifest commit/tree declarations do not establish source-to-image derivation; "
           "tradeoff text is an unverified proposal; latency and toolchain identity are runner-reported; "
           "no independent measurement attestation, repository-owner approval, global optimality, or fresh verification claim\n",
           options.workload, result.input_sha256, result.runner_sha256, result.toolchain_sha256,
           options.proof_kind, result.runner_timeout_ms,
           result.max_bytes, result.max_cycles, result.max_latency_ns,
           result.baseline_commit, result.baseline_tree, result.baseline_bytes, result.baseline_cycles,
           result.baseline_latency_ns, result.baseline_image_sha256, result.baseline_output_sha256,
           result.candidate_commit, result.candidate_tree, result.candidate_bytes, result.candidate_cycles,
           result.candidate_latency_ns, result.candidate_image_sha256, result.candidate_output_sha256,
           result.local_pareto_improvement ? "local Pareto improvement" : "documented tradeoff proposal");
    return EXIT_SUCCESS;
}
