#define _POSIX_C_SOURCE 200809L

#include "logic_reduce.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static int monotonic_milliseconds(uint64_t *milliseconds)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;
    *milliseconds = (uint64_t)now.tv_sec * 1000 +
                    (uint64_t)now.tv_nsec / 1000000;
    return 0;
}

static int elapsed_milliseconds(uint64_t started, uint32_t *elapsed)
{
    uint64_t now, difference;
    if (monotonic_milliseconds(&now) != 0 || now < started)
        return -1;
    difference = now - started;
    *elapsed = difference > UINT32_MAX ? UINT32_MAX : (uint32_t)difference;
    return 0;
}

static int signatures_equal(const LogicFailureSignature *a,
                            const LogicFailureSignature *b)
{
    return a->kind == b->kind && a->logic == b->logic &&
           a->logical_pc == b->logical_pc && a->opcode == b->opcode &&
           a->reason == b->reason && a->code == b->code;
}

/* 0 = oracle result, 1 = budget expired, -1 = oracle/clock error. */
static int run_oracle(LogicReplayOracle oracle, void *context,
                      const LogicReplayManifest *manifest,
                      const LogicReplayEvent *events, size_t count,
                      const LogicReductionOptions *options,
                      uint64_t started, LogicReductionReport *report,
                      int *oracle_result, LogicFailureSignature *signature)
{
    uint32_t elapsed;
    uint32_t remaining;
    int result;
    if (elapsed_milliseconds(started, &elapsed) != 0)
        return -1;
    if (elapsed >= options->timeout_ms) {
        report->limit_reached = 1;
        return 1;
    }
    remaining = options->timeout_ms - elapsed;
    memset(signature, 0, sizeof *signature);
    result = oracle(context, manifest, events, count, remaining, signature);
    ++report->oracle_runs;
    if (elapsed_milliseconds(started, &elapsed) != 0)
        return -1;
    if (elapsed > options->timeout_ms) {
        report->limit_reached = 1;
        return 1;
    }
    if (result != 0 && result != 1)
        return -1;
    *oracle_result = result;
    return 0;
}

static int establish_baseline(LogicReplayOracle oracle, void *context,
                              const LogicReplayManifest *manifest,
                              const LogicReplayEvent *events, size_t count,
                              const LogicReductionOptions *options,
                              uint64_t started, LogicReductionReport *report,
                              LogicFailureSignature *failure)
{
    unsigned run;
    for (run = 0; run < options->agreement_runs; ++run) {
        LogicFailureSignature current;
        int oracle_result;
        int status = run_oracle(oracle, context, manifest, events, count,
                                options, started, report, &oracle_result,
                                &current);
        if (status != 0)
            return -1;
        if (oracle_result != 0)
            return -1;
        if (!run)
            *failure = current;
        else if (!signatures_equal(failure, &current))
            return -1;
    }
    report->repeated_run_agreement = 1;
    return 0;
}

/* 0 = accepted, 1 = rejected, 2 = limit reached, -1 = error. */
static int candidate_preserves_failure(LogicReplayOracle oracle,
                                       void *context,
                                       const LogicReplayManifest *manifest,
                                       const LogicReplayEvent *events,
                                       size_t count,
                                       const LogicReductionOptions *options,
                                       uint64_t started,
                                       LogicReductionReport *report,
                                       const LogicFailureSignature *failure)
{
    unsigned run;
    if (report->attempts >= options->max_attempts) {
        report->limit_reached = 1;
        return 2;
    }
    ++report->attempts;
    for (run = 0; run < options->agreement_runs; ++run) {
        LogicFailureSignature current;
        int oracle_result;
        int status = run_oracle(oracle, context, manifest, events, count,
                                options, started, report, &oracle_result,
                                &current);
        if (status == 1)
            return 2;
        if (status < 0)
            return -1;
        if (oracle_result != 0 || !signatures_equal(failure, &current))
            return 1;
    }
    return 0;
}

