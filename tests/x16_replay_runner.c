#define _POSIX_C_SOURCE 200809L

#include "x16_replay_runner.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define REPLAY_PROTOCOL_EVENT_BYTES LOGIC_REPLAY_EVENT_RECORD_SIZE
#define REPLAY_PROTOCOL_OUTPUT_SIZE 19u
#define REPLAY_PROTOCOL_LOGIC_COUNT 3u
#define REPLAY_PROTOCOL_LOGIC_BYTES 256u
#define REPLAY_PROTOCOL_MAX_EVENTS 64u
#define REPLAY_PROTOCOL_OBJECT_BYTES 9u

/* The replay PRG exports these globals. The first RUN at .Start reaches the
 * testbench stop after cc65 clears BSS; the adapter fills the input globals,
 * then RUNs _replay_execute. `_replay_output` is:
 * outcome:u8, consumed:u32le, hash:u32le, failure kind:u8, logic:u8,
 * pc:u16le, opcode:u8, reason:u8, failure code:u32le. */
typedef struct ReplayTargetLabels {
    unsigned start;
    unsigned execute;
    unsigned event_count;
    unsigned events;
    unsigned rng_seed;
    unsigned tick_numerator;
    unsigned tick_denominator;
    unsigned fault_flags;
    unsigned checkpoint_length;
    unsigned checkpoint;
    unsigned resource_sizes;
    unsigned output;
    unsigned variables;
    unsigned flags;
    unsigned room;
    unsigned object_count;
    unsigned objects;
    unsigned rng_call_count;
    unsigned tick_count;
    unsigned ticks_executed;
    unsigned rng_final_state;
} ReplayTargetLabels;

typedef struct ReplayResourceCode {
    uint8_t bytes[REPLAY_PROTOCOL_LOGIC_BYTES];
    size_t size;
    unsigned present;
} ReplayResourceCode;

static int resource_path(char *path, size_t capacity, const char *directory,
                         const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length > 0 && (size_t)length < capacity ? 0 : -1;
}

static int read_file(const char *path, uint8_t **bytes, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *buffer;
    if (!file)
        return -1;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    buffer = (uint8_t *)malloc(length ? (size_t)length : 1);
    if (!buffer) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        fclose(file);
        free(buffer);
        return -1;
    }
    if (fclose(file) != 0) {
        free(buffer);
        return -1;
    }
    *bytes = buffer;
    *size = (size_t)length;
    return 0;
}

static int copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (!destination || !capacity || !source)
        return -1;
    length = strlen(source);
    if (length >= capacity)
        return -1;
    memcpy(destination, source, length + 1);
    return 0;
}

static int read_labels(const char *path, ReplayTargetLabels *labels)
{
    char line[512];
    FILE *file;
    unsigned found = 0;

    memset(labels, 0, sizeof *labels);
    file = fopen(path, "r");
    if (!file)
        return -1;
    while (fgets(line, sizeof line, file)) {
        char symbol[256];
        unsigned address;
        if (sscanf(line, "al %x %255s", &address, symbol) != 2)
            continue;
#define LABEL(name, field, bit) \
        if (!strcmp(symbol, name)) { labels->field = address; found |= 1u << bit; }
        LABEL(".Start", start, 0)
        else LABEL("._replay_execute", execute, 1)
        else LABEL("._replay_event_count", event_count, 2)
        else LABEL("._replay_events", events, 3)
        else LABEL("._replay_rng_seed", rng_seed, 4)
        else LABEL("._replay_tick_numerator", tick_numerator, 5)
        else LABEL("._replay_tick_denominator", tick_denominator, 6)
        else LABEL("._replay_fault_flags", fault_flags, 7)
        else LABEL("._replay_checkpoint_length", checkpoint_length, 8)
        else LABEL("._replay_checkpoint", checkpoint, 9)
        else LABEL("._replay_resource_sizes", resource_sizes, 10)
        else LABEL("._replay_output", output, 11)
        else LABEL("._replay_variables", variables, 12)
        else LABEL("._replay_flags", flags, 13)
        else LABEL("._replay_room", room, 14)
        else LABEL("._replay_object_count", object_count, 15)
        else LABEL("._replay_objects", objects, 16)
        else LABEL("._replay_rng_call_count", rng_call_count, 17)
        else LABEL("._replay_tick_count", tick_count, 18)
        else LABEL("._replay_ticks_executed", ticks_executed, 19)
        else LABEL("._replay_rng_final_state", rng_final_state, 20)
#undef LABEL
    }
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0)
        return -1;
    return found == 0x1FFFFFu ? 0 : -1;
}

