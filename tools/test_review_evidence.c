#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_paths.h"
#include "test_run_from.h"
#include "review_sha256.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char output_root[4096];
static char project[4096];
static char report[4096];
static char plan_path[4096];
static char tool_log[4096];
static char fixture_input[4096];

static void path_join(char *path, size_t capacity, const char *root, const char *part)
{
    suite_test_path(path, capacity, root, part);
}

static void cleanup_output_root(void)
{
    if (output_root[0] && !suite_test_paths_remove(output_root)) {
        fprintf(stderr, "Could not clean review evidence test output %s\n", output_root);
        _Exit(EXIT_FAILURE);
    }
}

static void write_text(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create review evidence test input");
    size_t length = strlen(contents);
    CHECK(fwrite(contents, 1, length, file) == length, "Cannot write review evidence test input");
    CHECK(!fclose(file), "Cannot close review evidence test input");
}

static void create_unix_socket(const char *path)
{
    int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(descriptor >= 0, "Cannot create review evidence Unix socket fixture");
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    size_t length = strlen(path);
    CHECK(length < sizeof(address.sun_path), "Review evidence Unix socket path is too long");
    memcpy(address.sun_path, path, length + 1);
    CHECK(!bind(descriptor, (struct sockaddr *)&address, sizeof(address)),
          "Cannot bind review evidence Unix socket fixture");
    CHECK(!close(descriptor), "Cannot close review evidence Unix socket fixture");
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL, "Cannot open review evidence output");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek review evidence output");
    long size = ftell(file);
    CHECK(size >= 0 && size < 1024 * 1024, "Review evidence output has an unexpected size");
    CHECK(!fseek(file, 0, SEEK_SET), "Cannot rewind review evidence output");
    char *contents = calloc((size_t)size + 1, 1);
    CHECK(contents != NULL, "Cannot allocate review evidence output buffer");
    CHECK(fread(contents, 1, (size_t)size, file) == (size_t)size, "Cannot read review evidence output");
    CHECK(!fclose(file), "Cannot close review evidence output");
    return contents;
}

static bool file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL, "Cannot open review evidence log");
    size_t needle_length = strlen(needle);
    CHECK(needle_length && needle_length < 4096, "Review evidence log search term is invalid");
    char buffer[8192];
    size_t tail = 0;
    bool found = false;
    for (;;) {
        size_t count = fread(buffer + tail, 1, sizeof(buffer) - tail - 1, file);
        size_t total = tail + count;
        for (size_t i = 0; i + needle_length <= total; ++i)
            if (!memcmp(buffer + i, needle, needle_length)) { found = true; break; }
        if (found) break;
        if (count < sizeof(buffer) - tail - 1) break;
        tail = needle_length - 1;
        if (tail > total) tail = total;
        memmove(buffer, buffer + total - tail, tail);
    }
    int read_failed = ferror(file);
    int close_failed = fclose(file);
    CHECK(!read_failed && !close_failed, "Cannot read review evidence log");
    return found;
}

static void replace_once(char *text, const char *needle, const char *replacement)
{
    char *match = strstr(text, needle);
    CHECK(match != NULL, "Review evidence tamper target is missing");
    size_t needle_length = strlen(needle);
    CHECK(strlen(replacement) == needle_length, "Review evidence replacement changes record length");
    memcpy(match, replacement, needle_length);
}

static char *replace_text(const char *text, const char *needle, const char *replacement)
{
    const char *match = strstr(text, needle);
    CHECK(match != NULL, "Review evidence text replacement target is missing");
    size_t prefix = (size_t)(match - text);
    size_t needle_length = strlen(needle), replacement_length = strlen(replacement);
    size_t suffix = strlen(match + needle_length);
    CHECK(prefix <= SIZE_MAX - replacement_length &&
          prefix + replacement_length <= SIZE_MAX - suffix - 1,
          "Review evidence replacement result is too large");
    char *result = malloc(prefix + replacement_length + suffix + 1);
    CHECK(result != NULL, "Cannot allocate review evidence replacement");
    memcpy(result, text, prefix);
    memcpy(result + prefix, replacement, replacement_length);
    memcpy(result + prefix + replacement_length, match + needle_length, suffix + 1);
    return result;
}

static void last_field_from_record(const char *text, const char *prefix, char value[65])
{
    const char *record = strstr(text, prefix);
    CHECK(record != NULL, "Review evidence record is missing");
    const char *end = strchr(record, '\n');
    CHECK(end != NULL, "Review evidence record is incomplete");
    const char *last = NULL;
    for (const char *at = record; at < end; ++at)
        if (*at == '|') last = at + 1;
    CHECK(last && end - last == 64, "Review evidence record has no SHA-256 final field");
    memcpy(value, last, 64);
    value[64] = '\0';
}

static void field_from_record(const char *text, const char *prefix, size_t field_index,
                             char *value, size_t capacity)
{
    const char *record = strstr(text, prefix);
    CHECK(record != NULL, "Review evidence record is missing");
    const char *end = strchr(record, '\n');
    CHECK(end != NULL, "Review evidence record is incomplete");
    const char *start = record;
    for (size_t field = 0; field < field_index; ++field) {
        const char *separator = memchr(start, '|', (size_t)(end - start));
        CHECK(separator != NULL, "Review evidence record has too few fields");
        start = separator + 1;
    }
    const char *separator = memchr(start, '|', (size_t)(end - start));
    const char *field_end = separator ? separator : end;
    size_t length = (size_t)(field_end - start);
    CHECK(length && length < capacity, "Review evidence field is missing or too long");
    memcpy(value, start, length);
    value[length] = '\0';
}

static void check_plan_digest(const char *plan_path, const char *evidence_path)
{
    char expected[65];
    CHECK(!review_sha256_file(plan_path, expected), "Cannot hash review evidence test plan");
    char *evidence = read_text(evidence_path);
    char recorded[65];
    field_from_record(evidence, "plan_sha|", 1, recorded, sizeof(recorded));
    if (strcmp(expected, recorded)) {
        fprintf(stderr, "Test plan digest changed: plan=%s evidence=%s\n", expected, recorded);
        exit(EXIT_FAILURE);
    }
    free(evidence);
}

