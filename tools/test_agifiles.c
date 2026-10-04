/* Exercise the production AGI directory loader with owned binary inputs. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/agifiles.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char fixture[4096];

extern boolean b6LoadAGIDir(int dirNum, const char *fileName, int *count);

/* Match the production helper's directory-entry copy without bank hardware. */
void b10SetResourceDirectory(AGIFilePosType *source, AGIFilePosType *destination)
{
    *destination = *source;
}

static void cleanup(void)
{
    const char *names[] = {
        "one.bin", "type1.bin", "type2.bin", "type3.bin", "sentinel.bin", "empty.bin",
        "full.bin", "too-many.bin", "partial.bin", "missing.bin", "read-error"
    };
    char path[4096];
    bool failed = false;

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (snprintf(path, sizeof(path), "%s/%s", fixture, names[i]) >= (int)sizeof(path)) {
            failed = true;
        } else if (i == sizeof(names) / sizeof(names[0]) - 1) {
            if (rmdir(path) && errno != ENOENT) {
                fprintf(stderr, "Could not remove fixture directory %s: %s\n", path, strerror(errno));
                failed = true;
            }
        } else if (unlink(path) && errno != ENOENT) {
            fprintf(stderr, "Could not remove fixture file %s: %s\n", path, strerror(errno));
            failed = true;
        }
    }
    if (fixture[0] && rmdir(fixture) && errno != ENOENT) {
        fprintf(stderr, "Could not remove fixture root %s: %s\n", fixture, strerror(errno));
        failed = true;
    }
    if (failed) {
        fprintf(stderr, "Could not clean AGI loader fixture: %s\n", fixture);
        _Exit(EXIT_FAILURE);
    }
}

static void path_for(char *path, size_t size, const char *name)
{
    CHECK(snprintf(path, size, "%s/%s", fixture, name) < (int)size, "Fixture path too long");
}

static void write_bytes(const char *name, const uint8_t *bytes, size_t size)
{
    char path[4096];
    path_for(path, sizeof(path), name);
    FILE *file = fopen(path, "wb");
    CHECK(file, "Cannot create directory fixture");
    CHECK(fwrite(bytes, 1, size, file) == size, "Cannot write directory fixture");
    CHECK(!fclose(file), "Cannot close directory fixture");
}

static void check_exact_single_entry(void)
{
    static const uint8_t bytes[] = { 0x12, 0x34, 0x56 };
    static const char *names[] = { "one.bin", "type1.bin", "type2.bin", "type3.bin" };
    AGIFilePosType *directories[] = { logdir, picdir, viewdir, snddir };
    char path[4096];
    unsigned int type;

    for (type = 0; type < 4; ++type) {
        int count = 0;
        write_bytes(names[type], bytes, sizeof(bytes));
        path_for(path, sizeof(path), names[type]);
        CHECK(b6LoadAGIDir(type, path, &count), "Valid one-entry directory was rejected");
        CHECK(count == 1, "Exact three-byte directory gained a phantom entry");
        CHECK(directories[type][0].fileNum == 1 && (uint32_t)directories[type][0].filePos == 0x23456,
              "Directory type decoded the wrong resource location");
    }

    static const uint8_t sentinel[] = { 0xFF, 0xFF, 0xFF };
    int count = 0;
    write_bytes("sentinel.bin", sentinel, sizeof(sentinel));
    path_for(path, sizeof(path), "sentinel.bin");
    CHECK(b6LoadAGIDir(3, path, &count) && count == 1, "Empty resource sentinel was rejected");
    CHECK(snddir[0].fileNum == 15 && snddir[0].filePos == EMPTY,
          "Empty resource sentinel decoded incorrectly");
}

static void check_empty_directory(void)
{
    static const uint8_t no_bytes[] = { 0 };
    char path[4096];
    int count = 0;
    write_bytes("empty.bin", no_bytes, 0);
    path_for(path, sizeof(path), "empty.bin");
    CHECK(b6LoadAGIDir(0, path, &count) && count == 0,
          "Empty directory was not accepted as zero entries");
}