static int without_indices(const LogicReplayEvent *source, size_t source_count,
                           size_t first, size_t second,
                           LogicReplayEvent **output, size_t *output_count)
{
    size_t removed = second == SIZE_MAX || second == first ? 1 : 2;
    size_t i, count, at = 0;
    LogicReplayEvent *candidate;
    if (removed > source_count)
        return -1;
    count = source_count - removed;
    candidate = count
        ? (LogicReplayEvent *)malloc(count * sizeof *candidate) : NULL;
    if (count && !candidate)
        return -1;
    for (i = 0; i < source_count; ++i) {
        if (i == first || i == second)
            continue;
        candidate[at] = source[i];
        candidate[at].sequence = (uint32_t)at;
        ++at;
    }
    if (logic_replay_trace_validate(candidate, count) != 0) {
        free(candidate);
        return 1;
    }
    *output = candidate;
    *output_count = count;
    return 0;
}

static int without_range(const LogicReplayEvent *source, size_t source_count,
                         size_t first, size_t end,
                         LogicReplayEvent **output, size_t *output_count)
{
    size_t i, count, at = 0;
    LogicReplayEvent *candidate;
    if (first >= end || end > source_count)
        return -1;
    count = source_count - (end - first);
    candidate = count
        ? (LogicReplayEvent *)malloc(count * sizeof *candidate) : NULL;
    if (count && !candidate)
        return -1;
    for (i = 0; i < source_count; ++i) {
        if (i >= first && i < end)
            continue;
        candidate[at] = source[i];
        candidate[at].sequence = (uint32_t)at;
        ++at;
    }
    if (logic_replay_trace_validate(candidate, count) != 0) {
        free(candidate);
        return 1;
    }
    *output = candidate;
    *output_count = count;
    return 0;
}

static void accept_candidate(LogicReplayEvent **current, size_t *current_count,
                             LogicReplayEvent *candidate, size_t candidate_count,
                             LogicReductionReport *report)
{
    free(*current);
    *current = candidate;
    *current_count = candidate_count;
    report->minimized_count = candidate_count;
}

