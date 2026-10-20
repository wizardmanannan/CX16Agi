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

