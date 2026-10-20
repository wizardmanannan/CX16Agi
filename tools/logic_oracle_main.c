#include "logic_oracle.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_trace(const char *label, const char *path, int has_trace,
                        uint32_t sequence)
{
    LogicOracleTraceEvent event;
    int found;
    if (!has_trace) {
        printf("  %s trace: no event recorded\n", label);
        return;
    }
    if (!path) {
        printf("  %s trace: event #%" PRIu32 " (trace file not supplied)\n",
               label, sequence);
        return;
    }
    found = logic_oracle_trace_find(path, sequence, &event);
    if (found < 0) {
        printf("  %s trace: invalid or unreadable trace file\n", label);
    } else if (found > 0) {
        printf("  %s trace: event #%" PRIu32 " not present\n", label, sequence);
    } else {
        printf("  %s trace: sequence=%" PRIu32 " tick=%" PRIu32
               " kind=%u logic=%u pc=$%04X opcode=$%02X depth=%u bank=%u"
               " field=%u before=%" PRIu32 " after=%" PRIu32 " reason=%u\n",
               label, event.sequence, event.tick, event.kind, event.logic,
               event.logical_pc, event.opcode, event.depth, event.bank,
               event.field, event.before, event.after, event.reason);
    }
}

static void print_result(const LogicOracleResult *result,
                         const char *reference_trace,
                         const char *candidate_trace)
{
    printf("%s checkpoint=%" PRIu32 " (%s)",
           logic_oracle_status_name(result->status), result->checkpoint_index,
           result->checkpoint[0] ? result->checkpoint : "unknown");
    if (result->field[0]) {
        printf(" field=%s", result->field);
        if (result->reference_text[0] || result->candidate_text[0])
            printf(" reference=\"%s\" candidate=\"%s\"",
                   result->reference_text, result->candidate_text);
        else
            printf(" reference=%" PRId64 " candidate=%" PRId64,
                   result->reference_value, result->candidate_value);
    }
    if (result->detail[0]) printf(": %s", result->detail);
    putchar('\n');
    if (result->reference_provenance[0] || result->candidate_provenance[0])
        printf("  provenance: reference=\"%s\" candidate=\"%s\"\n",
               result->reference_provenance, result->candidate_provenance);
    if (result->status == LOGIC_ORACLE_DIVERGED) {
        print_trace("reference", reference_trace, result->reference_has_trace,
                    result->reference_trace_sequence);
        print_trace("candidate", candidate_trace, result->candidate_has_trace,
                    result->candidate_trace_sequence);
    }
}

int main(int argc, char **argv)
{
    LogicOracleSnapshot *reference = NULL, *candidate = NULL;
    size_t reference_count = 0, candidate_count = 0;
    LogicOracleResult result;
    LogicOracleStatus status;
    LogicOraclePolicy policy = LOGIC_ORACLE_POLICY_DETERMINISTIC;
    const char *reference_trace = NULL, *candidate_trace = NULL;
    int exit_code, first_path = 1, remaining;

    if (argc > 1 && strcmp(argv[1], "--cross-engine-semantic") == 0) {
        policy = LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC;
        first_path++;
    }
    remaining = argc - first_path;
    if (remaining != 2 && remaining != 4) {
        fprintf(stderr, "usage: %s [--cross-engine-semantic]"
                " reference.snap candidate.snap"
                " [reference.trace candidate.trace]\n", argv[0]);
        return 2;
    }
    if (remaining == 4) {
        reference_trace = argv[first_path + 2];
        candidate_trace = argv[first_path + 3];
        if (logic_oracle_trace_validate(reference_trace) != 0 ||
            logic_oracle_trace_validate(candidate_trace) != 0) {
            fprintf(stderr, "invalid trace input\n");
            return 2;
        }
    }
    if (logic_oracle_snapshot_load(argv[first_path], &reference,
                                   &reference_count) != 0) {
        fprintf(stderr, "invalid reference snapshot stream: %s\n",
                argv[first_path]);
        return 2;
    }
    if (logic_oracle_snapshot_load(argv[first_path + 1], &candidate,
                                   &candidate_count) != 0) {
        fprintf(stderr, "invalid candidate snapshot stream: %s\n",
                argv[first_path + 1]);
        free(reference);
        return 2;
    }
    status = logic_oracle_compare_streams_with_policy(
        reference, reference_count, candidate, candidate_count, policy, &result);
    print_result(&result, reference_trace, candidate_trace);
    exit_code = status == LOGIC_ORACLE_MATCH ? 0 :
                status == LOGIC_ORACLE_DIVERGED ? 1 : 2;
    free(reference);
    free(candidate);
    return exit_code;
}
