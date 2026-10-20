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

static int launch_emulator(const char *emulator, const char *rom,
                           const char *program, const char *game_directory,
                           pid_t *child_out, int *input_out, int *output_out)
{
    int to_child[2], from_child[2];
    pid_t child;
    if (pipe(to_child) != 0)
        return -1;
    if (pipe(from_child) != 0) {
        close(to_child[0]);
        close(to_child[1]);
        return -1;
    }
    child = fork();
    if (child < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        return -1;
    }
    if (child == 0) {
        char *arguments[24];
        int count = 0;
        int capture_fd;
        if (dup2(to_child[0], STDIN_FILENO) < 0 ||
            dup2(from_child[1], STDOUT_FILENO) < 0 ||
            dup2(from_child[1], STDERR_FILENO) < 0)
            _exit(126);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        capture_fd = open("/dev/null", O_WRONLY);
        if (capture_fd < 0)
            _exit(126);
        if (capture_fd != 3) {
            if (dup2(capture_fd, 3) < 0)
                _exit(126);
            close(capture_fd);
        }
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("TAS_CAPTURE_FD", "3", 1);
        arguments[count++] = (char *)emulator;
        arguments[count++] = "-testbench";
        arguments[count++] = "-warp";
        arguments[count++] = "-sound";
        arguments[count++] = "none";
        arguments[count++] = "-zeroram";
        arguments[count++] = "-rom";
        arguments[count++] = (char *)rom;
        arguments[count++] = "-fsroot";
        arguments[count++] = (char *)game_directory;
        arguments[count++] = "-startin";
        arguments[count++] = (char *)game_directory;
        arguments[count++] = "-prg";
        arguments[count++] = (char *)program;
        arguments[count] = NULL;
        if (strchr(emulator, '/'))
            execv(emulator, arguments);
        else
            execvp(emulator, arguments);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    *child_out = child;
    *input_out = to_child[1];
    *output_out = from_child[0];
    return 0;
}

static void stop_child(pid_t child, int input, int output)
{
    int status;
    int64_t deadline;
    struct timespec delay = {0, 10000000};
    if (input >= 0)
        close(input);
    if (output >= 0)
        close(output);
    if (child <= 0)
        return;
    (void)kill(child, SIGTERM);
    deadline = monotonic_ms() + 500;
    while (waitpid(child, &status, WNOHANG) == 0 && remaining_ms(deadline))
        nanosleep(&delay, NULL);
    if (waitpid(child, &status, WNOHANG) == 0) {
        (void)kill(child, SIGKILL);
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
    }
}

static int finish_child(pid_t child, int input, int output,
                        int64_t deadline)
{
    int status;
    struct timespec delay = {0, 10000000};
    if (close(input) != 0) {
        close(output);
        stop_child(child, -1, -1);
        return -1;
    }
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0 && errno != EINTR) {
            close(output);
            stop_child(child, -1, -1);
            return -1;
        }
        if (!remaining_ms(deadline)) {
            close(output);
            stop_child(child, -1, -1);
            return -2;
        }
        nanosleep(&delay, NULL);
    }
    close(output);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static void put_u32(uint8_t bytes[4], uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
}

/* Corpus/input-trace bytes are semantic ASCII/control keys. Translate only
   at the X16 boundary; the shared LTRC trace remains engine-independent. */
static int semantic_key_to_petscii(uint8_t semantic, uint8_t *petscii)
{
    if (!petscii)
        return -1;
    switch (semantic) {
    case 0x80: *petscii = 0x91; return 0; /* up */
    case 0x81: *petscii = 0x11; return 0; /* down */
    case 0x82: *petscii = 0x9D; return 0; /* left */
    case 0x83: *petscii = 0x1D; return 0; /* right */
    default: break;
    }
    if (semantic >= 'A' && semantic <= 'Z') {
        *petscii = (uint8_t)(semantic + 0x80u);
    } else if (semantic >= 'a' && semantic <= 'z') {
        *petscii = (uint8_t)(semantic - 0x20u);
    } else if ((semantic >= 0x20u && semantic <= 0x7Eu) ||
               semantic == 0x08u || semantic == 0x09u ||
               semantic == 0x0Au || semantic == 0x0Du ||
               semantic == 0x1Bu) {
        *petscii = semantic;
    } else {
        return -1;
    }
    return 0;
}

static uint32_t get_u32(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t get_u16(const uint8_t bytes[2])
{
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static int set_detail(char *detail, size_t capacity, const char *text)
{
    return copy_text(detail, capacity, text);
}