static int64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int remaining_ms(int64_t deadline)
{
    int64_t left = deadline - monotonic_ms();
    if (left <= 0)
        return 0;
    return left > 2147483647 ? 2147483647 : (int)left;
}

static int write_all(int fd, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    while (size) {
        ssize_t written = write(fd, bytes, size);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return -1;
        bytes += (size_t)written;
        size -= (size_t)written;
    }
    return 0;
}

static int read_line(int fd, char *line, size_t size, int64_t deadline)
{
    size_t used = 0;
    if (!size)
        return -1;
    while (used + 1 < size) {
        struct pollfd pfd = {fd, POLLIN, 0};
        char ch;
        int wait = remaining_ms(deadline);
        int ready;
        ssize_t count;
        if (!wait)
            return -2;
        ready = poll(&pfd, 1, wait);
        if (ready == 0)
            return -2;
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0)
            return -1;
        count = read(fd, &ch, 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count != 1)
            return -1;
        if (ch == '\n') {
            line[used] = '\0';
            return 0;
        }
        if (ch != '\r')
            line[used++] = ch;
    }
    return -1;
}

static int wait_for_ready(int fd, char *line, size_t size, int64_t deadline)
{
    for (;;) {
        int status = read_line(fd, line, size, deadline);
        if (status != 0)
            return status;
        if (!strcmp(line, "RDY"))
            return 0;
        if (!strcmp(line, "STP") || !strncmp(line, "ERR", 3))
            return -1;
    }
}

static int send_command(int input, int output, const char *command,
                        char *line, size_t line_size, int64_t deadline)
{
    if (write_all(input, command, strlen(command)) != 0)
        return -1;
    return wait_for_ready(output, line, line_size, deadline);
}

static int write_memory_byte(int input, int output, unsigned address,
                             uint8_t value, char *line, size_t line_size,
                             int64_t deadline)
{
    char command[32];
    int length = snprintf(command, sizeof command, "STM %04X %02X\n",
                          address, value);
    if (length <= 0 || (size_t)length >= sizeof command)
        return -1;
    return send_command(input, output, command, line, line_size, deadline);
}

static int write_memory(int input, int output, unsigned address,
                        const uint8_t *bytes, size_t size, char *line,
                        size_t line_size, int64_t deadline)
{
    size_t i;
    for (i = 0; i < size; ++i)
        if (write_memory_byte(input, output, address + (unsigned)i,
                              bytes[i], line, line_size, deadline) != 0)
            return -1;
    return 0;
}

static int write_logic_code(int input, int output, unsigned bank,
                            unsigned address, const uint8_t *bytes,
                            size_t size, char *line, size_t line_size,
                            int64_t deadline)
{
    char command[32];
    size_t i;
    int length = snprintf(command, sizeof command, "RAM %02X\n", bank);
    if (length <= 0 || (size_t)length >= sizeof command ||
        send_command(input, output, command, line, line_size, deadline) != 0)
        return -1;
    for (i = 0; i < size; ++i)
        if (write_memory_byte(input, output, address + (unsigned)i,
                              bytes[i], line, line_size, deadline) != 0)
            return -1;
    return 0;
}

