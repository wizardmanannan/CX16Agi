#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "review_sha256.h"
#include "review_format.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PATH_CAP 4096
#define FIELD_CAP 8192
#define MAX_SUITES 32
#define MAX_WITNESSES 32
#define MAX_SCOPES MAX_WITNESSES
#define MAX_TOOLS 32
#define MAX_INPUTS 16
#define MAX_ARTIFACT_REQUIREMENTS 32
#define MAX_ARTIFACT_MATCHES 128
#define MAX_ARTIFACT_PATTERN 512
#define MAX_OVERLAYS 16
#define MAX_ARGS 64
#define MAX_FIELDS 256
#define MAX_TEXT (16 * 1024 * 1024)
#define RUN_CAPTURE_TIMEOUT_SECONDS 30
#define MAX_CHILD_LOG_BYTES (8u * 1024u * 1024u)

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

typedef struct {
    char name[128];
    char executable[PATH_CAP];
} Tool;

typedef struct {
    char name[128];
    char path[PATH_CAP];
} Input;

typedef struct {
    char name[128];
    unsigned timeout;
    size_t argc;
    char *argv[MAX_ARGS];
} Suite;

typedef struct {
    char suite[128];
    char pattern[MAX_ARTIFACT_PATTERN];
} ArtifactRequirement;

typedef struct {
    char path[PATH_CAP];
    char sha[65];
} ArtifactMatch;

typedef struct {
    char name[128];
    char baseline[65];
    unsigned timeout;
    int symptom_exit;
    char execution_marker[512];
    char symptom_marker[512];
    char pass_marker[512];
    size_t build_argc;
    char *build_argv[MAX_ARGS];
    size_t run_argc;
    char *run_argv[MAX_ARGS];
    size_t overlay_count;
    char overlays[MAX_OVERLAYS][PATH_CAP];
} Witness;

typedef struct {
    char witness[128];
    char contract[1024];
    char entry[256];
    char entry_path[PATH_CAP];
    char callers[2048];
    char case_path[PATH_CAP];
    size_t suite_count;
    char suite_names[MAX_SUITES][128];
    char obligations[2048];
    char unavailable[2048];
} Scope;

typedef struct {
    char repo[PATH_CAP];
    char candidate[65];
    char toolchain[PATH_CAP];
    size_t tool_count;
    Tool tools[MAX_TOOLS];
    size_t input_count;
    Input inputs[MAX_INPUTS];
    size_t suite_count;
    Suite suites[MAX_SUITES];
    size_t artifact_requirement_count;
    ArtifactRequirement artifact_requirements[MAX_ARTIFACT_REQUIREMENTS];
    size_t witness_count;
    Witness witnesses[MAX_WITNESSES];
    size_t scope_count;
    Scope scopes[MAX_SCOPES];
    char plan_sha[65];
} Plan;

typedef struct {
    int exit_code;
    int signal_number;
    int timed_out;
    int skipped;
} ProcessResult;

typedef struct {
    char name[128];
    char path[PATH_CAP];
    char sha[65];
    char version_sha[65];
} ToolFact;

typedef struct {
    char name[128];
    char path[PATH_CAP];
    char sha[65];
} InputFact;

typedef struct {
    char name[128];
    ProcessResult result;
    char log[256];
    char log_sha[65];
    char artifact_sha[65];
} SuiteFact;

typedef struct {
    char name[128];
    char side[16];
    char ref[65];
    char tree[65];
    ProcessResult build;
    char build_log[256];
    char build_log_sha[65];
    ProcessResult run;
    char run_log[256];
    char run_log_sha[65];
    char artifact_sha[65];
    char overlay_sha[65];
} WitnessFact;

typedef struct {
    char plan_sha[65];
    char repo[PATH_CAP];
    char candidate[65];
    char tree[65];
    char clean[8];
    char toolchain_path[PATH_CAP];
    char toolchain_sha[65];
    char toolchain_version_sha[65];
    char review_tool_sha[65];
    size_t tool_count;
    ToolFact tools[MAX_TOOLS + 2];
    size_t input_count;
    InputFact inputs[MAX_INPUTS];
    char packet_sha[65];
    size_t suite_count;
    SuiteFact suites[MAX_SUITES];
    size_t witness_fact_count;
    WitnessFact witnesses[MAX_WITNESSES * 2];
    char seal[65];
} Evidence;

static char *make_review_packet(const Plan *plan, const Evidence *evidence, const char *root, size_t *length);
static void run_capture_command(const char *plan_path, const char *output_path, const char *argv0);
static void git_value(const char *repo, const char *revision, char value[65]);
static bool tracked_regular_file(const char *repo, const char *revision, const char *path);
static int run_capture(char *const argv[], const char *directory, char *output, size_t capacity,
                       bool capture_stderr);

static char temporary_roots[16][PATH_CAP];
static size_t temporary_root_count;
static bool temporary_cleanup_registered;

static void fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("review-tools: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}

static void buffer_reserve(Buffer *buffer, size_t extra)
{
    if (extra > MAX_TEXT || buffer->length > MAX_TEXT - extra) fail("output is too large");
    size_t needed = buffer->length + extra + 1;
    if (needed <= buffer->capacity) return;
    size_t capacity = buffer->capacity ? buffer->capacity : 1024;
    while (capacity < needed) {
        if (capacity > MAX_TEXT / 2) capacity = MAX_TEXT + 1;
        else capacity *= 2;
    }
    char *data = realloc(buffer->data, capacity);
    if (!data) fail("out of memory");
    buffer->data = data;
    buffer->capacity = capacity;
}

static void buffer_append(Buffer *buffer, const char *text)
{
    size_t length = strlen(text);
    buffer_reserve(buffer, length);
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void buffer_printf(Buffer *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) fail("cannot format output");
    buffer_reserve(buffer, (size_t)length);
    vsnprintf(buffer->data + buffer->length, (size_t)length + 1, format, args);
    buffer->length += (size_t)length;
    va_end(args);
}

static void copy_field(char *destination, size_t capacity, const char *source, const char *description)
{
    size_t length = strlen(source);
    if (!length || length >= capacity) fail("invalid or oversized %s", description);
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)source[i];
        if (ch < 32 || ch == 127 || ch == '|') fail("invalid character in %s", description);
    }
    memcpy(destination, source, length + 1);
}

static bool valid_name(const char *name)
{
    if (!*name || !isalnum((unsigned char)*name) || !strcmp(name, ".") || !strcmp(name, "..")) return false;
    for (const unsigned char *at = (const unsigned char *)name; *at; ++at)
        if (!isalnum(*at) && *at != '-' && *at != '_' && *at != '.') return false;
    return true;
}

static bool safe_relative_path(const char *path)
{
    if (!*path || path[0] == '/' || strchr(path, '\\') || path[strlen(path) - 1] == '/') return false;
    const char *part = path;
    for (const char *at = path;; ++at) {
        if (*at != '/' && *at != '\0') continue;
        size_t length = (size_t)(at - part);
        if (!length || (length == 1 && part[0] == '.') || (length == 2 && part[0] == '.' && part[1] == '.'))
            return false;
        if (!*at) break;
        part = at + 1;
    }
    return true;
}

static bool safe_artifact_pattern(const char *pattern)
{
    size_t length = strlen(pattern);
    if (!length || length >= MAX_ARTIFACT_PATTERN || !safe_relative_path(pattern)) return false;
    size_t wildcards = 0;
    for (const unsigned char *at = (const unsigned char *)pattern; *at; ++at) {
        if (isalnum(*at) || *at == '/' || *at == '.' || *at == '_' || *at == '-') continue;
        if (*at == '*' && ++wildcards <= 8) continue;
        return false;
    }
    return true;
}

static bool valid_sha(const char *value)
{
    if (strlen(value) != 40 && strlen(value) != 64) return false;
    for (const unsigned char *at = (const unsigned char *)value; *at; ++at)
        if (!isxdigit(*at)) return false;
    return true;
}

static bool valid_environment_token(const char *value)
{
    size_t length = strlen(value);
    if (length < 7 || strncmp(value, "@ENV:", 5) || value[length - 1] != '@') return false;
    const char *name = value + 5;
    size_t name_length = length - 6;
    if (!name_length || name_length >= 128 ||
        !(isalpha((unsigned char)name[0]) || name[0] == '_')) return false;
    for (size_t i = 0; i < name_length; ++i)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') return false;
    return true;
}

static unsigned parse_unsigned(const char *value, unsigned minimum, unsigned maximum, const char *description)
{
    if (!*value) fail("missing %s", description);
    for (const unsigned char *at = (const unsigned char *)value; *at; ++at)
        if (!isdigit(*at)) fail("invalid %s: %s", description, value);
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno || !end || *end || parsed < minimum || parsed > maximum)
        fail("invalid %s: %s", description, value);
    return (unsigned)parsed;
}

static int parse_status(const char *value, const char *description)
{
    return (int)parse_unsigned(value, 0, 255, description);
}

static char *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) fail("cannot read %s: %s", path, strerror(errno));
    Buffer buffer = { 0 };
    char chunk[8192];
    size_t count;
    while ((count = fread(chunk, 1, sizeof(chunk), file)) != 0) {
        buffer_reserve(&buffer, count);
        memcpy(buffer.data + buffer.length, chunk, count);
        buffer.length += count;
        buffer.data[buffer.length] = '\0';
    }
    int read_failed = ferror(file);
    int close_failed = fclose(file);
    if (read_failed || close_failed) fail("cannot read %s", path);
    if (!buffer.data) {
        buffer.data = calloc(1, 1);
        if (!buffer.data) fail("out of memory");
    }
    if (length) *length = buffer.length;
    return buffer.data;
}

static void write_file(const char *path, const char *data, size_t length)
{
    int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) fail("cannot create %s: %s", path, strerror(errno));
    size_t written = 0;
    while (written < length) {
        ssize_t amount = write(descriptor, data + written, length - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            int saved_errno = errno;
            close(descriptor);
            fail("cannot write %s: %s", path, strerror(saved_errno));
        }
        written += (size_t)amount;
    }
    if (close(descriptor)) fail("cannot close %s: %s", path, strerror(errno));
}

static void join_path(char *destination, size_t capacity, const char *root, const char *part)
{
    int length = snprintf(destination, capacity, "%s/%s", root, part);
    if (length < 0 || (size_t)length >= capacity) fail("path is too long");
}

static void mkdir_one(const char *path)
{
    if (mkdir(path, 0700) && errno != EEXIST) fail("cannot create directory %s: %s", path, strerror(errno));
    struct stat info;
    if (stat(path, &info) || !S_ISDIR(info.st_mode)) fail("not a directory: %s", path);
}

static void split_fields(char *line, char **fields, size_t capacity, size_t *count)
{
    size_t used = 0;
    char *at = line;
    if (!*at) fail("blank record in strict input");
    while (true) {
        if (used == capacity) fail("too many fields in input record");
        fields[used++] = at;
        char *separator = strchr(at, '|');
        if (!separator) break;
        *separator = '\0';
        if (!*at) fail("empty field in strict input");
        at = separator + 1;
        if (!*at) fail("empty trailing field in strict input");
    }
    *count = used;
    for (size_t i = 0; i < used; ++i)
        for (const unsigned char *ch = (const unsigned char *)fields[i]; *ch; ++ch)
            if (*ch < 32 || *ch == 127) fail("control byte in strict input");
}

static size_t split_lines(char *text, char **lines, size_t capacity)
{
    size_t count = 0;
    char *start = text;
    char *at = text;
    while (*at) {
        if (*at == '\n') {
            *at = '\0';
            if (!*start) fail("blank line in strict input");
            if (count == capacity) fail("too many records in strict input");
            lines[count++] = start;
            start = at + 1;
        }
        ++at;
    }
    if (*start) {
        if (count == capacity) fail("too many records in strict input");
        lines[count++] = start;
    }
    if (!count) fail("empty strict input");
    return count;
}

static void parse_arguments(char **fields, size_t field_count, size_t offset,
                            size_t *argc, char **argv, const char *description)
{
    if (offset >= field_count) fail("missing argument count for %s", description);
    unsigned count = parse_unsigned(fields[offset], 1, MAX_ARGS, description);
    if (field_count != offset + 1 + count) fail("argument count mismatch for %s", description);
    *argc = count;
    for (unsigned i = 0; i < count; ++i) {
        char checked[FIELD_CAP];
        copy_field(checked, sizeof(checked), fields[offset + 1 + i], description);
        argv[i] = strdup(fields[offset + 1 + i]);
        if (!argv[i]) fail("out of memory");
    }
}

static Witness *find_witness(Plan *plan, const char *name)
{
    for (size_t i = 0; i < plan->witness_count; ++i)
        if (!strcmp(plan->witnesses[i].name, name)) return &plan->witnesses[i];
    return NULL;
}

static Scope *find_scope(Plan *plan, const char *witness)
{
    for (size_t i = 0; i < plan->scope_count; ++i)
        if (!strcmp(plan->scopes[i].witness, witness)) return &plan->scopes[i];
    return NULL;
}

static bool coverage_available(const Scope *scope)
{
    return !strcmp(scope->unavailable, "none");
}

static void parse_scope_suites(const Plan *plan, Scope *scope, const char *value)
{
    const char *at = value;
    while (*at) {
        const char *end = strchr(at, ',');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        if (!length || length >= sizeof(scope->suite_names[0]) ||
            scope->suite_count == MAX_SUITES) fail("invalid scope suite list for %s", scope->witness);
        char name[128];
        memcpy(name, at, length);
        name[length] = '\0';
        if (!valid_name(name)) fail("invalid scope suite name for %s", scope->witness);
        bool found = false;
        for (size_t i = 0; i < plan->suite_count; ++i)
            if (!strcmp(plan->suites[i].name, name)) found = true;
        if (!found) fail("scope names an unknown required suite: %s", name);
        for (size_t i = 0; i < scope->suite_count; ++i)
            if (!strcmp(scope->suite_names[i], name)) fail("duplicate scope suite: %s", name);
        copy_field(scope->suite_names[scope->suite_count++], sizeof(scope->suite_names[0]), name,
                   "scope suite name");
        if (!end) break;
        at = end + 1;
        if (!*at) fail("invalid scope suite list for %s", scope->witness);
    }
    if (!scope->suite_count) fail("scope has no required suite: %s", scope->witness);
}

static bool plan_uses_token(const Plan *plan, const char *token)
{
    for (size_t i = 0; i < plan->suite_count; ++i)
        for (size_t arg = 0; arg < plan->suites[i].argc; ++arg)
            if (strstr(plan->suites[i].argv[arg], token)) return true;
    for (size_t i = 0; i < plan->witness_count; ++i) {
        const Witness *witness = &plan->witnesses[i];
        for (size_t arg = 0; arg < witness->build_argc; ++arg)
            if (strstr(witness->build_argv[arg], token)) return true;
        for (size_t arg = 0; arg < witness->run_argc; ++arg)
            if (strstr(witness->run_argv[arg], token)) return true;
    }
    return false;
}