static void reseal_evidence(const char *evidence_path, const char *packet_path)
{
    char *packet = read_text(packet_path);
    char packet_sha[65];
    CHECK(!review_sha256_file(packet_path, packet_sha), "Cannot hash modified review packet");
    free(packet);

    char *evidence = read_text(evidence_path);
    char *packet_record = strstr(evidence, "packet_sha|");
    CHECK(packet_record != NULL, "Tampered review evidence has no packet digest");
    memcpy(packet_record + strlen("packet_sha|"), packet_sha, sizeof(packet_sha) - 1);
    char *seal_record = strstr(evidence, "seal|");
    CHECK(seal_record != NULL, "Tampered review evidence has no seal");
    review_sha256 context;
    unsigned char digest[32];
    char seal[65];
    review_sha256_init(&context);
    review_sha256_update(&context, evidence, (size_t)(seal_record - evidence));
    review_sha256_final(&context, digest);
    review_sha256_hex(digest, seal);

    size_t prefix_length = (size_t)(seal_record - evidence);
    char *sealed = malloc(prefix_length + 70 + 1);
    CHECK(sealed != NULL, "Cannot allocate resealed evidence");
    memcpy(sealed, evidence, prefix_length);
    snprintf(sealed + prefix_length, 71, "seal|%s\n", seal);
    write_text(evidence_path, sealed);
    free(sealed);
    free(evidence);
}

