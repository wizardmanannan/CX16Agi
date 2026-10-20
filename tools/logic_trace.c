#include "logic_trace.h"

#include <string.h>

static int valid_kind(uint8_t kind)
{
    return kind >= LOGIC_TRACE_KIND_WRITE && kind <= LOGIC_TRACE_KIND_MARK;
}

static size_t ring_index(const LogicTraceRing *ring, size_t offset)
{
    size_t to_end = ring->capacity - ring->first;
    return offset >= to_end ? offset - to_end : ring->first + offset;
}

#if !LOGIC_TRACE_RING_ONLY
static void put_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
}

static uint16_t get_u16(const uint8_t *buffer)
{
    return (uint16_t)((uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8));
}

static uint32_t get_u32(const uint8_t *buffer)
{
    return (uint32_t)buffer[0] |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}
#endif

int logic_trace_ring_init(LogicTraceRing *ring, LogicTraceEvent *storage,
                          size_t capacity)
{
    if (!ring || !storage || capacity == 0)
        return -1;
    ring->events = storage;
    ring->capacity = capacity;
    ring->first = 0;
    ring->count = 0;
    ring->next_sequence = 0;
    ring->dropped = 0;
    return 0;
}

#if LOGIC_TRACE_RECORDING
int logic_trace_record(LogicTraceRing *ring, const LogicTraceEvent *event)
{
    LogicTraceEvent copy;
    size_t slot;

    if (!ring || !ring->events || ring->capacity == 0 || !event ||
        !valid_kind(event->kind))
        return -1;
    copy = *event;
    copy.sequence = ring->next_sequence++;
    if (ring->count == ring->capacity) {
        slot = ring->first;
        if (++ring->first == ring->capacity)
            ring->first = 0;
        ++ring->dropped;
    } else {
        slot = ring_index(ring, ring->count);
        ++ring->count;
    }
    ring->events[slot] = copy;
    return 0;
}
#endif

size_t logic_trace_ring_count(const LogicTraceRing *ring)
{
    return ring ? ring->count : 0;
}

uint32_t logic_trace_ring_dropped(const LogicTraceRing *ring)
{
    return ring ? ring->dropped : 0;
}

int logic_trace_ring_get(const LogicTraceRing *ring, size_t oldest_index,
                         LogicTraceEvent *event)
{
    if (!ring || !ring->events || !event || oldest_index >= ring->count)
        return -1;
    *event = ring->events[ring_index(ring, oldest_index)];
    return 0;
}

#if !LOGIC_TRACE_RING_ONLY
int logic_trace_event_matches(const LogicTraceEvent *event,
                              const LogicTraceFilter *filter)
{
    const uint8_t known_flags = LOGIC_TRACE_FILTER_KIND |
                                LOGIC_TRACE_FILTER_LOGIC |
                                LOGIC_TRACE_FILTER_FIELD;
    if (!event || !filter || (filter->flags & (uint8_t)~known_flags) != 0)
        return 0;
    if ((filter->flags & LOGIC_TRACE_FILTER_KIND) &&
        event->kind != filter->kind)
        return 0;
    if ((filter->flags & LOGIC_TRACE_FILTER_LOGIC) &&
        event->logic != filter->logic)
        return 0;
    if ((filter->flags & LOGIC_TRACE_FILTER_FIELD) &&
        event->field != filter->field)
        return 0;
    return 1;
}

int logic_trace_find_last_writer(const LogicTraceEvent *events, size_t count,
                                 uint8_t logic, uint8_t field,
                                 LogicTraceEvent *event)
{
    size_t i = count;
    if ((!events && count != 0) || !event)
        return -1;
    while (i != 0) {
        const LogicTraceEvent *candidate = &events[--i];
        if (candidate->kind == LOGIC_TRACE_KIND_WRITE &&
            candidate->logic == logic && candidate->field == field) {
            *event = *candidate;
            return 0;
        }
    }
    return 1;
}

