#define _POSIX_C_SOURCE 200809L

#include "logic_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANIFEST_MAGIC "CX16LRM1"
#define MANIFEST_PAYLOAD_MAX ( \
    (2 + LOGIC_REPLAY_ENGINE_MAX - 1) + \
    (2 + LOGIC_REPLAY_BUILD_MAX - 1) + \
    (2 + LOGIC_REPLAY_AGI_VERSION_MAX - 1) + 2 + \
    LOGIC_REPLAY_MAX_RESOURCES * LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE + \
    2 + LOGIC_REPLAY_CHECKPOINT_MAX + \
    (2 + LOGIC_REPLAY_RNG_ALGORITHM_MAX - 1) + 8 + 4 + 4)

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0;
    while (length < capacity && text[length])
        ++length;
    return length;
}

static uint16_t get_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t get_u64(const uint8_t *bytes)
{
    return (uint64_t)get_u32(bytes) | ((uint64_t)get_u32(bytes + 4) << 32);
}

static void put_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void put_u64(uint8_t *bytes, uint64_t value)
{
    put_u32(bytes, (uint32_t)value);
    put_u32(bytes + 4, (uint32_t)(value >> 32));
}

static int put_bytes(uint8_t **cursor, const uint8_t *end,
                     const void *bytes, size_t length)
{
    if ((size_t)(end - *cursor) < length)
        return -1;
    if (length)
        memcpy(*cursor, bytes, length);
    *cursor += length;
    return 0;
}

static int put_string(uint8_t **cursor, const uint8_t *end,
                      const char *text, size_t capacity)
{
    size_t length = bounded_length(text, capacity);
    uint8_t encoded_length[2];
    if (length == capacity || length > UINT16_MAX)
        return -1;
    put_u16(encoded_length, (uint16_t)length);
    return put_bytes(cursor, end, encoded_length, sizeof encoded_length) == 0 &&
           put_bytes(cursor, end, (const uint8_t *)text, length) == 0 ? 0 : -1;
}

static int get_bytes(const uint8_t **cursor, const uint8_t *end,
                     void *output, size_t length)
{
    if ((size_t)(end - *cursor) < length)
        return -1;
    if (length)
        memcpy(output, *cursor, length);
    *cursor += length;
    return 0;
}

static int get_string(const uint8_t **cursor, const uint8_t *end,
                      char *output, size_t capacity)
{
    uint8_t encoded_length[2];
    size_t length;
    if (get_bytes(cursor, end, encoded_length, sizeof encoded_length) != 0)
        return -1;
    length = get_u16(encoded_length);
    if (!length || length >= capacity || (size_t)(end - *cursor) < length ||
        memchr(*cursor, '\0', length) != NULL)
        return -1;
    memcpy(output, *cursor, length);
    output[length] = '\0';
    *cursor += length;
    return 0;
}

static int valid_resource_name(const char *name)
{
    size_t length = bounded_length(name, LOGIC_REPLAY_RESOURCE_NAME_MAX);
    size_t start = 0, i;
    if (!length || length == LOGIC_REPLAY_RESOURCE_NAME_MAX || name[0] == '/' ||
        strchr(name, '\\'))
        return 0;
    for (i = 0; i <= length; ++i) {
        if (name[i] == '/' || name[i] == '\0') {
            size_t part_length = i - start;
            if (!part_length ||
                (part_length == 1 && name[start] == '.') ||
                (part_length == 2 && name[start] == '.' && name[start + 1] == '.'))
                return 0;
            start = i + 1;
        }
    }
    return 1;
}

int logic_replay_manifest_validate(const LogicReplayManifest *manifest)
{
    size_t i, j;
    if (!manifest || manifest->version != LOGIC_REPLAY_MANIFEST_VERSION ||
        !bounded_length(manifest->engine, sizeof manifest->engine) ||
        bounded_length(manifest->engine, sizeof manifest->engine) == sizeof manifest->engine ||
        !bounded_length(manifest->build, sizeof manifest->build) ||
        bounded_length(manifest->build, sizeof manifest->build) == sizeof manifest->build ||
        !bounded_length(manifest->agi_version, sizeof manifest->agi_version) ||
        bounded_length(manifest->agi_version, sizeof manifest->agi_version) == sizeof manifest->agi_version ||
        !bounded_length(manifest->rng_algorithm, sizeof manifest->rng_algorithm) ||
        bounded_length(manifest->rng_algorithm, sizeof manifest->rng_algorithm) == sizeof manifest->rng_algorithm ||
        !manifest->resource_count ||
        manifest->resource_count > LOGIC_REPLAY_MAX_RESOURCES ||
        manifest->checkpoint_length > LOGIC_REPLAY_CHECKPOINT_MAX ||
        !manifest->tick_numerator || !manifest->tick_denominator)
        return -1;
    for (i = 0; i < manifest->resource_count; ++i) {
        if (!valid_resource_name(manifest->resources[i].name))
            return -1;
        for (j = 0; j < i; ++j)
            if (!strcmp(manifest->resources[i].name,
                        manifest->resources[j].name))
                return -1;
    }
    return 0;
}

