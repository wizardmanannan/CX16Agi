#define _POSIX_C_SOURCE 200809L
#include "review_policy.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define REVIEW_JSON_LIMIT (32u * 1024u * 1024u)
#define REVIEW_JSON_NODE_LIMIT 500000u
#define REVIEW_POLICY_VALUE_LIMIT 128
#define REVIEW_POLICY_LIST_LIMIT 32
#define REVIEW_REVIEWER_LIMIT 4096
#ifndef REVIEW_COMMAND_TIMEOUT_MS
#define REVIEW_COMMAND_TIMEOUT_MS 60000
#endif

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_type;

typedef struct {
    json_type type;
    size_t start;
    size_t end;
    int first_child;
    int last_child;
    int next_sibling;
    size_t child_count;
} json_node;

typedef struct {
    const char *text;
    char *owned_text;
    size_t length;
    json_node *nodes;
    size_t count;
    size_t capacity;
    char error[256];
} json_document;

typedef enum {
    GH_OK,
    GH_NOT_FOUND,
    GH_FAILED
} gh_status;

typedef struct {
    char *stdout_text;
    char *stderr_text;
    int exit_status;
} command_result;

typedef struct {
    char branch[REVIEW_POLICY_VALUE_LIMIT];
    char check_app_slug[REVIEW_POLICY_VALUE_LIMIT];
    char owners[REVIEW_POLICY_LIST_LIMIT][REVIEW_POLICY_VALUE_LIMIT];
    size_t owner_count;
    char checks[REVIEW_POLICY_LIST_LIMIT][REVIEW_POLICY_VALUE_LIMIT];
    size_t check_count;
    bool strict;
    bool dismiss_stale_reviews;
    bool require_code_owner_reviews;
    int required_approving_review_count;
    bool require_last_push_approval;
    bool enforce_admins;
    bool require_conversation_resolution;
    bool allow_force_pushes;
    bool allow_deletions;
} review_policy;

typedef struct {
    char login[REVIEW_POLICY_VALUE_LIMIT];
    char association[32];
    char state[32];
    char commit_id[65];
    char submitted_at[40];
    long long id;
} reviewer_state;

typedef struct {
    char context[REVIEW_POLICY_VALUE_LIMIT];
    long long app_id;
} observed_check;

typedef struct {
    observed_check checks[REVIEW_POLICY_LIST_LIMIT];
    size_t count;
} observed_checks;

static void set_error(char *error, size_t capacity, const char *message)
{
    if (capacity) snprintf(error, capacity, "%s", message);
}

static bool append_bytes(char **buffer, size_t *used, size_t *capacity,
                         const char *bytes, size_t count)
{
    if (count > REVIEW_JSON_LIMIT - *used) return false;
    if (*used + count + 1 > *capacity) {
        size_t next = *capacity ? *capacity : 4096;
        while (next < *used + count + 1) {
            if (next > REVIEW_JSON_LIMIT / 2) {
                next = REVIEW_JSON_LIMIT + 1;
                break;
            }
            next *= 2;
        }
        if (next > REVIEW_JSON_LIMIT + 1) return false;
        char *grown = realloc(*buffer, next);
        if (!grown) return false;
        *buffer = grown;
        *capacity = next;
    }
    memcpy(*buffer + *used, bytes, count);
    *used += count;
    (*buffer)[*used] = '\0';
    return true;
}

static void command_result_free(command_result *result)
{
    free(result->stdout_text);
    free(result->stderr_text);
    memset(result, 0, sizeof(*result));
}

static bool read_pipe(int *descriptor, char **buffer, size_t *used, size_t *capacity)
{
    char chunk[8192];
    ssize_t count = read(*descriptor, chunk, sizeof(chunk));
    if (count > 0) return append_bytes(buffer, used, capacity, chunk, (size_t)count);
    if (!count) {
        close(*descriptor);
        *descriptor = -1;
        return true;
    }
    if (errno == EINTR || errno == EAGAIN) return true;
    return false;
}

static bool poll_timeout(const struct timespec *started, int *timeout_ms)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return false;
    long long elapsed_ms = (long long)(now.tv_sec - started->tv_sec) * 1000 +
        (now.tv_nsec - started->tv_nsec) / 1000000;
    if (elapsed_ms < 0) elapsed_ms = 0;
    *timeout_ms = elapsed_ms >= REVIEW_COMMAND_TIMEOUT_MS ? 0 :
        REVIEW_COMMAND_TIMEOUT_MS - (int)elapsed_ms;
    return true;
}

static void kill_child_group(pid_t child, bool group_ready)
{
    if (group_ready) {
        if (!kill(-child, SIGKILL) || errno == ESRCH) return;
    }
    (void)kill(child, SIGKILL);
}

static bool wait_for_child(pid_t child, int *status)
{
    for (;;) {
        pid_t waited = waitpid(child, status, 0);
        if (waited == child) return true;
        if (waited < 0 && errno == EINTR) continue;
        return false;
    }
}

