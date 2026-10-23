#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "review_sha256.h"
#include "review_format.h"
#include "test_paths.h"
#include "test_run_from.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char output_root[4096];
static char route_dir[4096];
static char tool_log[4096];
static char project[4096];
static char manifest[4096];

static void cleanup_output(void)
{
    if (output_root[0] && !suite_test_paths_remove(output_root)) {
        fprintf(stderr, "Could not clean review series output %s\n", output_root);
        _Exit(EXIT_FAILURE);
    }
}

static void write_bytes(const char *path, const void *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create review series fixture input");
    CHECK(fwrite(bytes, 1, length, file) == length, "Cannot write review series fixture input");
    CHECK(!fclose(file), "Cannot close review series fixture input");
}

static void write_text(const char *path, const char *text)
{
    write_bytes(path, text, strlen(text));
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL, "Cannot read review series route output");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek review series route output");
    long size = ftell(file);
    CHECK(size >= 0 && size < 1024 * 1024, "Review series route output has an unexpected size");
    CHECK(!fseek(file, 0, SEEK_SET), "Cannot rewind review series route output");
    char *text = calloc((size_t)size + 1, 1);
    CHECK(text != NULL, "Cannot allocate review series route output buffer");
    CHECK(fread(text, 1, (size_t)size, file) == (size_t)size, "Cannot read review series route output");
    CHECK(!fclose(file), "Cannot close review series route output");
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

static void git_commit_files(const char *message, const char *const paths[])
{
    char *add[16];
    size_t count = 0;
    add[count++] = (char *)"add";
    for (size_t i = 0; paths[i]; ++i) {
        CHECK(count + 1 < sizeof(add) / sizeof(add[0]), "Too many paths in fixture commit");
        add[count++] = (char *)paths[i];
    }
    add[count] = NULL;
    git_project_ok(add);
    char *commit[] = { (char *)"-c", (char *)"user.name=Review Series Test",
        (char *)"-c", (char *)"user.email=review-series@example.invalid",
        (char *)"-c", (char *)"core.hooksPath=/dev/null", (char *)"commit", (char *)"--quiet",
        (char *)"-m", (char *)message, NULL };
    git_project_ok(commit);
}

static void git_commit(const char *message, const char *path)
{
    const char *paths[] = { path, NULL };
    git_commit_files(message, paths);
}

static void git_revision(const char *revision, char *sha, size_t capacity)
{
    int descriptors[2];
    CHECK(!pipe(descriptors), "Cannot create fixture Git revision pipe");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork fixture Git revision command");
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
        ssize_t count = read(descriptors[0], sha + used, capacity - used - 1);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += (size_t)count;
    }
    CHECK(used > 0 && used + 1 < capacity, "Cannot read fixture Git revision");
    sha[used] = '\0';
    CHECK(!close(descriptors[0]), "Cannot close fixture Git revision pipe");
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status),
          "Cannot resolve fixture Git revision");
    sha[strcspn(sha, "\r\n")] = '\0';
    CHECK(strlen(sha) == 40, "Fixture Git revision is not a full commit SHA");
}

