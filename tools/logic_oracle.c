#define _POSIX_C_SOURCE 200809L
#include "logic_oracle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_u16(FILE *file, uint16_t value)
{
    unsigned char bytes[2] = {(unsigned char)value, (unsigned char)(value >> 8)};
    return fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes ? 0 : -1;
}

static int write_u32(FILE *file, uint32_t value)
{
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24)
    };
    return fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes ? 0 : -1;
}

static int write_u64(FILE *file, uint64_t value)
{
    return write_u32(file, (uint32_t)value) == 0 &&
           write_u32(file, (uint32_t)(value >> 32)) == 0 ? 0 : -1;
}

static int read_u16(FILE *file, uint16_t *value)
{
    unsigned char bytes[2];
    if (fread(bytes, 1, sizeof bytes, file) != sizeof bytes) return -1;
    *value = (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
    return 0;
}

static int read_u32(FILE *file, uint32_t *value)
{
    unsigned char bytes[4];
    if (fread(bytes, 1, sizeof bytes, file) != sizeof bytes) return -1;
    *value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
             ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return 0;
}

static int read_u64(FILE *file, uint64_t *value)
{
    uint32_t low, high;
    if (read_u32(file, &low) != 0 || read_u32(file, &high) != 0)
        return -1;
    *value = (uint64_t)low | ((uint64_t)high << 32);
    return 0;
}

static int write_header(FILE *file, const char magic[4], uint16_t version,
                        uint32_t record_size, size_t count)
{
    if (count > UINT32_MAX || fwrite(magic, 1, 4, file) != 4 ||
        write_u16(file, version) != 0 ||
        write_u32(file, record_size) != 0 ||
        write_u32(file, (uint32_t)count) != 0)
        return -1;
    return 0;
}

static int read_header(FILE *file, const char magic[4], uint16_t version,
                       uint32_t record_size, uint32_t *count)
{
    unsigned char bytes[4];
    uint16_t file_version;
    uint32_t actual_size;
    long data_offset, file_length;
    uint64_t expected_length;
    if (fread(bytes, 1, sizeof bytes, file) != sizeof bytes ||
        memcmp(bytes, magic, sizeof bytes) != 0 ||
        read_u16(file, &file_version) != 0 || read_u32(file, &actual_size) != 0 ||
        read_u32(file, count) != 0 || file_version != version ||
        actual_size != record_size)
        return -1;
    data_offset = ftell(file);
    if (data_offset < 0 || fseek(file, 0, SEEK_END) != 0 ||
        (file_length = ftell(file)) < 0 || fseek(file, data_offset, SEEK_SET) != 0)
        return -1;
    expected_length = (uint64_t)LOGIC_ORACLE_HEADER_SIZE +
                      (uint64_t)*count * (uint64_t)record_size;
    return (uint64_t)file_length == expected_length ? 0 : -1;
}

static int file_finished(FILE *file)
{
    int next = fgetc(file);
    return next == EOF && !ferror(file) ? 0 : -1;
}

static int string_valid(const char *value, size_t size)
{
    const char *end = (const char *)memchr(value, '\0', size);
    return end && end != value;
}

static int string_terminated(const char *value, size_t size)
{
    return memchr(value, '\0', size) != NULL;
}

static int string_write(FILE *file, const char *value, size_t size,
                        int allow_empty)
{
    unsigned char bytes[LOGIC_ORACLE_MESSAGE_TEXT_SIZE];
    size_t length;
    if (size > sizeof bytes || !string_terminated(value, size) ||
        (!allow_empty && !string_valid(value, size)))
        return -1;
    length = strlen(value);
    memset(bytes, 0, size);
    memcpy(bytes, value, length);
    return fwrite(bytes, 1, size, file) == size ? 0 : -1;
}

static int string_read(FILE *file, char *value, size_t size, int allow_empty)
{
    unsigned char bytes[LOGIC_ORACLE_MESSAGE_TEXT_SIZE];
    unsigned char *end;
    size_t i;
    if (size > sizeof bytes || fread(bytes, 1, size, file) != size) return -1;
    end = (unsigned char *)memchr(bytes, '\0', size);
    if (!end || (!allow_empty && end == bytes)) return -1;
    for (i = (size_t)(end - bytes) + 1; i < size; i++)
        if (bytes[i] != 0) return -1;
    memcpy(value, bytes, size);
    return 0;
}

static int object_valid(const LogicOracleSnapshot *snapshot, char *detail,
                        size_t detail_size)
{
    unsigned char seen[256] = {0};
    size_t i;
    if (snapshot->object_count > LOGIC_ORACLE_MAX_OBJECTS) {
        snprintf(detail, detail_size, "too many objects");
        return 0;
    }
    for (i = 0; i < snapshot->object_count; i++) {
        if (seen[snapshot->objects[i].id]) {
            snprintf(detail, detail_size, "duplicate object id %u",
                     (unsigned)snapshot->objects[i].id);
            return 0;
        }
        seen[snapshot->objects[i].id] = 1;
    }
    return 1;
}

static int message_valid(const LogicOracleSnapshot *snapshot, char *detail,
                         size_t detail_size)
{
    size_t i;
    if (snapshot->message_count > LOGIC_ORACLE_MAX_MESSAGES) {
        snprintf(detail, detail_size, "too many messages");
        return 0;
    }
    for (i = 0; i < snapshot->message_count; i++) {
        const LogicOracleMessage *message = &snapshot->messages[i];
        if (!string_terminated(message->text, sizeof message->text)) {
            snprintf(detail, detail_size, "message %u:%u has invalid text",
                     (unsigned)message->logic, (unsigned)message->id);
            return 0;
        }
    }
    return 1;
}

static int snapshot_valid(const LogicOracleSnapshot *snapshot, char *detail,
                          size_t detail_size)
{
    size_t i;
    if (!snapshot) {
        snprintf(detail, detail_size, "missing snapshot");
        return 0;
    }
    if (!string_valid(snapshot->game_id, sizeof snapshot->game_id) ||
        !string_valid(snapshot->checkpoint, sizeof snapshot->checkpoint) ||
        !string_valid(snapshot->provenance, sizeof snapshot->provenance) ||
        !string_valid(snapshot->rng_algorithm,
                      sizeof snapshot->rng_algorithm)) {
        snprintf(detail, detail_size,
                 "missing game, checkpoint, provenance, or RNG identity");
        return 0;
    }
    if (snapshot->has_trace > 1 || (!snapshot->has_trace && snapshot->trace_sequence) ||
        !snapshot->agi_version || snapshot->timing_profile < LOGIC_ORACLE_TIMING_AGI_TICK ||
        snapshot->timing_profile > LOGIC_ORACLE_TIMING_WALL_MILLISECONDS ||
        snapshot->control_flow_reliable > 1 || snapshot->has_failure > 1 ||
        !snapshot->tick_numerator ||
        !snapshot->tick_denominator) {
        snprintf(detail, detail_size, "invalid AGI version, timing identity, or reliability flag");
        return 0;
    }
    if (snapshot->inventory_known > 1 || snapshot->inventory_count > 256 ||
        (!snapshot->inventory_known && snapshot->inventory_count)) {
        snprintf(detail, detail_size, "invalid inventory availability or count");
        return 0;
    }
    for (i = snapshot->inventory_count; i < 256; ++i) {
        if (snapshot->inventory_rooms[i]) {
            snprintf(detail, detail_size, "nonzero unused inventory location");
            return 0;
        }
    }
    for (i = 0; i < sizeof snapshot->flags; i++) {
        if (snapshot->flags[i] > 1) {
            snprintf(detail, detail_size, "flag %u is not boolean", (unsigned)i);
            return 0;
        }
    }
    if ((snapshot->has_failure && !snapshot->failure_kind) ||
        (!snapshot->has_failure &&
         (snapshot->failure_kind || snapshot->failure_logic ||
          snapshot->failure_logical_pc || snapshot->failure_opcode ||
          snapshot->failure_reason || snapshot->failure_code))) {
        snprintf(detail, detail_size, "invalid optional failure signature");
        return 0;
    }
    return object_valid(snapshot, detail, detail_size) &&
           message_valid(snapshot, detail, detail_size);
}

static int snapshot_identity_equal(const LogicOracleSnapshot *a,
                                   const LogicOracleSnapshot *b)
{
    return strcmp(a->game_id, b->game_id) == 0 &&
           a->agi_version == b->agi_version &&
           a->timing_profile == b->timing_profile &&
           memcmp(a->manifest_sha256, b->manifest_sha256,
                  sizeof a->manifest_sha256) == 0 &&
           memcmp(a->input_sha256, b->input_sha256,
                  sizeof a->input_sha256) == 0 &&
           a->rng_seed == b->rng_seed &&
           strcmp(a->rng_algorithm, b->rng_algorithm) == 0 &&
           a->tick_numerator == b->tick_numerator &&
           a->tick_denominator == b->tick_denominator;
}

static int snapshot_stream_valid(const LogicOracleSnapshot *snapshots,
                                 size_t count, char *detail,
                                 size_t detail_size)
{
    size_t i;
    uint32_t previous_trace = 0;
    int have_previous_trace = 0;
    if (!snapshots || !count) {
        snprintf(detail, detail_size, "snapshot stream is empty");
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (!snapshot_valid(&snapshots[i], detail, detail_size)) return 0;
        if (snapshots[i].checkpoint_index != i) {
            snprintf(detail, detail_size,
                     "checkpoint indices are out of order at record %lu",
                     (unsigned long)i);
            return 0;
        }
        if (i && !snapshot_identity_equal(&snapshots[0], &snapshots[i])) {
            snprintf(detail, detail_size,
                     "replay identity changes at record %lu", (unsigned long)i);
            return 0;
        }
        if (snapshots[i].has_trace && have_previous_trace &&
            snapshots[i].trace_sequence < previous_trace) {
            snprintf(detail, detail_size,
                     "trace references are out of order at record %lu",
                     (unsigned long)i);
            return 0;
        }
        if (snapshots[i].has_trace) {
            previous_trace = snapshots[i].trace_sequence;
            have_previous_trace = 1;
        }
    }
    return 1;
}

static int write_object(FILE *file, const LogicOracleObject *object)
{
    return fputc(object->id, file) == EOF ||
           write_u16(file, (uint16_t)object->x) != 0 ||
           write_u16(file, (uint16_t)object->y) != 0 ||
           fputc(object->view, file) == EOF || fputc(object->loop, file) == EOF ||
           fputc(object->cel, file) == EOF || fputc(object->motion, file) == EOF
        ? -1 : 0;
}

static int read_object(FILE *file, LogicOracleObject *object)
{
    uint16_t x, y;
    int byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    object->id = (uint8_t)byte;
    if (read_u16(file, &x) != 0 || read_u16(file, &y) != 0) return -1;
    object->x = (int16_t)(x < 0x8000 ? (int32_t)x : (int32_t)x - 0x10000);
    object->y = (int16_t)(y < 0x8000 ? (int32_t)y : (int32_t)y - 0x10000);
    if ((byte = fgetc(file)) == EOF) return -1;
    object->view = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    object->loop = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    object->cel = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    object->motion = (uint8_t)byte;
    return 0;
}

static int object_zero(const LogicOracleObject *object)
{
    return object->id == 0 && object->x == 0 && object->y == 0 &&
           object->view == 0 && object->loop == 0 && object->cel == 0 &&
           object->motion == 0;
}

static int write_snapshot(FILE *file, const LogicOracleSnapshot *snapshot)
{
    LogicOracleObject empty_object = {0, 0, 0, 0, 0, 0, 0};
    LogicOracleMessage empty_message = {0};
    size_t i;
    if (write_u32(file, snapshot->checkpoint_index) != 0 ||
        write_u32(file, snapshot->tick) != 0 ||
        write_u32(file, snapshot->trace_sequence) != 0 ||
        fputc(snapshot->has_trace, file) == EOF ||
        write_u16(file, snapshot->agi_version) != 0 ||
        fputc(snapshot->timing_profile, file) == EOF ||
        fputc(snapshot->control_flow_reliable, file) == EOF ||
        string_write(file, snapshot->game_id, sizeof snapshot->game_id, 0) != 0 ||
        string_write(file, snapshot->checkpoint, sizeof snapshot->checkpoint, 0) != 0 ||
        string_write(file, snapshot->provenance, sizeof snapshot->provenance, 0) != 0 ||
        fwrite(snapshot->manifest_sha256, 1, sizeof snapshot->manifest_sha256, file) != sizeof snapshot->manifest_sha256 ||
        fwrite(snapshot->input_sha256, 1, sizeof snapshot->input_sha256, file) != sizeof snapshot->input_sha256 ||
        write_u64(file, snapshot->rng_seed) != 0 ||
        string_write(file, snapshot->rng_algorithm, sizeof snapshot->rng_algorithm, 0) != 0 ||
        write_u32(file, snapshot->tick_numerator) != 0 ||
        write_u32(file, snapshot->tick_denominator) != 0 ||
        write_u32(file, snapshot->rng_call_count) != 0 ||
        write_u32(file, snapshot->rng_final_state) != 0 ||
        fwrite(snapshot->variables, 1, sizeof snapshot->variables, file) != sizeof snapshot->variables ||
        fwrite(snapshot->flags, 1, sizeof snapshot->flags, file) != sizeof snapshot->flags ||
        write_u16(file, snapshot->room) != 0 ||
        write_u16(file, snapshot->object_count) != 0)
        return -1;
    for (i = 0; i < LOGIC_ORACLE_MAX_OBJECTS; i++)
        if (write_object(file, i < snapshot->object_count ? &snapshot->objects[i] : &empty_object) != 0)
            return -1;
    if (write_u16(file, snapshot->message_count) != 0) return -1;
    for (i = 0; i < LOGIC_ORACLE_MAX_MESSAGES; i++) {
        const LogicOracleMessage *message = i < snapshot->message_count
            ? &snapshot->messages[i] : &empty_message;
        if (fputc(message->logic, file) == EOF || fputc(message->id, file) == EOF ||
            string_write(file, message->text, sizeof message->text, 1) != 0)
            return -1;
    }
    if (fputc(snapshot->logic, file) == EOF ||
        write_u16(file, snapshot->logical_pc) != 0 ||
        fputc(snapshot->has_failure, file) == EOF ||
        fputc(snapshot->failure_kind, file) == EOF ||
        fputc(snapshot->failure_logic, file) == EOF ||
        write_u16(file, snapshot->failure_logical_pc) != 0 ||
        fputc(snapshot->failure_opcode, file) == EOF ||
        fputc(snapshot->failure_reason, file) == EOF ||
        write_u32(file, snapshot->failure_code) != 0 ||
        fputc(snapshot->inventory_known, file) == EOF ||
        write_u16(file, snapshot->inventory_count) != 0 ||
        fwrite(snapshot->inventory_rooms, 1, 256, file) != 256)
        return -1;
    return 0;
}

static int message_zero(const LogicOracleMessage *message)
{
    return message->logic == 0 && message->id == 0 && message->text[0] == '\0';
}