static void parse_plan(const char *path, Plan *plan)
{
    size_t raw_length = 0;
    char *raw = read_file(path, &raw_length);
    if (memchr(raw, '\0', raw_length)) fail("NUL byte in strict plan input");
    if (!raw_length || raw[raw_length - 1] != '\n') fail("plan must end with a newline");
    if (review_sha256_file(path, plan->plan_sha)) fail("cannot hash plan %s", path);
    char *lines[128];
    size_t line_count = split_lines(raw, lines, sizeof(lines) / sizeof(lines[0]));
    if (strcmp(lines[0], "review-plan-v2")) fail("unsupported plan version");
    if (line_count < 6) fail("plan must declare a repository, candidate, toolchain, suite, and witness");

    char *fields[MAX_FIELDS];
    size_t count;
    split_fields(lines[1], fields, MAX_FIELDS, &count);
    if (count != 2 || strcmp(fields[0], "repo")) fail("expected repo record after plan header");
    if (!strcmp(fields[1], ".")) {
        char *argv[] = { (char *)"git", (char *)"rev-parse", (char *)"--show-toplevel", NULL };
        char output[PATH_CAP];
        if (run_capture(argv, ".", output, sizeof(output), false))
            fail("cannot resolve repository root from current directory");
        output[strcspn(output, "\r\n")] = '\0';
        if (!*output || !realpath(output, plan->repo))
            fail("cannot resolve Git repository root: %s", strerror(errno));
    } else {
        copy_field(plan->repo, sizeof(plan->repo), fields[1], "repository path");
        if (plan->repo[0] != '/') fail("repository path must be absolute or .");
    }

    split_fields(lines[2], fields, MAX_FIELDS, &count);
    if (count != 2 || strcmp(fields[0], "candidate"))
        fail("expected full candidate Git SHA or HEAD after repository path");
    if (!strcmp(fields[1], "HEAD")) {
        git_value(plan->repo, "HEAD^{commit}", plan->candidate);
    } else if (!valid_sha(fields[1]) || strlen(fields[1]) != 40) {
        fail("expected full candidate Git SHA after repository path");
    } else {
        copy_field(plan->candidate, sizeof(plan->candidate), fields[1], "candidate Git SHA");
    }

    split_fields(lines[3], fields, MAX_FIELDS, &count);
    if (count != 2 || strcmp(fields[0], "toolchain")) fail("expected toolchain record after candidate SHA");
    copy_field(plan->toolchain, sizeof(plan->toolchain), fields[1], "toolchain executable");

    bool saw_suite = false, saw_artifact = false, saw_witness = false;
    for (size_t line = 4; line < line_count; ++line) {
        split_fields(lines[line], fields, MAX_FIELDS, &count);
        if (!strcmp(fields[0], "tool")) {
            if (saw_suite || saw_witness || count != 3 || plan->tool_count == MAX_TOOLS)
                fail("tools must precede required suites and witnesses");
            Tool *tool = &plan->tools[plan->tool_count++];
            copy_field(tool->name, sizeof(tool->name), fields[1], "tool name");
            if (!valid_name(tool->name) || !strcmp(tool->name, "git") || !strcmp(tool->name, "tar"))
                fail("invalid or reserved tool name: %s", tool->name);
            for (size_t i = 0; i + 1 < plan->tool_count; ++i)
                if (!strcmp(plan->tools[i].name, tool->name)) fail("duplicate tool name: %s", tool->name);
            copy_field(tool->executable, sizeof(tool->executable), fields[2], "tool executable");
        } else if (!strcmp(fields[0], "input")) {
            if (saw_suite || saw_witness || count != 3 || plan->input_count == MAX_INPUTS)
                fail("external inputs must precede required suites and witnesses");
            Input *input = &plan->inputs[plan->input_count++];
            copy_field(input->name, sizeof(input->name), fields[1], "input name");
            if (!valid_name(input->name)) fail("invalid input name: %s", input->name);
            for (size_t i = 0; i + 1 < plan->input_count; ++i)
                if (!strcmp(plan->inputs[i].name, input->name)) fail("duplicate input name: %s", input->name);
            copy_field(input->path, sizeof(input->path), fields[2], "input path");
            if (input->path[0] != '/' && !valid_environment_token(input->path))
                fail("external input path must be absolute or an @ENV:NAME@ token: %s", input->path);
        } else if (!strcmp(fields[0], "suite")) {
            if (saw_artifact || saw_witness)
                fail("required suites must precede artifact requirements and witnesses");
            if (plan->suite_count == MAX_SUITES || count < 5) fail("too many or incomplete required suites");
            saw_suite = true;
            Suite *suite = &plan->suites[plan->suite_count++];
            copy_field(suite->name, sizeof(suite->name), fields[1], "suite name");
            if (!valid_name(suite->name)) fail("invalid suite name: %s", suite->name);
            for (size_t i = 0; i + 1 < plan->suite_count; ++i)
                if (!strcmp(plan->suites[i].name, suite->name)) fail("duplicate suite name: %s", suite->name);
            suite->timeout = parse_unsigned(fields[2], 1, 3600, "suite timeout");
            parse_arguments(fields, count, 3, &suite->argc, suite->argv, "suite argv");
        } else if (!strcmp(fields[0], "artifact")) {
            if (!saw_suite || saw_witness || count != 3 ||
                plan->artifact_requirement_count == MAX_ARTIFACT_REQUIREMENTS)
                fail("too many or incomplete required artifact records");
            saw_artifact = true;
            ArtifactRequirement *requirement =
                &plan->artifact_requirements[plan->artifact_requirement_count++];
            copy_field(requirement->suite, sizeof(requirement->suite), fields[1], "artifact suite name");
            if (!valid_name(requirement->suite))
                fail("invalid required artifact suite name: %s", requirement->suite);
            bool found_suite = false;
            for (size_t i = 0; i < plan->suite_count; ++i)
                if (!strcmp(plan->suites[i].name, requirement->suite)) found_suite = true;
            if (!found_suite) fail("required artifact names an unknown suite: %s", requirement->suite);
            copy_field(requirement->pattern, sizeof(requirement->pattern), fields[2],
                       "artifact path pattern");
            if (!safe_artifact_pattern(requirement->pattern))
                fail("invalid required artifact path pattern: %s", requirement->pattern);
            for (size_t i = 0; i + 1 < plan->artifact_requirement_count; ++i)
                if (!strcmp(plan->artifact_requirements[i].suite, requirement->suite) &&
                    !strcmp(plan->artifact_requirements[i].pattern, requirement->pattern))
                    fail("duplicate required artifact pattern for suite %s: %s",
                         requirement->suite, requirement->pattern);
        } else if (!strcmp(fields[0], "witness")) {
            saw_witness = true;
            if (plan->witness_count == MAX_WITNESSES || count < 12) fail("too many or incomplete regression witnesses");
            Witness *witness = &plan->witnesses[plan->witness_count++];
            copy_field(witness->name, sizeof(witness->name), fields[1], "witness name");
            if (!valid_name(witness->name)) fail("invalid witness name: %s", witness->name);
            for (size_t i = 0; i + 1 < plan->witness_count; ++i)
                if (!strcmp(plan->witnesses[i].name, witness->name)) fail("duplicate witness name: %s", witness->name);
            if (strlen(fields[2]) != 40 || !valid_sha(fields[2])) fail("witness baseline must be a full Git SHA");
            copy_field(witness->baseline, sizeof(witness->baseline), fields[2], "baseline Git SHA");
            witness->timeout = parse_unsigned(fields[3], 1, 3600, "witness timeout");
            witness->symptom_exit = (int)parse_unsigned(fields[4], 1, 255, "baseline symptom exit status");
            copy_field(witness->execution_marker, sizeof(witness->execution_marker), fields[5], "execution marker");
            copy_field(witness->symptom_marker, sizeof(witness->symptom_marker), fields[6], "baseline symptom marker");
            copy_field(witness->pass_marker, sizeof(witness->pass_marker), fields[7], "candidate pass marker");
            unsigned build_count = parse_unsigned(fields[8], 1, MAX_ARGS, "witness build argv count");
            if (count < 10 + build_count) fail("missing witness run argv count");
            witness->build_argc = build_count;
            for (unsigned i = 0; i < build_count; ++i) {
                copy_field(fields[9 + i], FIELD_CAP, fields[9 + i], "witness build argument");
                witness->build_argv[i] = strdup(fields[9 + i]);
                if (!witness->build_argv[i]) fail("out of memory");
            }
            size_t run_count_at = 9 + build_count;
            unsigned run_count = parse_unsigned(fields[run_count_at], 1, MAX_ARGS, "witness run argv count");
            if (count != run_count_at + 1 + run_count) fail("witness run argument count mismatch");
            witness->run_argc = run_count;
            for (unsigned i = 0; i < run_count; ++i) {
                copy_field(fields[run_count_at + 1 + i], FIELD_CAP,
                           fields[run_count_at + 1 + i], "witness run argument");
                witness->run_argv[i] = strdup(fields[run_count_at + 1 + i]);
                if (!witness->run_argv[i]) fail("out of memory");
            }
            if (!strstr(witness->run_argv[0], "@OUT@"))
                fail("witness %s executable must be produced under @OUT@", witness->name);
        } else if (!strcmp(fields[0], "overlay")) {
            if (!saw_witness || count != 3) fail("regression overlays must follow their witness");
            Witness *witness = find_witness(plan, fields[1]);
            if (!witness) fail("regression overlay names an unknown witness: %s", fields[1]);
            if (!safe_relative_path(fields[2])) fail("invalid regression overlay path: %s", fields[2]);
            if (witness->overlay_count == MAX_OVERLAYS) fail("too many overlays for witness %s", witness->name);
            for (size_t i = 0; i < witness->overlay_count; ++i)
                if (!strcmp(witness->overlays[i], fields[2]))
                    fail("duplicate overlay path for witness %s: %s", witness->name, fields[2]);
            copy_field(witness->overlays[witness->overlay_count++], PATH_CAP, fields[2], "overlay path");
        } else if (!strcmp(fields[0], "scope")) {
            if (count != 10 || !saw_witness || plan->scope_count == MAX_SCOPES)
                fail("too many or incomplete contract scope records");
            Scope *scope = &plan->scopes[plan->scope_count++];
            copy_field(scope->witness, sizeof(scope->witness), fields[1], "scope witness");
            if (!find_witness(plan, scope->witness))
                fail("contract scope names an unknown witness: %s", scope->witness);
            if (find_scope(plan, scope->witness) != scope)
                fail("duplicate contract scope for witness: %s", scope->witness);
            copy_field(scope->contract, sizeof(scope->contract), fields[2], "behavior contract");
            copy_field(scope->entry, sizeof(scope->entry), fields[3], "production entry point");
            copy_field(scope->entry_path, sizeof(scope->entry_path), fields[4], "production source path");
            copy_field(scope->callers, sizeof(scope->callers), fields[5], "production callers");
            copy_field(scope->case_path, sizeof(scope->case_path), fields[6], "permanent case path");
            if (!safe_relative_path(scope->entry_path) || !safe_relative_path(scope->case_path))
                fail("invalid contract scope source or case path for %s", scope->witness);
            parse_scope_suites(plan, scope, fields[7]);
            copy_field(scope->obligations, sizeof(scope->obligations), fields[8], "coverage obligation");
            copy_field(scope->unavailable, sizeof(scope->unavailable), fields[9], "unavailable coverage");
        } else {
            fail("unknown plan record: %s", fields[0]);
        }
    }
    if (!plan->suite_count) fail("plan has no required suite; missing execution seam");
    if (!plan->witness_count) fail("plan has no regression witness; missing regression seam");
    if (plan->scope_count != plan->witness_count)
        fail("every regression witness must declare one contract and coverage scope");
    bool toolchain_bound = plan_uses_token(plan, "@TOOLCHAIN@");
    if (!toolchain_bound) fail("plan must use the declared toolchain through @TOOLCHAIN@");
    for (size_t i = 0; i < plan->tool_count; ++i) {
        char token[160];
        snprintf(token, sizeof(token), "@TOOL:%s@", plan->tools[i].name);
        if (!plan_uses_token(plan, token)) fail("declared tool is unused: %s", plan->tools[i].name);
    }
    for (size_t i = 0; i < plan->input_count; ++i) {
        char token[160];
        snprintf(token, sizeof(token), "@INPUT:%s@", plan->inputs[i].name);
        bool used = plan_uses_token(plan, token) || strstr(plan->toolchain, token) != NULL;
        for (size_t tool = 0; tool < plan->tool_count; ++tool)
            if (strstr(plan->tools[tool].executable, token)) used = true;
        if (!used) fail("declared external input is unused: %s", plan->inputs[i].name);
    }
    for (size_t i = 0; i < plan->witness_count; ++i)
        if (!plan->witnesses[i].overlay_count)
            fail("witness %s must declare its regression test overlays", plan->witnesses[i].name);
    for (size_t i = 0; i < plan->scope_count; ++i) {
        const Scope *scope = &plan->scopes[i];
        if (!tracked_regular_file(plan->repo, plan->candidate, scope->entry_path))
            fail("contract production source is not a tracked regular file at candidate: %s", scope->entry_path);
        if (!tracked_regular_file(plan->repo, plan->candidate, scope->case_path))
            fail("contract case is not a tracked regular file at candidate: %s", scope->case_path);
    }
}

static char *find_executable(const char *name);
static char *find_executable_at(const char *name, const char *directory);

static bool write_descriptor(int descriptor, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    size_t written = 0;
    while (written < length) {
        ssize_t amount = write(descriptor, bytes + written, length - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) return false;
        written += (size_t)amount;
    }
    return true;
}

