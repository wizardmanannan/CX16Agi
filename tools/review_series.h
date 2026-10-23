#ifndef CX16AGI_REVIEW_SERIES_H
#define CX16AGI_REVIEW_SERIES_H

#include <stddef.h>

/* Build a review route from a strict review-series-v1 manifest and real Git objects. */
int review_series_route(const char *repository, const char *manifest_path,
                        const char *output_directory, char *error, size_t error_capacity);

#endif