static bool run_argv(char *const argv[], command_result *result)
{
    int output_pipe[2], error_pipe[2];
    memset(result, 0, sizeof(*result));
    result->exit_status = -1;
    if (pipe(output_pipe)) return false;
    if (pipe(error_pipe)) {
        close(output_pipe[0]); close(output_pipe[1]);
        return false;
    }
    pid_t child = fork();
    if (child < 0) {
        close(output_pipe[0]); close(output_pipe[1]);
        close(error_pipe[0]); close(error_pipe[1]);
        return false;
    }
    if (!child) {
        if (setpgid(0, 0)) _Exit(126);
        close(output_pipe[0]); close(error_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(error_pipe[1], STDERR_FILENO) < 0) _Exit(126);
        close(output_pipe[1]); close(error_pipe[1]);
        execvp(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute %s: %s\n", argv[0], strerror(errno));
        _Exit(127);
    }

    bool group_ready = false;
    bool group_setup_ok = true;
    if (!setpgid(child, child) || errno == EACCES) group_ready = true;
    else if (errno == ESRCH) {
        if (!kill(-child, 0) || errno == EPERM) group_ready = true;
    } else group_setup_ok = false;
    if (!group_setup_ok) {
        kill_child_group(child, false);
        close(output_pipe[0]); close(output_pipe[1]);
        close(error_pipe[0]); close(error_pipe[1]);
        int status;
        (void)wait_for_child(child, &status);
        return false;
    }

    close(output_pipe[1]); close(error_pipe[1]);
    struct pollfd descriptors[2] = {
        { .fd = output_pipe[0], .events = POLLIN | POLLHUP },
        { .fd = error_pipe[0], .events = POLLIN | POLLHUP }
    };
    size_t output_used = 0, output_capacity = 0;
    size_t error_used = 0, error_capacity = 0;
    int open_pipes = 2;
    bool read_ok = true;
    struct timespec started;
    if (clock_gettime(CLOCK_MONOTONIC, &started)) read_ok = false;
    while (open_pipes) {
        int remaining_ms;
        if (!poll_timeout(&started, &remaining_ms)) { read_ok = false; break; }
        if (!remaining_ms) { read_ok = false; break; }
        int ready = poll(descriptors, 2, remaining_ms < 1000 ? remaining_ms : 1000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            read_ok = false;
            break;
        }
        if (!ready) continue;
        for (size_t i = 0; i < 2; ++i) {
            if (descriptors[i].fd < 0 ||
                !(descriptors[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) continue;
            int *fd = &descriptors[i].fd;
            bool ok = i ? read_pipe(fd, &result->stderr_text, &error_used, &error_capacity) :
                          read_pipe(fd, &result->stdout_text, &output_used, &output_capacity);
            if (!ok) { read_ok = false; break; }
            if (*fd < 0) --open_pipes;
        }
        if (!read_ok) break;
    }
    if (!read_ok) kill_child_group(child, group_ready);
    if (descriptors[0].fd >= 0) close(descriptors[0].fd);
    if (descriptors[1].fd >= 0) close(descriptors[1].fd);
    int status = 0;
    bool waited = false;
    while (read_ok) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) { waited = true; break; }
        if (result < 0 && errno != EINTR) { read_ok = false; break; }
        int remaining_ms;
        if (!poll_timeout(&started, &remaining_ms)) { read_ok = false; break; }
        if (!remaining_ms) { read_ok = false; break; }
        struct timespec delay = { .tv_sec = 0, .tv_nsec = 10000000 };
        nanosleep(&delay, NULL);
    }
    if (!read_ok) kill_child_group(child, group_ready);
    if (!waited) waited = wait_for_child(child, &status);
    if (!result->stdout_text) result->stdout_text = calloc(1, 1);
    if (!result->stderr_text) result->stderr_text = calloc(1, 1);
    if (!result->stdout_text || !result->stderr_text || !read_ok || !waited || !WIFEXITED(status)) return false;
    result->exit_status = WEXITSTATUS(status);
    return true;
}

static gh_status call_gh(char *const argv[], command_result *result, char *error, size_t error_capacity)
{
    if (!run_argv(argv, result)) {
        set_error(error, error_capacity, "could not read GitHub CLI response");
        command_result_free(result);
        return GH_FAILED;
    }
    if (!result->exit_status) return GH_OK;
    if (strstr(result->stderr_text, "404") || strstr(result->stderr_text, "Not Found")) return GH_NOT_FOUND;
    if (error_capacity) {
        snprintf(error, error_capacity, "gh api failed: %.220s",
                 result->stderr_text[0] ? result->stderr_text : "nonzero exit status");
    }
    return GH_FAILED;
}

static void skip_space(const json_document *document, size_t *position)
{
    while (*position < document->length &&
           (document->text[*position] == ' ' || document->text[*position] == '\t' ||
            document->text[*position] == '\r' || document->text[*position] == '\n')) ++*position;
}

static int json_add_node(json_document *document, json_type type, size_t start)
{
    if (document->count == REVIEW_JSON_NODE_LIMIT) return -1;
    if (document->count == document->capacity) {
        size_t capacity = document->capacity ? document->capacity * 2 : 256;
        if (capacity > REVIEW_JSON_NODE_LIMIT) capacity = REVIEW_JSON_NODE_LIMIT;
        json_node *nodes = realloc(document->nodes, capacity * sizeof(*nodes));
        if (!nodes) return -1;
        document->nodes = nodes;
        document->capacity = capacity;
    }
    int index = (int)document->count++;
    document->nodes[index] = (json_node){
        .type = type,
        .start = start,
        .end = start,
        .first_child = -1,
        .last_child = -1,
        .next_sibling = -1,
        .child_count = 0
    };
    return index;
}

static void json_link_child(json_document *document, int parent, int child)
{
    json_node *object = &document->nodes[parent];
    if (object->last_child >= 0) document->nodes[object->last_child].next_sibling = child;
    else object->first_child = child;
    object->last_child = child;
    ++object->child_count;
}

static bool parse_string_token(json_document *document, size_t *position, int *index)
{
    size_t start = *position;
    if (start >= document->length || document->text[start] != '"') return false;
    int node = json_add_node(document, JSON_STRING, start);
    if (node < 0) return false;
    ++*position;
    while (*position < document->length) {
        unsigned char ch = (unsigned char)document->text[*position];
        if (ch == '"') {
            ++*position;
            document->nodes[node].end = *position;
            *index = node;
            return true;
        }
        if (ch < 0x20) return false;
        if (ch != '\\') { ++*position; continue; }
        if (++*position >= document->length) return false;
        ch = (unsigned char)document->text[(*position)++];
        if (ch == '"' || ch == '\\' || ch == '/' || ch == 'b' || ch == 'f' || ch == 'n' || ch == 'r' || ch == 't') continue;
        if (ch != 'u' || *position + 4 > document->length) return false;
        unsigned value = 0;
        for (size_t i = 0; i < 4; ++i) {
            unsigned char digit = (unsigned char)document->text[*position + i];
            unsigned parsed = digit >= '0' && digit <= '9' ? digit - '0' :
                digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 :
                digit >= 'A' && digit <= 'F' ? digit - 'A' + 10 : 16;
            if (parsed > 15) return false;
            value = (value << 4) | parsed;
        }
        *position += 4;
        if (value >= 0xD800 && value <= 0xDBFF) {
            if (*position + 6 > document->length || document->text[*position] != '\\' || document->text[*position + 1] != 'u') return false;
            *position += 2;
            unsigned low = 0;
            for (size_t i = 0; i < 4; ++i) {
                unsigned char digit = (unsigned char)document->text[*position + i];
                unsigned parsed = digit >= '0' && digit <= '9' ? digit - '0' :
                    digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 :
                    digit >= 'A' && digit <= 'F' ? digit - 'A' + 10 : 16;
                if (parsed > 15) return false;
                low = (low << 4) | parsed;
            }
            *position += 4;
            if (low < 0xDC00 || low > 0xDFFF) return false;
        } else if (value >= 0xDC00 && value <= 0xDFFF) return false;
    }
    return false;
}

static bool parse_number_token(json_document *document, size_t *position, int *index)
{
    size_t start = *position;
    if (document->text[*position] == '-') ++*position;
    if (*position >= document->length) return false;
    if (document->text[*position] == '0') ++*position;
    else {
        if (document->text[*position] < '1' || document->text[*position] > '9') return false;
        while (*position < document->length && isdigit((unsigned char)document->text[*position])) ++*position;
    }
    if (*position < document->length && document->text[*position] == '.') {
        ++*position;
        size_t digits = *position;
        while (*position < document->length && isdigit((unsigned char)document->text[*position])) ++*position;
        if (*position == digits) return false;
    }
    if (*position < document->length && (document->text[*position] == 'e' || document->text[*position] == 'E')) {
        ++*position;
        if (*position < document->length && (document->text[*position] == '+' || document->text[*position] == '-')) ++*position;
        size_t digits = *position;
        while (*position < document->length && isdigit((unsigned char)document->text[*position])) ++*position;
        if (*position == digits) return false;
    }
    int node = json_add_node(document, JSON_NUMBER, start);
    if (node < 0) return false;
    document->nodes[node].end = *position;
    *index = node;
    return true;
}

static bool parse_value(json_document *document, size_t *position, unsigned depth, int *index)
{
    if (depth > 64) return false;
    skip_space(document, position);
    if (*position >= document->length) return false;
    char ch = document->text[*position];
    if (ch == '"') return parse_string_token(document, position, index);
    if (ch == '-' || (ch >= '0' && ch <= '9')) return parse_number_token(document, position, index);
    if (ch == '{' || ch == '[') {
        json_type type = ch == '{' ? JSON_OBJECT : JSON_ARRAY;
        int container = json_add_node(document, type, *position);
        if (container < 0) return false;
        ++*position;
        skip_space(document, position);
        char close = ch == '{' ? '}' : ']';
        if (*position < document->length && document->text[*position] == close) {
            document->nodes[container].end = ++*position;
            *index = container;
            return true;
        }
        for (;;) {
            int child;
            if (type == JSON_OBJECT) {
                if (!parse_string_token(document, position, &child)) return false;
                json_link_child(document, container, child);
                skip_space(document, position);
                if (*position >= document->length || document->text[(*position)++] != ':') return false;
                if (!parse_value(document, position, depth + 1, &child)) return false;
                json_link_child(document, container, child);
            } else {
                if (!parse_value(document, position, depth + 1, &child)) return false;
                json_link_child(document, container, child);
            }
            skip_space(document, position);
            if (*position >= document->length) return false;
            char delimiter = document->text[(*position)++];
            if (delimiter == close) break;
            if (delimiter != ',') return false;
            skip_space(document, position);
        }
        document->nodes[container].end = *position;
        *index = container;
        return true;
    }
    json_type primitive_type;
    const char *primitive;
    if (ch == 't') { primitive_type = JSON_TRUE; primitive = "true"; }
    else if (ch == 'f') { primitive_type = JSON_FALSE; primitive = "false"; }
    else if (ch == 'n') { primitive_type = JSON_NULL; primitive = "null"; }
    else return false;
    size_t length = strlen(primitive);
    if (*position + length > document->length || memcmp(document->text + *position, primitive, length)) return false;
    int node = json_add_node(document, primitive_type, *position);
    if (node < 0) return false;
    *position += length;
    document->nodes[node].end = *position;
    *index = node;
    return true;
}

static bool json_parse(json_document *document, const char *text, size_t length)
{
    memset(document, 0, sizeof(*document));
    document->text = text;
    document->length = length;
    if (!text || length > REVIEW_JSON_LIMIT) {
        set_error(document->error, sizeof(document->error), "JSON response is too large or missing");
        return false;
    }
    size_t position = 0;
    int root;
    if (!parse_value(document, &position, 0, &root)) {
        set_error(document->error, sizeof(document->error), "invalid JSON response");
        return false;
    }
    skip_space(document, &position);
    if (position != length) {
        set_error(document->error, sizeof(document->error), "trailing data after JSON response");
        return false;
    }
    return true;
}

static void json_free(json_document *document)
{
    free(document->owned_text);
    free(document->nodes);
    memset(document, 0, sizeof(*document));
}

static bool append_codepoint(char *destination, size_t capacity, size_t *used, unsigned codepoint)
{
    unsigned char bytes[4];
    size_t count;
    if (codepoint <= 0x7F) { bytes[0] = (unsigned char)codepoint; count = 1; }
    else if (codepoint <= 0x7FF) {
        bytes[0] = (unsigned char)(0xC0 | (codepoint >> 6));
        bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3F)); count = 2;
    } else if (codepoint <= 0xFFFF) {
        bytes[0] = (unsigned char)(0xE0 | (codepoint >> 12));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3F)); count = 3;
    } else {
        bytes[0] = (unsigned char)(0xF0 | (codepoint >> 18));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
        bytes[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3F)); count = 4;
    }
    if (*used + count >= capacity) return false;
    memcpy(destination + *used, bytes, count);
    *used += count;
    destination[*used] = '\0';
    return true;
}

