#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "review_sha256.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

#ifdef REVIEW_METRICS_FIXTURE_RUNNER
static void runner_fail(const char *message)
{
    fprintf(stderr, "metrics fixture runner: %s\n", message);
    exit(2);
}

static const char *runner_option(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], name)) return argv[i + 1];
    return NULL;
}

static void runner_read_image(const char *path, char *cycles, size_t cycles_capacity,
                              char *latency, size_t latency_capacity,
                              char *source, size_t source_capacity, char *output, size_t output_capacity,
                              char *reported_workload, size_t workload_capacity,
                              char *reported_input, size_t input_capacity,
                              char *reported_toolchain, size_t toolchain_capacity,
                              char *runner_mode, size_t runner_mode_capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file) runner_fail("cannot open image");
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *separator = strchr(line, '|');
        if (!separator) continue;
        *separator++ = '\0';
        char *destination = NULL;
        size_t capacity = 0;
        if (!strcmp(line, "cycles")) { destination = cycles; capacity = cycles_capacity; }
        else if (!strcmp(line, "latency_ns") || !strcmp(line, "report_latency_ns")) {
            destination = latency;
            capacity = latency_capacity;
        }
        else if (!strcmp(line, "source")) { destination = source; capacity = source_capacity; }
        else if (!strcmp(line, "output")) { destination = output; capacity = output_capacity; }
        else if (!strcmp(line, "report_workload")) { destination = reported_workload; capacity = workload_capacity; }
        else if (!strcmp(line, "report_input")) { destination = reported_input; capacity = input_capacity; }
        else if (!strcmp(line, "toolchain_sha256") || !strcmp(line, "report_toolchain")) {
            destination = reported_toolchain;
            capacity = toolchain_capacity;
        }
        else if (!strcmp(line, "runner_mode")) { destination = runner_mode; capacity = runner_mode_capacity; }
        if (destination) {
            if (strlen(separator) >= capacity) runner_fail("image metric field is too long");
            strcpy(destination, separator);
        }
    }
    if (ferror(file) || fclose(file)) runner_fail("cannot read image");
}

int main(int argc, char **argv)
{
    const char *input = runner_option(argc, argv, "--input");
    const char *image = runner_option(argc, argv, "--image");
    const char *output_path = runner_option(argc, argv, "--output");
    const char *workload = runner_option(argc, argv, "--workload");
    if (argc != 9 || !input || !image || !output_path || !workload) runner_fail("invalid argv contract");

    char cycles[128] = { 0 }, latency[128] = { 0 }, source[128] = { 0 }, output[512] = { 0 };
    char reported_workload[128] = { 0 }, reported_input[65] = { 0 }, reported_toolchain[65] = { 0 };
    char runner_mode[32] = { 0 };
    char input_digest[65];
    runner_read_image(image, cycles, sizeof(cycles), latency, sizeof(latency), source, sizeof(source),
                      output, sizeof(output), reported_workload, sizeof(reported_workload),
                      reported_input, sizeof(reported_input), reported_toolchain, sizeof(reported_toolchain),
                      runner_mode, sizeof(runner_mode));
    if (!cycles[0] || !latency[0] || !source[0] || !output[0] || !reported_toolchain[0] ||
        review_sha256_file(input, input_digest))
        runner_fail("image or input is incomplete");
    if (!reported_workload[0]) strcpy(reported_workload, workload);
    if (!reported_input[0]) strcpy(reported_input, input_digest);

    if (!strcmp(runner_mode, "hang")) for (;;) pause();
    if (!strcmp(runner_mode, "mutate-input")) {
        FILE *mutated = fopen(input, "wb");
        if (!mutated || fputs("runner changed the supplied workload\n", mutated) < 0 || fclose(mutated))
            runner_fail("cannot mutate fixture workload input");
    }

    FILE *file = fopen(output_path, "wb");
    if (!file) runner_fail("cannot create execution output");
    size_t length = strlen(output);
    if (fwrite(output, 1, length, file) != length || fclose(file)) runner_fail("cannot write execution output");
#ifdef METRICS_RUNNER_VARIANT
    fputs("alternate runner pin\n", stderr);
#endif
    if (!strcmp(runner_mode, "oversize")) {
    for (size_t i = 0; i < 65537; ++i) {
        if (putchar('x') == EOF) runner_fail("cannot emit oversized evidence");
        if ((i & 1023u) == 1023u) fflush(stdout);
    }
    fflush(stdout);
    return EXIT_SUCCESS;
    }
    if (runner_mode[0] && strcmp(runner_mode, "mutate-input") && strcmp(runner_mode, "nul-suffix"))
        runner_fail("unknown fixture runner mode");
    printf("review-metric-v2|%s|%s|%s|%s|%s|%s%s",
           reported_workload, reported_input, reported_toolchain, cycles, latency, source,
           !strcmp(runner_mode, "nul-suffix") ? "" : "\n");
    if (!strcmp(runner_mode, "nul-suffix") && fwrite("\0junk\n", 1, 6, stdout) != 6)
        runner_fail("cannot emit suffixed evidence regression");
    return EXIT_SUCCESS;
}
#else

