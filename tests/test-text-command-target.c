/* Compiled command operands at the real bank-3 layout boundary. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target-fixture.h"

static void bytes(FILE* input, unsigned int at, const unsigned char* data, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++) fprintf(input, "STM %04X %02X\n", at + (unsigned int)i, data[i]);
}

int main(int argc, char** argv)
{
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    FILE* bank;
    unsigned int stack, fetch, helper, pop, var, at, i, j, value, count = 0;
    unsigned int expected[256];
    unsigned char jump[3], capture[40];
    const unsigned char fetchCode[] = { 0xae,0x10,0x90,0xbd,0x00,0x90,0xee,0x10,0x90,0xa2,0,0x60 };
    char line[256];
    int byte, status;
    pid_t child;
    assert(argc == 6);
    stack = targetFixtureSymbol(argv[2], "c_sp");
    fetch = targetFixtureSymbol(argv[2], "_loadAndIncWinCode");
    helper = targetFixtureSymbol(argv[2], "_b3PrintMessageInTextbox");
    pop = targetFixtureSymbol(argv[2], "incsp4");
    var = targetFixtureSymbol(argv[2], "_var");
    bank = fopen(argv[3], "rb");
    assert(bank);
    at = (unsigned int)fgetc(bank);
    at |= (unsigned int)fgetc(bank) << 8;
    fprintf(input, "RAM 05\n");
    while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", at++, byte);
    fclose(bank);
    /* Only instruction transport and the display sink are substituted. */
    bytes(input, 0x9100, fetchCode, sizeof(fetchCode));
    jump[0] = 0x4c; jump[1] = 0; jump[2] = 0x91;
    bytes(input, fetch, jump, sizeof(jump));
    at = 0;
    capture[at++] = 0x8d; capture[at++] = 0x20; capture[at++] = 0x90;
    for (i = 0; i < 4; i++) {
        capture[at++] = 0xa0; capture[at++] = i;
        capture[at++] = 0xb1; capture[at++] = stack;
        capture[at++] = 0x8d; capture[at++] = 0x21 + i; capture[at++] = 0x90;
    }
    capture[at++] = 0x4c; capture[at++] = pop & 255; capture[at++] = pop >> 8;
    bytes(input, 0x9120, capture, at);
    fprintf(input, "RAM 03\n");
    jump[1] = 0x20;
    bytes(input, helper, jump, sizeof(jump));
    fprintf(input, "STM %04X 00\nSTM %04X 93\nSTM 932A 01\n", var, var + 1);
    for (i = 0; i < 2; i++) {
        unsigned int routine = targetFixtureSymbol(argv[2], i ? "_b5Print_at_v" : "_b5Print_at");
        unsigned char operands[] = { i ? 42 : 1, 2, 10, 18 };
        bytes(input, 0x9000, operands, sizeof(operands));
        fprintf(input, "STM 9010 00\nSTM %04X 00\nSTM %04X 8F\nSTM 0000 05\nRUN %04X\n",
                stack, stack + 1, routine);
        /* Sink receives wrap, width, row, column and message, in stack order. */
        for (j = 0; j < 5; j++) fprintf(input, "RQM %04X\n", 0x9020 + j);
        expected[count++] = 1; expected[count++] = 18; expected[count++] = 2;
        expected[count++] = 10; expected[count++] = 1;
        fprintf(input, "RQM 9010\nRQM %04X\nRQM %04X\n", stack, stack + 1);
        expected[count++] = 4; expected[count++] = 0; expected[count++] = 0x8f;
    }
    for (i = 4; i < 6; i++) {
        bank = fopen(argv[i], "rb"); assert(bank);
        at = (unsigned int)fgetc(bank); at |= (unsigned int)fgetc(bank) << 8;
        fprintf(input, "STM 0000 %02X\n", i == 4 ? 1 : 7);
        while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", at++, byte);
        fclose(bank);
    }
    {
        unsigned int current = targetFixtureSymbol(argv[2], "_b7CurrentInputStr");
        unsigned int previous = targetFixtureSymbol(argv[2], "_b7LastLine");
        unsigned int outputLine = targetFixtureSymbol(argv[2], "_b7OutputString");
        const unsigned char prefixes[][8] = { "A", "123456", "A", "abc" };
        const unsigned char results[][8] = { "Aa'!", "123456", "Aa", "" };
        const unsigned char outputs[][16] = { "Cmd4:Aa'!_", "Cmd4:123456_", "Cmd4:Aa_", "Cmd4:_" };
        bytes(input, targetFixtureSymbol(argv[2], "_string"), (const unsigned char*)"Cmd%v2:", 8);
        bytes(input, previous, (const unsigned char*)"Aa'!", 5);
        fprintf(input, "STM 9302 04\nSTM %04X 60\n", targetFixtureSymbol(argv[2], "mainLoop"));
        for (i = 0; i < 4; i++) {
            unsigned int opcode = i == 3 ? 138 : 137;
            unsigned int bankAddress = targetFixtureSymbol(argv[2], "codeBankArray") + opcode;
            unsigned int vector = targetFixtureSymbol(argv[2], "jmpTableCommands2") + (opcode - 128) * 2;
            unsigned char dispatch[] = { 0xad, bankAddress & 255, bankAddress >> 8, 0x85, 0, 0x6c, vector & 255, vector >> 8 };
            bytes(input, current, prefixes[i], strlen((const char*)prefixes[i]) + 1);
            fprintf(input, "STM 9318 %02X\n", i == 2 ? 2 : 40);
            bytes(input, 0x9180, dispatch, sizeof(dispatch));
            fprintf(input, "RUN 9180\nSTM 0000 07\n");
            for (j = 0; j <= strlen((const char*)results[i]); j++) {
                fprintf(input, "RQM %04X\n", current + j);
                expected[count++] = results[i][j];
            }
            for (j = 0; j <= strlen((const char*)outputs[i]); j++) {
                fprintf(input, "RQM %04X\n", outputLine + j);
                expected[count++] = outputs[i][j];
            }
            fprintf(input, "RQM %04X\nRQM %04X\nRQM %04X\n", targetFixtureSymbol(argv[2], "_b7InputReady"), stack, stack + 1);
            expected[count++] = 1; expected[count++] = 0; expected[count++] = 0x8f;
        }
    }
    rewind(input);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), 0) >= 0 && dup2(fileno(output), 1) >= 0 && dup2(fileno(output), 2) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom", getenv("X16_ROM"),
                "-prg", argv[1], "-warp", "-sound", "none", (char*)NULL);
        _exit(127);
    }
    assert(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        rewind(output);
        while (fgets(line, sizeof(line), output)) fputs(line, stderr);
        fprintf(stderr, "positioned-print emulator status: %d\n", status);
        abort();
    }
    rewind(output);
    i = 0;
    while (fgets(line, sizeof(line), output)) {
        char extra;
        if (sscanf(line, "%x %c", &value, &extra) != 1) continue;
        assert(i < count);
        if (value != expected[i]) fprintf(stderr, "positioned print byte %u: got %u, expected %u\n", i, value, expected[i]);
        assert(value == expected[i++]);
    }
    assert(i == count);
    fclose(input);
    fclose(output);
    puts("compiled positioned-print cases passed");
    return 0;
}