static bool json_string(const json_document *document, int index, char *destination, size_t capacity)
{
    if (index < 0 || (size_t)index >= document->count || document->nodes[index].type != JSON_STRING || capacity == 0) return false;
    const json_node *node = &document->nodes[index];
    size_t position = node->start + 1, end = node->end - 1, used = 0;
    destination[0] = '\0';
    while (position < end) {
        unsigned char ch = (unsigned char)document->text[position++];
        if (ch != '\\') {
            if (used + 1 >= capacity) return false;
            destination[used++] = (char)ch;
            destination[used] = '\0';
            continue;
        }
        ch = (unsigned char)document->text[position++];
        unsigned codepoint;
        switch (ch) {
            case '"': codepoint = '"'; break;
            case '\\': codepoint = '\\'; break;
            case '/': codepoint = '/'; break;
            case 'b': codepoint = '\b'; break;
            case 'f': codepoint = '\f'; break;
            case 'n': codepoint = '\n'; break;
            case 'r': codepoint = '\r'; break;
            case 't': codepoint = '\t'; break;
            case 'u': {
                codepoint = 0;
                for (size_t i = 0; i < 4; ++i) {
                    unsigned char digit = (unsigned char)document->text[position++];
                    codepoint = (codepoint << 4) | (digit >= '0' && digit <= '9' ? digit - '0' :
                        digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit - 'A' + 10);
                }
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    position += 2;
                    unsigned low = 0;
                    for (size_t i = 0; i < 4; ++i) {
                        unsigned char digit = (unsigned char)document->text[position++];
                        low = (low << 4) | (digit >= '0' && digit <= '9' ? digit - '0' :
                            digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit - 'A' + 10);
                    }
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                }
                break;
            }
            default: return false;
        }
        if (!append_codepoint(destination, capacity, &used, codepoint)) return false;
    }
    return true;
}

static int json_object_get(const json_document *document, int object, const char *key)
{
    if (object < 0 || (size_t)object >= document->count || document->nodes[object].type != JSON_OBJECT) return -1;
    int name = document->nodes[object].first_child;
    while (name >= 0) {
        int value = document->nodes[name].next_sibling;
        char candidate[256];
        if (value < 0) return -1;
        if (json_string(document, name, candidate, sizeof(candidate)) && !strcmp(candidate, key)) return value;
        name = document->nodes[value].next_sibling;
    }
    return -1;
}

static bool json_get_string(const json_document *document, int object, const char *key,
                            char *destination, size_t capacity)
{
    return json_string(document, json_object_get(document, object, key), destination, capacity);
}

static bool json_number(const json_document *document, int index, long long *value)
{
    if (index < 0 || (size_t)index >= document->count || document->nodes[index].type != JSON_NUMBER) return false;
    const json_node *node = &document->nodes[index];
    char text[64];
    size_t length = node->end - node->start;
    if (!length || length >= sizeof(text)) return false;
    memcpy(text, document->text + node->start, length); text[length] = '\0';
    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (errno || !end || *end) return false;
    *value = parsed;
    return true;
}

static bool json_get_number(const json_document *document, int object, const char *key, long long *value)
{
    return json_number(document, json_object_get(document, object, key), value);
}

static bool json_get_boolean(const json_document *document, int object, const char *key, bool *value)
{
    int node = json_object_get(document, object, key);
    if (node < 0) return false;
    if (document->nodes[node].type == JSON_TRUE) { *value = true; return true; }
    if (document->nodes[node].type == JSON_FALSE) { *value = false; return true; }
    return false;
}

static bool copy_if(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (!capacity || length >= capacity) return false;
    memcpy(destination, source, length + 1);
    return true;
}

static bool api_segment(const char *name, bool allow_bot_suffix)
{
    if (!name || !*name) return false;
    for (const unsigned char *ch = (const unsigned char *)name; *ch; ++ch) {
        if (isalnum(*ch) || *ch == '-' || *ch == '_' || *ch == '.') continue;
        if (allow_bot_suffix && (*ch == '[' || *ch == ']')) continue;
        return false;
    }
    return strcmp(name, ".") && strcmp(name, "..");
}

static bool check_context(const char *name)
{
    if (!name || !*name) return false;
    for (const unsigned char *ch = (const unsigned char *)name; *ch; ++ch) {
        if (isalnum(*ch) || *ch == '-' || *ch == '_' || *ch == '.' || *ch == '(' || *ch == ')' || *ch == ' ' || *ch == '/') continue;
        return false;
    }
    return true;
}

static bool valid_sha(const char *sha)
{
    size_t length = strlen(sha);
    if (length != 40 && length != 64) return false;
    for (size_t i = 0; i < length; ++i)
        if (!isxdigit((unsigned char)sha[i])) return false;
    return true;
}

static bool valid_repo(const char *repo, char *owner, size_t owner_capacity,
                       char *name, size_t name_capacity)
{
    const char *slash = strchr(repo, '/');
    if (!slash || slash == repo || !slash[1] || strchr(slash + 1, '/')) return false;
    size_t owner_length = (size_t)(slash - repo);
    if (owner_length >= owner_capacity || strlen(slash + 1) >= name_capacity) return false;
    memcpy(owner, repo, owner_length); owner[owner_length] = '\0';
    memcpy(name, slash + 1, strlen(slash + 1) + 1);
    return api_segment(owner, false) && api_segment(name, false);
}

static bool key_is_allowed(const char *key, const char *const *allowed, size_t count)
{
    for (size_t i = 0; i < count; ++i) if (!strcmp(key, allowed[i])) return true;
    return false;
}

static bool object_has_only_keys(const json_document *document, int object,
                                 const char *const *allowed, size_t count,
                                 char *error, size_t error_capacity)
{
    if (object < 0 || (size_t)object >= document->count || document->nodes[object].type != JSON_OBJECT) {
        set_error(error, error_capacity, "policy field must be an object");
        return false;
    }
    int key = document->nodes[object].first_child;
    while (key >= 0) {
        char name[256];
        int value = document->nodes[key].next_sibling;
        if (value < 0 || !json_string(document, key, name, sizeof(name))) {
            set_error(error, error_capacity, "policy contains an invalid field name");
            return false;
        }
        if (!key_is_allowed(name, allowed, count)) {
            snprintf(error, error_capacity, "unexpected policy field: %.180s", name);
            return false;
        }
        key = document->nodes[value].next_sibling;
    }
    return true;
}

static bool parse_string_array(const json_document *document, int array,
                               char values[][REVIEW_POLICY_VALUE_LIMIT], size_t *count,
                               bool checks, char *error, size_t error_capacity)
{
    if (array < 0 || (size_t)array >= document->count || document->nodes[array].type != JSON_ARRAY) {
        set_error(error, error_capacity, "policy list must be an array");
        return false;
    }
    *count = 0;
    int child = document->nodes[array].first_child;
    while (child >= 0) {
        if (*count == REVIEW_POLICY_LIST_LIMIT || !json_string(document, child, values[*count], REVIEW_POLICY_VALUE_LIMIT) ||
            (checks ? !check_context(values[*count]) : !api_segment(values[*count], true))) {
            set_error(error, error_capacity, "policy list has an invalid or excessive value");
            return false;
        }
        if (checks && (!strcmp(values[*count], "review-policy") || !strncmp(values[*count], "review-policy ", 14))) {
            set_error(error, error_capacity, "the readiness aggregate cannot require its own check context");
            return false;
        }
        for (size_t i = 0; i < *count; ++i) {
            if (!strcasecmp(values[i], values[*count])) {
                set_error(error, error_capacity, "policy list contains a duplicate value");
                return false;
            }
        }
        ++*count;
        child = document->nodes[child].next_sibling;
    }
    if (!*count) {
        set_error(error, error_capacity, "policy list cannot be empty");
        return false;
    }
    return true;
}