static int run_cli(const char *tool)
{
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork review series CLI");
    if (!child) {
        int descriptor = open(tool_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
            dup2(descriptor, STDERR_FILENO) < 0) _Exit(125);
        close(descriptor);
        char *argv[] = { (char *)tool, (char *)"route", (char *)"--repo", project,
            (char *)"--manifest", manifest, (char *)"--output", route_dir, NULL };
        execv(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute %s: %s\n", argv[0], strerror(errno));
        _Exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for review series CLI");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

static unsigned rejection_number;

static char *replace_once(const char *input, const char *needle, const char *replacement)
{
    const char *found = strstr(input, needle);
    CHECK(found != NULL, "Cannot find expected review series manifest record to mutate");
    CHECK(!strstr(found + strlen(needle), needle), "Review series test replacement is ambiguous");
    size_t before = (size_t)(found - input);
    size_t after = strlen(found + strlen(needle));
    size_t result_size = before + strlen(replacement) + after + 1;
    char *result = malloc(result_size);
    CHECK(result != NULL, "Cannot allocate mutated review series manifest");
    memcpy(result, input, before);
    memcpy(result + before, replacement, strlen(replacement));
    memcpy(result + before + strlen(replacement), found + strlen(needle), after + 1);
    return result;
}

static char *reseal_evidence(const char *contents)
{
    const char *seal_line = strstr(contents, REVIEW_EVIDENCE_RECORD_SEAL "|");
    CHECK(seal_line && (seal_line == contents || seal_line[-1] == '\n'),
          "Review evidence fixture has no final seal row");
    CHECK(!strstr(seal_line + 1, REVIEW_EVIDENCE_RECORD_SEAL "|"),
          "Review evidence fixture has multiple seal rows");
    size_t prefix_length = (size_t)(seal_line - contents);
    review_sha256 seal_hash;
    unsigned char seal_digest[32];
    char seal_text[65], replacement_seal[80];
    review_sha256_init(&seal_hash);
    review_sha256_update(&seal_hash, contents, prefix_length);
    review_sha256_final(&seal_hash, seal_digest);
    review_sha256_hex(seal_digest, seal_text);
    int replacement_length = snprintf(replacement_seal, sizeof(replacement_seal), "%s|%s\n",
                                      REVIEW_EVIDENCE_RECORD_SEAL, seal_text);
    CHECK(replacement_length > 0 && replacement_length < (int)sizeof(replacement_seal),
          "Cannot format resealed evidence record");
    char *result = malloc(prefix_length + (size_t)replacement_length + 1);
    CHECK(result != NULL, "Cannot allocate resealed evidence fixture");
    memcpy(result, contents, prefix_length);
    memcpy(result + prefix_length, replacement_seal, (size_t)replacement_length + 1);
    return result;
}

static void expect_cli_rejected_bytes(const char *tool, const void *contents, size_t length,
                                      const char *diagnostic)
{
    write_bytes(manifest, contents, length);
    fprintf(stderr, "review-series expected rejection: %s\n", diagnostic);
    char reject_dir[4096];
    CHECK(snprintf(reject_dir, sizeof(reject_dir), "%s/rejected-%u", output_root, ++rejection_number) <
          (int)sizeof(reject_dir), "Review series rejection path is too long");
    strcpy(route_dir, reject_dir);
    int status = run_cli(tool);
    CHECK(status != 0, "review-series accepted a malformed or incomplete series manifest");
    char *log = read_text(tool_log);
    if (!strstr(log, diagnostic)) fprintf(stderr, "review-series diagnostic: %s\n", log);
    CHECK(strstr(log, diagnostic) != NULL, "review-series returned the wrong rejection diagnostic");
    free(log);
    CHECK(access(reject_dir, F_OK) && errno == ENOENT,
          "Rejected review-series input left a partial route directory");
}

static void expect_cli_rejected(const char *tool, const char *contents, const char *diagnostic)
{
    expect_cli_rejected_bytes(tool, contents, strlen(contents), diagnostic);
}

static void capture_evidence(const char *tool, const char *host_cc, const char *project_path,
                             const char *baseline, const char *candidate, const char *plan_name,
                             const char *output)
{
    char plan_path[4096], plan[8192];
    suite_test_path(plan_path, sizeof(plan_path), suite_fixture_directory, plan_name);
    int length = snprintf(plan, sizeof(plan),
        "review-plan-v2\n"
        "repo|%s\n"
        "candidate|%s\n"
        "toolchain|%s\n"
        "tool|make|make\n"
        "suite|required-contract|15|6|@TOOL:make@|-f|Makefile|test|OUT=@OUT@|CC=@TOOLCHAIN@\n"
        "witness|addition-contract|%s|15|7|WITNESS_EXECUTED: addition-contract|"
        "ASSERTION: expected 42, got 41|PASS: addition-contract|6|@TOOL:make@|-f|Makefile|build|"
        "OUT=@OUT@|CC=@TOOLCHAIN@|1|@OUT@/contract-test\n"
        "overlay|addition-contract|contract_test.c\n"
        "scope|addition-contract|add returns 42 for 20 plus 22|add|calc.c|contract_test.c calls add|"
        "contract_test.c|required-contract|baseline returns 41 and candidate returns 42|none\n",
        project_path, candidate, host_cc, baseline);
    CHECK(length > 0 && length < (int)sizeof(plan), "Review evidence plan fixture is too large");
    write_text(plan_path, plan);
    char *argv[] = { (char *)tool, (char *)"capture", (char *)"--plan", plan_path,
        (char *)"--output", (char *)output, NULL };
    run_from(argv, NULL, 120, NULL);
}

static void evidence_digest(const char *directory, char digest[65])
{
    char evidence_path[4096];
    suite_test_path(evidence_path, sizeof(evidence_path), directory, REVIEW_EVIDENCE_FILENAME);
    CHECK(!review_sha256_file(evidence_path, digest), "Cannot hash sealed review evidence fixture");
}

static void write_manifest(const char *base, const char *head_one, const char *head_two,
                           const char *commit_one, const char *commit_two,
                           const char *evidence_one, const char *evidence_digest_one,
                           const char *evidence_two, const char *evidence_digest_two)
{
    char text[8192];
    int length = snprintf(text, sizeof(text),
        "review-series-v1\n"
        "range|%s|%s\n"
        "pr|pr-two|%s|%s|%s|%s\n"
        "pr|pr-one|%s|%s|%s|%s\n"
        "requires|pr-two|pr-one\n"
        "commit|%s|pr-one\n"
        "commit|%s|pr-two\n",
        base, head_two, head_one, head_two, evidence_two, evidence_digest_two,
        base, head_one, evidence_one, evidence_digest_one,
        commit_one, commit_two);
    CHECK(length > 0 && length < (int)sizeof(text), "Review series manifest is too large");
    write_text(manifest, text);
}

static void write_extended_manifest(const char *base, const char *head_one, const char *head_two,
                                    const char *head_three, const char *evidence_one,
                                    const char *evidence_digest_one, const char *evidence_two,
                                    const char *evidence_digest_two, const char *evidence_three,
                                    const char *evidence_digest_three, const char *commit_one,
                                    const char *commit_two, const char *commit_target,
                                    const char *commit_split, const char *commit_excluded,
                                    const char *commit_folded)
{
    char text[16384];
    int length = snprintf(text, sizeof(text),
        "review-series-v1\n"
        "range|%s|%s\n"
        "pr|pr-one|%s|%s|%s|%s\n"
        "pr|pr-two|%s|%s|%s|%s\n"
        "pr|pr-three|%s|%s|%s|%s\n"
        "requires|pr-two|pr-one\n"
        "requires|pr-three|pr-two\n"
        "commit|%s|pr-one\n"
        "commit|%s|pr-two\n"
        "commit|%s|pr-three\n"
        "commit|%s|pr-three\n"
        "commit|%s|pr-three\n"
        "commit|%s|pr-three\n"
        "split|%s|split-one.txt|part-one\n"
        "split|%s|split-two.txt|part-two\n"
        "exclude|%s|removed generated fixture output\n"
        "fold|%s|%s|same review unit\n",
        base, head_three, base, head_one, evidence_one, evidence_digest_one,
        head_one, head_two, evidence_two, evidence_digest_two,
        head_two, head_three, evidence_three, evidence_digest_three,
        commit_one, commit_two, commit_target, commit_split, commit_excluded, commit_folded,
        commit_split, commit_split, commit_excluded, commit_folded, commit_target);
    CHECK(length > 0 && length < (int)sizeof(text), "Extended review series manifest is too large");
    write_text(manifest, text);
}

int main(int argc, char **argv)
{
    CHECK(argc == 5, "Expected review series CLI, review-tools, host C compiler, and repository root");
    suite_test_paths_init("review-series");
    CHECK(snprintf(output_root, sizeof(output_root), "%s/review-series-route-XXXXXX", suite_output_directory) <
          (int)sizeof(output_root), "Review series output path is too long");
    CHECK(mkdtemp(output_root) != NULL, "Cannot create review series suite output");
    CHECK(!atexit(cleanup_output), "Cannot register review series output cleanup");
    suite_test_path(route_dir, sizeof(route_dir), output_root, "route");
    suite_test_path(tool_log, sizeof(tool_log), output_root, "review-series.log");

    char tools_directory[4096];
    char output_override[4200];
    suite_test_path(tools_directory, sizeof(tools_directory), argv[4], "tools");
    CHECK(snprintf(output_override, sizeof(output_override), "REVIEW_SERIES_BIN=%s", argv[1]) <
          (int)sizeof(output_override), "Review series output override is too long");
    char *main_build[] = { (char *)"make", (char *)"-C", tools_directory, (char *)"review-series",
        output_override, NULL };
    run_from(main_build, tool_log, 60, NULL);
    CHECK(!access(argv[1], X_OK), "Main review-series target did not build the focused route CLI");

    char external_diff[4096], textconv[4096], external_marker[4096], textconv_marker[4096];
    char extended_path[8192];
    suite_test_path(external_diff, sizeof(external_diff), output_root, "external-diff-helper");
    suite_test_path(textconv, sizeof(textconv), output_root, "textconv-helper");
    suite_test_path(external_marker, sizeof(external_marker), output_root, "external-diff-invoked");
    suite_test_path(textconv_marker, sizeof(textconv_marker), output_root, "textconv-invoked");
    write_text(external_diff,
        "#!/bin/sh\n"
        ": > \"$CX16AGI_REVIEW_SERIES_EXTERNAL_MARKER\"\n"
        "exit 0\n");
    write_text(textconv,
        "#!/bin/sh\n"
        ": > \"$CX16AGI_REVIEW_SERIES_TEXTCONV_MARKER\"\n"
        "cat \"$1\"\n");
    CHECK(!chmod(external_diff, S_IRUSR | S_IWUSR | S_IXUSR) &&
          !chmod(textconv, S_IRUSR | S_IWUSR | S_IXUSR), "Cannot make configured diff helpers executable");
    const char *old_path = getenv("PATH");
    CHECK(snprintf(extended_path, sizeof(extended_path), "%s:%s", output_root, old_path ? old_path : "") <
          (int)sizeof(extended_path), "Review series helper path is too long");
    CHECK(!setenv("PATH", extended_path, 1) &&
          !setenv("CX16AGI_REVIEW_SERIES_EXTERNAL_MARKER", external_marker, 1) &&
          !setenv("CX16AGI_REVIEW_SERIES_TEXTCONV_MARKER", textconv_marker, 1),
          "Cannot configure marker environment for Git diff helper regression");

    suite_test_path(project, sizeof(project), suite_fixture_directory, "git-project");
    suite_test_path(manifest, sizeof(manifest), suite_fixture_directory, "series.manifest");
    char evidence_one[4096], evidence_two[4096];
    suite_test_path(evidence_one, sizeof(evidence_one), output_root, "evidence-pr-one");
    suite_test_path(evidence_two, sizeof(evidence_two), output_root, "evidence-pr-two");

    char *init[] = { (char *)"git", (char *)"init", (char *)"--quiet", (char *)"--initial-branch=main", project, NULL };
    git_ok(init);
    char file[4096];
    suite_test_path(file, sizeof(file), project, ".gitattributes");
    write_text(file, "*.txt diff=review-series-test\n");
    char *external_config[] = { (char *)"config", (char *)"diff.external", (char *)"cx16agi-external-diff", NULL };
    char *textconv_config[] = { (char *)"config", (char *)"diff.review-series-test.textconv",
        (char *)"cx16agi-textconv", NULL };
    git_project_ok(external_config);
    git_project_ok(textconv_config);
    suite_test_path(file, sizeof(file), project, "base.txt");
    write_text(file, "base\n");
    suite_test_path(file, sizeof(file), project, "Makefile");
    write_text(file,
        "CC ?= cc\n"
        "OUT ?= build\n"
        ".PHONY: build test\n"
        "build:\n"
        "\tmkdir -p \"$(OUT)\"\n"
        "\t\"$(CC)\" -std=c11 -Wall -Wextra -Werror -o \"$(OUT)/contract-test\" contract_test.c calc.c\n"
        "test: build\n"
        "\t\"$(OUT)/contract-test\"\n");
    suite_test_path(file, sizeof(file), project, "contract_test.c");
    write_text(file,
        "#include <stdio.h>\n"
        "int add(int a, int b);\n"
        "int main(void) {\n"
        "    int actual = add(20, 22);\n"
        "    fputs(\"WITNESS_EXECUTED: addition-contract\\n\", stderr);\n"
        "    if (actual != 42) { fprintf(stderr, \"ASSERTION: expected 42, got %d\\n\", actual); return 7; }\n"
        "    puts(\"PASS: addition-contract\");\n"
        "    return 0;\n"
        "}\n");
    suite_test_path(file, sizeof(file), project, "calc.c");
    write_text(file, "int add(int a, int b) { return a + b - 1; }\n");
    git_commit("buggy base", ".");
    char base[64], head_one[64], head_two[64], commit_one[64], commit_two[64];
    git_revision("HEAD", base, sizeof(base));

    suite_test_path(file, sizeof(file), project, "first.txt");
    write_text(file, "first review\n");
    suite_test_path(file, sizeof(file), project, "calc.c");
    write_text(file, "int add(int a, int b) { return a + b; }\n");
    const char *first_paths[] = { "first.txt", "calc.c", NULL };
    git_commit_files("first PR fixes the contract", first_paths);
    git_revision("HEAD", head_one, sizeof(head_one));
    strcpy(commit_one, head_one);
    char evidence_digest_one[65];
    capture_evidence(argv[2], argv[3], project, base, head_one, "pr-one.plan", evidence_one);
    evidence_digest(evidence_one, evidence_digest_one);

    suite_test_path(file, sizeof(file), project, "second.txt");
    write_text(file, "second review\n");
    git_commit("second PR", "second.txt");
    git_revision("HEAD", head_two, sizeof(head_two));
    strcpy(commit_two, head_two);
    char evidence_digest_two[65];
    capture_evidence(argv[2], argv[3], project, base, head_two, "pr-two.plan", evidence_two);
    evidence_digest(evidence_two, evidence_digest_two);
    write_manifest(base, head_one, head_two, commit_one, commit_two,
                   evidence_one, evidence_digest_one, evidence_two, evidence_digest_two);

    int cli_status = run_cli(argv[1]);
    if (cli_status) {
        char *diagnostics = read_text(tool_log);
        fprintf(stderr, "review-series exited %d: %s\n", cli_status, diagnostics);
        free(diagnostics);
    }
    CHECK(cli_status == 0, "review-series route rejected a valid real-Git series with exact-SHA evidence");
    CHECK(access(external_marker, F_OK) && errno == ENOENT,
          "Review series invoked a configured external diff helper");
    CHECK(access(textconv_marker, F_OK) && errno == ENOENT,
          "Review series invoked a configured textconv filter");
    char path[4096];
    suite_test_path(path, sizeof(path), route_dir, "0001-pr-one.patch");
    char *first = read_text(path);
    CHECK(strstr(first, "first.txt") && strstr(first, "first review") && !strstr(first, "second.txt"),
          "First review route entry was not the incremental PR diff");
    free(first);
    suite_test_path(path, sizeof(path), route_dir, "0002-pr-two.patch");
    char *second = read_text(path);
    CHECK(strstr(second, "second.txt") && strstr(second, "second review") && !strstr(second, "first.txt"),
          "Second review route entry repeated an earlier diff instead of showing its increment");
    free(second);
    suite_test_path(path, sizeof(path), route_dir, "route.md");
    char *route = read_text(path);
    CHECK(strstr(route, head_one) && strstr(route, head_two) && strstr(route, "pr-one") && strstr(route, "pr-two"),
          "Generated review route omitted its evidence-bound PR heads");
    CHECK(strstr(route, "local integrity and identity") && strstr(route, "not approval or fresh verification") &&
          strstr(route, "rerun the required suites at publication"),
          "Generated route overstated supplied evidence authority or omitted publication reruns");
    free(route);

    char target_path[4096], split_one_path[4096], split_two_path[4096], excluded_path[4096], folded_path[4096];
    suite_test_path(target_path, sizeof(target_path), project, "target.txt");
    write_text(target_path, "target change\n");
    git_commit("third PR target", "target.txt");
    char commit_target[64];
    git_revision("HEAD", commit_target, sizeof(commit_target));

    suite_test_path(split_one_path, sizeof(split_one_path), project, "split-one.txt");
    suite_test_path(split_two_path, sizeof(split_two_path), project, "split-two.txt");
    write_text(split_one_path, "first split part\n");
    write_text(split_two_path, "second split part\n");
    const char *split_paths[] = { "split-one.txt", "split-two.txt", NULL };
    git_commit_files("third PR split", split_paths);
    char commit_split[64];
    git_revision("HEAD", commit_split, sizeof(commit_split));

    suite_test_path(excluded_path, sizeof(excluded_path), project, "excluded.txt");
    write_text(excluded_path, "excluded noise\n");
    git_commit("third PR excluded", "excluded.txt");
    char commit_excluded[64];
    git_revision("HEAD", commit_excluded, sizeof(commit_excluded));

    suite_test_path(folded_path, sizeof(folded_path), project, "folded.txt");
    write_text(folded_path, "folded into target\n");
    git_commit("third PR folded fixup", "folded.txt");
    char head_three[64], commit_folded[64];
    git_revision("HEAD", head_three, sizeof(head_three));
    strcpy(commit_folded, head_three);
    char evidence_three[4096], evidence_digest_three[65];
    suite_test_path(evidence_three, sizeof(evidence_three), output_root, "evidence-pr-three");
    capture_evidence(argv[2], argv[3], project, base, head_three, "pr-three.plan", evidence_three);
    evidence_digest(evidence_three, evidence_digest_three);
    write_extended_manifest(base, head_one, head_two, head_three, evidence_one, evidence_digest_one,
                            evidence_two, evidence_digest_two, evidence_three, evidence_digest_three,
                            commit_one, commit_two, commit_target, commit_split, commit_excluded, commit_folded);

    char extended_route[4096];
    suite_test_path(extended_route, sizeof(extended_route), output_root, "extended-route");
    strcpy(route_dir, extended_route);
    cli_status = run_cli(argv[1]);
    if (cli_status) {
        char *diagnostics = read_text(tool_log);
        fprintf(stderr, "review-series exited %d: %s\n", cli_status, diagnostics);
        free(diagnostics);
    }
    CHECK(cli_status == 0, "review-series rejected complete fold, split, and reasoned exclusion decisions over real commits");
    suite_test_path(path, sizeof(path), extended_route, "0003-pr-three.patch");
    char *third = read_text(path);
    CHECK(strstr(third, "target.txt") && strstr(third, "folded.txt") &&
          !strstr(third, "excluded.txt") && !strstr(third, "split-one.txt"),
          "Folded and excluded commits were not routed with their target change");
    free(third);
    char split_patch[4096];
    char split_sha_prefix[9];
    memcpy(split_sha_prefix, commit_split, 8);
    split_sha_prefix[8] = '\0';
    int split_name_length = snprintf(split_patch, sizeof(split_patch), "%s/0003-pr-three-%s-part-one.patch",
                                     extended_route, split_sha_prefix);
    CHECK(split_name_length > 0 && split_name_length < (int)sizeof(split_patch), "Split patch path is too long");
    char *split_one = read_text(split_patch);
    CHECK(strstr(split_one, "split-one.txt") && !strstr(split_one, "split-two.txt"),
          "First split unit did not contain only its declared path");
    free(split_one);
    split_name_length = snprintf(split_patch, sizeof(split_patch), "%s/0003-pr-three-%s-part-two.patch",
                                 extended_route, split_sha_prefix);
    CHECK(split_name_length > 0 && split_name_length < (int)sizeof(split_patch), "Split patch path is too long");
    char *split_two = read_text(split_patch);
    CHECK(strstr(split_two, "split-two.txt") && !strstr(split_two, "split-one.txt"),
          "Second split unit did not contain only its declared path");
    free(split_two);
    suite_test_path(path, sizeof(path), extended_route, "route.md");
    route = read_text(path);
    CHECK(strstr(route, "removed generated fixture output") && strstr(route, "same review unit") &&
          strstr(route, "part-one") && strstr(route, "part-two"),
          "Generated review route omitted a required exclusion, fold, or split decision");
    free(route);

    char *valid_manifest = read_text(manifest);
    expect_cli_rejected(argv[1], "review-series-v9\n", "unsupported review series version");
    const size_t header_length = strlen("review-series-v1");
    const size_t valid_manifest_length = strlen(valid_manifest);
    const char nul_header_suffix[] = "\0ignored\n";
    const size_t nul_header_suffix_length = sizeof(nul_header_suffix) - 1;
    char *nul_manifest = malloc(valid_manifest_length + nul_header_suffix_length + 1);
    CHECK(nul_manifest != NULL, "Cannot allocate embedded-NUL manifest fixture");
    memcpy(nul_manifest, valid_manifest, header_length);
    memcpy(nul_manifest + header_length, nul_header_suffix, nul_header_suffix_length);
    memcpy(nul_manifest + header_length + nul_header_suffix_length,
           valid_manifest + header_length + 1, valid_manifest_length - header_length - 1);
    expect_cli_rejected_bytes(argv[1], nul_manifest,
                              valid_manifest_length + nul_header_suffix_length - 1,
                              "embedded NUL");
    free(nul_manifest);
    expect_cli_rejected_bytes(argv[1], valid_manifest, valid_manifest_length - 1,
                              "truncated manifest line");
    char *bad_record = replace_once(valid_manifest, "fold|", "unknown|fold|");
    expect_cli_rejected(argv[1], bad_record, "unsupported record type");
    free(bad_record);
    bad_record = replace_once(valid_manifest, "requires|pr-three|pr-two", "requires|pr-three|missing");
    expect_cli_rejected(argv[1], bad_record, "missing or duplicate PR prerequisite");
    free(bad_record);
    char mapping_line[128], replacement_mapping[128];
    int mapping_length = snprintf(mapping_line, sizeof(mapping_line), "commit|%s|pr-one\n", commit_one);
    CHECK(mapping_length > 0 && mapping_length < (int)sizeof(mapping_line), "Commit mapping row is too long");
    mapping_length = snprintf(replacement_mapping, sizeof(replacement_mapping), "commit|%s|pr-two\n", commit_one);
    CHECK(mapping_length > 0 && mapping_length < (int)sizeof(replacement_mapping), "Replacement commit mapping row is too long");
    bad_record = replace_once(valid_manifest, mapping_line, replacement_mapping);
    expect_cli_rejected(argv[1], bad_record, "not in its declared PR range");
    free(bad_record);
    bad_record = replace_once(valid_manifest, "requires|pr-three|pr-two\n",
                              "requires|pr-three|pr-two\nrequires|pr-one|pr-three\n");
    expect_cli_rejected(argv[1], bad_record, "prerequisite cycle");
    free(bad_record);
    char missing_commit[160];
    int missing_commit_length = snprintf(missing_commit, sizeof(missing_commit), "commit|%s|pr-three\n", commit_split);
    CHECK(missing_commit_length > 0 && missing_commit_length < (int)sizeof(missing_commit),
          "Missing commit test row is too long");
    bad_record = replace_once(valid_manifest, missing_commit, "");
    expect_cli_rejected(argv[1], bad_record, "commit accounting is incomplete");
    free(bad_record);
    char missing_split[160];
    int missing_split_length = snprintf(missing_split, sizeof(missing_split),
                                        "split|%s|split-two.txt|part-two\n", commit_split);
    CHECK(missing_split_length > 0 && missing_split_length < (int)sizeof(missing_split),
          "Missing split test row is too long");
    bad_record = replace_once(valid_manifest, missing_split, "");
    expect_cli_rejected(argv[1], bad_record, "split commit");
    free(bad_record);
    char bad_fold[192];
    int bad_fold_length = snprintf(bad_fold, sizeof(bad_fold), "fold|%s|%s|same review unit",
                                   commit_folded, commit_target);
    CHECK(bad_fold_length > 0 && bad_fold_length < (int)sizeof(bad_fold), "Fold test row is too long");
    char replacement_fold[160];
    int replacement_fold_length = snprintf(replacement_fold, sizeof(replacement_fold),
                                           "fold|%s|%s|same review unit", commit_folded, base);
    CHECK(replacement_fold_length > 0 && replacement_fold_length < (int)sizeof(replacement_fold),
          "Replacement fold test row is too long");
    bad_record = replace_once(valid_manifest, bad_fold, replacement_fold);
    expect_cli_rejected(argv[1], bad_record, "fold");
    free(bad_record);
    bad_record = replace_once(valid_manifest, "removed generated fixture output", "");
    expect_cli_rejected(argv[1], bad_record, "malformed fields");
    free(bad_record);
    bad_record = replace_once(valid_manifest, evidence_three, evidence_two);
    char *stale_digest_manifest = replace_once(bad_record, evidence_digest_three, evidence_digest_two);
    free(bad_record);
    bad_record = stale_digest_manifest;
    expect_cli_rejected(argv[1], bad_record, "not a passing report for its exact head");
    free(bad_record);
    char wrong_digest[65];
    memset(wrong_digest, '0', 64);
    wrong_digest[64] = '\0';
    bad_record = replace_once(valid_manifest, evidence_digest_three, wrong_digest);
    expect_cli_rejected(argv[1], bad_record, "evidence digest mismatch");
    free(bad_record);

    char evidence_file[4096], *evidence_contents;
    suite_test_path(evidence_file, sizeof(evidence_file), evidence_one, REVIEW_EVIDENCE_FILENAME);
    evidence_contents = read_text(evidence_file);
    char *tree_line = strstr(evidence_contents, "\ntree|");
    CHECK(tree_line != NULL, "Review evidence v2 does not contain its candidate tree boundary");
    tree_line++;
    char old_tree_line[80];
    int old_tree_length = snprintf(old_tree_line, sizeof(old_tree_line), "tree|%.40s\n", tree_line + 5);
    CHECK(old_tree_length == 46, "Review evidence candidate tree record is malformed");
    char fake_tree_line[] = "tree|0000000000000000000000000000000000000000\n";
    char *mutated_evidence = replace_once(evidence_contents, old_tree_line, fake_tree_line);
    char *seal_line = strstr(mutated_evidence, "seal|");
    CHECK(seal_line != NULL, "Review evidence v2 has no final seal");
    size_t prefix_length = (size_t)(seal_line - mutated_evidence);
    review_sha256 seal_hash;
    unsigned char seal_digest[32];
    char seal_text[65], replacement_seal[80];
    review_sha256_init(&seal_hash);
    review_sha256_update(&seal_hash, mutated_evidence, prefix_length);
    review_sha256_final(&seal_hash, seal_digest);
    review_sha256_hex(seal_digest, seal_text);
    int replacement_seal_length = snprintf(replacement_seal, sizeof(replacement_seal), "seal|%s\n", seal_text);
    CHECK(replacement_seal_length == 70, "Cannot reseal mutated review evidence");
    char *resealed_evidence = malloc(prefix_length + (size_t)replacement_seal_length + 1);
    CHECK(resealed_evidence != NULL, "Cannot allocate resealed review evidence");
    memcpy(resealed_evidence, mutated_evidence, prefix_length);
    memcpy(resealed_evidence + prefix_length, replacement_seal, (size_t)replacement_seal_length + 1);
    write_text(evidence_file, resealed_evidence);
    char mutated_digest[65];
    CHECK(!review_sha256_file(evidence_file, mutated_digest), "Cannot hash resealed review evidence");
    bad_record = replace_once(valid_manifest, evidence_digest_one, mutated_digest);
    expect_cli_rejected(argv[1], bad_record, "candidate tree does not match its real Git commit");
    free(bad_record);
    free(resealed_evidence);
    free(mutated_evidence);

    char base_tree_expression[80], head_two_tree_expression[80];
    char base_tree[64], head_two_tree[64], old_baseline_prefix[256], future_baseline_prefix[256];
    CHECK(snprintf(base_tree_expression, sizeof(base_tree_expression), "%s^{tree}", base) <
          (int)sizeof(base_tree_expression), "Base evidence tree revision is too long");
    CHECK(snprintf(head_two_tree_expression, sizeof(head_two_tree_expression), "%s^{tree}", head_two) <
          (int)sizeof(head_two_tree_expression), "Future evidence tree revision is too long");
    git_revision(base_tree_expression, base_tree, sizeof(base_tree));
    git_revision(head_two_tree_expression, head_two_tree, sizeof(head_two_tree));
    int old_prefix_length = snprintf(old_baseline_prefix, sizeof(old_baseline_prefix),
        "witness|addition-contract|baseline|%s|%s|", base, base_tree);
    int future_prefix_length = snprintf(future_baseline_prefix, sizeof(future_baseline_prefix),
        "witness|addition-contract|baseline|%s|%s|", head_two, head_two_tree);
    CHECK(old_prefix_length > 0 && old_prefix_length < (int)sizeof(old_baseline_prefix) &&
          future_prefix_length > 0 && future_prefix_length < (int)sizeof(future_baseline_prefix),
          "Cannot format mutated regression boundary");
    mutated_evidence = replace_once(evidence_contents, old_baseline_prefix, future_baseline_prefix);
    resealed_evidence = reseal_evidence(mutated_evidence);
    write_text(evidence_file, resealed_evidence);
    CHECK(!review_sha256_file(evidence_file, mutated_digest), "Cannot hash evidence with a future baseline");
    bad_record = replace_once(valid_manifest, evidence_digest_one, mutated_digest);
    expect_cli_rejected(argv[1], bad_record, "regression witness baseline is not an earlier ancestor");
    free(bad_record);
    free(resealed_evidence);
    free(mutated_evidence);
    write_text(evidence_file, evidence_contents);

    free(evidence_contents);
    free(valid_manifest);

    puts("PASS review series validates real Git ranges, strict manifests, exact evidence, PR dependencies, and route decisions");
    return EXIT_SUCCESS;
}
