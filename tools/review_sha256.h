#ifndef CX16AGI_REVIEW_SHA256_H
#define CX16AGI_REVIEW_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_length;
} review_sha256;

void review_sha256_init(review_sha256 *context);
void review_sha256_update(review_sha256 *context, const void *data, size_t length);
void review_sha256_final(review_sha256 *context, unsigned char digest[32]);
void review_sha256_hex(const unsigned char digest[32], char text[65]);
int review_sha256_file(const char *path, char text[65]);

#endif