static bool parse_policy_document(const json_document *document, review_policy *policy,
                                  char *error, size_t error_capacity)
{
    static const char *const root_keys[] = {
        "schema", "branch", "designated_owners", "required_checks", "required_check_app_slug", "protection"
    };
    static const char *const protection_keys[] = {
        "strict", "dismiss_stale_reviews", "require_code_owner_reviews", "required_approving_review_count",
        "require_last_push_approval", "enforce_admins", "require_conversation_resolution",
        "allow_force_pushes", "allow_deletions"
    };
    memset(policy, 0, sizeof(*policy));
    int root = 0;
    char schema[64];
    bool ok = object_has_only_keys(document, root, root_keys, sizeof(root_keys) / sizeof(root_keys[0]), error, error_capacity) &&
        json_get_string(document, root, "schema", schema, sizeof(schema)) &&
        !strcmp(schema, "cx16agi-review-policy-v1") &&
        json_get_string(document, root, "branch", policy->branch, sizeof(policy->branch)) &&
        api_segment(policy->branch, false) &&
        json_get_string(document, root, "required_check_app_slug", policy->check_app_slug,
                        sizeof(policy->check_app_slug)) &&
        api_segment(policy->check_app_slug, false) &&
        parse_string_array(document, json_object_get(document, root, "designated_owners"),
                           policy->owners, &policy->owner_count, false, error, error_capacity) &&
        parse_string_array(document, json_object_get(document, root, "required_checks"),
                           policy->checks, &policy->check_count, true, error, error_capacity);
    int protection = json_object_get(document, root, "protection");
    if (ok) ok = object_has_only_keys(document, protection, protection_keys,
                                      sizeof(protection_keys) / sizeof(protection_keys[0]), error, error_capacity);
    if (ok) {
        long long approvals = 0;
        ok = json_get_boolean(document, protection, "strict", &policy->strict) &&
            json_get_boolean(document, protection, "dismiss_stale_reviews", &policy->dismiss_stale_reviews) &&
            json_get_boolean(document, protection, "require_code_owner_reviews", &policy->require_code_owner_reviews) &&
            json_get_number(document, protection, "required_approving_review_count", &approvals) &&
            approvals >= 1 && approvals <= 6 &&
            json_get_boolean(document, protection, "require_last_push_approval", &policy->require_last_push_approval) &&
            json_get_boolean(document, protection, "enforce_admins", &policy->enforce_admins) &&
            json_get_boolean(document, protection, "require_conversation_resolution", &policy->require_conversation_resolution) &&
            json_get_boolean(document, protection, "allow_force_pushes", &policy->allow_force_pushes) &&
            json_get_boolean(document, protection, "allow_deletions", &policy->allow_deletions);
        if (ok) policy->required_approving_review_count = (int)approvals;
    }
    if (!ok && !error[0]) set_error(error, error_capacity, "policy is missing a required field or has an invalid value");
    return ok;
}

static bool parse_policy_file(const char *path, review_policy *policy, char *error, size_t error_capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, error_capacity, "cannot open policy file: %s", strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END)) { fclose(file); set_error(error, error_capacity, "cannot seek policy file"); return false; }
    long length = ftell(file);
    if (length < 0 || (unsigned long)length > REVIEW_JSON_LIMIT || fseek(file, 0, SEEK_SET)) {
        fclose(file); set_error(error, error_capacity, "policy file is too large or unreadable"); return false;
    }
    char *text = malloc((size_t)length + 1);
    if (!text) { fclose(file); set_error(error, error_capacity, "out of memory reading policy file"); return false; }
    size_t read = fread(text, 1, (size_t)length, file);
    int close_status = fclose(file);
    if (read != (size_t)length || close_status) {
        free(text); set_error(error, error_capacity, "cannot read policy file"); return false;
    }
    text[length] = '\0';
    json_document document;
    if (!json_parse(&document, text, (size_t)length)) {
        set_error(error, error_capacity, document.error);
        json_free(&document); free(text); return false;
    }
    bool ok = parse_policy_document(&document, policy, error, error_capacity);
    json_free(&document);
    free(text);
    return ok;
}

static bool parse_positive_number(const char *text, long long *number)
{
    if (!text || !*text) return false;
    for (const unsigned char *ch = (const unsigned char *)text; *ch; ++ch) if (!isdigit(*ch)) return false;
    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (errno || !end || *end || parsed <= 0) return false;
    *number = parsed;
    return true;
}

static bool gh_json(char *const argv[], json_document *document, char **text_out,
                    gh_status *status, char *error, size_t error_capacity)
{
    command_result result;
    *status = call_gh(argv, &result, error, error_capacity);
    if (*status != GH_OK) {
        if (text_out) *text_out = NULL;
        command_result_free(&result);
        return false;
    }
    if (!json_parse(document, result.stdout_text, strlen(result.stdout_text))) {
        if (error_capacity) snprintf(error, error_capacity, "%s", document->error);
        command_result_free(&result);
        json_free(document);
        return false;
    }
    document->owned_text = result.stdout_text;
    result.stdout_text = NULL;
    if (text_out) *text_out = document->owned_text;
    command_result_free(&result);
    return true;
}

static bool gh_endpoint_json(const char *endpoint, bool paginate, json_document *document,
                             gh_status *status, char *error, size_t error_capacity)
{
    char *argv[6];
    size_t count = 0;
    argv[count++] = (char *)"gh";
    argv[count++] = (char *)"api";
    if (paginate) {
        argv[count++] = (char *)"--paginate";
        argv[count++] = (char *)"--slurp";
    }
    argv[count++] = (char *)endpoint;
    argv[count] = NULL;
    return gh_json(argv, document, NULL, status, error, error_capacity);
}

static bool string_lists_equal(const char left[][REVIEW_POLICY_VALUE_LIMIT], size_t left_count,
                               const char right[][REVIEW_POLICY_VALUE_LIMIT], size_t right_count,
                               bool case_insensitive)
{
    if (left_count != right_count) return false;
    for (size_t i = 0; i < left_count; ++i) {
        size_t matches = 0;
        for (size_t j = 0; j < right_count; ++j)
            if (case_insensitive ? !strcasecmp(left[i], right[j]) : !strcmp(left[i], right[j])) ++matches;
        if (matches != 1) return false;
    }
    return true;
}

static bool policies_equal(const review_policy *left, const review_policy *right)
{
    return !strcmp(left->branch, right->branch) &&
        !strcmp(left->check_app_slug, right->check_app_slug) &&
        string_lists_equal(left->owners, left->owner_count, right->owners, right->owner_count, true) &&
        string_lists_equal(left->checks, left->check_count, right->checks, right->check_count, false) &&
        left->strict == right->strict &&
        left->dismiss_stale_reviews == right->dismiss_stale_reviews &&
        left->require_code_owner_reviews == right->require_code_owner_reviews &&
        left->required_approving_review_count == right->required_approving_review_count &&
        left->require_last_push_approval == right->require_last_push_approval &&
        left->enforce_admins == right->enforce_admins &&
        left->require_conversation_resolution == right->require_conversation_resolution &&
        left->allow_force_pushes == right->allow_force_pushes &&
        left->allow_deletions == right->allow_deletions;
}

static bool verify_trusted_policy(const char *repo, const char *base_sha, const char *local_path,
                                  review_policy *policy, char *error, size_t error_capacity)
{
    review_policy local, trusted;
    if (!parse_policy_file(local_path, &local, error, error_capacity)) return false;
    char endpoint[1024];
    if (snprintf(endpoint, sizeof(endpoint),
                 "repos/%s/contents/tools/review/branch-protection.json?ref=%s", repo, base_sha) >= (int)sizeof(endpoint)) {
        set_error(error, error_capacity, "trusted review policy endpoint is too long");
        return false;
    }
    static char accept[] = "Accept: application/vnd.github.raw+json";
    char *argv[] = { (char *)"gh", (char *)"api", (char *)"-H", accept, endpoint, NULL };
    json_document document;
    gh_status status;
    if (!gh_json(argv, &document, NULL, &status, error, error_capacity)) {
        if (status == GH_NOT_FOUND) set_error(error, error_capacity, "trusted base has no review policy file");
        return false;
    }
    bool valid = parse_policy_document(&document, &trusted, error, error_capacity);
    json_free(&document);
    if (!valid) return false;
    if (!policies_equal(&local, &trusted)) {
        set_error(error, error_capacity, "local review policy differs from the trusted base policy");
        return false;
    }
    *policy = trusted;
    return true;
}

