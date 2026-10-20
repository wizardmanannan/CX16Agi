#ifndef LOGIC_REDUCE_H
#define LOGIC_REDUCE_H

#include "logic_replay.h"

typedef struct LogicReductionOptions {
    size_t max_attempts;
    uint32_t timeout_ms;
    unsigned agreement_runs;
} LogicReductionOptions;

typedef struct LogicReductionReport {
    size_t original_count;
    size_t minimized_count;
    size_t attempts;
    size_t oracle_runs;
    uint32_t elapsed_ms;
    int repeated_run_agreement;
    int limit_reached;
} LogicReductionReport;

/* Return 0 when the same structured failure remains, 1 when it does not,
 * and -1 on an oracle error. The oracle must stop within timeout_ms. */
typedef int (*LogicReplayOracle)(void *context,
                                const LogicReplayManifest *manifest,
                                const LogicReplayEvent *events, size_t count,
                                uint32_t timeout_ms,
                                LogicFailureSignature *signature);

/* Chunks are reduced first, then individual events. Input down/up records are
 * deleted as one pair. The returned trace is heap allocated. timeout_ms is
 * the total reduction budget; each oracle call receives its remaining time. */
int logic_reduce_trace(const LogicReplayManifest *manifest,
                       const LogicReplayEvent *events, size_t event_count,
                       LogicReplayOracle oracle, void *oracle_context,
                       const LogicReductionOptions *options,
                       LogicReplayEvent **minimized_events,
                       size_t *minimized_count,
                       LogicFailureSignature *failure,
                       LogicReductionReport *report);

#endif