static int read_memory_byte(int input, int output, unsigned address,
                            uint8_t *value, char *line, size_t line_size,
                            int64_t deadline)
{
    char command[32];
    unsigned parsed;
    int length = snprintf(command, sizeof command, "RQM %04X\n", address);
    if (length <= 0 || (size_t)length >= sizeof command ||
        write_all(input, command, (size_t)length) != 0)
        return -1;
    if (read_line(output, line, line_size, deadline) != 0 ||
        sscanf(line, "%x", &parsed) != 1 || parsed > 0xFFu)
        return -1;
    *value = (uint8_t)parsed;
    return 0;
}

static int verify_uploaded_resource(int input, int output, unsigned bank,
                                    unsigned address, uint8_t expected,
                                    char *line, size_t line_size,
                                    int64_t deadline)
{
    char command[32];
    uint8_t observed;
    int length = snprintf(command, sizeof command, "RAM %02X\n", bank);
    if (length <= 0 || (size_t)length >= sizeof command ||
        send_command(input, output, command, line, line_size, deadline) != 0 ||
        read_memory_byte(input, output, address, &observed, line, line_size,
                         deadline) != 0)
        return -1;
    return observed == expected ? 0 : -1;
}

static int read_memory(int input, int output, unsigned address,
                       uint8_t *bytes, size_t size, char *line,
                       size_t line_size, int64_t deadline)
{
    size_t i;
    for (i = 0; i < size; ++i)
        if (read_memory_byte(input, output, address + (unsigned)i, &bytes[i],
                             line, line_size, deadline) != 0)
            return -1;
    return 0;
}

static int read_resource_file(const char *directory, const char *name,
                              ReplayResourceCode *resource)
{
    char path[1024];
    char *end;
    unsigned long logic;
    uint8_t *bytes;
    size_t size;

    if (strncmp(name, "LOGIC.", 6) != 0)
        return -1;
    errno = 0;
    logic = strtoul(name + 6, &end, 10);
    if (errno || *end || logic >= REPLAY_PROTOCOL_LOGIC_COUNT ||
        resource[logic].present)
        return -1;
    if (resource_path(path, sizeof path, directory, name) != 0)
        return -1;
    if (read_file(path, &bytes, &size) != 0)
        return -1;
    if (!size || size > sizeof resource[logic].bytes) {
        free(bytes);
        return -1;
    }
    memcpy(resource[logic].bytes, bytes, size);
    free(bytes);
    resource[logic].size = size;
    resource[logic].present = 1;
    return 0;
}

static int read_manifest_resource(const LogicReplayManifest *manifest,
                                  const char *directory, const char *name,
                                  uint8_t **bytes, size_t *size)
{
    char path[1024];
    unsigned i;
    for (i = 0; i < manifest->resource_count; ++i) {
        if (!strcmp(manifest->resources[i].name, name)) {
            if (resource_path(path, sizeof path, directory, name) != 0)
                return -1;
            return read_file(path, bytes, size);
        }
    }
    return -1;
}

static int read_volume_logic(const uint8_t *volume, size_t volume_size,
                             size_t offset, ReplayResourceCode *resource)
{
    uint16_t payload_size, code_size;
    if (offset > volume_size || volume_size - offset < 5 ||
        volume[offset] != 0x12 || volume[offset + 1] != 0x34 ||
        volume[offset + 2] != 0)
        return -1;
    payload_size = (uint16_t)(volume[offset + 3] |
                              ((uint16_t)volume[offset + 4] << 8));
    offset += 5;
    if (payload_size < 2 || payload_size > volume_size - offset)
        return -1;
    code_size = (uint16_t)(volume[offset] |
                           ((uint16_t)volume[offset + 1] << 8));
    offset += 2;
    if (!code_size || code_size > REPLAY_PROTOCOL_LOGIC_BYTES ||
        code_size > payload_size - 2)
        return -1;
    memcpy(resource->bytes, volume + offset, code_size);
    resource->size = code_size;
    resource->present = 1;
    return 0;
}

