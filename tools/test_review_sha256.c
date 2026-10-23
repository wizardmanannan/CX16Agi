#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "review_sha256.h"
#include "test_paths.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static void check_vector(const unsigned char *data, size_t length, const char *expected, size_t chunk)
{
    review_sha256 context;
    review_sha256_init(&context);
    size_t at = 0;
    while (at < length) {
        size_t amount = length - at;
        if (amount > chunk) amount = chunk;
        review_sha256_update(&context, data + at, amount);
        at += amount;
    }
    unsigned char digest[32];
    char actual[65];
    review_sha256_final(&context, digest);
    review_sha256_hex(digest, actual);
    CHECK(!strcmp(actual, expected), "SHA-256 known vector or block-boundary update failed");
}

static void check_directory_read_error(const char *directory, const char *valid_file)
{
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork isolated SHA-256 descriptor test");
    if (!child) {
        struct rlimit limit;
        if (getrlimit(RLIMIT_NOFILE, &limit) || limit.rlim_max < 7) _Exit(40);
        limit.rlim_cur = 7;
        if (setrlimit(RLIMIT_NOFILE, &limit)) _Exit(41);

        char digest[65];
        for (int attempt = 0; attempt < 4; ++attempt) {
            errno = 0;
            if (!review_sha256_file(directory, digest) || errno != EISDIR) {
                dprintf(STDERR_FILENO, "Expected real directory read error on attempt %d, got errno %d\n",
                        attempt + 1, errno);
                _Exit(42);
            }
        }
        if (review_sha256_file(valid_file, digest)) {
            dprintf(STDERR_FILENO,
                    "VALID_HASH_AFTER_REAL_READ_ERRORS: failed errno %d (file descriptor leak)\n", errno);
            _Exit(43);
        }
        if (strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) _Exit(44);
        _Exit(EXIT_SUCCESS);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for isolated SHA-256 descriptor test");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status),
          "SHA-256 failed to close files after real read errors before hashing a valid file");
}

int main(void)
{
    suite_test_paths_init("review-sha256");
    check_vector((const unsigned char *)"", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", 1);
    check_vector((const unsigned char *)"abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 2);

    static const size_t lengths[] = { 55, 56, 63, 64, 65 };
    static const char *const expected[] = {
        "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
        "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
        "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34",
        "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
        "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"
    };
    unsigned char boundary_data[65];
    memset(boundary_data, 'a', sizeof(boundary_data));
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        check_vector(boundary_data, lengths[i], expected[i], 7);

    char valid_file[4096];
    suite_test_path(valid_file, sizeof(valid_file), suite_fixture_directory, "valid.txt");
    FILE *file = fopen(valid_file, "wb");
    CHECK(file && fwrite("abc", 1, 3, file) == 3 && !fclose(file), "Cannot create SHA-256 valid-file input");

    FILE *directory = fopen(suite_fixture_directory, "rb");
    CHECK(directory != NULL, "Cannot open directory for real SHA-256 read-error case");
    unsigned char byte;
    CHECK(!fread(&byte, 1, 1, directory) && ferror(directory), "Directory read did not produce a real stdio error");
    CHECK(!fclose(directory), "Cannot close directory after real SHA-256 read-error case");

    check_directory_read_error(suite_fixture_directory, valid_file);
    puts("PASS SHA-256 vectors, padding boundaries, and file cleanup after read errors");
    return EXIT_SUCCESS;
}