static int write_header(FILE *file, const char magic[8], uint16_t version,
                        uint16_t record_length, uint32_t payload_length)
{
    uint8_t header[LOGIC_REPLAY_MANIFEST_HEADER_SIZE] = {0};
    memcpy(header, magic, 8);
    put_u16(header + 8, version);
    put_u16(header + 10, LOGIC_REPLAY_MANIFEST_HEADER_SIZE);
    put_u16(header + 12, record_length);
    put_u16(header + 14, 0);
    put_u32(header + 16, payload_length);
    return fwrite(header, 1, sizeof header, file) == sizeof header ? 0 : -1;
}

static int read_header(FILE *file, const char expected_magic[8],
                       uint16_t expected_version,
                       uint16_t expected_record_length,
                       uint32_t *payload_length)
{
    uint8_t header[LOGIC_REPLAY_MANIFEST_HEADER_SIZE];
    if (fread(header, 1, sizeof header, file) != sizeof header ||
        memcmp(header, expected_magic, 8) != 0 ||
        get_u16(header + 8) != expected_version ||
        get_u16(header + 10) != LOGIC_REPLAY_MANIFEST_HEADER_SIZE ||
        get_u16(header + 12) != expected_record_length ||
        get_u16(header + 14) != 0)
        return -1;
    *payload_length = get_u32(header + 16);
    return 0;
}

static int file_at_end(FILE *file)
{
    int value = fgetc(file);
    return value == EOF && !ferror(file);
}

int logic_replay_manifest_write(const char *path,
                                const LogicReplayManifest *manifest)
{
    uint8_t payload[MANIFEST_PAYLOAD_MAX];
    uint8_t *cursor = payload;
    const uint8_t *end = payload + sizeof payload;
    uint8_t encoded[8];
    uint16_t i;
    FILE *file;
    int result = -1;
    if (!path || logic_replay_manifest_validate(manifest) != 0)
        return -1;
    if (put_string(&cursor, end, manifest->engine, sizeof manifest->engine) != 0 ||
        put_string(&cursor, end, manifest->build, sizeof manifest->build) != 0 ||
        put_string(&cursor, end, manifest->agi_version,
                   sizeof manifest->agi_version) != 0)
        return -1;
    put_u16(encoded, manifest->resource_count);
    if (put_bytes(&cursor, end, encoded, 2) != 0)
        return -1;
    for (i = 0; i < manifest->resource_count; ++i) {
        uint8_t record[LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE] = {0};
        size_t name_length = strlen(manifest->resources[i].name);
        put_u16(record, (uint16_t)name_length);
        memcpy(record + 2, manifest->resources[i].name, name_length);
        memcpy(record + 2 + LOGIC_REPLAY_RESOURCE_NAME_MAX,
               manifest->resources[i].sha256, LOGIC_REPLAY_SHA256_SIZE);
        if (put_bytes(&cursor, end, record, sizeof record) != 0)
            return -1;
    }
    put_u16(encoded, manifest->checkpoint_length);
    if (put_bytes(&cursor, end, encoded, 2) != 0 ||
        put_bytes(&cursor, end, manifest->initial_checkpoint,
                  manifest->checkpoint_length) != 0 ||
        put_string(&cursor, end, manifest->rng_algorithm,
                   sizeof manifest->rng_algorithm) != 0)
        return -1;
    put_u64(encoded, manifest->rng_seed);
    if (put_bytes(&cursor, end, encoded, 8) != 0)
        return -1;
    put_u32(encoded, manifest->tick_numerator);
    put_u32(encoded + 4, manifest->tick_denominator);
    if (put_bytes(&cursor, end, encoded, 8) != 0 ||
        (size_t)(cursor - payload) > UINT32_MAX)
        return -1;

    file = fopen(path, "wb");
    if (!file)
        return -1;
    if (write_header(file, MANIFEST_MAGIC, LOGIC_REPLAY_MANIFEST_VERSION,
                     LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE,
                     (uint32_t)(cursor - payload)) == 0 &&
        fwrite(payload, 1, (size_t)(cursor - payload), file) ==
            (size_t)(cursor - payload))
        result = 0;
    if (fclose(file) != 0)
        result = -1;
    if (result != 0)
        remove(path);
    return result;
}

