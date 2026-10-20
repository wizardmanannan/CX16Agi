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

static int fail_child(pid_t child, int input, int output)
{
    int status;
    if (input >= 0)
        close(input);
    if (output >= 0)
        close(output);
    kill(child, SIGTERM);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
    return 1;
}

static const char *failure_message(unsigned result)
{
    static const char *const messages[] = {
        "nested dispatch regression: caller B context was not captured twice",
        "nested dispatch regression: caller B LOGICEntry was overwritten when C returned",
        "nested dispatch regression: caller B LOGICFile was overwritten when C returned",
        "nested dispatch regression: caller A post-call context was not observed",
        "nested dispatch regression: caller A LOGICEntry was overwritten after B returned",
        "nested dispatch regression: caller A LOGICFile was overwritten after B returned",
        "nested dispatch regression: caller B code bounds, bank, or logical PC was not restored",
        "nested dispatch regression: caller A code bounds, bank, or logical PC was not restored",
        "nested dispatch regression: top-level LOGICEntry or LOGICFile was not restored",
        "nested dispatch regression: caller start address was not restored",
        "nested dispatch regression: game variable/flag side effects were lost on return",
        "nested dispatch regression: caller B state was not captured after return",
        "nested dispatch regression: context frames remained active after return",
        "nested dispatch regression: context verifier reported an error",
        "nested dispatch regression: independent caller-state verifier found a mismatch",
        "nested dispatch regression: caller end address was not restored",
        "nested dispatch regression: caller logical PC was not restored",
        "nested dispatch regression: caller entry/file pointers were not restored",
        "nested dispatch regression: caller code bank was not restored",
        "nested dispatch regression: caller RAM bank was not restored",
        "nested dispatch regression: code window invalidation state was not restored",
        "nested dispatch regression: production trace event count differs",
        "nested dispatch regression: production trace ring overflowed unexpectedly",
        "nested dispatch regression: root call event differs",
        "nested dispatch regression: call.v event differs",
        "nested dispatch regression: immediate call event differs",
        "nested dispatch regression: B return event differs",
        "nested dispatch regression: root return event differs",
        "nested dispatch regression: production trace ring dropped events",
        "nested dispatch regression: hardware stack usage was not measured",
        "nested dispatch regression: cc65 stack grew across a context frame",
        "nested dispatch regression: context underflow was accepted",
        "nested dispatch regression: underflow diagnostic was lost",
        "nested dispatch regression: context frame could not be filled",
        "nested dispatch regression: context overflow was accepted",
        "nested dispatch regression: overflow depth or diagnostic differs",
        "nested dispatch regression: overflow frame could not be unwound",
        "nested dispatch regression: overflow diagnostic was lost after unwind",
        "room transition regression: interpreter side effects were lost",
        "room transition regression: new-room request was not retained",
        "room transition regression: child logic context was restored",
        "room transition regression: abandoned contexts or verifier state differ",
        "context verifier regression: depth corruption was not detected",
        "context verifier regression: depth corruption did not unwind",
        "context verifier regression: depth mismatch field differs",
        "context verifier regression: generation corruption did not enter",
        "context verifier regression: generation corruption did not unwind",
        "context verifier regression: generation mismatch field differs",
        "nested dispatch regression: 1024 opcode step limit reached",
        "nested dispatch regression: caller A logical PC differs",
        "nested dispatch regression: caller A code bank differs",
        "nested dispatch regression: caller A entry pointer differs",
        "nested dispatch regression: caller A file pointer differs",
        "nested dispatch regression: caller A code-window first opcode differs",
        "nested dispatch regression: caller A code-window invalid flag differs",
        "room transition regression: context depth remained active",
        "room transition regression: context error was not clear",
        "room transition regression: independent verifier found a mismatch",
        "room transition regression: caller RAM bank was not restored",
        "nested dispatch regression: bank-2 return PC resumed in bank 6"
    };
    if (result == 67)
        return "nested dispatch regression: bank-2 return PC resumed in bank 6";
    if (result == 74)
        return "release context size regression: diagnostic bookkeeping remains in caller frames";
    if (result == 77)
        return "context verifier regression: corrupted abandonment was not detected";
    if (result == 78 || result == 79)
        return "toggle.v regression: indexed flag or index variable differs";
    if (result == 80 || result == 81)
        return "random regression: inclusive, equal, or full-byte range differs";
    if (result == 82)
        return "release context code exceeds the reviewed 260-byte budget";
    if (result == 68)
        return "room transition regression: new-room request was not retained";
    if (result == 69)
        return "room transition regression: bank-2 call return bank differs";
    if (result == 70)
        return "room transition regression: top-level interpreter bank was not retained";
    if (result == 71)
        return "room transition regression: interpreter contexts did not unwind cleanly";
    return result >= 1 && result <= sizeof messages / sizeof messages[0]
        ? messages[result - 1] : "nested dispatch regression: target test did not report success";
}