int logic_trace_validate(const LogicTraceEvent *events, size_t count,
                         LogicTraceCallFrame *call_stack,
                         size_t call_stack_capacity, size_t *bad_index)
{
    size_t i;
    size_t depth = 0;

    if ((!events && count != 0) || (!call_stack && call_stack_capacity != 0))
        return -1;
    if (bad_index)
        *bad_index = 0;
    for (i = 0; i < count; ++i) {
        const LogicTraceEvent *event = &events[i];
        if (!valid_kind(event->kind) ||
            (i != 0 && event->sequence != events[i - 1].sequence + 1u)) {
            if (bad_index)
                *bad_index = i;
            return -1;
        }
        if (event->kind == LOGIC_TRACE_KIND_CALL) {
            if (depth == call_stack_capacity || depth >= UINT8_MAX ||
                event->depth != (uint8_t)(depth + 1)) {
                if (bad_index)
                    *bad_index = i;
                return -1;
            }
            call_stack[depth].logic = event->logic;
            call_stack[depth].depth = event->depth;
            call_stack[depth].context_transition = event->field == LOGIC_TRACE_FIELD_CURRENT_LOG;
            call_stack[depth].callee = (uint8_t)event->after;
            call_stack[depth].call_site = event->logical_pc;
            call_stack[depth].opcode = event->opcode;
            if (call_stack[depth].context_transition &&
                (event->before != event->logic || event->after > UINT8_MAX)) {
                if (bad_index) *bad_index = i;
                return -1;
            }
            ++depth;
        } else if (event->kind == LOGIC_TRACE_KIND_RETURN) {
            if (depth == 0 || event->depth != call_stack[depth - 1].depth ||
                event->logic != call_stack[depth - 1].logic) {
                if (bad_index)
                    *bad_index = i;
                return -1;
            }
            if (call_stack[depth - 1].context_transition &&
                (event->field != LOGIC_TRACE_FIELD_CURRENT_LOG ||
                 event->before != call_stack[depth - 1].callee ||
                 event->after != call_stack[depth - 1].logic ||
                 event->logical_pc != call_stack[depth - 1].call_site ||
                 event->opcode != call_stack[depth - 1].opcode)) {
                if (bad_index) *bad_index = i;
                return -1;
            }
            --depth;
        }
    }
    if (depth != 0) {
        if (bad_index)
            *bad_index = count;
        return -1;
    }
    return 0;
}

int logic_trace_header_encode(uint32_t record_count, uint8_t *buffer,
                              size_t size)
{
    if (!buffer || size != LOGIC_TRACE_HEADER_SIZE)
        return -1;
    buffer[0] = 'L';
    buffer[1] = 'T';
    buffer[2] = 'R';
    buffer[3] = 'C';
    put_u16(buffer + 4, LOGIC_TRACE_VERSION);
    put_u16(buffer + 6, LOGIC_TRACE_RECORD_SIZE);
    put_u32(buffer + 8, record_count);
    return 0;
}

int logic_trace_header_decode(const uint8_t *buffer, size_t size,
                              uint32_t *record_count)
{
    if (!buffer || !record_count || size != LOGIC_TRACE_HEADER_SIZE ||
        buffer[0] != 'L' ||
        buffer[1] != 'T' || buffer[2] != 'R' || buffer[3] != 'C' ||
        get_u16(buffer + 4) != LOGIC_TRACE_VERSION ||
        get_u16(buffer + 6) != LOGIC_TRACE_RECORD_SIZE)
        return -1;
    *record_count = get_u32(buffer + 8);
    return 0;
}

int logic_trace_event_encode(const LogicTraceEvent *event, uint8_t *buffer,
                             size_t size)
{
    if (!event || !buffer || size != LOGIC_TRACE_RECORD_SIZE ||
        !valid_kind(event->kind))
        return -1;
    put_u32(buffer, event->sequence);
    put_u32(buffer + 4, event->tick);
    buffer[8] = event->kind;
    buffer[9] = event->logic;
    put_u16(buffer + 10, event->logical_pc);
    buffer[12] = event->opcode;
    buffer[13] = event->depth;
    buffer[14] = event->bank;
    buffer[15] = event->field;
    put_u32(buffer + 16, event->before);
    put_u32(buffer + 20, event->after);
    buffer[24] = event->reason;
    return 0;
}

int logic_trace_event_decode(const uint8_t *buffer, size_t size,
                             LogicTraceEvent *event)
{
    if (!buffer || !event || size != LOGIC_TRACE_RECORD_SIZE ||
        !valid_kind(buffer[8]))
        return -1;
    memset(event, 0, sizeof *event);
    event->sequence = get_u32(buffer);
    event->tick = get_u32(buffer + 4);
    event->kind = buffer[8];
    event->logic = buffer[9];
    event->logical_pc = get_u16(buffer + 10);
    event->opcode = buffer[12];
    event->depth = buffer[13];
    event->bank = buffer[14];
    event->field = buffer[15];
    event->before = get_u32(buffer + 16);
    event->after = get_u32(buffer + 20);
    event->reason = buffer[24];
    return 0;
}
#endif