static bool gh_graphql_json(const char *owner, const char *name, const char *number,
                            json_document *document, char *error, size_t error_capacity)
{
    static const char query[] =
        "query($owner:String!,$name:String!,$number:Int!){repository(owner:$owner,name:$name){"
        "pullRequest(number:$number){reviewDecision mergeable mergeStateStatus baseRefOid headRefOid}}}";
    char owner_arg[160], name_arg[160], number_arg[48], query_arg[512];
    if (snprintf(owner_arg, sizeof(owner_arg), "owner=%s", owner) >= (int)sizeof(owner_arg) ||
        snprintf(name_arg, sizeof(name_arg), "name=%s", name) >= (int)sizeof(name_arg) ||
        snprintf(number_arg, sizeof(number_arg), "number=%s", number) >= (int)sizeof(number_arg) ||
        snprintf(query_arg, sizeof(query_arg), "query=%s", query) >= (int)sizeof(query_arg)) {
        set_error(error, error_capacity, "GitHub GraphQL argument is too long");
        return false;
    }
    char *argv[] = {
        (char *)"gh", (char *)"api", (char *)"graphql", (char *)"-f", query_arg,
        (char *)"-F", owner_arg, (char *)"-F", name_arg, (char *)"-F", number_arg, NULL
    };
    gh_status status;
    if (!gh_json(argv, document, NULL, &status, error, error_capacity)) {
        if (status == GH_NOT_FOUND) set_error(error, error_capacity, "GitHub GraphQL pull request was not found");
        return false;
    }
    return true;
}

static bool json_member_string(const json_document *document, int object, const char *key,
                               char *value, size_t capacity, char *error, size_t error_capacity)
{
    if (!json_get_string(document, object, key, value, capacity)) {
        snprintf(error, error_capacity, "GitHub response is missing string field: %s", key);
        return false;
    }
    return true;
}

static bool parse_pull_request(const json_document *document, long long expected_number,
                               const char *expected_head, const char *expected_base,
                               const review_policy *policy, char *author, size_t author_capacity,
                               char *error, size_t error_capacity)
{
    long long number;
    char state[32], mergeable_state[32], base_ref[REVIEW_POLICY_VALUE_LIMIT], head[65], base[65];
    int user = json_object_get(document, 0, "user");
    int head_object = json_object_get(document, 0, "head");
    int base_object = json_object_get(document, 0, "base");
    bool draft, mergeable;
    int number_node = json_object_get(document, 0, "number");
    if (!json_number(document, number_node, &number) || number != expected_number) {
        int first = document->nodes[0].first_child;
        char first_key[128] = "?";
        if (first >= 0) json_string(document, first, first_key, sizeof(first_key));
        snprintf(error, error_capacity, "GitHub pull request response has an invalid number (root type %d, field %d, first key %.80s)",
                 document->nodes[0].type, number_node, first_key);
        return false;
    }
    if (!json_get_boolean(document, 0, "draft", &draft)) {
        set_error(error, error_capacity, "GitHub pull request response has an invalid draft flag"); return false;
    }
    if (!json_get_boolean(document, 0, "mergeable", &mergeable)) {
        set_error(error, error_capacity, "pull request mergeability is unknown"); return false;
    }
    if (!json_member_string(document, 0, "state", state, sizeof(state), error, error_capacity) ||
        !json_member_string(document, 0, "mergeable_state", mergeable_state, sizeof(mergeable_state), error, error_capacity) ||
        !json_member_string(document, user, "login", author, author_capacity, error, error_capacity) ||
        !json_member_string(document, head_object, "sha", head, sizeof(head), error, error_capacity) ||
        !json_member_string(document, base_object, "sha", base, sizeof(base), error, error_capacity) ||
        !json_member_string(document, base_object, "ref", base_ref, sizeof(base_ref), error, error_capacity)) return false;
    if (strcmp(state, "open") || draft) {
        set_error(error, error_capacity, "pull request is not open and ready for review");
        return false;
    }
    if (!mergeable) {
        set_error(error, error_capacity, "pull request is not mergeable");
        return false;
    }
    if (strcmp(mergeable_state, "clean")) {
        set_error(error, error_capacity, "pull request merge state is not clean");
        return false;
    }
    if (strcasecmp(base_ref, policy->branch)) {
        set_error(error, error_capacity, "pull request targets a different protected branch");
        return false;
    }
    if (strcasecmp(head, expected_head) || strcasecmp(base, expected_base)) {
        set_error(error, error_capacity, "pull request head or base changed from the expected revision");
        return false;
    }
    return true;
}

static bool parse_review_decision(const json_document *document, const char *expected_head,
                                 const char *expected_base, bool *approved,
                                 char *error, size_t error_capacity)
{
    int data = json_object_get(document, 0, "data");
    int repository = json_object_get(document, data, "repository");
    int pull = json_object_get(document, repository, "pullRequest");
    char decision[32], mergeable[32], merge_state[32], head[65], base[65];
    if (!json_member_string(document, pull, "reviewDecision", decision, sizeof(decision), error, error_capacity) ||
        !json_member_string(document, pull, "mergeable", mergeable, sizeof(mergeable), error, error_capacity) ||
        !json_member_string(document, pull, "mergeStateStatus", merge_state, sizeof(merge_state), error, error_capacity) ||
        !json_member_string(document, pull, "headRefOid", head, sizeof(head), error, error_capacity) ||
        !json_member_string(document, pull, "baseRefOid", base, sizeof(base), error, error_capacity)) return false;
    if (strcasecmp(head, expected_head) || strcasecmp(base, expected_base)) {
        set_error(error, error_capacity, "GitHub review decision is for a different head or base revision");
        return false;
    }
    if (strcmp(mergeable, "MERGEABLE")) {
        set_error(error, error_capacity, "GitHub reports pull request is not mergeable");
        return false;
    }
    if (strcmp(merge_state, "CLEAN")) {
        set_error(error, error_capacity, "GitHub merge state is not CLEAN");
        return false;
    }
    *approved = !strcmp(decision, "APPROVED");
    return true;
}

static bool timestamp_valid(const char *text)
{
    if (strlen(text) != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') return false;
    for (size_t i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!isdigit((unsigned char)text[i])) return false;
    }
    return true;
}

static int find_reviewer_state(reviewer_state *reviewers, size_t count, const char *login)
{
    for (size_t i = 0; i < count; ++i) if (!strcasecmp(reviewers[i].login, login)) return (int)i;
    return -1;
}

static bool review_is_newer(const char *timestamp, long long id, const reviewer_state *previous)
{
    int order = strcmp(timestamp, previous->submitted_at);
    return order > 0 || (order == 0 && id > previous->id);
}

static bool collect_reviews(const json_document *document, reviewer_state **result,
                            size_t *result_count, char *error, size_t error_capacity)
{
    if (document->nodes[0].type != JSON_ARRAY) {
        set_error(error, error_capacity, "paginated review response must be an array");
        return false;
    }
    reviewer_state *reviewers = calloc(REVIEW_REVIEWER_LIMIT, sizeof(*reviewers));
    if (!reviewers) { set_error(error, error_capacity, "out of memory reading reviews"); return false; }
    size_t count = 0;
    for (int page = document->nodes[0].first_child; page >= 0; page = document->nodes[page].next_sibling) {
        if (document->nodes[page].type != JSON_ARRAY) {
            set_error(error, error_capacity, "review page is not an array"); free(reviewers); return false;
        }
        for (int review = document->nodes[page].first_child; review >= 0; review = document->nodes[review].next_sibling) {
            char state[32];
            if (!json_get_string(document, review, "state", state, sizeof(state))) {
                set_error(error, error_capacity, "review response is missing its state"); free(reviewers); return false;
            }
            if (!strcmp(state, "COMMENTED") || !strcmp(state, "PENDING")) continue;
            if (strcmp(state, "APPROVED") && strcmp(state, "CHANGES_REQUESTED") && strcmp(state, "DISMISSED")) {
                snprintf(error, error_capacity, "review response has unsupported state: %.100s", state);
                free(reviewers); return false;
            }
            int user = json_object_get(document, review, "user");
            reviewer_state candidate = {0};
            if (!json_get_string(document, user, "login", candidate.login, sizeof(candidate.login)) ||
                !json_get_string(document, review, "author_association", candidate.association, sizeof(candidate.association)) ||
                !json_get_string(document, review, "commit_id", candidate.commit_id, sizeof(candidate.commit_id)) ||
                !json_get_string(document, review, "submitted_at", candidate.submitted_at, sizeof(candidate.submitted_at)) ||
                !json_get_number(document, review, "id", &candidate.id) || candidate.id <= 0 ||
                !timestamp_valid(candidate.submitted_at) || !valid_sha(candidate.commit_id) ||
                !api_segment(candidate.login, true)) {
                set_error(error, error_capacity, "opinionated review has invalid identity or timestamp");
                free(reviewers); return false;
            }
            copy_if(candidate.state, sizeof(candidate.state), state);
            int existing = find_reviewer_state(reviewers, count, candidate.login);
            if (existing < 0) {
                if (count == REVIEW_REVIEWER_LIMIT) {
                    set_error(error, error_capacity, "too many distinct GitHub reviewers"); free(reviewers); return false;
                }
                reviewers[count++] = candidate;
            } else {
                if (!strcmp(candidate.submitted_at, reviewers[existing].submitted_at) &&
                    candidate.id == reviewers[existing].id && strcmp(candidate.state, reviewers[existing].state)) {
                    set_error(error, error_capacity, "GitHub returned conflicting states for one review"); free(reviewers); return false;
                }
                if (review_is_newer(candidate.submitted_at, candidate.id, &reviewers[existing])) reviewers[existing] = candidate;
            }
        }
    }
    *result = reviewers;
    *result_count = count;
    return true;
}