int logic_reduce_trace(const LogicReplayManifest *manifest,
                       const LogicReplayEvent *events, size_t event_count,
                       LogicReplayOracle oracle, void *oracle_context,
                       const LogicReductionOptions *options,
                       LogicReplayEvent **minimized_events,
                       size_t *minimized_count,
                       LogicFailureSignature *failure,
                       LogicReductionReport *report)
{
    LogicReplayEvent *current = NULL;
    size_t current_count = event_count, granularity = 2;
    uint64_t started;
    uint32_t elapsed;
    int result = -1;
    if (!minimized_events || !minimized_count || !failure || !report)
        return -1;
    *minimized_events = NULL;
    *minimized_count = 0;
    memset(report, 0, sizeof *report);
    if (logic_replay_manifest_validate(manifest) != 0 ||
        logic_replay_trace_validate(events, event_count) != 0 ||
        !oracle || !options || !options->max_attempts ||
        !options->timeout_ms || !options->agreement_runs ||
        options->agreement_runs > 64 ||
        event_count > SIZE_MAX / sizeof *current ||
        monotonic_milliseconds(&started) != 0)
        return -1;
    report->original_count = event_count;
    report->minimized_count = event_count;
    if (event_count) {
        current = (LogicReplayEvent *)malloc(event_count * sizeof *current);
        if (!current)
            return -1;
        memcpy(current, events, event_count * sizeof *current);
    }
    if (establish_baseline(oracle, oracle_context, manifest, current,
                           current_count, options, started, report,
                           failure) != 0)
        goto done;

    while (current_count > 1 && !report->limit_reached) {
        size_t chunk_size, start;
        int changed = 0;
        if (granularity > current_count)
            granularity = current_count;
        chunk_size = current_count / granularity +
                     (current_count % granularity != 0);
        for (start = 0; start < current_count;) {
            size_t end = chunk_size > current_count - start
                ? current_count : start + chunk_size;
            LogicReplayEvent *candidate = NULL;
            size_t candidate_count = 0;
            int made = without_range(current, current_count, start,
                                     end, &candidate, &candidate_count);
            int accepted;
            if (made < 0)
                goto done;
            start = end;
            if (made > 0)
                continue;
            accepted = candidate_preserves_failure(
                oracle, oracle_context, manifest, candidate, candidate_count,
                options, started, report, failure);
            if (accepted < 0) {
                free(candidate);
                goto done;
            }
            if (accepted == 2) {
                free(candidate);
                break;
            }
            if (accepted == 0) {
                accept_candidate(&current, &current_count, candidate,
                                 candidate_count, report);
                changed = 1;
                break;
            }
            free(candidate);
        }
        if (report->limit_reached)
            break;
        if (changed) {
            if (granularity > 2)
                --granularity;
        } else if (granularity < current_count) {
            granularity = granularity > current_count / 2
                ? current_count : granularity * 2;
        } else {
            break;
        }
    }

    /* Paired inputs and CALL/RETURN records are one deletion unit. */
    while (current_count && !report->limit_reached) {
        size_t i;
        int changed = 0;
        for (i = 0; i < current_count; ++i) {
            size_t paired = SIZE_MAX, j;
            LogicReplayEvent *candidate = NULL;
            size_t candidate_count = 0;
            int made, accepted;
            if (current[i].kind == LOGIC_TRACE_KIND_RETURN)
                continue;
            if (current[i].kind == LOGIC_TRACE_KIND_CALL) {
                for (j = i + 1; j < current_count; ++j)
                    if (current[j].kind == LOGIC_TRACE_KIND_RETURN &&
                        current[j].logic == current[i].logic &&
                        current[j].depth == current[i].depth) {
                        paired = j;
                        break;
                    }
                if (paired == SIZE_MAX)
                    goto done;
            } else if (current[i].kind == LOGIC_TRACE_KIND_MARK &&
                current[i].reason == LOGIC_REPLAY_REASON_INPUT_UP)
                continue;
            else if (current[i].kind == LOGIC_TRACE_KIND_MARK &&
                current[i].reason == LOGIC_REPLAY_REASON_INPUT_DOWN) {
                for (j = i + 1; j < current_count; ++j)
                    if (current[j].kind == LOGIC_TRACE_KIND_MARK &&
                        current[j].reason == LOGIC_REPLAY_REASON_INPUT_UP &&
                        current[j].logic == current[i].logic &&
                        current[j].field == current[i].field) {
                        paired = j;
                        break;
                    }
                if (paired == SIZE_MAX)
                    goto done;
            }
            made = without_indices(current, current_count, i, paired,
                                   &candidate, &candidate_count);
            if (made < 0)
                goto done;
            if (made > 0)
                continue;
            accepted = candidate_preserves_failure(
                oracle, oracle_context, manifest, candidate, candidate_count,
                options, started, report, failure);
            if (accepted < 0) {
                free(candidate);
                goto done;
            }
            if (accepted == 2) {
                free(candidate);
                break;
            }
            if (accepted == 0) {
                accept_candidate(&current, &current_count, candidate,
                                 candidate_count, report);
                changed = 1;
                break;
            }
            free(candidate);
        }
        if (report->limit_reached || !changed)
            break;
    }

    if (current_count) {
        *minimized_events = (LogicReplayEvent *)malloc(current_count * sizeof **minimized_events);
        if (!*minimized_events)
            goto done;
        memcpy(*minimized_events, current, current_count * sizeof **minimized_events);
    }
    *minimized_count = current_count;
    result = 0;

done:
    if (elapsed_milliseconds(started, &elapsed) == 0)
        report->elapsed_ms = elapsed;
    else if (result == 0)
        result = -1;
    if (result != 0) {
        free(*minimized_events);
        *minimized_events = NULL;
        *minimized_count = 0;
    }
    free(current);
    return result;
}
