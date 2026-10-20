#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int labels(const char *path, unsigned *start, unsigned *result,
                  unsigned returns[2], unsigned metrics[7])
{
    char line[512];
    FILE *file = fopen(path, "r");
    int got_start = 0;
    int got_result = 0;
    unsigned got_returns = 0;
    unsigned got_metrics = 0;

    if (!file)
        return -1;
    while (fgets(line, sizeof line, file)) {
        unsigned address;
        char symbol[256];
        if (sscanf(line, "al %x %255s", &address, symbol) != 2)
            continue;
        if (!strcmp(symbol, ".Start")) {
            *start = address;
            got_start = 1;
        } else if (!strcmp(symbol, "._test_result")) {
            *result = address;
            got_result = 1;
        } else if (!strcmp(symbol, ".b2CallAfterLogic")) {
            returns[0] = address;
            got_returns |= 1u << 0;
        } else if (!strcmp(symbol, ".b2CallVAfterLogic")) {
            returns[1] = address;
            got_returns |= 1u << 1;
        } else if (!strcmp(symbol, "._test_context_storage_bytes")) {
            metrics[0] = address;
            got_metrics |= 1u << 0;
        } else if (!strcmp(symbol, "._test_context_hardware_stack_bytes")) {
            metrics[1] = address;
            got_metrics |= 1u << 1;
        } else if (!strcmp(symbol, "._test_context_software_stack_bytes")) {
            metrics[2] = address;
            got_metrics |= 1u << 2;
        } else if (!strcmp(symbol, "._test_logic_entry_size")) {
            metrics[3] = address;
            got_metrics |= 1u << 3;
        } else if (!strcmp(symbol, "._test_logic_file_size")) {
            metrics[4] = address;
            got_metrics |= 1u << 4;
        } else if (!strcmp(symbol, "._test_trace_event_size")) {
            metrics[5] = address;
            got_metrics |= 1u << 5;
        } else if (!strcmp(symbol, "._test_diag_a_window")) {
            metrics[6] = address;
            got_metrics |= 1u << 6;
        }
    }
    fclose(file);
    return got_start && got_result && got_returns == 0x3u &&
        got_metrics == 0x7Fu ? 0 : -1;
}

