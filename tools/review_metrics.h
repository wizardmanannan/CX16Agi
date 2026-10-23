#ifndef CX16AGI_REVIEW_METRICS_H
#define CX16AGI_REVIEW_METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *repository;
    const char *input_path;
    const char *workload;
    const char *baseline_image;
    const char *baseline_manifest;
    const char *baseline_runner;
    const char *candidate_image;
    const char *candidate_manifest;
    const char *candidate_runner;
    const char *proof_kind;
    const char *tradeoff_path;
    const char *scratch_directory;
    uint64_t max_bytes;
    uint64_t max_cycles;
    uint64_t max_latency_ns;
    uint64_t runner_timeout_ms;
} ReviewMetricsOptions;

typedef struct {
    char baseline_commit[65];
    char baseline_tree[65];
    char candidate_commit[65];
    char candidate_tree[65];
    char input_sha256[65];
    char runner_sha256[65];
    char toolchain_sha256[65];
    char baseline_image_sha256[65];
    char candidate_image_sha256[65];
    char baseline_output_sha256[65];
    char candidate_output_sha256[65];
    uint64_t baseline_bytes;
    uint64_t baseline_cycles;
    uint64_t baseline_latency_ns;
    uint64_t candidate_bytes;
    uint64_t candidate_cycles;
    uint64_t candidate_latency_ns;
    uint64_t max_bytes;
    uint64_t max_cycles;
    uint64_t max_latency_ns;
    uint64_t runner_timeout_ms;
    bool local_pareto_improvement;
} ReviewMetricsResult;

/*
 * Latency and toolchain identity are supplied by the pinned measurement runner. The gate checks
 * their schema, bounds, and baseline/candidate consistency; it cannot independently attest them.
 */
int review_metrics_gate(const ReviewMetricsOptions *options, ReviewMetricsResult *result,
                        char *error, size_t error_capacity);

#endif