#include "test_paths.h"
#include "test_run_from.h"

static char output_root[4096];
static char project[4096];
static char input_path[4096];
static char baseline_image[4096];
static char candidate_image[4096];
static char baseline_manifest[4096];
static char candidate_manifest[4096];
static char tradeoff_path[4096];
static char local_approval_path[4096];
static char output_log[4096];

static void cleanup_output(void)
{
    if (output_root[0] && !suite_test_paths_remove(output_root)) {
        fprintf(stderr, "Could not clean review metrics output %s\n", output_root);
        _Exit(EXIT_FAILURE);
    }
}

static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create review metrics fixture input");
    size_t length = strlen(text);
    CHECK(fwrite(text, 1, length, file) == length, "Cannot write review metrics fixture input");
    CHECK(!fclose(file), "Cannot close review metrics fixture input");
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL, "Cannot read review metrics output");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek review metrics output");
    long size = ftell(file);
    CHECK(size >= 0 && size < 1024 * 1024, "Review metrics output has an unexpected size");
    CHECK(!fseek(file, 0, SEEK_SET), "Cannot rewind review metrics output");
    char *text = calloc((size_t)size + 1, 1);
    CHECK(text != NULL, "Cannot allocate review metrics output");
    CHECK(fread(text, 1, (size_t)size, file) == (size_t)size, "Cannot read review metrics output");
    CHECK(!fclose(file), "Cannot close review metrics output");
    return text;
}

static void git_ok(char *const argv[])
{
    run_from(argv, NULL, 10, NULL);
}

static void git_project_ok(char *const args[])
{
    char *argv[24];
    size_t count = 0;
    argv[count++] = (char *)"git";
    argv[count++] = (char *)"-C";
    argv[count++] = project;
    for (size_t i = 0; args[i]; ++i) {
        CHECK(count + 1 < sizeof(argv) / sizeof(argv[0]), "Too many fixture Git arguments");
        argv[count++] = args[i];
    }
    argv[count] = NULL;
    git_ok(argv);
}

static void git_commit(const char *message)
{
    char *add[] = { (char *)"add", (char *)"--all", NULL };
    git_project_ok(add);
    char *commit[] = { (char *)"-c", (char *)"user.name=Metrics Product Author",
        (char *)"-c", (char *)"user.email=metrics-author@example.invalid",
        (char *)"-c", (char *)"core.hooksPath=/dev/null", (char *)"commit", (char *)"--quiet",
        (char *)"-m", (char *)message, NULL };
    git_project_ok(commit);
}

static void git_value(const char *revision, char *value, size_t capacity)
{
    int descriptors[2];
    CHECK(!pipe(descriptors), "Cannot create fixture Git identity pipe");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork fixture Git identity command");
    if (!child) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0) _Exit(125);
        close(descriptors[1]);
        char *argv[] = { (char *)"git", (char *)"-C", project, (char *)"rev-parse",
            (char *)"--verify", (char *)revision, NULL };
        execvp(argv[0], argv);
        _Exit(127);
    }
    close(descriptors[1]);
    size_t used = 0;
    while (used + 1 < capacity) {
        ssize_t count = read(descriptors[0], value + used, capacity - used - 1);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += (size_t)count;
    }
    CHECK(used > 0 && used + 1 < capacity, "Cannot read fixture Git identity");
    value[used] = '\0';
    CHECK(!close(descriptors[0]), "Cannot close fixture Git identity pipe");
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status),
          "Cannot resolve fixture Git identity");
    value[strcspn(value, "\r\n")] = '\0';
    CHECK(strlen(value) == 40, "Fixture Git identity is not a full SHA-1");
}

