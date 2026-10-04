/* Exercise the production AGI directory loader with owned binary inputs. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/agifiles.h"
#include <cbm.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char fixture[4096];
static FILE *resourceVolume;
static byte resourceWorkArea[LOCAL_WORK_AREA_SIZE];
#undef GOLDEN_RAM_WORK_AREA
#define GOLDEN_RAM_WORK_AREA resourceWorkArea
static byte *allocations[32];
static size_t allocationSizes[32];
static size_t outstandingAllocations;
static unsigned char seekResult;
static bool trapInvalidAllocation;
static bool trapInvalidResourceCopy;
static jmp_buf invalidAllocation;

extern boolean b6LoadAGIDir(int dirNum, const char *fileName, int *count);
extern boolean b6LoadAGIFile(int resType, AGIFilePosType *location, AGIFile *AGIData);

/* Match the production helper's directory-entry copy without bank hardware. */
void b10SetResourceDirectory(AGIFilePosType *source, AGIFilePosType *destination)
{
    *destination = *source;
}

unsigned char cbm_open(unsigned char lfn, unsigned char device, unsigned char sec_addr, const char *name)
{
    char path[4096], volume[16];
    (void)lfn;
    (void)device;
    (void)sec_addr;
    CHECK(sscanf(name, "%15[^,]", volume) == 1, "Invalid test volume name");
    CHECK(snprintf(path, sizeof(path), "%s/%s", fixture, volume) < (int)sizeof(path),
          "Volume fixture path too long");
    resourceVolume = fopen(path, "rb");
    return resourceVolume ? 0 : 4;
}

int cbm_read(unsigned char lfn, void *buffer, unsigned int size)
{
    (void)lfn;
    CHECK(resourceVolume, "CBM read without an open volume");
    return (int)fread(buffer, 1, size, resourceVolume);
}

void cbm_close(unsigned char lfn)
{
    (void)lfn;
    if (resourceVolume) {
        fclose(resourceVolume);
        resourceVolume = NULL;
    }
}

unsigned char cbm_k_open(void) { return seekResult; }
void cbm_k_setlfs(unsigned char lfn, unsigned char device, unsigned char sec_addr)
{ (void)lfn; (void)device; (void)sec_addr; }
void cbm_k_close(unsigned char lfn) { (void)lfn; }

byte *b10BankedAlloc(int size, byte *bank)
{
    byte *result;
    size_t slot;

    if (size < 0 || size > LARGE_SIZE) {
        if (trapInvalidAllocation)
            longjmp(invalidAllocation, 1);
        return NULL;
    }
    for (slot = 0; slot < sizeof(allocations) / sizeof(allocations[0]); ++slot)
        if (!allocations[slot]) break;
    CHECK(slot < sizeof(allocations) / sizeof(allocations[0]), "Too many banked test allocations");
    result = (byte *)calloc(size ? (size_t)size : 1, 1);
    CHECK(result, "Cannot allocate simulated banked memory");
    allocations[slot] = result;
    allocationSizes[slot] = size ? (size_t)size : 1;
    ++outstandingAllocations;
    *bank = 1;
    return result;
}

boolean b10BankedDealloc(byte *ptr, byte bank)
{
    size_t slot;
    (void)bank;
    for (slot = 0; slot < sizeof(allocations) / sizeof(allocations[0]); ++slot) {
        if (allocations[slot] == ptr) {
            free(ptr);
            allocations[slot] = NULL;
            allocationSizes[slot] = 0;
            --outstandingAllocations;
            return TRUE;
        }
    }
    return FALSE;
}

void memsetBanked(void *destination, int value, size_t size, byte bank)
{
    (void)bank;
    if (destination && size <= LARGE_SIZE)
        memset(destination, value, size);
}

void *memCpyBanked(byte *destination, byte *source, byte bank, size_t size)
{
    uintptr_t src = (uintptr_t)source;
    (void)bank;
    for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); ++i) {
        uintptr_t start = (uintptr_t)allocations[i];
        uintptr_t end = start + allocationSizes[i];
        if (allocations[i] && src >= start && src <= end + 2 &&
            (src > end || size > end - src)) {
            if (trapInvalidResourceCopy)
                longjmp(invalidAllocation, 2);
            return NULL;
        }
    }
    if (!destination)
        return NULL;
    return memcpy(destination, source, size);
}