static int run_capture(char *const argv[], const char *directory, char *output, size_t capacity,
                       bool capture_stderr)
{
    int pipe_fds[2];
    char *program = find_executable(argv[0]);
    if (!program) fail("cannot find executable %s on sanitized PATH", argv[0]);
    if (pipe(pipe_fds)) fail("cannot create command output pipe: %s", strerror(errno));
    pid_t child = fork();
    if (child < 0) fail("cannot fork %s: %s", argv[0], strerror(errno));
    if (!child) {
        static char path[] = "PATH=/usr/local/bin:/usr/bin:/bin";
        static char home[] = "HOME=/dev/null";
        static char temp[] = "TMPDIR=/tmp";
        static char lang[] = "LANG=C";
        static char locale[] = "LC_ALL=C";
        static char no_system_config[] = "GIT_CONFIG_NOSYSTEM=1";
        static char no_global_config[] = "GIT_CONFIG_GLOBAL=/dev/null";
        static char no_prompt[] = "GIT_TERMINAL_PROMPT=0";
        char *environment[] = { path, home, temp, lang, locale, no_system_config,
                                no_global_config, no_prompt, NULL };
        (void)setpgid(0, 0);
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _Exit(125);
        if (capture_stderr && dup2(pipe_fds[1], STDERR_FILENO) < 0) _Exit(125);
        close(pipe_fds[1]);
        if (!capture_stderr) {
            int null = open("/dev/null", O_WRONLY);
            if (null >= 0) { dup2(null, STDERR_FILENO); close(null); }
        }
        if (directory && chdir(directory)) _Exit(125);
        execve(program, argv, environment);
        _Exit(127);
    }
    (void)setpgid(child, child);
    close(pipe_fds[1]);
    int read_flags = fcntl(pipe_fds[0], F_GETFL);
    if (read_flags < 0 || fcntl(pipe_fds[0], F_SETFL, read_flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        (void)kill(-child, SIGKILL);
        (void)kill(child, SIGKILL);
        close(pipe_fds[0]);
        (void)waitpid(child, NULL, 0);
        free(program);
        fail("cannot configure %s output pipe: %s", argv[0], strerror(saved_errno));
    }
    size_t used = 0;
    bool pipe_closed = false, child_exited = false;
    int status = 0;
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) fail("cannot read monotonic clock: %s", strerror(errno));
    while (!pipe_closed || !child_exited) {
        if (clock_gettime(CLOCK_MONOTONIC, &now)) fail("cannot read monotonic clock: %s", strerror(errno));
        if (now.tv_sec - start.tv_sec >= RUN_CAPTURE_TIMEOUT_SECONDS) {
            (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            close(pipe_fds[0]);
            free(program);
            fail("%s identity probe timed out after %d seconds", argv[0], RUN_CAPTURE_TIMEOUT_SECONDS);
        }
        struct pollfd descriptor = { pipe_fds[0], POLLIN | POLLHUP, 0 };
        int ready = poll(&descriptor, 1, 100);
        if (ready < 0 && errno != EINTR) {
            int saved_errno = errno;
            (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            close(pipe_fds[0]);
            free(program);
            fail("cannot poll %s output: %s", argv[0], strerror(saved_errno));
        }
        if (!pipe_closed && ready > 0) {
            for (;;) {
                char chunk[1024];
                ssize_t amount = read(pipe_fds[0], chunk, sizeof(chunk));
                if (!amount) { pipe_closed = true; break; }
                if (amount < 0 && errno == EINTR) continue;
                if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (amount < 0) {
                    int saved_errno = errno;
                    (void)kill(-child, SIGKILL);
                    (void)kill(child, SIGKILL);
                    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                    close(pipe_fds[0]);
                    free(program);
                    fail("cannot read %s output: %s", argv[0], strerror(saved_errno));
                }
                if ((size_t)amount >= capacity || used >= capacity - (size_t)amount - 1) {
                    (void)kill(-child, SIGKILL);
                    (void)kill(child, SIGKILL);
                    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                    close(pipe_fds[0]);
                    free(program);
                    fail("%s output is too large", argv[0]);
                }
                memcpy(output + used, chunk, (size_t)amount);
                used += (size_t)amount;
            }
        }
        if (!child_exited) {
            pid_t waited = waitpid(child, &status, WNOHANG);
            if (waited == child) child_exited = true;
            else if (waited < 0 && errno != EINTR) {
                int saved_errno = errno;
                (void)kill(-child, SIGKILL);
                close(pipe_fds[0]);
                free(program);
                fail("cannot wait for %s: %s", argv[0], strerror(saved_errno));
            }
        }
    }
    close(pipe_fds[0]);
    output[used] = '\0';
    free(program);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

static void git_value(const char *repo, const char *revision, char value[65])
{
    char expression[96];
    if (snprintf(expression, sizeof(expression), "%s", revision) >= (int)sizeof(expression)) fail("Git revision is too long");
    char *argv[] = { (char *)"git", (char *)"-C", (char *)repo, (char *)"rev-parse", (char *)"--verify", expression, NULL };
    char output[128];
    if (run_capture(argv, NULL, output, sizeof(output), false)) fail("Git cannot resolve %s", revision);
    output[strcspn(output, "\r\n")] = '\0';
    if (!valid_sha(output)) fail("Git returned an invalid object ID for %s", revision);
    copy_field(value, 65, output, "Git object ID");
}

static void git_tree(const char *repo, const char *commit, char tree[65])
{
    char expression[80];
    if (snprintf(expression, sizeof(expression), "%s^{tree}", commit) >= (int)sizeof(expression)) fail("Git commit is too long");
    git_value(repo, expression, tree);
}

static bool tracked_regular_file(const char *repo, const char *revision, const char *path)
{
    char *argv[] = { (char *)"git", (char *)"-C", (char *)repo, (char *)"ls-tree", (char *)"-z",
        (char *)revision, (char *)"--", (char *)path, NULL };
    char output[PATH_CAP + 128];
    if (run_capture(argv, NULL, output, sizeof(output), false)) return false;
    if (!*output) return false;
    char *mode_end = strchr(output, ' ');
    char *type_end = mode_end ? strchr(mode_end + 1, ' ') : NULL;
    char *tab = type_end ? strchr(type_end + 1, '\t') : NULL;
    if (!mode_end || !type_end || !tab || type_end - (mode_end + 1) != 4 ||
        strncmp(mode_end + 1, "blob", 4) ||
        strcmp(tab + 1, path)) return false;
    size_t mode_length = (size_t)(mode_end - output);
    return (mode_length == 6 && (!strncmp(output, "100644", mode_length) ||
                                  !strncmp(output, "100755", mode_length)));
}

static char *resolve_executable(const char *name)
{
    if (strchr(name, '/')) {
        char resolved[PATH_CAP];
        if (!realpath(name, resolved)) fail("cannot resolve executable %s: %s", name, strerror(errno));
        if (access(resolved, X_OK)) fail("executable is not runnable: %s", resolved);
        return strdup(resolved);
    }
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char *copy = strdup(path);
    if (!copy) fail("out of memory");
    char *save = NULL;
    for (char *part = strtok_r(copy, ":", &save); part; part = strtok_r(NULL, ":", &save)) {
        char candidate[PATH_CAP], resolved[PATH_CAP];
        if (snprintf(candidate, sizeof(candidate), "%s/%s", *part ? part : ".", name) >= (int)sizeof(candidate)) continue;
        if (!access(candidate, X_OK) && realpath(candidate, resolved)) {
            free(copy);
            return strdup(resolved);
        }
    }
    free(copy);
    fail("cannot find executable %s on PATH", name);
    return NULL;
}

static const Input *find_input(const Plan *plan, const char *name)
{
    for (size_t i = 0; i < plan->input_count; ++i)
        if (!strcmp(plan->inputs[i].name, name)) return &plan->inputs[i];
    return NULL;
}

static const Tool *find_tool(const Plan *plan, const char *name)
{
    for (size_t i = 0; i < plan->tool_count; ++i)
        if (!strcmp(plan->tools[i].name, name)) return &plan->tools[i];
    return NULL;
}

static char *resolved_input_path(const Plan *plan, const char *name)
{
    const Input *input = find_input(plan, name);
    if (!input) fail("unknown declared input: %s", name);
    char declared_path[PATH_CAP];
    if (valid_environment_token(input->path)) {
        size_t name_length = strlen(input->path) - 6;
        char environment_name[128];
        memcpy(environment_name, input->path + 5, name_length);
        environment_name[name_length] = '\0';
        const char *value = getenv(environment_name);
        if (!value || !*value) fail("required input environment variable is unset: %s", environment_name);
        copy_field(declared_path, sizeof(declared_path), value, "external input path");
    } else {
        copy_field(declared_path, sizeof(declared_path), input->path, "external input path");
    }
    if (declared_path[0] != '/') fail("external input path must resolve to an absolute path: %s", declared_path);
    struct stat info;
    if (lstat(declared_path, &info) || S_ISLNK(info.st_mode) ||
        (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)))
        fail("external input is missing or has an unsupported type: %s", declared_path);
    char resolved[PATH_CAP];
    if (!realpath(declared_path, resolved)) fail("cannot resolve external input %s", declared_path);
    return strdup(resolved);
}

static char *replace_input_tokens(const Plan *plan, const char *source)
{
    Buffer buffer = { 0 };
    const char *at = source;
    while (*at) {
        if (!strncmp(at, "@INPUT:", 7)) {
            const char *end = strchr(at + 7, '@');
            if (!end) fail("unterminated external input token");
            size_t name_length = (size_t)(end - (at + 7));
            if (!name_length || name_length >= 128) fail("invalid external input token");
            char name[128];
            memcpy(name, at + 7, name_length);
            name[name_length] = '\0';
            char *path = resolved_input_path(plan, name);
            buffer_append(&buffer, path);
            free(path);
            at = end + 1;
        } else {
            char one[2] = { *at++, '\0' };
            buffer_append(&buffer, one);
        }
    }
    if (!buffer.data) buffer.data = strdup("");
    if (!buffer.data) fail("out of memory");
    return buffer.data;
}

static char *resolve_plan_toolchain(const Plan *plan)
{
    char *declared = replace_input_tokens(plan, plan->toolchain);
    char *resolved = resolve_executable(declared);
    free(declared);
    return resolved;
}

static char *resolved_tool_path(const Plan *plan, const char *name)
{
    const Tool *tool = find_tool(plan, name);
    if (!tool) fail("unknown declared tool: %s", name);
    char *executable = replace_input_tokens(plan, tool->executable);
    char *path = resolve_executable(executable);
    free(executable);
    return path;
}

static void path_sha(const char *path, char digest[65])
{
    if (review_sha256_file(path, digest)) fail("cannot hash %s: %s", path, strerror(errno));
}

static void append_sha(review_sha256 *context, const void *data, size_t length)
{
    review_sha256_update(context, data, length);
}

static void append_u64(review_sha256 *context, uint64_t value)
{
    unsigned char bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[sizeof(bytes) - 1 - i] = (unsigned char)(value >> (i * 8));
    append_sha(context, bytes, sizeof(bytes));
}

static void hash_tree_at(review_sha256 *context, const char *root, const char *relative,
                         bool exclude_root_git)
{
    char path[PATH_CAP];
    if (*relative) join_path(path, sizeof(path), root, relative);
    else copy_field(path, sizeof(path), root, "tree path");
    struct stat info;
    if (lstat(path, &info)) fail("cannot inspect artifact %s: %s", path, strerror(errno));
    if (S_ISDIR(info.st_mode)) {
        struct dirent **entries = NULL;
        int count = scandir(path, &entries, NULL, alphasort);
        if (count < 0) fail("cannot list artifact directory %s: %s", path, strerror(errno));
        if (count < 2) fail("invalid artifact directory listing: %s", path);
        bool is_excluded_root = exclude_root_git && !*relative;
        uint64_t entry_count = 0;
        for (int i = 0; i < count; ++i) {
            const char *name = entries[i]->d_name;
            if (!strcmp(name, ".") || !strcmp(name, "..") ||
                (is_excluded_root && !strcmp(name, ".git"))) continue;
            ++entry_count;
        }
        append_u64(context, strlen(relative));
        append_sha(context, relative, strlen(relative));
        char kind = 'd';
        append_sha(context, &kind, 1);
        append_u64(context, (uint64_t)(info.st_mode & 07777));
        append_u64(context, entry_count);
        for (int i = 0; i < count; ++i) {
            const char *name = entries[i]->d_name;
            if (strcmp(name, ".") && strcmp(name, "..") &&
                !(is_excluded_root && !strcmp(name, ".git"))) {
                char child[PATH_CAP];
                int length = *relative ? snprintf(child, sizeof(child), "%s/%s", relative, name) :
                                         snprintf(child, sizeof(child), "%s", name);
                if (length < 0 || length >= (int)sizeof(child)) fail("artifact path is too long");
                hash_tree_at(context, root, child, false);
            }
            free(entries[i]);
        }
        free(entries);
        return;
    }
    char kind = S_ISREG(info.st_mode) ? 'f' : S_ISLNK(info.st_mode) ? 'l' : '?';
    if (kind == '?') fail("unsupported artifact type: %s", path);
    append_u64(context, strlen(relative));
    append_sha(context, relative, strlen(relative));
    append_sha(context, &kind, 1);
    append_u64(context, (uint64_t)(info.st_mode & 07777));
    if (S_ISREG(info.st_mode)) {
        if (info.st_size < 0) fail("invalid artifact size: %s", path);
        append_u64(context, (uint64_t)info.st_size);
        int descriptor = open(path, O_RDONLY | O_NOFOLLOW);
        if (descriptor < 0) fail("cannot read artifact %s: %s", path, strerror(errno));
        struct stat opened;
        if (fstat(descriptor, &opened) || !S_ISREG(opened.st_mode) || opened.st_dev != info.st_dev || opened.st_ino != info.st_ino) {
            close(descriptor);
            fail("artifact changed while hashing: %s", path);
        }
        FILE *file = fdopen(descriptor, "rb");
        if (!file) {
            close(descriptor);
            fail("cannot read artifact %s: %s", path, strerror(errno));
        }
        unsigned char chunk[16384];
        size_t length;
        uint64_t total = 0;
        while ((length = fread(chunk, 1, sizeof(chunk), file)) != 0) append_sha(context, chunk, length);
        total = (uint64_t)ftello(file);
        int read_failed = ferror(file);
        int close_failed = fclose(file);
        if (read_failed || close_failed || total != (uint64_t)info.st_size) fail("cannot hash artifact %s", path);
    } else {
        char target[PATH_CAP];
        ssize_t length = readlink(path, target, sizeof(target));
        if (length < 0 || (size_t)length == sizeof(target)) fail("cannot read artifact link %s", path);
        append_u64(context, (uint64_t)length);
        append_sha(context, target, (size_t)length);
    }
}

static void tree_sha(const char *path, char digest[65])
{
    review_sha256 context;
    review_sha256_init(&context);
    hash_tree_at(&context, path, "", false);
    unsigned char bytes[32];
    review_sha256_final(&context, bytes);
    review_sha256_hex(bytes, digest);
}

static void artifact_file_sha(const char *path, const struct stat *expected, char digest[65])
{
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW);
    if (descriptor < 0) fail("cannot open required artifact %s: %s", path, strerror(errno));
    struct stat opened;
    if (fstat(descriptor, &opened) || !S_ISREG(opened.st_mode) ||
        opened.st_dev != expected->st_dev || opened.st_ino != expected->st_ino ||
        opened.st_size != expected->st_size || opened.st_size <= 0) {
        close(descriptor);
        fail("required artifact changed or is empty while hashing: %s", path);
    }
    review_sha256 context;
    review_sha256_init(&context);
    unsigned char chunk[16384];
    uint64_t total = 0;
    for (;;) {
        ssize_t length = read(descriptor, chunk, sizeof(chunk));
        if (length < 0) {
            if (errno == EINTR) continue;
            close(descriptor);
            fail("cannot read required artifact %s: %s", path, strerror(errno));
        }
        if (!length) break;
        review_sha256_update(&context, chunk, (size_t)length);
        total += (uint64_t)length;
    }
    struct stat after;
    if (fstat(descriptor, &after) || after.st_dev != opened.st_dev || after.st_ino != opened.st_ino ||
        after.st_size != opened.st_size || total != (uint64_t)opened.st_size) {
        close(descriptor);
        fail("required artifact changed while hashing: %s", path);
    }
    if (close(descriptor)) fail("cannot close required artifact %s: %s", path, strerror(errno));
    unsigned char bytes[32];
    review_sha256_final(&context, bytes);
    review_sha256_hex(bytes, digest);
}

static void collect_artifact_matches_at(const char *suite_root, const char *suite_relative,
                                        const char *relative, const char *pattern,
                                        ArtifactMatch matches[MAX_ARTIFACT_MATCHES],
                                        size_t *match_count, unsigned depth)
{
    if (depth > 64) fail("required artifact tree exceeds the maximum directory depth");
    char directory_path[PATH_CAP];
    if (*relative) join_path(directory_path, sizeof(directory_path), suite_root, relative);
    else copy_field(directory_path, sizeof(directory_path), suite_root, "suite artifact root");
    DIR *directory = opendir(directory_path);
    if (!directory) fail("cannot inspect required artifact directory %s: %s",
                         directory_path, strerror(errno));
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            if (errno) {
                int error = errno;
                closedir(directory);
                fail("cannot read required artifact directory %s: %s", directory_path, strerror(error));
            }
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child_relative[PATH_CAP], child_path[PATH_CAP], display_path[PATH_CAP];
        int relative_length = *relative ?
            snprintf(child_relative, sizeof(child_relative), "%s/%s", relative, entry->d_name) :
            snprintf(child_relative, sizeof(child_relative), "%s", entry->d_name);
        if (relative_length < 0 || relative_length >= (int)sizeof(child_relative)) {
            closedir(directory);
            fail("required artifact path is too long under %s", suite_root);
        }
        join_path(child_path, sizeof(child_path), suite_root, child_relative);
        int display_length = snprintf(display_path, sizeof(display_path), "%s/%s",
                                      suite_relative, child_relative);
        if (display_length < 0 || display_length >= (int)sizeof(display_path)) {
            closedir(directory);
            fail("required artifact display path is too long");
        }
        struct stat info;
        if (lstat(child_path, &info)) {
            int error = errno;
            closedir(directory);
            fail("cannot inspect required artifact %s: %s", display_path, strerror(error));
        }
        bool matched = fnmatch(pattern, child_relative, FNM_PATHNAME) == 0;
        if (matched) {
            if (!S_ISREG(info.st_mode)) {
                closedir(directory);
                fail("required artifact match is not a regular file: %s", display_path);
            }
            if (info.st_size <= 0) {
                closedir(directory);
                fail("required artifact match is empty: %s", display_path);
            }
            if (*match_count == MAX_ARTIFACT_MATCHES) {
                closedir(directory);
                fail("required artifact pattern matched too many files: %s", display_path);
            }
            ArtifactMatch *match = &matches[(*match_count)++];
            copy_field(match->path, sizeof(match->path), child_relative, "matched artifact path");
            artifact_file_sha(child_path, &info, match->sha);
        }
        if (S_ISDIR(info.st_mode) && !S_ISLNK(info.st_mode))
            collect_artifact_matches_at(suite_root, suite_relative, child_relative, pattern,
                                        matches, match_count, depth + 1);
    }
    if (closedir(directory)) fail("cannot close required artifact directory %s", directory_path);
}

static int compare_artifact_matches(const void *left, const void *right)
{
    const ArtifactMatch *a = left;
    const ArtifactMatch *b = right;
    return strcmp(a->path, b->path);
}

static size_t collect_artifact_matches(const char *root, const ArtifactRequirement *requirement,
                                       ArtifactMatch matches[MAX_ARTIFACT_MATCHES])
{
    char suite_relative[256], suite_root[PATH_CAP];
    snprintf(suite_relative, sizeof(suite_relative), "artifacts/suites/%s", requirement->suite);
    join_path(suite_root, sizeof(suite_root), root, suite_relative);
    struct stat info;
    if (lstat(suite_root, &info) || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))
        fail("required artifact suite root is not a real directory: %s", suite_relative);
    size_t match_count = 0;
    collect_artifact_matches_at(suite_root, suite_relative, "", requirement->pattern,
                                matches, &match_count, 0);
    if (!match_count)
        fail("required artifact pattern matched no files: %s:%s",
             requirement->suite, requirement->pattern);
    qsort(matches, match_count, sizeof(matches[0]), compare_artifact_matches);
    return match_count;
}

static void validate_artifact_requirements(const Plan *plan, const char *suite_name,
                                           const char *root)
{
    for (size_t i = 0; i < plan->artifact_requirement_count; ++i) {
        const ArtifactRequirement *requirement = &plan->artifact_requirements[i];
        if (strcmp(requirement->suite, suite_name)) continue;
        ArtifactMatch matches[MAX_ARTIFACT_MATCHES];
        collect_artifact_matches(root, requirement, matches);
    }
}

static void external_input_sha(const char *path, char digest[65])
{
    static const char input_domain[] = "cx16agi-external-input-v1";
    static const char directory_policy[] = "directory-root-dotgit-excluded-v1";
    static const char file_policy[] = "regular-file-v1";
    struct stat info;
    if (stat(path, &info)) fail("cannot inspect external input %s: %s", path, strerror(errno));
    if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode))
        fail("external input has an unsupported type: %s", path);
    review_sha256 context;
    review_sha256_init(&context);
    append_sha(&context, input_domain, sizeof(input_domain) - 1);
    if (S_ISDIR(info.st_mode)) {
        append_sha(&context, directory_policy, sizeof(directory_policy) - 1);
        hash_tree_at(&context, path, "", true);
    } else {
        append_sha(&context, file_policy, sizeof(file_policy) - 1);
        hash_tree_at(&context, path, "", false);
    }
    unsigned char bytes[32];
    review_sha256_final(&context, bytes);
    review_sha256_hex(bytes, digest);
}