static int load_volume_resources(const LogicReplayManifest *manifest,
                                 const char *directory,
                                 ReplayResourceCode code[REPLAY_PROTOCOL_LOGIC_COUNT])
{
    uint8_t *logdir = NULL;
    size_t logdir_size = 0;
    unsigned logic;
    if (read_manifest_resource(manifest, directory, "LOGDIR",
                               &logdir, &logdir_size) != 0 ||
        logdir_size < 256u * 3u) {
        free(logdir);
        return -1;
    }
    memset(code, 0, sizeof(ReplayResourceCode) * REPLAY_PROTOCOL_LOGIC_COUNT);
    for (logic = 0; logic < REPLAY_PROTOCOL_LOGIC_COUNT; ++logic) {
        const uint8_t *entry = logdir + logic * 3u;
        unsigned volume_number;
        size_t offset;
        uint8_t *volume = NULL;
        size_t volume_size = 0;
        char volume_name[16];
        int found;

        if (entry[0] == 0xFF && entry[1] == 0xFF && entry[2] == 0xFF)
            continue;
        volume_number = entry[0] >> 4;
        offset = ((size_t)(entry[0] & 0x0Fu) << 16) |
                 ((size_t)entry[1] << 8) | entry[2];
        if (snprintf(volume_name, sizeof volume_name, "VOL.%u", volume_number) < 0 ||
            read_manifest_resource(manifest, directory, volume_name,
                                   &volume, &volume_size) != 0) {
            free(volume);
            free(logdir);
            return -1;
        }
        found = read_volume_logic(volume, volume_size, offset, &code[logic]);
        free(volume);
        if (found != 0) {
            free(logdir);
            return -1;
        }
    }
    free(logdir);
    return code[0].present && code[1].present ? 0 : -1;
}

static int load_resource_code(const LogicReplayManifest *manifest,
                              const char *directory,
                              ReplayResourceCode code[REPLAY_PROTOCOL_LOGIC_COUNT])
{
    unsigned i;
    memset(code, 0, sizeof(ReplayResourceCode) * REPLAY_PROTOCOL_LOGIC_COUNT);
    for (i = 0; i < manifest->resource_count; ++i)
        if (!strcmp(manifest->resources[i].name, "LOGDIR"))
            return load_volume_resources(manifest, directory, code);
    if (manifest->resource_count != REPLAY_PROTOCOL_LOGIC_COUNT)
        return -1;
    for (i = 0; i < manifest->resource_count; ++i)
        if (read_resource_file(directory, manifest->resources[i].name, code) != 0)
            return -1;
    return code[0].present && code[1].present && code[2].present ? 0 : -1;
}

static void put_u16(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t output[4], uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
        output[i] = (uint8_t)(value >> (i * 8));
}

static uint32_t get_u32(const uint8_t input[4])
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static uint16_t get_u16(const uint8_t input[2])
{
    return (uint16_t)(input[0] | ((uint16_t)input[1] << 8));
}

static int16_t get_i16(const uint8_t input[2])
{
    uint16_t value = get_u16(input);
    return value <= INT16_MAX ? (int16_t)value
                              : (int16_t)((int32_t)value - 65536);
}

static void hash_byte(uint32_t *hash, uint8_t byte)
{
    *hash ^= byte;
    *hash *= UINT32_C(16777619);
}

