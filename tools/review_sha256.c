#include "review_sha256.h"

#include <stdio.h>

static const uint32_t round_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t rotate_right(uint32_t value, unsigned amount)
{
    return (value >> amount) | (value << (32 - amount));
}

static void transform(review_sha256 *context, const unsigned char block[64])
{
    uint32_t words[64];
    for (size_t i = 0; i < 16; ++i) {
        size_t at = i * 4;
        words[i] = ((uint32_t)block[at] << 24) | ((uint32_t)block[at + 1] << 16) |
                   ((uint32_t)block[at + 2] << 8) | block[at + 3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
        uint32_t s1 = rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    uint32_t a = context->state[0], b = context->state[1], c = context->state[2], d = context->state[3];
    uint32_t e = context->state[4], f = context->state[5], g = context->state[6], h = context->state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t upper1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t first = h + upper1 + choice + round_constants[i] + words[i];
        uint32_t upper0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t second = upper0 + majority;
        h = g; g = f; f = e; e = d + first;
        d = c; c = b; b = a; a = first + second;
    }
    context->state[0] += a; context->state[1] += b; context->state[2] += c; context->state[3] += d;
    context->state[4] += e; context->state[5] += f; context->state[6] += g; context->state[7] += h;
}

void review_sha256_init(review_sha256 *context)
{
    static const uint32_t initial[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    for (size_t i = 0; i < 8; ++i) context->state[i] = initial[i];
    context->bit_count = 0;
    context->block_length = 0;
}

void review_sha256_update(review_sha256 *context, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    context->bit_count += (uint64_t)length * 8;
    while (length) {
        size_t amount = sizeof(context->block) - context->block_length;
        if (amount > length) amount = length;
        for (size_t i = 0; i < amount; ++i) context->block[context->block_length + i] = bytes[i];
        context->block_length += amount;
        bytes += amount;
        length -= amount;
        if (context->block_length == sizeof(context->block)) {
            transform(context, context->block);
            context->block_length = 0;
        }
    }
}

void review_sha256_final(review_sha256 *context, unsigned char digest[32])
{
    uint64_t bit_count = context->bit_count;
    unsigned char marker = 0x80;
    review_sha256_update(context, &marker, 1);
    unsigned char zero = 0;
    while (context->block_length != 56) review_sha256_update(context, &zero, 1);
    unsigned char length[8];
    for (size_t i = 0; i < 8; ++i) length[7 - i] = (unsigned char)(bit_count >> (i * 8));
    review_sha256_update(context, length, sizeof(length));
    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4] = (unsigned char)(context->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(context->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(context->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)context->state[i];
    }
}

void review_sha256_hex(const unsigned char digest[32], char text[65])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        text[i * 2] = digits[digest[i] >> 4];
        text[i * 2 + 1] = digits[digest[i] & 15];
    }
    text[64] = '\0';
}

int review_sha256_file(const char *path, char text[65])
{
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    review_sha256 context;
    review_sha256_init(&context);
    unsigned char buffer[16384];
    size_t length;
    while ((length = fread(buffer, 1, sizeof(buffer), file)) != 0)
        review_sha256_update(&context, buffer, length);
    int read_failed = ferror(file);
    int close_failed = fclose(file);
    if (read_failed || close_failed) return -1;
    unsigned char digest[32];
    review_sha256_final(&context, digest);
    review_sha256_hex(digest, text);
    return 0;
}