static bool remove_tree(const char *path)
{
    struct stat info;
    if (lstat(path, &info)) return errno == ENOENT;
    if (!S_ISDIR(info.st_mode)) return !unlink(path) || errno == ENOENT;
    if (chmod(path, info.st_mode | S_IRUSR | S_IWUSR | S_IXUSR)) return false;
    DIR *directory = opendir(path);
    if (!directory) return false;
    bool okay = true;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            if (errno) okay = false;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[PATH_CAP];
        int length = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (length < 0 || length >= (int)sizeof(child) || !remove_tree(child)) okay = false;
    }
    if (closedir(directory)) okay = false;
    if (rmdir(path) && errno != ENOENT) okay = false;
    return okay;
}

static void cleanup_temporary_root(void)
{
    while (temporary_root_count) {
        const char *path = temporary_roots[temporary_root_count - 1];
        if (!remove_tree(path)) {
            fprintf(stderr, "review-tools: cannot clean owned replay directory %s\n", path);
            _Exit(EXIT_FAILURE);
        }
        --temporary_root_count;
    }
}

static void make_safe_environment(const char *root, const char *fixture_temp,
                                  char home[PATH_CAP], char temp[PATH_CAP],
                                  char output[PATH_CAP], char *env[11])
{
    join_path(home, PATH_CAP, root, "home");
    copy_field(temp, PATH_CAP, fixture_temp, "external fixture temp directory");
    join_path(output, PATH_CAP, root, "test-output");
    mkdir_one(home);
    struct stat temp_info;
    if (lstat(temp, &temp_info) || !S_ISDIR(temp_info.st_mode) || S_ISLNK(temp_info.st_mode))
        fail("external fixture temp is not a real directory: %s", temp);
    mkdir_one(output);
    static char path_env[] = "PATH=/usr/local/bin:/usr/bin:/bin";
    static char lang_env[] = "LANG=C";
    static char locale_env[] = "LC_ALL=C";
    static char timezone_env[] = "TZ=UTC";
    static char git_no_system[] = "GIT_CONFIG_NOSYSTEM=1";
    static char git_global[] = "GIT_CONFIG_GLOBAL=/dev/null";
    static char git_no_prompt[] = "GIT_TERMINAL_PROMPT=0";
    static char home_prefix[] = "HOME=";
    static char temp_prefix[] = "TMPDIR=";
    static char output_prefix[] = "TEST_OUTPUT_DIR=";
    static char home_value[PATH_CAP + 6];
    static char temp_value[PATH_CAP + 7];
    static char output_value[PATH_CAP + 16];
    if (snprintf(home_value, sizeof(home_value), "%s%s", home_prefix, home) >= (int)sizeof(home_value) ||
        snprintf(temp_value, sizeof(temp_value), "%s%s", temp_prefix, temp) >= (int)sizeof(temp_value) ||
        snprintf(output_value, sizeof(output_value), "%s%s", output_prefix, output) >= (int)sizeof(output_value))
        fail("sandbox path is too long");
    env[0] = path_env;
    env[1] = home_value;
    env[2] = temp_value;
    env[3] = output_value;
    env[4] = lang_env;
    env[5] = locale_env;
    env[6] = timezone_env;
    env[7] = git_no_system;
    env[8] = git_global;
    env[9] = git_no_prompt;
    env[10] = NULL;
}

static char *find_executable(const char *name)
{
    return find_executable_at(name, NULL);
}

static char *find_executable_at(const char *name, const char *directory)
{
    if (strchr(name, '/')) {
        char candidate[PATH_CAP], resolved[PATH_CAP];
        if (name[0] == '/' || !directory) {
            if (snprintf(candidate, sizeof(candidate), "%s", name) >= (int)sizeof(candidate)) return NULL;
        } else if (snprintf(candidate, sizeof(candidate), "%s/%s", directory, name) >= (int)sizeof(candidate)) {
            return NULL;
        }
        if (!realpath(candidate, resolved) || access(resolved, X_OK)) return NULL;
        return strdup(resolved);
    }
    const char *path = "/usr/local/bin:/usr/bin:/bin";
    char copy[PATH_CAP];
    copy_field(copy, sizeof(copy), path, "safe executable path");
    char *save = NULL;
    for (char *part = strtok_r(copy, ":", &save); part; part = strtok_r(NULL, ":", &save)) {
        char candidate[PATH_CAP], resolved[PATH_CAP];
        if (snprintf(candidate, sizeof(candidate), "%s/%s", part, name) >= (int)sizeof(candidate)) continue;
        if (!access(candidate, X_OK) && realpath(candidate, resolved)) return strdup(resolved);
    }
    return NULL;
}

static ProcessResult run_logged(char *const argv[], const char *directory, const char *log,
                                const char *sandbox, const char *fixture_temp, unsigned timeout)
{
    ProcessResult result = { .exit_code = 127, .signal_number = 0, .timed_out = 0, .skipped = 0 };
    char *program = find_executable_at(argv[0], directory);
    if (!program) {
        int descriptor = open(log, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
        if (descriptor >= 0) {
            dprintf(descriptor, "Cannot execute %s: executable not found on sanitized PATH\n", argv[0]);
            close(descriptor);
        }
        return result;
    }
    char home[PATH_CAP], temp[PATH_CAP], output[PATH_CAP];
    char *env[11];
    make_safe_environment(sandbox, fixture_temp, home, temp, output, env);
    char limit_message[128];
    int limit_length = snprintf(limit_message, sizeof(limit_message),
                                "\nreview-tools: child log output limit reached (%u bytes)\n",
                                MAX_CHILD_LOG_BYTES);
    if (limit_length < 0 || (size_t)limit_length >= sizeof(limit_message) ||
        (size_t)limit_length >= MAX_CHILD_LOG_BYTES) fail("cannot format child log limit diagnostic");
    size_t output_limit = MAX_CHILD_LOG_BYTES - (size_t)limit_length;
    int output_pipe[2];
    if (pipe(output_pipe)) fail("cannot create %s output pipe: %s", argv[0], strerror(errno));
    int log_descriptor = open(log, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (log_descriptor < 0) {
        int saved_errno = errno;
        close(output_pipe[0]);
        close(output_pipe[1]);
        free(program);
        fail("cannot create %s: %s", log, strerror(saved_errno));
    }
    pid_t child = fork();
    if (child < 0) {
        int saved_errno = errno;
        close(log_descriptor);
        close(output_pipe[0]);
        close(output_pipe[1]);
        free(program);
        fail("cannot fork %s: %s", argv[0], strerror(saved_errno));
    }
    if (!child) {
        (void)setpgid(0, 0);
        umask(077);
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0 || dup2(output_pipe[1], STDERR_FILENO) < 0) _Exit(125);
        close(output_pipe[1]);
        if (chdir(directory)) _Exit(125);
        execve(program, argv, env);
        dprintf(STDERR_FILENO, "Cannot execute %s: %s\n", argv[0], strerror(errno));
        _Exit(127);
    }
    (void)setpgid(child, child);
    close(output_pipe[1]);
    int read_flags = fcntl(output_pipe[0], F_GETFL);
    if (read_flags < 0 || fcntl(output_pipe[0], F_SETFL, read_flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        (void)kill(-child, SIGKILL);
        (void)kill(child, SIGKILL);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) { }
        close(output_pipe[0]);
        close(log_descriptor);
        free(program);
        fail("cannot configure %s output pipe: %s", argv[0], strerror(saved_errno));
    }
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) {
        int saved_errno = errno;
        (void)kill(-child, SIGKILL);
        (void)kill(child, SIGKILL);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) { }
        close(output_pipe[0]);
        close(log_descriptor);
        free(program);
        fail("cannot read monotonic clock: %s", strerror(saved_errno));
    }
    int status = 0;
    size_t logged = 0;
    bool pipe_closed = false, child_exited = false, stop_after_drain = false, output_limited = false;
    while (!pipe_closed || !child_exited) {
        if (!pipe_closed) {
            struct pollfd descriptor = { output_pipe[0], POLLIN | POLLHUP, 0 };
            int ready = poll(&descriptor, 1, 100);
            if (ready < 0 && errno != EINTR) {
                int saved_errno = errno;
                (void)kill(-child, SIGKILL);
                (void)kill(child, SIGKILL);
                while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                close(output_pipe[0]);
                close(log_descriptor);
                free(program);
                fail("cannot poll %s output: %s", argv[0], strerror(saved_errno));
            }
            if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
                for (;;) {
                    char chunk[8192];
                    ssize_t amount = read(output_pipe[0], chunk, sizeof(chunk));
                    if (!amount) {
                        pipe_closed = true;
                        close(output_pipe[0]);
                        break;
                    }
                    if (amount < 0 && errno == EINTR) continue;
                    if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        if (stop_after_drain && child_exited) {
                            pipe_closed = true;
                            close(output_pipe[0]);
                        }
                        break;
                    }
                    if (amount < 0) {
                        int saved_errno = errno;
                        (void)kill(-child, SIGKILL);
                        (void)kill(child, SIGKILL);
                        while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                        close(output_pipe[0]);
                        close(log_descriptor);
                        free(program);
                        fail("cannot read %s output: %s", argv[0], strerror(saved_errno));
                    }
                    size_t available = output_limit - logged;
                    size_t keep = (size_t)amount < available ? (size_t)amount : available;
                    if (keep && !write_descriptor(log_descriptor, chunk, keep)) {
                        int saved_errno = errno;
                        (void)kill(-child, SIGKILL);
                        (void)kill(child, SIGKILL);
                        while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                        close(output_pipe[0]);
                        close(log_descriptor);
                        free(program);
                        fail("cannot write %s: %s", log, strerror(saved_errno));
                    }
                    logged += keep;
                    if (keep != (size_t)amount) {
                        output_limited = true;
                        stop_after_drain = true;
                        if (kill(-child, SIGKILL) && errno != ESRCH) {
                            int saved_errno = errno;
                            (void)kill(child, SIGKILL);
                            while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                            close(output_pipe[0]);
                            close(log_descriptor);
                            free(program);
                            fail("cannot stop output-limited command %s: %s", argv[0], strerror(saved_errno));
                        }
                    }
                    if (stop_after_drain && child_exited) {
                        pipe_closed = true;
                        close(output_pipe[0]);
                        break;
                    }
                }
            }
            if (!pipe_closed && ready == 0 && stop_after_drain && child_exited) {
                pipe_closed = true;
                close(output_pipe[0]);
            }
        }
        if (!child_exited) {
            pid_t waited = waitpid(child, &status, WNOHANG);
            if (waited == child) child_exited = true;
            else if (waited < 0 && errno != EINTR) {
                int saved_errno = errno;
                (void)kill(-child, SIGKILL);
                close(output_pipe[0]);
                close(log_descriptor);
                free(program);
                fail("cannot wait for %s: %s", argv[0], strerror(saved_errno));
            }
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now)) {
            int saved_errno = errno;
            (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            close(output_pipe[0]);
            close(log_descriptor);
            free(program);
            fail("cannot read monotonic clock: %s", strerror(saved_errno));
        }
        if (!stop_after_drain &&
            (now.tv_sec - start.tv_sec > (time_t)timeout ||
             (now.tv_sec - start.tv_sec == (time_t)timeout && now.tv_nsec >= start.tv_nsec))) {
            result.timed_out = 1;
            stop_after_drain = true;
            if (kill(-child, SIGKILL) && errno != ESRCH) {
                int saved_errno = errno;
                (void)kill(child, SIGKILL);
                while (!child_exited && waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                close(output_pipe[0]);
                close(log_descriptor);
                free(program);
                fail("cannot stop timed out command %s: %s", argv[0], strerror(saved_errno));
            }
        }
        if (pipe_closed && !child_exited) {
            struct timespec delay = { .tv_sec = 0, .tv_nsec = 20000000 };
            nanosleep(&delay, NULL);
        }
        if (pipe_closed && child_exited) break;
    }
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.signal_number = WTERMSIG(status);
    if (output_limited) {
        if (!write_descriptor(log_descriptor, limit_message, (size_t)limit_length)) {
            int saved_errno = errno;
            close(log_descriptor);
            free(program);
            fail("cannot record output limit for %s: %s", argv[0], strerror(saved_errno));
        }
        if (WIFEXITED(status)) result.exit_code = 125;
    }
    if (close(log_descriptor)) {
        int saved_errno = errno;
        free(program);
        fail("cannot close %s: %s", log, strerror(saved_errno));
    }
    free(program);
    return result;
}

static char *replace_tokens(const Plan *plan, const char *source, const char *out, const char *temp,
                            const char *repo, const char *toolchain)
{
    Buffer buffer = { 0 };
    const char *at = source;
    while (*at) {
        if (!strncmp(at, "@INPUT:", 7)) {
            const char *end = strchr(at + 7, '@');
            if (!end) fail("unterminated external input token");
            size_t name_length = (size_t)(end - (at + 7));
            if (!name_length || name_length >= 128) fail("invalid external input token");
            char name[128];
            memcpy(name, at + 7, name_length);
            name[name_length] = '\0';
            char *path = resolved_input_path(plan, name);
            buffer_append(&buffer, path);
            free(path);
            at = end + 1;
            continue;
        }
        if (!strncmp(at, "@TOOL:", 6)) {
            const char *end = strchr(at + 6, '@');
            if (!end) fail("unterminated tool token");
            size_t name_length = (size_t)(end - (at + 6));
            if (!name_length || name_length >= 128) fail("invalid tool token");
            char name[128];
            memcpy(name, at + 6, name_length);
            name[name_length] = '\0';
            char *path = resolved_tool_path(plan, name);
            buffer_append(&buffer, path);
            free(path);
            at = end + 1;
            continue;
        }
        const char *value = NULL;
        size_t token_length = 0;
        if (!strncmp(at, "@OUT@", 5)) { value = out; token_length = 5; }
        else if (!strncmp(at, "@TMP@", 5)) { value = temp; token_length = 5; }
        else if (!strncmp(at, "@REPO@", 6)) { value = repo; token_length = 6; }
        else if (!strncmp(at, "@TOOLCHAIN@", 11)) { value = toolchain; token_length = 11; }
        if (value) {
            buffer_append(&buffer, value);
            at += token_length;
        } else {
            char one[2] = { *at++, '\0' };
            buffer_append(&buffer, one);
        }
    }
    if (!buffer.data) {
        buffer.data = strdup("");
        if (!buffer.data) fail("out of memory");
    }
    return buffer.data;
}

static char **expand_argv(const Plan *plan, char *const source[], size_t count,
                          const char *out, const char *temp, const char *repo,
                          const char *toolchain)
{
    char **argv = calloc(count + 1, sizeof(*argv));
    if (!argv) fail("out of memory");
    for (size_t i = 0; i < count; ++i)
        argv[i] = replace_tokens(plan, source[i], out, temp, repo, toolchain);
    return argv;
}

static void free_argv(char **argv, size_t count)
{
    if (!argv) return;
    for (size_t i = 0; i < count; ++i) free(argv[i]);
    free(argv);
}

static bool log_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t needle_length = strlen(needle);
    if (!needle_length || needle_length >= 4096) { fclose(file); return false; }
    char buffer[8192];
    size_t tail = 0;
    bool found = false;
    for (;;) {
        size_t count = fread(buffer + tail, 1, sizeof(buffer) - tail - 1, file);
        size_t total = tail + count;
        buffer[total] = '\0';
        if (strstr(buffer, needle)) { found = true; break; }
        if (count < sizeof(buffer) - tail - 1) break;
        tail = needle_length - 1;
        if (tail > total) tail = total;
        memmove(buffer, buffer + total - tail, tail);
    }
    if (fclose(file)) return false;
    return found;
}

static void hash_log(const char *root, const char *relative, char digest[65])
{
    char path[PATH_CAP];
    join_path(path, sizeof(path), root, relative);
    path_sha(path, digest);
}

static void readonly_tree(const char *path)
{
    struct stat info;
    if (lstat(path, &info)) fail("cannot inspect replay source %s: %s", path, strerror(errno));
    if (S_ISDIR(info.st_mode)) {
        DIR *directory = opendir(path);
        if (!directory) fail("cannot read replay source %s", path);
        for (;;) {
            struct dirent *entry = readdir(directory);
            if (!entry) break;
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char child[PATH_CAP];
            join_path(child, sizeof(child), path, entry->d_name);
            readonly_tree(child);
        }
        if (closedir(directory)) fail("cannot close replay source directory");
        if (chmod(path, 0555)) fail("cannot make replay source read-only: %s", path);
    } else if (S_ISREG(info.st_mode)) {
        mode_t mode = (info.st_mode & 0111) ? 0555 : 0444;
        if (chmod(path, mode)) fail("cannot make replay input read-only: %s", path);
    } else if (!S_ISLNK(info.st_mode)) {
        fail("unsupported replay input type: %s", path);
    }
}