int main(int argc, char **argv)
{
    unsigned start = 0, result_address = 0;
    unsigned bank2_returns[2] = {0};
    unsigned metric_addresses[7] = {0};
    unsigned metric_values[7];
    int to_child[2], from_child[2], status;
    pid_t child;
    char line[256], command[64];
    unsigned result;
    int banked_test = argc == 6 && !strcmp(argv[5], "--banked");
    int allocator_test = argc == 6 && !strcmp(argv[5], "--trace-allocator");

    if (argc != 5 && !banked_test && !allocator_test) {
        fprintf(stderr, "usage: %s X16EMU X16ROM TEST.PRG TEST.LBL [--banked|--trace-allocator]\n",
                argv[0]);
        return 2;
    }
    if (labels(argv[4], &start, &result_address, bank2_returns,
               metric_addresses) != 0) {
        fprintf(stderr, "x16 test runner: required test labels missing in %s\n", argv[4]);
        return 2;
    }
    if (pipe(to_child) != 0 || pipe(from_child) != 0) {
        perror("x16 test runner: pipe");
        return 2;
    }
    child = fork();
    if (child < 0) {
        perror("x16 test runner: fork");
        return 2;
    }
    if (child == 0) {
        char *emu_args[16];
        int arg_count = 0;
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        dup2(from_child[1], STDERR_FILENO);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        emu_args[arg_count++] = argv[1];
        emu_args[arg_count++] = "-testbench";
        /* The assembly target is clock-independent; avoid wall-clock throttling. */
        emu_args[arg_count++] = "-warp";
        emu_args[arg_count++] = "-sound";
        emu_args[arg_count++] = "none";
        emu_args[arg_count++] = "-zeroram";
        emu_args[arg_count++] = "-rom";
        emu_args[arg_count++] = argv[2];
        emu_args[arg_count++] = "-prg";
        emu_args[arg_count++] = argv[3];
        emu_args[arg_count] = NULL;
        if (strchr(argv[1], '/'))
            execv(argv[1], emu_args);
        else
            execvp(argv[1], emu_args);
        perror("x16 test runner: exec emulator");
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    if (wait_for_ready(from_child[0], line, sizeof line) != 0) {
        fprintf(stderr, "x16 test runner: emulator did not become ready (%s)\n", line);
        return fail_child(child, to_child[1], from_child[0]);
    }
    if (banked_test &&
        (load_program_bank(to_child[1], from_child[0], argv[3], 2,
                          line, sizeof line) != 0 ||
        install_bank_return_trap(to_child[1], from_child[0], bank2_returns[0],
                                 result_address, line, sizeof line) != 0 ||
        install_bank_return_trap(to_child[1], from_child[0], bank2_returns[1],
                                 result_address, line, sizeof line) != 0)) {
        fprintf(stderr, "x16 test runner: could not load BANKRAM02 code (%s)\n", line);
        return fail_child(child, to_child[1], from_child[0]);
    }
    if (!allocator_test && load_logic_fixtures(to_child[1], from_child[0], line, sizeof line) != 0) {
        fprintf(stderr, "x16 test runner: could not seed test logic resources (%s)\n", line);
        return fail_child(child, to_child[1], from_child[0]);
    }
    snprintf(command, sizeof command, "RUN %04X\n", start);
    if (write(to_child[1], command, strlen(command)) != (ssize_t)strlen(command))
        return fail_child(child, to_child[1], from_child[0]);
    if (wait_for_ready(from_child[0], line, sizeof line) != 0) {
        fprintf(stderr, "x16 test runner: target program did not return to testbench (%s)\n", line);
        return fail_child(child, to_child[1], from_child[0]);
    }
    snprintf(command, sizeof command, "RQM %04X\n", result_address);
    if (write(to_child[1], command, strlen(command)) != (ssize_t)strlen(command) ||
        read_line(from_child[0], line, sizeof line, 10000) != 0 ||
        sscanf(line, "%x", &result) != 1) {
        fprintf(stderr, "x16 test runner: could not read target test result\n");
        return fail_child(child, to_child[1], from_child[0]);
    }
    {
        unsigned i;
        for (i = 0; i < 7; ++i) {
            if (read_word(to_child[1], from_child[0], metric_addresses[i],
                          &metric_values[i], line, sizeof line) != 0) {
                fprintf(stderr, "x16 test runner: could not read target measurements\n");
                return fail_child(child, to_child[1], from_child[0]);
            }
        }
    }
    close(to_child[1]);
    while (read_line(from_child[0], line, sizeof line, 10000) == 0) { }
    close(from_child[0]);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "x16 test runner: emulator did not exit cleanly (wait status %#x)\n", status);
        return 1;
    }
    if (result != 0) {
        if (!allocator_test && (result == 83 || result == 84))
            fprintf(stderr, "%s\n", result == 83 ?
                    "add.v skipped its following opcode or produced the wrong sum" :
                    "new.room.v failed to store its variable destination");
        if (allocator_test) {
            static const char *const errors[] = {
                "tiny allocation layout", "live tiny allocation corrupted by tracing",
                "ring capacity/drop count", "trace event payload", "trace storage accounting",
                "trace must own exactly one SMALL allocation", "runtime RAM bank restoration"
            };
            fprintf(stderr, "trace allocator regression: %s (code %u, first tiny slot=%u)\n",
                    result <= sizeof errors / sizeof errors[0] ? errors[result - 1] :
                    "unknown target failure", result, metric_values[6]);
            return 1;
        }
        if (result == 80 || result == 81)
            fprintf(stderr, "random endpoints: random(3,4)=%u random(10,10)=%u\n",
                    metric_values[6] & 255u, metric_values[6] >> 8);
        if (result == 78 || result == 79)
            fprintf(stderr, "toggle.v state: f255=%u index v240=%u\n",
                    metric_values[6] & 255u, metric_values[6] >> 8);
        if (result > 99 && result <= 99 + 16)
            fprintf(stderr, "room-return verifier first mismatch field=%u\n",
                    result - 99);
        if (result == 54 || result == 55)
            fprintf(stderr, "caller A window diagnostic: first opcode=$%02X invalid=%u\n",
                    metric_values[6] & 0xFFu, (metric_values[6] >> 8) & 0xFFu);
        fprintf(stderr, "%s (code %u)\n", failure_message(result), result);
        return 1;
    }
    puts(allocator_test ? "production trace preserves live allocator blocks on CX16" :
                         "nested production dispatch context passed on CX16");
    printf("target sizes: context=%u bytes, sampled hardware call-stack depth=%u bytes, "
           "sampled software call-stack growth=%u bytes; LOGICEntry=%u bytes, LOGICFile=%u bytes, "
           "trace event=%u bytes\n",
           metric_values[0], metric_values[1], metric_values[2],
           metric_values[3], metric_values[4], metric_values[5]);
    return 0;
}
