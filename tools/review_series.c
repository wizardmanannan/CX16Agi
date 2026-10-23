#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "review_series.h"

#include "review_format.h"
#include "review_sha256.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERIES_MAX_PRS 128
#define SERIES_MAX_COMMITS 4096
#define SERIES_MAX_DEPENDENCIES 2048
#define SERIES_MAX_DECISIONS 8192
#define SERIES_MAX_LINE 8192
#define SERIES_MAX_CAPTURE (16 * 1024 * 1024)
#define SERIES_PATH 4096

typedef struct {
    char id[64];
    char base[65];
    char head[65];
    char evidence_path[SERIES_PATH];
    char evidence_digest[65];
} SeriesPr;

typedef struct {
    char sha[65];
    char pr_id[64];
} SeriesCommit;

typedef struct {
    char pr_id[64];
    char prerequisite_id[64];
} SeriesDependency;

typedef enum {
    DECISION_FOLD,
    DECISION_SPLIT,
    DECISION_EXCLUDE
} SeriesDecisionKind;

typedef struct {
    SeriesDecisionKind kind;
    char sha[65];
    char target[65];
    char path[SERIES_PATH];
    char label[64];
    char reason[512];
} SeriesDecision;

typedef struct {
    char base[65];
    char head[65];
    SeriesPr prs[SERIES_MAX_PRS];
    size_t pr_count;
    SeriesCommit commits[SERIES_MAX_COMMITS];
    size_t commit_count;
    SeriesDependency dependencies[SERIES_MAX_DEPENDENCIES];
    size_t dependency_count;
    SeriesDecision *decisions;
    size_t decision_count;
    size_t decision_capacity;
} SeriesManifest;

static void set_error(char *error, size_t capacity, const char *format, ...)
{
    if (!error || !capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, capacity, format, args);
    va_end(args);
}

static bool copy_field(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (!length || length >= capacity) return false;
    memcpy(destination, source, length + 1);
    return true;
}

static SeriesDecision *append_decision(SeriesManifest *manifest)
{
    if (manifest->decision_count == SERIES_MAX_DECISIONS) return NULL;
    if (manifest->decision_count == manifest->decision_capacity) {
        size_t capacity = manifest->decision_capacity ? manifest->decision_capacity * 2 : 16;
        if (capacity > SERIES_MAX_DECISIONS) capacity = SERIES_MAX_DECISIONS;
        SeriesDecision *decisions = realloc(manifest->decisions, capacity * sizeof(*decisions));
        if (!decisions) return NULL;
        manifest->decisions = decisions;
        manifest->decision_capacity = capacity;
    }
    SeriesDecision *decision = &manifest->decisions[manifest->decision_count++];
    memset(decision, 0, sizeof(*decision));
    return decision;
}