static void safe_scratch_root(const char *root, char resolved[PATH_CAP])
{
    struct stat info;
    if (lstat(root, &info)) fail("replay root does not exist: %s", root);
    if (!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) fail("replay root must be a real directory: %s", root);
    if (!realpath(root, resolved)) fail("cannot resolve replay root %s", root);
}

static bool clean_repo(const char *repo)
{
    char *argv[] = { (char *)"git", (char *)"-C", (char *)repo, (char *)"status", (char *)"--porcelain=v1",
        (char *)"--untracked-files=all", NULL };
    char output[PATH_CAP];
    if (run_capture(argv, NULL, output, sizeof(output), false)) fail("cannot inspect Git checkout status");
    return !*output;
}

static void append_result(Buffer *buffer, const ProcessResult *result)
{
    buffer_printf(buffer, "|%d|%d|%d|%d", result->exit_code, result->signal_number,
                  result->timed_out, result->skipped);
}

static void parse_result(char **fields, size_t field_count, size_t offset, ProcessResult *result)
{
    if (field_count < offset + 4) fail("incomplete process result in evidence");
    result->exit_code = parse_status(fields[offset], "process exit status");
    result->signal_number = (int)parse_unsigned(fields[offset + 1], 0, 127, "process signal");
    result->timed_out = (int)parse_unsigned(fields[offset + 2], 0, 1, "process timeout flag");
    result->skipped = (int)parse_unsigned(fields[offset + 3], 0, 1, "process skipped flag");
}

static bool result_ok(const ProcessResult *result)
{
    return !result->exit_code && !result->signal_number && !result->timed_out && !result->skipped;
}

static void make_child_path(char destination[PATH_CAP], const char *root, const char *relative)
{
    join_path(destination, PATH_CAP, root, relative);
}

static void ensure_directory(const char *path)
{
    struct stat info;
    if (!lstat(path, &info)) {
        if (!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) fail("not a real directory: %s", path);
        return;
    }
    if (errno != ENOENT) fail("cannot inspect directory %s: %s", path, strerror(errno));
    mkdir_one(path);
}

static void create_output_layout(const char *root)
{
    char path[PATH_CAP];
    const char *directories[] = { "logs", "artifacts", "artifacts/suites", "artifacts/witnesses" };
    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        make_child_path(path, root, directories[i]);
        if (strchr(directories[i], '/')) {
            char parent[PATH_CAP];
            make_child_path(parent, root, "artifacts");
            ensure_directory(parent);
            ensure_directory(path);
        } else {
            ensure_directory(path);
        }
    }
}

static void create_owned_directory_named(const char *root, const char *prefix,
                                         char owned[PATH_CAP])
{
    if (temporary_root_count == sizeof(temporary_roots) / sizeof(temporary_roots[0]))
        fail("too many nested owned replay directories");
    char template[PATH_CAP];
    int length = snprintf(template, sizeof(template), "%s/%sXXXXXX", root, prefix);
    if (length < 0 || length >= (int)sizeof(template)) fail("replay path is too long");
    if (!mkdtemp(template)) fail("cannot create owned replay directory under %s: %s", root, strerror(errno));
    if (!temporary_cleanup_registered) {
        if (atexit(cleanup_temporary_root)) {
            remove_tree(template);
            fail("cannot register replay cleanup");
        }
        temporary_cleanup_registered = true;
    }
    char resolved[PATH_CAP];
    if (!realpath(template, resolved)) {
        int saved_errno = errno;
        remove_tree(template);
        fail("cannot resolve owned replay directory: %s", strerror(saved_errno));
    }
    copy_field(owned, PATH_CAP, resolved, "owned replay directory");
    copy_field(temporary_roots[temporary_root_count++], PATH_CAP, resolved, "temporary replay directory");
}

static void create_owned_directory(const char *root, char owned[PATH_CAP])
{
    create_owned_directory_named(root, "review-replay-", owned);
}

static bool path_is_within(const char *root, const char *path)
{
    size_t length = strlen(root);
    if (length == 1 && root[0] == '/') return path[0] == '/';
    return !strncmp(path, root, length) && (path[length] == '/' || path[length] == '\0');
}

static void resolve_fixture_temp_base(char base[PATH_CAP])
{
    const char *requested = getenv("TMPDIR");
    if (!requested || !*requested) requested = "/tmp";
    if (requested[0] != '/') fail("caller TMPDIR must be an absolute existing directory");
    struct stat info;
    if (!realpath(requested, base) || stat(base, &info) || !S_ISDIR(info.st_mode))
        fail("caller TMPDIR must be an absolute existing directory: %s", requested);
}

static void remove_owned_directory(const char *path);

static void create_external_fixture_directory(const Plan *plan, const char *base,
                                              const char *prefix, char owned[PATH_CAP])
{
    create_owned_directory_named(base, prefix, owned);
    char repo[PATH_CAP];
    if (!realpath(plan->repo, repo))
        fail("cannot resolve external suite fixture directory");
    if (path_is_within(repo, owned)) {
        remove_owned_directory(owned);
        fail("external suite fixture directory is inside the product worktree: %s", owned);
    }
}

static void remove_owned_directory(const char *path)
{
    if (!temporary_root_count || strcmp(temporary_roots[temporary_root_count - 1], path))
        fail("owned replay cleanup order is invalid");
    if (!remove_tree(path)) fail("cannot clean owned replay directory %s", path);
    --temporary_root_count;
}

static void record_skip(const char *path, const char *reason)
{
    Buffer message = { 0 };
    buffer_printf(&message, "SKIPPED: %s\n", reason);
    write_file(path, message.data, message.length);
    free(message.data);
}

static void calculate_toolchain_identity(const Plan *plan, Evidence *evidence)
{
    char *path = resolve_plan_toolchain(plan);
    copy_field(evidence->toolchain_path, sizeof(evidence->toolchain_path), path, "toolchain path");
    path_sha(path, evidence->toolchain_sha);
    char *version_argv[] = { path, (char *)"--version", NULL };
    char output[8192];
    int status = run_capture(version_argv, NULL, output, sizeof(output), true);
    if (status || !*output) fail("cannot identify toolchain with %s --version", path);
    review_sha256 context;
    unsigned char digest[32];
    review_sha256_init(&context);
    review_sha256_update(&context, output, strlen(output));
    review_sha256_final(&context, digest);
    review_sha256_hex(digest, evidence->toolchain_version_sha);
    free(path);
}

static void calculate_tool_fact(ToolFact *fact, const char *name, char *path)
{
    copy_field(fact->name, sizeof(fact->name), name, "tool name");
    copy_field(fact->path, sizeof(fact->path), path, "tool path");
    path_sha(path, fact->sha);
    char *version_argv[] = { path, (char *)"--version", NULL };
    char output[8192];
    if (run_capture(version_argv, NULL, output, sizeof(output), true) || !*output)
        fail("cannot identify declared tool with %s --version", path);
    review_sha256 context;
    unsigned char digest[32];
    review_sha256_init(&context);
    review_sha256_update(&context, output, strlen(output));
    review_sha256_final(&context, digest);
    review_sha256_hex(digest, fact->version_sha);
    free(path);
}

static void calculate_dependency_identities(const Plan *plan, Evidence *evidence)
{
    evidence->tool_count = 0;
    const char *internal_names[] = { "git", "tar" };
    for (size_t i = 0; i < sizeof(internal_names) / sizeof(internal_names[0]); ++i) {
        char *path = find_executable(internal_names[i]);
        if (!path) fail("cannot find internal tool %s on the sanitized PATH", internal_names[i]);
        calculate_tool_fact(&evidence->tools[evidence->tool_count++], internal_names[i], path);
    }
    for (size_t i = 0; i < plan->tool_count; ++i) {
        char *path = resolved_tool_path(plan, plan->tools[i].name);
        calculate_tool_fact(&evidence->tools[evidence->tool_count++], plan->tools[i].name, path);
    }

    evidence->input_count = plan->input_count;
    for (size_t i = 0; i < plan->input_count; ++i) {
        InputFact *fact = &evidence->inputs[i];
        char *path = resolved_input_path(plan, plan->inputs[i].name);
        copy_field(fact->name, sizeof(fact->name), plan->inputs[i].name, "input name");
        copy_field(fact->path, sizeof(fact->path), path, "input path");
        external_input_sha(path, fact->sha);
        free(path);
    }
}

static void identify_review_tool(const char *argv0, Evidence *evidence)
{
    char path[PATH_CAP];
    if (!realpath(argv0, path)) fail("cannot resolve review-tools executable %s: %s", argv0, strerror(errno));
    path_sha(path, evidence->review_tool_sha);
}

static void require_identity(const Plan *plan, char head[65], char tree[65])
{
    git_value(plan->repo, "HEAD", head);
    if (strcmp(head, plan->candidate)) fail("candidate HEAD mismatch: expected %s, found %s", plan->candidate, head);
    git_tree(plan->repo, head, tree);
    if (!clean_repo(plan->repo)) fail("Git checkout has tracked or untracked input changes");
}

static void run_suite_capture(const Plan *plan, const Suite *suite, const char *root,
                              const char *fixture_base, SuiteFact *fact)
{
    char artifact_relative[256], log_relative[256], sandbox_relative[256];
    snprintf(artifact_relative, sizeof(artifact_relative), "artifacts/suites/%s", suite->name);
    snprintf(log_relative, sizeof(log_relative), "logs/%s.log", suite->name);
    snprintf(sandbox_relative, sizeof(sandbox_relative), "sandbox-suite-%s", suite->name);
    char artifacts[PATH_CAP], log[PATH_CAP], sandbox[PATH_CAP], fixture_temp[PATH_CAP];
    join_path(artifacts, sizeof(artifacts), root, artifact_relative);
    join_path(log, sizeof(log), root, log_relative);
    join_path(sandbox, sizeof(sandbox), root, sandbox_relative);
    ensure_directory(artifacts);
    ensure_directory(sandbox);
    char fixture_prefix[256];
    int prefix_length = snprintf(fixture_prefix, sizeof(fixture_prefix), "review-suite-%s-", suite->name);
    if (prefix_length < 0 || prefix_length >= (int)sizeof(fixture_prefix))
        fail("suite fixture temp name is too long");
    create_external_fixture_directory(plan, fixture_base, fixture_prefix, fixture_temp);
    char *toolchain_path = resolve_plan_toolchain(plan);
    char **arguments = expand_argv(plan, suite->argv, suite->argc, artifacts, fixture_temp,
                                   plan->repo, toolchain_path);
    free(toolchain_path);
    fact->result = run_logged(arguments, plan->repo, log, sandbox, fixture_temp, suite->timeout);
    free_argv(arguments, suite->argc);
    if (result_ok(&fact->result)) validate_artifact_requirements(plan, suite->name, root);
    copy_field(fact->name, sizeof(fact->name), suite->name, "suite name");
    copy_field(fact->log, sizeof(fact->log), log_relative, "suite log path");
    hash_log(root, fact->log, fact->log_sha);
    tree_sha(artifacts, fact->artifact_sha);
    remove_owned_directory(fixture_temp);
    if (!remove_tree(sandbox)) fail("cannot clean suite sandbox %s", sandbox);
}

static void extract_revision(const Plan *plan, const char *revision, const char *scratch,
                            const char *fixture_temp,
                            char source[PATH_CAP], char tree[65])
{
    char archive[PATH_CAP], archive_log[PATH_CAP], extract_log[PATH_CAP];
    join_path(archive, sizeof(archive), scratch, "source.tar");
    join_path(archive_log, sizeof(archive_log), scratch, "archive.log");
    join_path(extract_log, sizeof(extract_log), scratch, "extract.log");
    join_path(source, PATH_CAP, scratch, "source");
    ensure_directory(source);

    char *archive_argv[] = { (char *)"git", (char *)"-C", (char *)plan->repo, (char *)"archive",
        (char *)"--format=tar", (char *)"--output", archive, (char *)revision, NULL };
    ProcessResult archived = run_logged(archive_argv, plan->repo, archive_log, scratch,
                                        fixture_temp, 60);
    if (!result_ok(&archived)) fail("cannot archive regression revision %s", revision);

    char *tar_argv[] = { (char *)"tar", (char *)"-xf", archive, (char *)"-C", source,
        (char *)"--no-same-owner", (char *)"--no-same-permissions", NULL };
    ProcessResult extracted = run_logged(tar_argv, scratch, extract_log, scratch,
                                         fixture_temp, 60);
    if (!result_ok(&extracted)) fail("cannot extract regression revision %s", revision);
    git_tree(plan->repo, revision, tree);
}

static void overlay_path(const char *root, const char *relative, bool create_parents,
                         char path[PATH_CAP])
{
    if (!safe_relative_path(relative)) fail("invalid regression overlay path: %s", relative);
    struct stat info;
    if (lstat(root, &info) || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))
        fail("overlay root is not a real directory: %s", root);
    char current[PATH_CAP];
    copy_field(current, sizeof(current), root, "overlay root");
    const char *part = relative;
    for (const char *slash = strchr(part, '/'); slash; slash = strchr(part, '/')) {
        char component[PATH_CAP];
        size_t length = (size_t)(slash - part);
        if (length >= sizeof(component)) fail("overlay path component is too long");
        memcpy(component, part, length);
        component[length] = '\0';
        char next[PATH_CAP];
        join_path(next, sizeof(next), current, component);
        if (lstat(next, &info)) {
            if (!create_parents || errno != ENOENT) fail("cannot inspect overlay path %s", next);
            mkdir_one(next);
            if (lstat(next, &info)) fail("cannot create overlay directory %s", next);
        }
        if (!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))
            fail("symlink or non-directory in regression overlay path: %s", next);
        copy_field(current, sizeof(current), next, "overlay directory");
        part = slash + 1;
    }
    join_path(path, PATH_CAP, current, part);
}

static unsigned char *read_overlay_file(const char *path, size_t *length, mode_t *mode)
{
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW);
    if (descriptor < 0) fail("cannot open regression overlay %s: %s", path, strerror(errno));
    struct stat info;
    if (fstat(descriptor, &info) || !S_ISREG(info.st_mode) || info.st_size < 0 || info.st_size > MAX_TEXT) {
        close(descriptor);
        fail("regression overlay is not a supported regular file: %s", path);
    }
    size_t size = (size_t)info.st_size;
    unsigned char *data = malloc(size ? size : 1);
    if (!data) { close(descriptor); fail("out of memory"); }
    size_t used = 0;
    while (used < size) {
        ssize_t amount = read(descriptor, data + used, size - used);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { free(data); close(descriptor); fail("cannot read regression overlay %s", path); }
        used += (size_t)amount;
    }
    if (close(descriptor)) { free(data); fail("cannot close regression overlay %s", path); }
    *length = size;
    *mode = info.st_mode & 0777;
    return data;
}

static void write_overlay_file(const char *path, const unsigned char *data, size_t length, mode_t mode)
{
    struct stat info;
    if (!lstat(path, &info) && (!S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)))
        fail("regression overlay would replace a non-regular file: %s", path);
    if (errno != ENOENT && lstat(path, &info)) fail("cannot inspect regression overlay destination %s", path);
    int descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, mode);
    if (descriptor < 0) fail("cannot write regression overlay %s: %s", path, strerror(errno));
    if (fchmod(descriptor, mode)) { close(descriptor); fail("cannot set regression overlay mode %s", path); }
    size_t used = 0;
    while (used < length) {
        ssize_t amount = write(descriptor, data + used, length - used);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { close(descriptor); fail("cannot write regression overlay %s", path); }
        used += (size_t)amount;
    }
    if (close(descriptor)) fail("cannot close regression overlay %s", path);
}

static void apply_witness_overlays(const Witness *witness, const char *overlay_root,
                                   const char *source_root, char digest[65])
{
    review_sha256 context;
    review_sha256_init(&context);
    for (size_t i = 0; i < witness->overlay_count; ++i) {
        char source[PATH_CAP], destination[PATH_CAP];
        overlay_path(overlay_root, witness->overlays[i], false, source);
        struct stat info;
        if (lstat(source, &info) || !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))
            fail("regression overlay source is missing or not a regular file: %s", witness->overlays[i]);
        size_t length = 0;
        mode_t mode = 0;
        unsigned char *data = read_overlay_file(source, &length, &mode);
        overlay_path(source_root, witness->overlays[i], true, destination);
        write_overlay_file(destination, data, length, mode);
        append_u64(&context, strlen(witness->overlays[i]));
        append_sha(&context, witness->overlays[i], strlen(witness->overlays[i]));
        append_u64(&context, (uint64_t)mode);
        append_u64(&context, (uint64_t)length);
        append_sha(&context, data, length);
        free(data);
    }
    unsigned char bytes[32];
    review_sha256_final(&context, bytes);
    review_sha256_hex(bytes, digest);
}

