/* Host display/parser tests call the one production 65C02 formatter in x16emu.
 * This transports fixtures and bytes; it does not implement format codes. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "stringFormat.h"
#include "logic.h"
#include "object.h"

extern char string[12][40], b7WordText[10][80];
extern byte* var;
extern int numInputWords, currentLog;
extern char* getMessagePointer(byte logic, byte message);
byte textClipColumns, textClipRows;

static unsigned int address(const char* symbol)
{
    char line[256], name[128];
    unsigned int value;
    FILE* labels = fopen(getenv("FORMAT_TARGET_LABELS"), "r");
    assert(labels);
    while (fgets(line, sizeof(line), labels)) {
        if (sscanf(line, "al %x %127s", &value, name) == 2 &&
            name[0] == '.' && strcmp(name + 1, symbol) == 0) {
            fclose(labels);
            return value;
        }
    }
    fclose(labels);
    fprintf(stderr, "missing target symbol: %s\n", symbol);
    abort();
}

static void bytes(FILE* commands, unsigned int at, const void* data, size_t length)
{
    const unsigned char* source = data;
    size_t i;
    for (i = 0; i < length; i++) fprintf(commands, "STM %04X %02X\n", at + (unsigned int)i, source[i]);
}

static void putWord(FILE* commands, unsigned int at, unsigned int value)
{
    unsigned char pair[2] = { value & 255, value >> 8 };
    bytes(commands, at, pair, 2);
}

static byte targetTransform(const char* source, byte sourceBank, char* destination,
                            byte destinationBank, size_t capacity, byte controls)
{
    FILE* commands;
    FILE* responses;
    LOGICFile logic;
    char line[256];
    unsigned int packed, i, j, value;
    size_t length, produced = 0;
    char* output;
    pid_t child;
    int status, result = -1, lines = -1;
    byte active = 1;
    (void)sourceBank;
    if (!capacity) return 0;
    assert(capacity <= 2000 && strlen(source) < 0x1f00);
    commands = tmpfile();
    responses = tmpfile();
    assert(commands && responses);
    fprintf(commands, "RUN %04X\nRAM 00\n", address("targetPrepareKq3Entry"));
    bytes(commands, address("_bridgeActive"), &active, 1);
    bytes(commands, address("_bridgeControls"), &controls, 1);
    bytes(commands, address("_textClipColumns"), &textClipColumns, 1);
    bytes(commands, address("_textClipRows"), &textClipRows, 1);
    putWord(commands, address("_bridgeCapacity"), capacity);
    bytes(commands, address("_variables"), var, 256);
    putWord(commands, address("_numInputWords"), numInputWords);
    for (i = 0; i < 2; i++) {
        b5GetLogicFile(&logic, i ? currentLog : 0);
        fprintf(commands, "RAM 00\n");
        bytes(commands, address("_bridgeCounts") + i, &logic.numMessages, 1);
        packed = 0xa000;
        for (j = 0; j < logic.numMessages; j++) {
            char* text = getMessagePointer(i ? currentLog : 0, j);
            if (!text) text = "";
            length = strlen(text) + 1;
            assert(packed + length <= 0xc000);
            fprintf(commands, "RAM 00\n");
            putWord(commands, address("_bridgeMessages") + i * 512 + j * 2, packed);
            fprintf(commands, "RAM %02X\n", i ? 20 : 19);
            bytes(commands, packed, text, length);
            packed += length;
        }
    }
    fprintf(commands, "RAM 07\n");
    bytes(commands, address("_string"), string, sizeof(string));
    bytes(commands, address("_b7WordText"), b7WordText, sizeof(b7WordText));
    fprintf(commands, "RAM 0D\n");
    putWord(commands, address("_bDNumObjects"), bDNumObjects);
    packed = 0xa400;
    for (i = 0; i < (unsigned int)bDNumObjects; i++) {
        const char* name = bDObjects[i].name ? bDObjects[i].name : "";
        length = strlen(name) + 1;
        assert(packed + length <= 0xc000);
        putWord(commands, address("_bDObjects") + i * 5 + 1, packed);
        bytes(commands, packed, name, length);
        packed += length;
    }
    fprintf(commands, "RAM 15\n");
    bytes(commands, 0xa100, source, strlen(source) + 1);
    fprintf(commands, "RUN %04X\nRAM 00\nRQM %04X\nRAM 16\n",
            address("targetBridgeEntry"), address("_targetResult"));
    fprintf(commands, "RAM 00\nRQM %04X\nRAM 16\n", address("_bridgeLines"));
    if (controls) fprintf(commands, "RAM 15\n");
    for (i = 0; i < capacity; i++) fprintf(commands, "RQM %04X\n", (controls ? 0xa100 : 0xa300) + i);
    rewind(commands);
    fflush(responses);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(commands), STDIN_FILENO) >= 0);
        assert(dup2(fileno(responses), STDOUT_FILENO) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom",
               getenv("X16_ROM"), "-prg", getenv("FORMAT_TARGET_PRG"),
               "-warp", "-sound", "none", (char*)NULL);
        _exit(127);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    rewind(responses);
    output = malloc(capacity);
    assert(output);
    while (fgets(line, sizeof(line), responses)) {
        char extra;
        if (sscanf(line, "%x %c", &value, &extra) != 1) continue;
        if (result == -1) result = value;
        else if (lines == -1) lines = value;
        else {
            assert(produced < capacity);
            output[produced++] = value;
        }
    }
    assert(result == 0 && produced == capacity && memchr(output, 0, capacity));
    length = strlen(output);
    /* Retain the host fixtures' bank-copy observations at the transport edge. */
    for (i = 0; i < length; i += j) {
        j = length - i > 50 ? 50 : length - i;
        memCpyBanked((byte*)destination + i, (byte*)output + i, destinationBank, j);
    }
    memCpyBanked((byte*)destination + length, (byte*)output + length, destinationBank, 1);
    free(output);
    fclose(commands);
    fclose(responses);
    return lines;
}

void expandFormatCodes(const char* source, byte sourceBank, char* destination,
                       byte destinationBank, size_t capacity)
{
    targetTransform(source, sourceBank, destination, destinationBank, capacity, 0);
}

void b3ApplyTextModeControls(char* text)
{
    targetTransform(text, 3, text, 3, strlen(text) + 1, 1);
}

byte b3ClipTextToMap(char* text)
{
    return targetTransform(text, 3, text, 3, strlen(text) + 1, 2);
}
