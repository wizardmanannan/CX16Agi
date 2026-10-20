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

static int read_snapshot(FILE *file, LogicOracleSnapshot *snapshot)
{
    size_t i;
    int byte;
    memset(snapshot, 0, sizeof *snapshot);
    if (read_u32(file, &snapshot->checkpoint_index) != 0 ||
        read_u32(file, &snapshot->tick) != 0 ||
        read_u32(file, &snapshot->trace_sequence) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    snapshot->has_trace = (uint8_t)byte;
    if (read_u16(file, &snapshot->agi_version) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    snapshot->timing_profile = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    snapshot->control_flow_reliable = (uint8_t)byte;
    if (string_read(file, snapshot->game_id, sizeof snapshot->game_id, 0) != 0 ||
        string_read(file, snapshot->checkpoint, sizeof snapshot->checkpoint, 0) != 0 ||
        string_read(file, snapshot->provenance, sizeof snapshot->provenance, 0) != 0 ||
        fread(snapshot->manifest_sha256, 1, sizeof snapshot->manifest_sha256, file) != sizeof snapshot->manifest_sha256 ||
        fread(snapshot->input_sha256, 1, sizeof snapshot->input_sha256, file) != sizeof snapshot->input_sha256 ||
        read_u64(file, &snapshot->rng_seed) != 0 ||
        string_read(file, snapshot->rng_algorithm, sizeof snapshot->rng_algorithm, 0) != 0 ||
        read_u32(file, &snapshot->tick_numerator) != 0 ||
        read_u32(file, &snapshot->tick_denominator) != 0 ||
        read_u32(file, &snapshot->rng_call_count) != 0 ||
        read_u32(file, &snapshot->rng_final_state) != 0 ||
        fread(snapshot->variables, 1, sizeof snapshot->variables, file) != sizeof snapshot->variables ||
        fread(snapshot->flags, 1, sizeof snapshot->flags, file) != sizeof snapshot->flags ||
        read_u16(file, &snapshot->room) != 0 ||
        read_u16(file, &snapshot->object_count) != 0 ||
        snapshot->object_count > LOGIC_ORACLE_MAX_OBJECTS)
        return -1;
    for (i = 0; i < LOGIC_ORACLE_MAX_OBJECTS; i++) {
        LogicOracleObject object;
        if (read_object(file, &object) != 0) return -1;
        if (i < snapshot->object_count) snapshot->objects[i] = object;
        else if (!object_zero(&object)) return -1;
    }
    if (read_u16(file, &snapshot->message_count) != 0 ||
        snapshot->message_count > LOGIC_ORACLE_MAX_MESSAGES)
        return -1;
    for (i = 0; i < LOGIC_ORACLE_MAX_MESSAGES; i++) {
        LogicOracleMessage message;
        if ((byte = fgetc(file)) == EOF) return -1;
        message.logic = (uint8_t)byte;
        if ((byte = fgetc(file)) == EOF) return -1;
        message.id = (uint8_t)byte;
        if (string_read(file, message.text, sizeof message.text, 1) != 0)
            return -1;
        if (i < snapshot->message_count) snapshot->messages[i] = message;
        else if (!message_zero(&message)) return -1;
    }
    if ((byte = fgetc(file)) == EOF || read_u16(file, &snapshot->logical_pc) != 0)
        return -1;
    snapshot->logic = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF)
        return -1;
    snapshot->has_failure = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF)
        return -1;
    snapshot->failure_kind = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF)
        return -1;
    snapshot->failure_logic = (uint8_t)byte;
    if (read_u16(file, &snapshot->failure_logical_pc) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    snapshot->failure_opcode = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF)
        return -1;
    snapshot->failure_reason = (uint8_t)byte;
    if (read_u32(file, &snapshot->failure_code) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    snapshot->inventory_known = (uint8_t)byte;
    if (read_u16(file, &snapshot->inventory_count) != 0 ||
        fread(snapshot->inventory_rooms, 1, 256, file) != 256)
        return -1;
    return 0;
}