static void run_witness_side(const Plan *plan, const Witness *witness, const char *side,
                             const char *revision, const char *root, const char *fixture_base,
                             WitnessFact *fact)
{
    char artifact_relative[256], build_relative[256], run_relative[256];
    snprintf(artifact_relative, sizeof(artifact_relative), "artifacts/witnesses/%s-%s", witness->name, side);
    snprintf(build_relative, sizeof(build_relative), "logs/%s-%s-build.log", witness->name, side);
    snprintf(run_relative, sizeof(run_relative), "logs/%s-%s-run.log", witness->name, side);
    char artifacts[PATH_CAP], build_log[PATH_CAP], run_log[PATH_CAP];
    char scratch[PATH_CAP], fixture_temp[PATH_CAP], fixture_prefix[256];
    join_path(artifacts, sizeof(artifacts), root, artifact_relative);
    join_path(build_log, sizeof(build_log), root, build_relative);
    join_path(run_log, sizeof(run_log), root, run_relative);
    ensure_directory(artifacts);
    create_owned_directory(root, scratch);
    int prefix_length = snprintf(fixture_prefix, sizeof(fixture_prefix),
                                 "review-witness-%s-%s-", witness->name, side);
    if (prefix_length < 0 || prefix_length >= (int)sizeof(fixture_prefix))
        fail("witness fixture temp name is too long");
    create_external_fixture_directory(plan, fixture_base, fixture_prefix, fixture_temp);

    char source[PATH_CAP];
    extract_revision(plan, revision, scratch, fixture_temp, source, fact->tree);
    char overlay_root[PATH_CAP];
    if (!strcmp(revision, plan->candidate)) {
        copy_field(overlay_root, sizeof(overlay_root), source, "candidate overlay source");
    } else {
        char overlay_scratch[PATH_CAP], ignored_tree[65];
        join_path(overlay_scratch, sizeof(overlay_scratch), scratch, "candidate-overlay");
        ensure_directory(overlay_scratch);
        extract_revision(plan, plan->candidate, overlay_scratch, fixture_temp,
                         overlay_root, ignored_tree);
    }
    apply_witness_overlays(witness, overlay_root, source, fact->overlay_sha);
    readonly_tree(source);
    char *toolchain_path = resolve_plan_toolchain(plan);
    char **build_argv = expand_argv(plan, witness->build_argv, witness->build_argc, artifacts,
                                    fixture_temp, source, toolchain_path);
    free(toolchain_path);
    fact->build = run_logged(build_argv, source, build_log, scratch, fixture_temp,
                             witness->timeout);
    free_argv(build_argv, witness->build_argc);
    if (result_ok(&fact->build)) {
        char *run_toolchain = resolve_plan_toolchain(plan);
        char **run_argv = expand_argv(plan, witness->run_argv, witness->run_argc, artifacts,
                                      fixture_temp, source, run_toolchain);
        free(run_toolchain);
        fact->run = run_logged(run_argv, source, run_log, scratch, fixture_temp,
                               witness->timeout);
        free_argv(run_argv, witness->run_argc);
    } else {
        fact->run.exit_code = 0;
        fact->run.signal_number = 0;
        fact->run.timed_out = 0;
        fact->run.skipped = 1;
        record_skip(run_log, "witness build did not complete successfully");
    }

    copy_field(fact->name, sizeof(fact->name), witness->name, "witness name");
    copy_field(fact->side, sizeof(fact->side), side, "witness side");
    copy_field(fact->ref, sizeof(fact->ref), revision, "witness Git SHA");
    copy_field(fact->build_log, sizeof(fact->build_log), build_relative, "witness build log path");
    copy_field(fact->run_log, sizeof(fact->run_log), run_relative, "witness run log path");
    hash_log(root, fact->build_log, fact->build_log_sha);
    hash_log(root, fact->run_log, fact->run_log_sha);
    tree_sha(artifacts, fact->artifact_sha);
    remove_owned_directory(fixture_temp);
    remove_owned_directory(scratch);
}

static bool witness_side_passed(const Witness *plan, const WitnessFact *fact, const char *root)
{
    if (!result_ok(&fact->build) || fact->run.signal_number || fact->run.timed_out || fact->run.skipped) return false;
    if (!fact->run_log[0]) return false;
    char run_log_path[PATH_CAP];
    if (snprintf(run_log_path, sizeof(run_log_path), "%s/%s", root, fact->run_log) >= (int)sizeof(run_log_path)) return false;
    if (!strcmp(fact->side, "candidate"))
        return fact->run.exit_code == 0 && log_contains(run_log_path, plan->execution_marker) &&
               log_contains(run_log_path, plan->pass_marker);
    if (strcmp(fact->side, "baseline")) return false;
    return fact->run.exit_code == plan->symptom_exit &&
           log_contains(run_log_path, plan->execution_marker) && log_contains(run_log_path, plan->symptom_marker);
}

static void copy_sha(char destination[65], const char *value, size_t expected_length, const char *description)
{
    if (strlen(value) != expected_length || !valid_sha(value)) fail("invalid %s", description);
    copy_field(destination, 65, value, description);
}

static size_t parse_named_count(char **lines, size_t line_count, size_t *line_index,
                                const char *record, size_t maximum)
{
    if (*line_index >= line_count) fail("missing %s record in evidence", record);
    char *fields[4];
    size_t count;
    split_fields(lines[(*line_index)++], fields, 4, &count);
    if (count != 2 || strcmp(fields[0], record)) fail("expected %s record in evidence", record);
    return parse_unsigned(fields[1], 1, (unsigned)maximum, record);
}

static size_t parse_named_count_allow_zero(char **lines, size_t line_count, size_t *line_index,
                                           const char *record, size_t maximum)
{
    if (*line_index >= line_count) fail("missing %s record in evidence", record);
    char *fields[4];
    size_t count;
    split_fields(lines[(*line_index)++], fields, 4, &count);
    if (count != 2 || strcmp(fields[0], record)) fail("expected %s record in evidence", record);
    return parse_unsigned(fields[1], 0, (unsigned)maximum, record);
}

static void parse_evidence_file(const char *path, Evidence *evidence)
{
    size_t length = 0;
    char *raw = read_file(path, &length);
    if (memchr(raw, '\0', length)) fail("NUL byte in strict evidence input");
    if (!length || raw[length - 1] != '\n') fail("evidence must end with a newline");
    size_t seal_start = length - 1;
    while (seal_start && raw[seal_start - 1] != '\n') --seal_start;
    if (seal_start < 1 || strncmp(raw + seal_start, REVIEW_EVIDENCE_RECORD_SEAL "|",
                                  sizeof(REVIEW_EVIDENCE_RECORD_SEAL)) || length - seal_start != 70)
        fail("evidence has no final strict seal record");
    review_sha256 hash;
    unsigned char digest[32];
    char expected_seal[65];
    review_sha256_init(&hash);
    review_sha256_update(&hash, raw, seal_start);
    review_sha256_final(&hash, digest);
    review_sha256_hex(digest, expected_seal);

    char *lines[256];
    size_t line_count = split_lines(raw, lines, sizeof(lines) / sizeof(lines[0]));
    if (strcmp(lines[0], REVIEW_EVIDENCE_RECORD_MAGIC)) fail("unsupported evidence version");
    size_t line_index = 1;
    char *fields[MAX_FIELDS];
    size_t count;
#define READ_PAIR(key, destination, capacity, description) do { \
    if (line_index >= line_count) fail("missing %s record in evidence", key); \
    split_fields(lines[line_index++], fields, MAX_FIELDS, &count); \
    if (count != 2 || strcmp(fields[0], key)) fail("expected %s record in evidence", key); \
    copy_field(destination, capacity, fields[1], description); \
} while (0)

    READ_PAIR(REVIEW_EVIDENCE_RECORD_PLAN_SHA, evidence->plan_sha, sizeof(evidence->plan_sha), "plan SHA-256");
    copy_sha(evidence->plan_sha, evidence->plan_sha, 64, "plan SHA-256");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_REPO, evidence->repo, sizeof(evidence->repo), "repository path");
    if (evidence->repo[0] != '/') fail("evidence repository path is not absolute");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_CANDIDATE, evidence->candidate, sizeof(evidence->candidate), "candidate Git SHA");
    copy_sha(evidence->candidate, evidence->candidate, 40, "candidate Git SHA");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_TREE, evidence->tree, sizeof(evidence->tree), "candidate tree SHA");
    copy_sha(evidence->tree, evidence->tree, 40, "candidate tree SHA");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_CLEAN, evidence->clean, sizeof(evidence->clean), "checkout status");
    if (strcmp(evidence->clean, "yes") && strcmp(evidence->clean, "no")) fail("invalid checkout status in evidence");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_TOOLCHAIN_PATH, evidence->toolchain_path, sizeof(evidence->toolchain_path), "toolchain path");
    if (evidence->toolchain_path[0] != '/') fail("evidence toolchain path is not absolute");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_TOOLCHAIN_SHA, evidence->toolchain_sha, sizeof(evidence->toolchain_sha), "toolchain SHA-256");
    copy_sha(evidence->toolchain_sha, evidence->toolchain_sha, 64, "toolchain SHA-256");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_TOOLCHAIN_VERSION_SHA, evidence->toolchain_version_sha, sizeof(evidence->toolchain_version_sha), "toolchain version SHA-256");
    copy_sha(evidence->toolchain_version_sha, evidence->toolchain_version_sha, 64, "toolchain version SHA-256");
    READ_PAIR(REVIEW_EVIDENCE_RECORD_REVIEW_TOOL_SHA, evidence->review_tool_sha, sizeof(evidence->review_tool_sha), "review tool SHA-256");
    copy_sha(evidence->review_tool_sha, evidence->review_tool_sha, 64, "review tool SHA-256");

    evidence->tool_count = parse_named_count(lines, line_count, &line_index,
                                             REVIEW_EVIDENCE_RECORD_TOOL_COUNT,
                                             REVIEW_EVIDENCE_MAX_TOOL_FACTS);
    for (size_t i = 0; i < evidence->tool_count; ++i) {
        if (line_index >= line_count) fail("missing dependency tool evidence row");
        split_fields(lines[line_index++], fields, MAX_FIELDS, &count);
        if (count != 5 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_TOOL)) fail("malformed dependency tool evidence row");
        ToolFact *fact = &evidence->tools[i];
        copy_field(fact->name, sizeof(fact->name), fields[1], "tool name");
        if (!valid_name(fact->name)) fail("invalid tool name in evidence");
        copy_field(fact->path, sizeof(fact->path), fields[2], "tool path");
        if (fact->path[0] != '/') fail("evidence tool path is not absolute");
        copy_sha(fact->sha, fields[3], 64, "tool SHA-256");
        copy_sha(fact->version_sha, fields[4], 64, "tool version SHA-256");
    }
    evidence->input_count = parse_named_count_allow_zero(lines, line_count, &line_index,
                                                         REVIEW_EVIDENCE_RECORD_INPUT_COUNT,
                                                         REVIEW_EVIDENCE_MAX_INPUT_FACTS);
    for (size_t i = 0; i < evidence->input_count; ++i) {
        if (line_index >= line_count) fail("missing external input evidence row");
        split_fields(lines[line_index++], fields, MAX_FIELDS, &count);
        if (count != 4 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_INPUT)) fail("malformed external input evidence row");
        InputFact *fact = &evidence->inputs[i];
        copy_field(fact->name, sizeof(fact->name), fields[1], "input name");
        if (!valid_name(fact->name)) fail("invalid input name in evidence");
        copy_field(fact->path, sizeof(fact->path), fields[2], "input path");
        if (fact->path[0] != '/') fail("evidence input path is not absolute");
        copy_sha(fact->sha, fields[3], 64, "input SHA-256");
    }

    evidence->suite_count = parse_named_count(lines, line_count, &line_index,
                                              REVIEW_EVIDENCE_RECORD_SUITE_COUNT,
                                              REVIEW_EVIDENCE_MAX_SUITE_FACTS);
    for (size_t i = 0; i < evidence->suite_count; ++i) {
        if (line_index >= line_count) fail("missing required suite evidence row");
        split_fields(lines[line_index++], fields, MAX_FIELDS, &count);
        if (count != 9 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_SUITE)) fail("malformed required suite evidence row");
        SuiteFact *fact = &evidence->suites[i];
        copy_field(fact->name, sizeof(fact->name), fields[1], "suite name");
        if (!valid_name(fact->name)) fail("invalid suite name in evidence");
        parse_result(fields, count, 2, &fact->result);
        copy_field(fact->log, sizeof(fact->log), fields[6], "suite log path");
        copy_sha(fact->log_sha, fields[7], 64, "suite log SHA-256");
        copy_sha(fact->artifact_sha, fields[8], 64, "suite artifact SHA-256");
        char expected_log[256];
        snprintf(expected_log, sizeof(expected_log), "logs/%s.log", fact->name);
        if (strcmp(fact->log, expected_log)) fail("invalid required suite log path");
    }

    evidence->witness_fact_count = parse_named_count(lines, line_count, &line_index,
                                                     REVIEW_EVIDENCE_RECORD_WITNESS_COUNT,
                                                     REVIEW_EVIDENCE_MAX_WITNESS_FACTS);
    if (evidence->witness_fact_count % 2) fail("evidence has an incomplete regression witness pair");
    for (size_t i = 0; i < evidence->witness_fact_count; ++i) {
        if (line_index >= line_count) fail("missing regression witness evidence row");
        split_fields(lines[line_index++], fields, MAX_FIELDS, &count);
        if (count != 19 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_WITNESS)) fail("malformed regression witness evidence row");
        WitnessFact *fact = &evidence->witnesses[i];
        copy_field(fact->name, sizeof(fact->name), fields[1], "witness name");
        if (!valid_name(fact->name)) fail("invalid witness name in evidence");
        copy_field(fact->side, sizeof(fact->side), fields[2], "witness side");
        if (strcmp(fact->side, "baseline") && strcmp(fact->side, "candidate")) fail("invalid witness side in evidence");
        copy_sha(fact->ref, fields[3], 40, "witness Git SHA");
        copy_sha(fact->tree, fields[4], 40, "witness tree SHA");
        parse_result(fields, count, 5, &fact->build);
        copy_field(fact->build_log, sizeof(fact->build_log), fields[9], "witness build log path");
        copy_sha(fact->build_log_sha, fields[10], 64, "witness build log SHA-256");
        parse_result(fields, count, 11, &fact->run);
        copy_field(fact->run_log, sizeof(fact->run_log), fields[15], "witness run log path");
        copy_sha(fact->run_log_sha, fields[16], 64, "witness run log SHA-256");
        copy_sha(fact->artifact_sha, fields[17], 64, "witness artifact SHA-256");
        copy_sha(fact->overlay_sha, fields[18], 64, "witness overlay SHA-256");
        char expected_build[256], expected_run[256];
        snprintf(expected_build, sizeof(expected_build), "logs/%s-%s-build.log", fact->name, fact->side);
        snprintf(expected_run, sizeof(expected_run), "logs/%s-%s-run.log", fact->name, fact->side);
        if (strcmp(fact->build_log, expected_build) || strcmp(fact->run_log, expected_run))
            fail("invalid regression witness log path");
    }
    READ_PAIR(REVIEW_EVIDENCE_RECORD_PACKET_SHA, evidence->packet_sha, sizeof(evidence->packet_sha), "review packet SHA-256");
    copy_sha(evidence->packet_sha, evidence->packet_sha, 64, "review packet SHA-256");
    if (line_index >= line_count) fail("missing evidence seal record");
    split_fields(lines[line_index++], fields, MAX_FIELDS, &count);
    if (count != 2 || strcmp(fields[0], REVIEW_EVIDENCE_RECORD_SEAL)) fail("expected final evidence seal record");
    copy_sha(evidence->seal, fields[1], 64, "evidence seal");
    if (strcmp(evidence->seal, expected_seal)) fail("evidence seal mismatch");
    if (line_index != line_count) fail("extra records after evidence seal");
#undef READ_PAIR
    free(raw);
}

static void verify_log(const char *root, const char *relative, const char *expected)
{
    char path[PATH_CAP], actual[65];
    join_path(path, sizeof(path), root, relative);
    struct stat info;
    if (lstat(path, &info) || !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))
        fail("evidence log is missing or not a regular file: %s", path);
    path_sha(path, actual);
    if (strcmp(actual, expected)) fail("log digest mismatch: %s", path);
}