static void cleanup(void)
{
    const char *names[] = {
        "one.bin", "type1.bin", "type2.bin", "type3.bin", "sentinel.bin", "empty.bin",
        "full.bin", "too-many.bin", "partial.bin", "missing.bin", "read-error", "vol.0", "vol.2", "vol.3",
        "logdir", "picdir", "viewdir", "snddir"
    };
    char path[4096];
    bool failed = false;

    if (resourceVolume) {
        fclose(resourceVolume);
        resourceVolume = NULL;
    }
    for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); ++i) {
        free(allocations[i]);
        allocations[i] = NULL;
        allocationSizes[i] = 0;
    }
    outstandingAllocations = 0;

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (snprintf(path, sizeof(path), "%s/%s", fixture, names[i]) >= (int)sizeof(path)) {
            failed = true;
        } else if (!strcmp(names[i], "read-error")) {
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

static uint32_t append_volume_resource(uint8_t *volume, size_t capacity, size_t *used,
                                       uint8_t volume_num, uint16_t declared_size,
                                       const uint8_t *payload, size_t available_size)
{
    size_t offset = *used;

    CHECK(offset <= capacity && capacity - offset >= 5 + available_size,
          "Volume fixture buffer is too small");
    volume[offset++] = 0x12;
    volume[offset++] = 0x34;
    volume[offset++] = volume_num;
    volume[offset++] = (uint8_t)declared_size;
    volume[offset++] = (uint8_t)(declared_size >> 8);
    if (available_size)
        memcpy(volume + offset, payload, available_size);
    *used = offset + available_size;
    return (uint32_t)(offset - 5);
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

static void check_truncated_resource_payload(void)
{
    static const uint8_t truncated_volume[] = {
        0x12, 0x34, 0x00, 0x04, 0x00, 0xA1, 0xB2
    };
    AGIFilePosType location = { 0, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.0", truncated_volume, sizeof(truncated_volume));
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Truncated resource payload was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)),
          "Truncated resource payload was published as a loaded resource");
    CHECK(outstandingAllocations == 0, "Truncated resource payload leaked its allocation");
}

static void check_short_resource_types_preserve_loaded_resource(void)
{
    static const uint8_t sound_payload[] = { 0xD4, 0xE5 };
    static const uint8_t short_logic_payload[] = { 0, 0 };
    static const uint8_t short_payload[] = { 0xA1, 0xB2 };
    static const uint8_t full_view_payload[] = { 0x51, 0x62, 0x73, 0x84 };
    static const int failed_types[] = { LOGIC, PICTURE, VIEW, SOUND };
    uint8_t volume[64], retry_volume[64];
    size_t used, retry_used;
    uint32_t baseline_offset, failed_offset, retry_offset;
    AGIFilePosType location;
    AGIFile data, before;

    used = 0;
    baseline_offset = append_volume_resource(volume, sizeof(volume), &used,
        2, sizeof(sound_payload), sound_payload, sizeof(sound_payload));
    write_bytes("vol.2", volume, used);
    location.fileNum = 2;
    location.filePos = baseline_offset;
    memset(&data, 0, sizeof(data));
    CHECK(b6LoadAGIFile(SOUND, &location, &data), "Could not load the retained resource before EOF checks");
    before = data;
    CHECK(outstandingAllocations == 1, "Retained resource did not own exactly one allocation");

    for (size_t i = 0; i < sizeof(failed_types) / sizeof(failed_types[0]); ++i) {
        uint16_t declared_size = failed_types[i] == LOGIC ? 6 : 4;
        const uint8_t *payload = failed_types[i] == LOGIC ? short_logic_payload : short_payload;
        size_t available_size = failed_types[i] == LOGIC ? sizeof(short_logic_payload) : sizeof(short_payload);

        used = 0;
        append_volume_resource(volume, sizeof(volume), &used,
            2, sizeof(sound_payload), sound_payload, sizeof(sound_payload));
        failed_offset = append_volume_resource(volume, sizeof(volume), &used,
            2, declared_size, payload, available_size);
        write_bytes("vol.2", volume, used);
        location.filePos = failed_offset;
        CHECK(!b6LoadAGIFile(failed_types[i], &location, &data),
              "Short resource payload was accepted for one resource type");
        CHECK(!memcmp(&data, &before, sizeof(data)),
              "Short resource payload changed the previously loaded resource");
        CHECK(data.code[0] == sound_payload[0] && data.code[1] == sound_payload[1],
              "Short resource payload damaged previously loaded bytes");
        CHECK(outstandingAllocations == 1,
              "Short resource payload released retained memory or leaked a candidate allocation");
    }

    retry_used = 0;
    append_volume_resource(retry_volume, sizeof(retry_volume), &retry_used,
        2, sizeof(sound_payload), sound_payload, sizeof(sound_payload));
    retry_offset = append_volume_resource(retry_volume, sizeof(retry_volume), &retry_used,
        2, sizeof(full_view_payload), full_view_payload, sizeof(full_view_payload));
    CHECK(retry_offset == failed_offset, "Retry fixture moved the failed resource offset");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release retained resource before retry");
    CHECK(outstandingAllocations == 0, "Retained resource allocation remained live before retry");
    write_bytes("vol.2", retry_volume, retry_used);
    location.filePos = retry_offset;
    CHECK(b6LoadAGIFile(VIEW, &location, &data), "Valid nonzero-offset retry after EOF failure was rejected");
    CHECK(data.totalSize == sizeof(full_view_payload) && data.codeSize == sizeof(full_view_payload) &&
          !memcmp(data.code, full_view_payload, sizeof(full_view_payload)),
          "Valid nonzero-offset retry did not publish complete view bytes");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release retried view resource");
    CHECK(outstandingAllocations == 0, "Cross-type EOF/retry case leaked banked memory");
}