static bool policy_owner(const review_policy *policy, const char *login)
{
    for (size_t i = 0; i < policy->owner_count; ++i)
        if (!strcasecmp(policy->owners[i], login)) return true;
    return false;
}

static bool review_authority(const char *repo, const char *pull_number,
                             const char *author, const char *head, const review_policy *policy,
                             bool *authorized_approval, char *error, size_t error_capacity)
{
    char endpoint[1024];
    if (snprintf(endpoint, sizeof(endpoint), "repos/%s/pulls/%s/reviews?per_page=100", repo, pull_number) >= (int)sizeof(endpoint)) {
        set_error(error, error_capacity, "GitHub review endpoint is too long");
        return false;
    }
    json_document document;
    gh_status status;
    if (!gh_endpoint_json(endpoint, true, &document, &status, error, error_capacity)) {
        if (status == GH_NOT_FOUND) set_error(error, error_capacity, "GitHub review endpoint was not found");
        return false;
    }
    reviewer_state *reviewers = NULL;
    size_t reviewer_count = 0;
    bool collected = collect_reviews(&document, &reviewers, &reviewer_count, error, error_capacity);
    json_free(&document);
    if (!collected) return false;

    bool approved = false;
    bool requested_changes = false;
    for (size_t i = 0; i < reviewer_count; ++i) {
        reviewer_state *review = &reviewers[i];
        if (!strcasecmp(review->login, author) || strcasecmp(review->commit_id, head)) continue;
        if (strcmp(review->state, "APPROVED") && strcmp(review->state, "CHANGES_REQUESTED")) continue;
        if (!policy_owner(policy, review->login) || strcmp(review->association, "OWNER")) continue;
        if (!strcmp(review->state, "APPROVED")) approved = true;
        if (!strcmp(review->state, "CHANGES_REQUESTED")) requested_changes = true;
    }
    free(reviewers);
    if (requested_changes) {
        set_error(error, error_capacity, "a trusted reviewer has an active current-head changes request");
        return true;
    }
    *authorized_approval = approved;
    return true;
}

static bool associated_with_pull_request(const json_document *document, int run,
                                         long long pull_number, const char *head,
                                         const char *base)
{
    int pulls = json_object_get(document, run, "pull_requests");
    if (pulls < 0 || document->nodes[pulls].type != JSON_ARRAY) return false;
    for (int pull = document->nodes[pulls].first_child; pull >= 0; pull = document->nodes[pull].next_sibling) {
        long long number;
        char pull_head[65], pull_base[65];
        int head_object = json_object_get(document, pull, "head");
        int base_object = json_object_get(document, pull, "base");
        if (json_get_number(document, pull, "number", &number) && number == pull_number &&
            json_get_string(document, head_object, "sha", pull_head, sizeof(pull_head)) &&
            json_get_string(document, base_object, "sha", pull_base, sizeof(pull_base)) &&
            !strcasecmp(pull_head, head) && !strcasecmp(pull_base, base)) return true;
    }
    return false;
}

static bool check_run_identity(const json_document *document, int run, const review_policy *policy,
                               const char *head,
                               long long pull_number, const char *base,
                               bool require_pull, const char *pull_head, long long *app_id,
                               char *error, size_t error_capacity)
{
    char run_head[65], status[32], conclusion[32], app_slug[REVIEW_POLICY_VALUE_LIMIT];
    long long run_id, suite_id;
    int app = json_object_get(document, run, "app");
    int suite = json_object_get(document, run, "check_suite");
    if (!json_get_number(document, run, "id", &run_id) || run_id <= 0 ||
        !json_get_string(document, run, "head_sha", run_head, sizeof(run_head)) ||
        !json_get_string(document, run, "status", status, sizeof(status)) ||
        !json_get_string(document, run, "conclusion", conclusion, sizeof(conclusion)) ||
        !json_get_number(document, app, "id", app_id) || *app_id <= 0 ||
        !json_get_number(document, suite, "id", &suite_id) || suite_id <= 0) {
        set_error(error, error_capacity, "check run has incomplete identity or App data");
        return false;
    }
    if (!json_get_string(document, app, "slug", app_slug, sizeof(app_slug))) {
        set_error(error, error_capacity, "check run is missing its provider slug");
        return false;
    }
    if (strcmp(app_slug, policy->check_app_slug)) {
        set_error(error, error_capacity, "check run provider does not match trusted policy");
        return false;
    }
    if (strcasecmp(run_head, head)) {
        set_error(error, error_capacity, "check run belongs to a different head revision (stale or replayed run)");
        return false;
    }
    if (require_pull && !associated_with_pull_request(document, run, pull_number, pull_head, base)) {
        set_error(error, error_capacity, "check run is not tied to the current pull request base and head");
        return false;
    }
    if (strcmp(status, "completed") || strcmp(conclusion, "success")) {
        snprintf(error, error_capacity, "required check is not successful (status %.24s, conclusion %.24s)", status, conclusion);
        return false;
    }
    return true;
}

static bool collect_check_identities(const json_document *document, const review_policy *policy,
                                     const char *head, long long pull_number, const char *base,
                                     bool require_pull, const char *pull_head, observed_checks *observed,
                                     char *error, size_t error_capacity)
{
    if (document->nodes[0].type != JSON_ARRAY) {
        set_error(error, error_capacity, "paginated check run response must be an array");
        return false;
    }
    memset(observed, 0, sizeof(*observed));
    size_t matches[REVIEW_POLICY_LIST_LIMIT] = {0};
    for (int page = document->nodes[0].first_child; page >= 0; page = document->nodes[page].next_sibling) {
        if (document->nodes[page].type != JSON_OBJECT) {
            set_error(error, error_capacity, "check run page is not an object");
            return false;
        }
        int runs = json_object_get(document, page, "check_runs");
        if (runs < 0 || document->nodes[runs].type != JSON_ARRAY) {
            set_error(error, error_capacity, "check run page is missing check_runs");
            return false;
        }
        for (int run = document->nodes[runs].first_child; run >= 0; run = document->nodes[run].next_sibling) {
            char name[REVIEW_POLICY_VALUE_LIMIT];
            if (!json_get_string(document, run, "name", name, sizeof(name))) {
                set_error(error, error_capacity, "check run is missing its name");
                return false;
            }
            size_t check_index;
            for (check_index = 0; check_index < policy->check_count; ++check_index)
                if (!strcmp(policy->checks[check_index], name)) break;
            if (check_index == policy->check_count) continue;
            if (++matches[check_index] > 1) {
                snprintf(error, error_capacity, "required check %.120s has multiple current runs", name);
                return false;
            }
            long long app_id;
            if (!check_run_identity(document, run, policy, head, pull_number, base, require_pull, pull_head,
                                    &app_id, error, error_capacity)) return false;
            copy_if(observed->checks[check_index].context, sizeof(observed->checks[check_index].context), name);
            observed->checks[check_index].app_id = app_id;
        }
    }
    observed->count = policy->check_count;
    for (size_t i = 0; i < policy->check_count; ++i) {
        if (!matches[i]) {
            snprintf(error, error_capacity, "required check is missing for current head: %.120s", policy->checks[i]);
            return false;
        }
    }
    return true;
}

static bool fetch_observed_checks(const char *repo, const char *sha, const review_policy *policy,
                                  long long pull_number, const char *base, bool require_pull,
                                  const char *pull_head, observed_checks *observed, char *error, size_t error_capacity)
{
    char endpoint[1024];
    if (snprintf(endpoint, sizeof(endpoint), "repos/%s/commits/%s/check-runs?filter=latest&per_page=100", repo, sha) >= (int)sizeof(endpoint)) {
        set_error(error, error_capacity, "GitHub check run endpoint is too long");
        return false;
    }
    json_document document;
    gh_status status;
    if (!gh_endpoint_json(endpoint, true, &document, &status, error, error_capacity)) {
        if (status == GH_NOT_FOUND) set_error(error, error_capacity, "GitHub check run endpoint was not found");
        return false;
    }
    bool ok = collect_check_identities(&document, policy, sha, pull_number, base,
                                       require_pull, pull_head, observed, error, error_capacity);
    json_free(&document);
    return ok;
}

static bool empty_array(const json_document *document, int object, const char *key)
{
    int array = json_object_get(document, object, key);
    return array >= 0 && document->nodes[array].type == JSON_ARRAY && !document->nodes[array].child_count;
}