static int read_line(int fd, char *line, size_t size, int timeout_ms)
{
    size_t used = 0;
    while (used + 1 < size) {
        struct pollfd pfd = {fd, POLLIN, 0};
        char ch;
        ssize_t count;
        int ready = poll(&pfd, 1, timeout_ms);
        if (ready <= 0)
            return -1;
        count = read(fd, &ch, 1);
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

static int wait_for_ready(int fd, char *line, size_t size)
{
    while (read_line(fd, line, size, 10000) == 0) {
        if (!strcmp(line, "RDY"))
            return 0;
        if (!strcmp(line, "STP"))
            return -1;
        if (!strncmp(line, "ERR", 3))
            return -1;
    }
    return -1;
}

static int send_command(int input, int output, const char *command,
                        char *line, size_t size)
{
    size_t length = strlen(command);
    return write(input, command, length) == (ssize_t)length &&
           wait_for_ready(output, line, size) == 0 ? 0 : -1;
}

static int write_code(int input, int output, unsigned bank, unsigned address,
                      const uint8_t *code, size_t length,
                      char *line, size_t line_size)
{
    char command[32];
    size_t i;

    snprintf(command, sizeof command, "RAM %02X\n", bank);
    if (send_command(input, output, command, line, line_size) != 0)
        return -1;
    for (i = 0; i < length; ++i) {
        snprintf(command, sizeof command, "STM %04X %02X\n",
                 address + (unsigned)i, code[i]);
        if (send_command(input, output, command, line, line_size) != 0)
            return -1;
    }
    return 0;
}

static int install_bank_return_trap(int input, int output, unsigned address,
                                    unsigned result_address,
                                    char *line, size_t line_size)
{
    uint8_t code[] = {
        0xA9, 67, 0x8D, (uint8_t)result_address,
        (uint8_t)(result_address >> 8), 0x4C, 0xFD, 0xFF
    };
    if (address < 0xA000 || address >= 0xC000)
        return -1;
    return write_code(input, output, 6, address, code, sizeof code,
                      line, line_size);
}

static int load_program_bank(int input, int output, const char *program,
                             unsigned bank, char *line, size_t line_size)
{
    uint8_t code[0x2002];
    char path[4096];
    FILE *file;
    size_t length;
    int written, failed;
    written = snprintf(path, sizeof path, "%s.%02X", program, bank);
    if (written < 0 || (size_t)written >= sizeof path)
        return -1;
    file = fopen(path, "rb");
    if (!file)
        return -1;
    length = fread(code, 1, sizeof code, file);
    failed = ferror(file) || fgetc(file) != EOF;
    if (fclose(file) != 0)
        failed = 1;
    if (failed || length < 2 || code[0] != 0x00 || code[1] != 0xA0 ||
        length - 2 > 0x2000)
        return -1;
    return write_code(input, output, bank, 0xA000, code + 2, length - 2,
                      line, line_size);
}

static int check_memory(int input, int output, unsigned bank, unsigned address,
                        unsigned expected, char *line, size_t line_size)
{
    char command[32];
    unsigned actual;
    snprintf(command, sizeof command, "RAM %02X\n", bank);
    if (send_command(input, output, command, line, line_size) != 0)
        return -1;
    snprintf(command, sizeof command, "RQM %04X\n", address);
    if (write(input, command, strlen(command)) != (ssize_t)strlen(command) ||
        read_line(output, line, line_size, 10000) != 0 ||
        sscanf(line, "%x", &actual) != 1 || actual != expected)
        return -1;
    return 0;
}

static int read_word(int input, int output, unsigned address, unsigned *value,
                     char *line, size_t line_size)
{
    char command[32];
    unsigned low, high;
    snprintf(command, sizeof command, "RAM 00\n");
    if (send_command(input, output, command, line, line_size) != 0)
        return -1;
    snprintf(command, sizeof command, "RQM %04X\n", address);
    if (write(input, command, strlen(command)) != (ssize_t)strlen(command) ||
        read_line(output, line, line_size, 10000) != 0 ||
        sscanf(line, "%x", &low) != 1)
        return -1;
    snprintf(command, sizeof command, "RAM 00\n");
    if (send_command(input, output, command, line, line_size) != 0)
        return -1;
    snprintf(command, sizeof command, "RQM %04X\n", address + 1);
    if (write(input, command, strlen(command)) != (ssize_t)strlen(command) ||
        read_line(output, line, line_size, 10000) != 0 ||
        sscanf(line, "%x", &high) != 1)
        return -1;
    *value = (low & 0xFFu) | ((high & 0xFFu) << 8);
    return 0;
}

static void make_logic_code(uint8_t *code, size_t size, unsigned entry,
                            unsigned increments, unsigned variable,
                            unsigned child, unsigned child_calls, int call_v,
                            unsigned after_variable, int set_flag)
{
    unsigned i, at = entry;
    memset(code, 0xEA, size);
    for (i = 0; i < increments; ++i) {
        code[at++] = 1;
        code[at++] = (uint8_t)variable;
    }
    for (i = 0; child != 0xFF && i < child_calls; ++i) {
        code[at++] = call_v && i == 0 ? 23 : 22;
        code[at++] = call_v && i == 0 ? 100 : (uint8_t)child;
    }
    if (after_variable != 0xFF) {
        code[at++] = 1;
        code[at++] = (uint8_t)after_variable;
    }
    if (variable == 0)
        code[at++] = 145; /* set.scan.start must update the caller's entry. */
    if (set_flag) {
        code[at++] = 12;
        code[at++] = 0;
    }
    code[at] = 0;
}

static int load_logic_fixtures(int input, int output,
                               char *line, size_t line_size)
{
    uint8_t a[80], b[80], c[80];
    make_logic_code(a, sizeof a, 4, 26, 0, 1, 1, 1, 3, 0);
    make_logic_code(b, sizeof b, 7, 24, 1, 2, 2, 0, 4, 0);
    make_logic_code(c, sizeof c, 9, 1, 2, 0xFF, 0, 0, 0xFF, 1);
    if (write_code(input, output, 40, 0xA000, a, sizeof a, line, line_size) != 0 ||
        write_code(input, output, 41, 0xA100, b, sizeof b, line, line_size) != 0 ||
        write_code(input, output, 42, 0xA200, c, sizeof c, line, line_size) != 0)
        return -1;
    return check_memory(input, output, 40, 0xA004, a[4], line, line_size) == 0 &&
           check_memory(input, output, 41, 0xA107, b[7], line, line_size) == 0 &&
           check_memory(input, output, 42, 0xA209, c[9], line, line_size) == 0 ? 0 : -1;
}

