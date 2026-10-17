#define _POSIX_C_SOURCE 200809L
#include "review_graphics_metrics.h"

#include "review_sha256.h"
#include "test_graphics_clear.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WORKLOAD_LINE_MAX 512
#define WORKLOAD_INPUT_MAX 8192
#define GRAPHICS_METRIC_FIELDS 17

typedef struct {
    char workload[128];
    char baseline_image_sha256[65];
    char candidate_image_sha256[65];
    char toolchain_sha256[65];
    GraphicsClearTestImage image;
} GraphicsWorkload;

static const char *const workload_keys[GRAPHICS_METRIC_FIELDS] = {
    "workload", "image_kind", "baseline_image_sha256", "candidate_image_sha256",
    "load_address", "image_bytes", "reset_entry", "slice_entry", "return_entry",
    "cursor_address", "remaining_address", "priority_address", "priority_bytes",
    "slice_count", "fill_byte", "sentinel_byte", "toolchain_sha256"
};

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (!length || length >= capacity) return false;
    memcpy(destination, source, length + 1);
    return true;
}

static bool is_sha256(const char *text)
{
    if (strlen(text) != 64) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (!( (*at >= '0' && *at <= '9') || (*at >= 'a' && *at <= 'f') )) return false;
    return true;
}

static bool parse_u64(const char *text, uint64_t *value)
{
    if (!*text) return false;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at)
        if (*at < '0' || *at > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end) return false;
#if ULLONG_MAX > UINT64_MAX
    if (parsed > UINT64_MAX) return false;
#endif
    *value = (uint64_t)parsed;
    return true;
}

static int hash_component(review_sha256 *hash, const char *label, const char *path,
                          char *error, size_t error_capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (error && error_capacity) snprintf(error, error_capacity, "cannot read toolchain component %s", label);
        return -1;
    }
    if (fseek(file, 0, SEEK_END)) { fclose(file); set_error(error, error_capacity, "cannot size toolchain component"); return -1; }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET)) { fclose(file); set_error(error, error_capacity, "cannot seek toolchain component"); return -1; }
    uint64_t bytes = (uint64_t)length;
    unsigned char encoded_size[8];
    for (unsigned i = 0; i < 8; ++i) encoded_size[7 - i] = (unsigned char)(bytes >> (i * 8));
    review_sha256_update(hash, label, strlen(label) + 1);
    review_sha256_update(hash, encoded_size, sizeof(encoded_size));
    unsigned char buffer[16384];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0)
        review_sha256_update(hash, buffer, count);
    bool failed = ferror(file);
    if (fclose(file)) failed = true;
    if (failed) { set_error(error, error_capacity, "cannot hash toolchain component"); return -1; }
    return 0;
}

int review_graphics_metrics_toolchain_sha256(char digest[65], char *error, size_t error_capacity)
{
    const char *cc65_home = getenv("CC65_HOME");
    const char *emulator = getenv("X16_EMULATOR_SOURCE");
    if (!cc65_home || !*cc65_home || !emulator || !*emulator) {
        set_error(error, error_capacity, "CC65_HOME and X16_EMULATOR_SOURCE must pin the test-image toolchain");
        return -1;
    }
    char ca65[4096], ld65[4096], fake6502[4096], fake6502_header[4096];
    if (snprintf(ca65, sizeof(ca65), "%s/bin/ca65", cc65_home) >= (int)sizeof(ca65) ||
        snprintf(ld65, sizeof(ld65), "%s/bin/ld65", cc65_home) >= (int)sizeof(ld65) ||
        snprintf(fake6502, sizeof(fake6502), "%s/src/cpu/fake6502.c", emulator) >= (int)sizeof(fake6502) ||
        snprintf(fake6502_header, sizeof(fake6502_header), "%s/src/cpu/fake6502.h", emulator) >= (int)sizeof(fake6502_header)) {
        set_error(error, error_capacity, "toolchain component path is too long");
        return -1;
    }
    review_sha256 hash;
    review_sha256_init(&hash);
    static const char domain[] = "cx16agi-graphics-test-toolchain-v1";
    review_sha256_update(&hash, domain, sizeof(domain));
    if (hash_component(&hash, "ca65", ca65, error, error_capacity) ||
        hash_component(&hash, "ld65", ld65, error, error_capacity) ||
        hash_component(&hash, "fake6502.c", fake6502, error, error_capacity) ||
        hash_component(&hash, "fake6502.h", fake6502_header, error, error_capacity)) return -1;
    unsigned char bytes[32];
    review_sha256_final(&hash, bytes);
    review_sha256_hex(bytes, digest);
    return 0;
}