static bool no_pull_request_bypass(const json_document *document, int reviews)
{
    int bypass = json_object_get(document, reviews, "bypass_pull_request_allowances");
    if (bypass < 0 || document->nodes[bypass].type == JSON_NULL) return true;
    return empty_array(document, bypass, "users") &&
        empty_array(document, bypass, "teams") && empty_array(document, bypass, "apps");
}

static bool enabled_value(const json_document *document, int root, const char *key, bool expected)
{
    int object = json_object_get(document, root, key);
    bool value;
    return json_get_boolean(document, object, "enabled", &value) && value == expected;
}

static bool protection_matches(const json_document *document, const review_policy *policy,
                               const observed_checks *observed, char *error, size_t error_capacity)
{
    int status_checks = json_object_get(document, 0, "required_status_checks");
    int contexts = json_object_get(document, status_checks, "contexts");
    int checks = json_object_get(document, status_checks, "checks");
    bool strict;
    if (!json_get_boolean(document, status_checks, "strict", &strict) || strict != policy->strict ||
        contexts < 0 || document->nodes[contexts].type != JSON_ARRAY ||
        checks < 0 || document->nodes[checks].type != JSON_ARRAY ||
        document->nodes[contexts].child_count != policy->check_count ||
        document->nodes[checks].child_count != policy->check_count) {
        set_error(error, error_capacity, "branch protection required checks are missing or not strict");
        return false;
    }
    for (size_t i = 0; i < policy->check_count; ++i) {
        size_t context_matches = 0, check_matches = 0;
        for (int context = document->nodes[contexts].first_child; context >= 0; context = document->nodes[context].next_sibling) {
            char value[REVIEW_POLICY_VALUE_LIMIT];
            if (!json_string(document, context, value, sizeof(value))) {
                set_error(error, error_capacity, "branch protection context is malformed");
                return false;
            }
            if (!strcmp(value, policy->checks[i])) ++context_matches;
        }
        for (int check = document->nodes[checks].first_child; check >= 0; check = document->nodes[check].next_sibling) {
            char context[REVIEW_POLICY_VALUE_LIMIT];
            long long app_id;
            if (!json_get_string(document, check, "context", context, sizeof(context)) ||
                !json_get_number(document, check, "app_id", &app_id)) {
                set_error(error, error_capacity, "branch protection check is missing its observed App ID");
                return false;
            }
            if (strcmp(context, policy->checks[i])) continue;
            ++check_matches;
            if (app_id != observed->checks[i].app_id) {
                snprintf(error, error_capacity, "branch protection App ID for %.100s does not match the current GitHub check run", context);
                return false;
            }
        }
        if (context_matches != 1 || check_matches != 1) {
            snprintf(error, error_capacity, "branch protection does not require exactly one %.100s check", policy->checks[i]);
            return false;
        }
    }

    int reviews = json_object_get(document, 0, "required_pull_request_reviews");
    bool dismiss_stale, code_owner, last_push;
    long long approvals;
    if (!json_get_boolean(document, reviews, "dismiss_stale_reviews", &dismiss_stale) ||
        dismiss_stale != policy->dismiss_stale_reviews ||
        !json_get_boolean(document, reviews, "require_code_owner_reviews", &code_owner) ||
        code_owner != policy->require_code_owner_reviews ||
        !json_get_number(document, reviews, "required_approving_review_count", &approvals) ||
        approvals != policy->required_approving_review_count ||
        !json_get_boolean(document, reviews, "require_last_push_approval", &last_push) ||
        last_push != policy->require_last_push_approval || !no_pull_request_bypass(document, reviews)) {
        set_error(error, error_capacity, "branch protection review rules are incomplete or allow a bypass");
        return false;
    }
    if (!enabled_value(document, 0, "enforce_admins", policy->enforce_admins) ||
        !enabled_value(document, 0, "required_conversation_resolution", policy->require_conversation_resolution) ||
        !enabled_value(document, 0, "allow_force_pushes", policy->allow_force_pushes) ||
        !enabled_value(document, 0, "allow_deletions", policy->allow_deletions)) {
        set_error(error, error_capacity, "branch protection admin, conversation, or bypass settings do not match policy");
        return false;
    }
    return true;
}

static bool json_print_string(FILE *output, const char *text)
{
    if (fputc('"', output) == EOF) return false;
    for (const unsigned char *ch = (const unsigned char *)text; *ch; ++ch) {
        if (*ch == '"' || *ch == '\\') {
            if (fputc('\\', output) == EOF || fputc(*ch, output) == EOF) return false;
        } else if (*ch < 0x20) return false;
        else if (fputc(*ch, output) == EOF) return false;
    }
    return fputc('"', output) != EOF;
}

static void print_context_array(const review_policy *policy)
{
    putchar('[');
    for (size_t i = 0; i < policy->check_count; ++i) {
        if (i) putchar(',');
        json_print_string(stdout, policy->checks[i]);
    }
    putchar(']');
}

static void print_protection_payload(const review_policy *policy, const observed_checks *observed)
{
    puts("{");
    printf("  \"required_status_checks\":{\"strict\":%s,\"contexts\":",
           policy->strict ? "true" : "false");
    print_context_array(policy);
    fputs(",\"checks\":[", stdout);
    for (size_t i = 0; i < observed->count; ++i) {
        if (i) putchar(',');
        fputs("{\"context\":", stdout);
        json_print_string(stdout, observed->checks[i].context);
        printf(",\"app_id\":%lld}", observed->checks[i].app_id);
    }
    puts("]},");
    printf("  \"enforce_admins\":%s,\n", policy->enforce_admins ? "true" : "false");
    printf("  \"required_pull_request_reviews\":{\"dismiss_stale_reviews\":%s,\"require_code_owner_reviews\":%s,\"required_approving_review_count\":%d,"
           "\"require_last_push_approval\":%s,\"bypass_pull_request_allowances\":{\"users\":[],\"teams\":[],\"apps\":[]}},\n",
           policy->dismiss_stale_reviews ? "true" : "false",
           policy->require_code_owner_reviews ? "true" : "false",
           policy->required_approving_review_count,
           policy->require_last_push_approval ? "true" : "false");
    fputs("  \"restrictions\":null,\n", stdout);
    printf("  \"required_conversation_resolution\":%s,\n", policy->require_conversation_resolution ? "true" : "false");
    printf("  \"allow_force_pushes\":%s,\n", policy->allow_force_pushes ? "true" : "false");
    printf("  \"allow_deletions\":%s\n", policy->allow_deletions ? "true" : "false");
    puts("}");
}

typedef struct {
    const char *repo;
    const char *pull_number;
    const char *head;
    const char *base;
    const char *policy;
} command_options;

static bool parse_options(int argc, char **argv, command_options *options,
                          bool need_pull, char *error, size_t error_capacity)
{
    memset(options, 0, sizeof(*options));
    bool have_repo = false, have_pull = false, have_head = false, have_base = false, have_policy = false;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) { set_error(error, error_capacity, "option is missing its value"); return false; }
        const char *key = argv[i], *value = argv[i + 1];
        if (!strcmp(key, "--repo") && !have_repo) { options->repo = value; have_repo = true; }
        else if (!strcmp(key, "--pr") && need_pull && !have_pull) { options->pull_number = value; have_pull = true; }
        else if (!strcmp(key, "--head") && need_pull && !have_head) { options->head = value; have_head = true; }
        else if (!strcmp(key, "--base") && need_pull && !have_base) { options->base = value; have_base = true; }
        else if (!strcmp(key, "--policy") && !have_policy) { options->policy = value; have_policy = true; }
        else { snprintf(error, error_capacity, "unknown or duplicate option: %.120s", key); return false; }
    }
    if (!have_repo || !have_policy || (need_pull && (!have_pull || !have_head || !have_base))) {
        set_error(error, error_capacity, "required option is missing");
        return false;
    }
    return true;
}

static int not_ready(const char *reason)
{
    printf("NOT READY: %s\n", reason);
    return 1;
}

static int command_error(const char *reason)
{
    fprintf(stderr, "review-policy: %s\n", reason);
    return 2;
}

static bool get_api_document(const char *endpoint, json_document *document,
                             char *error, size_t error_capacity, bool *not_found)
{
    gh_status status = GH_FAILED;
    *not_found = false;
    if (!gh_endpoint_json(endpoint, false, document, &status, error, error_capacity)) {
        if (status == GH_NOT_FOUND) *not_found = true;
        return false;
    }
    return true;
}

