/* Execute the actual parser and text loops from the complete product image. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target-fixture.h"

static unsigned int symbol(const char* labels, const char* wanted)
{
    FILE* file = fopen(labels, "r");
    char line[256], name[128];
    unsigned int address;
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "al %x %127s", &address, name) == 2 &&
            name[0] == '.' && strcmp(name + 1, wanted) == 0) {
            fclose(file);
            return address;
        }
    }
    abort();
}

int main(int argc, char** argv)
{
    static const unsigned char phrase[] = "go!,  north.  (now)";
    static const unsigned char cleaned[] = "go north now";
    static const unsigned char crossing[] = { 'a', ',', ' ', ' ', 'b', '!', 0xc1, 0 };
    static const unsigned char crossingCleaned[] = { 'a', ' ', 'b', 0xc1, 0 };
    static const unsigned char wrapInput[] = "ab cd ef gh ij";
    static const unsigned char wrapped[] = "ab cd\nef gh\nij";
    static const unsigned char borderInput[] = "one\n\ntwo\n";
    static const unsigned char borderExpected[] = { 8, 158, 'o', 32, 't', 32, 0, 0x5a };
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    FILE* bank;
    unsigned int routine, stack, size, address, i, repeat, wrap, border, buffers, lines;
    unsigned long value, before = 0, elapsed = 0, wrapCycles = 0, borderCycles = 0;
    unsigned int parserValues = 2 + sizeof(cleaned) + sizeof(crossingCleaned);
    unsigned int wrapValues = 2 + sizeof(wrapped) + 2;
    char line[256];
    int status, byte, phase = 0, valid = 1;
    pid_t child;
    assert(argc == 5 && input && output);
    wrap = symbol(argv[2], "_b3WrapText");
    border = symbol(argv[2], "_b3DrawBorder");
    buffers = symbol(argv[2], "_b3TextBuffers");
    lines = symbol(argv[2], "_b3LastBoxLines");
    routine = symbol(argv[2], "_b7StripExtraChars");
    size = symbol(argv[2], "_b7StripExtraCharsEnd") - routine;
    stack = symbol(argv[2], "c_sp");
    bank = fopen(argv[3], "rb");
    assert(bank);
    address = (unsigned int)fgetc(bank);
    address |= (unsigned int)fgetc(bank) << 8;
    fprintf(input, "RAM 07\n");
    while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", address++, byte);
    fclose(bank);
    /* The C software stack is fixed RAM; the parser routine lives in bank 7. */
    fprintf(input, "RAM 00\nSTM %04X 00\nSTM %04X 8F\nRAM 07\nRQC\n", stack, stack + 1);
    for (repeat = 0; repeat < 32; repeat++) {
        for (i = 0; i < sizeof(phrase); i++) fprintf(input, "STM %04X %02X\n", 0x8000 + i, phrase[i]);
        fprintf(input, "STA 00\nSTX 80\nRUN %04X\n", routine);
    }
    fprintf(input, "RQC\n");
    for (i = 0; i < sizeof(cleaned); i++) fprintf(input, "RQM %04X\n", 0x8000 + i);
    for (i = 0; i < sizeof(crossing); i++) fprintf(input, "STM %04X %02X\n", 0x80fd + i, crossing[i]);
    fprintf(input, "STA FD\nSTX 80\nRUN %04X\n", routine);
    for (i = 0; i < sizeof(crossingCleaned); i++) fprintf(input, "RQM %04X\n", 0x80fd + i);
    bank = fopen(argv[4], "rb");
    assert(bank);
    address = (unsigned int)fgetc(bank);
    address |= (unsigned int)fgetc(bank) << 8;
    fprintf(input, "RAM 03\n");
    while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", address++, byte);
    fclose(bank);
    fprintf(input, "STM %04X 00\nRQC\n", lines);
    for (repeat = 0; repeat < 32; repeat++) {
        for (i = 0; i < sizeof(wrapInput); i++) fprintf(input, "STM %04X %02X\n", 0x8000 + i, wrapInput[i]);
        fprintf(input, "STM %04X FC\nSTM %04X 8E\nSTM 8EFC 05\nSTM 8EFD 00\nSTM 8EFE 00\nSTM 8EFF 80\nSTA 00\nSTX 81\nRUN %04X\n", stack, stack + 1, wrap);
    }
    fprintf(input, "RQC\n");
    for (i = 0; i < sizeof(wrapped); i++) fprintf(input, "RQM %04X\n", 0x8000 + i);
    fprintf(input, "RQA\nRQM 8100\n");
    fprintf(input, "FLM %04X %04X 5A\nRQC\n", buffers + 1000, buffers + 1999);
    for (repeat = 0; repeat < 32; repeat++) {
        for (i = 0; i < sizeof(borderInput); i++) fprintf(input, "STM %04X %02X\n", buffers + i, borderInput[i]);
        fprintf(input, "STM %04X FF\nSTM %04X 8E\nSTM 8EFF 1E\nSTA 0A\nSTX 00\nRUN %04X\n", stack, stack + 1, border);
    }
    fprintf(input, "RQC\nRQM %04X\n", lines);
    {
        unsigned int offsets[] = { 0, 65, 97, 129, 161, 255, 256 };
        for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) fprintf(input, "RQM %04X\n", buffers + 1000 + offsets[i]);
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
        if (sscanf(line, "%lx %c", &value, &extra) != 1) continue;
        if (!phase) before = value;
        else if (phase == 1) elapsed = value - before;
        else if ((unsigned int)phase < parserValues) {
            unsigned int offset = phase - 2;
            unsigned char expected;
            assert(offset < sizeof(cleaned) + sizeof(crossingCleaned));
            expected = offset < sizeof(cleaned) ? cleaned[offset] : crossingCleaned[offset - sizeof(cleaned)];
            if (value != expected) {
                fprintf(stderr, "parser byte %d: got %02lx, expected %02x\n",
                        phase - 2, value, expected);
                valid = 0;
            }
        }
        else if ((unsigned int)phase == parserValues) before = value;
        else if ((unsigned int)phase == parserValues + 1) wrapCycles = value - before;
        else if ((unsigned int)phase < parserValues + wrapValues) {
            unsigned int offset = phase - parserValues - 2;
            unsigned int expected = offset < sizeof(wrapped) ? wrapped[offset] : (offset == sizeof(wrapped) ? 3 : 6);
            assert(value == expected);
        }
        else if ((unsigned int)phase == parserValues + wrapValues) before = value;
        else if ((unsigned int)phase == parserValues + wrapValues + 1) borderCycles = value - before;
        else {
            unsigned int offset = phase - parserValues - wrapValues - 2;
            assert(offset < sizeof(borderExpected) && value == borderExpected[offset]);
        }
        phase++;
    }
    assert((unsigned int)phase == parserValues + wrapValues + 2 + sizeof(borderExpected));
    printf("Text layout: 32 wraps %lu cycles; 32 borders %lu cycles\n", wrapCycles, borderCycles);
    printf("Parser cleanup: 32 phrases, %lu cycles, %u code bytes\n", elapsed, size);
    fflush(stdout);
    /* Same full-product workload: the original C routine took 511,232 cycles. */
    assert(elapsed < 90000);
    assert(size <= 96);
    assert(valid);
    fclose(input);
    fclose(output);
    return 0;
}