static bool parse_workload(const char *path, GraphicsWorkload *workload,
                           char *error, size_t error_capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file) { set_error(error, error_capacity, "cannot open graphics workload input"); return false; }
    if (fseek(file, 0, SEEK_END)) { fclose(file); set_error(error, error_capacity, "cannot size graphics workload input"); return false; }
    long file_length = ftell(file);
    if (file_length <= 0 || (unsigned long)file_length > WORKLOAD_INPUT_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file); set_error(error, error_capacity, "graphics workload input size is out of bounds"); return false;
    }
    size_t input_length = (size_t)file_length;
    char input[WORKLOAD_INPUT_MAX + 1];
    bool read_failed = fread(input, 1, input_length, file) != input_length;
    if (fclose(file)) read_failed = true;
    if (read_failed || memchr(input, '\0', input_length) || input[input_length - 1] != '\n') {
        set_error(error, error_capacity, "graphics workload input contains truncated or trailing bytes");
        return false;
    }
    input[input_length] = '\0';
    char values[GRAPHICS_METRIC_FIELDS][256] = {{0}};
    char *at = input;
    size_t field = 0;
    for (size_t line_number = 0; line_number < GRAPHICS_METRIC_FIELDS + 1; ++line_number) {
        char *newline = memchr(at, '\n', input + input_length - at);
        if (!newline) { set_error(error, error_capacity, "graphics workload input has a missing line terminator"); return false; }
        size_t length = (size_t)(newline - at);
        if (!length || length >= WORKLOAD_LINE_MAX) { set_error(error, error_capacity, "graphics workload line is empty or too long"); return false; }
        char line[WORKLOAD_LINE_MAX];
        memcpy(line, at, length); line[length] = '\0';
        for (size_t i = 0; i < length; ++i)
            if ((unsigned char)line[i] < 0x20 || (unsigned char)line[i] == 0x7f) {
                set_error(error, error_capacity, "graphics workload line contains control bytes");
                return false;
            }
        at = newline + 1;
        if (!line_number) {
            if (strcmp(line, REVIEW_GRAPHICS_METRICS_INPUT_MAGIC)) {
                set_error(error, error_capacity, "graphics workload format version is invalid");
                return false;
            }
            continue;
        }
        char *separator = strchr(line, '|');
        if (!separator || strchr(separator + 1, '|')) {
            set_error(error, error_capacity, "graphics workload field is malformed");
            return false;
        }
        *separator++ = '\0';
        if (strcmp(line, workload_keys[field]) || !copy_text(values[field], sizeof(values[field]), separator)) {
            set_error(error, error_capacity, "graphics workload fields are missing, reordered, or duplicated");
            return false;
        }
        ++field;
    }
    if (field != GRAPHICS_METRIC_FIELDS || at != input + input_length) {
        set_error(error, error_capacity, "graphics workload input has unrecognized trailing bytes");
        return false;
    }

    uint64_t number;
    if (strcmp(values[0], REVIEW_GRAPHICS_METRICS_WORKLOAD) || strcmp(values[1], "test") ||
        !is_sha256(values[2]) || !copy_text(workload->baseline_image_sha256, sizeof(workload->baseline_image_sha256), values[2]) ||
        !is_sha256(values[3]) || !copy_text(workload->candidate_image_sha256, sizeof(workload->candidate_image_sha256), values[3]) ||
        !parse_u64(values[4], &number) || number != GRAPHICS_CLEAR_TEST_LOAD_ADDRESS ||
        !parse_u64(values[5], &number) || !number || number > GRAPHICS_CLEAR_TEST_IMAGE_CAPACITY) {
        set_error(error, error_capacity, "graphics workload identity or image bounds are invalid");
        return false;
    }
    workload->image.image_bytes = (size_t)number;
    uint16_t *addresses[] = { &workload->image.reset_entry, &workload->image.slice_entry,
        &workload->image.return_entry, &workload->image.cursor_address, &workload->image.remaining_address };
    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i) {
        if (!parse_u64(values[6 + i], &number) || number > UINT16_MAX ||
            number < GRAPHICS_CLEAR_TEST_LOAD_ADDRESS ||
            number >= GRAPHICS_CLEAR_TEST_LOAD_ADDRESS + workload->image.image_bytes) {
            set_error(error, error_capacity, "graphics workload entry or state address is outside the test image");
            return false;
        }
        *addresses[i] = (uint16_t)number;
    }
    if (!parse_u64(values[11], &number) || number != GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS ||
        !parse_u64(values[12], &number) || number != GRAPHICS_CLEAR_TEST_PRIORITY_BYTES ||
        !parse_u64(values[13], &number) || number != GRAPHICS_CLEAR_TEST_SLICE_COUNT ||
        !parse_u64(values[14], &number) || number != 0x44 ||
        !parse_u64(values[15], &number) || number != 0x5A || !is_sha256(values[16]) ||
        !copy_text(workload->toolchain_sha256, sizeof(workload->toolchain_sha256), values[16]) ||
        !copy_text(workload->workload, sizeof(workload->workload), values[0])) {
        set_error(error, error_capacity, "graphics workload constants or toolchain binding are invalid");
        return false;
    }
    return true;
}

static bool write_vram(const char *path, const uint8_t *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool failed = fwrite(bytes, 1, length, file) != length;
    if (fclose(file)) failed = true;
    return !failed;
}