static void write_image(const char *path, const char *cycles, const char *source,
                        const char *output, size_t padding, const char *overrides)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create metrics image fixture");
    CHECK(fprintf(file, "cycles|%s\nlatency_ns|1000\ntoolchain_sha256|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                  "source|%s\noutput|%s\n%s", cycles, source, output,
                  overrides ? overrides : "") > 0, "Cannot write metrics image fixture");
    for (size_t i = 0; i < padding; ++i) CHECK(fputc('x', file) != EOF, "Cannot pad metrics image fixture");
    CHECK(!fclose(file), "Cannot close metrics image fixture");
}

static void write_image_manifest(const char *path, const char *kind, const char *revision,
                                 const char *tree, const char *image)
{
    char digest[65];
    CHECK(!review_sha256_file(image, digest), "Cannot hash metrics image fixture");
    char text[512];
    int length = snprintf(text, sizeof(text),
        "review-image-v1\nkind|%s\ncommit|%s\ntree|%s\nimage_sha|%s\n",
        kind, revision, tree, digest);
    CHECK(length > 0 && length < (int)sizeof(text), "Metrics image manifest is too large");
    write_text(path, text);
}

static uint64_t test_monotonic_milliseconds(void)
{
    struct timespec now;
    CHECK(!clock_gettime(CLOCK_MONOTONIC, &now), "Cannot read monotonic test clock");
    return (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec / UINT64_C(1000000);
}

static int run_gate_with_limits(const char *tool, const char *baseline_runner, const char *candidate_runner,
                                const char *proof_kind, const char *max_bytes, const char *max_cycles,
                                const char *max_latency, const char *timeout, const char *tradeoff)
{
    char *argv[44];
    size_t count = 0;
    argv[count++] = (char *)tool;
    argv[count++] = (char *)"gate";
    argv[count++] = (char *)"--repo"; argv[count++] = project;
    argv[count++] = (char *)"--input"; argv[count++] = input_path;
    argv[count++] = (char *)"--workload"; argv[count++] = (char *)"addition-contract";
    argv[count++] = (char *)"--baseline-image"; argv[count++] = baseline_image;
    argv[count++] = (char *)"--baseline-manifest"; argv[count++] = baseline_manifest;
    argv[count++] = (char *)"--candidate-image"; argv[count++] = candidate_image;
    argv[count++] = (char *)"--candidate-manifest"; argv[count++] = candidate_manifest;
    argv[count++] = (char *)"--baseline-runner"; argv[count++] = (char *)baseline_runner;
    argv[count++] = (char *)"--candidate-runner"; argv[count++] = (char *)candidate_runner;
    argv[count++] = (char *)"--proof-kind"; argv[count++] = (char *)proof_kind;
    if (max_bytes) { argv[count++] = (char *)"--max-bytes"; argv[count++] = (char *)max_bytes; }
    if (max_cycles) { argv[count++] = (char *)"--max-cycles"; argv[count++] = (char *)max_cycles; }
    if (max_latency) { argv[count++] = (char *)"--max-latency-ns"; argv[count++] = (char *)max_latency; }
    if (timeout) { argv[count++] = (char *)"--runner-timeout-ms"; argv[count++] = (char *)timeout; }
    argv[count++] = (char *)"--scratch-dir"; argv[count++] = output_root;
    if (tradeoff) { argv[count++] = (char *)"--tradeoff"; argv[count++] = (char *)tradeoff; }
    argv[count] = NULL;
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork review metrics CLI");
    if (!child) {
        (void)setpgid(0, 0);
        int descriptor = open(output_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
            dup2(descriptor, STDERR_FILENO) < 0) _Exit(125);
        close(descriptor);
        execv(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute %s: %s\n", argv[0], strerror(errno));
        _Exit(127);
    }
    (void)setpgid(child, child);
    uint64_t deadline = test_monotonic_milliseconds() + UINT64_C(5000);
    int status = 0;
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        CHECK(waited >= 0 || errno == EINTR, "Cannot wait for review metrics CLI");
        if (test_monotonic_milliseconds() >= deadline) {
            (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            (void)waitpid(child, &status, 0);
            fprintf(stderr, "review_metrics suite watchdog expired\n");
            return 124;
        }
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 10000000 };
        while (nanosleep(&pause, &pause) < 0 && errno == EINTR) { }
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

static int run_gate_with_latency(const char *tool, const char *baseline_runner, const char *candidate_runner,
                                 const char *proof_kind, const char *max_bytes, const char *max_cycles,
                                 const char *max_latency, const char *tradeoff)
{
    return run_gate_with_limits(tool, baseline_runner, candidate_runner, proof_kind, max_bytes,
                                max_cycles, max_latency, "10000", tradeoff);
}

static int run_gate(const char *tool, const char *baseline_runner, const char *candidate_runner,
                    const char *proof_kind, const char *max_bytes, const char *max_cycles,
                    const char *tradeoff)
{
    return run_gate_with_latency(tool, baseline_runner, candidate_runner, proof_kind,
                                 max_bytes, max_cycles, "5000", tradeoff);
}

static void expect_rejected_pair(const char *tool, const char *baseline_runner, const char *candidate_runner,
                                 const char *kind,
                            const char *max_bytes, const char *max_cycles,
                            const char *tradeoff, const char *reason)
{
    CHECK(run_gate(tool, baseline_runner, candidate_runner, kind, max_bytes, max_cycles, tradeoff) != 0,
          "review_metrics accepted incomparable or invalid execution evidence");
    char *log = read_text(output_log);
    if (!strstr(log, reason)) fprintf(stderr, "review_metrics diagnostic: %s\n", log);
    CHECK(strstr(log, reason) != NULL, "review_metrics returned the wrong rejection reason");
    free(log);
}

static void expect_rejected(const char *tool, const char *runner, const char *kind,
                            const char *max_bytes, const char *max_cycles,
                            const char *tradeoff, const char *reason)
{
    expect_rejected_pair(tool, runner, runner, kind, max_bytes, max_cycles, tradeoff, reason);
}

int main(int argc, char **argv)
{
    CHECK(argc == 4, "Expected review metrics CLI, fixture runner, and alternate runner paths");
    suite_test_paths_init("review-metrics");
    CHECK(snprintf(output_root, sizeof(output_root), "%s/review-metrics-output-XXXXXX", suite_output_directory) <
          (int)sizeof(output_root), "Review metrics output path is too long");
    CHECK(mkdtemp(output_root) != NULL, "Cannot create review metrics output directory");
    CHECK(!atexit(cleanup_output), "Cannot register review metrics output cleanup");
    suite_test_path(project, sizeof(project), suite_fixture_directory, "git-project");
    suite_test_path(input_path, sizeof(input_path), suite_fixture_directory, "workload.input");
    suite_test_path(baseline_image, sizeof(baseline_image), suite_fixture_directory, "baseline.img");
    suite_test_path(candidate_image, sizeof(candidate_image), suite_fixture_directory, "candidate.img");
    suite_test_path(baseline_manifest, sizeof(baseline_manifest), suite_fixture_directory, "baseline.image-v1");
    suite_test_path(candidate_manifest, sizeof(candidate_manifest), suite_fixture_directory, "candidate.image-v1");
    suite_test_path(tradeoff_path, sizeof(tradeoff_path), project, "metrics-tradeoff.txt");
    suite_test_path(local_approval_path, sizeof(local_approval_path), project, "metrics-local-approval.txt");
    suite_test_path(output_log, sizeof(output_log), output_root, "metrics.log");

    char *init[] = { (char *)"git", (char *)"init", (char *)"--quiet", (char *)"--initial-branch=main", project, NULL };
    git_ok(init);
    char path[4096];
    suite_test_path(path, sizeof(path), project, "product.txt");
    write_text(path, "baseline product\n");
    git_commit("baseline product");
    char baseline[64], baseline_tree[64], candidate[64], candidate_tree[64];
    git_value("HEAD", baseline, sizeof(baseline));
    char revision[80];
    CHECK(snprintf(revision, sizeof(revision), "%s^{tree}", baseline) < (int)sizeof(revision),
          "Baseline tree revision is too long");
    git_value(revision, baseline_tree, sizeof(baseline_tree));

    suite_test_path(path, sizeof(path), project, "product.txt");
    write_text(path, "candidate product\n");
    write_text(tradeoff_path,
        "review-metrics-tradeoff-v1\n"
        "review_reference|PR-42-review-7\n"
        "reason|Proposed space-time tradeoff for the recorded workload\n");
    write_text(local_approval_path,
        "review-metrics-tradeoff-v1\n"
        "review_reference|PR-42-review-7\n"
        "reason|Proposed space-time tradeoff for the recorded workload\n"
        "locally_approved|true\n");
    git_commit("candidate product and documented tradeoff proposal");
    git_value("HEAD", candidate, sizeof(candidate));
    CHECK(snprintf(revision, sizeof(revision), "%s^{tree}", candidate) < (int)sizeof(revision),
          "Candidate tree revision is too long");
    git_value(revision, candidate_tree, sizeof(candidate_tree));

    write_text(input_path, "addition workload input\n");
    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128,
                "report_latency_ns|1300\n");
    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);

    int status = run_gate(argv[1], argv[2], argv[2], "release", "4096", "500", NULL);
    if (status) {
        char *log = read_text(output_log);
        fprintf(stderr, "review_metrics exited %d: %s\n", status, log);
        free(log);
    }
    CHECK(status == 0, "review_metrics rejected an execution-derived local Pareto improvement");
    char *report = read_text(output_log);
    CHECK(strstr(report, "local Pareto improvement") && strstr(report, "image_kind|release") &&
          strstr(report, "image manifest commit/tree declarations do not establish source-to-image derivation") &&
          strstr(report, "no independent measurement attestation, repository-owner approval, global optimality, or fresh verification claim"),
          "review_metrics overstated the scope or source provenance of the local comparison");
    CHECK(strstr(report, "runner_reported_toolchain_sha256|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") &&
          strstr(report, "baseline_latency_ns|1300") && strstr(report, "candidate_latency_ns|1000") &&
          strstr(report, "runner_timeout_ms|10000") && strstr(report, "max_latency_ns|5000"),
          "review_metrics did not report the pinned toolchain and measured latency comparison");
    free(report);

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32,
                "report_latency_ns|1400\n");
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    CHECK(run_gate(argv[1], argv[2], argv[2], "release", "4096", "500", NULL) != 0,
          "review_metrics accepted a latency regression without tradeoff proposal provenance");
    report = read_text(output_log);
    CHECK(strstr(report, "candidate does not locally Pareto-improve; documented tradeoff proposal provenance is required"),
          "review_metrics did not include latency in the Pareto comparison");
    free(report);
    CHECK(run_gate(argv[1], argv[2], argv[2], "release", "4096", "500", "metrics-tradeoff.txt") == 0,
          "review_metrics rejected documented provenance for a measured latency tradeoff");
    report = read_text(output_log);
    CHECK(strstr(report, "decision|documented tradeoff proposal"),
          "review_metrics did not identify the documented latency proposal");
    CHECK(strstr(report, "tradeoff text is an unverified proposal") &&
          strstr(report, "no independent measurement attestation, repository-owner approval, global optimality, or fresh verification claim"),
          "review_metrics treated local tradeoff text as approval or independent evidence");
    free(report);

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);

    CHECK(run_gate_with_latency(argv[1], argv[2], argv[2], "release", "4096", "500", "900", NULL) != 0,
          "review_metrics accepted latency above its active budget");
    report = read_text(output_log);
    CHECK(strstr(report, "candidate exceeds an active byte, cycle, or latency budget"),
          "review_metrics returned the wrong latency budget rejection");
    free(report);
    CHECK(run_gate_with_latency(argv[1], argv[2], argv[2], "release", "4096", "500", "0", NULL) != 0,
          "review_metrics accepted a zero latency budget");
    report = read_text(output_log);
    CHECK(strstr(report, "max-latency-ns gate must be positive"),
          "review_metrics returned the wrong zero latency budget rejection");
    free(report);

    write_image(candidate_image, "250", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    CHECK(run_gate(argv[1], argv[2], argv[2], "release", "4096", "500", "metrics-tradeoff.txt") == 0,
          "review_metrics rejected a candidate with committed tradeoff proposal provenance");
    report = read_text(output_log);
    CHECK(strstr(report, "decision|documented tradeoff proposal") != NULL,
          "review_metrics did not record its documented tradeoff proposal");
    free(report);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", "product.txt",
                    "tradeoff proposal provenance needs a review reference and reason");
    expect_rejected(argv[1], argv[2], "release", "4096", "500", "metrics-local-approval.txt",
                    "tradeoff proposal provenance needs a review reference and reason");

    write_image(candidate_image, "150", "emulator-cycle-counter", "different-output", 32, NULL);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "not equivalent");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(candidate_manifest, "test", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "test image");
    write_image_manifest(baseline_manifest, "test", baseline, baseline_tree, baseline_image);
    CHECK(run_gate(argv[1], argv[2], argv[2], "test", "4096", "500", NULL) == 0,
          "review_metrics did not accept a test-image latency proof for test budgets");

    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    write_image(candidate_image, "150", "host-wall-time-ns", "same-output", 32, NULL);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "emulator cycle counter");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32,
                "report_workload|other-workload\n");
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "workload identity mismatch");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32,
                "report_input|0000000000000000000000000000000000000000000000000000000000000000\n");
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "input digest mismatch");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32,
                "report_toolchain|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL,
                    "measurement toolchain identity mismatch");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "0", "500", NULL, "max-bytes gate must be positive");
    expect_rejected(argv[1], argv[2], "release", "4096", NULL, NULL,
                    "both metric budgets and a latency budget are required");
    expect_rejected_pair(argv[1], argv[2], argv[3], "release", "4096", "500", NULL, "runner tool pins differ");

    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128,
                "runner_mode|hang\n");
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    CHECK(run_gate_with_limits(argv[1], argv[2], argv[2], "release", "4096", "500", "5000", "100", NULL) != 0,
          "review_metrics accepted a runner that exceeded its execution deadline");
    report = read_text(output_log);
    if (!strstr(report, "metric runner timed out after 100 ms"))
        fprintf(stderr, "review_metrics timeout diagnostic: %s\n", report);
    CHECK(strstr(report, "metric runner timed out after 100 ms"),
          "review_metrics did not report its bounded runner timeout");
    free(report);

    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128,
                "runner_mode|oversize\n");
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    CHECK(run_gate_with_limits(argv[1], argv[2], argv[2], "release", "4096", "500", "5000", "1000", NULL) != 0,
          "review_metrics accepted an oversized runner report");
    report = read_text(output_log);
    CHECK(strstr(report, "metric runner output exceeds 65536 bytes"),
          "review_metrics did not stop an oversized runner report at its output bound");
    free(report);

    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128,
                "runner_mode|nul-suffix\n");
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL,
                    "metric runner returned malformed execution evidence");

    char original_input_sha[65];
    CHECK(!review_sha256_file(input_path, original_input_sha), "Cannot hash fixture workload before mutation test");
    char reported_input[128];
    CHECK(snprintf(reported_input, sizeof(reported_input), "report_input|%s\n", original_input_sha) <
          (int)sizeof(reported_input), "Cannot format fixture workload digest override");
    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128,
                "runner_mode|mutate-input\n");
    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32, reported_input);
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    CHECK(run_gate_with_limits(argv[1], argv[2], argv[2], "release", "4096", "500", "5000", "1000", NULL) != 0,
          "review_metrics accepted a runner that changed the workload after reporting its original digest");
    report = read_text(output_log);
    if (!strstr(report, "workload input changed during metric execution"))
        fprintf(stderr, "review_metrics mutation diagnostic: %s\n", report);
    CHECK(strstr(report, "workload input changed during metric execution"),
          "review_metrics trusted a reported input digest without rechecking the supplied bytes");
    free(report);
    write_text(input_path, "addition workload input\n");

    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128, NULL);
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);

    write_image(baseline_image, "0", "emulator-cycle-counter", "same-output", 128, NULL);
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL,
                    "baseline bytes, cycles, and latency must be nonzero");

    write_image(baseline_image, "200", "emulator-cycle-counter", "same-output", 128, NULL);
    write_image(candidate_image, "18446744073709551616", "emulator-cycle-counter", "same-output", 32, NULL);
    write_image_manifest(baseline_manifest, "release", baseline, baseline_tree, baseline_image);
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL, "cycle count overflows uint64");

    write_image(candidate_image, "150", "emulator-cycle-counter", "same-output", 32,
                "report_latency_ns|18446744073709551616\n");
    write_image_manifest(candidate_manifest, "release", candidate, candidate_tree, candidate_image);
    expect_rejected(argv[1], argv[2], "release", "4096", "500", NULL,
                    "latency count overflows uint64");

    puts("PASS review metrics pins execution identity and gates release/test byte, cycle, latency, and output evidence");
    return EXIT_SUCCESS;
}
#endif
