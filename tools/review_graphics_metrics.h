#ifndef CX16AGI_REVIEW_GRAPHICS_METRICS_H
#define CX16AGI_REVIEW_GRAPHICS_METRICS_H

#include <stddef.h>
#include <stdint.h>

#define REVIEW_GRAPHICS_METRICS_WORKLOAD "graphics-priority-clear-test-v1"
#define REVIEW_GRAPHICS_METRICS_INPUT_MAGIC "review-graphics-test-workload-v2"

typedef struct {
    uint64_t cycles;
    uint64_t total_cycles;
    uint64_t latency_ns;
    char input_sha256[65];
    char output_sha256[65];
    char toolchain_sha256[65];
} ReviewGraphicsMetricsMeasurement;

/* Run one image against the hashed, image-bound AGI test workload. */
int review_graphics_metrics_measure(const char *input_path, const char *image_path,
                                   const char *output_path, const char *workload,
                                   ReviewGraphicsMetricsMeasurement *measurement,
                                   char *error, size_t error_capacity);

int review_graphics_metrics_toolchain_sha256(char digest[65], char *error, size_t error_capacity);
int review_graphics_metrics_cli(int argc, char **argv);

#endif