int review_graphics_metrics_measure(const char *input_path, const char *image_path,
                                   const char *output_path, const char *workload_name,
                                   ReviewGraphicsMetricsMeasurement *measurement,
                                   char *error, size_t error_capacity)
{
    if (!input_path || !*input_path || !image_path || !*image_path || !output_path || !*output_path ||
        !workload_name || !*workload_name || !measurement) {
        set_error(error, error_capacity, "missing graphics metric runner argument");
        return -1;
    }
    memset(measurement, 0, sizeof(*measurement));
    GraphicsWorkload workload = {0};
    if (!parse_workload(input_path, &workload, error, error_capacity)) return -1;
    if (strcmp(workload_name, workload.workload)) {
        set_error(error, error_capacity, "graphics workload identity does not match the runner argument");
        return -1;
    }
    if (review_sha256_file(input_path, measurement->input_sha256)) {
        set_error(error, error_capacity, "cannot hash graphics workload input");
        return -1;
    }
    char image_sha256[65];
    if (review_sha256_file(image_path, image_sha256)) {
        set_error(error, error_capacity, "cannot hash linked graphics test image");
        return -1;
    }
    if (strcmp(image_sha256, workload.baseline_image_sha256) &&
        strcmp(image_sha256, workload.candidate_image_sha256)) {
        set_error(error, error_capacity, "linked image is not bound by this graphics workload input");
        return -1;
    }
    char actual_toolchain[65];
    if (review_graphics_metrics_toolchain_sha256(actual_toolchain, error, error_capacity)) return -1;
    if (strcmp(actual_toolchain, workload.toolchain_sha256)) {
        set_error(error, error_capacity, "graphics workload toolchain digest does not match the installed toolchain");
        return -1;
    }
    memcpy(measurement->toolchain_sha256, actual_toolchain, sizeof(actual_toolchain));

    uint8_t output_vram[GRAPHICS_CLEAR_TEST_VRAM_BYTES];
    uint32_t cycles = 0;
    uint64_t total_cycles = 0;
    int oracle = graphics_clear_run_test_image(image_path, &workload.image, output_vram,
                                               sizeof(output_vram), &cycles, &total_cycles,
                                               error, error_capacity);
    if (!cycles) {
        if (!error || !*error) set_error(error, error_capacity, "graphics workload produced no emulator cycles");
        return -1;
    }
    measurement->cycles = cycles;
    measurement->total_cycles = total_cycles;
    measurement->latency_ns = (uint64_t)cycles * 125;
    if (!write_vram(output_path, output_vram, sizeof(output_vram)) ||
        review_sha256_file(output_path, measurement->output_sha256)) {
        set_error(error, error_capacity, "cannot write or hash graphics output VRAM");
        return -1;
    }
    return oracle ? -1 : 0;
}

int review_graphics_metrics_cli(int argc, char **argv)
{
    if (argc != 9 || strcmp(argv[1], "--input") || strcmp(argv[3], "--image") ||
        strcmp(argv[5], "--output") || strcmp(argv[7], "--workload")) {
        fprintf(stderr, "review-graphics-metrics: expected --input FILE --image FILE --output FILE --workload ID\n");
        return EXIT_FAILURE;
    }
    ReviewGraphicsMetricsMeasurement measurement;
    char error[256] = {0};
    if (review_graphics_metrics_measure(argv[2], argv[4], argv[6], argv[8],
                                       &measurement, error, sizeof(error))) {
        fprintf(stderr, "review-graphics-metrics: %s\n", error[0] ? error : "graphics workload failed");
        return EXIT_FAILURE;
    }
    printf("review-metric-v2|%s|%s|%s|%" PRIu64 "|%" PRIu64 "|emulator-cycle-counter\n",
           argv[8], measurement.input_sha256, measurement.toolchain_sha256,
           measurement.cycles, measurement.latency_ns);
    return EXIT_SUCCESS;
}

#ifndef REVIEW_GRAPHICS_METRICS_TEST
int main(int argc, char **argv)
{
    return review_graphics_metrics_cli(argc, argv);
}
#else
#include "test_paths.h"

#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void path_in_output(char *destination, size_t capacity, const char *name)
{
    suite_test_path(destination, capacity, suite_output_directory, name);
}

static bool write_bytes(const char *path, const void *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool failed = fwrite(bytes, 1, length, file) != length;
    if (fclose(file)) failed = true;
    return !failed;
}

static bool copy_file(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    if (!input) return false;
    FILE *output = fopen(destination, "wb");
    if (!output) { fclose(input); return false; }
    unsigned char buffer[8192];
    bool failed = false;
    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), input);
        if (count && fwrite(buffer, 1, count, output) != count) { failed = true; break; }
        if (count < sizeof(buffer)) {
            if (ferror(input)) failed = true;
            break;
        }
    }
    if (fclose(input)) failed = true;
    if (fclose(output)) failed = true;
    return !failed;
}

static int run_program(char *const arguments[], const char *directory,
                       char *capture, size_t capacity)
{
    CHECK(capacity >= 2, "Command output buffer is too small");
    int descriptors[2]; CHECK(!pipe(descriptors), "Cannot create command capture pipe");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork command");
    if (!child) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0 || dup2(descriptors[1], STDERR_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        if (directory && chdir(directory)) _Exit(125);
        execvp(arguments[0], arguments);
        _Exit(127);
    }
    close(descriptors[1]);
    size_t used = 0;
    for (;;) {
        ssize_t count = read(descriptors[0], capture + used, capacity - used - 1);
        if (count < 0 && errno == EINTR) continue;
        CHECK(count >= 0, "Cannot read command output");
        if (!count) break;
        used += (size_t)count;
        CHECK(used < capacity - 1, "Command output is too long");
    }
    close(descriptors[0]); capture[used] = '\0';
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for command");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 128;
}

static int run_git(const char *repository, char *const arguments[],
                   char *capture, size_t capacity)
{
    char *argv[20];
    size_t count = 0;
    argv[count++] = (char *)"git";
    argv[count++] = (char *)"-C";
    argv[count++] = (char *)repository;
    for (size_t i = 0; arguments[i]; ++i) {
        CHECK(count + 1 < sizeof(argv) / sizeof(argv[0]), "Too many Git fixture arguments");
        argv[count++] = arguments[i];
    }
    argv[count] = NULL;
    return run_program(argv, NULL, capture, capacity);
}

static void git_fixture_ok(const char *repository, char *const arguments[], const char *message)
{
    char capture[4096] = {0};
    CHECK(!run_git(repository, arguments, capture, sizeof(capture)),
          capture[0] ? capture : message);
}