static void verify_artifact_tree(const char *root, const char *relative, const char *expected)
{
    char path[PATH_CAP], actual[65];
    join_path(path, sizeof(path), root, relative);
    tree_sha(path, actual);
    if (strcmp(actual, expected)) fail("artifact digest mismatch: %s", path);
}

static void require_exact_directory_entries(const char *path, const char *const expected[], size_t expected_count)
{
    struct stat info;
    if (lstat(path, &info) || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))
        fail("evidence directory is missing or not a real directory: %s", path);
    DIR *directory = opendir(path);
    if (!directory) fail("cannot inspect evidence directory %s: %s", path, strerror(errno));
    bool seen[256] = { false };
    if (expected_count > sizeof(seen) / sizeof(seen[0])) fail("too many evidence directory entries");
    size_t count = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            if (errno) fail("cannot read evidence directory %s: %s", path, strerror(errno));
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        size_t i = 0;
        for (; i < expected_count; ++i)
            if (!strcmp(entry->d_name, expected[i])) break;
        if (i == expected_count || seen[i]) {
            closedir(directory);
            fail("unexpected entry in evidence directory %s: %s", path, entry->d_name);
        }
        seen[i] = true;
        ++count;
    }
    if (closedir(directory)) fail("cannot close evidence directory %s", path);
    if (count != expected_count) fail("missing entry in evidence directory %s", path);
}

static const char *entry_basename(const char *relative)
{
    const char *slash = strrchr(relative, '/');
    return slash ? slash + 1 : relative;
}

static void verify_exact_bundle_layout(const Plan *plan, const Evidence *evidence, const char *root)
{
    char path[PATH_CAP], logs[PATH_CAP], artifacts[PATH_CAP], suites[PATH_CAP], witnesses[PATH_CAP];
    join_path(path, sizeof(path), root, REVIEW_EVIDENCE_FILENAME);
    join_path(logs, sizeof(logs), root, "logs");
    join_path(artifacts, sizeof(artifacts), root, "artifacts");
    join_path(suites, sizeof(suites), artifacts, "suites");
    join_path(witnesses, sizeof(witnesses), artifacts, "witnesses");
    struct stat file_info;
    if (lstat(path, &file_info) || !S_ISREG(file_info.st_mode) || S_ISLNK(file_info.st_mode))
        fail("evidence file is missing or not a regular file: %s", path);
    join_path(path, sizeof(path), root, REVIEW_EVIDENCE_PACKET_FILENAME);
    if (lstat(path, &file_info) || !S_ISREG(file_info.st_mode) || S_ISLNK(file_info.st_mode))
        fail("review packet is missing or not a regular file: %s", path);

    const char *root_entries[] = { "logs", "artifacts", REVIEW_EVIDENCE_FILENAME,
                                   REVIEW_EVIDENCE_PACKET_FILENAME };
    const char *artifact_entries[] = { "suites", "witnesses" };
    const char *suite_entries[MAX_SUITES];
    const char *witness_entries[MAX_WITNESSES * 2];
    const char *log_entries[MAX_SUITES + MAX_WITNESSES * 4];
    size_t log_count = 0;
    for (size_t i = 0; i < plan->suite_count; ++i) {
        suite_entries[i] = plan->suites[i].name;
        log_entries[log_count++] = entry_basename(evidence->suites[i].log);
    }
    for (size_t i = 0; i < evidence->witness_fact_count; ++i) {
        witness_entries[i] = NULL;
        log_entries[log_count++] = entry_basename(evidence->witnesses[i].build_log);
        log_entries[log_count++] = entry_basename(evidence->witnesses[i].run_log);
    }
    char witness_names[MAX_WITNESSES * 2][256];
    for (size_t i = 0; i < evidence->witness_fact_count; ++i) {
        snprintf(witness_names[i], sizeof(witness_names[i]), "%s-%s",
                 evidence->witnesses[i].name, evidence->witnesses[i].side);
        witness_entries[i] = witness_names[i];
    }
    require_exact_directory_entries(root, root_entries, sizeof(root_entries) / sizeof(root_entries[0]));
    require_exact_directory_entries(logs, log_entries, log_count);
    require_exact_directory_entries(artifacts, artifact_entries, sizeof(artifact_entries) / sizeof(artifact_entries[0]));
    require_exact_directory_entries(suites, suite_entries, plan->suite_count);
    require_exact_directory_entries(witnesses, witness_entries, evidence->witness_fact_count);
}

static void verify_captured_files(const Plan *plan, const Evidence *evidence, const char *root)
{
    verify_exact_bundle_layout(plan, evidence, root);
    char path[PATH_CAP], actual[65];
    for (size_t i = 0; i < evidence->suite_count; ++i) {
        const SuiteFact *fact = &evidence->suites[i];
        verify_log(root, fact->log, fact->log_sha);
        char artifact[256];
        snprintf(artifact, sizeof(artifact), "artifacts/suites/%s", fact->name);
        verify_artifact_tree(root, artifact, fact->artifact_sha);
        if (result_ok(&fact->result)) validate_artifact_requirements(plan, fact->name, root);
    }
    for (size_t i = 0; i < evidence->witness_fact_count; ++i) {
        const WitnessFact *fact = &evidence->witnesses[i];
        verify_log(root, fact->build_log, fact->build_log_sha);
        verify_log(root, fact->run_log, fact->run_log_sha);
        char artifact[256];
        snprintf(artifact, sizeof(artifact), "artifacts/witnesses/%s-%s", fact->name, fact->side);
        verify_artifact_tree(root, artifact, fact->artifact_sha);
    }
    join_path(path, sizeof(path), root, REVIEW_EVIDENCE_PACKET_FILENAME);
    path_sha(path, actual);
    if (strcmp(actual, evidence->packet_sha)) fail("review packet digest mismatch");
    size_t expected_length = 0, actual_length = 0;
    char *expected_packet = make_review_packet(plan, evidence, root, &expected_length);
    char *actual_packet = read_file(path, &actual_length);
    if (expected_length != actual_length || memcmp(expected_packet, actual_packet, expected_length))
        fail("review packet does not match evidence facts");
    free(expected_packet);
    free(actual_packet);
}

static void verify_plan_binding(const Plan *plan, const Evidence *evidence)
{
    if (strcmp(evidence->plan_sha, plan->plan_sha)) fail("evidence plan digest mismatch");
    if (strcmp(evidence->repo, plan->repo)) fail("evidence repository path mismatch");
    if (strcmp(evidence->candidate, plan->candidate)) fail("evidence candidate mismatch");
    if (evidence->suite_count != plan->suite_count) fail("evidence required suite count mismatch");
    if (evidence->witness_fact_count != plan->witness_count * 2) fail("evidence regression witness count mismatch");
    for (size_t i = 0; i < plan->suite_count; ++i)
        if (strcmp(evidence->suites[i].name, plan->suites[i].name)) fail("evidence required suite list mismatch");
    for (size_t i = 0; i < plan->witness_count; ++i) {
        const Witness *witness = &plan->witnesses[i];
        const WitnessFact *baseline = &evidence->witnesses[i * 2];
        const WitnessFact *candidate = &evidence->witnesses[i * 2 + 1];
        if (strcmp(baseline->name, witness->name) || strcmp(baseline->side, "baseline") ||
            strcmp(baseline->ref, witness->baseline) || strcmp(candidate->name, witness->name) ||
            strcmp(candidate->side, "candidate") || strcmp(candidate->ref, plan->candidate))
            fail("evidence regression witness list mismatch");
        if (strcmp(baseline->overlay_sha, candidate->overlay_sha))
            fail("regression witness overlay identity mismatch: %s", witness->name);
    }
}

static void verify_current_identity(const Plan *plan, const Evidence *evidence)
{
    char head[65], tree[65];
    git_value(plan->repo, "HEAD", head);
    if (strcmp(head, plan->candidate)) fail("candidate HEAD mismatch: expected %s, found %s", plan->candidate, head);
    git_tree(plan->repo, head, tree);
    if (strcmp(tree, evidence->tree)) fail("candidate tree mismatch");
    if (!clean_repo(plan->repo) || strcmp(evidence->clean, "yes")) fail("Git checkout is not clean");
}

static void verify_tool_identities(const Plan *plan, const Evidence *evidence, const char *argv0)
{
    Evidence current = { 0 };
    calculate_toolchain_identity(plan, &current);
    identify_review_tool(argv0, &current);
    if (strcmp(current.toolchain_path, evidence->toolchain_path) ||
        strcmp(current.toolchain_sha, evidence->toolchain_sha) ||
        strcmp(current.toolchain_version_sha, evidence->toolchain_version_sha))
        fail("toolchain identity mismatch");
    if (strcmp(current.review_tool_sha, evidence->review_tool_sha)) fail("review tool identity mismatch");
    calculate_dependency_identities(plan, &current);
    if (current.tool_count != evidence->tool_count) fail("declared tool identity count mismatch");
    for (size_t i = 0; i < current.tool_count; ++i) {
        const ToolFact *actual = &current.tools[i], *recorded = &evidence->tools[i];
        if (strcmp(actual->name, recorded->name) || strcmp(actual->path, recorded->path) ||
            strcmp(actual->sha, recorded->sha) || strcmp(actual->version_sha, recorded->version_sha))
            fail("declared tool identity mismatch: %s", recorded->name);
    }
    if (current.input_count != evidence->input_count) fail("external input identity count mismatch");
    for (size_t i = 0; i < current.input_count; ++i) {
        const InputFact *actual = &current.inputs[i], *recorded = &evidence->inputs[i];
        if (strcmp(actual->name, recorded->name) || strcmp(actual->path, recorded->path) ||
            strcmp(actual->sha, recorded->sha))
            fail("external input identity mismatch: %s", recorded->name);
    }
}

static void verify_recorded_outcomes(const Plan *plan, const Evidence *evidence, const char *root)
{
    for (size_t i = 0; i < plan->scope_count; ++i)
        if (!coverage_available(&plan->scopes[i]))
            fail("required coverage unavailable for witness %s: %s",
                 plan->scopes[i].witness, plan->scopes[i].unavailable);
    for (size_t i = 0; i < evidence->suite_count; ++i) {
        const SuiteFact *suite = &evidence->suites[i];
        if (!result_ok(&suite->result))
            fail("required suite failed, timed out, or was skipped: %s (exit=%d, signal=%d, timeout=%s, skipped=%s; log %s)",
                 suite->name, suite->result.exit_code, suite->result.signal_number,
                 suite->result.timed_out ? "yes" : "no", suite->result.skipped ? "yes" : "no", suite->log);
    }
    for (size_t i = 0; i < plan->witness_count; ++i) {
        const Witness *witness = &plan->witnesses[i];
        const WitnessFact *baseline = &evidence->witnesses[i * 2];
        const WitnessFact *candidate = &evidence->witnesses[i * 2 + 1];
        if (!witness_side_passed(witness, baseline, root))
            fail("baseline witness did not reach its declared symptom: %s (build log %s, run log %s)",
                 witness->name, baseline->build_log, baseline->run_log);
        if (!witness_side_passed(witness, candidate, root))
            fail("candidate witness did not pass: %s (build log %s, run log %s)",
                 witness->name, candidate->build_log, candidate->run_log);
    }
}

static void replay_required_suites(const Plan *plan, const char *root, const char *fixture_base)
{
    for (size_t i = 0; i < plan->suite_count; ++i) {
        SuiteFact replay = { 0 };
        run_suite_capture(plan, &plan->suites[i], root, fixture_base, &replay);
        if (!result_ok(&replay.result))
            fail("trusted required suite replay failed: %s (exit=%d, signal=%d, timeout=%s, skipped=%s; log %s/%s)",
                 plan->suites[i].name, replay.result.exit_code, replay.result.signal_number,
                 replay.result.timed_out ? "yes" : "no", replay.result.skipped ? "yes" : "no",
                 root, replay.log);
    }
}

static void replay_witnesses(const Plan *plan, const Evidence *captured, const char *root,
                             const char *fixture_base)
{
    Evidence replay = { 0 };
    for (size_t i = 0; i < plan->witness_count; ++i) {
        run_witness_side(plan, &plan->witnesses[i], "baseline", plan->witnesses[i].baseline,
                         root, fixture_base, &replay.witnesses[replay.witness_fact_count++]);
        run_witness_side(plan, &plan->witnesses[i], "candidate", plan->candidate,
                         root, fixture_base, &replay.witnesses[replay.witness_fact_count++]);
        for (size_t side = 0; side < 2; ++side) {
            const WitnessFact *recorded = &captured->witnesses[i * 2 + side];
            const WitnessFact *fresh = &replay.witnesses[i * 2 + side];
            if (strcmp(recorded->tree, fresh->tree))
                fail("trusted regression witness tree mismatch: %s %s", plan->witnesses[i].name, fresh->side);
            if (strcmp(recorded->overlay_sha, fresh->overlay_sha))
                fail("trusted regression witness overlay mismatch: %s %s", plan->witnesses[i].name, fresh->side);
        }
        if (strcmp(replay.witnesses[i * 2].overlay_sha, replay.witnesses[i * 2 + 1].overlay_sha))
            fail("trusted regression witness overlay identity mismatch: %s", plan->witnesses[i].name);
        if (!witness_side_passed(&plan->witnesses[i], &replay.witnesses[i * 2], root))
            fail("trusted baseline replay did not reach the declared symptom: %s", plan->witnesses[i].name);
        if (!witness_side_passed(&plan->witnesses[i], &replay.witnesses[i * 2 + 1], root))
            fail("trusted candidate replay did not pass: %s", plan->witnesses[i].name);
    }
}

static void run_verify_command(const char *plan_path, const char *evidence_path,
                               const char *replay_path, const char *argv0)
{
    Plan plan = { 0 };
    Evidence evidence = { 0 };
    parse_plan(plan_path, &plan);
    char root[PATH_CAP];
    safe_scratch_root(evidence_path, root);
    char evidence_file[PATH_CAP];
    join_path(evidence_file, sizeof(evidence_file), root, REVIEW_EVIDENCE_FILENAME);
    parse_evidence_file(evidence_file, &evidence);

    verify_current_identity(&plan, &evidence);
    verify_plan_binding(&plan, &evidence);
    verify_tool_identities(&plan, &evidence, argv0);
    verify_captured_files(&plan, &evidence, root);
    verify_recorded_outcomes(&plan, &evidence, root);
    char fixture_base[PATH_CAP];
    resolve_fixture_temp_base(fixture_base);
    char replay_base[PATH_CAP], replay_root[PATH_CAP];
    safe_scratch_root(replay_path, replay_base);
    create_owned_directory(replay_base, replay_root);
    create_output_layout(replay_root);
    replay_required_suites(&plan, replay_root, fixture_base);
    replay_witnesses(&plan, &evidence, replay_root, fixture_base);
    verify_tool_identities(&plan, &evidence, argv0);
    verify_current_identity(&plan, &evidence);
    remove_owned_directory(replay_root);
    printf("Verified executable review evidence for %s\n", evidence.candidate);
}

static void usage(FILE *stream)
{
    fputs("Usage:\n"
          "  review-tools capture --plan PLAN --output DIRECTORY\n"
          "  review-tools verify --plan PLAN --evidence-dir DIRECTORY --replay-root OBJ_DIRECTORY\n",
          stream);
}

static const char *option_value(int argc, char **argv, int *index, const char *option)
{
    if (*index + 1 >= argc || argv[*index + 1][0] == '-') fail("missing value for %s", option);
    *index += 1;
    return argv[*index];
}

int main(int argc, char **argv)
{
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "help"))) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    if (argc < 2) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    const char *operation = argv[1];
    const char *plan = NULL, *output = NULL, *evidence = NULL, *replay_root = NULL;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--plan")) {
            if (plan) fail("duplicate --plan option");
            plan = option_value(argc, argv, &i, "--plan");
        } else if (!strcmp(argv[i], "--output")) {
            if (output) fail("duplicate --output option");
            output = option_value(argc, argv, &i, "--output");
        } else if (!strcmp(argv[i], "--evidence-dir")) {
            if (evidence) fail("duplicate --evidence-dir option");
            evidence = option_value(argc, argv, &i, "--evidence-dir");
        } else if (!strcmp(argv[i], "--replay-root")) {
            if (replay_root) fail("duplicate --replay-root option");
            replay_root = option_value(argc, argv, &i, "--replay-root");
        } else {
            fail("unknown option: %s", argv[i]);
        }
    }
    if (!strcmp(operation, "capture")) {
        if (!plan || !output || evidence || replay_root) fail("capture requires exactly --plan and --output");
        run_capture_command(plan, output, argv[0]);
    } else if (!strcmp(operation, "verify")) {
        if (!plan || !evidence || !replay_root || output) fail("verify requires exactly --plan, --evidence-dir, and --replay-root");
        run_verify_command(plan, evidence, replay_root, argv[0]);
    } else {
        usage(stderr);
        fail("unknown command: %s", operation);
    }
    return EXIT_SUCCESS;
}