static void check_truncated_resource_header(void)
{
    static const uint8_t truncated_volume[] = { 0x12 };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", truncated_volume, sizeof(truncated_volume));
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Truncated volume header was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Truncated volume header was accepted");
    CHECK(outstandingAllocations == 0, "Truncated volume header leaked an allocation");
}

static void check_missing_volume(void)
{
    AGIFilePosType location = { 3, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Missing volume was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Missing volume changed caller state");
    CHECK(outstandingAllocations == 0, "Missing volume allocated resource data");
}

static void check_seek_failure(void)
{
    static const uint8_t volume[] = { 0x12, 0x34, 2, 3, 0, 0xA1, 0xB2, 0xC3 };
    AGIFilePosType location = { 2, 0x10000 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    seekResult = 1;
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Failed volume seek was accepted");
    seekResult = 0;
    CHECK(!memcmp(&data, &before, sizeof(data)), "Failed seek outside the volume published a resource");
    CHECK(outstandingAllocations == 0, "Failed volume seek leaked an allocation");
}

static void check_offset_outside_directory_range(void)
{
    static const uint8_t volume[] = { 0x12, 0x34, 2, 3, 0, 0xA1, 0xB2, 0xC3 };
    AGIFilePosType location = { 2, 0x100000 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Offset beyond the directory's 20-bit range was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Invalid resource offset changed caller state");
}

static void check_compressed_resource_marker(void)
{
    static const uint8_t volume[] = { 0x12, 0x34, 0x82, 3, 0, 0xA1, 0xB2, 0xC3 };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;
    int type;

    write_bytes("vol.2", volume, sizeof(volume));
    for (type = LOGIC; type <= SOUND; ++type) {
        memset(&data, 0xA5, sizeof(data));
        before = data;
        CHECK(!b6LoadAGIFile(type, &location, &data),
              "Compressed resource was exposed as uncompressed data");
        CHECK(!memcmp(&data, &before, sizeof(data)),
              "Unsupported compressed resource changed caller state");
        CHECK(outstandingAllocations == 0,
              "Unsupported compressed resource allocated banked memory");
    }
}

static void check_declared_length_limit(void)
{
    static const uint8_t volume[] = { 0x12, 0x34, 2, 0x41, 0x1F };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;
    int type;

    write_bytes("vol.2", volume, sizeof(volume));
    for (type = LOGIC; type <= SOUND; ++type) {
        memset(&data, 0xA5, sizeof(data));
        before = data;
        trapInvalidAllocation = true;
        if (setjmp(invalidAllocation) == 0) {
            CHECK(!b6LoadAGIFile(type, &location, &data), "Oversized resource length was accepted");
            trapInvalidAllocation = false;
            CHECK(!memcmp(&data, &before, sizeof(data)), "Oversized resource length changed caller state");
        } else {
            trapInvalidAllocation = false;
            CHECK(false, "Declared resource length reached the bank allocator");
        }
        CHECK(outstandingAllocations == 0, "Oversized resource length leaked an allocation");
    }
}

static uint8_t maximum_payload_byte(size_t index)
{
    return (uint8_t)(index * 13u + 7u);
}

static void check_maximum_resource_length(void)
{
    uint8_t volume[5 + LARGE_SIZE];
    AGIFilePosType location = { 2, 0 };
    AGIFile data;

    for (int type = LOGIC; type <= SOUND; ++type) {
        memset(volume, 0, sizeof(volume));
        volume[0] = 0x12;
        volume[1] = 0x34;
        volume[2] = 2;
        volume[3] = (uint8_t)LARGE_SIZE;
        volume[4] = (uint8_t)(LARGE_SIZE >> 8);
        if (type == LOGIC) {
            const unsigned int code_size = LARGE_SIZE - 5;
            volume[5] = (uint8_t)code_size;
            volume[6] = (uint8_t)(code_size >> 8);
            for (size_t i = 0; i < code_size; ++i)
                volume[7 + i] = maximum_payload_byte(i);
            volume[7 + code_size] = 0;
            volume[8 + code_size] = 2;
            volume[9 + code_size] = 0;
        } else {
            for (size_t i = 0; i < LARGE_SIZE; ++i)
                volume[5 + i] = maximum_payload_byte(i);
        }
        write_bytes("vol.2", volume, sizeof(volume));
        memset(&data, 0, sizeof(data));
        CHECK(b6LoadAGIFile(type, &location, &data), "Resource exactly at LARGE_SIZE was rejected");
        if (type == LOGIC) {
            CHECK(data.totalSize == LARGE_SIZE && data.codeSize == LARGE_SIZE - 5 &&
                  data.noMessages == 0 && data.code[0] == maximum_payload_byte(0) &&
                  data.code[LOCAL_WORK_AREA_SIZE - 1] == maximum_payload_byte(LOCAL_WORK_AREA_SIZE - 1) &&
                  data.code[LOCAL_WORK_AREA_SIZE] == maximum_payload_byte(LOCAL_WORK_AREA_SIZE) &&
                  data.code[data.codeSize - 1] == maximum_payload_byte(data.codeSize - 1),
                  "Maximum-size logic resource crossed a read chunk incorrectly");
            CHECK(b10BankedDealloc((byte *)data.messagePointers, data.messageBank),
                  "Could not release maximum-size logic message storage");
        } else {
            CHECK(data.totalSize == LARGE_SIZE && data.codeSize == LARGE_SIZE &&
                  data.code[0] == maximum_payload_byte(0) &&
                  data.code[LOCAL_WORK_AREA_SIZE - 1] == maximum_payload_byte(LOCAL_WORK_AREA_SIZE - 1) &&
                  data.code[LOCAL_WORK_AREA_SIZE] == maximum_payload_byte(LOCAL_WORK_AREA_SIZE) &&
                  data.code[data.codeSize - 1] == maximum_payload_byte(data.codeSize - 1),
                  "Maximum-size resource crossed a read chunk incorrectly");
        }
        CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release maximum-size resource data");
        CHECK(outstandingAllocations == 0, "Maximum-size resource case leaked banked memory");
    }
}

static void check_valid_resources_at_nonzero_offsets(void)
{
    static const uint8_t logic[] = { 1, 0, 0xCC, 0, 2, 0 };
    static const uint8_t picture[] = { 0xA1, 0xA2, 0xA3 };
    static const uint8_t view[] = { 0xB1, 0xB2, 0xB3, 0xB4 };
    static const uint8_t sound[] = { 0xC1, 0xC2, 0xC3, 0xC4, 0xC5 };
    static const uint8_t prefix[] = { 0x91, 0x82, 0x73 };
    const uint8_t *payloads[] = { logic, picture, view, sound };
    const uint16_t sizes[] = { sizeof(logic), sizeof(picture), sizeof(view), sizeof(sound) };
    uint8_t volume[128];
    uint32_t offsets[4];
    size_t used = sizeof(prefix);
    AGIFilePosType location = { 2, 0 };
    AGIFile data;

    memcpy(volume, prefix, sizeof(prefix));
    for (int type = LOGIC; type <= SOUND; ++type)
        offsets[type] = append_volume_resource(volume, sizeof(volume), &used,
            2, sizes[type], payloads[type], sizes[type]);
    write_bytes("vol.2", volume, used);

    for (int type = LOGIC; type <= SOUND; ++type) {
        CHECK(offsets[type] > 0, "Resource offset fixture unexpectedly started at zero");
        location.filePos = offsets[type];
        memset(&data, 0, sizeof(data));
        CHECK(b6LoadAGIFile(type, &location, &data),
              "Valid resource at a nonzero directory offset was rejected");
        CHECK(data.totalSize == sizes[type] && data.code,
              "Valid nonzero-offset resource returned incorrect length or no data");
        if (type == LOGIC) {
            CHECK(data.codeSize == 1 && data.noMessages == 0 && data.code[0] == 0xCC,
                  "Nonzero-offset logic resource was parsed incorrectly");
            CHECK(b10BankedDealloc((byte *)data.messagePointers, data.messageBank),
                  "Could not release nonzero-offset logic message storage");
        } else {
            CHECK(data.codeSize == sizes[type] &&
                  !memcmp(data.code, payloads[type], sizes[type]),
                  "Nonzero-offset picture, view, or sound bytes were read from the wrong position");
        }
        CHECK(b10BankedDealloc(data.code, data.codeBank),
              "Could not release nonzero-offset resource data");
        CHECK(outstandingAllocations == 0,
              "Nonzero-offset resource case leaked banked memory");
    }
}

static void check_logic_message_table_bounds(void)
{
    static const uint8_t volume[] = {
        0x12, 0x34, 2, 7, 0, 1, 0, 0xCC, 1, 3, 0, 0xA5
    };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    trapInvalidResourceCopy = true;
    if (setjmp(invalidAllocation) == 0) {
        CHECK(!b6LoadAGIFile(LOGIC, &location, &data), "Logic message offsets exceeded the declared section");
        trapInvalidResourceCopy = false;
        CHECK(!memcmp(&data, &before, sizeof(data)), "Logic message offsets exceeded the declared section");
    } else {
        trapInvalidResourceCopy = false;
        CHECK(false, "Logic message processing read beyond its declared section");
    }
    CHECK(outstandingAllocations == 0, "Rejected logic resource leaked an allocation");
}

static void check_unterminated_logic_message(void)
{
    static const uint8_t volume[] = {
        0x12, 0x34, 2, 9, 0, 1, 0, 0xCC, 1, 5, 0, 4, 0, 0x03
    };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    CHECK(!b6LoadAGIFile(LOGIC, &location, &data), "Unterminated logic message was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Malformed logic message changed caller state");
    CHECK(outstandingAllocations == 0, "Malformed logic message leaked its allocations");
}

static void check_bad_logic_message_offset(void)
{
    static const uint8_t volume[] = {
        0x12, 0x34, 2, 10, 0, 1, 0, 0xCC, 1, 6, 0, 6, 0, 0x03, 0x76
    };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    CHECK(!b6LoadAGIFile(LOGIC, &location, &data), "Logic message offset outside strings was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Invalid logic offset changed caller state");
    CHECK(outstandingAllocations == 0, "Invalid logic offset leaked its allocations");
}

static void check_logic_message_size_mismatch(void)
{
    static const uint8_t volume[] = {
        0x12, 0x34, 2, 10, 0, 1, 0, 0xCC, 1, 5, 0, 4, 0, 0x03, 0x76
    };
    AGIFilePosType location = { 2, 0 };
    AGIFile data, before;

    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", volume, sizeof(volume));
    CHECK(!b6LoadAGIFile(LOGIC, &location, &data), "Mismatched logic message size was accepted");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Mismatched message size changed caller state");
    CHECK(outstandingAllocations == 0, "Mismatched message size leaked its allocations");
}

static void check_valid_resources_and_retry(void)
{
    static const uint8_t ordinary_volume[] = { 0x12, 0x34, 2, 3, 0, 0xA1, 0xB2, 0xC3 };
    static const uint8_t logic_volume[] = { 0x12, 0x34, 2, 6, 0, 1, 0, 0xCC, 0, 2, 0 };
    static const uint8_t logic_message_volume[] = {
        0x12, 0x34, 2, 10, 0, 1, 0, 0xCC, 1, 6, 0, 4, 0, 0x03, 0x76
    };
    static const uint8_t reordered_messages_volume[] = {
        0x12, 0x34, 2, 14, 0, 1, 0, 0xCC, 2, 10, 0,
        8, 0, 6, 0, 0x03, 0x76, 0x2A, 0x73
    };
    AGIFilePosType location = { 2, 0 };
    AGIFile data;

    write_bytes("vol.2", ordinary_volume, sizeof(ordinary_volume));
    for (int type = PICTURE; type <= SOUND; ++type) {
        memset(&data, 0, sizeof(data));
        CHECK(b6LoadAGIFile(type, &location, &data), "Valid picture, view, or sound resource was rejected");
        CHECK(data.totalSize == 3 && data.codeBank == 1 && data.code &&
              data.code[0] == 0xA1 && data.code[1] == 0xB2 && data.code[2] == 0xC3,
              "Valid picture, view, or sound resource loaded incorrectly");
        CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release valid resource data");
    }

    write_bytes("vol.2", logic_volume, sizeof(logic_volume));
    memset(&data, 0, sizeof(data));
    CHECK(b6LoadAGIFile(LOGIC, &location, &data), "Valid logic resource was rejected");
    CHECK(data.totalSize == 6 && data.codeSize == 1 && data.noMessages == 0 &&
          data.code && data.code[0] == 0xCC && data.messagePointers,
          "Valid logic resource loaded incorrectly");
    CHECK(b10BankedDealloc((byte *)data.messagePointers, data.messageBank), "Could not release logic messages");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release logic code");

    write_bytes("vol.2", logic_message_volume, sizeof(logic_message_volume));
    memset(&data, 0, sizeof(data));
    CHECK(b6LoadAGIFile(LOGIC, &location, &data), "Valid encrypted logic message was rejected");
    CHECK(data.noMessages == 1 && data.messageData[0] == 'B' && data.messageData[1] == 0,
          "Valid encrypted logic message was not decrypted within its declared text bounds");
    CHECK(b10BankedDealloc((byte *)data.messagePointers, data.messageBank), "Could not release decoded message table");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release decoded logic code");

    write_bytes("vol.2", reordered_messages_volume, sizeof(reordered_messages_volume));
    memset(&data, 0, sizeof(data));
    CHECK(b6LoadAGIFile(LOGIC, &location, &data), "Valid reordered logic message offsets were rejected");
    uint16_t messageAddresses[2];
    memcpy(messageAddresses, data.messagePointers, sizeof(messageAddresses));
    CHECK(data.messageData[0] == 'B' && data.messageData[1] == 0 &&
          data.messageData[2] == 'C' && data.messageData[3] == 0,
          "Reordered logic strings were not decrypted correctly");
    CHECK(messageAddresses[0] == (uint16_t)(uintptr_t)(data.messageData + 2) &&
          messageAddresses[1] == (uint16_t)(uintptr_t)data.messageData,
          "Logic message offsets did not preserve reordered target pointers");
    CHECK(b10BankedDealloc((byte *)data.messagePointers, data.messageBank), "Could not release reordered message table");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release reordered logic code");

    static const uint8_t truncated_volume[] = { 0x12, 0x34, 2, 4, 0, 0xA1, 0xB2 };
    static const uint8_t retry_volume[] = { 0x12, 0x34, 2, 2, 0, 0xD4, 0xE5 };
    AGIFile before;
    memset(&data, 0xA5, sizeof(data));
    before = data;
    write_bytes("vol.2", truncated_volume, sizeof(truncated_volume));
    CHECK(!b6LoadAGIFile(SOUND, &location, &data), "Truncated resource attempt was accepted before retry");
    CHECK(!memcmp(&data, &before, sizeof(data)), "Failed resource attempt changed caller state before retry");
    CHECK(outstandingAllocations == 0, "Failed resource attempt leaked before retry");
    write_bytes("vol.2", retry_volume, sizeof(retry_volume));
    CHECK(b6LoadAGIFile(SOUND, &location, &data), "Valid retry after resource failure was rejected");
    CHECK(data.totalSize == 2 && data.code[0] == 0xD4 && data.code[1] == 0xE5,
          "Valid retry did not publish the complete resource");
    CHECK(b10BankedDealloc(data.code, data.codeBank), "Could not release retried resource data");
}

static void check_startup_directory_transaction(void)
{
    static const uint8_t entry[] = { 0x12, 0x34, 0x56 };
    AGIFilePosType *directories[] = { logdir, picdir, viewdir, snddir };
    AGIFilePosType before[4];
    int *counts[] = { &numLogics, &numPictures, &numViews, &numSounds };
    const char *names[] = { "logdir", "picdir", "viewdir", "snddir" };
    char originalDirectory[4096];
    boolean loaded;

    for (int i = 0; i < 4; ++i) {
        *counts[i] = 10 + i;
        directories[i][0].fileNum = 0xA0 + i;
        directories[i][0].filePos = 0x100 + i;
        before[i] = directories[i][0];
        if (i < 3)
            write_bytes(names[i], entry, sizeof(entry));
    }

    CHECK(getcwd(originalDirectory, sizeof(originalDirectory)), "Cannot save working directory");
    CHECK(!chdir(fixture), "Cannot enter startup directory fixture");
    loaded = b6LoadAGIDirs();
    CHECK(!chdir(originalDirectory), "Cannot restore working directory");
    CHECK(!loaded, "Startup accepted a missing directory in the last resource type");
    for (int i = 0; i < 4; ++i)
        CHECK(*counts[i] == 10 + i && !memcmp(&directories[i][0], &before[i], sizeof(before[i])),
              "Failed multi-directory startup published partial state");

    write_bytes("snddir", entry, sizeof(entry));
    CHECK(!chdir(fixture), "Cannot enter startup directory fixture for retry");
    loaded = b6LoadAGIDirs();
    CHECK(!chdir(originalDirectory), "Cannot restore working directory after retry");
    CHECK(loaded, "Startup retry rejected complete resource directories");
    for (int i = 0; i < 4; ++i)
        CHECK(*counts[i] == 1 && directories[i][0].fileNum == 1 &&
              (uint32_t)directories[i][0].filePos == 0x23456,
              "Startup retry did not publish all resource directory entries");
}

int main(int argc, char **argv)
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

    if (argc == 2 && !strcmp(argv[1], "--resource-offsets")) {
        check_short_resource_types_preserve_loaded_resource();
        check_valid_resources_at_nonzero_offsets();
        puts("AGI resource offset checks pass: EOF rollback across all types and valid nonzero offsets");
        cleanup();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--resource-boundaries")) {
        check_compressed_resource_marker();
        check_declared_length_limit();
        check_maximum_resource_length();
        puts("AGI resource boundary checks pass: compression and length limits across all types");
        cleanup();
        return 0;
    }
    CHECK(argc == 1, "Unknown AGI loader test selector");

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

    check_truncated_resource_payload();
    check_short_resource_types_preserve_loaded_resource();
    check_truncated_resource_header();
    check_missing_volume();
    check_seek_failure();
    check_offset_outside_directory_range();
    check_compressed_resource_marker();
    check_declared_length_limit();
    check_maximum_resource_length();
    check_valid_resources_at_nonzero_offsets();
    check_logic_message_table_bounds();
    check_unterminated_logic_message();
    check_bad_logic_message_offset();
    check_logic_message_size_mismatch();
    check_valid_resources_and_retry();
    check_startup_directory_transaction();

    puts("AGI loader checks pass: directory bounds, volume integrity, rollback, and retry");
    cleanup();
    return 0;
}

/* Let the real production seek code build its DOS command, then model KERNAL open. */
typedef struct {
    char command;
    uint8_t lfn;
    uint32_t offset;
} HostSeekCommand;

static unsigned char host_k_open_at_offset(const void *encoded_command)
{
    HostSeekCommand command;

    memcpy(&command, encoded_command, sizeof(command));
    CHECK(command.command == 'p' && command.lfn == SEQUENTIAL_LFN,
          "Production seek did not build a valid CX16 DOS P command");
    if (seekResult)
        return seekResult;
    CHECK(resourceVolume, "Production seek issued without an open volume fixture");
    return fseek(resourceVolume, (long)command.offset, SEEK_SET) ? 1 : 0;
}

#define cbm_k_open() host_k_open_at_offset(&cmd)
/* Compile and exercise the production loader with its RAM work area mapped. */
#include "../src/agifiles.c"
#undef cbm_k_open