static void git_fixture_value(const char *repository, char *const arguments[], char value[65])
{
    char capture[256] = {0};
    CHECK(!run_git(repository, arguments, capture, sizeof(capture)),
          capture[0] ? capture : "Cannot read Git fixture identity");
    capture[strcspn(capture, "\r\n")] = '\0';
    size_t length = strlen(capture);
    CHECK(length == 40 || length == 64, "Git fixture identity has the wrong size");
    memcpy(value, capture, length + 1);
}

static unsigned char *read_bytes(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot read test image or VRAM output");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek test image or VRAM output");
    long size = ftell(file); CHECK(size >= 0 && size <= GRAPHICS_CLEAR_TEST_VRAM_BYTES, "Test artifact size is out of bounds");
    CHECK(!fseek(file, 0, SEEK_SET), "Cannot rewind test image or VRAM output");
    unsigned char *bytes = malloc((size_t)size ? (size_t)size : 1); CHECK(bytes, "Out of memory reading test artifact");
    CHECK(fread(bytes, 1, (size_t)size, file) == (size_t)size, "Cannot read test image or VRAM output");
    CHECK(!fclose(file), "Cannot close test artifact");
    *length = (size_t)size;
    return bytes;
}

static size_t find_bytes(const unsigned char *bytes, size_t length,
                         const unsigned char *needle, size_t needle_length)
{
    if (!needle_length || needle_length > length) return SIZE_MAX;
    for (size_t i = 0; i <= length - needle_length; ++i)
        if (!memcmp(bytes + i, needle, needle_length)) return i;
    return SIZE_MAX;
}

static void write_workload(const char *path, const GraphicsClearTestImage *image,
                           const char *baseline_sha, const char *candidate_sha,
                           const char *toolchain_sha)
{
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create graphics workload input");
    int result = fprintf(file,
        REVIEW_GRAPHICS_METRICS_INPUT_MAGIC "\n"
        "workload|%s\nimage_kind|test\nbaseline_image_sha256|%s\ncandidate_image_sha256|%s\n"
        "load_address|%u\nimage_bytes|%zu\nreset_entry|%u\nslice_entry|%u\nreturn_entry|%u\n"
        "cursor_address|%u\nremaining_address|%u\npriority_address|%u\npriority_bytes|%u\n"
        "slice_count|%u\nfill_byte|68\nsentinel_byte|90\ntoolchain_sha256|%s\n",
        REVIEW_GRAPHICS_METRICS_WORKLOAD, baseline_sha, candidate_sha,
        GRAPHICS_CLEAR_TEST_LOAD_ADDRESS, image->image_bytes, image->reset_entry, image->slice_entry,
        image->return_entry, image->cursor_address, image->remaining_address,
        GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS, GRAPHICS_CLEAR_TEST_PRIORITY_BYTES,
        GRAPHICS_CLEAR_TEST_SLICE_COUNT, toolchain_sha);
    CHECK(result > 0 && !ferror(file), "Cannot write graphics workload input");
    CHECK(!fclose(file), "Cannot close graphics workload input");
}

static void write_image_manifest(const char *path, const char *commit,
                                 const char *tree, const char *image_sha)
{
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create graphics image manifest");
    int result = fprintf(file, "review-image-v1\nkind|test\ncommit|%s\ntree|%s\nimage_sha|%s\n",
                         commit, tree, image_sha);
    CHECK(result > 0 && !ferror(file), "Cannot write graphics image manifest");
    CHECK(!fclose(file), "Cannot close graphics image manifest");
}

static bool report_field_equals(const char *report, const char *key, const char *expected)
{
    size_t key_length = strlen(key), expected_length = strlen(expected);
    for (const char *line = report; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        if ((size_t)(end - line) > key_length &&
            !strncmp(line, key, key_length) && line[key_length] == '|' &&
            (size_t)(end - (line + key_length + 1)) == expected_length &&
            !memcmp(line + key_length + 1, expected, expected_length)) return true;
        line = *end ? end + 1 : end;
    }
    return false;
}

static int run_gate(const char *gate, const char *repository, const char *input,
                    const char *baseline_image, const char *baseline_manifest,
                    const char *candidate_image, const char *candidate_manifest,
                    const char *runner, const char *scratch, const char *max_bytes,
                    const char *max_cycles, const char *max_latency,
                    char *capture, size_t capacity)
{
    char *arguments[] = { (char *)gate, (char *)"gate", (char *)"--repo", (char *)repository,
        (char *)"--input", (char *)input, (char *)"--workload", (char *)REVIEW_GRAPHICS_METRICS_WORKLOAD,
        (char *)"--baseline-image", (char *)baseline_image, (char *)"--baseline-manifest", (char *)baseline_manifest,
        (char *)"--candidate-image", (char *)candidate_image, (char *)"--candidate-manifest", (char *)candidate_manifest,
        (char *)"--baseline-runner", (char *)runner, (char *)"--candidate-runner", (char *)runner,
        (char *)"--proof-kind", (char *)"test", (char *)"--max-bytes", (char *)max_bytes,
        (char *)"--max-cycles", (char *)max_cycles, (char *)"--max-latency-ns", (char *)max_latency,
        (char *)"--scratch-dir", (char *)scratch, (char *)"--tradeoff", (char *)"review-metrics-tradeoff.txt", NULL };
    return run_program(arguments, NULL, capture, capacity);
}