static void check_full_directory(void)
{
    uint8_t bytes[NO_DIRECTORY_ENTRYS * 3];
    char path[4096];
    int count = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    write_bytes("full.bin", bytes, sizeof(bytes));
    path_for(path, sizeof(path), "full.bin");
    CHECK(b6LoadAGIDir(0, path, &count) && count == NO_DIRECTORY_ENTRYS,
          "Full 256-entry directory was not loaded exactly");
}

static void check_append_capacity(void)
{
    static const uint8_t bytes[] = { 0x12, 0x34, 0x56 };
    char path[4096];
    int count = NO_DIRECTORY_ENTRYS - 1;
    write_bytes("one.bin", bytes, sizeof(bytes));
    path_for(path, sizeof(path), "one.bin");
    CHECK(b6LoadAGIDir(0, path, &count) && count == NO_DIRECTORY_ENTRYS,
          "Last available directory slot was rejected");
    CHECK(logdir[NO_DIRECTORY_ENTRYS - 1].fileNum == 1 &&
          (uint32_t)logdir[NO_DIRECTORY_ENTRYS - 1].filePos == 0x23456,
          "Last available directory slot received the wrong entry");

    count = NO_DIRECTORY_ENTRYS;
    logdir[0].fileNum = 0xA5;
    logdir[0].filePos = 0x5A;
    CHECK(!b6LoadAGIDir(0, path, &count), "Directory append beyond slot 255 was accepted");
    CHECK(count == NO_DIRECTORY_ENTRYS && logdir[0].fileNum == 0xA5 &&
          (uint32_t)logdir[0].filePos == 0x5A,
          "Rejected append changed published directory state");
}

static void check_rejected_file(const char *name, const uint8_t *bytes, size_t size)
{
    AGIFilePosType before[NO_DIRECTORY_ENTRYS];
    char path[4096];
    int count = 17;
    logdir[0].fileNum = 0xA5;
    logdir[0].filePos = 0x5A;
    memcpy(before, logdir, sizeof(before));
    write_bytes(name, bytes, size);
    path_for(path, sizeof(path), name);
    CHECK(!b6LoadAGIDir(0, path, &count), "Malformed directory was accepted");
    CHECK(count == 17, "Rejected directory published a partial count");
    CHECK(!memcmp(logdir, before, sizeof(before)), "Rejected directory published partial entries");
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    uint8_t too_many[257 * 3] = { 0 };
    const uint8_t partial[] = { 0x12, 0x34, 0x56, 0x78 };
    char path[4096];

    CHECK(snprintf(fixture, sizeof(fixture), "%s/agifiles-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(fixture),
                  "Fixture path too long");
    CHECK(mkdtemp(fixture), "Cannot create AGI loader fixture directory");
    if (atexit(cleanup) != 0) {
        cleanup();
        fprintf(stderr, "Cannot register fixture cleanup\n");
        return EXIT_FAILURE;
    }

    /* This exact-length reproducer catches the legacy feof-after-read bug. */
    check_exact_single_entry();
    check_empty_directory();
    check_full_directory();
    check_append_capacity();
    check_rejected_file("too-many.bin", too_many, sizeof(too_many));
    check_rejected_file("partial.bin", partial, sizeof(partial));

    int count = 31;
    logdir[0].fileNum = 0xA5;
    logdir[0].filePos = 0x5A;
    path_for(path, sizeof(path), "missing.bin");
    CHECK(!b6LoadAGIDir(0, path, &count), "Missing directory file was accepted");
    CHECK(count == 31 && logdir[0].fileNum == 0xA5 && (uint32_t)logdir[0].filePos == 0x5A,
          "Missing directory changed published state");

    count = 29;
    path_for(path, sizeof(path), "one.bin");
    CHECK(!b6LoadAGIDir(4, path, &count), "Invalid directory type was accepted");
    CHECK(count == 29, "Invalid directory type changed the published count");

    path_for(path, sizeof(path), "read-error");
    CHECK(!mkdir(path, 0700), "Cannot create read-error fixture");
    count = 23;
    CHECK(!b6LoadAGIDir(0, path, &count), "Unreadable directory path was accepted as a resource file");
    CHECK(count == 23, "Read error changed the published entry count");

    puts("AGI directory loader checks pass: empty, exact, capacity, malformed, and read error");
    cleanup();
    return 0;
}