static int review_policy_check(int argc, char **argv)
{
    command_options options;
    review_policy policy;
    char error[512] = {0};
    if (!parse_options(argc, argv, &options, true, error, sizeof(error))) return command_error(error);
    char owner[128], name[128];
    if (!valid_repo(options.repo, owner, sizeof(owner), name, sizeof(name)))
        return command_error("--repo must be a safe OWNER/REPOSITORY value");
    long long pull_number;
    if (!parse_positive_number(options.pull_number, &pull_number)) return command_error("--pr must be a positive integer");
    char pull_text[32];
    snprintf(pull_text, sizeof(pull_text), "%lld", pull_number);
    if (!valid_sha(options.head) || !valid_sha(options.base))
        return command_error("--head and --base must be full Git commit SHAs");
    char expected_head[65], expected_base[65];
    if (!copy_if(expected_head, sizeof(expected_head), options.head) ||
        !copy_if(expected_base, sizeof(expected_base), options.base)) return command_error("commit SHA is too long");
    for (char *p = expected_head; *p; ++p) *p = (char)tolower((unsigned char)*p);
    for (char *p = expected_base; *p; ++p) *p = (char)tolower((unsigned char)*p);
    if (!parse_policy_file(options.policy, &policy, error, sizeof(error))) return command_error(error);

    char endpoint[1024];
    if (snprintf(endpoint, sizeof(endpoint), "repos/%s/pulls/%s", options.repo, pull_text) >= (int)sizeof(endpoint))
        return command_error("GitHub pull request endpoint is too long");
    json_document pull;
    bool is_not_found;
    error[0] = '\0';
    if (!get_api_document(endpoint, &pull, error, sizeof(error), &is_not_found)) {
        if (is_not_found) return not_ready("pull request was not found");
        return command_error(error);
    }
    char author[REVIEW_POLICY_VALUE_LIMIT];
    bool valid_pull = parse_pull_request(&pull, pull_number, expected_head, expected_base,
                                        &policy, author, sizeof(author), error, sizeof(error));
    char integration_sha[65];
    bool valid_integration = json_get_string(&pull, 0, "merge_commit_sha", integration_sha, sizeof(integration_sha)) &&
        valid_sha(integration_sha);
    json_free(&pull);
    if (!valid_pull) return not_ready(error);
    if (!valid_integration) return not_ready("integration revision is unavailable");
    error[0] = '\0';
    if (!verify_trusted_policy(options.repo, expected_base, options.policy,
                               &policy, error, sizeof(error))) return command_error(error);

    json_document decision_doc;
    error[0] = '\0';
    if (!gh_graphql_json(owner, name, pull_text, &decision_doc, error, sizeof(error))) return command_error(error);
    bool server_approved = false;
    bool valid_decision = parse_review_decision(&decision_doc, expected_head, expected_base,
                                                &server_approved, error, sizeof(error));
    json_free(&decision_doc);
    if (!valid_decision) return not_ready(error);
    if (!server_approved) return not_ready("GitHub server reviewDecision is not APPROVED");

    bool approved_review = false;
    error[0] = '\0';
    if (!review_authority(options.repo, pull_text, author, expected_head, &policy,
                          &approved_review, error, sizeof(error))) return command_error(error);
    if (error[0]) return not_ready(error);
    if (!approved_review) return not_ready("no trusted, non-author approval applies to the current head");

    observed_checks observed;
    error[0] = '\0';
    if (!fetch_observed_checks(options.repo, integration_sha, &policy, pull_number,
                               expected_base, true, expected_head, &observed, error, sizeof(error))) return not_ready(error);

    char protection_endpoint[1024];
    if (snprintf(protection_endpoint, sizeof(protection_endpoint), "repos/%s/branches/%s/protection",
                 options.repo, policy.branch) >= (int)sizeof(protection_endpoint))
        return command_error("GitHub branch protection endpoint is too long");
    json_document protection;
    error[0] = '\0';
    if (!get_api_document(protection_endpoint, &protection, error, sizeof(error), &is_not_found)) {
        if (is_not_found) return not_ready("protected branch policy is not active on GitHub");
        return command_error(error);
    }
    bool protected = protection_matches(&protection, &policy, &observed, error, sizeof(error));
    json_free(&protection);
    if (!protected) return not_ready(error);

    json_document final_pull;
    error[0] = '\0';
    if (!get_api_document(endpoint, &final_pull, error, sizeof(error), &is_not_found)) {
        if (is_not_found) return not_ready("pull request disappeared during readiness check");
        return not_ready(error);
    }
    char final_author[REVIEW_POLICY_VALUE_LIMIT];
    bool final_pull_valid = parse_pull_request(&final_pull, pull_number, expected_head, expected_base,
                                               &policy, final_author, sizeof(final_author), error, sizeof(error));
    char final_integration[65];
    bool same_integration = json_get_string(&final_pull, 0, "merge_commit_sha", final_integration, sizeof(final_integration)) &&
        !strcasecmp(final_integration, integration_sha);
    json_free(&final_pull);
    if (!final_pull_valid) return not_ready(error);
    if (!same_integration) return not_ready("integration revision changed during readiness check");
    if (strcasecmp(final_author, author)) return not_ready("pull request author changed during readiness check");

    json_document final_decision_doc;
    error[0] = '\0';
    if (!gh_graphql_json(owner, name, pull_text, &final_decision_doc, error, sizeof(error))) return not_ready(error);
    bool final_server_approved = false;
    bool final_decision_valid = parse_review_decision(&final_decision_doc, expected_head, expected_base,
                                                      &final_server_approved, error, sizeof(error));
    json_free(&final_decision_doc);
    if (!final_decision_valid) return not_ready(error);
    if (!final_server_approved) return not_ready("GitHub server reviewDecision is no longer APPROVED");

    bool final_review_approved = false;
    error[0] = '\0';
    if (!review_authority(options.repo, pull_text, author, expected_head, &policy,
                          &final_review_approved, error, sizeof(error))) return not_ready(error);
    if (error[0]) return not_ready(error);
    if (!final_review_approved) return not_ready("no trusted, non-author approval applies to the current head");

    printf("READY: PR #%lld at head %s against base %s; GitHub review and branch protection agree\n",
           pull_number, expected_head, expected_base);
    return 0;
}

static int review_policy_payload(int argc, char **argv)
{
    command_options options;
    review_policy policy;
    char error[512] = {0};
    if (!parse_options(argc, argv, &options, false, error, sizeof(error))) return command_error(error);
    char owner[128], name[128];
    if (!valid_repo(options.repo, owner, sizeof(owner), name, sizeof(name)))
        return command_error("--repo must be a safe OWNER/REPOSITORY value");
    if (!parse_policy_file(options.policy, &policy, error, sizeof(error))) return command_error(error);
    char endpoint[1024];
    if (snprintf(endpoint, sizeof(endpoint), "repos/%s/branches/%s", options.repo, policy.branch) >= (int)sizeof(endpoint))
        return command_error("GitHub branch endpoint is too long");
    json_document branch;
    bool is_not_found;
    error[0] = '\0';
    if (!get_api_document(endpoint, &branch, error, sizeof(error), &is_not_found)) {
        if (is_not_found) return command_error("protected branch was not found");
        return command_error(error);
    }
    char sha[65];
    int commit = json_object_get(&branch, 0, "commit");
    bool valid = json_get_string(&branch, commit, "sha", sha, sizeof(sha)) && valid_sha(sha);
    json_free(&branch);
    if (!valid) return command_error("GitHub branch response has no full commit SHA");
    error[0] = '\0';
    if (!verify_trusted_policy(options.repo, sha, options.policy,
                               &policy, error, sizeof(error))) return command_error(error);
    observed_checks observed;
    error[0] = '\0';
    if (!fetch_observed_checks(options.repo, sha, &policy, 0, "", false,
                               "", &observed, error, sizeof(error))) return command_error(error);
    print_protection_payload(&policy, &observed);
    return ferror(stdout) ? command_error("could not write branch protection payload") : 0;
}

static void usage(FILE *output)
{
    fputs("Usage:\n"
          "  review-policy check --repo OWNER/REPO --pr NUMBER --head SHA --base SHA --policy FILE\n"
          "  review-policy protection-payload --repo OWNER/REPO --policy FILE\n", output);
}

int review_policy_command(int argc, char **argv)
{
    if (argc < 1 || !argv || !argv[0] || !strcmp(argv[0], "--help") || !strcmp(argv[0], "help")) {
        usage(stdout);
        return argc < 1 ? 2 : 0;
    }
    if (!strcmp(argv[0], "check")) return review_policy_check(argc, argv);
    if (!strcmp(argv[0], "protection-payload")) return review_policy_payload(argc, argv);
    usage(stderr);
    return command_error("unsupported policy command");
}

#ifndef REVIEW_POLICY_NO_MAIN
int main(int argc, char **argv)
{
    return review_policy_command(argc - 1, argv + 1);
}
#endif