static int run_runner(const char *runner, const char *input, const char *image,
                      const char *output, char *capture, size_t capacity)
{
    int descriptors[2]; CHECK(!pipe(descriptors), "Cannot create graphics runner capture pipe");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork graphics metric runner");
    if (!child) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0 || dup2(descriptors[1], STDERR_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        char *arguments[] = { (char *)runner, (char *)"--input", (char *)input,
            (char *)"--image", (char *)image, (char *)"--output", (char *)output,
            (char *)"--workload", (char *)REVIEW_GRAPHICS_METRICS_WORKLOAD, NULL };
        execv(runner, arguments);
        _Exit(127);
    }
    close(descriptors[1]);
    size_t used = 0;
    for (;;) {
        ssize_t count = read(descriptors[0], capture + used, capacity - used - 1);
        if (count < 0 && errno == EINTR) continue;
        CHECK(count >= 0, "Cannot read graphics runner output");
        if (!count) break;
        used += (size_t)count;
        CHECK(used < capacity - 1, "Graphics runner output is too long");
    }
    close(descriptors[0]); capture[used] = '\0';
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for graphics metric runner");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 128;
}

static uint64_t parse_decimal(const char *text)
{
    CHECK(*text, "Empty decimal metric field");
    uint64_t value = 0;
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at) {
        CHECK(isdigit(*at), "Malformed decimal metric field");
        CHECK(value <= (UINT64_MAX - (uint64_t)(*at - '0')) / 10, "Metric field overflows");
        value = value * 10 + (uint64_t)(*at - '0');
    }
    return value;
}

static uint64_t parse_report(char *capture, const char *input_sha, const char *toolchain_sha)
{
    size_t length = strlen(capture);
    CHECK(length && capture[length - 1] == '\n' && !strchr(capture, '\r'), "Metric report is not newline terminated");
    CHECK(!memchr(capture, '\n', length - 1), "Metric runner emitted more than one line");
    capture[length - 1] = '\0';
    char *fields[8]; size_t count = 0; fields[count++] = capture;
    for (char *at = capture; *at; ++at) if (*at == '|') {
        *at = '\0'; CHECK(count < 8, "Metric report has too many fields"); fields[count++] = at + 1;
    }
    CHECK(count == 7, "Metric report has the wrong field count");
    for (size_t i = 0; i < count; ++i) CHECK(*fields[i], "Metric report has an empty field");
    CHECK(!strcmp(fields[0], "review-metric-v2"), "Metric report version differs");
    CHECK(!strcmp(fields[1], REVIEW_GRAPHICS_METRICS_WORKLOAD), "Metric report workload differs");
    CHECK(!strcmp(fields[2], input_sha), "Metric report input hash differs");
    CHECK(!strcmp(fields[3], toolchain_sha), "Metric report toolchain hash differs");
    CHECK(!strcmp(fields[6], "emulator-cycle-counter"), "Metric report source differs");
    uint64_t cycles = parse_decimal(fields[4]);
    CHECK(cycles > 0 && parse_decimal(fields[5]) == cycles * 125,
          "Metric latency is not the deterministic emulated 8 MHz duration");
    return cycles;
}

static void check_full_vram(const unsigned char *vram, bool cleared)
{
    for (unsigned address = 0; address < GRAPHICS_CLEAR_TEST_VRAM_BYTES; ++address) {
        bool in_priority = address >= GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS &&
                           address < GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS + GRAPHICS_CLEAR_TEST_PRIORITY_BYTES;
        unsigned char expected = in_priority && cleared ? 0x44 : 0x5A;
        CHECK(vram[address] == expected, "Graphics metric output does not match exact VRAM workload");
    }
}

static void check_malformed_input(const char *input, const char *image,
                                  const char *output, const char *message)
{
    ReviewGraphicsMetricsMeasurement measurement;
    char error[256] = {0};
    CHECK(review_graphics_metrics_measure(input, image, output,
              REVIEW_GRAPHICS_METRICS_WORKLOAD, &measurement, error, sizeof(error)) != 0,
          message);
    CHECK(access(output, F_OK) != 0 && errno == ENOENT,
          "Rejected graphics input created an output artifact");
}

