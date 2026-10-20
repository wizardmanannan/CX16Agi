#define _POSIX_C_SOURCE 200809L

#include "x16_replay_runner.h"

#include <errno.h>
#include <fcntl.h>
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

#define APP_REPLAY_MAX_EVENTS LOGIC_APP_REPLAY_INPUT_MAX

typedef struct AppReplayLabels {
    unsigned start;
    unsigned main;
    unsigned seed;
    unsigned event_count;
    unsigned events;
    unsigned fault_flags;
    unsigned run_requested;
    unsigned tick_limit;
    unsigned ready;
    unsigned output_length;
    unsigned output_data;
    unsigned step;
} AppReplayLabels;

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

static int read_labels(const char *path, AppReplayLabels *labels)
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
        else LABEL("._main", main, 1)
        else LABEL("._logicAppReplaySeed", seed, 2)
        else LABEL("._logicAppReplayEventCount", event_count, 3)
        else LABEL("._logicAppReplayEvents", events, 4)
        else LABEL("._logicAppReplayFaultFlags", fault_flags, 5)
        else LABEL("._logicAppReplayRunRequested", run_requested, 6)
        else LABEL("._logicAppReplayTickLimit", tick_limit, 7)
        else LABEL("._logicAppReplayReady", ready, 8)
        else LABEL("._logicAppReplayOutputLength", output_length, 9)
        else LABEL("._logicAppReplayOutput", output_data, 10)
        else LABEL("._logicAppReplayStep", step, 11)
#undef LABEL
    }
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0)
        return -1;
    return found == 0xFFFu ? 0 : -1;
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
        struct pollfd descriptor = {fd, POLLIN, 0};
        char ch;
        int wait = remaining_ms(deadline);
        int ready;
        ssize_t count;
        if (!wait)
            return -2;
        ready = poll(&descriptor, 1, wait);
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

static int write_memory(int input, int output, unsigned address,
                        const uint8_t *bytes, size_t size, char *line,
                        size_t line_size, int64_t deadline)
{
    size_t i;
    for (i = 0; i < size; ++i) {
        char command[32];
        int length = snprintf(command, sizeof command, "STM %04X %02X\n",
                              address + (unsigned)i, bytes[i]);
        if (length <= 0 || (size_t)length >= sizeof command ||
            send_command(input, output, command, line, line_size,
                         deadline) != 0)
            return -1;
    }
    return 0;
}

static int select_ram_bank_zero(int input, int output, char *line,
                                size_t line_size, int64_t deadline)
{
    return send_command(input, output, "RAM 00\n", line, line_size, deadline);
}

static int read_memory_byte(int input, int output, unsigned address,
                            uint8_t *value, char *line, size_t line_size,
                            int64_t deadline)
{
    char command[32];
    unsigned parsed;
    int length = snprintf(command, sizeof command, "RQM %04X\n", address);
    if (length <= 0 || (size_t)length >= sizeof command ||
        write_all(input, command, (size_t)length) != 0 ||
        read_line(output, line, line_size, deadline) != 0 ||
        sscanf(line, "%x", &parsed) != 1 || parsed > 0xFFu)
        return -1;
    *value = (uint8_t)parsed;
    return 0;
}

static int read_memory(int input, int output, unsigned address,
                       uint8_t *bytes, size_t size, char *line,
                       size_t line_size, int64_t deadline)
{
    size_t at = 0;
    if (address > 0xFFFFu || size > 0x10000u - address) return -1;
    while (at < size) {
        /* Bound both pipe directions below PIPE_BUF so neither side can
           block writing a full pipe while the other is still writing. */
        char commands[64 * 9 + 1];
        size_t count = size - at < 64 ? size - at : 64;
        for (size_t i = 0; i < count; ++i)
            if (snprintf(commands + i * 9, sizeof commands - i * 9,
                         "RQM %04X\n", address + (unsigned)(at + i)) != 9)
                return -1;
        if (write_all(input, commands, count * 9)) return -1;
        for (size_t i = 0; i < count; ++i) {
            unsigned parsed;
            if (read_line(output, line, line_size, deadline) ||
                sscanf(line, "%x", &parsed) != 1 || parsed > 255u)
                return -1;
            bytes[at + i] = (uint8_t)parsed;
        }
        at += count;
    }
    return 0;
}