int logic_replay_manifest_read(const char *path,
                               LogicReplayManifest *manifest)
{
    uint8_t payload[MANIFEST_PAYLOAD_MAX];
    uint8_t encoded[8];
    const uint8_t *cursor, *end;
    uint32_t payload_length;
    uint16_t resource_count, checkpoint_length, i;
    FILE *file;
    int result = -1;
    if (!path || !manifest)
        return -1;
    memset(manifest, 0, sizeof *manifest);
    file = fopen(path, "rb");
    if (!file)
        return -1;
    if (read_header(file, MANIFEST_MAGIC, LOGIC_REPLAY_MANIFEST_VERSION,
                    LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE,
                    &payload_length) != 0 ||
        payload_length > sizeof payload ||
        fread(payload, 1, payload_length, file) != payload_length ||
        !file_at_end(file))
        goto done;
    cursor = payload;
    end = payload + payload_length;
    manifest->version = LOGIC_REPLAY_MANIFEST_VERSION;
    if (get_string(&cursor, end, manifest->engine,
                   sizeof manifest->engine) != 0 ||
        get_string(&cursor, end, manifest->build,
                   sizeof manifest->build) != 0 ||
        get_string(&cursor, end, manifest->agi_version,
                   sizeof manifest->agi_version) != 0 ||
        get_bytes(&cursor, end, encoded, 2) != 0)
        goto done;
    resource_count = get_u16(encoded);
    if (!resource_count || resource_count > LOGIC_REPLAY_MAX_RESOURCES)
        goto done;
    manifest->resource_count = resource_count;
    for (i = 0; i < resource_count; ++i) {
        uint8_t record[LOGIC_REPLAY_MANIFEST_RESOURCE_RECORD_SIZE];
        uint16_t name_length;
        if (get_bytes(&cursor, end, record, sizeof record) != 0)
            goto done;
        name_length = get_u16(record);
        if (!name_length || name_length >= LOGIC_REPLAY_RESOURCE_NAME_MAX ||
            memchr(record + 2, '\0', name_length) != NULL)
            goto done;
        for (size_t pad = name_length;
             pad < LOGIC_REPLAY_RESOURCE_NAME_MAX; ++pad)
            if (record[2 + pad] != 0)
                goto done;
        memcpy(manifest->resources[i].name, record + 2, name_length);
        manifest->resources[i].name[name_length] = '\0';
        memcpy(manifest->resources[i].sha256,
               record + 2 + LOGIC_REPLAY_RESOURCE_NAME_MAX,
               LOGIC_REPLAY_SHA256_SIZE);
    }
    if (get_bytes(&cursor, end, encoded, 2) != 0)
        goto done;
    checkpoint_length = get_u16(encoded);
    if (checkpoint_length > LOGIC_REPLAY_CHECKPOINT_MAX ||
        get_bytes(&cursor, end, manifest->initial_checkpoint,
                  checkpoint_length) != 0 ||
        get_string(&cursor, end, manifest->rng_algorithm,
                   sizeof manifest->rng_algorithm) != 0 ||
        get_bytes(&cursor, end, encoded, 8) != 0)
        goto done;
    manifest->checkpoint_length = checkpoint_length;
    manifest->rng_seed = get_u64(encoded);
    if (get_bytes(&cursor, end, encoded, 8) != 0 || cursor != end)
        goto done;
    manifest->tick_numerator = get_u32(encoded);
    manifest->tick_denominator = get_u32(encoded + 4);
    if (logic_replay_manifest_validate(manifest) != 0)
        goto done;
    result = 0;

done:
    if (fclose(file) != 0)
        result = -1;
    if (result != 0)
        memset(manifest, 0, sizeof *manifest);
    return result;
}

int logic_replay_manifest_matches(const LogicReplayManifest *expected,
                                  const LogicReplayManifest *actual)
{
    uint16_t i;
    if (logic_replay_manifest_validate(expected) != 0 ||
        logic_replay_manifest_validate(actual) != 0 ||
        strcmp(expected->engine, actual->engine) != 0 ||
        strcmp(expected->build, actual->build) != 0 ||
        strcmp(expected->agi_version, actual->agi_version) != 0 ||
        expected->resource_count != actual->resource_count ||
        expected->checkpoint_length != actual->checkpoint_length ||
        memcmp(expected->initial_checkpoint, actual->initial_checkpoint,
               expected->checkpoint_length) != 0 ||
        expected->rng_seed != actual->rng_seed ||
        strcmp(expected->rng_algorithm, actual->rng_algorithm) != 0 ||
        expected->tick_numerator != actual->tick_numerator ||
        expected->tick_denominator != actual->tick_denominator)
        return 0;
    for (i = 0; i < expected->resource_count; ++i) {
        uint16_t j;
        for (j = 0; j < actual->resource_count; ++j)
            if (!strcmp(expected->resources[i].name, actual->resources[j].name))
                break;
        if (j == actual->resource_count ||
            memcmp(expected->resources[i].sha256, actual->resources[j].sha256,
                   LOGIC_REPLAY_SHA256_SIZE) != 0)
            return 0;
    }
    return 1;
}