static bool capture_passed(const Plan *plan, const Evidence *evidence, const char *root)
{
    if (strcmp(evidence->clean, "yes") || evidence->suite_count != plan->suite_count ||
        evidence->witness_fact_count != plan->witness_count * 2) return false;
    for (size_t i = 0; i < plan->scope_count; ++i)
        if (!coverage_available(&plan->scopes[i])) return false;
    for (size_t i = 0; i < evidence->suite_count; ++i)
        if (!result_ok(&evidence->suites[i].result)) return false;
    for (size_t i = 0; i < plan->witness_count; ++i) {
        if (strcmp(evidence->witnesses[i * 2].overlay_sha,
                   evidence->witnesses[i * 2 + 1].overlay_sha)) return false;
        if (strcmp(evidence->witnesses[i * 2].side, "baseline") ||
            strcmp(evidence->witnesses[i * 2 + 1].side, "candidate") ||
            !witness_side_passed(&plan->witnesses[i], &evidence->witnesses[i * 2], root) ||
            !witness_side_passed(&plan->witnesses[i], &evidence->witnesses[i * 2 + 1], root)) return false;
    }
    return true;
}

static char *make_review_packet(const Plan *plan, const Evidence *evidence, const char *root, size_t *length)
{
    Buffer packet = { 0 };
    buffer_printf(&packet,
        "# Executable review evidence\n\n"
        "Candidate: `%s`\n\n"
        "Tree: `%s`\n\n"
        "Checkout clean at completion: `%s`\n\n"
        "Toolchain: `%s` (binary SHA-256 `%s`, version output SHA-256 `%s`)\n\n"
        "Review tool SHA-256: `%s`\n\n",
        evidence->candidate, evidence->tree, evidence->clean, evidence->toolchain_path,
        evidence->toolchain_sha, evidence->toolchain_version_sha, evidence->review_tool_sha);
    buffer_append(&packet, "## Declared tools\n\n");
    for (size_t i = 0; i < evidence->tool_count; ++i) {
        const ToolFact *fact = &evidence->tools[i];
        buffer_printf(&packet, "- `%s`: `%s` (binary SHA-256 `%s`, version output SHA-256 `%s`).\n",
                      fact->name, fact->path, fact->sha, fact->version_sha);
    }
    buffer_append(&packet,
        "\n## External inputs\n\n"
        "External inputs use the `cx16agi-external-input-v1` hash domain. Directory inputs add the `directory-root-dotgit-excluded-v1` policy and omit exactly the root `.git` entry; every other entry remains hashed. Regular files use the `regular-file-v1` policy. Artifact-tree hashes do not apply the `.git` exclusion.\n\n");
    for (size_t i = 0; i < evidence->input_count; ++i) {
        const InputFact *fact = &evidence->inputs[i];
        buffer_printf(&packet, "- `%s`: `%s` (tree SHA-256 `%s`).\n", fact->name, fact->path, fact->sha);
    }
    buffer_append(&packet, "\n## Required suites\n\n");
    for (size_t i = 0; i < evidence->suite_count; ++i) {
        const SuiteFact *fact = &evidence->suites[i];
        buffer_printf(&packet, "- `%s`: exit=%d signal=%d timeout=%s skipped=%s; log `%s` (SHA-256 `%s`); artifacts SHA-256 `%s`.\n",
                      fact->name, fact->result.exit_code, fact->result.signal_number,
                      fact->result.timed_out ? "yes" : "no", fact->result.skipped ? "yes" : "no",
                      fact->log, fact->log_sha, fact->artifact_sha);
    }
    if (plan->artifact_requirement_count) {
        buffer_append(&packet, "\n## Required artifacts\n\n");
        for (size_t i = 0; i < plan->artifact_requirement_count; ++i) {
            const ArtifactRequirement *requirement = &plan->artifact_requirements[i];
            const SuiteFact *suite_fact = NULL;
            for (size_t suite = 0; suite < evidence->suite_count; ++suite)
                if (!strcmp(evidence->suites[suite].name, requirement->suite))
                    suite_fact = &evidence->suites[suite];
            buffer_printf(&packet, "- `%s` requires a nonempty regular file matching `%s`.\n",
                          requirement->suite, requirement->pattern);
            if (!suite_fact || !result_ok(&suite_fact->result)) {
                buffer_append(&packet, "  - No successful suite artifact output was recorded.\n");
                continue;
            }
            ArtifactMatch matches[MAX_ARTIFACT_MATCHES];
            size_t match_count = collect_artifact_matches(root, requirement, matches);
            for (size_t match = 0; match < match_count; ++match)
                buffer_printf(&packet, "  - `artifacts/suites/%s/%s` (file SHA-256 `%s`).\n",
                              requirement->suite, matches[match].path, matches[match].sha);
        }
    }
    buffer_append(&packet, "\n## Regression witnesses\n\n");
    for (size_t i = 0; i < plan->witness_count; ++i) {
        const Witness *witness = &plan->witnesses[i];
        const WitnessFact *baseline = &evidence->witnesses[i * 2];
        const WitnessFact *candidate = &evidence->witnesses[i * 2 + 1];
        buffer_printf(&packet, "- `%s` baseline `%s` (tree `%s`, overlay SHA-256 `%s`): %s; build exit=%d, run exit=%d, signal=%d, timeout=%s.\n",
                      witness->name, baseline->ref, baseline->tree,
                      baseline->overlay_sha,
                      witness_side_passed(witness, baseline, root) ? "baseline symptom reached" : "baseline symptom not established",
                      baseline->build.exit_code, baseline->run.exit_code, baseline->run.signal_number,
                      baseline->build.timed_out || baseline->run.timed_out ? "yes" : "no");
        buffer_printf(&packet, "- `%s` candidate `%s` (tree `%s`, overlay SHA-256 `%s`): %s; build exit=%d, run exit=%d, signal=%d, timeout=%s.\n",
                      witness->name, candidate->ref, candidate->tree,
                      candidate->overlay_sha,
                      witness_side_passed(witness, candidate, root) ? "candidate passed" : "candidate did not pass",
                      candidate->build.exit_code, candidate->run.exit_code, candidate->run.signal_number,
                      candidate->build.timed_out || candidate->run.timed_out ? "yes" : "no");
    }
    buffer_append(&packet, "\n## Contract and coverage\n\n");
    for (size_t i = 0; i < plan->scope_count; ++i) {
        const Scope *scope = &plan->scopes[i];
        buffer_printf(&packet,
            "### `%s`\n\n"
            "- Behavior contract: %s\n"
            "- Production entry: %s:%s\n"
            "- Declared production callers: %s\n"
            "- Permanent case: %s\n"
            "- Required suites: ",
            scope->witness, scope->contract, scope->entry_path, scope->entry,
            scope->callers, scope->case_path);
        for (size_t suite = 0; suite < scope->suite_count; ++suite) {
            if (suite) buffer_append(&packet, ", ");
            buffer_append(&packet, scope->suite_names[suite]);
        }
        buffer_printf(&packet,
            "\n- Coverage obligations: %s\n"
            "- Unavailable required coverage: %s\n\n",
            scope->obligations, coverage_available(scope) ? "none" : scope->unavailable);
    }
    buffer_append(&packet,
        "Scope entries, callers, cases, and obligations are declarative. A reviewer must verify that the named permanent case reaches the production entry point and covers each stated obligation.\n\n"
        "\n## Limits\n\n"
        "This packet records executable results for named Git revisions. temporal order cannot be proven from this evidence.\n\n"
        "This is local evidence and does not represent reviewer approval or GitHub readiness.\n");
    if (length) *length = packet.length;
    return packet.data;
}

static char *serialize_evidence(const Evidence *evidence, size_t *length)
{
    Buffer output = { 0 };
    buffer_printf(&output, "%s\n", REVIEW_EVIDENCE_RECORD_MAGIC);
    buffer_printf(&output, "%s|%s\n%s|%s\n%s|%s\n%s|%s\n%s|%s\n",
                  REVIEW_EVIDENCE_RECORD_PLAN_SHA, evidence->plan_sha,
                  REVIEW_EVIDENCE_RECORD_REPO, evidence->repo,
                  REVIEW_EVIDENCE_RECORD_CANDIDATE, evidence->candidate,
                  REVIEW_EVIDENCE_RECORD_TREE, evidence->tree,
                  REVIEW_EVIDENCE_RECORD_CLEAN, evidence->clean);
    buffer_printf(&output, "%s|%s\n%s|%s\n%s|%s\n%s|%s\n",
                  REVIEW_EVIDENCE_RECORD_TOOLCHAIN_PATH, evidence->toolchain_path,
                  REVIEW_EVIDENCE_RECORD_TOOLCHAIN_SHA, evidence->toolchain_sha,
                  REVIEW_EVIDENCE_RECORD_TOOLCHAIN_VERSION_SHA, evidence->toolchain_version_sha,
                  REVIEW_EVIDENCE_RECORD_REVIEW_TOOL_SHA, evidence->review_tool_sha);
    buffer_printf(&output, "%s|%zu\n", REVIEW_EVIDENCE_RECORD_TOOL_COUNT, evidence->tool_count);
    for (size_t i = 0; i < evidence->tool_count; ++i) {
        const ToolFact *fact = &evidence->tools[i];
        buffer_printf(&output, "%s|%s|%s|%s|%s\n", REVIEW_EVIDENCE_RECORD_TOOL,
                      fact->name, fact->path, fact->sha, fact->version_sha);
    }
    buffer_printf(&output, "%s|%zu\n", REVIEW_EVIDENCE_RECORD_INPUT_COUNT, evidence->input_count);
    for (size_t i = 0; i < evidence->input_count; ++i) {
        const InputFact *fact = &evidence->inputs[i];
        buffer_printf(&output, "%s|%s|%s|%s\n", REVIEW_EVIDENCE_RECORD_INPUT,
                      fact->name, fact->path, fact->sha);
    }
    buffer_printf(&output, "%s|%zu\n", REVIEW_EVIDENCE_RECORD_SUITE_COUNT, evidence->suite_count);
    for (size_t i = 0; i < evidence->suite_count; ++i) {
        const SuiteFact *fact = &evidence->suites[i];
        buffer_printf(&output, "%s|%s", REVIEW_EVIDENCE_RECORD_SUITE, fact->name);
        append_result(&output, &fact->result);
        buffer_printf(&output, "|%s|%s|%s\n", fact->log, fact->log_sha, fact->artifact_sha);
    }
    buffer_printf(&output, "%s|%zu\n", REVIEW_EVIDENCE_RECORD_WITNESS_COUNT, evidence->witness_fact_count);
    for (size_t i = 0; i < evidence->witness_fact_count; ++i) {
        const WitnessFact *fact = &evidence->witnesses[i];
        buffer_printf(&output, "%s|%s|%s|%s|%s", REVIEW_EVIDENCE_RECORD_WITNESS,
                      fact->name, fact->side, fact->ref, fact->tree);
        append_result(&output, &fact->build);
        buffer_printf(&output, "|%s|%s", fact->build_log, fact->build_log_sha);
        append_result(&output, &fact->run);
        buffer_printf(&output, "|%s|%s|%s|%s\n", fact->run_log, fact->run_log_sha,
                      fact->artifact_sha, fact->overlay_sha);
    }
    buffer_printf(&output, "%s|%s\n", REVIEW_EVIDENCE_RECORD_PACKET_SHA, evidence->packet_sha);
    review_sha256 hash;
    unsigned char digest[32];
    char seal[65];
    review_sha256_init(&hash);
    review_sha256_update(&hash, output.data, output.length);
    review_sha256_final(&hash, digest);
    review_sha256_hex(digest, seal);
    buffer_printf(&output, "%s|%s\n", REVIEW_EVIDENCE_RECORD_SEAL, seal);
    if (length) *length = output.length;
    return output.data;
}

static void run_capture_command(const char *plan_path, const char *output_path, const char *argv0)
{
    Plan plan = { 0 };
    Evidence evidence = { 0 };
    parse_plan(plan_path, &plan);
    char fixture_base[PATH_CAP];
    resolve_fixture_temp_base(fixture_base);
    if (mkdir(output_path, 0700)) fail("cannot create evidence output %s: %s", output_path, strerror(errno));
    char output_root[PATH_CAP];
    if (!realpath(output_path, output_root)) fail("cannot resolve evidence output %s", output_path);
    create_output_layout(output_root);

    char head[65], tree[65];
    require_identity(&plan, head, tree);
    copy_field(evidence.plan_sha, sizeof(evidence.plan_sha), plan.plan_sha, "plan SHA-256");
    copy_field(evidence.repo, sizeof(evidence.repo), plan.repo, "repository path");
    copy_field(evidence.candidate, sizeof(evidence.candidate), head, "candidate Git SHA");
    copy_field(evidence.tree, sizeof(evidence.tree), tree, "candidate tree SHA");
    copy_field(evidence.clean, sizeof(evidence.clean), "yes", "clean checkout status");
    calculate_toolchain_identity(&plan, &evidence);
    identify_review_tool(argv0, &evidence);
    calculate_dependency_identities(&plan, &evidence);

    evidence.suite_count = plan.suite_count;
    for (size_t i = 0; i < plan.suite_count; ++i)
        run_suite_capture(&plan, &plan.suites[i], output_root, fixture_base,
                          &evidence.suites[i]);

    for (size_t i = 0; i < plan.witness_count; ++i) {
        run_witness_side(&plan, &plan.witnesses[i], "baseline", plan.witnesses[i].baseline,
                         output_root, fixture_base,
                         &evidence.witnesses[evidence.witness_fact_count++]);
        run_witness_side(&plan, &plan.witnesses[i], "candidate", plan.candidate,
                         output_root, fixture_base,
                         &evidence.witnesses[evidence.witness_fact_count++]);
    }

    verify_tool_identities(&plan, &evidence, argv0);

    char final_head[65], final_tree[65];
    git_value(plan.repo, "HEAD", final_head);
    git_tree(plan.repo, final_head, final_tree);
    bool clean = !strcmp(final_head, head) && !strcmp(final_tree, tree) && clean_repo(plan.repo);
    copy_field(evidence.clean, sizeof(evidence.clean), clean ? "yes" : "no", "clean checkout status");

    size_t packet_length = 0;
    char *packet = make_review_packet(&plan, &evidence, output_root, &packet_length);
    char packet_path[PATH_CAP];
    join_path(packet_path, sizeof(packet_path), output_root, REVIEW_EVIDENCE_PACKET_FILENAME);
    write_file(packet_path, packet, packet_length);
    free(packet);
    path_sha(packet_path, evidence.packet_sha);

    size_t evidence_length = 0;
    char *serialized = serialize_evidence(&evidence, &evidence_length);
    char evidence_path[PATH_CAP];
    join_path(evidence_path, sizeof(evidence_path), output_root, REVIEW_EVIDENCE_FILENAME);
    write_file(evidence_path, serialized, evidence_length);
    free(serialized);

    if (!clean) fail("checkout identity or cleanliness changed during evidence capture");
    for (size_t i = 0; i < evidence.suite_count; ++i) {
        const SuiteFact *suite = &evidence.suites[i];
        if (!result_ok(&suite->result))
            fail("required suite failed, timed out, or was skipped: %s (exit=%d, signal=%d, timeout=%s, skipped=%s; log %s/%s)",
                 suite->name, suite->result.exit_code, suite->result.signal_number,
                 suite->result.timed_out ? "yes" : "no", suite->result.skipped ? "yes" : "no",
                 output_root, suite->log);
    }
    if (!capture_passed(&plan, &evidence, output_root)) {
        for (size_t i = 0; i < plan.scope_count; ++i)
            if (!coverage_available(&plan.scopes[i]))
                fail("required coverage unavailable for witness %s: %s",
                     plan.scopes[i].witness, plan.scopes[i].unavailable);
        for (size_t i = 0; i < plan.witness_count; ++i) {
            const WitnessFact *baseline = &evidence.witnesses[i * 2];
            const WitnessFact *candidate = &evidence.witnesses[i * 2 + 1];
            if (!witness_side_passed(&plan.witnesses[i], baseline, output_root))
                fail("baseline regression witness failed: %s (build log %s/%s, run log %s/%s)",
                     plan.witnesses[i].name, output_root, baseline->build_log,
                     output_root, baseline->run_log);
            if (!witness_side_passed(&plan.witnesses[i], candidate, output_root))
                fail("candidate regression witness failed: %s (build log %s/%s, run log %s/%s)",
                     plan.witnesses[i].name, output_root, candidate->build_log,
                     output_root, candidate->run_log);
        }
        fail("one or more regression witness identities were inconsistent");
    }
    printf("Captured executable review evidence for %s\n", evidence.candidate);
}