static uint32_t semantic_hash(const LogicReplayResult *result)
{
    uint32_t hash = UINT32_C(2166136261);
    uint8_t bytes[4];
    size_t i;
    for (i = 0; i < sizeof result->variables; ++i)
        hash_byte(&hash, result->variables[i]);
    for (i = 0; i < sizeof result->flags; ++i)
        hash_byte(&hash, result->flags[i]);
    put_u16(bytes, result->room);
    put_u16(bytes + 2, result->object_count);
    for (i = 0; i < sizeof bytes; ++i)
        hash_byte(&hash, bytes[i]);
    for (i = 0; i < result->object_count; ++i) {
        const LogicReplayObject *object = &result->objects[i];
        hash_byte(&hash, object->id);
        put_u16(bytes, (uint16_t)object->x);
        put_u16(bytes + 2, (uint16_t)object->y);
        for (size_t j = 0; j < 4; ++j)
            hash_byte(&hash, bytes[j]);
        hash_byte(&hash, object->view);
        hash_byte(&hash, object->loop);
        hash_byte(&hash, object->cel);
        hash_byte(&hash, object->motion);
    }
    put_u32(bytes, result->rng_call_count);
    for (i = 0; i < sizeof bytes; ++i)
        hash_byte(&hash, bytes[i]);
    put_u32(bytes, result->rng_final_state);
    for (i = 0; i < sizeof bytes; ++i)
        hash_byte(&hash, bytes[i]);
    return hash;
}

static int encode_events(const LogicReplayEvent *events, size_t count,
                         uint8_t **encoded)
{
    size_t i;
    size_t size;
    uint8_t *bytes;
    if (count > UINT16_MAX || count > SIZE_MAX / REPLAY_PROTOCOL_EVENT_BYTES)
        return -1;
    size = count * REPLAY_PROTOCOL_EVENT_BYTES;
    bytes = (uint8_t *)malloc(size ? size : 1);
    if (!bytes)
        return -1;
    for (i = 0; i < count; ++i) {
        if (logic_trace_event_encode(&events[i],
                                     bytes + i * REPLAY_PROTOCOL_EVENT_BYTES,
                                     REPLAY_PROTOCOL_EVENT_BYTES) != 0) {
            free(bytes);
            return -1;
        }
    }
    *encoded = bytes;
    return 0;
}

static int kill_child(pid_t child, int input, int output)
{
    int status;
    if (input >= 0)
        close(input);
    if (output >= 0)
        close(output);
    (void)kill(child, SIGTERM);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
    return 0;
}

static int launch_emulator(const char *emulator, const char *rom,
                           const char *program, pid_t *child_out,
                           int *input_out, int *output_out)
{
    int to_child[2], from_child[2];
    pid_t child;
    if (pipe(to_child) != 0 || pipe(from_child) != 0)
        return -1;
    child = fork();
    if (child < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        return -1;
    }
    if (child == 0) {
        char *args[16];
        int count = 0;
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        dup2(from_child[1], STDERR_FILENO);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        args[count++] = (char *)emulator;
        args[count++] = "-testbench";
        args[count++] = "-sound";
        args[count++] = "none";
        args[count++] = "-zeroram";
        args[count++] = "-rom";
        args[count++] = (char *)rom;
        args[count++] = "-prg";
        args[count++] = (char *)program;
        args[count] = NULL;
        if (strchr(emulator, '/'))
            execv(emulator, args);
        else
            execvp(emulator, args);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    *child_out = child;
    *input_out = to_child[1];
    *output_out = from_child[0];
    return 0;
}

static int finish_child(pid_t child, int input, int output, int64_t deadline)
{
    int status;
    struct timespec pause = {0, 10000000};
    close(input);
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0 && errno != EINTR) {
            close(output);
            return -1;
        }
        if (remaining_ms(deadline) == 0) {
            close(output);
            (void)kill(child, SIGTERM);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            return -2;
        }
        nanosleep(&pause, NULL);
    }
    close(output);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static void set_detail(LogicReplayResult *result, const char *detail)
{
    (void)copy_text(result->detail, sizeof result->detail, detail);
}

static int fail_run(pid_t child, int input, int output,
                    LogicReplayResult *result, const char *detail,
                    int timeout)
{
    if (child > 0)
        (void)kill_child(child, input, output);
    else {
        if (input >= 0) close(input);
        if (output >= 0) close(output);
    }
    result->outcome = timeout ? LOGIC_REPLAY_OUTCOME_TIMEOUT
                              : LOGIC_REPLAY_OUTCOME_RUNNER_ERROR;
    set_detail(result, detail);
    return 0;
}