static int read_snapshot_records(FILE *file, uint32_t disk_count,
                                 LogicOracleSnapshot *snapshots,
                                 size_t capacity)
{
    uint32_t i, previous_trace = 0;
    int have_previous_trace = 0;
    char detail[128];
    if (!snapshots || disk_count == 0 || disk_count > capacity) return -1;
    for (i = 0; i < disk_count; i++) {
        if (read_snapshot(file, &snapshots[i]) != 0 ||
            snapshots[i].checkpoint_index != i ||
            (i && !snapshot_identity_equal(&snapshots[0], &snapshots[i])) ||
            (snapshots[i].has_trace && have_previous_trace &&
             snapshots[i].trace_sequence < previous_trace) ||
            !snapshot_valid(&snapshots[i], detail, sizeof detail))
            return -1;
        if (snapshots[i].has_trace) {
            previous_trace = snapshots[i].trace_sequence;
            have_previous_trace = 1;
        }
    }
    return file_finished(file);
}

static int trace_event_valid(const LogicOracleTraceEvent *event)
{
    return event != NULL;
}

static int write_trace_event(FILE *file, const LogicOracleTraceEvent *event)
{
    if (write_u32(file, event->sequence) != 0 || write_u32(file, event->tick) != 0 ||
        fputc(event->kind, file) == EOF || fputc(event->logic, file) == EOF ||
        write_u16(file, event->logical_pc) != 0 || fputc(event->opcode, file) == EOF ||
        fputc(event->depth, file) == EOF || fputc(event->bank, file) == EOF ||
        fputc(event->field, file) == EOF || write_u32(file, event->before) != 0 ||
        write_u32(file, event->after) != 0 || fputc(event->reason, file) == EOF)
        return -1;
    return 0;
}

