/* Dictionary comparison through the full product image, not a host substitute. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target-fixture.h"

static void prepare(FILE* input, const char* left, const char* right)
{
    unsigned int i;
    fprintf(input, "RAM 15\n");
    for (i = 0; i <= strlen(left); i++) fprintf(input, "STM %04X %02X\n", 0xa0fd + i, (unsigned char)left[i]);
    for (i = 0; i <= strlen(right); i++) fprintf(input, "STM %04X %02X\n", 0x8000 + i, (unsigned char)right[i]);
}

static void compare(FILE* input, unsigned int routine, unsigned int stack)
{
    fprintf(input, "STM 0000 15\nSTM %04X FE\nSTM %04X 8E\nSTM 8EFE FD\nSTM 8EFF A0\n",
            stack, stack + 1);
    fprintf(input, "STA 00\nSTX 80\nRUN %04X\n", routine);
}

int main(int argc, char** argv)
{
    static const struct { const char* left; const char* right; int result; } cases[] = {
        { "", "", 0 }, { "   ", "", 0 }, { "", "  z", -1 }, { "x ", "", 1 },
        { " g o  n o r t h ", "gonorth", 0 }, { "aa", "az", -25 },
        { "\xc1", "A", 128 }, { "A", "\xc1", -128 },
        { "\xff", "\x01", 254 }, { "\x01", "\xff", -254 }, { "\ta", "a", -88 }
    };
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    unsigned int expected[2048], count = 0, i, j, routine, stack;
    unsigned int value, phase = 0;
    unsigned long before = 0, cycles = 0;
    char line[256], longLeft[306], longRight[301];
    int status;
    pid_t child;
    assert(argc == 3);
    routine = targetFixtureSymbol(argv[2], "_strcmpIgnoreSpace");
    stack = targetFixtureSymbol(argv[2], "c_sp");
    prepare(input, " g o  n o r t h ", "gonorth");
    fprintf(input, "RQC\n");
    for (i = 0; i < 64; i++) compare(input, routine, stack);
    fprintf(input, "RQC\n");
    memset(longLeft, 'x', 305);
    memset(longLeft + 253, ' ', 5);
    longLeft[305] = 0;
    memset(longRight, 'x', 300);
    longRight[300] = 0;
    for (i = 0; i <= sizeof(cases) / sizeof(cases[0]); i++) {
        const char* left = i == sizeof(cases) / sizeof(cases[0]) ? longLeft : cases[i].left;
        const char* right = i == sizeof(cases) / sizeof(cases[0]) ? longRight : cases[i].right;
        unsigned int result = i == sizeof(cases) / sizeof(cases[0]) ? 0 : (unsigned int)cases[i].result & 0xffff;
        prepare(input, left, right);
        compare(input, routine, stack);
        fprintf(input, "RQA\nRQX\nRQM 0000\nRQM %04X\nRQM %04X\n", stack, stack + 1);
        expected[count++] = result & 255;
        expected[count++] = result >> 8;
        expected[count++] = 21;
        expected[count++] = 0;
        expected[count++] = 0x8f;
        /* Verify both source strings, including their terminators, survive. */
        for (j = 0; j <= strlen(left); j++) {
            fprintf(input, "RQM %04X\n", 0xa0fd + j);
            expected[count++] = (unsigned char)left[j];
        }
        for (j = 0; j <= strlen(right); j++) {
            fprintf(input, "RQM %04X\n", 0x8000 + j);
            expected[count++] = (unsigned char)right[j];
        }
        assert(count < sizeof(expected) / sizeof(expected[0]));
    }
    rewind(input);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), STDIN_FILENO) >= 0);
        assert(dup2(fileno(output), STDOUT_FILENO) >= 0);
        assert(dup2(fileno(output), STDERR_FILENO) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom",
               getenv("X16_ROM"), "-prg", argv[1], "-warp", "-sound", "none", (char*)NULL);
        _exit(127);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    rewind(output);
    while (fgets(line, sizeof(line), output)) {
        char extra;
        if (sscanf(line, "%x %c", &value, &extra) != 1) continue;
        if (!phase) before = value;
        else if (phase == 1) cycles = value - before;
        else { assert(phase - 2 < count); assert(value == expected[phase - 2]); }
        phase++;
    }
    assert(phase == count + 2);
    printf("Dictionary comparison: 64 phrases, %lu cycles\n", cycles);
    fflush(stdout);
    fclose(input);
    fclose(output);
    return 0;
}