static int run_argv(char *const argv[], const char *directory, const char *log)
{
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork review evidence command");
    if (!child) {
        if (directory && chdir(directory)) _Exit(125);
        if (log) {
            int descriptor = open(log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
                dup2(descriptor, STDERR_FILENO) < 0) _Exit(125);
            close(descriptor);
        }
        execvp(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute %s: %s\n", argv[0], strerror(errno));
        _Exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for review evidence command");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

static void git_ok(const char *const args[], size_t count)
{
    char *argv[24];
    CHECK(count + 2 < sizeof(argv) / sizeof(argv[0]), "Too many fixture git arguments");
    argv[0] = (char *)"git";
    argv[1] = (char *)"-C";
    argv[2] = project;
    for (size_t i = 0; i < count; ++i) argv[3 + i] = (char *)args[i];
    argv[3 + count] = NULL;
    CHECK(run_argv(argv, NULL, NULL) == 0, "Cannot create review evidence Git fixture");
}

static void git_commit(const char *message)
{
    const char *add[] = { "-c", "core.hooksPath=/dev/null", "add", "calc.c", "contract_test.c",
        "flood_test.c", "link_test.c", "Makefile" };
    git_ok(add, sizeof(add) / sizeof(add[0]));
    const char *commit[] = { "-c", "core.hooksPath=/dev/null", "-c", "user.name=Review Evidence Test",
        "-c", "user.email=review-evidence@example.invalid", "commit", "--quiet", "-m", message };
    git_ok(commit, sizeof(commit) / sizeof(commit[0]));
}

static void git_head(char *sha, size_t capacity)
{
    int descriptors[2];
    CHECK(!pipe(descriptors), "Cannot create fixture Git output pipe");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork fixture Git identity command");
    if (!child) {
        close(descriptors[0]);
        CHECK(dup2(descriptors[1], STDOUT_FILENO) >= 0, "Cannot redirect fixture Git output");
        close(descriptors[1]);
        char *argv[] = { (char *)"git", (char *)"-C", project, (char *)"rev-parse", (char *)"HEAD", NULL };
        execvp(argv[0], argv);
        _Exit(127);
    }
    close(descriptors[1]);
    ssize_t used = read(descriptors[0], sha, capacity - 1);
    CHECK(used > 0 && used < (ssize_t)capacity - 1, "Cannot read fixture Git identity");
    sha[used] = '\0';
    CHECK(!close(descriptors[0]), "Cannot close fixture Git identity pipe");
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status),
          "Cannot read fixture Git identity");
    sha[strcspn(sha, "\r\n")] = '\0';
    CHECK(strlen(sha) == 40, "Fixture Git identity was not a full commit SHA");
}

static void create_fixture(const char *host_cc, char *baseline, size_t baseline_capacity,
                           char *candidate, size_t candidate_capacity)
{
    char init_branch[] = "main";
    char *init[] = { (char *)"git", (char *)"init", (char *)"--quiet", (char *)"--initial-branch",
        init_branch, project, NULL };
    CHECK(run_argv(init, NULL, NULL) == 0, "Cannot initialize review evidence Git fixture");

    const char *makefile =
        "CC ?= cc\n"
        "HOST_CC ?= $(CC)\n"
        "OUT ?= build\n"
        ".PHONY: build test failure timeout flood\n"
        "build:\n"
        "\tmkdir -p \"$(OUT)\"\n"
        "\t\"$(HOST_CC)\" -std=c11 -Wall -Wextra -Werror -o \"$(OUT)/contract-test\" contract_test.c calc.c\n"
        "\t\"$(HOST_CC)\" -std=c11 -Wall -Wextra -Werror -o \"$(OUT)/flood-test\" flood_test.c\n"
        "test: build\n"
        "\t\"$(OUT)/contract-test\"\n"
        "\ttest -f \"$(INPUT_DIR)/input.txt\"\n"
        "flood: build\n"
        "\t\"$(OUT)/flood-test\"\n"
        "failure:\n"
        "\t\"$(HOST_CC)\" --version >/dev/null\n"
        "\tfalse\n"
        "timeout:\n"
        "\t\"$(HOST_CC)\" --version >/dev/null\n"
        "\tsleep 5\n";
    const char *test_source =
        "#include <stdio.h>\n"
        "int add(int a, int b);\n"
        "int main(void) {\n"
        "    int actual = add(20, 22);\n"
        "    fputs(\"WITNESS_EXECUTED: addition-contract\\n\", stderr);\n"
        "    if (actual != 42) { fprintf(stderr, \"ASSERTION: expected 42, got %d\\n\", actual); return 7; }\n"
        "    puts(\"PASS: addition-contract\");\n"
        "    return 0;\n"
        "}\n";
    const char *flood_source =
        "#include <unistd.h>\n"
        "int main(void) {\n"
        "    char block[4096] = { 0 };\n"
        "    while (write(STDOUT_FILENO, block, sizeof(block)) > 0) { }\n"
        "    return 0;\n"
        "}\n";
    char path[4096];
    path_join(path, sizeof(path), project, "Makefile");
    write_text(path, makefile);
    path_join(path, sizeof(path), project, "contract_test.c");
    write_text(path, test_source);
    path_join(path, sizeof(path), project, "calc.c");
    write_text(path, "int add(int a, int b) { return a + b - 1; }\n");
    path_join(path, sizeof(path), project, "flood_test.c");
    write_text(path, flood_source);
    const char *add_bug[] = { "-c", "core.hooksPath=/dev/null", "add", "calc.c", "contract_test.c",
        "flood_test.c", "Makefile" };
    git_ok(add_bug, sizeof(add_bug) / sizeof(add_bug[0]));
    const char *commit_bug[] = { "-c", "core.hooksPath=/dev/null", "-c", "user.name=Review Evidence Test",
        "-c", "user.email=review-evidence@example.invalid", "commit", "--quiet", "-m", "bug: off-by-one addition" };
    git_ok(commit_bug, sizeof(commit_bug) / sizeof(commit_bug[0]));
    git_head(baseline, baseline_capacity);

    path_join(path, sizeof(path), project, "calc.c");
    write_text(path, "int add(int a, int b) { return a + b; }\n");
    path_join(path, sizeof(path), project, "link_test.c");
    CHECK(!symlink("contract_test.c", path), "Cannot create tracked symlink fixture");
    git_commit("fix: return the sum");
    git_head(candidate, candidate_capacity);

    CHECK(host_cc && *host_cc, "Review evidence test needs the host C compiler name");
}

static void write_plan(const char *path, const char *candidate, const char *baseline,
                       bool failing_suite, bool timeout_suite, bool wrong_symptom,
                       bool broken_build, const char *overlay_test, const char *fixture_input,
                       const char *mutation_marker)
{
    char plan[8192], suite[4096];
    if (failing_suite) snprintf(suite, sizeof(suite),
        "suite|required-contract|15|5|@TOOL:make@|-f|Makefile|failure|HOST_CC=@TOOL:host-cc@\n");
    else if (timeout_suite) snprintf(suite, sizeof(suite),
        "suite|required-contract|1|5|@TOOL:make@|-f|Makefile|timeout|HOST_CC=@TOOL:host-cc@\n");
    else if (mutation_marker) snprintf(suite, sizeof(suite),
        "suite|required-contract|15|14|/bin/sh|-c|\"$3\" \"$4\" \"$5\" \"$6\" \"$7\" \"$8\" \"$9\" \"${10}\"; status=$?; if [ -e \"$1\" ]; then printf '%%s\\n' 'int add(int a, int b) { return a + b - 1; }' > \"$2\"; fi; exit \"$status\"|sh|%s|@REPO@/calc.c|@TOOL:make@|-f|Makefile|test|OUT=@OUT@|CC=@TOOLCHAIN@|HOST_CC=@TOOL:host-cc@|INPUT_DIR=@INPUT:fixture-data@\n",
        mutation_marker);
    else if (fixture_input && *fixture_input) snprintf(suite, sizeof(suite),
        "suite|required-contract|15|8|@TOOL:make@|-f|Makefile|test|OUT=@OUT@|CC=@TOOLCHAIN@|HOST_CC=@TOOL:host-cc@|INPUT_DIR=@INPUT:fixture-data@\n");
    else snprintf(suite, sizeof(suite),
        "suite|required-contract|15|7|@TOOL:make@|-f|Makefile|test|OUT=@OUT@|CC=@TOOLCHAIN@|HOST_CC=@TOOL:host-cc@\n");
    char input_record[4096] = "";
    if (fixture_input && *fixture_input) {
        int input_length = snprintf(input_record, sizeof(input_record), "input|fixture-data|%s\n", fixture_input);
        CHECK(input_length > 0 && input_length < (int)sizeof(input_record), "Review input path is too long");
    }
    int length = snprintf(plan, sizeof(plan),
        "review-plan-v2\n"
        "repo|%s\n"
        "candidate|%s\n"
        "toolchain|cc\n"
        "tool|make|make\n"
        "tool|host-cc|cc\n"
        "%s"
        "%s"
        "witness|addition-contract|%s|15|7|WITNESS_EXECUTED: addition-contract|"
        "%s|PASS: addition-contract|6|@TOOL:make@|-f|Makefile|build|"
        "OUT=@OUT@|CC=%s|1|@OUT@/contract-test\n"
        "scope|addition-contract|add two values and return their sum|add|calc.c|contract_test.c:main calls add|contract_test.c|required-contract|same permanent contract case must run on baseline and candidate|none\n"
        "overlay|addition-contract|Makefile\n"
        "overlay|addition-contract|%s\n",
        project, candidate, input_record, suite, baseline,
        wrong_symptom ? "ASSERTION: unrelated failure" : "ASSERTION: expected 42, got 41",
        broken_build ? "/bin/false" : "@TOOLCHAIN@", overlay_test);
    CHECK(length > 0 && length < (int)sizeof(plan), "Review evidence test plan is too large");
    write_text(path, plan);
}

static int run_tool_from(const char *tool, const char *operation, const char *plan,
                         const char *directory, const char *replay_root, const char *cwd)
{
    char *argv[9];
    size_t argc = 0;
    argv[argc++] = (char *)tool;
    argv[argc++] = (char *)operation;
    argv[argc++] = (char *)"--plan";
    argv[argc++] = (char *)plan;
    if (!strcmp(operation, "capture")) {
        argv[argc++] = (char *)"--output";
        argv[argc++] = (char *)directory;
    } else {
        argv[argc++] = (char *)"--evidence-dir";
        argv[argc++] = (char *)directory;
        argv[argc++] = (char *)"--replay-root";
        argv[argc++] = (char *)replay_root;
    }
    argv[argc] = NULL;
    if (cwd) {
        run_from(argv, tool_log, 120, cwd);
        return 0;
    }
    return run_argv(argv, NULL, tool_log);
}

static int run_tool(const char *tool, const char *operation, const char *plan,
                    const char *directory, const char *replay_root)
{
    return run_tool_from(tool, operation, plan, directory, replay_root, NULL);
}

static void expect_tool_status(const char *tool, const char *operation, const char *plan,
                               const char *directory, const char *replay_root, int expected,
                               const char *message)
{
    int status = run_tool(tool, operation, plan, directory, replay_root);
    if (status != expected) {
        fprintf(stderr, "%s (expected exit %d, got %d)\n", message, expected, status);
        run_from_print_log_tail(tool_log);
        exit(EXIT_FAILURE);
    }
}

static void expect_tool_failure(const char *tool, const char *operation, const char *plan,
                                const char *directory, const char *replay_root,
                                const char *message)
{
    int status = run_tool(tool, operation, plan, directory, replay_root);
    if (!status) {
        fprintf(stderr, "%s (expected nonzero exit)\n", message);
        run_from_print_log_tail(tool_log);
        exit(EXIT_FAILURE);
    }
}

static void expect_capture_rejected(const char *tool, const char *plan, const char *directory,
                                    const char *reason)
{
    expect_tool_failure(tool, "capture", plan, directory, NULL,
                        "Review evidence capture accepted an invalid plan");
    char *diagnostics = read_text(tool_log);
    if (!strstr(diagnostics, reason)) {
        fprintf(stderr, "Expected capture diagnostic [%s], received [%s]\n", reason, diagnostics);
        free(diagnostics);
        exit(EXIT_FAILURE);
    }
    free(diagnostics);
}

static void expect_verify_rejected_at(const char *tool, const char *plan, const char *directory,
                                     const char *replay_root, const char *reason)
{
    expect_tool_failure(tool, "verify", plan, directory, replay_root,
                        "Review evidence verifier accepted an invalid report");
    char *diagnostics = read_text(tool_log);
    if (!strstr(diagnostics, reason)) {
        fprintf(stderr, "Expected verifier diagnostic [%s], received [%s]\n", reason, diagnostics);
        free(diagnostics);
        exit(EXIT_FAILURE);
    }
    free(diagnostics);
}

static void expect_verify_rejected(const char *tool, const char *plan, const char *reason)
{
    expect_verify_rejected_at(tool, plan, report, output_root, reason);
}

int main(int argc, char **argv)
{
    CHECK(argc == 3, "Expected review-tools path and host C compiler name");
    suite_test_paths_init("review-evidence");
    CHECK(snprintf(output_root, sizeof(output_root), "%s/review-evidence-XXXXXX", suite_output_directory) <
          (int)sizeof(output_root), "Review evidence output path is too long");
    CHECK(mkdtemp(output_root) != NULL, "Cannot create review evidence suite output");
    CHECK(!atexit(cleanup_output_root), "Cannot register review evidence output cleanup");
    fprintf(stderr, "WITNESS_EXECUTED: review-evidence-v2\n");
    path_join(project, sizeof(project), suite_fixture_directory, "mini-c-product");
    CHECK(!mkdir(project, 0700), "Cannot create miniature C product fixture");
    path_join(fixture_input, sizeof(fixture_input), suite_fixture_directory, "fixture-data");
    CHECK(!mkdir(fixture_input, 0700), "Cannot create review test input directory");
    char input_file[4096], git_admin_directory[4096], git_metadata_file[4096], git_socket[4096];
    path_join(input_file, sizeof(input_file), fixture_input, "input.txt");
    write_text(input_file, "declared suite input\n");
    path_join(git_admin_directory, sizeof(git_admin_directory), fixture_input, ".git");
    CHECK(!mkdir(git_admin_directory, 0700), "Cannot create declared input VCS metadata directory");
    path_join(git_metadata_file, sizeof(git_metadata_file), git_admin_directory, "config");
    write_text(git_metadata_file, "[core]\nrepositoryformatversion = 0\n");
    path_join(git_socket, sizeof(git_socket), git_admin_directory, "s");
    create_unix_socket(git_socket);
    path_join(report, sizeof(report), output_root, "captured");
    path_join(plan_path, sizeof(plan_path), suite_fixture_directory, "review.plan");
    path_join(tool_log, sizeof(tool_log), output_root, "review-tools.log");

    char baseline[64], candidate[64];
    create_fixture(argv[2], baseline, sizeof(baseline), candidate, sizeof(candidate));
    write_plan(plan_path, candidate, baseline, false, false, false, false, "contract_test.c", fixture_input, NULL);

    expect_tool_status(argv[1], "capture", plan_path, report, NULL, 0,
                       "Review evidence capture did not record the real C baseline failure and fixed candidate");
    char evidence_path[4096], packet_path[4096];
    path_join(evidence_path, sizeof(evidence_path), report, "evidence.v2");
    path_join(packet_path, sizeof(packet_path), report, "review.md");
    if (access(evidence_path, R_OK) || access(packet_path, R_OK)) {
        fputs("Review evidence capture omitted its evidence file or review packet\n", stderr);
        run_from_print_log_tail(tool_log);
        exit(EXIT_FAILURE);
    }
    char *packet = read_text(packet_path);
    CHECK(strstr(packet, candidate) && strstr(packet, "baseline symptom reached") &&
          strstr(packet, "candidate passed") && strstr(packet, "temporal order cannot be proven"),
          "Generated review packet omitted the executable regression result or evidence limitation");
    CHECK(strstr(packet, "## Contract and coverage") &&
          strstr(packet, "add two values") && strstr(packet, "calc.c:add") &&
          strstr(packet, "contract_test.c") && strstr(packet, "required-contract") &&
          strstr(packet, "same permanent contract case must run on baseline and candidate") &&
          strstr(packet, "Unavailable required coverage: none"),
          "Generated review packet omitted the production contract, callers, case, or coverage obligation");
    CHECK(strstr(packet, "cx16agi-external-input-v1") &&
          strstr(packet, "directory-root-dotgit-excluded-v1") &&
          strstr(packet, "omit exactly the root `.git` entry") &&
          strstr(packet, "Artifact-tree hashes do not apply the `.git` exclusion"),
          "Generated review packet omitted external-input VCS exclusion semantics");
    free(packet);

    char stderr_version_helper[4096], stderr_version_plan_path[4096];
    char stderr_version_report[4096], stderr_version_evidence_path[4096];
    path_join(stderr_version_helper, sizeof(stderr_version_helper), suite_fixture_directory,
              "stderr-only-version-cc");
    write_text(stderr_version_helper,
               "#!/bin/sh\n"
               "if [ \"$1\" = --version ]; then\n"
               "  printf '%s\\n' 'fixture cc stderr-only version 1' >&2\n"
               "  exit 0\n"
               "fi\n"
               "exec cc \"$@\"\n");
    CHECK(!chmod(stderr_version_helper, 0700), "Cannot make stderr-only compiler fixture executable");
    path_join(stderr_version_plan_path, sizeof(stderr_version_plan_path), suite_fixture_directory,
              "stderr-version.plan");
    path_join(stderr_version_report, sizeof(stderr_version_report), output_root, "stderr-version");
    char input_record[8192];
    int input_record_length = snprintf(input_record, sizeof(input_record),
        "toolchain|@INPUT:stderr-version-cc@\ninput|stderr-version-cc|%s\n", stderr_version_helper);
    CHECK(input_record_length > 0 && input_record_length < (int)sizeof(input_record),
          "stderr-only compiler input path is too long");
    char *ordinary_plan = read_text(plan_path);
    char *stderr_version_plan = replace_text(ordinary_plan, "toolchain|cc\n", input_record);
    write_text(stderr_version_plan_path, stderr_version_plan);
    free(ordinary_plan);
    free(stderr_version_plan);
    expect_tool_status(argv[1], "capture", stderr_version_plan_path, stderr_version_report, NULL, 0,
                       "Capture did not accept a declared toolchain with stderr-only version output");
    path_join(stderr_version_evidence_path, sizeof(stderr_version_evidence_path), stderr_version_report,
              "evidence.v2");
    char *stderr_version_evidence = read_text(stderr_version_evidence_path);
    CHECK(strstr(stderr_version_evidence, "toolchain_path|") &&
          strstr(stderr_version_evidence, stderr_version_helper) &&
          strstr(stderr_version_evidence, "input|stderr-version-cc|"),
          "Evidence omitted the declared stderr-version toolchain identity");
    free(stderr_version_evidence);
    expect_tool_status(argv[1], "verify", stderr_version_plan_path, stderr_version_report, output_root, 0,
                       "Verifier did not accept a declared toolchain with stderr-only version output");

    char unsafe_overlay_plan[4096], unsafe_overlay_report[4096];
    path_join(unsafe_overlay_plan, sizeof(unsafe_overlay_plan), suite_fixture_directory, "unsafe-overlay.plan");
    path_join(unsafe_overlay_report, sizeof(unsafe_overlay_report), output_root, "unsafe-overlay");
    write_plan(unsafe_overlay_plan, candidate, baseline, false, false, false, false,
               "contract_test.c", fixture_input, NULL);
    char *unsafe_plan = read_text(unsafe_overlay_plan);
    replace_once(unsafe_plan, "overlay|addition-contract|Makefile", "overlay|addition-contract|./calc.c");
    write_text(unsafe_overlay_plan, unsafe_plan);
    free(unsafe_plan);
    expect_tool_failure(argv[1], "capture", unsafe_overlay_plan, unsafe_overlay_report, NULL,
                        "Capture accepted an unsafe regression overlay path");
    char *unsafe_overlay_diagnostic = read_text(tool_log);
    if (!strstr(unsafe_overlay_diagnostic, "invalid regression overlay path")) {
        fprintf(stderr, "Unsafe overlay was rejected for the wrong reason: %s\n", unsafe_overlay_diagnostic);
        run_from_print_log_tail(tool_log);
        exit(EXIT_FAILURE);
    }
    free(unsafe_overlay_diagnostic);

    char unavailable_plan[4096], unavailable_report[4096], unavailable_packet[4096];
    path_join(unavailable_plan, sizeof(unavailable_plan), suite_fixture_directory, "unavailable-coverage.plan");
    path_join(unavailable_report, sizeof(unavailable_report), output_root, "unavailable-coverage");
    char *scope_source = read_text(plan_path);
    char *scope_unavailable = replace_text(scope_source, "|none\n",
                                           "|full-game SDL caller unavailable in this native runner\n");
    write_text(unavailable_plan, scope_unavailable);
    expect_tool_failure(argv[1], "capture", unavailable_plan, unavailable_report, NULL,
                        "Required coverage availability did not block review evidence capture");
    char *unavailable_diagnostic = read_text(tool_log);
    CHECK(strstr(unavailable_diagnostic,
                 "required coverage unavailable for witness addition-contract: full-game SDL caller unavailable") != NULL,
          "Unavailable required coverage did not produce an actionable capture diagnostic");
    free(unavailable_diagnostic);
    path_join(unavailable_packet, sizeof(unavailable_packet), unavailable_report, "review.md");
    char *unavailable_packet_text = read_text(unavailable_packet);
    CHECK(strstr(unavailable_packet_text,
                 "Unavailable required coverage: full-game SDL caller unavailable in this native runner") != NULL,
          "Review packet omitted unavailable required coverage");
    free(unavailable_packet_text);
    free(scope_unavailable);

    char flood_plan[4096], flood_report[4096], flood_log[4096], flood_evidence_path[4096];
    path_join(flood_plan, sizeof(flood_plan), suite_fixture_directory, "flood-output.plan");
    path_join(flood_report, sizeof(flood_report), output_root, "flood-output");
    char *flood_plan_text = replace_text(scope_source, "|Makefile|test|OUT=", "|Makefile|flood|OUT=");
    write_text(flood_plan, flood_plan_text);
    expect_tool_failure(argv[1], "capture", flood_plan, flood_report, NULL,
                        "Review evidence capture accepted a child that exceeded the log output limit");
    path_join(flood_log, sizeof(flood_log), flood_report, "logs/required-contract.log");
    if (!file_contains(flood_log, "child log output limit reached (8388608 bytes)")) {
        struct stat log_info;
        if (!stat(flood_log, &log_info)) fprintf(stderr, "Flood suite log size: %lld bytes\n", (long long)log_info.st_size);
        fputs("Flooding native child log omitted the output limit diagnostic\n", stderr);
        run_from_print_log_tail(flood_log);
        exit(EXIT_FAILURE);
    }
    struct stat bounded_log_info;
    CHECK(!stat(flood_log, &bounded_log_info) && S_ISREG(bounded_log_info.st_mode) &&
          (uint64_t)bounded_log_info.st_size <= 8u * 1024u * 1024u,
          "Child output limit diagnostic exceeded the bounded log size");
    path_join(flood_evidence_path, sizeof(flood_evidence_path), flood_report, "evidence.v2");
    char *flood_evidence = read_text(flood_evidence_path);
    char *flood_suite_row = strstr(flood_evidence, "suite|required-contract|");
    CHECK(flood_suite_row != NULL && strncmp(flood_suite_row + strlen("suite|required-contract|"),
                                              "0|0|0|0|", strlen("0|0|0|0|")),
          "Flooding native child was incorrectly recorded as a passing required suite");
    free(flood_evidence);
    free(flood_plan_text);
    free(scope_source);

    char *captured_evidence = read_text(evidence_path);
    CHECK(strstr(captured_evidence, "tool|make|") && strstr(captured_evidence, "tool|host-cc|") &&
          strstr(captured_evidence, "tool|git|") &&
          strstr(captured_evidence, "tool|tar|") && strstr(captured_evidence, "input|fixture-data|") &&
          strstr(captured_evidence, "baseline") && strstr(captured_evidence, "candidate"),
          "Evidence omitted declared tool or external input identities");
    char baseline_overlay_sha[65], candidate_overlay_sha[65];
    char baseline_tree[65], candidate_tree[65];
    last_field_from_record(captured_evidence, "witness|addition-contract|baseline|", baseline_overlay_sha);
    last_field_from_record(captured_evidence, "witness|addition-contract|candidate|", candidate_overlay_sha);
    field_from_record(captured_evidence, "witness|addition-contract|baseline|", 4, baseline_tree, sizeof(baseline_tree));
    field_from_record(captured_evidence, "witness|addition-contract|candidate|", 4, candidate_tree, sizeof(candidate_tree));
    CHECK(!strcmp(baseline_overlay_sha, candidate_overlay_sha),
          "Baseline and candidate witnesses used different regression test inputs");
    CHECK(strcmp(baseline_tree, candidate_tree),
          "Candidate test overlays replaced the recorded production baseline tree");
    free(captured_evidence);
    write_text(git_metadata_file, "[core]\nrepositoryformatversion = 1\nchanged = true\n");
    char git_head_metadata[4096];
    path_join(git_head_metadata, sizeof(git_head_metadata), git_admin_directory, "HEAD");
    write_text(git_head_metadata, "ref: refs/heads/metadata-change\n");
    expect_tool_status(argv[1], "verify", plan_path, report, output_root, 0,
                       "Trusted review evidence verification rejected an intact run");

    char outside_git_socket[4096], outside_git_report[4096];
    path_join(outside_git_socket, sizeof(outside_git_socket), fixture_input, "s");
    create_unix_socket(outside_git_socket);
    path_join(outside_git_report, sizeof(outside_git_report), output_root, "outside-git-socket");
    expect_tool_failure(argv[1], "capture", plan_path, outside_git_report, NULL,
                        "Capture accepted an unsupported socket outside root .git");
    char *outside_git_diagnostic = read_text(tool_log);
    CHECK(strstr(outside_git_diagnostic, "unsupported artifact type") &&
          strstr(outside_git_diagnostic, outside_git_socket),
          "Socket outside root .git was not rejected by external-input hashing");
    free(outside_git_diagnostic);
    CHECK(!unlink(outside_git_socket), "Cannot remove outside-.git Unix socket fixture");

    char nested_workdir[4096], nested_plan[4096], nested_report[4096], nested_evidence_path[4096];
    path_join(nested_workdir, sizeof(nested_workdir), project, "tools");
    CHECK(!mkdir(nested_workdir, 0700), "Cannot create nested Git invocation directory");
    path_join(nested_plan, sizeof(nested_plan), suite_fixture_directory, "portable-review.plan");
    path_join(nested_report, sizeof(nested_report), output_root, "portable-review");
    char *portable = read_text(plan_path);
    char *repo_relative = replace_text(portable, project, ".");
    char *head_relative = replace_text(repo_relative, candidate, "HEAD");
    char *environment_input = replace_text(head_relative, fixture_input, "@ENV:REVIEW_TEST_INPUT@");
    write_text(nested_plan, environment_input);
    CHECK(!setenv("REVIEW_TEST_INPUT", fixture_input, 1), "Cannot set portable plan test input environment");
    free(portable);
    free(repo_relative);
    free(head_relative);
    free(environment_input);
    CHECK(run_tool_from(argv[1], "capture", nested_plan, nested_report, NULL, nested_workdir) == 0,
          "Repository-relative plan failed when invoked from a subdirectory");
    path_join(nested_evidence_path, sizeof(nested_evidence_path), nested_report, "evidence.v2");
    check_plan_digest(nested_plan, nested_evidence_path);
    CHECK(run_tool_from(argv[1], "verify", nested_plan, nested_report, output_root, nested_workdir) == 0,
          "Repository-relative evidence failed verification from a subdirectory");
    CHECK(!unsetenv("REVIEW_TEST_INPUT"), "Cannot clear portable plan test input environment");

    char symlink_case_plan[4096], missing_case_plan[4096], argv_bound_plan[4096];
    char *valid_plan = read_text(plan_path);
    char *symlink_case = replace_text(valid_plan, "|contract_test.c|required-contract|",
                                       "|link_test.c|required-contract|");
    path_join(symlink_case_plan, sizeof(symlink_case_plan), suite_fixture_directory, "symlink-case.plan");
    write_text(symlink_case_plan, symlink_case);
    expect_capture_rejected(argv[1], symlink_case_plan, output_root,
                            "contract case is not a tracked regular file at candidate: link_test.c");
    free(symlink_case);
    char *missing_case = replace_text(valid_plan, "|contract_test.c|required-contract|",
                                        "|missing_case.c|required-contract|");
    path_join(missing_case_plan, sizeof(missing_case_plan), suite_fixture_directory, "missing-case.plan");
    write_text(missing_case_plan, missing_case);
    expect_capture_rejected(argv[1], missing_case_plan, output_root,
                            "contract case is not a tracked regular file at candidate: missing_case.c");
    free(missing_case);
    char *too_many_arguments = replace_text(valid_plan, "|6|@TOOL:make@|-f|Makefile|build|",
                                              "|65|@TOOL:make@|-f|Makefile|build|");
    path_join(argv_bound_plan, sizeof(argv_bound_plan), suite_fixture_directory, "argv-bound.plan");
    write_text(argv_bound_plan, too_many_arguments);
    expect_capture_rejected(argv[1], argv_bound_plan, output_root,
                            "invalid witness build argv count: 65");
    free(too_many_arguments);
    free(valid_plan);

    char *valid_overlay_evidence = read_text(evidence_path);
    char *valid_overlay_packet = read_text(packet_path);
    char *mismatched_evidence = strdup(valid_overlay_evidence);
    char *mismatched_packet = strdup(valid_overlay_packet);
    CHECK(mismatched_evidence && mismatched_packet, "Cannot copy review evidence for overlay tamper case");
    char *baseline_row = strstr(mismatched_evidence, "witness|addition-contract|baseline|");
    CHECK(baseline_row != NULL, "Review evidence omitted the baseline witness row");
    char *baseline_row_end = strchr(baseline_row, '\n');
    CHECK(baseline_row_end != NULL, "Baseline witness evidence row is incomplete");
    char *baseline_digest = NULL;
    for (char *at = baseline_row; at < baseline_row_end; ++at)
        if (*at == '|') baseline_digest = at + 1;
    CHECK(baseline_digest && baseline_row_end - baseline_digest == 64,
          "Baseline witness omitted its overlay digest");
    char original_overlay_sha[65], weakened_overlay_sha[65];
    memcpy(original_overlay_sha, baseline_digest, 64);
    original_overlay_sha[64] = '\0';
    memcpy(weakened_overlay_sha, original_overlay_sha, sizeof(weakened_overlay_sha));
    weakened_overlay_sha[0] = weakened_overlay_sha[0] == '0' ? '1' : '0';
    memcpy(baseline_digest, weakened_overlay_sha, 64);
    replace_once(mismatched_packet, original_overlay_sha, weakened_overlay_sha);
    write_text(evidence_path, mismatched_evidence);
    write_text(packet_path, mismatched_packet);
    reseal_evidence(evidence_path, packet_path);
    expect_verify_rejected(argv[1], plan_path, "regression witness overlay identity mismatch");
    free(mismatched_evidence);
    free(mismatched_packet);
    write_text(evidence_path, valid_overlay_evidence);
    write_text(packet_path, valid_overlay_packet);
    free(valid_overlay_evidence);
    free(valid_overlay_packet);

    char *valid_tool_evidence = read_text(evidence_path);
    char *valid_tool_packet = read_text(packet_path);
    char *tampered_tool_evidence = strdup(valid_tool_evidence);
    char *tampered_tool_packet = strdup(valid_tool_packet);
    CHECK(tampered_tool_evidence && tampered_tool_packet, "Cannot copy review evidence for tool identity tamper case");
    char old_tool_sha[65], weakened_tool_sha[65];
    field_from_record(tampered_tool_evidence, "tool|make|", 3, old_tool_sha, sizeof(old_tool_sha));
    memcpy(weakened_tool_sha, old_tool_sha, sizeof(weakened_tool_sha));
    weakened_tool_sha[0] = weakened_tool_sha[0] == '0' ? '1' : '0';
    char *tool_row = strstr(tampered_tool_evidence, "tool|make|");
    CHECK(tool_row != NULL, "Review evidence omitted make identity");
    char *tool_field = tool_row;
    for (size_t i = 0; i < 3; ++i) {
        tool_field = strchr(tool_field, '|');
        CHECK(tool_field != NULL, "Review evidence make row is incomplete");
        ++tool_field;
    }
    memcpy(tool_field, weakened_tool_sha, 64);
    replace_once(tampered_tool_packet, old_tool_sha, weakened_tool_sha);
    write_text(evidence_path, tampered_tool_evidence);
    write_text(packet_path, tampered_tool_packet);
    reseal_evidence(evidence_path, packet_path);
    expect_verify_rejected(argv[1], plan_path, "declared tool identity mismatch: make");
    free(tampered_tool_evidence);
    free(tampered_tool_packet);
    write_text(evidence_path, valid_tool_evidence);
    write_text(packet_path, valid_tool_packet);
    free(valid_tool_evidence);
    free(valid_tool_packet);

    write_text(input_file, "tampered declared suite input\n");
    expect_verify_rejected(argv[1], plan_path, "external input identity mismatch: fixture-data");
    write_text(input_file, "declared suite input\n");

    char baseline_log[4096];
    path_join(baseline_log, sizeof(baseline_log), report, "logs/addition-contract-baseline-run.log");
    char *log_contents = read_text(baseline_log);
    FILE *log = fopen(baseline_log, "ab");
    CHECK(log != NULL && fputs("tampered\n", log) >= 0 && !fclose(log), "Cannot tamper test evidence log");
    expect_verify_rejected(argv[1], plan_path, "log digest mismatch");
    write_text(baseline_log, log_contents);
    free(log_contents);

    char original_evidence[4096];
    path_join(original_evidence, sizeof(original_evidence), report, "evidence.v2");
    char *valid_evidence = read_text(original_evidence);
    char *missing_suite_evidence = strdup(valid_evidence);
    CHECK(missing_suite_evidence != NULL, "Cannot allocate missing-suite evidence case");
    char *suite_row = strstr(missing_suite_evidence, "suite|required-contract|");
    CHECK(suite_row != NULL, "Review evidence omitted its required suite row");
    char *suite_row_end = strchr(suite_row, '\n');
    CHECK(suite_row_end != NULL, "Review evidence required suite row was incomplete");
    memmove(suite_row, suite_row_end + 1, strlen(suite_row_end + 1) + 1);
    write_text(original_evidence, missing_suite_evidence);
    expect_verify_rejected(argv[1], plan_path, "malformed required suite evidence row");
    write_text(original_evidence, valid_evidence);
    free(missing_suite_evidence);
    free(valid_evidence);

    char failed_plan[4096], failed_report[4096];
    path_join(failed_plan, sizeof(failed_plan), suite_fixture_directory, "failed-suite.plan");
    path_join(failed_report, sizeof(failed_report), output_root, "failed-suite");
    write_plan(failed_plan, candidate, baseline, true, false, false, false, "contract_test.c", NULL, NULL);
    expect_tool_failure(argv[1], "capture", failed_plan, failed_report, NULL,
                        "Capture accepted a nonzero required suite");
    path_join(evidence_path, sizeof(evidence_path), failed_report, "evidence.v2");
    CHECK(access(evidence_path, R_OK) == 0, "Failed required suite did not leave inspectable failure evidence");
    check_plan_digest(failed_plan, evidence_path);
    expect_tool_failure(argv[1], "verify", failed_plan, failed_report, output_root,
                        "Verifier accepted a failed required suite");
    char *failed_suite_diagnostic = read_text(tool_log);
    if (!strstr(failed_suite_diagnostic, "required suite failed, timed out, or was skipped")) {
        fprintf(stderr, "Verifier rejected the failed suite for the wrong reason: %s\n", failed_suite_diagnostic);
        run_from_print_log_tail(tool_log);
        exit(EXIT_FAILURE);
    }
    free(failed_suite_diagnostic);

    path_join(evidence_path, sizeof(evidence_path), failed_report, "evidence.v2");
    path_join(packet_path, sizeof(packet_path), failed_report, "review.md");
    char *forged_suite_evidence = read_text(evidence_path);
    char *forged_suite_row = strstr(forged_suite_evidence, "suite|required-contract|");
    CHECK(forged_suite_row != NULL, "Failed-suite evidence omitted its suite result");
    char *suite_exit = strchr(forged_suite_row, '|');
    suite_exit = suite_exit ? strchr(suite_exit + 1, '|') : NULL;
    CHECK(suite_exit && suite_exit[1] == '2', "Failed-suite result was not recorded as a real nonzero exit");
    suite_exit[1] = '0';
    write_text(evidence_path, forged_suite_evidence);
    free(forged_suite_evidence);
    char *forged_packet = read_text(packet_path);
    replace_once(forged_packet, "exit=2 signal=0", "exit=0 signal=0");
    write_text(packet_path, forged_packet);
    free(forged_packet);
    reseal_evidence(evidence_path, packet_path);
    check_plan_digest(failed_plan, evidence_path);
    expect_verify_rejected_at(argv[1], failed_plan, failed_report, output_root,
                              "trusted required suite replay failed: required-contract");

    char wrong_symptom_plan[4096], wrong_symptom_report[4096];
    path_join(wrong_symptom_plan, sizeof(wrong_symptom_plan), suite_fixture_directory, "wrong-symptom.plan");
    path_join(wrong_symptom_report, sizeof(wrong_symptom_report), output_root, "wrong-symptom");
    write_plan(wrong_symptom_plan, candidate, baseline, false, false, true, false,
               "contract_test.c", fixture_input, NULL);
    expect_tool_failure(argv[1], "capture", wrong_symptom_plan, wrong_symptom_report, NULL,
                        "Capture accepted a generic baseline failure without its declared symptom");
    path_join(packet_path, sizeof(packet_path), wrong_symptom_report, "review.md");
    packet = read_text(packet_path);
    CHECK(strstr(packet, "baseline symptom not established") != NULL,
          "Review packet treated an undeclared baseline failure as a regression witness");
    free(packet);

    char broken_build_plan[4096], broken_build_report[4096], baseline_run_log[4096];
    path_join(broken_build_plan, sizeof(broken_build_plan), suite_fixture_directory, "broken-build.plan");
    path_join(broken_build_report, sizeof(broken_build_report), output_root, "broken-build");
    write_plan(broken_build_plan, candidate, baseline, false, false, false, true,
               "contract_test.c", fixture_input, NULL);
    expect_tool_failure(argv[1], "capture", broken_build_plan, broken_build_report, NULL,
                        "Capture accepted a compiler failure as a regression witness");
    path_join(baseline_run_log, sizeof(baseline_run_log), broken_build_report,
              "logs/addition-contract-baseline-run.log");
    char *skipped_log = read_text(baseline_run_log);
    CHECK(strstr(skipped_log, "SKIPPED:") != NULL,
          "Witness run was not marked skipped after its compiler failure");
    free(skipped_log);

    char timeout_plan[4096], timeout_report[4096];
    path_join(timeout_plan, sizeof(timeout_plan), suite_fixture_directory, "timeout.plan");
    path_join(timeout_report, sizeof(timeout_report), output_root, "timeout");
    write_plan(timeout_plan, candidate, baseline, false, true, false, false,
               "contract_test.c", NULL, NULL);
    expect_tool_failure(argv[1], "capture", timeout_plan, timeout_report, NULL,
                        "Capture accepted a timed-out required suite");
    path_join(evidence_path, sizeof(evidence_path), timeout_report, "evidence.v2");
    char *timeout_evidence = read_text(evidence_path);
    CHECK(strstr(timeout_evidence, "suite|required-contract|127|9|1|0|logs/required-contract.log") != NULL,
          "Timed-out suite evidence omitted its signal and timeout status");
    free(timeout_evidence);
    expect_tool_failure(argv[1], "verify", timeout_plan, timeout_report, output_root,
                        "Verifier accepted a timed-out required suite");
    char *timeout_diagnostic = read_text(tool_log);
    CHECK(strstr(timeout_diagnostic, "required suite failed, timed out, or was skipped") != NULL,
          "Verifier rejected the timed-out suite for the wrong reason");
    free(timeout_diagnostic);

    char mutation_plan[4096], mutation_report[4096], mutation_marker[4096], calc_path[4096];
    path_join(mutation_plan, sizeof(mutation_plan), suite_fixture_directory, "verify-mutation.plan");
    path_join(mutation_report, sizeof(mutation_report), output_root, "verify-mutation");
    path_join(mutation_marker, sizeof(mutation_marker), suite_fixture_directory, "mutate-during-verify");
    path_join(calc_path, sizeof(calc_path), project, "calc.c");
    write_plan(mutation_plan, candidate, baseline, false, false, false, false,
               "contract_test.c", fixture_input, mutation_marker);
    expect_tool_status(argv[1], "capture", mutation_plan, mutation_report, NULL, 0,
                       "Mutation replay fixture did not capture a clean candidate");
    write_text(mutation_marker, "mutate candidate during verification\n");
    expect_verify_rejected_at(argv[1], mutation_plan, mutation_report, output_root,
                              "Git checkout is not clean");
    char *mutated_calc = read_text(calc_path);
    CHECK(strstr(mutated_calc, "return a + b - 1") != NULL,
          "Verification identity check did not follow required-suite execution");
    free(mutated_calc);
    write_text(calc_path, "int add(int a, int b) { return a + b; }\n");
    CHECK(!unlink(mutation_marker), "Cannot remove verification mutation marker");

    const char *add[] = { "-c", "core.hooksPath=/dev/null", "add", "calc.c", "contract_test.c", "Makefile" };
    git_ok(add, sizeof(add) / sizeof(add[0]));
    const char *advance[] = { "-c", "core.hooksPath=/dev/null", "-c", "user.name=Review Evidence Test",
        "-c", "user.email=review-evidence@example.invalid", "commit", "--quiet", "--allow-empty", "-m", "advance head" };
    git_ok(advance, sizeof(advance) / sizeof(advance[0]));
    expect_verify_rejected(argv[1], plan_path, "candidate HEAD mismatch");

    puts("PASS review evidence binds real C regression, clean Git identity, artifacts, required suite, and tamper checks");
    return EXIT_SUCCESS;
}
