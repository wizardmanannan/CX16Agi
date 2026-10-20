#ifndef CX16AGI_LOGIC_TRACE_H
#define CX16AGI_LOGIC_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifndef LOGIC_TRACE_RECORDING
#define LOGIC_TRACE_RECORDING 1
#endif

#define LOGIC_TRACE_VERSION 1u
#define LOGIC_TRACE_HEADER_SIZE 12u
#define LOGIC_TRACE_RECORD_SIZE 25u

#ifndef LOGIC_TRACE_RING_ONLY
/* The interpreter needs the bounded ring; wire codecs and analysis run on
   the host. Override explicitly when building a target-side analysis tool. */
#ifdef __CC65__
#define LOGIC_TRACE_RING_ONLY 1
#else
#define LOGIC_TRACE_RING_ONLY 0
#endif
#endif

#define LOGIC_TRACE_FIELD_CURRENT_LOG 9u

enum {
    LOGIC_TRACE_KIND_WRITE = 1,
    LOGIC_TRACE_KIND_CALL = 2,
    LOGIC_TRACE_KIND_RETURN = 3,
    LOGIC_TRACE_KIND_MARK = 4
};

enum {
    LOGIC_TRACE_WRITE_REASON_UNSPECIFIED = 0,
    LOGIC_TRACE_WRITE_REASON_VARIABLE = 1,
    LOGIC_TRACE_WRITE_REASON_FLAG = 2
};

enum {
    LOGIC_TRACE_FILTER_KIND = 1u,
    LOGIC_TRACE_FILTER_LOGIC = 2u,
    LOGIC_TRACE_FILTER_FIELD = 4u
};

typedef struct {
    uint32_t sequence;
    uint32_t tick;
    uint8_t kind;
    uint8_t logic;
    uint16_t logical_pc;
    uint8_t opcode;
    uint8_t depth;
    uint8_t bank;
    uint8_t field;
    uint32_t before;
    uint32_t after;
    uint8_t reason;
} LogicTraceEvent;

typedef struct {
    LogicTraceEvent *events;
    size_t capacity;
    size_t first;
    size_t count;
    uint32_t next_sequence;
    uint32_t dropped;
} LogicTraceRing;

typedef struct {
    uint8_t flags;
    uint8_t kind;
    uint8_t logic;
    uint8_t field;
} LogicTraceFilter;

typedef struct {
    uint8_t logic;
    uint8_t depth;
    uint8_t callee;
    uint8_t context_transition;
    uint16_t call_site;
    uint8_t opcode;
} LogicTraceCallFrame;

/* Caller supplies fixed storage; a full ring evicts its oldest event. */
int logic_trace_ring_init(LogicTraceRing *ring, LogicTraceEvent *storage,
                          size_t capacity);
#if LOGIC_TRACE_RECORDING
int logic_trace_record(LogicTraceRing *ring, const LogicTraceEvent *event);
#else
#define logic_trace_record(ring, event) (0)
#endif
size_t logic_trace_ring_count(const LogicTraceRing *ring);
uint32_t logic_trace_ring_dropped(const LogicTraceRing *ring);
int logic_trace_ring_get(const LogicTraceRing *ring, size_t oldest_index,
                         LogicTraceEvent *event);

#if !LOGIC_TRACE_RING_ONLY
int logic_trace_event_matches(const LogicTraceEvent *event,
                              const LogicTraceFilter *filter);
/* Returns 0 when found, 1 when absent, and -1 for invalid arguments. */
int logic_trace_find_last_writer(const LogicTraceEvent *events, size_t count,
                                 uint8_t logic, uint8_t field,
                                 LogicTraceEvent *event);

/* CALL/RETURN records pair the same owner and depth. Production context
   records also pair caller/callee transitions, call site, and opcode.
   Sequences must be consecutive and calls must fully pair. */
int logic_trace_validate(const LogicTraceEvent *events, size_t count,
                         LogicTraceCallFrame *call_stack,
                         size_t call_stack_capacity, size_t *bad_index);

/* File format is an LTRC/version/record-length/count header followed by
   fixed 25-byte records. All integer fields are encoded little-endian. */
int logic_trace_header_encode(uint32_t record_count, uint8_t *buffer,
                              size_t size);
int logic_trace_header_decode(const uint8_t *buffer, size_t size,
                              uint32_t *record_count);
int logic_trace_event_encode(const LogicTraceEvent *event, uint8_t *buffer,
                             size_t size);
int logic_trace_event_decode(const uint8_t *buffer, size_t size,
                             LogicTraceEvent *event);
#endif

#endif
