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

/* Compact SHA-256 implementation keeps resource verification dependency-free. */
typedef struct Sha256State {
    uint32_t words[8];
    uint64_t bit_length;
    uint8_t block[64];
    size_t block_length;
} Sha256State;

static uint32_t rotate_right(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32 - count));
}

static void sha256_transform(Sha256State *state, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t schedule[64], a,b,c,d,e,f,g,h;
    unsigned i;
    for (i = 0; i < 16; ++i)
        schedule[i] = ((uint32_t)block[i * 4] << 24) |
                      ((uint32_t)block[i * 4 + 1] << 16) |
                      ((uint32_t)block[i * 4 + 2] << 8) |
                      (uint32_t)block[i * 4 + 3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(schedule[i - 15], 7) ^
                      rotate_right(schedule[i - 15], 18) ^
                      (schedule[i - 15] >> 3);
        uint32_t s1 = rotate_right(schedule[i - 2], 17) ^
                      rotate_right(schedule[i - 2], 19) ^
                      (schedule[i - 2] >> 10);
        schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
    }
    a = state->words[0]; b = state->words[1]; c = state->words[2]; d = state->words[3];
    e = state->words[4]; f = state->words[5]; g = state->words[6]; h = state->words[7];
    for (i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t first = h + s1 + choose + constants[i] + schedule[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t second = s0 + majority;
        h = g; g = f; f = e; e = d + first;
        d = c; c = b; b = a; a = first + second;
    }
    state->words[0] += a; state->words[1] += b; state->words[2] += c; state->words[3] += d;
    state->words[4] += e; state->words[5] += f; state->words[6] += g; state->words[7] += h;
}

static void sha256_init(Sha256State *state)
{
    static const uint32_t initial[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(state->words, initial, sizeof initial);
    state->bit_length = 0;
    state->block_length = 0;
}

static int sha256_update(Sha256State *state, const uint8_t *bytes, size_t length)
{
    size_t offset = 0;
    if ((uint64_t)length > (UINT64_MAX - state->bit_length) / 8)
        return -1;
    state->bit_length += (uint64_t)length * 8;
    while (offset < length) {
        size_t available = sizeof state->block - state->block_length;
        size_t copy_length = length - offset < available ? length - offset : available;
        memcpy(state->block + state->block_length, bytes + offset, copy_length);
        state->block_length += copy_length;
        offset += copy_length;
        if (state->block_length == sizeof state->block) {
            sha256_transform(state, state->block);
            state->block_length = 0;
        }
    }
    return 0;
}

static void sha256_finish(Sha256State *state, uint8_t digest[32])
{
    uint8_t length_bytes[8];
    unsigned i;
    uint64_t length = state->bit_length;
    state->block[state->block_length++] = 0x80;
    if (state->block_length > 56) {
        memset(state->block + state->block_length, 0,
               sizeof state->block - state->block_length);
        sha256_transform(state, state->block);
        state->block_length = 0;
    }
    memset(state->block + state->block_length, 0, 56 - state->block_length);
    for (i = 0; i < 8; ++i)
        length_bytes[7 - i] = (uint8_t)(length >> (i * 8));
    memcpy(state->block + 56, length_bytes, sizeof length_bytes);
    sha256_transform(state, state->block);
    for (i = 0; i < 8; ++i) {
        digest[i * 4] = (uint8_t)(state->words[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(state->words[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(state->words[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)state->words[i];
    }
}

int logic_replay_sha256_file(const char *path,
                             uint8_t digest[LOGIC_REPLAY_SHA256_SIZE])
{
    uint8_t buffer[8192];
    Sha256State state;
    FILE *file;
    int result = -1;
    if (!path || !digest)
        return -1;
    file = fopen(path, "rb");
    if (!file)
        return -1;
    sha256_init(&state);
    for (;;) {
        size_t count = fread(buffer, 1, sizeof buffer, file);
        if (count && sha256_update(&state, buffer, count) != 0)
            goto done;
        if (count < sizeof buffer) {
            if (ferror(file))
                goto done;
            break;
        }
    }
    sha256_finish(&state, digest);
    result = 0;

done:
    if (fclose(file) != 0)
        result = -1;
    return result;
}

int logic_replay_manifest_verify_resources(const LogicReplayManifest *manifest,
                                           const char *resource_directory)
{
    size_t directory_length;
    uint16_t i;
    if (logic_replay_manifest_validate(manifest) != 0 ||
        !resource_directory || !resource_directory[0])
        return -1;
    directory_length = strlen(resource_directory);
    for (i = 0; i < manifest->resource_count; ++i) {
        size_t name_length = strlen(manifest->resources[i].name);
        int separator = directory_length && resource_directory[directory_length - 1] != '/';
        char *path;
        uint8_t digest[LOGIC_REPLAY_SHA256_SIZE];
        if (directory_length > SIZE_MAX - name_length - (size_t)separator - 1)
            return -1;
        path = (char *)malloc(directory_length + name_length + (size_t)separator + 1);
        if (!path)
            return -1;
        memcpy(path, resource_directory, directory_length);
        if (separator)
            path[directory_length] = '/';
        memcpy(path + directory_length + (size_t)separator,
               manifest->resources[i].name, name_length + 1);
        if (logic_replay_sha256_file(path, digest) != 0 ||
            memcmp(digest, manifest->resources[i].sha256, sizeof digest) != 0) {
            free(path);
            return -1;
        }
        free(path);
    }
    return 0;
}

int logic_replay_trace_validate(const LogicReplayEvent *events, size_t count)
{
    uint8_t active[256 * 256] = {0};
    LogicTraceCallFrame *call_stack = NULL;
    uint32_t previous_tick = 0;
    size_t i;
    if ((!events && count) || count > UINT32_MAX ||
        count > SIZE_MAX / sizeof *call_stack)
        return -1;
    if (count) {
        call_stack = (LogicTraceCallFrame *)malloc(count * sizeof *call_stack);
        if (!call_stack)
            return -1;
    }
    if (logic_trace_validate(events, count, call_stack, count, NULL) != 0) {
        free(call_stack);
        return -1;
    }
    for (i = 0; i < count; ++i) {
        const LogicReplayEvent *event = &events[i];
        size_t input_index = ((size_t)event->logic << 8) | event->field;
        if (i && event->tick < previous_tick) {
            free(call_stack);
            return -1;
        }
        previous_tick = event->tick;
        if (event->kind == LOGIC_TRACE_KIND_MARK &&
            event->reason == LOGIC_REPLAY_REASON_INPUT_DOWN) {
            if (event->before != 0 || event->after != 1 || active[input_index]) {
                free(call_stack);
                return -1;
            }
            active[input_index] = 1;
        } else if (event->kind == LOGIC_TRACE_KIND_MARK &&
                   event->reason == LOGIC_REPLAY_REASON_INPUT_UP) {
            if (event->before != 1 || event->after != 0 || !active[input_index]) {
                free(call_stack);
                return -1;
            }
            active[input_index] = 0;
        }
    }
    free(call_stack);
    for (i = 0; i < sizeof active; ++i)
        if (active[i])
            return -1;
    return 0;
}

int logic_replay_trace_write(const char *path,
                             const LogicReplayEvent *events, size_t count)
{
    FILE *file;
    size_t i;
    int result = -1;
    uint8_t header[LOGIC_REPLAY_TRACE_HEADER_SIZE];
    if (!path || logic_replay_trace_validate(events, count) != 0 ||
        count > UINT32_MAX ||
        logic_trace_header_encode((uint32_t)count, header, sizeof header) != 0)
        return -1;
    file = fopen(path, "wb");
    if (!file)
        return -1;
    if (fwrite(header, 1, sizeof header, file) != sizeof header)
        goto done;
    for (i = 0; i < count; ++i) {
        uint8_t record[LOGIC_REPLAY_EVENT_RECORD_SIZE];
        if (logic_trace_event_encode(&events[i], record, sizeof record) != 0 ||
            fwrite(record, 1, sizeof record, file) != sizeof record)
            goto done;
    }
    result = 0;

done:
    if (fclose(file) != 0)
        result = -1;
    if (result != 0)
        remove(path);
    return result;
}

int logic_replay_trace_read(const char *path, LogicReplayEvent **events,
                            size_t *count)
{
    uint8_t header[LOGIC_REPLAY_TRACE_HEADER_SIZE];
    uint32_t encoded_count;
    size_t event_count, i;
    LogicReplayEvent *loaded = NULL;
    FILE *file;
    int result = -1;
    if (!path || !events || !count)
        return -1;
    *events = NULL;
    *count = 0;
    file = fopen(path, "rb");
    if (!file)
        return -1;
    if (fread(header, 1, sizeof header, file) != sizeof header ||
        logic_trace_header_decode(header, sizeof header, &encoded_count) != 0)
        goto done;
    event_count = encoded_count;
    if (event_count > SIZE_MAX / sizeof *loaded)
        goto done;
    if (event_count) {
        loaded = (LogicReplayEvent *)malloc(event_count * sizeof *loaded);
        if (!loaded)
            goto done;
    }
    for (i = 0; i < event_count; ++i) {
        uint8_t record[LOGIC_REPLAY_EVENT_RECORD_SIZE];
        if (fread(record, 1, sizeof record, file) != sizeof record ||
            logic_trace_event_decode(record, sizeof record, &loaded[i]) != 0)
            goto done;
    }
    if (!file_at_end(file) || logic_replay_trace_validate(loaded, event_count) != 0)
        goto done;
    *events = loaded;
    *count = event_count;
    loaded = NULL;
    result = 0;

done:
    free(loaded);
    if (fclose(file) != 0)
        result = -1;
    if (result != 0) {
        free(*events);
        *events = NULL;
        *count = 0;
    }
    return result;
}

void logic_replay_events_free(LogicReplayEvent *events)
{
    free(events);
}
