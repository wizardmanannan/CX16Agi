/* Inventory name limits through the linked product's actual assembly copier. */
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
static void load(FILE* input, const char* path, unsigned int bank, int header)
{
    FILE* file = fopen(path, "rb");
    unsigned int at = 0xa000;
    int byte;
    assert(file);
    if (header) { at = fgetc(file); at |= (unsigned int)fgetc(file) << 8; }
    fprintf(input, "RAM %02X\n", bank);
    while ((byte = fgetc(file)) != EOF) fprintf(input, "STM %04X %02X\n", at++, byte);
    fclose(file);
}
int main(int argc, char** argv)
{
    const unsigned int limits[] = { 0, 1, 20, 255, 20 };
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    unsigned int routine, source, stack, i, j, value, destination, mismatches = 0, highlightStart;
    char line[256];
    int status;
    pid_t child;
    assert(argc == 5);
    routine = targetFixtureSymbol(argv[2], "_bDDisplayInventoryInner");
    source = targetFixtureSymbol(argv[2], "_bDObjData");
    stack = targetFixtureSymbol(argv[2], "c_sp");
    load(input, argv[4], 5, 1);
    load(input, argv[3], 13, 0);
    for (i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
        destination = i == 4 ? 0x7f1 : 0x602;
        fprintf(input, "STM 0000 0D\nSTM %04X 00\nFLM 0601 07F6 5A\n", source + 255);
        for (j = 0; j < 255; j++) fprintf(input, "STM %04X %02X\n", source + j, 'a' + j % 26);
        fprintf(input, "STM 00BD %02X\nSTM 00BE %02X\nSTM 00BF 00\nSTM 00C0 81\nSTM 00C1 %02X\nSTM 00C2 %02X\n",
                destination & 255, destination >> 8, source & 255, source >> 8);
        fprintf(input, "STM 8100 00\nSTM 8101 A0\nSTM 8102 15\nSTM 8103 00\nSTM %04X 00\nSTM %04X 8F\nSTA %02X\nSST %02X\nRUN %04X\n",
                stack, stack + 1, limits[i], i % 2 ? 2 : 0, routine);
        if (i == 4) {
            check(input, 0x8103, 1);
            check(input, 0xbd, 0x11);
            check(input, 0xbe, 6);
            for (j = 0; j < 16; j++) check(input, 0x602 + j, j < 15 ? 'a' + (j + 5) % 26 : 0x5a);
            fprintf(input, "RAM 15\n");
            for (j = 495; j < 500; j++) check(input, 0xa000 + j, 'a' + j - 495);
            fprintf(input, "RAM 0D\n");
        } else {
            for (j = 0; j <= limits[i]; j++) check(input, destination + j, j < limits[i] ? 'a' + j % 26 : 0x5a);
        }
        check(input, stack, 0); check(input, stack + 1, 0x8f);
        check(input, 0x601, 0x5a); check(input, 0x7f6, 0x5a);
        for (j = 0; j <= 255; j++) check(input, source + j, j < 255 ? 'a' + j % 26 : 0);
    }
    highlightStart = count;
    {
        const unsigned int lengths[] = { 0, 2, 255 };
        unsigned int objects = targetFixtureSymbol(argv[2], "_bDObjects");
        unsigned int selected = targetFixtureSymbol(argv[2], "_bDSelectedObject");
        unsigned int rows = targetFixtureSymbol(argv[2], "_bDObjectRowPaletteAddresses");
        unsigned int nameLengths = targetFixtureSymbol(argv[2], "_bDObjectNameLengths");
        routine = targetFixtureSymbol(argv[2], "bDHighlightInventoryRow");
        fprintf(input, "RAM 0D\nSTM %04X %02X\nSTM %04X %02X\nSTM %04X 00\nSTM %04X 00\n",
                selected, objects & 255, selected + 1, objects >> 8, objects + 3, objects + 4);
        fprintf(input, "STM %04X 81\nSTM %04X DA\nSTM %04X 00\nSTM %04X FF\n", rows, rows + 1,
                targetFixtureSymbol(argv[2], "_bDSelectedScreenObjNum"),
                targetFixtureSymbol(argv[2], "_bDUnselectedScreenObjNum"));
        for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
            fprintf(input, "STM %04X %02X\nSTM 9F25 00\nSTM 9F20 80\nSTM 9F21 DA\nSTM 9F22 10\n", nameLengths, lengths[i]);
            for (j = 0; j < 128; j++) fprintf(input, "STM 9F23 5A\n");
            fprintf(input, "STM 0000 0D\nRUN %04X\n", routine);
            for (j = 0; j < 64; j++) {
                unsigned int address = 0xda81 + j * 2;
                fprintf(input, "STM 9F25 00\nSTM 9F20 %02X\nSTM 9F21 %02X\nSTM 9F22 00\n",
                        address & 255, address >> 8);
                check(input, 0x9f23, j < lengths[i] && j < 20 ? 0x20 : 0x5a);
            }
        }
        routine = targetFixtureSymbol(argv[2], "_bDPadWordsWithSpaces");
        for (i = 0; i < 2; i++) {
            unsigned int length = i ? 2 : 255, spaces = i ? 36 : 0;
            fprintf(input, "STM 0000 0D\nSTM %04X %02X\nFLM 0601 07F6 5A\nSTM 00BD 02\nSTM 00BE 06\n",
                    nameLengths, length);
            fprintf(input, "STM %04X FF\nSTM %04X 8E\nSTM 8EFF 00\nSTA %02X\nRUN %04X\nRQA\n",
                    stack, stack + 1, i ? 2 : 20, routine);
            expected[count++] = spaces;
            check(input, 0xbd, 2 + spaces); check(input, 0xbe, 6);
            check(input, stack, 0); check(input, stack + 1, 0x8f);
            check(input, nameLengths, length);
            for (j = 0; j <= spaces; j++) check(input, 0x602 + j, j < spaces ? 32 : 0x5a);
        }
    }
    rewind(input);
    child = fork(); assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), 0) >= 0 && dup2(fileno(output), 1) >= 0 && dup2(fileno(output), 2) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom", getenv("X16_ROM"),
                "-prg", argv[1], "-warp", "-sound", "none", (char*)NULL);
        _exit(127);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    rewind(output); i = 0;
    while (fgets(line, sizeof(line), output)) {
        char extra;
        if (sscanf(line, "%x %c", &value, &extra) != 1) continue;
        assert(i < count);
        if (value != expected[i]) {
            if (mismatches++ < 16 || (i >= highlightStart && i < highlightStart + 3))
                fprintf(stderr, "inventory byte %u: got %x, expected %x\n", i, value, expected[i]);
        }
        i++;
    }
    assert(i == count);
    assert(mismatches == 0);
    fclose(input); fclose(output);
    puts("compiled inventory name-copy cases passed");
    return 0;
}