static bool is_sha(const char *text)
{
    size_t length = strlen(text);
    if (length != 40 && length != 64) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

static bool is_digest(const char *text)
{
    if (strlen(text) != 64) return false;
    for (size_t i = 0; i < 64; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

static bool is_identifier(const char *text)
{
    if (!*text || !strcmp(text, ".") || !strcmp(text, "..")) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (!(isalnum(*at) || *at == '-' || *at == '_' || *at == '.')) return false;
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

static int find_pr(const SeriesManifest *manifest, const char *id)
{
    for (size_t i = 0; i < manifest->pr_count; ++i)
        if (!strcmp(manifest->prs[i].id, id)) return (int)i;
    return -1;
}

static int find_commit(const SeriesManifest *manifest, const char *sha)
{
    for (size_t i = 0; i < manifest->commit_count; ++i)
        if (!strcmp(manifest->commits[i].sha, sha)) return (int)i;
    return -1;
}

static bool valid_reason(const char *text)
{
    if (!*text) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < 0x20 || *at == 0x7f) return false;
    return true;
}

static bool valid_repo_path(const char *path)
{
    if (!*path || path[0] == '/' || path[0] == '-') return false;
    const char *component = path;
    for (const char *at = path;; ++at) {
        if (*at && *at != '/') {
            if ((unsigned char)*at < 0x20 || *at == 0x7f) return false;
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

static bool parse_manifest(const char *path, SeriesManifest *manifest, char *error, size_t error_capacity)
{
    memset(manifest, 0, sizeof(*manifest));
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_capacity, "cannot open manifest %s: %s", path, strerror(errno));
        return false;
    }

    char *line = NULL;
    size_t allocated = 0;
    ssize_t length;
    size_t line_number = 0;
    bool header = false, range = false, failed = false;
    while ((length = getline(&line, &allocated, file)) >= 0) {
        ++line_number;
        if ((size_t)length > SERIES_MAX_LINE) {
            set_error(error, error_capacity, "manifest line %zu is too long", line_number);
            failed = true;
            break;
        }
        if (!length || line[length - 1] != '\n') {
            set_error(error, error_capacity, "truncated manifest line %zu", line_number);
            failed = true;
            break;
        }
        if (memchr(line, '\0', (size_t)length)) {
            set_error(error, error_capacity, "manifest line %zu contains an embedded NUL", line_number);
            failed = true;
            break;
        }
        line[--length] = '\0';
        if (strchr(line, '\r') || !*line) {
            set_error(error, error_capacity, "manifest line %zu is empty or has CR characters", line_number);
            failed = true;
            break;
        }
        if (!header) {
            if (strcmp(line, "review-series-v1")) {
                set_error(error, error_capacity, "unsupported review series version on line %zu", line_number);
                failed = true;
                break;
            }
            header = true;
            continue;
        }

        char *fields[8];
        int count = split_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        if (count < 0) {
            set_error(error, error_capacity, "malformed fields on manifest line %zu", line_number);
            failed = true;
            break;
        }
        if (!strcmp(fields[0], "range")) {
            if (count != 3 || range || !is_sha(fields[1]) || !is_sha(fields[2]) ||
                !copy_field(manifest->base, sizeof(manifest->base), fields[1]) ||
                !copy_field(manifest->head, sizeof(manifest->head), fields[2])) {
                set_error(error, error_capacity, "invalid or duplicate range on manifest line %zu", line_number);
                failed = true;
                break;
            }
            range = true;
        } else if (!strcmp(fields[0], "pr")) {
            if (count != 6 || manifest->pr_count == SERIES_MAX_PRS || !is_identifier(fields[1]) ||
                find_pr(manifest, fields[1]) >= 0 || !is_sha(fields[2]) || !is_sha(fields[3]) ||
                !is_digest(fields[5])) {
                set_error(error, error_capacity, "invalid or duplicate PR on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesPr *pr = &manifest->prs[manifest->pr_count++];
            if (!copy_field(pr->id, sizeof(pr->id), fields[1]) ||
                !copy_field(pr->base, sizeof(pr->base), fields[2]) ||
                !copy_field(pr->head, sizeof(pr->head), fields[3]) ||
                !copy_field(pr->evidence_path, sizeof(pr->evidence_path), fields[4]) ||
                !copy_field(pr->evidence_digest, sizeof(pr->evidence_digest), fields[5])) {
                set_error(error, error_capacity, "oversized PR field on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else if (!strcmp(fields[0], "requires")) {
            if (count != 3 || manifest->dependency_count == SERIES_MAX_DEPENDENCIES ||
                !is_identifier(fields[1]) || !is_identifier(fields[2])) {
                set_error(error, error_capacity, "invalid dependency on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesDependency *dependency = &manifest->dependencies[manifest->dependency_count++];
            if (!copy_field(dependency->pr_id, sizeof(dependency->pr_id), fields[1]) ||
                !copy_field(dependency->prerequisite_id, sizeof(dependency->prerequisite_id), fields[2])) {
                set_error(error, error_capacity, "oversized dependency on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else if (!strcmp(fields[0], "commit")) {
            if (count != 3 || manifest->commit_count == SERIES_MAX_COMMITS || !is_sha(fields[1]) ||
                !is_identifier(fields[2]) || find_commit(manifest, fields[1]) >= 0) {
                set_error(error, error_capacity, "invalid or duplicate commit on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesCommit *commit = &manifest->commits[manifest->commit_count++];
            if (!copy_field(commit->sha, sizeof(commit->sha), fields[1]) ||
                !copy_field(commit->pr_id, sizeof(commit->pr_id), fields[2])) {
                set_error(error, error_capacity, "oversized commit field on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else if (!strcmp(fields[0], "fold")) {
            if (count != 4 ||
                !is_sha(fields[1]) || !is_sha(fields[2]) || !valid_reason(fields[3])) {
                set_error(error, error_capacity, "invalid fold on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesDecision *decision = append_decision(manifest);
            if (!decision) {
                set_error(error, error_capacity, "too many review decisions or insufficient memory");
                failed = true;
                break;
            }
            decision->kind = DECISION_FOLD;
            if (!copy_field(decision->sha, sizeof(decision->sha), fields[1]) ||
                !copy_field(decision->target, sizeof(decision->target), fields[2]) ||
                !copy_field(decision->reason, sizeof(decision->reason), fields[3])) {
                set_error(error, error_capacity, "oversized fold on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else if (!strcmp(fields[0], "split")) {
            if (count != 4 ||
                !is_sha(fields[1]) || !valid_repo_path(fields[2]) || !is_identifier(fields[3])) {
                set_error(error, error_capacity, "invalid split on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesDecision *decision = append_decision(manifest);
            if (!decision) {
                set_error(error, error_capacity, "too many review decisions or insufficient memory");
                failed = true;
                break;
            }
            decision->kind = DECISION_SPLIT;
            if (!copy_field(decision->sha, sizeof(decision->sha), fields[1]) ||
                !copy_field(decision->path, sizeof(decision->path), fields[2]) ||
                !copy_field(decision->label, sizeof(decision->label), fields[3])) {
                set_error(error, error_capacity, "oversized split on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else if (!strcmp(fields[0], "exclude")) {
            if (count != 3 ||
                !is_sha(fields[1]) || !valid_reason(fields[2])) {
                set_error(error, error_capacity, "invalid exclusion on manifest line %zu", line_number);
                failed = true;
                break;
            }
            SeriesDecision *decision = append_decision(manifest);
            if (!decision) {
                set_error(error, error_capacity, "too many review decisions or insufficient memory");
                failed = true;
                break;
            }
            decision->kind = DECISION_EXCLUDE;
            if (!copy_field(decision->sha, sizeof(decision->sha), fields[1]) ||
                !copy_field(decision->reason, sizeof(decision->reason), fields[2])) {
                set_error(error, error_capacity, "oversized exclusion on manifest line %zu", line_number);
                failed = true;
                break;
            }
        } else {
            set_error(error, error_capacity, "unsupported record type on manifest line %zu", line_number);
            failed = true;
            break;
        }
    }
    if (ferror(file)) {
        set_error(error, error_capacity, "cannot read manifest %s", path);
        failed = true;
    }
    free(line);
    if (fclose(file)) {
        set_error(error, error_capacity, "cannot close manifest %s", path);
        failed = true;
    }
    if (!failed && (!header || !range || !manifest->pr_count || !manifest->commit_count)) {
        set_error(error, error_capacity, "manifest requires a version, range, PRs, and commits");
        failed = true;
    }
    return !failed;
}

static int run_git(const char *repository, char *const arguments[], const char *stdout_path,
                   bool append, char **captured, char *error, size_t error_capacity)
{
    int pipe_fds[2] = { -1, -1 };
    if (captured && pipe(pipe_fds)) {
        set_error(error, error_capacity, "cannot create Git output pipe: %s", strerror(errno));
        return -1;
    }
    pid_t child = fork();
    if (child < 0) {
        if (pipe_fds[0] >= 0) close(pipe_fds[0]);
        if (pipe_fds[1] >= 0) close(pipe_fds[1]);
        set_error(error, error_capacity, "cannot fork Git: %s", strerror(errno));
        return -1;
    }
    if (!child) {
        if (captured) {
            close(pipe_fds[0]);
            if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _Exit(125);
            close(pipe_fds[1]);
        } else if (stdout_path) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int descriptor = open(stdout_path, flags, 0600);
            if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0) _Exit(125);
            close(descriptor);
        }
        char *argv[72];
        size_t count = 0;
        argv[count++] = (char *)"git";
        argv[count++] = (char *)"-C";
        argv[count++] = (char *)repository;
        for (size_t i = 0; arguments[i]; ++i) {
            if (count + 1 >= sizeof(argv) / sizeof(argv[0])) _Exit(125);
            argv[count++] = arguments[i];
        }
        argv[count] = NULL;
        execvp(argv[0], argv);
        _Exit(127);
    }
    if (captured) close(pipe_fds[1]);

    char *output = NULL;
    size_t used = 0, capacity = 0;
    bool too_large = false, allocation_failed = false;
    if (captured) {
        char buffer[8192];
        for (;;) {
            ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            if (used + (size_t)count > SERIES_MAX_CAPTURE) {
                too_large = true;
                continue;
            }
            if (!allocation_failed && used + (size_t)count + 1 > capacity) {
                size_t next = capacity ? capacity * 2 : 8192;
                while (next < used + (size_t)count + 1) next *= 2;
                char *grown = realloc(output, next);
                if (!grown) allocation_failed = true;
                else { output = grown; capacity = next; }
            }
            if (!allocation_failed) {
                memcpy(output + used, buffer, (size_t)count);
                used += (size_t)count;
            }
        }
        close(pipe_fds[0]);
    }

    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        free(output);
        set_error(error, error_capacity, "cannot wait for Git: %s", strerror(errno));
        return -1;
    }
    if (allocation_failed || too_large) {
        free(output);
        set_error(error, error_capacity, "Git output exceeds the review series limit");
        return -1;
    }
    if (captured) {
        if (!output) output = calloc(1, 1);
        if (!output) {
            set_error(error, error_capacity, "cannot allocate Git output");
            return -1;
        }
        output[used] = '\0';
        *captured = output;
    }
    if (!WIFEXITED(status)) return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    return WEXITSTATUS(status);
}

static bool git_commit_exists(const char *repository, const char *sha, char *error, size_t error_capacity)
{
    char revision[80];
    if (snprintf(revision, sizeof(revision), "%s^{commit}", sha) >= (int)sizeof(revision)) return false;
    char *arguments[] = { (char *)"cat-file", (char *)"-e", revision, NULL };
    return run_git(repository, arguments, NULL, false, NULL, error, error_capacity) == 0;
}

static bool git_ancestor(const char *repository, const char *ancestor, const char *descendant,
                         char *error, size_t error_capacity)
{
    char *arguments[] = { (char *)"merge-base", (char *)"--is-ancestor", (char *)ancestor,
        (char *)descendant, NULL };
    int status = run_git(repository, arguments, NULL, false, NULL, error, error_capacity);
    if (status == 0) return true;
    if (status == 1) return false;
    if (!error || !*error) set_error(error, error_capacity, "Git could not compare commit ancestry");
    return false;
}

static bool revision_list(const char *repository, const char *base, const char *head,
                          char ***revisions, size_t *count, char *error, size_t error_capacity)
{
    char range[132];
    if (snprintf(range, sizeof(range), "%s..%s", base, head) >= (int)sizeof(range)) return false;
    char *arguments[] = { (char *)"rev-list", (char *)"--reverse", (char *)"--topo-order", range, NULL };
    char *text = NULL;
    int status = run_git(repository, arguments, NULL, false, &text, error, error_capacity);
    if (status) {
        free(text);
        if (!error || !*error) set_error(error, error_capacity, "Git could not enumerate the requested revision range");
        return false;
    }
    size_t capacity = 16;
    char **items = calloc(capacity, sizeof(*items));
    if (!items) { free(text); set_error(error, error_capacity, "cannot allocate revision list"); return false; }
    char *cursor = text;
    while (*cursor) {
        char *end = strchr(cursor, '\n');
        if (!end) { free(text); free(items); set_error(error, error_capacity, "Git returned a malformed revision list"); return false; }
        *end = '\0';
        if (!is_sha(cursor) || *count == SERIES_MAX_COMMITS) {
            free(text); free(items); set_error(error, error_capacity, "Git returned an invalid or oversized revision list"); return false;
        }
        if (*count == capacity) {
            size_t next = capacity * 2;
            char **grown = realloc(items, next * sizeof(*items));
            if (!grown) { free(text); free(items); set_error(error, error_capacity, "cannot grow revision list"); return false; }
            items = grown;
            capacity = next;
        }
        items[*count] = strdup(cursor);
        if (!items[*count]) { free(text); for (size_t i = 0; i < *count; ++i) free(items[i]); free(items); set_error(error, error_capacity, "cannot copy Git revision"); return false; }
        ++*count;
        cursor = end + 1;
    }
    free(text);
    *revisions = items;
    return true;
}

static void free_revisions(char **revisions, size_t count)
{
    for (size_t i = 0; i < count; ++i) free(revisions[i]);
    free(revisions);
}

static bool parse_unsigned_field(const char *text, unsigned maximum, unsigned *value)
{
    if (!*text) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < '0' || *at > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno || !end || *end || parsed > maximum) return false;
    *value = (unsigned)parsed;
    return true;
}

static bool evidence_result_ok(char **fields, size_t count, size_t offset)
{
    unsigned values[4];
    if (count < offset + 4) return false;
    for (size_t i = 0; i < 4; ++i) {
        unsigned maximum = i == 1 ? 127 : (i >= 2 ? 1 : 255);
        if (!parse_unsigned_field(fields[offset + i], maximum, &values[i])) return false;
    }
    return !values[0] && !values[1] && !values[2] && !values[3];
}

static bool evidence_log_matches(const char *root, const char *relative, const char *expected)
{
    if (!valid_repo_path(relative) || !is_digest(expected)) return false;
    char current[SERIES_PATH];
    if (!copy_field(current, sizeof(current), root)) return false;
    char path_copy[SERIES_PATH];
    if (!copy_field(path_copy, sizeof(path_copy), relative)) return false;
    char *save = NULL;
    char *part = strtok_r(path_copy, "/", &save);
    while (part) {
        char next[SERIES_PATH];
        if (snprintf(next, sizeof(next), "%s/%s", current, part) >= (int)sizeof(next)) return false;
        struct stat info;
        if (lstat(next, &info) || S_ISLNK(info.st_mode)) return false;
        char *following = strtok_r(NULL, "/", &save);
        if (following ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)) return false;
        strcpy(current, next);
        part = following;
    }
    char actual[65];
    return !review_sha256_file(current, actual) && !strcmp(actual, expected);
}

static bool evidence_record(char **fields, size_t count, const char *key)
{
    return count == 2 && !strcmp(fields[0], key);
}

static bool evidence_git_tree(const char *repository, const char *revision, const char *tree,
                              char *error, size_t error_capacity)
{
    char expression[80];
    if (snprintf(expression, sizeof(expression), "%s^{tree}", revision) >= (int)sizeof(expression)) return false;
    char *arguments[] = { (char *)"rev-parse", (char *)"--verify", expression, NULL };
    char *actual = NULL;
    int status = run_git(repository, arguments, NULL, false, &actual, error, error_capacity);
    if (status) {
        free(actual);
        return false;
    }
    actual[strcspn(actual, "\r\n")] = '\0';
    bool matches = is_sha(actual) && !strcmp(actual, tree);
    free(actual);
    return matches;
}

static bool evidence_packet_matches(const char *root, const char *expected_sha)
{
    char path[SERIES_PATH];
    if (snprintf(path, sizeof(path), "%s/%s", root, REVIEW_EVIDENCE_PACKET_FILENAME) >= (int)sizeof(path)) return false;
    struct stat info;
    char actual_sha[65];
    if (lstat(path, &info) || !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) ||
        info.st_size < 0 || (uintmax_t)info.st_size > SERIES_MAX_CAPTURE ||
        review_sha256_file(path, actual_sha) || strcmp(actual_sha, expected_sha)) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t size = (size_t)info.st_size;
    char *contents = malloc(size + 1);
    if (!contents) { fclose(file); return false; }
    bool read_ok = fread(contents, 1, size, file) == size && !ferror(file);
    bool close_ok = fclose(file) == 0;
    bool ok = read_ok && close_ok;
    if (ok) {
        contents[size] = '\0';
        ok = !memchr(contents, '\0', size) && strstr(contents, "baseline symptom reached") &&
             strstr(contents, "candidate passed");
    }
    free(contents);
    return ok;
}

static bool validate_evidence(const char *repository, const SeriesPr *pr, const char *manifest_path,
                              char *error, size_t error_capacity)
{
    char evidence_root[SERIES_PATH];
    if (pr->evidence_path[0] == '/') {
        if (!copy_field(evidence_root, sizeof(evidence_root), pr->evidence_path)) {
            set_error(error, error_capacity, "evidence path is too long for PR %s", pr->id);
            return false;
        }
    } else {
        const char *slash = strrchr(manifest_path, '/');
        if (slash) {
            size_t prefix = (size_t)(slash - manifest_path + 1);
            if (prefix + strlen(pr->evidence_path) >= sizeof(evidence_root)) {
                set_error(error, error_capacity, "evidence path is too long for PR %s", pr->id);
                return false;
            }
            memcpy(evidence_root, manifest_path, prefix);
            strcpy(evidence_root + prefix, pr->evidence_path);
        } else if (!copy_field(evidence_root, sizeof(evidence_root), pr->evidence_path)) {
            set_error(error, error_capacity, "evidence path is too long for PR %s", pr->id);
            return false;
        }
    }
    struct stat root_info;
    char resolved_root[SERIES_PATH];
    if (lstat(evidence_root, &root_info) || !S_ISDIR(root_info.st_mode) || S_ISLNK(root_info.st_mode) ||
        !realpath(evidence_root, resolved_root)) {
        set_error(error, error_capacity, "PR %s evidence directory is missing or unsafe", pr->id);
        return false;
    }
    char evidence_file[SERIES_PATH];
    if (snprintf(evidence_file, sizeof(evidence_file), "%s/%s", resolved_root,
                 REVIEW_EVIDENCE_FILENAME) >= (int)sizeof(evidence_file)) {
        set_error(error, error_capacity, "PR %s evidence path is too long", pr->id);
        return false;
    }
    struct stat evidence_info;
    char actual_digest[65];
    if (lstat(evidence_file, &evidence_info) || !S_ISREG(evidence_info.st_mode) || S_ISLNK(evidence_info.st_mode) ||
        evidence_info.st_size <= 0 || (uintmax_t)evidence_info.st_size > SERIES_MAX_CAPTURE ||
        review_sha256_file(evidence_file, actual_digest) || strcmp(actual_digest, pr->evidence_digest)) {
        set_error(error, error_capacity, "PR %s evidence digest mismatch", pr->id);
        return false;
    }

    FILE *file = fopen(evidence_file, "rb");
    if (!file) { set_error(error, error_capacity, "cannot open PR %s evidence", pr->id); return false; }
    char *line = NULL;
    size_t allocated = 0;
    ssize_t length;
    static const char *const keys[] = { REVIEW_EVIDENCE_RECORD_PLAN_SHA, REVIEW_EVIDENCE_RECORD_REPO,
        REVIEW_EVIDENCE_RECORD_CANDIDATE, REVIEW_EVIDENCE_RECORD_TREE, REVIEW_EVIDENCE_RECORD_CLEAN,
        REVIEW_EVIDENCE_RECORD_TOOLCHAIN_PATH, REVIEW_EVIDENCE_RECORD_TOOLCHAIN_SHA,
        REVIEW_EVIDENCE_RECORD_TOOLCHAIN_VERSION_SHA, REVIEW_EVIDENCE_RECORD_REVIEW_TOOL_SHA };
    unsigned stage = 0, tools_remaining = 0, inputs_remaining = 0;
    unsigned suites_remaining = 0, witnesses_remaining = 0;
    bool malformed = false, saw_seal = false, expect_candidate = false;
    char baseline_name[128] = { 0 }, baseline_overlay[65] = { 0 };
    char candidate_tree[65] = { 0 }, packet_sha[65] = { 0 }, resolved_repository[SERIES_PATH];
    unsigned char digest[32];
    char actual_seal[65];
    review_sha256 seal_hash;
    review_sha256_init(&seal_hash);
    if (!realpath(repository, resolved_repository)) {
        fclose(file);
        set_error(error, error_capacity, "cannot resolve series repository for evidence");
        return false;
    }
    while ((length = getline(&line, &allocated, file)) >= 0) {
        if ((size_t)length > SERIES_MAX_LINE || !length || line[length - 1] != '\n' ||
            memchr(line, '\0', (size_t)length)) { malformed = true; break; }
        bool seal_line = stage == 19;
        if (!seal_line) review_sha256_update(&seal_hash, line, (size_t)length);
        line[--length] = '\0';
        if (strchr(line, '\r') || !*line) { malformed = true; break; }
        char *fields[32];
        int field_count = split_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        if (field_count < 0) { malformed = true; break; }
        size_t count = (size_t)field_count;
        if (stage == 0) {
            malformed = count != 1 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_MAGIC);
            stage = 1;
        } else if (stage <= 9) {
            const char *key = keys[stage - 1];
            if (!evidence_record(fields, count, key)) malformed = true;
            else if (stage == 1 || stage == 6 || stage == 7 || stage == 8 || stage == 9) {
                if (stage == 1 || stage >= 7) malformed = !is_digest(fields[1]);
                else malformed = fields[1][0] != '/';
            } else if (stage == 2) {
                malformed = fields[1][0] != '/' || strcmp(fields[1], resolved_repository);
            } else if (stage == 3) {
                malformed = !is_sha(fields[1]) || strcmp(fields[1], pr->head);
            } else if (stage == 4) malformed = !is_sha(fields[1]);
            else if (stage == 5) malformed = strcmp(fields[1], "yes") != 0;
            if (!malformed && stage == 4) {
                if (!copy_field(candidate_tree, sizeof(candidate_tree), fields[1]) ||
                    !evidence_git_tree(repository, pr->head, candidate_tree, error, error_capacity)) {
                    set_error(error, error_capacity, "PR %s candidate tree does not match its real Git commit", pr->id);
                    malformed = true;
                }
            }
            ++stage;
        } else if (stage == 10) {
            malformed = !evidence_record(fields, count, REVIEW_EVIDENCE_RECORD_TOOL_COUNT) ||
                        !parse_unsigned_field(fields[1], REVIEW_EVIDENCE_MAX_TOOL_FACTS, &tools_remaining);
            stage = tools_remaining ? 11 : 12;
        } else if (stage == 11 && tools_remaining) {
            malformed = count != 5 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_TOOL) || !is_identifier(fields[1]) ||
                        fields[2][0] != '/' || !is_digest(fields[3]) || !is_digest(fields[4]);
            if (tools_remaining) --tools_remaining;
            if (!tools_remaining) stage = 12;
        } else if (stage == 12) {
            malformed = !evidence_record(fields, count, REVIEW_EVIDENCE_RECORD_INPUT_COUNT) ||
                        !parse_unsigned_field(fields[1], REVIEW_EVIDENCE_MAX_INPUT_FACTS, &inputs_remaining);
            stage = inputs_remaining ? 13 : 14;
        } else if (stage == 13 && inputs_remaining) {
            malformed = count != 4 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_INPUT) || !is_identifier(fields[1]) ||
                        fields[2][0] != '/' || !is_digest(fields[3]);
            if (inputs_remaining) --inputs_remaining;
            if (!inputs_remaining) stage = 14;
        } else if (stage == 14) {
            malformed = !evidence_record(fields, count, REVIEW_EVIDENCE_RECORD_SUITE_COUNT) ||
                        !parse_unsigned_field(fields[1], REVIEW_EVIDENCE_MAX_SUITE_FACTS, &suites_remaining) || !suites_remaining;
            stage = 15;
        } else if (stage == 15 && suites_remaining) {
            malformed = count != 9 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_SUITE) || !is_identifier(fields[1]) ||
                        !evidence_result_ok(fields, count, 2) || !is_digest(fields[8]) ||
                        !evidence_log_matches(resolved_root, fields[6], fields[7]);
            if (suites_remaining) --suites_remaining;
            if (!suites_remaining) stage = 16;
        } else if (stage == 16) {
            malformed = !evidence_record(fields, count, REVIEW_EVIDENCE_RECORD_WITNESS_COUNT) ||
                        !parse_unsigned_field(fields[1], REVIEW_EVIDENCE_MAX_WITNESS_FACTS, &witnesses_remaining) ||
                        witnesses_remaining < 2 || witnesses_remaining % 2;
            stage = 17;
        } else if (stage == 17 && witnesses_remaining) {
            unsigned run_exit = 0, run_signal = 0, timed_out = 0, skipped = 0;
            malformed = count != 19 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_WITNESS) || !is_identifier(fields[1]) ||
                        !is_sha(fields[3]) || !is_sha(fields[4]) || !evidence_result_ok(fields, count, 5) ||
                        !is_digest(fields[17]) || !is_digest(fields[18]) ||
                        !evidence_log_matches(resolved_root, fields[9], fields[10]) ||
                        !is_digest(fields[16]) || !evidence_log_matches(resolved_root, fields[15], fields[16]);
            if (!malformed && !expect_candidate) {
                malformed = strcmp(fields[2], "baseline") ||
                            !copy_field(baseline_name, sizeof(baseline_name), fields[1]) ||
                            !is_digest(fields[18]) ||
                            !parse_unsigned_field(fields[11], 255, &run_exit) || !run_exit ||
                            !parse_unsigned_field(fields[12], 127, &run_signal) || run_signal ||
                            !parse_unsigned_field(fields[13], 1, &timed_out) || timed_out ||
                            !parse_unsigned_field(fields[14], 1, &skipped) || skipped ||
                            !evidence_git_tree(repository, fields[3], fields[4], error, error_capacity) ||
                            !copy_field(baseline_overlay, sizeof(baseline_overlay), fields[18]);
                if (!malformed && (!strcmp(fields[3], pr->head) ||
                    !git_ancestor(repository, fields[3], pr->head, error, error_capacity))) {
                    if (!error || !*error)
                        set_error(error, error_capacity,
                                  "PR %s regression witness baseline is not an earlier ancestor of its tested head",
                                  pr->id);
                    malformed = true;
                }
                expect_candidate = !malformed;
            } else if (!malformed) {
                malformed = strcmp(fields[2], "candidate") || strcmp(fields[1], baseline_name) ||
                            strcmp(fields[3], pr->head) ||
                            strcmp(fields[18], baseline_overlay) ||
                            !parse_unsigned_field(fields[11], 255, &run_exit) || run_exit ||
                            !parse_unsigned_field(fields[12], 127, &run_signal) || run_signal ||
                            !parse_unsigned_field(fields[13], 1, &timed_out) || timed_out ||
                            !parse_unsigned_field(fields[14], 1, &skipped) || skipped ||
                            !evidence_git_tree(repository, fields[3], fields[4], error, error_capacity);
                expect_candidate = false;
            }
            if (witnesses_remaining) --witnesses_remaining;
            if (!witnesses_remaining) stage = 18;
        } else if (stage == 18) {
            malformed = !evidence_record(fields, count, REVIEW_EVIDENCE_RECORD_PACKET_SHA) || !is_digest(fields[1]) ||
                        !copy_field(packet_sha, sizeof(packet_sha), fields[1]);
            stage = 19;
        } else if (stage == 19) {
            if (count != 2 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_SEAL) || !is_digest(fields[1])) malformed = true;
            else {
                review_sha256_final(&seal_hash, digest);
                review_sha256_hex(digest, actual_seal);
                malformed = strcmp(actual_seal, fields[1]) != 0;
            }
            saw_seal = !malformed;
            stage = 20;
            if (!malformed) {
                int trailing = fgetc(file);
                if (trailing != EOF || ferror(file)) malformed = true;
            }
        } else {
            malformed = true;
        }
        if (malformed) break;
    }
    if (ferror(file) || !saw_seal || stage != 20 || expect_candidate ||
        !evidence_packet_matches(resolved_root, packet_sha)) malformed = true;
    if (fclose(file)) malformed = true;
    free(line);
    if (malformed) {
        if (!error || !*error)
            set_error(error, error_capacity, "PR %s evidence is not a passing report for its exact head", pr->id);
        return false;
    }
    return true;
}

static bool validate_graph(const char *repository, const SeriesManifest *manifest, const char *manifest_path,
                           size_t order[SERIES_MAX_PRS], char *error, size_t error_capacity)
{
    if (!git_commit_exists(repository, manifest->base, error, error_capacity) ||
        !git_commit_exists(repository, manifest->head, error, error_capacity)) {
        if (!error || !*error) set_error(error, error_capacity, "series range refers to a missing Git commit");
        return false;
    }
    if (!git_ancestor(repository, manifest->base, manifest->head, error, error_capacity) ||
        !strcmp(manifest->base, manifest->head)) {
        if (!error || !*error) set_error(error, error_capacity, "series head must descend from its base");
        return false;
    }

    bool edges[SERIES_MAX_PRS][SERIES_MAX_PRS] = { { false } };
    size_t indegree[SERIES_MAX_PRS] = { 0 };
    for (size_t i = 0; i < manifest->dependency_count; ++i) {
        const SeriesDependency *dependency = &manifest->dependencies[i];
        int pr = find_pr(manifest, dependency->pr_id);
        int prerequisite = find_pr(manifest, dependency->prerequisite_id);
        if (pr < 0 || prerequisite < 0 || pr == prerequisite || edges[prerequisite][pr]) {
            set_error(error, error_capacity, "missing or duplicate PR prerequisite %s -> %s",
                      dependency->pr_id, dependency->prerequisite_id);
            return false;
        }
        edges[prerequisite][pr] = true;
        ++indegree[pr];
    }

    bool emitted[SERIES_MAX_PRS] = { false };
    size_t ordered = 0;
    while (ordered < manifest->pr_count) {
        size_t next = manifest->pr_count;
        for (size_t i = 0; i < manifest->pr_count; ++i)
            if (!emitted[i] && !indegree[i]) { next = i; break; }
        if (next == manifest->pr_count) {
            set_error(error, error_capacity, "review series has a prerequisite cycle");
            return false;
        }
        emitted[next] = true;
        order[ordered++] = next;
        for (size_t i = 0; i < manifest->pr_count; ++i) if (edges[next][i]) --indegree[i];
    }

    bool head_found = false;
    for (size_t i = 0; i < manifest->pr_count; ++i) {
        const SeriesPr *pr = &manifest->prs[i];
        if (!git_commit_exists(repository, pr->base, error, error_capacity) ||
            !git_commit_exists(repository, pr->head, error, error_capacity)) {
            if (!error || !*error) set_error(error, error_capacity, "PR %s refers to a missing Git commit", pr->id);
            return false;
        }
        if (!git_ancestor(repository, pr->base, pr->head, error, error_capacity) ||
            !strcmp(pr->base, pr->head)) {
            if (!error || !*error) set_error(error, error_capacity, "PR %s head must descend from its base", pr->id);
            return false;
        }
        size_t dependency_count = 0;
        bool base_is_dependency = false;
        for (size_t d = 0; d < manifest->dependency_count; ++d) {
            if (strcmp(manifest->dependencies[d].pr_id, pr->id)) continue;
            ++dependency_count;
            int prerequisite = find_pr(manifest, manifest->dependencies[d].prerequisite_id);
            if (prerequisite >= 0) {
                if (!git_ancestor(repository, manifest->prs[prerequisite].head, pr->base, error, error_capacity)) {
                    set_error(error, error_capacity, "PR %s base does not include prerequisite %s",
                              pr->id, manifest->prs[prerequisite].id);
                    return false;
                }
                if (!strcmp(manifest->prs[prerequisite].head, pr->base)) base_is_dependency = true;
            }
        }
        if ((!dependency_count && strcmp(pr->base, manifest->base)) ||
            (dependency_count && !base_is_dependency)) {
            set_error(error, error_capacity, "PR %s base does not match its declared prerequisite route", pr->id);
            return false;
        }
        if (!strcmp(pr->head, manifest->head)) head_found = true;
        if (!validate_evidence(repository, pr, manifest_path, error, error_capacity)) return false;
    }
    if (!head_found) {
        set_error(error, error_capacity, "series head must be one of the declared, evidence-bound PR heads");
        return false;
    }
    for (size_t i = 0; i < manifest->pr_count; ++i) {
        if (!git_ancestor(repository, manifest->prs[i].head, manifest->head, error, error_capacity)) {
            set_error(error, error_capacity, "declared PR head is not included in the series head");
            return false;
        }
    }
    return true;
}

static bool validate_accounting(const char *repository, const SeriesManifest *manifest,
                                char ***global_revisions, size_t *global_count,
                                char *error, size_t error_capacity)
{
    if (!revision_list(repository, manifest->base, manifest->head, global_revisions, global_count,
                       error, error_capacity)) return false;
    if (*global_count != manifest->commit_count) {
        set_error(error, error_capacity, "manifest commit accounting is incomplete for the declared Git range");
        return false;
    }
    for (size_t i = 0; i < *global_count; ++i) {
        int declared = find_commit(manifest, (*global_revisions)[i]);
        if (declared < 0 || find_pr(manifest, manifest->commits[declared].pr_id) < 0) {
            set_error(error, error_capacity, "Git commit %s is unaccounted or names a missing PR", (*global_revisions)[i]);
            return false;
        }
    }
    for (size_t i = 0; i < manifest->commit_count; ++i) {
        bool found = false;
        for (size_t r = 0; r < *global_count; ++r)
            if (!strcmp(manifest->commits[i].sha, (*global_revisions)[r])) { found = true; break; }
        if (!found) {
            set_error(error, error_capacity, "manifest commit %s is outside the declared Git range", manifest->commits[i].sha);
            return false;
        }
        const SeriesPr *owner = &manifest->prs[find_pr(manifest, manifest->commits[i].pr_id)];
        char **pr_revisions = NULL;
        size_t pr_count = 0;
        if (!revision_list(repository, owner->base, owner->head, &pr_revisions, &pr_count,
                           error, error_capacity)) return false;
        bool belongs = false;
        for (size_t r = 0; r < pr_count; ++r)
            if (!strcmp(manifest->commits[i].sha, pr_revisions[r])) { belongs = true; break; }
        free_revisions(pr_revisions, pr_count);
        if (!belongs) {
            set_error(error, error_capacity, "commit %s is not in its declared PR range", manifest->commits[i].sha);
            return false;
        }
    }
    for (size_t p = 0; p < manifest->pr_count; ++p) {
        char **pr_revisions = NULL;
        size_t pr_count = 0;
        if (!revision_list(repository, manifest->prs[p].base, manifest->prs[p].head, &pr_revisions, &pr_count,
                           error, error_capacity)) return false;
        for (size_t r = 0; r < pr_count; ++r) {
            int declared = find_commit(manifest, pr_revisions[r]);
            if (declared < 0 || strcmp(manifest->commits[declared].pr_id, manifest->prs[p].id)) {
                set_error(error, error_capacity, "PR %s does not account for every commit in its Git range",
                          manifest->prs[p].id);
                free_revisions(pr_revisions, pr_count);
                return false;
            }
        }
        if (!pr_count) {
            set_error(error, error_capacity, "PR %s has no commits in its declared Git range", manifest->prs[p].id);
            free_revisions(pr_revisions, pr_count);
            return false;
        }
        free_revisions(pr_revisions, pr_count);
    }
    return true;
}

static int decision_type(const SeriesManifest *manifest, const char *sha)
{
    for (size_t i = 0; i < manifest->decision_count; ++i)
        if (!strcmp(manifest->decisions[i].sha, sha)) return (int)manifest->decisions[i].kind;
    return -1;
}

static size_t decision_count_for(const SeriesManifest *manifest, const char *sha)
{
    size_t count = 0;
    for (size_t i = 0; i < manifest->decision_count; ++i)
        if (!strcmp(manifest->decisions[i].sha, sha)) ++count;
    return count;
}

static size_t revision_position(char **revisions, size_t count, const char *sha)
{
    for (size_t i = 0; i < count; ++i) if (!strcmp(revisions[i], sha)) return i;
    return count;
}

static bool validate_split_paths(const char *repository, const SeriesManifest *manifest,
                                 const char *sha, char *error, size_t error_capacity)
{
    char *arguments[] = { (char *)"diff-tree", (char *)"--root", (char *)"--no-commit-id",
        (char *)"--first-parent", (char *)"--no-ext-diff", (char *)"--no-textconv",
        (char *)"--name-only", (char *)"-r", (char *)sha, NULL };
    char *changed = NULL;
    int status = run_git(repository, arguments, NULL, false, &changed, error, error_capacity);
    if (status) {
        free(changed);
        if (!error || !*error) set_error(error, error_capacity, "cannot inspect split commit %s", sha);
        return false;
    }
    size_t actual_count = 0;
    char *cursor = changed;
    while (*cursor) {
        char *end = strchr(cursor, '\n');
        if (!end) { free(changed); set_error(error, error_capacity, "Git returned malformed split paths"); return false; }
        *end = '\0';
        if (!*cursor || strchr(cursor, '\r')) {
            free(changed);
            set_error(error, error_capacity, "split commit %s has a path the manifest cannot represent", sha);
            return false;
        }
        size_t matches = 0;
        for (size_t i = 0; i < manifest->decision_count; ++i)
            if (manifest->decisions[i].kind == DECISION_SPLIT &&
                !strcmp(manifest->decisions[i].sha, sha) &&
                !strcmp(manifest->decisions[i].path, cursor)) ++matches;
        if (matches != 1) {
            free(changed);
            set_error(error, error_capacity, "split commit %s must assign every changed path exactly once", sha);
            return false;
        }
        ++actual_count;
        cursor = end + 1;
    }
    free(changed);
    size_t declared_count = 0;
    for (size_t i = 0; i < manifest->decision_count; ++i)
        if (manifest->decisions[i].kind == DECISION_SPLIT &&
            !strcmp(manifest->decisions[i].sha, sha)) ++declared_count;
    if (!actual_count || actual_count != declared_count) {
        set_error(error, error_capacity, "split commit %s does not completely account for its changed paths", sha);
        return false;
    }
    return true;
}

static bool validate_decisions(const char *repository, const SeriesManifest *manifest,
                               char **revisions, size_t revision_count,
                               char *error, size_t error_capacity)
{
    for (size_t i = 0; i < manifest->decision_count; ++i) {
        const SeriesDecision *decision = &manifest->decisions[i];
        if (find_commit(manifest, decision->sha) < 0) {
            set_error(error, error_capacity, "review decision refers to unaccounted commit %s", decision->sha);
            return false;
        }
        for (size_t j = 0; j < i; ++j) {
            const SeriesDecision *previous = &manifest->decisions[j];
            if (strcmp(previous->sha, decision->sha)) continue;
            if (decision->kind != DECISION_SPLIT || previous->kind != DECISION_SPLIT ||
                !strcmp(previous->path, decision->path)) {
                set_error(error, error_capacity, "commit %s has duplicate or conflicting review decisions", decision->sha);
                return false;
            }
        }
        if (decision->kind == DECISION_FOLD) {
            int source_index = find_commit(manifest, decision->sha);
            int target_index = find_commit(manifest, decision->target);
            if (source_index < 0 || target_index < 0 || source_index == target_index ||
                strcmp(manifest->commits[source_index].pr_id, manifest->commits[target_index].pr_id) ||
                decision_type(manifest, decision->target) >= 0 ||
                revision_position(revisions, revision_count, decision->target) >=
                    revision_position(revisions, revision_count, decision->sha)) {
                set_error(error, error_capacity, "fold %s must target an earlier kept commit in the same PR",
                          decision->sha);
                return false;
            }
        }
    }
    for (size_t c = 0; c < manifest->commit_count; ++c) {
        const char *sha = manifest->commits[c].sha;
        int kind = decision_type(manifest, sha);
        size_t count = decision_count_for(manifest, sha);
        if (kind == DECISION_SPLIT) {
            size_t unique_labels = 0;
            for (size_t i = 0; i < manifest->decision_count; ++i) {
                const SeriesDecision *decision = &manifest->decisions[i];
                if (decision->kind != DECISION_SPLIT || strcmp(decision->sha, sha)) continue;
                bool seen = false;
                for (size_t j = 0; j < i; ++j)
                    if (manifest->decisions[j].kind == DECISION_SPLIT &&
                        !strcmp(manifest->decisions[j].sha, sha) &&
                        !strcmp(manifest->decisions[j].label, decision->label)) seen = true;
                if (!seen) ++unique_labels;
            }
            if (count < 2 || unique_labels < 2 || !validate_split_paths(repository, manifest, sha, error, error_capacity)) {
                if (!error || !*error)
                    set_error(error, error_capacity, "split commit %s requires multiple complete review parts", sha);
                return false;
            }
        } else if (count > 1) {
            set_error(error, error_capacity, "commit %s has incomplete or conflicting review decisions", sha);
            return false;
        }
    }
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
        char child[SERIES_PATH];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            !remove_tree(child)) ok = false;
    }
    if (closedir(directory)) ok = false;
    if (ok && rmdir(path)) ok = false;
    return ok;
}

static bool create_patch_file(const char *path, char *error, size_t error_capacity)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        set_error(error, error_capacity, "cannot create patch file %s: %s", path, strerror(errno));
        return false;
    }
    if (fclose(file)) {
        set_error(error, error_capacity, "cannot close patch file %s", path);
        return false;
    }
    return true;
}

static bool append_commit_diff(const char *repository, const char *sha, const char *path_filter,
                               const char *patch_path, char *error, size_t error_capacity)
{
    char *arguments[14];
    size_t count = 0;
    arguments[count++] = (char *)"diff-tree";
    arguments[count++] = (char *)"--root";
    arguments[count++] = (char *)"--no-commit-id";
    arguments[count++] = (char *)"--first-parent";
    arguments[count++] = (char *)"--no-ext-diff";
    arguments[count++] = (char *)"--no-textconv";
    arguments[count++] = (char *)"--patch";
    arguments[count++] = (char *)"--binary";
    arguments[count++] = (char *)"--full-index";
    arguments[count++] = (char *)"-r";
    arguments[count++] = (char *)sha;
    if (path_filter) {
        arguments[count++] = (char *)"--";
        arguments[count++] = (char *)path_filter;
    }
    arguments[count] = NULL;
    if (run_git(repository, arguments, patch_path, true, NULL, error, error_capacity)) {
        if (!error || !*error) set_error(error, error_capacity, "cannot generate incremental diff for %s", sha);
        return false;
    }
    return true;
}

static bool has_default_patch(const SeriesManifest *manifest, const char *pr_id,
                              char **revisions, size_t revision_count)
{
    for (size_t r = 0; r < revision_count; ++r) {
        int commit = find_commit(manifest, revisions[r]);
        if (commit < 0 || strcmp(manifest->commits[commit].pr_id, pr_id)) continue;
        int kind = decision_type(manifest, revisions[r]);
        if (kind != DECISION_SPLIT && kind != DECISION_EXCLUDE) return true;
    }
    return false;
}

static bool write_route(const char *repository, const SeriesManifest *manifest,
                        char **revisions, size_t revision_count, const size_t order[SERIES_MAX_PRS],
                        const char *output_directory, char *error, size_t error_capacity)
{
    if (mkdir(output_directory, 0700)) {
        set_error(error, error_capacity, "cannot create route output %s: %s", output_directory, strerror(errno));
        return false;
    }
    char index_path[SERIES_PATH];
    if (snprintf(index_path, sizeof(index_path), "%s/route.md", output_directory) >= (int)sizeof(index_path)) {
        set_error(error, error_capacity, "route output path is too long");
        remove_tree(output_directory);
        return false;
    }
    FILE *index = fopen(index_path, "wb");
    if (!index) {
        set_error(error, error_capacity, "cannot create route index: %s", strerror(errno));
        remove_tree(output_directory);
        return false;
    }
    fprintf(index, "# Review route\n\nRange: `%s` to `%s`\n\n"
                   "This route checks local integrity and identity for the supplied evidence. "
                   "It is not approval or fresh verification authority; rerun the required suites at publication.\n",
            manifest->base, manifest->head);
    bool ok = !ferror(index);
    for (size_t position = 0; position < manifest->pr_count && ok; ++position) {
        const SeriesPr *pr = &manifest->prs[order[position]];
        char patch_path[SERIES_PATH], patch_name[256];
        int name_length = snprintf(patch_name, sizeof(patch_name), "%04zu-%s.patch", position + 1, pr->id);
        bool default_patch = has_default_patch(manifest, pr->id, revisions, revision_count);
        if (name_length < 0 || name_length >= (int)sizeof(patch_name) ||
            (default_patch && snprintf(patch_path, sizeof(patch_path), "%s/%s", output_directory, patch_name) >= (int)sizeof(patch_path))) {
            set_error(error, error_capacity, "route patch path is too long");
            ok = false;
            break;
        }
        if (default_patch && !create_patch_file(patch_path, error, error_capacity)) { ok = false; break; }
        for (size_t r = 0; r < revision_count; ++r) {
            int declared = find_commit(manifest, revisions[r]);
            if (declared < 0 || strcmp(manifest->commits[declared].pr_id, pr->id)) continue;
            int kind = decision_type(manifest, revisions[r]);
            if (kind == DECISION_EXCLUDE) continue;
            if (kind == DECISION_SPLIT) {
                for (size_t d = 0; d < manifest->decision_count; ++d) {
                    const SeriesDecision *decision = &manifest->decisions[d];
                    if (decision->kind != DECISION_SPLIT || strcmp(decision->sha, revisions[r])) continue;
                    char split_name[256], split_path[SERIES_PATH];
                    int split_length = snprintf(split_name, sizeof(split_name), "%04zu-%s-%.8s-%s.patch",
                                                position + 1, pr->id, decision->sha, decision->label);
                    if (split_length < 0 || split_length >= (int)sizeof(split_name) ||
                        snprintf(split_path, sizeof(split_path), "%s/%s", output_directory, split_name) >= (int)sizeof(split_path)) {
                        set_error(error, error_capacity, "split patch path is too long");
                        ok = false;
                        break;
                    }
                    bool first_label = true;
                    for (size_t prior = 0; prior < d; ++prior)
                        if (manifest->decisions[prior].kind == DECISION_SPLIT &&
                            !strcmp(manifest->decisions[prior].sha, decision->sha) &&
                            !strcmp(manifest->decisions[prior].label, decision->label)) first_label = false;
                    if (first_label && !create_patch_file(split_path, error, error_capacity)) { ok = false; break; }
                    if (!append_commit_diff(repository, decision->sha, decision->path,
                                            split_path, error, error_capacity)) { ok = false; break; }
                }
            } else if (!append_commit_diff(repository, revisions[r], NULL,
                                           patch_path, error, error_capacity)) {
                ok = false;
            }
            if (!ok) break;
        }
        if (!ok) break;
        fprintf(index, "\n## %zu. %s\n\n- Base: `%s`\n- Tested head: `%s`\n- Evidence SHA-256: `%s`\n",
                position + 1, pr->id, pr->base, pr->head, pr->evidence_digest);
        if (default_patch) fprintf(index, "- Incremental diff: `%s`\n", patch_name);
        for (size_t d = 0; d < manifest->dependency_count; ++d) {
            if (!strcmp(manifest->dependencies[d].pr_id, pr->id))
                fprintf(index, "- Requires: `%s`\n", manifest->dependencies[d].prerequisite_id);
        }
        for (size_t d = 0; d < manifest->decision_count; ++d) {
            const SeriesDecision *decision = &manifest->decisions[d];
            int commit = find_commit(manifest, decision->sha);
            if (commit < 0 || strcmp(manifest->commits[commit].pr_id, pr->id)) continue;
            if (decision->kind == DECISION_FOLD) {
                fprintf(index, "- Fold `%s` into `%s`: %s\n", decision->sha, decision->target, decision->reason);
            } else if (decision->kind == DECISION_SPLIT) {
                fprintf(index, "- Split `%s` path `%s` into `%s`\n", decision->sha, decision->path, decision->label);
                fprintf(index, "- Split diff: `%04zu-%s-%.8s-%s.patch`\n", position + 1, pr->id,
                        decision->sha, decision->label);
            } else {
                fprintf(index, "- Exclude `%s`: %s\n", decision->sha, decision->reason);
            }
        }
        ok = !ferror(index);
    }
    if (fclose(index)) ok = false;
    if (!ok) remove_tree(output_directory);
    return ok;
}

int review_series_route(const char *repository, const char *manifest_path,
                        const char *output_directory, char *error, size_t error_capacity)
{
    if (!repository || !*repository || !manifest_path || !*manifest_path ||
        !output_directory || !*output_directory) {
        set_error(error, error_capacity, "repository, manifest, and output paths are required");
        return -1;
    }
    if (error && error_capacity) error[0] = '\0';
    SeriesManifest *manifest = calloc(1, sizeof(*manifest));
    if (!manifest) {
        set_error(error, error_capacity, "cannot allocate review series manifest");
        return -1;
    }
    if (!parse_manifest(manifest_path, manifest, error, error_capacity)) {
        free(manifest->decisions);
        free(manifest);
        return -1;
    }
    size_t order[SERIES_MAX_PRS] = { 0 };
    if (!validate_graph(repository, manifest, manifest_path, order, error, error_capacity)) {
        free(manifest->decisions);
        free(manifest);
        return -1;
    }
    char **revisions = NULL;
    size_t revision_count = 0;
    if (!validate_accounting(repository, manifest, &revisions, &revision_count, error, error_capacity)) {
        free_revisions(revisions, revision_count);
        free(manifest->decisions);
        free(manifest);
        return -1;
    }
    if (!validate_decisions(repository, manifest, revisions, revision_count, error, error_capacity)) {
        free_revisions(revisions, revision_count);
        free(manifest->decisions);
        free(manifest);
        return -1;
    }
    bool ok = write_route(repository, manifest, revisions, revision_count, order,
                          output_directory, error, error_capacity);
    free_revisions(revisions, revision_count);
    free(manifest->decisions);
    free(manifest);
    return ok ? 0 : -1;
}

#ifndef REVIEW_SERIES_NO_MAIN
int main(int argc, char **argv)
{
    if (argc != 8 || strcmp(argv[1], "route") || strcmp(argv[2], "--repo") ||
        strcmp(argv[4], "--manifest") || strcmp(argv[6], "--output")) {
        fprintf(stderr, "usage: review_series route --repo DIR --manifest FILE --output DIR\n");
        return EXIT_FAILURE;
    }
    char error[1024];
    if (review_series_route(argv[3], argv[5], argv[7], error, sizeof(error))) {
        fprintf(stderr, "review-series: %s\n", error[0] ? error : "route generation failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
