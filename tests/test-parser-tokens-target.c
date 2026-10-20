/* Exercise the tokenizer's real product code and cc65 pointer/stack ABI. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target-fixture.h"

static unsigned int expected[2048], count;
static void check(FILE* input, unsigned int address, unsigned int value)
{
    assert(count < sizeof(expected) / sizeof(expected[0]));
    expected[count++] = value;
    fprintf(input, "RQM %04X\n", address);
}
static void call(FILE* input, unsigned int routine, unsigned int stack, const char* text)
{
    unsigned int i;
    for (i = 0; i <= strlen(text); i++) fprintf(input, "STM %04X %02X\n", 0x80fd + i, (unsigned char)text[i]);
    fprintf(input, "STM 0000 07\nSTM %04X FE\nSTM %04X 8E\nSTM 8EFE FD\nSTM 8EFF 80\nSTA FD\nSTX 83\nRUN %04X\n", stack, stack + 1, routine);
}
static void tokenCase(FILE* input, unsigned int routine, unsigned int stack,
                      const char* text, const char* split, const unsigned int* offsets, unsigned int tokens)
{
    unsigned int i, size = strlen(text) + 1, last = 0x83fd + tokens * 2 - 2;
    fprintf(input, "FLM 83FC 8425 5A\nSTM 80FC 5A\nSTM %04X 5A\n", 0x80fd + size);
    call(input, routine, stack, text);
    fprintf(input, "RQA\nRQX\n");
    assert(count + 2 < sizeof(expected) / sizeof(expected[0]));
    expected[count++] = last & 255;
    expected[count++] = last >> 8;
    check(input, 0, 7);
    check(input, stack, 0);
    check(input, stack + 1, 0x8f);
    check(input, 0x80fc, 0x5a);
    for (i = 0; i < size; i++) check(input, 0x80fd + i, (unsigned char)split[i]);
    check(input, 0x80fd + size, 0x5a);
    check(input, 0x83fc, 0x5a);
    for (i = 0; i < 40; i++) {
        unsigned int pointer = i / 2 < tokens ? 0x80fd + offsets[i / 2] : 0x5a5a;
        check(input, 0x83fd + i, i % 2 ? pointer >> 8 : pointer & 255);
    }
    check(input, 0x8425, 0x5a);
}
int main(int argc, char** argv)
{
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    FILE* bank;
    unsigned int routine, stack, address, i, value, phase = 0;
    unsigned int offsets[20], pair[] = { 1, 4 }, tab[] = { 0, 4 }, longOffsets[] = { 0, 301 };
    unsigned long before = 0, cycles = 0;
    char words[40], split[40], longText[304], longSplit[304], line[256];
    int byte, status;
    pid_t child;
    assert(argc == 4);
    routine = targetFixtureSymbol(argv[2], "_b7TokenizeWords");
    stack = targetFixtureSymbol(argv[2], "c_sp");
    bank = fopen(argv[3], "rb");
    assert(bank);
    address = (unsigned int)fgetc(bank);
    address |= (unsigned int)fgetc(bank) << 8;
    fprintf(input, "RAM 07\n");
    while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", address++, byte);
    fclose(bank);
    for (i = 0; i < 20; i++) {
        offsets[i] = i * 2;
        words[i * 2] = split[i * 2] = 'a' + i;
        words[i * 2 + 1] = i == 19 ? 0 : ' ';
        split[i * 2 + 1] = 0;
    }
    fprintf(input, "RQC\n");
    for (i = 0; i < 64; i++) call(input, routine, stack, words);
    fprintf(input, "RQC\n");
    tokenCase(input, routine, stack, "", "", offsets, 0);
    tokenCase(input, routine, stack, "   ", "   ", offsets, 0);
    tokenCase(input, routine, stack, "a", "a", offsets, 1);
    tokenCase(input, routine, stack, " a  b ", " a\0 b\0", pair, 2);
    tokenCase(input, routine, stack, "a\tb c", "a\tb\0c", tab, 2);
    tokenCase(input, routine, stack, words, split, offsets, 20);
    memset(longText, 'x', 300);
    strcpy(longText + 300, " y ");
    memcpy(longSplit, longText, sizeof(longText));
    longSplit[300] = longSplit[302] = 0;
    tokenCase(input, routine, stack, longText, longSplit, longOffsets, 2);
    rewind(input);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), STDIN_FILENO) >= 0 && dup2(fileno(output), STDOUT_FILENO) >= 0 && dup2(fileno(output), STDERR_FILENO) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom", getenv("X16_ROM"), "-prg", argv[1], "-warp", "-sound", "none", (char*)NULL);
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
    printf("Tokenization: 64 x 20 words, %lu cycles\n", cycles);
    fflush(stdout);
    assert(cycles < 160000);
    fclose(input);
    fclose(output);
    return 0;
}
