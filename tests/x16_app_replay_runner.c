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

static int run_application(const char *game_directory, uint16_t seed,
                             const LogicAppReplayInput *events,
                             size_t event_count, uint32_t tick_count,
                             uint32_t timeout_ms, uint8_t fault_flags,
                             uint8_t output_bytes[LOGIC_APP_REPLAY_OUTPUT_SIZE],
                             char *detail, size_t detail_capacity,
                             uint8_t snapshots[][LOGIC_APP_REPLAY_OUTPUT_SIZE])
{
    static const char *const default_emulator = "x16emu";
    static const char *const default_rom = "rom.bin";
    static const char *const default_program = LOGIC_APP_REPLAY_DEFAULT_PROGRAM;
    static const char *const default_labels = "obj/cx16/logic-app-replay/agi.cx16.lbl";
    const char *emulator = getenv("X16EMU");
    const char *rom = getenv("X16ROM");
    const char *program = getenv("CX16_LOGIC_APP_PRG");
    const char *label_path = getenv("CX16_LOGIC_APP_LABELS");
    AppReplayLabels labels;
    uint8_t seed_bytes[2];
    uint8_t tick_bytes[4];
    uint8_t translated_events[APP_REPLAY_MAX_EVENTS];
    uint8_t event_count_byte;
    uint8_t fault_byte = fault_flags;
    uint8_t run_byte = 1;
    pid_t child = -1;
    int input = -1, output = -1;
    int64_t deadline;
    char line[256], command[64];
    unsigned i;
    uint32_t latest_event_frame = 0;
    int status;
    uint8_t requested, ready;
    uint8_t empty_header[4] = {0};
    uint8_t output_length_bytes[2];

    if (detail && detail_capacity)
        detail[0] = '\0';
    if (output_bytes)
        memset(output_bytes, 0, LOGIC_APP_REPLAY_OUTPUT_SIZE);
    if (!game_directory || !*game_directory || !output_bytes || !detail ||
        !detail_capacity || !timeout_ms || (!events && event_count) ||
        event_count > APP_REPLAY_MAX_EVENTS || tick_count > 255u ||
        (fault_flags & (uint8_t)~(LOGIC_APP_REPLAY_FAULT_OMIT_METADATA_SAVE |
                                LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE)))
    {
        if (detail && detail_capacity)
            set_detail(detail, detail_capacity,
                       event_count > APP_REPLAY_MAX_EVENTS
                           ? "full-app input event capacity exceeded (max 64)"
                           : tick_count > 255u
                                 ? "full-app tick capacity exceeded (max 255)"
                                 : "invalid full-app replay arguments");
        return -1;
    }
    (void)signal(SIGPIPE, SIG_IGN);
    for (i = 0; i < event_count; ++i) {
        if (!events[i].frame || events[i].frame > UINT16_MAX ||
            events[i].petscii == 0 ||
            (i && events[i - 1].frame > events[i].frame) ||
            semantic_key_to_petscii(events[i].petscii,
                                    &translated_events[i]) != 0) {
            (void)snprintf(detail, detail_capacity,
                           "invalid semantic input event %u (frame=%u key=%02x)",
                           i, (unsigned)events[i].frame,
                           (unsigned)events[i].petscii);
            return -1;
        }
        latest_event_frame = events[i].frame;
    }
    if (latest_event_frame + tick_count * 6u > UINT16_MAX) {
        set_detail(detail, detail_capacity,
                   "full-app virtual frame range exceeds 16-bit VERA clock");
        return -1;
    }
    if (!emulator || !*emulator) emulator = default_emulator;
    if (!rom || !*rom) rom = default_rom;
    if (!program || !*program) program = default_program;
    if (!label_path || !*label_path) label_path = default_labels;
    if (read_labels(label_path, &labels) != 0) {
        set_detail(detail, detail_capacity, "full-app replay labels are missing or invalid");
        return -1;
    }
    if (launch_emulator(emulator, rom, program, game_directory,
                        &child, &input, &output) != 0) {
        set_detail(detail, detail_capacity, "cannot launch full-app emulator");
        return -1;
    }
    deadline = monotonic_ms();
    if (deadline < 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity, "cannot read monotonic clock");
        return -1;
    }
    deadline += timeout_ms;
    status = wait_for_ready(output, line, sizeof line, deadline);
    if (status != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   status == -2 ? "full-app emulator startup timed out"
                                : "full-app emulator did not reach the testbench");
        return -1;
    }
    status = snprintf(command, sizeof command, "RUN %04X\n", labels.start);
    if (status <= 0 || (size_t)status >= sizeof command ||
        send_command(input, output, command, line, sizeof line, deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   "full application did not complete its normal CRT entry");
        return -1;
    }

    seed_bytes[0] = (uint8_t)seed;
    seed_bytes[1] = (uint8_t)(seed >> 8);
    event_count_byte = (uint8_t)event_count;
    put_u32(tick_bytes, snapshots ? 0 : tick_count);
    if (write_memory(input, output, labels.seed, seed_bytes,
                     sizeof seed_bytes, line, sizeof line, deadline) != 0 ||
        write_memory(input, output, labels.event_count, &event_count_byte,
                     sizeof event_count_byte, line, sizeof line, deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity, "cannot set full-app replay seed or event count");
        return -1;
    }
    for (i = 0; i < event_count; ++i) {
        uint8_t record[LOGIC_APP_REPLAY_INPUT_RECORD_SIZE];
        put_u32(record, events[i].frame);
        record[4] = translated_events[i];
        if (write_memory(input, output,
                         labels.events + i * LOGIC_APP_REPLAY_INPUT_RECORD_SIZE,
                         record, sizeof record, line, sizeof line,
                         deadline) != 0) {
            stop_child(child, input, output);
            set_detail(detail, detail_capacity, "cannot deliver full-app input events");
            return -1;
        }
    }
    if (write_memory(input, output, labels.fault_flags, &fault_byte, 1,
                     line, sizeof line, deadline) != 0 ||
        write_memory(input, output, labels.tick_limit, tick_bytes,
                     sizeof tick_bytes, line, sizeof line, deadline) != 0 ||
        write_memory(input, output, labels.run_requested, &run_byte, 1,
                     line, sizeof line, deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity, "cannot deliver full-app replay request");
        return -1;
    }
    if (select_ram_bank_zero(input, output, line, sizeof line, deadline) != 0 ||
        read_memory_byte(input, output, labels.run_requested, &requested,
                         line, sizeof line, deadline) != 0 || requested != 1) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   "testbench did not deliver the full-app run request to BSS");
        return -1;
    }
    if (read_memory(input, output, labels.output_data, empty_header,
                    sizeof empty_header, line, sizeof line, deadline) != 0 ||
        empty_header[0] != 0 || empty_header[1] != 0 ||
        empty_header[2] != 0 || empty_header[3] != 0) {
        stop_child(child, input, output);
        (void)snprintf(detail, detail_capacity,
                       "testbench BSS preflight read unexpected output bytes %02x%02x%02x%02x",
                       empty_header[0], empty_header[1],
                       empty_header[2], empty_header[3]);
        return -1;
    }
    status = snprintf(command, sizeof command, "RUN %04X\n", labels.main);
    if (status <= 0 || (size_t)status >= sizeof command ||
        send_command(input, output, command, line, sizeof line, deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   remaining_ms(deadline) == 0
                       ? "full production application boot or replay timed out"
                       : "full production application boot or replay did not return to testbench");
        return -1;
    }
    if (select_ram_bank_zero(input, output, line, sizeof line, deadline) != 0 ||
        read_memory_byte(input, output, labels.ready, &ready,
                         line, sizeof line, deadline) != 0 ||
        read_memory(input, output, labels.output_length, output_length_bytes,
                    sizeof output_length_bytes, line, sizeof line,
                    deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   "testbench could not read full-app completion state");
        return -1;
    }
    if (ready != 1 || (output_length_bytes[0] |
                       ((uint16_t)output_length_bytes[1] << 8)) !=
                      LOGIC_APP_REPLAY_OUTPUT_SIZE ||
        read_memory(input, output, labels.output_data, output_bytes,
                    LOGIC_APP_REPLAY_OUTPUT_SIZE, line, sizeof line,
                    deadline) != 0) {
        stop_child(child, input, output);
        set_detail(detail, detail_capacity,
                   "full production application returned without a complete semantic checkpoint");
        return -1;
    }
    if (snapshots) {
        for (uint32_t tick = 1; tick <= tick_count; ++tick) {
            uint8_t *checkpoint = snapshots[tick - 1];
            snprintf(command, sizeof command, "RUN %04X\n", labels.step);
            if (send_command(input, output, command, line, sizeof line, deadline) ||
                select_ram_bank_zero(input, output, line, sizeof line, deadline) ||
                read_memory(input, output, labels.output_data, checkpoint,
                    LOGIC_APP_REPLAY_OUTPUT_SIZE, line, sizeof line, deadline)) {
                stop_child(child, input, output);
                set_detail(detail, detail_capacity, "application checkpoint series timed out or stopped");
                return -1;
            }
            if (memcmp(checkpoint, "X16R", 4) ||
                checkpoint[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION ||
                get_u32(checkpoint + 6) != tick) {
                stop_child(child, input, output);
                set_detail(detail, detail_capacity, "application checkpoint series skipped a cycle");
                return -1;
            }
            if (checkpoint[58] & (LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED |
                                  LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED)) {
                memcpy(output_bytes, checkpoint, LOGIC_APP_REPLAY_OUTPUT_SIZE);
                break;
            }
        }
    }
    status = finish_child(child, input, output, deadline);
    if (status != 0) {
        set_detail(detail, detail_capacity,
                   status == -2 ? "full-app emulator shutdown timed out"
                                : "full-app emulator did not exit cleanly");
        return -1;
    }
    if (output_bytes[0] != 'X' || output_bytes[1] != '1' ||
        output_bytes[2] != '6' || output_bytes[3] != 'R' ||
        output_bytes[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION ||
        (!(output_bytes[58] &
           (LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED |
            LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED)) &&
         get_u32(output_bytes + 6) != tick_count)) {
        (void)snprintf(detail, detail_capacity,
                       "full-app checkpoint invalid: ready=%u length=%u header=%02x%02x%02x%02x version=%u tick=%u expected=%u",
                       (unsigned)ready,
                       (unsigned)(output_length_bytes[0] |
                           ((uint16_t)output_length_bytes[1] << 8)),
                       output_bytes[0], output_bytes[1], output_bytes[2],
                       output_bytes[3], output_bytes[4],
                       (unsigned)get_u32(output_bytes + 6), tick_count);
        return -1;
    }
    if (output_bytes[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED) {
        (void)snprintf(detail, detail_capacity,
                       "input_wait_exhausted: tick=%u events=%u/%u frame=%u key=%02x polls=%u",
                       (unsigned)get_u32(output_bytes + 6),
                       (unsigned)get_u16(output_bytes + 12),
                       (unsigned)get_u16(output_bytes + 10),
                       (unsigned)get_u32(output_bytes +
                           LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET),
                       (unsigned)output_bytes[
                           LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET],
                       (unsigned)get_u16(output_bytes +
                           LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET));
        return -1;
    }
    if (fault_flags &&
        !(output_bytes[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED)) {
        (void)snprintf(detail, detail_capacity,
                       "input-triggered fault event was not delivered (target frame=%u PETSCII=%02x)",
                       (unsigned)LOGIC_APP_REPLAY_FAULT_TRIGGER_FRAME,
                       (unsigned)LOGIC_APP_REPLAY_FAULT_TRIGGER_PETSCII);
        return -1;
    }
    if (output_bytes[58] & LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED) {
        (void)snprintf(detail, detail_capacity,
                       "context verifier mismatch: field=%u offset=%u logic=%u opcode=%u pc=%04x expected=%08x actual=%08x fault_triggered=%u",
                       (unsigned)output_bytes[23],
                       (unsigned)output_bytes[24],
                       (unsigned)output_bytes[25],
                       (unsigned)output_bytes[26],
                       (unsigned)get_u16(output_bytes + 27),
                       (unsigned)get_u32(output_bytes + 29),
                       (unsigned)get_u32(output_bytes + 33),
                       (unsigned)((output_bytes[58] &
                           LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED) != 0));
        return -1;
    }
    if ((output_bytes[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED) &&
        !(fault_flags & LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE)) {
        set_detail(detail, detail_capacity,
                   "input-triggered fault fired but produced no verifier mismatch");
        return -1;
    }
    {
        uint8_t expected_status =
            LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
        uint16_t capacity_flags = get_u16(output_bytes +
            LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET);
        uint32_t trace_dropped = get_u32(output_bytes + 42);
        uint8_t actual_status = output_bytes[58];
        if (fault_flags & LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE)
            actual_status &= (uint8_t)~LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED;
        int bounded_trace = output_bytes[41] == LOGIC_APP_REPLAY_TRACE_MAX &&
            trace_dropped != 0 && actual_status == expected_status &&
            capacity_flags == LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
        if ((actual_status || capacity_flags || trace_dropped) &&
            !bounded_trace) {
            (void)snprintf(detail, detail_capacity,
                           "full-app checkpoint has status=%02x capacity=%04x",
                           output_bytes[58], (unsigned)capacity_flags);
            return -1;
        }
    }
    detail[0] = '\0';
    return 0;
}

int logic_app_replay_x16_run(const char *game_directory, uint16_t seed,
    const LogicAppReplayInput *events, size_t event_count, uint32_t tick_count,
    uint32_t timeout_ms, uint8_t fault_flags,
    uint8_t output[LOGIC_APP_REPLAY_OUTPUT_SIZE], char *detail, size_t detail_capacity)
{
    return run_application(game_directory, seed, events, event_count, tick_count,
        timeout_ms, fault_flags, output, detail, detail_capacity, NULL);
}

int logic_app_replay_x16_series(const char *game_directory, uint16_t seed,
    const LogicAppReplayInput *events, size_t event_count, uint32_t ticks,
    uint32_t timeout_ms, uint8_t fault_flags,
    uint8_t snapshots[][LOGIC_APP_REPLAY_OUTPUT_SIZE],
    size_t capacity, char *detail, size_t detail_capacity)
{
    if (!snapshots || !ticks || ticks > 255u || capacity < ticks) {
        if (snapshots && capacity)
            memset(snapshots, 0, (capacity < 255u ? capacity : 255u) * sizeof snapshots[0]);
        if (detail && detail_capacity)
            set_detail(detail, detail_capacity, "invalid application checkpoint series capacity");
        return -1;
    }
    memset(snapshots, 0, ticks * sizeof snapshots[0]);
    return run_application(game_directory, seed, events, event_count, ticks,
        timeout_ms, fault_flags, snapshots[ticks - 1], detail, detail_capacity, snapshots);
}