static int read_trace_event(FILE *file, LogicOracleTraceEvent *event)
{
    int byte;
    if (read_u32(file, &event->sequence) != 0 || read_u32(file, &event->tick) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    event->kind = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    event->logic = (uint8_t)byte;
    if (read_u16(file, &event->logical_pc) != 0 || (byte = fgetc(file)) == EOF)
        return -1;
    event->opcode = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    event->depth = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    event->bank = (uint8_t)byte;
    if ((byte = fgetc(file)) == EOF) return -1;
    event->field = (uint8_t)byte;
    if (read_u32(file, &event->before) != 0 || read_u32(file, &event->after) != 0 ||
        (byte = fgetc(file)) == EOF)
        return -1;
    event->reason = (uint8_t)byte;
    return trace_event_valid(event) ? 0 : -1;
}

static int read_trace_records(FILE *file, uint32_t disk_count,
                              LogicOracleTraceEvent *events, size_t capacity)
{
    uint32_t i, previous_sequence = 0;
    if (disk_count > capacity || (disk_count && !events)) return -1;
    for (i = 0; i < disk_count; i++) {
        if (read_trace_event(file, &events[i]) != 0 ||
            (i && events[i].sequence <= previous_sequence))
            return -1;
        previous_sequence = events[i].sequence;
    }
    return file_finished(file);
}

static void result_context(LogicOracleResult *result,
                           const LogicOracleSnapshot *reference,
                           const LogicOracleSnapshot *candidate)
{
    if (reference) {
        result->checkpoint_index = reference->checkpoint_index;
        snprintf(result->checkpoint, sizeof result->checkpoint, "%s", reference->checkpoint);
        snprintf(result->reference_provenance, sizeof result->reference_provenance,
                 "%s", reference->provenance);
        result->reference_trace_sequence = reference->trace_sequence;
        result->reference_has_trace = reference->has_trace;
    }
    if (candidate) {
        if (!reference) {
            result->checkpoint_index = candidate->checkpoint_index;
            snprintf(result->checkpoint, sizeof result->checkpoint, "%s", candidate->checkpoint);
        }
        snprintf(result->candidate_provenance, sizeof result->candidate_provenance,
                 "%s", candidate->provenance);
        result->candidate_trace_sequence = candidate->trace_sequence;
        result->candidate_has_trace = candidate->has_trace;
    }
}

static LogicOracleStatus invalid_result(LogicOracleResult *result, const char *detail)
{
    result->status = LOGIC_ORACLE_INVALID;
    snprintf(result->detail, sizeof result->detail, "%s", detail);
    return result->status;
}

static LogicOracleStatus incompatible_result(LogicOracleResult *result,
                                             const LogicOracleSnapshot *reference,
                                             const LogicOracleSnapshot *candidate,
                                             const char *detail)
{
    result_context(result, reference, candidate);
    result->status = LOGIC_ORACLE_INCOMPATIBLE;
    snprintf(result->detail, sizeof result->detail, "%s", detail);
    return result->status;
}

static LogicOracleStatus diverged_value(LogicOracleResult *result,
                                       const LogicOracleSnapshot *reference,
                                       const LogicOracleSnapshot *candidate,
                                       const char *field, int64_t reference_value,
                                       int64_t candidate_value)
{
    result_context(result, reference, candidate);
    result->status = LOGIC_ORACLE_DIVERGED;
    result->reference_value = reference_value;
    result->candidate_value = candidate_value;
    snprintf(result->field, sizeof result->field, "%s", field);
    return result->status;
}

static LogicOracleStatus diverged_text(LogicOracleResult *result,
                                      const LogicOracleSnapshot *reference,
                                      const LogicOracleSnapshot *candidate,
                                      const char *field, const char *reference_text,
                                      const char *candidate_text)
{
    result_context(result, reference, candidate);
    result->status = LOGIC_ORACLE_DIVERGED;
    snprintf(result->field, sizeof result->field, "%s", field);
    snprintf(result->reference_text, sizeof result->reference_text, "%s", reference_text);
    snprintf(result->candidate_text, sizeof result->candidate_text, "%s", candidate_text);
    return result->status;
}

static const LogicOracleObject *object_by_id(const LogicOracleSnapshot *snapshot,
                                             unsigned id)
{
    size_t i;
    for (i = 0; i < snapshot->object_count; i++)
        if (snapshot->objects[i].id == id) return &snapshot->objects[i];
    return NULL;
}

LogicOracleStatus logic_oracle_compare_with_policy(
    const LogicOracleSnapshot *reference, const LogicOracleSnapshot *candidate,
    LogicOraclePolicy policy, LogicOracleResult *result)
{
    char detail[128], field[48];
    unsigned i;
    int rng_algorithm_matches;
    if (!result) return LOGIC_ORACLE_INVALID;
    memset(result, 0, sizeof *result);
    if (policy != LOGIC_ORACLE_POLICY_DETERMINISTIC &&
        policy != LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC)
        return invalid_result(result, "invalid comparison policy");
    if (!snapshot_valid(reference, detail, sizeof detail) ||
        !snapshot_valid(candidate, detail, sizeof detail))
        return invalid_result(result, detail);
    result_context(result, reference, candidate);
    if (strcmp(reference->game_id, candidate->game_id) != 0)
        return incompatible_result(result, reference, candidate, "game identifiers differ");
    if (reference->agi_version != candidate->agi_version)
        return incompatible_result(result, reference, candidate, "AGI resource versions differ");
    if (reference->timing_profile != candidate->timing_profile)
        return incompatible_result(result, reference, candidate,
            "sample boundaries differ; explicitly aligned timing profiles required");
    if (reference->tick_numerator != candidate->tick_numerator ||
        reference->tick_denominator != candidate->tick_denominator)
        return incompatible_result(result, reference, candidate, "timing identities differ");
    if (memcmp(reference->manifest_sha256, candidate->manifest_sha256,
               sizeof reference->manifest_sha256) != 0)
        return incompatible_result(result, reference, candidate,
                                   "resource manifest identities differ");
    if (memcmp(reference->input_sha256, candidate->input_sha256,
               sizeof reference->input_sha256) != 0)
        return incompatible_result(result, reference, candidate,
                                   "delivered input identities differ");
    rng_algorithm_matches =
        strcmp(reference->rng_algorithm, candidate->rng_algorithm) == 0;
    if (policy == LOGIC_ORACLE_POLICY_DETERMINISTIC &&
        (reference->rng_seed != candidate->rng_seed || !rng_algorithm_matches))
        return incompatible_result(result, reference, candidate,
                                   "RNG identities differ");
    if (policy == LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC &&
        reference->rng_seed != candidate->rng_seed)
        return incompatible_result(result, reference, candidate,
                                   "RNG seed identities differ");
    if (policy == LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC &&
        reference->tick != candidate->tick)
        return diverged_value(result, reference, candidate, "tick",
                              reference->tick, candidate->tick);
    if (reference->checkpoint_index != candidate->checkpoint_index)
        return diverged_value(result, reference, candidate, "checkpoint_index",
                              reference->checkpoint_index, candidate->checkpoint_index);
    if (strcmp(reference->checkpoint, candidate->checkpoint) != 0)
        return diverged_text(result, reference, candidate, "checkpoint",
                             reference->checkpoint, candidate->checkpoint);

    if (reference->inventory_known != candidate->inventory_known)
        return incompatible_result(result, reference, candidate,
                                   "inventory availability differs");
    if (reference->inventory_known) {
        if (reference->inventory_count != candidate->inventory_count)
            return diverged_value(result, reference, candidate, "inventory_count",
                                  reference->inventory_count, candidate->inventory_count);
        for (i = 0; i < reference->inventory_count; ++i) {
            if (reference->inventory_rooms[i] != candidate->inventory_rooms[i]) {
                snprintf(field, sizeof field, "inventory[%u].room", i);
                return diverged_value(result, reference, candidate, field,
                    reference->inventory_rooms[i], candidate->inventory_rooms[i]);
            }
        }
    }

    for (i = 0; i < 256; i++) {
        if (reference->variables[i] != candidate->variables[i]) {
            snprintf(field, sizeof field, "variable[%u]", i);
            return diverged_value(result, reference, candidate, field,
                                  reference->variables[i], candidate->variables[i]);
        }
    }
    for (i = 0; i < 256; i++) {
        if (reference->flags[i] != candidate->flags[i]) {
            snprintf(field, sizeof field, "flag[%u]", i);
            return diverged_value(result, reference, candidate, field,
                                  reference->flags[i], candidate->flags[i]);
        }
    }
    if (reference->room != candidate->room)
        return diverged_value(result, reference, candidate, "room",
                              reference->room, candidate->room);
    if (rng_algorithm_matches) {
        if (reference->rng_call_count != candidate->rng_call_count)
            return diverged_value(result, reference, candidate, "rng_call_count",
                                  reference->rng_call_count,
                                  candidate->rng_call_count);
        if (reference->rng_final_state != candidate->rng_final_state)
            return diverged_value(result, reference, candidate, "rng_final_state",
                                  reference->rng_final_state,
                                  candidate->rng_final_state);
    }

    for (i = 0; i < 256; i++) {
        const LogicOracleObject *a = object_by_id(reference, i);
        const LogicOracleObject *b = object_by_id(candidate, i);
        if (!!a != !!b) {
            snprintf(field, sizeof field, "object[%u].present", i);
            return diverged_value(result, reference, candidate, field, !!a, !!b);
        }
        if (!a) continue;
#define COMPARE_OBJECT(member) \
        do { \
            if (a->member != b->member) { \
                snprintf(field, sizeof field, "object[%u].%s", i, #member); \
                return diverged_value(result, reference, candidate, field, \
                                      a->member, b->member); \
            } \
        } while (0)
        COMPARE_OBJECT(x);
        COMPARE_OBJECT(y);
        COMPARE_OBJECT(view);
        COMPARE_OBJECT(loop);
        COMPARE_OBJECT(cel);
        COMPARE_OBJECT(motion);
#undef COMPARE_OBJECT
    }
    if (reference->message_count != candidate->message_count)
        return diverged_value(result, reference, candidate, "message_count",
                              reference->message_count, candidate->message_count);
    for (i = 0; i < reference->message_count; ++i) {
        const LogicOracleMessage *a = &reference->messages[i];
        const LogicOracleMessage *b = &candidate->messages[i];
        if (a->logic != b->logic || a->id != b->id) {
            snprintf(field, sizeof field, "message[%u].identity", i);
            return diverged_value(result, reference, candidate, field,
                                  (unsigned)a->logic * 256u + a->id,
                                  (unsigned)b->logic * 256u + b->id);
        }
        if (strcmp(a->text, b->text) != 0) {
            snprintf(field, sizeof field, "message[%u:%u].text", a->logic, a->id);
            return diverged_text(result, reference, candidate, field, a->text, b->text);
        }
    }
    if (reference->has_failure != candidate->has_failure)
        return diverged_value(result, reference, candidate, "failure.present",
                              reference->has_failure, candidate->has_failure);
    if (reference->has_failure) {
        if (reference->failure_kind != candidate->failure_kind)
            return diverged_value(result, reference, candidate, "failure.kind",
                                  reference->failure_kind,
                                  candidate->failure_kind);
        if (reference->failure_logic != candidate->failure_logic)
            return diverged_value(result, reference, candidate, "failure.logic",
                                  reference->failure_logic,
                                  candidate->failure_logic);
        if (reference->failure_logical_pc != candidate->failure_logical_pc)
            return diverged_value(result, reference, candidate,
                                  "failure.logical_pc",
                                  reference->failure_logical_pc,
                                  candidate->failure_logical_pc);
        if (reference->failure_opcode != candidate->failure_opcode)
            return diverged_value(result, reference, candidate,
                                  "failure.opcode",
                                  reference->failure_opcode,
                                  candidate->failure_opcode);
        if (reference->failure_reason != candidate->failure_reason)
            return diverged_value(result, reference, candidate,
                                  "failure.reason",
                                  reference->failure_reason,
                                  candidate->failure_reason);
        if (reference->failure_code != candidate->failure_code)
            return diverged_value(result, reference, candidate,
                                  "failure.code",
                                  reference->failure_code,
                                  candidate->failure_code);
    }
    if (reference->control_flow_reliable && candidate->control_flow_reliable) {
        if (reference->logic != candidate->logic)
            return diverged_value(result, reference, candidate, "logic",
                                  reference->logic, candidate->logic);
        if (reference->logical_pc != candidate->logical_pc)
            return diverged_value(result, reference, candidate, "logical_pc",
                                  reference->logical_pc, candidate->logical_pc);
    }
    result->status = LOGIC_ORACLE_MATCH;
    return result->status;
}

LogicOracleStatus logic_oracle_compare(const LogicOracleSnapshot *reference,
                                      const LogicOracleSnapshot *candidate,
                                      LogicOracleResult *result)
{
    return logic_oracle_compare_with_policy(reference, candidate,
                                            LOGIC_ORACLE_POLICY_DETERMINISTIC,
                                            result);
}

LogicOracleStatus logic_oracle_compare_streams_with_policy(
    const LogicOracleSnapshot *reference, size_t reference_count,
    const LogicOracleSnapshot *candidate, size_t candidate_count,
    LogicOraclePolicy policy, LogicOracleResult *result)
{
    char detail[128];
    size_t i, shared_count;
    LogicOracleStatus status;
    if (!result) return LOGIC_ORACLE_INVALID;
    memset(result, 0, sizeof *result);
    if (policy != LOGIC_ORACLE_POLICY_DETERMINISTIC &&
        policy != LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC)
        return invalid_result(result, "invalid comparison policy");
    if (!snapshot_stream_valid(reference, reference_count, detail, sizeof detail) ||
        !snapshot_stream_valid(candidate, candidate_count, detail, sizeof detail))
        return invalid_result(result, detail);
    shared_count = reference_count < candidate_count ? reference_count : candidate_count;
    for (i = 0; i < shared_count; i++) {
        status = logic_oracle_compare_with_policy(&reference[i], &candidate[i],
                                                  policy, result);
        if (status != LOGIC_ORACLE_MATCH) return status;
    }
    if (reference_count != candidate_count) {
        const LogicOracleSnapshot *a = i < reference_count ? &reference[i] : NULL;
        const LogicOracleSnapshot *b = i < candidate_count ? &candidate[i] : NULL;
        result_context(result, a, b);
        result->checkpoint_index = (uint32_t)i;
        result->status = LOGIC_ORACLE_DIVERGED;
        snprintf(result->field, sizeof result->field, "checkpoint");
        snprintf(result->reference_text, sizeof result->reference_text, "%s",
                 a ? a->checkpoint : "<end>");
        snprintf(result->candidate_text, sizeof result->candidate_text, "%s",
                 b ? b->checkpoint : "<end>");
        return result->status;
    }
    result->status = LOGIC_ORACLE_MATCH;
    return result->status;
}

LogicOracleStatus logic_oracle_compare_streams(
    const LogicOracleSnapshot *reference, size_t reference_count,
    const LogicOracleSnapshot *candidate, size_t candidate_count,
    LogicOracleResult *result)
{
    return logic_oracle_compare_streams_with_policy(
        reference, reference_count, candidate, candidate_count,
        LOGIC_ORACLE_POLICY_DETERMINISTIC, result);
}

int logic_oracle_snapshot_write(const char *path,
                                const LogicOracleSnapshot *snapshots,
                                size_t count)
{
    FILE *file;
    size_t i;
    char detail[128];
    int ok = 0;
    if (!path || !snapshots || !count || count > UINT32_MAX) return -1;
    if (!snapshot_stream_valid(snapshots, count, detail, sizeof detail))
        return -1;
    file = fopen(path, "wb");
    if (!file) return -1;
    if (write_header(file, "LORS", LOGIC_ORACLE_SNAPSHOT_VERSION,
                     LOGIC_ORACLE_SNAPSHOT_RECORD_SIZE, count) != 0)
        goto done;
    for (i = 0; i < count; i++) {
        if (write_snapshot(file, &snapshots[i]) != 0)
            goto done;
    }
    ok = 1;
done:
    if (fclose(file) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int snapshot_load_from(FILE *file, uint32_t disk_count,
                              LogicOracleSnapshot **snapshots, size_t *count)
{
    LogicOracleSnapshot *items;
    if (!snapshots || !count || disk_count == 0) return -1;
    items = (LogicOracleSnapshot *)calloc(disk_count, sizeof *items);
    if (!items) return -1;
    if (read_snapshot_records(file, disk_count, items, disk_count) != 0) {
        free(items);
        return -1;
    }
    *snapshots = items;
    *count = disk_count;
    return 0;
}

int logic_oracle_snapshot_read(const char *path,
                               LogicOracleSnapshot *snapshots,
                               size_t capacity, size_t *count)
{
    FILE *file;
    uint32_t disk_count;
    int ok;
    if (!path || !count) return -1;
    file = fopen(path, "rb");
    if (!file) return -1;
    ok = read_header(file, "LORS", LOGIC_ORACLE_SNAPSHOT_VERSION,
                     LOGIC_ORACLE_SNAPSHOT_RECORD_SIZE,
                     &disk_count) == 0 &&
         read_snapshot_records(file, disk_count, snapshots, capacity) == 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) return -1;
    *count = disk_count;
    return 0;
}

int logic_oracle_snapshot_load(const char *path,
                               LogicOracleSnapshot **snapshots,
                               size_t *count)
{
    FILE *file;
    uint32_t disk_count;
    int ok;
    if (!path || !snapshots || !count) return -1;
    *snapshots = NULL;
    *count = 0;
    file = fopen(path, "rb");
    if (!file) return -1;
    ok = read_header(file, "LORS", LOGIC_ORACLE_SNAPSHOT_VERSION,
                     LOGIC_ORACLE_SNAPSHOT_RECORD_SIZE,
                     &disk_count) == 0 &&
         snapshot_load_from(file, disk_count, snapshots, count) == 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        free(*snapshots);
        *snapshots = NULL;
        *count = 0;
        return -1;
    }
    return 0;
}

int logic_oracle_trace_write(const char *path,
                             const LogicOracleTraceEvent *events,
                             size_t count)
{
    FILE *file;
    size_t i;
    uint32_t previous_sequence = 0;
    int ok = 0;
    if (!path || (count && !events) || count > UINT32_MAX) return -1;
    file = fopen(path, "wb");
    if (!file) return -1;
    if (write_header(file, "LORT", LOGIC_ORACLE_TRACE_VERSION,
                     LOGIC_ORACLE_TRACE_RECORD_SIZE, count) != 0)
        goto done;
    for (i = 0; i < count; i++) {
        if (!trace_event_valid(&events[i]) ||
            (i && events[i].sequence <= previous_sequence) ||
            write_trace_event(file, &events[i]) != 0)
            goto done;
        previous_sequence = events[i].sequence;
    }
    ok = 1;
done:
    if (fclose(file) != 0) ok = 0;
    return ok ? 0 : -1;
}

int logic_oracle_trace_read(const char *path,
                            LogicOracleTraceEvent *events,
                            size_t capacity, size_t *count)
{
    FILE *file;
    uint32_t disk_count;
    int ok;
    if (!path || !count) return -1;
    file = fopen(path, "rb");
    if (!file) return -1;
    ok = read_header(file, "LORT", LOGIC_ORACLE_TRACE_VERSION,
                     LOGIC_ORACLE_TRACE_RECORD_SIZE,
                     &disk_count) == 0 &&
         read_trace_records(file, disk_count, events, capacity) == 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) return -1;
    *count = disk_count;
    return 0;
}

int logic_oracle_trace_validate(const char *path)
{
    FILE *file;
    uint32_t count, i, previous_sequence = 0;
    LogicOracleTraceEvent event;
    int ok = 0;
    if (!path) return -1;
    file = fopen(path, "rb");
    if (!file) return -1;
    if (read_header(file, "LORT", LOGIC_ORACLE_TRACE_VERSION,
                    LOGIC_ORACLE_TRACE_RECORD_SIZE, &count) != 0)
        goto done;
    for (i = 0; i < count; i++) {
        if (read_trace_event(file, &event) != 0 ||
            (i && event.sequence <= previous_sequence))
            goto done;
        previous_sequence = event.sequence;
    }
    if (file_finished(file) != 0) goto done;
    ok = 1;
done:
    if (fclose(file) != 0) ok = 0;
    return ok ? 0 : -1;
}

int logic_oracle_trace_find(const char *path, uint32_t sequence,
                            LogicOracleTraceEvent *event)
{
    FILE *file;
    uint32_t count, i, previous_sequence = 0;
    int found = 0, ok = 0;
    LogicOracleTraceEvent current, saved;
    if (!path || !event) return -1;
    file = fopen(path, "rb");
    if (!file) return -1;
    if (read_header(file, "LORT", LOGIC_ORACLE_TRACE_VERSION,
                    LOGIC_ORACLE_TRACE_RECORD_SIZE, &count) != 0)
        goto done;
    for (i = 0; i < count; i++) {
        if (read_trace_event(file, &current) != 0 ||
            (i && current.sequence <= previous_sequence))
            goto done;
        previous_sequence = current.sequence;
        if (current.sequence == sequence) {
            saved = current;
            found = 1;
        }
    }
    if (file_finished(file) != 0) goto done;
    if (found) *event = saved;
    ok = 1;
done:
    if (fclose(file) != 0) ok = 0;
    return ok ? (found ? 0 : 1) : -1;
}

const char *logic_oracle_status_name(LogicOracleStatus status)
{
    switch (status) {
    case LOGIC_ORACLE_MATCH: return "match";
    case LOGIC_ORACLE_DIVERGED: return "diverged";
    case LOGIC_ORACLE_INCOMPATIBLE: return "incompatible";
    case LOGIC_ORACLE_INVALID: return "invalid";
    }
    return "invalid";
}
