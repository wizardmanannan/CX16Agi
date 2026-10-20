/* Render the production IRQ input routine into real emulator VERA memory. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target-fixture.h"

static unsigned int expected[2048], count;
static void store(FILE* input, unsigned int at, unsigned int value)
{
    fprintf(input, "STM %04X %02X\n", at, value & 255);
}
static void text(FILE* input, unsigned int at, const char* value)
{
    do { store(input, at++, (unsigned char)*value); } while (*value++);
}
static void vera(FILE* input, unsigned int at)
{
    store(input, 0x9f25, 0);
    store(input, 0x9f20, at & 255);
    store(input, 0x9f21, at >> 8);
    store(input, 0x9f22, 0);
}
static void checkRow(FILE* input, unsigned int address, unsigned int column, const char* value, unsigned int erase)
{
    unsigned int i, size = strlen(value);
    for (i = 0; i < 64; i++) {
        unsigned int tile = 0x5a, palette = 0x5a;
        if (i >= column && i < 40 && i - column < size + erase) {
            tile = i - column < size ? (unsigned char)value[i - column] : ' ';
            palette = 0x20;
        }
        vera(input, address + i * 2);
        fprintf(input, "RQM 9F23\n"); expected[count++] = tile;
        vera(input, address + i * 2 + 1);
        fprintf(input, "RQM 9F23\n"); expected[count++] = palette;
    }
}
static void fillRow(FILE* input, unsigned int address)
{
    unsigned int i;
    for (i = 0; i < 128; i++) { vera(input, address + i); store(input, 0x9f23, 0x5a); }
}
int main(int argc, char** argv)
{
    FILE* input = targetFixtureFile();
    FILE* output = targetFixtureFile();
    FILE* bank;
    unsigned int at = 0xa000, routine, value, i, row = 0xda00 + 5 * 128;
    char line[256];
    int byte, status;
    pid_t child;
    assert(argc == 4);
    routine = targetFixtureSymbol(argv[2], "b7HandleInputLine");
    bank = fopen(argv[3], "rb"); assert(bank);
    fprintf(input, "STM 0000 07\n");
    while ((byte = fgetc(bank)) != EOF) store(input, at++, byte);
    fclose(bank);
    store(input, targetFixtureSymbol(argv[2], "_inputLineDisplayed"), 0);
    store(input, targetFixtureSymbol(argv[2], "_b7ReadingString"), 1);
    store(input, targetFixtureSymbol(argv[2], "_b7InputReady"), 1);
    store(input, targetFixtureSymbol(argv[2], "_b7InputRow"), 2);
    store(input, targetFixtureSymbol(argv[2], "_b7InputColumn"), 3);
    text(input, targetFixtureSymbol(argv[2], "_b7CurrentInputStr"), "12");
    text(input, targetFixtureSymbol(argv[2], "_b7OutputString"), "Name:12?");
    store(input, targetFixtureSymbol(argv[2], "_cursorChar"), '?');
    fillRow(input, row);
    fprintf(input, "STX 08\nRUN %04X\n", routine);
    checkRow(input, row, 3, "Name:12?", 0);
    text(input, targetFixtureSymbol(argv[2], "_b7OutputString"), "Name:1?");
    fprintf(input, "RUN %04X\n", routine);
    checkRow(input, row, 3, "Name:1?", 1);
    row = 0xda00 + 27 * 128;
    store(input, targetFixtureSymbol(argv[2], "_b7InputRow"), 24);
    store(input, targetFixtureSymbol(argv[2], "_b7InputColumn"), 39);
    text(input, targetFixtureSymbol(argv[2], "_b7OutputString"), "long");
    fillRow(input, row);
    fprintf(input, "RUN %04X\n", routine);
    checkRow(input, row, 39, "l", 0);
    store(input, targetFixtureSymbol(argv[2], "_b7InputRow"), 255);
    fillRow(input, row);
    fprintf(input, "RUN %04X\n", routine);
    checkRow(input, row, 64, "", 0);
    /* A hidden normal prompt and an incomplete publication leave VRAM alone. */
    store(input, targetFixtureSymbol(argv[2], "_b7ReadingString"), 0);
    store(input, targetFixtureSymbol(argv[2], "_user_input_line"), 23);
    store(input, targetFixtureSymbol(argv[2], "_user_input_line") + 1, 0);
    row = 0xda00 + 26 * 128;
    fillRow(input, row);
    fprintf(input, "RUN %04X\n", routine);
    checkRow(input, row, 64, "", 0);
    store(input, targetFixtureSymbol(argv[2], "_inputLineDisplayed"), 1);
    store(input, targetFixtureSymbol(argv[2], "_b7InputReady"), 0);
    fprintf(input, "RUN %04X\n", routine);
    checkRow(input, row, 64, "", 0);
    store(input, targetFixtureSymbol(argv[2], "_b7InputReady"), 1);
    text(input, targetFixtureSymbol(argv[2], "_b7OutputString"), "Cmd:go?");
    fprintf(input, "STX FF\nRUN %04X\n", routine);
    checkRow(input, row, 0, "Cmd:go?", 0);
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
        if (value != expected[i]) fprintf(stderr, "input tile byte %u: got %x, expected %x\n", i, value, expected[i]);
        assert(value == expected[i++]);
    }
    assert(i == count);
    fclose(input); fclose(output);
    puts("IRQ input tile/palette cases passed");
    return 0;
}