int main(int argc, char **argv)
{
    CHECK(argc == 3 && argv[1][0] == '/' && argv[2][0] == '/',
          "Pass the built review_metrics gate and graphics metric runner paths");
    suite_test_paths_init("review-graphics-metrics");
    char image_path[4096], candidate_path[4096], mutated_path[4096];
    char repository[4096], fixture_source[4096], repo_candidate_image[4096], repo_tradeoff[4096];
    char baseline_manifest[4096], candidate_manifest[4096], mutation_manifest[4096];
    char input_path[4096], mutated_input_path[4096], trailing_input_path[4096], nul_input_path[4096];
    char baseline_output[4096], candidate_output[4096], mutated_output[4096];
    char trailing_output[4096], nul_output[4096];
    char gate_report[8192], mutation_gate_report[8192];
    suite_test_path(repository, sizeof(repository), suite_fixture_directory, "metrics-git");
    CHECK(!mkdir(repository, 0700), "Cannot create owned Git fixture directory");
    suite_test_path(fixture_source, sizeof(fixture_source), repository, "graphicsAsm.s");
    suite_test_path(image_path, sizeof(image_path), repository, "graphics-baseline.bin");
    suite_test_path(repo_candidate_image, sizeof(repo_candidate_image), repository, "graphics-candidate.bin");
    suite_test_path(repo_tradeoff, sizeof(repo_tradeoff), repository, "review-metrics-tradeoff.txt");
    path_in_output(baseline_manifest, sizeof(baseline_manifest), "graphics-baseline.manifest");
    path_in_output(candidate_manifest, sizeof(candidate_manifest), "graphics-candidate.manifest");
    path_in_output(mutation_manifest, sizeof(mutation_manifest), "graphics-mutated.manifest");
    path_in_output(candidate_path, sizeof(candidate_path), "graphics-clear-candidate.bin");
    path_in_output(mutated_path, sizeof(mutated_path), "graphics-clear-mutated.bin");
    path_in_output(input_path, sizeof(input_path), "graphics-clear-workload.txt");
    path_in_output(mutated_input_path, sizeof(mutated_input_path), "graphics-clear-mutated-workload.txt");
    path_in_output(trailing_input_path, sizeof(trailing_input_path), "graphics-clear-trailing-workload.txt");
    path_in_output(nul_input_path, sizeof(nul_input_path), "graphics-clear-nul-workload.txt");
    path_in_output(baseline_output, sizeof(baseline_output), "graphics-clear-baseline-vram.bin");
    path_in_output(candidate_output, sizeof(candidate_output), "graphics-clear-candidate-vram.bin");
    path_in_output(mutated_output, sizeof(mutated_output), "graphics-clear-mutated-vram.bin");
    path_in_output(trailing_output, sizeof(trailing_output), "graphics-clear-trailing-vram.bin");
    path_in_output(nul_output, sizeof(nul_output), "graphics-clear-nul-vram.bin");

    char error[256] = {0}, image_sha[65], candidate_sha[65], toolchain_sha[65];
    GraphicsClearTestImage image;
    if (!copy_file("src/graphicsAsm.s", fixture_source)) {
        fprintf(stderr, "Cannot create fixture copy of production graphics assembly: cwd source=%d destination=%s errno=%d\n",
                access("src/graphicsAsm.s", R_OK), fixture_source, errno);
        exit(EXIT_FAILURE);
    }
    char *init_arguments[] = { (char *)"git", (char *)"init", (char *)"--quiet", repository, NULL };
    char git_capture[4096] = {0};
    CHECK(!run_program(init_arguments, NULL, git_capture, sizeof(git_capture)),
          git_capture[0] ? git_capture : "Cannot initialize owned Git fixture");
    char *config_name[] = { (char *)"config", (char *)"user.name", (char *)"Graphics Metrics Test", NULL };
    char *config_email[] = { (char *)"config", (char *)"user.email", (char *)"graphics-metrics-test@example.invalid", NULL };
    git_fixture_ok(repository, config_name, "Cannot configure owned Git fixture author");
    git_fixture_ok(repository, config_email, "Cannot configure owned Git fixture email");
    CHECK(!graphics_clear_export_test_image(fixture_source, image_path, &image, error, sizeof(error)),
          error[0] ? error : "Could not export linked graphics assembly test image");
    CHECK(image.image_bytes > 0 && image.image_bytes <= GRAPHICS_CLEAR_TEST_IMAGE_CAPACITY,
          "Exported graphics test image is outside the bounded fixture size");
    CHECK(!review_sha256_file(image_path, image_sha), "Cannot hash exported graphics test image");
    char *add_baseline[] = { (char *)"add", (char *)"--", (char *)"graphicsAsm.s", (char *)"graphics-baseline.bin", NULL };
    git_fixture_ok(repository, add_baseline, "Cannot stage linked baseline graphics fixture");
    char *commit_baseline[] = { (char *)"commit", (char *)"--quiet", (char *)"-m", (char *)"Add linked graphics test image", NULL };
    git_fixture_ok(repository, commit_baseline, "Cannot commit linked baseline graphics fixture");
    char *head[] = { (char *)"rev-parse", (char *)"--verify", (char *)"HEAD", NULL };
    char *tree[] = { (char *)"rev-parse", (char *)"--verify", (char *)"HEAD^{tree}", NULL };
    char baseline_commit[65], baseline_tree[65], candidate_commit[65], candidate_tree[65];
    git_fixture_value(repository, head, baseline_commit);
    git_fixture_value(repository, tree, baseline_tree);
    if (!copy_file(image_path, repo_candidate_image)) {
        fprintf(stderr, "Cannot create candidate image in Git fixture: source=%d destination=%s errno=%d\n",
                access(image_path, R_OK), repo_candidate_image, errno);
        exit(EXIT_FAILURE);
    }
    static const char tradeoff[] =
        "review-metrics-tradeoff-v1\n"
        "review_reference|native regression calibration\n"
        "reason|Byte-identical test images validate the integrated gate and workload; no performance improvement is claimed.\n";
    CHECK(write_bytes(repo_tradeoff, tradeoff, sizeof(tradeoff) - 1), "Cannot write neutral calibration proposal");
    char *add_candidate[] = { (char *)"add", (char *)"--", (char *)"graphics-candidate.bin",
        (char *)"review-metrics-tradeoff.txt", NULL };
    git_fixture_ok(repository, add_candidate, "Cannot stage candidate graphics fixture and proposal");
    char *commit_candidate[] = { (char *)"commit", (char *)"--quiet", (char *)"-m", (char *)"Record equivalent test candidate", NULL };
    git_fixture_ok(repository, commit_candidate, "Cannot commit candidate graphics fixture");
    git_fixture_value(repository, head, candidate_commit);
    git_fixture_value(repository, tree, candidate_tree);
    write_image_manifest(baseline_manifest, baseline_commit, baseline_tree, image_sha);
    write_image_manifest(candidate_manifest, candidate_commit, candidate_tree, image_sha);
    size_t copied_bytes;
    unsigned char *image_copy = read_bytes(image_path, &copied_bytes);
    CHECK(copied_bytes == image.image_bytes, "Exported test image size changed while copying");
    CHECK(write_bytes(candidate_path, image_copy, copied_bytes), "Cannot create byte-identical candidate image");
    free(image_copy);
    CHECK(!review_sha256_file(candidate_path, candidate_sha) && !strcmp(image_sha, candidate_sha),
          "Candidate test image differs from the baseline image");
    CHECK(!review_graphics_metrics_toolchain_sha256(toolchain_sha, error, sizeof(error)),
          error[0] ? error : "Cannot hash pinned graphics test toolchain");
    write_workload(input_path, &image, image_sha, candidate_sha, toolchain_sha);

    size_t input_bytes;
    unsigned char *valid_input = read_bytes(input_path, &input_bytes);
    static const unsigned char trailing_bytes[] = "trailing|bytes\n";
    unsigned char *trailing_input = malloc(input_bytes + sizeof(trailing_bytes) - 1);
    CHECK(trailing_input, "Out of memory creating trailing-byte workload control");
    memcpy(trailing_input, valid_input, input_bytes);
    memcpy(trailing_input + input_bytes, trailing_bytes, sizeof(trailing_bytes) - 1);
    CHECK(write_bytes(trailing_input_path, trailing_input, input_bytes + sizeof(trailing_bytes) - 1),
          "Cannot write trailing-byte workload control");
    free(trailing_input);
    check_malformed_input(trailing_input_path, image_path, trailing_output,
                          "Trailing workload bytes were accepted");

    static const unsigned char sentinel_field[] = "sentinel_byte|90\n";
    size_t sentinel_offset = find_bytes(valid_input, input_bytes, sentinel_field, sizeof(sentinel_field) - 1);
    CHECK(sentinel_offset != SIZE_MAX, "Cannot locate sentinel field for embedded-NUL control");
    valid_input[sentinel_offset + sizeof("sentinel_byte|") - 1] = '\0';
    CHECK(write_bytes(nul_input_path, valid_input, input_bytes),
          "Cannot write embedded-NUL workload control");
    free(valid_input);
    check_malformed_input(nul_input_path, image_path, nul_output,
                          "Embedded-NUL workload input was accepted");

    char baseline_report[2048], candidate_report[2048];
    CHECK(!run_runner(argv[2], input_path, image_path, baseline_output, baseline_report, sizeof(baseline_report)),
          baseline_report[0] ? baseline_report : "Baseline graphics metric runner failed");
    CHECK(!run_runner(argv[2], input_path, candidate_path, candidate_output, candidate_report, sizeof(candidate_report)),
          candidate_report[0] ? candidate_report : "Candidate graphics metric runner failed");
    char input_sha[65]; CHECK(!review_sha256_file(input_path, input_sha), "Cannot hash graphics workload input");
    uint64_t baseline_cycles = parse_report(baseline_report, input_sha, toolchain_sha);
    uint64_t candidate_cycles = parse_report(candidate_report, input_sha, toolchain_sha);
    CHECK(baseline_cycles == candidate_cycles, "Identical linked test images changed measured cycle counts");
    ReviewGraphicsMetricsMeasurement baseline_measurement = {0};
    memset(error, 0, sizeof(error));
    CHECK(!review_graphics_metrics_measure(input_path, image_path, baseline_output,
              REVIEW_GRAPHICS_METRICS_WORKLOAD, &baseline_measurement, error, sizeof(error)),
          error[0] ? error : "Could not retrieve total graphics workload cycles");
    CHECK(baseline_measurement.cycles == baseline_cycles &&
          baseline_measurement.total_cycles >= baseline_measurement.cycles &&
          baseline_measurement.latency_ns == baseline_measurement.cycles * UINT64_C(125),
          "Direct graphics measurement disagrees with the runner's worst-slice metric");
    size_t baseline_bytes, candidate_bytes;
    unsigned char *baseline_vram = read_bytes(baseline_output, &baseline_bytes);
    unsigned char *candidate_vram = read_bytes(candidate_output, &candidate_bytes);
    CHECK(baseline_bytes == GRAPHICS_CLEAR_TEST_VRAM_BYTES && candidate_bytes == baseline_bytes,
          "Metric runner output is not the complete VERA VRAM image");
    CHECK(!memcmp(baseline_vram, candidate_vram, baseline_bytes), "Equivalent image output VRAM differs");
    check_full_vram(baseline_vram, true);
    char baseline_output_sha[65];
    CHECK(!review_sha256_file(baseline_output, baseline_output_sha), "Cannot hash baseline VRAM output");
    char candidate_output_sha[65];
    CHECK(!review_sha256_file(candidate_output, candidate_output_sha), "Cannot hash candidate VRAM output");
    CHECK(!strcmp(baseline_output_sha, baseline_measurement.output_sha256) &&
          !strcmp(baseline_output_sha, candidate_output_sha),
          "Independent metric runner and direct output hashes differ");
    free(candidate_vram);

    char max_bytes[32], max_cycles[32], max_latency[32], expected_number[64];
    CHECK(snprintf(max_bytes, sizeof(max_bytes), "%zu", image.image_bytes) < (int)sizeof(max_bytes) &&
          snprintf(max_cycles, sizeof(max_cycles), "%" PRIu64, baseline_measurement.cycles) < (int)sizeof(max_cycles) &&
          snprintf(max_latency, sizeof(max_latency), "%" PRIu64, baseline_measurement.latency_ns) < (int)sizeof(max_latency),
          "Graphics metric budget value is too long");
    CHECK(!run_gate(argv[1], repository, input_path, image_path, baseline_manifest,
                    repo_candidate_image, candidate_manifest, argv[2], suite_output_directory,
                    max_bytes, max_cycles, max_latency, gate_report, sizeof(gate_report)),
          gate_report[0] ? gate_report : "Actual review_metrics gate rejected equivalent graphics test images");
    CHECK(report_field_equals(gate_report, "workload", REVIEW_GRAPHICS_METRICS_WORKLOAD) &&
          report_field_equals(gate_report, "input_sha256", input_sha) &&
          report_field_equals(gate_report, "baseline_commit", baseline_commit) &&
          report_field_equals(gate_report, "baseline_tree", baseline_tree) &&
          report_field_equals(gate_report, "candidate_commit", candidate_commit) &&
          report_field_equals(gate_report, "candidate_tree", candidate_tree),
          "Actual review_metrics gate reported different fixture identities");
    CHECK(snprintf(expected_number, sizeof(expected_number), "%zu", image.image_bytes) < (int)sizeof(expected_number),
          "Graphics image size does not fit the gate result field");
    CHECK(report_field_equals(gate_report, "baseline_bytes", expected_number) &&
          report_field_equals(gate_report, "candidate_bytes", expected_number),
          "Actual review_metrics gate reported different linked image sizes");
    CHECK(snprintf(expected_number, sizeof(expected_number), "%" PRIu64, baseline_measurement.cycles) < (int)sizeof(expected_number),
          "Graphics cycle count does not fit the gate result field");
    CHECK(report_field_equals(gate_report, "baseline_cycles", expected_number) &&
          report_field_equals(gate_report, "candidate_cycles", expected_number),
          "Actual review_metrics gate reported cycles different from the emulator workload");
    CHECK(snprintf(expected_number, sizeof(expected_number), "%" PRIu64, baseline_measurement.latency_ns) < (int)sizeof(expected_number),
          "Graphics latency does not fit the gate result field");
    CHECK(report_field_equals(gate_report, "baseline_latency_ns", expected_number) &&
          report_field_equals(gate_report, "candidate_latency_ns", expected_number),
          "Actual review_metrics gate reported latency different from the emulator workload");
    CHECK(report_field_equals(gate_report, "baseline_output_sha256", baseline_output_sha) &&
          report_field_equals(gate_report, "candidate_output_sha256", baseline_output_sha) &&
          report_field_equals(gate_report, "baseline_image_sha256", image_sha) &&
          report_field_equals(gate_report, "candidate_image_sha256", candidate_sha) &&
          report_field_equals(gate_report, "decision", "documented tradeoff proposal"),
          "Actual review_metrics gate did not preserve measured output and image identities");

    size_t mutated_size;
    unsigned char *mutated = read_bytes(image_path, &mutated_size);
    size_t opcode_offset = (size_t)(image.slice_entry - GRAPHICS_CLEAR_TEST_LOAD_ADDRESS);
    CHECK(mutated_size == image.image_bytes && opcode_offset < mutated_size && mutated[opcode_offset] == 0x20,
          "Linked slice entry is not the expected production JSR instruction");
    mutated[opcode_offset] = 0x60;
    CHECK(write_bytes(mutated_path, mutated, mutated_size), "Cannot write real-instruction mutation image");
    free(mutated); free(baseline_vram);
    CHECK(!review_sha256_file(mutated_path, candidate_sha) && strcmp(candidate_sha, image_sha),
          "Instruction mutation did not change the linked image digest");
    write_workload(mutated_input_path, &image, image_sha, candidate_sha, toolchain_sha);
    write_image_manifest(mutation_manifest, candidate_commit, candidate_tree, candidate_sha);

    ReviewGraphicsMetricsMeasurement mutation = {0};
    memset(error, 0, sizeof(error));
    CHECK(review_graphics_metrics_measure(mutated_input_path, mutated_path, mutated_output,
              REVIEW_GRAPHICS_METRICS_WORKLOAD, &mutation, error, sizeof(error)) != 0,
          "Real linked instruction mutation unexpectedly passed the VRAM oracle");
    CHECK(strstr(error, "failed exact VRAM oracle") != NULL && mutation.cycles > 0 &&
          mutation.cycles != baseline_cycles &&
          mutation.total_cycles != baseline_measurement.total_cycles,
          "Mutated instruction did not fail the VRAM oracle with changed measured cycles");
    size_t mutated_vram_bytes;
    unsigned char *mutated_vram = read_bytes(mutated_output, &mutated_vram_bytes);
    CHECK(mutated_vram_bytes == GRAPHICS_CLEAR_TEST_VRAM_BYTES,
          "Mutated image output artifact is missing or malformed");
    check_full_vram(mutated_vram, false);
    CHECK(mutated_vram[GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS] == 0x5A,
          "RTS mutation unexpectedly wrote priority VRAM");
    CHECK(strcmp(mutation.output_sha256, baseline_output_sha), "Mutated output VRAM hash did not change");
    free(mutated_vram);

    memset(mutation_gate_report, 0, sizeof(mutation_gate_report));
    CHECK(run_gate(argv[1], repository, mutated_input_path, image_path, baseline_manifest,
                   mutated_path, mutation_manifest, argv[2], suite_output_directory,
                   max_bytes, max_cycles, max_latency, mutation_gate_report,
                   sizeof(mutation_gate_report)) != 0 &&
          strstr(mutation_gate_report, "metric runner did not complete successfully") != NULL,
          "Actual review_metrics gate accepted a real instruction mutation that failed VRAM");

    printf("Graphics metrics integration: %zu linked test-image bytes, %" PRIu64
           " total workload cycles, %" PRIu64 " worst-slice cycles, %" PRIu64
           " derived ns; actual review_metrics gate accepted the equivalent pair and rejected the RTS mutation; malformed inputs rejected\n",
           image.image_bytes, baseline_measurement.total_cycles, baseline_cycles,
           baseline_cycles * UINT64_C(125));
    return EXIT_SUCCESS;
}
#endif
