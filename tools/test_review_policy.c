#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "test_paths.h"
#include "test_run_from.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static const char current_head[] = "1111111111111111111111111111111111111111";
static const char current_merge[] = "4444444444444444444444444444444444444444";
static const char current_base[] = "2222222222222222222222222222222222222222";
static char fixture_directory[4096];
static char output_directory[4096];
static char gh_log[4096];
static char cli_log[4096];
static long last_transport_group;
static bool last_cli_saw_transport_group;
static bool last_cli_saw_transport_descendant;

#define TEST_CLI_TIMEOUT_MS 4000
#define TEST_CLI_TIMED_OUT 124
static long long last_cli_elapsed_ms;

static void path_join(char *path, size_t capacity, const char *root, const char *part)
{
    suite_test_path(path, capacity, root, part);
}

static void cleanup_output(void)
{
    if (output_directory[0] && !suite_test_paths_remove(output_directory)) {
        fprintf(stderr, "Could not clean review policy test output %s\n", output_directory);
        _Exit(EXIT_FAILURE);
    }
}

static void write_text(const char *name, const char *contents)
{
    char path[4096];
    path_join(path, sizeof(path), fixture_directory, name);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create offline GitHub protocol fixture");
    size_t length = strlen(contents);
    CHECK(fwrite(contents, 1, length, file) == length, "Cannot write offline GitHub protocol fixture");
    CHECK(!fclose(file), "Cannot close offline GitHub protocol fixture");
}

static long read_pid_file(const char *name)
{
    char path[4096];
    path_join(path, sizeof(path), fixture_directory, name);
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    long pid = 0;
    (void)fscanf(file, "%ld", &pid);
    fclose(file);
    return pid;
}

static void expect_transport_group_gone(void)
{
    CHECK(last_cli_saw_transport_group, "Fake GitHub CLI did not run in its owned process group");
    CHECK(last_cli_saw_transport_descendant, "Fake GitHub CLI did not start a child in its owned process group");
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (kill(-last_transport_group, 0) < 0 && errno == ESRCH) return;
        struct timespec delay = { .tv_sec = 0, .tv_nsec = 10000000 };
        nanosleep(&delay, NULL);
    }
    (void)kill(-last_transport_group, SIGKILL);
    CHECK(false, "Review policy left its GitHub CLI process group running");
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL, "Cannot read review policy test output");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek review policy test output");
    long length = ftell(file);
    CHECK(length >= 0 && length < 1024 * 1024, "Review policy test output is too large");
    CHECK(!fseek(file, 0, SEEK_SET), "Cannot rewind review policy test output");
    char *text = calloc((size_t)length + 1, 1);
    CHECK(text != NULL, "Cannot allocate review policy test output");
    CHECK(fread(text, 1, (size_t)length, file) == (size_t)length, "Cannot read review policy test output");
    CHECK(!fclose(file), "Cannot close review policy test output");
    return text;
}

static void write_protocol_fixture(void)
{
    char json[8192];
    int length;
    char protection_marker[4096];
    path_join(protection_marker, sizeof(protection_marker), fixture_directory, "protection_404");
    if (unlink(protection_marker) && errno != ENOENT) CHECK(false, "Cannot reset protection fixture");
    const char *reset_files[] = {
        "late-pull.json", "late-decision.json", "late-reviews.json",
        "pull-call-count", "graphql-call-count", "reviews-call-count",
        "transport-continuous", "transport-oversize-hold", "transport-child.pid",
        "transport-grandchild.pid"
    };
    for (size_t i = 0; i < sizeof(reset_files) / sizeof(reset_files[0]); ++i) {
        path_join(protection_marker, sizeof(protection_marker), fixture_directory, reset_files[i]);
        if (unlink(protection_marker) && errno != ENOENT) CHECK(false, "Cannot reset late protocol fixture");
    }

    length = snprintf(json, sizeof(json),
        "{\"number\":41,\"state\":\"open\",\"draft\":false,\"mergeable\":true,\"mergeable_state\":\"clean\","
        "\"merge_commit_sha\":\"4444444444444444444444444444444444444444\",\"user\":{\"login\":\"pull-author\"},"
        "\"head\":{\"sha\":\"%s\",\"ref\":\"feature\"},"
        "\"base\":{\"sha\":\"%s\",\"ref\":\"main\"}}",
        current_head, current_base);
    CHECK(length > 0 && length < (int)sizeof(json), "Pull request fixture is too large");
    write_text("pull.json", json);

    length = snprintf(json, sizeof(json),
        "{\"data\":{\"repository\":{\"pullRequest\":{"
        "\"reviewDecision\":\"APPROVED\",\"mergeable\":\"MERGEABLE\",\"mergeStateStatus\":\"CLEAN\","
        "\"baseRefOid\":\"%s\",\"headRefOid\":\"%s\"}}}}",
        current_base, current_head);
    CHECK(length > 0 && length < (int)sizeof(json), "Review decision fixture is too large");
    write_text("decision.json", json);

    length = snprintf(json, sizeof(json),
        "[[],[{\"id\":1,\"user\":{\"login\":\"wizardmanannan\"},\"author_association\":\"OWNER\","
        "\"state\":\"APPROVED\",\"commit_id\":\"%s\","
        "\"submitted_at\":\"2026-10-18T00:00:00Z\"},"
        "{\"id\":2,\"user\":{\"login\":\"wizardmanannan\"},"
        "\"state\":\"COMMENTED\",\"commit_id\":\"%s\","
        "\"submitted_at\":\"2026-10-18T00:01:00Z\"}]]",
        current_head, current_head);
    CHECK(length > 0 && length < (int)sizeof(json), "Review fixture is too large");
    write_text("reviews.json", json);
    length = snprintf(json, sizeof(json),
        "{\"name\":\"main\",\"commit\":{\"sha\":\"%s\"}}", current_base);
    CHECK(length > 0 && length < (int)sizeof(json), "Branch fixture is too large");
    write_text("branch.json", json);

    length = snprintf(json, sizeof(json),
        "[{\"total_count\":0,\"check_runs\":[]},{\"total_count\":1,\"check_runs\":[{\"id\":71,\"name\":\"native-ready\","
        "\"head_sha\":\"%s\",\"status\":\"completed\",\"conclusion\":\"success\","
        "\"app\":{\"id\":900001,\"slug\":\"github-actions\",\"name\":\"Offline protocol fixture\"},"
        "\"check_suite\":{\"id\":70,\"head_sha\":\"%s\"},"
        "\"pull_requests\":[{\"number\":41,\"head\":{\"sha\":\"%s\"},"
        "\"base\":{\"sha\":\"%s\"}}]}]}]",
        current_merge, current_merge, current_head, current_base);
    CHECK(length > 0 && length < (int)sizeof(json), "Check run fixture is too large");
    write_text("checks.json", json);

    write_text("protection.json",
        "{\"required_status_checks\":{\"strict\":true,\"contexts\":[\"native-ready\"],"
        "\"checks\":[{\"context\":\"native-ready\",\"app_id\":900001}]},"
        "\"required_pull_request_reviews\":{\"dismiss_stale_reviews\":true,"
        "\"require_code_owner_reviews\":true,\"required_approving_review_count\":1,"
        "\"require_last_push_approval\":true,\"bypass_pull_request_allowances\":{"
        "\"users\":[],\"teams\":[],\"apps\":[]}},"
        "\"enforce_admins\":{\"enabled\":true},"
        "\"required_conversation_resolution\":{\"enabled\":true},"
        "\"allow_force_pushes\":{\"enabled\":false},"
        "\"allow_deletions\":{\"enabled\":false}}");
}

static void install_fake_gh(void)
{
    static const char script[] =
        "#!/bin/sh\n"
        "set -eu\n"
        "printf '%s\\n' \"$*\" >> \"$REVIEW_POLICY_CALL_LOG\"\n"
        "if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/transport-continuous\" ]; then\n"
        "  printf '%s\\n' \"$$\" > \"$REVIEW_POLICY_FIXTURE_DIR/transport-child.pid\"\n"
        "  sleep 30 & printf '%s\\n' \"$!\" > \"$REVIEW_POLICY_FIXTURE_DIR/transport-grandchild.pid\"\n"
        "  while :; do printf '%64s' x; sleep 0.05; done\n"
        "fi\n"
        "if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/transport-oversize-hold\" ]; then\n"
        "  printf '%s\\n' \"$$\" > \"$REVIEW_POLICY_FIXTURE_DIR/transport-child.pid\"\n"
        "  sleep 30 & printf '%s\\n' \"$!\" > \"$REVIEW_POLICY_FIXTURE_DIR/transport-grandchild.pid\"\n"
        "  dd if=/dev/zero bs=65536 count=513 2>/dev/null || true\n"
        "  wait\n"
        "fi\n"
        "if [ \"${2:-}\" = graphql ]; then\n"
        "  count=0; if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/graphql-call-count\" ]; then read count < \"$REVIEW_POLICY_FIXTURE_DIR/graphql-call-count\"; fi\n"
        "  count=$((count + 1)); printf '%s\\n' \"$count\" > \"$REVIEW_POLICY_FIXTURE_DIR/graphql-call-count\"\n"
        "  if [ \"$count\" -gt 1 ] && [ -f \"$REVIEW_POLICY_FIXTURE_DIR/late-decision.json\" ]; then cat \"$REVIEW_POLICY_FIXTURE_DIR/late-decision.json\"; else cat \"$REVIEW_POLICY_FIXTURE_DIR/decision.json\"; fi\n"
        "  exit 0\n"
        "fi\n"
        "endpoint=\n"
        "for argument do endpoint=$argument; done\n"
        "case \"$endpoint\" in\n"
        "  */pulls/41)\n"
        "    count=0; if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/pull-call-count\" ]; then read count < \"$REVIEW_POLICY_FIXTURE_DIR/pull-call-count\"; fi\n"
        "    count=$((count + 1)); printf '%s\\n' \"$count\" > \"$REVIEW_POLICY_FIXTURE_DIR/pull-call-count\"\n"
        "    if [ \"$count\" -gt 1 ] && [ -f \"$REVIEW_POLICY_FIXTURE_DIR/late-pull.json\" ]; then cat \"$REVIEW_POLICY_FIXTURE_DIR/late-pull.json\"; else cat \"$REVIEW_POLICY_FIXTURE_DIR/pull.json\"; fi ;;\n"
        "  */pulls/41/reviews*)\n"
        "    count=0; if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/reviews-call-count\" ]; then read count < \"$REVIEW_POLICY_FIXTURE_DIR/reviews-call-count\"; fi\n"
        "    count=$((count + 1)); printf '%s\\n' \"$count\" > \"$REVIEW_POLICY_FIXTURE_DIR/reviews-call-count\"\n"
        "    if [ \"$count\" -gt 1 ] && [ -f \"$REVIEW_POLICY_FIXTURE_DIR/late-reviews.json\" ]; then cat \"$REVIEW_POLICY_FIXTURE_DIR/late-reviews.json\"; else cat \"$REVIEW_POLICY_FIXTURE_DIR/reviews.json\"; fi ;;\n"
        "  */check-runs*) cat \"$REVIEW_POLICY_FIXTURE_DIR/checks.json\" ;;\n"
        "  */contents/tools/review/branch-protection.json*) cat \"$REVIEW_POLICY_FIXTURE_DIR/base-policy.json\" ;;\n"
        "  */branches/main) cat \"$REVIEW_POLICY_FIXTURE_DIR/branch.json\" ;;\n"
        "  */branches/main/protection) if [ -f \"$REVIEW_POLICY_FIXTURE_DIR/protection_404\" ]; then echo 'gh: Not Found (HTTP 404)' >&2; exit 1; fi; cat \"$REVIEW_POLICY_FIXTURE_DIR/protection.json\" ;;\n"
        "  *) echo \"Unexpected offline GitHub API request: $*\" >&2; exit 90 ;;\n"
        "esac\n";
    char path[4096];
    path_join(path, sizeof(path), fixture_directory, "gh");
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL, "Cannot create offline gh protocol fixture");
    CHECK(fwrite(script, 1, sizeof(script) - 1, file) == sizeof(script) - 1, "Cannot write offline gh protocol fixture");
    CHECK(!fclose(file), "Cannot close offline gh protocol fixture");
    CHECK(!chmod(path, 0700), "Cannot enable offline gh protocol fixture");
}

static void write_one_review(const char *login, const char *state, const char *commit,
                             bool add_later_comment, bool forged_approved_field)
{
    char json[4096];
    const char *association = !strcmp(login, "wizardmanannan") ? "OWNER" :
        !strcmp(login, "pull-author") ? "CONTRIBUTOR" : "NONE";
    int length = snprintf(json, sizeof(json),
        "[[],[{\"id\":1,\"user\":{\"login\":\"%s\"},\"author_association\":\"%s\",\"state\":\"%s\","
        "\"commit_id\":\"%s\",\"submitted_at\":\"2026-10-18T00:00:00Z\"%s}%s]]",
        login, association, state, commit, forged_approved_field ? ",\"approved\":true" : "",
        add_later_comment ? ", {\"id\":2,\"user\":{\"login\":\"wizardmanannan\"},\"state\":\"COMMENTED\",\"commit_id\":\"1111111111111111111111111111111111111111\",\"submitted_at\":\"2026-10-18T00:01:00Z\"}" : "");
    CHECK(length > 0 && length < (int)sizeof(json), "Review protocol fixture is too large");
    write_text("reviews.json", json);
}

static void write_review_pair(const char *first_state, const char *second_state)
{
    char json[4096];
    int length = snprintf(json, sizeof(json),
        "[[],[{\"id\":1,\"user\":{\"login\":\"wizardmanannan\"},\"state\":\"%s\","
        "\"author_association\":\"OWNER\","
        "\"commit_id\":\"%s\",\"submitted_at\":\"2026-10-18T00:00:00Z\"},"
        "{\"id\":2,\"user\":{\"login\":\"wizardmanannan\"},\"author_association\":\"OWNER\",\"state\":\"%s\","
        "\"commit_id\":\"%s\",\"submitted_at\":\"2026-10-18T00:01:00Z\"}]]",
        first_state, current_head, second_state, current_head);
    CHECK(length > 0 && length < (int)sizeof(json), "Review protocol fixture is too large");
    write_text("reviews.json", json);
}

static void write_check_run(const char *run_head, const char *suite_head, const char *run_base,
                            const char *status, const char *conclusion, long long app_id,
                            bool duplicate, bool omit)
{
    char json[8192];
    int length;
    if (omit) {
        write_text("checks.json", "[{\"total_count\":0,\"check_runs\":[]}, {\"total_count\":0,\"check_runs\":[]}]");
        return;
    }
    if (duplicate) {
        length = snprintf(json, sizeof(json),
            "[{\"total_count\":0,\"check_runs\":[]},{\"total_count\":2,\"check_runs\":[{"
            "\"id\":71,\"name\":\"native-ready\",\"head_sha\":\"%s\",\"status\":\"%s\",\"conclusion\":\"%s\","
            "\"app\":{\"id\":%lld,\"slug\":\"github-actions\",\"name\":\"Offline protocol fixture\"},"
            "\"check_suite\":{\"id\":70,\"head_sha\":\"%s\"},"
            "\"pull_requests\":[{\"number\":41,\"head\":{\"sha\":\"%s\"},\"base\":{\"sha\":\"%s\"}}]},"
            "{\"id\":72,\"name\":\"native-ready\",\"head_sha\":\"%s\",\"status\":\"%s\",\"conclusion\":\"%s\","
            "\"app\":{\"id\":%lld,\"slug\":\"github-actions\",\"name\":\"Offline protocol fixture\"},"
            "\"check_suite\":{\"id\":73,\"head_sha\":\"%s\"},"
            "\"pull_requests\":[{\"number\":41,\"head\":{\"sha\":\"%s\"},\"base\":{\"sha\":\"%s\"}}]}]}]",
            run_head, status, conclusion, app_id, suite_head, current_head, run_base,
            run_head, status, conclusion, app_id, suite_head, current_head, run_base);
    } else {
        length = snprintf(json, sizeof(json),
            "[{\"total_count\":0,\"check_runs\":[]},{\"total_count\":1,\"check_runs\":[{"
            "\"id\":71,\"name\":\"native-ready\",\"head_sha\":\"%s\",\"status\":\"%s\",\"conclusion\":\"%s\","
            "\"app\":{\"id\":%lld,\"slug\":\"github-actions\",\"name\":\"Offline protocol fixture\"},"
            "\"check_suite\":{\"id\":70,\"head_sha\":\"%s\"},"
            "\"pull_requests\":[{\"number\":41,\"head\":{\"sha\":\"%s\"},\"base\":{\"sha\":\"%s\"}}]}]}]",
            run_head, status, conclusion, app_id, suite_head, current_head, run_base);
    }
    CHECK(length > 0 && length < (int)sizeof(json), "Check run protocol fixture is too large");
    write_text("checks.json", json);
}

static void write_policy_variant(const char *policy_path, const char *name, const char *addition)
{
    char *original = read_text(policy_path);
    size_t length = strlen(original);
    size_t close = length;
    while (close && isspace((unsigned char)original[close - 1])) --close;
    CHECK(close && original[close - 1] == '}', "Policy file does not end with a JSON object");
    char *variant = calloc(length + strlen(addition) + 8, 1);
    CHECK(variant != NULL, "Cannot allocate policy protocol fixture");
    memcpy(variant, original, close - 1);
    snprintf(variant + close - 1, length + strlen(addition) + 8 - (close - 1), "%s}\n", addition);
    write_text(name, variant);
    free(original);
    free(variant);
}

static int run_cli(const char *tool, const char *policy, bool forged_approval_option)
{
    last_transport_group = 0;
    last_cli_saw_transport_group = false;
    last_cli_saw_transport_descendant = false;
    char *argv[16];
    size_t count = 0;
    argv[count++] = (char *)tool;
    argv[count++] = (char *)"check";
    argv[count++] = (char *)"--repo"; argv[count++] = (char *)"wizardmanannan/CX16Agi";
    argv[count++] = (char *)"--pr"; argv[count++] = (char *)"41";
    argv[count++] = (char *)"--head"; argv[count++] = (char *)current_head;
    argv[count++] = (char *)"--base"; argv[count++] = (char *)current_base;
    argv[count++] = (char *)"--policy"; argv[count++] = (char *)policy;
    if (forged_approval_option) { argv[count++] = (char *)"--approved"; argv[count++] = (char *)"true"; }
    argv[count] = NULL;
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork review policy CLI");
    if (!child) {
        int descriptor = open(cli_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
            dup2(descriptor, STDERR_FILENO) < 0) _Exit(125);
        close(descriptor);
        execvp(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute review policy CLI: %s\n", strerror(errno));
        _Exit(127);
    }
    int status;
    struct timespec started, now;
    CHECK(!clock_gettime(CLOCK_MONOTONIC, &started), "Cannot read CLI test start time");
    for (;;) {
        long transport_pid = read_pid_file("transport-child.pid");
        long descendant_pid = read_pid_file("transport-grandchild.pid");
        if (transport_pid > 1) {
            last_transport_group = transport_pid;
            if (getpgid((pid_t)transport_pid) == (pid_t)transport_pid) {
                last_cli_saw_transport_group = true;
                if (descendant_pid > 1 && getpgid((pid_t)descendant_pid) == (pid_t)transport_pid)
                    last_cli_saw_transport_descendant = true;
            }
        }
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        CHECK(waited == 0 || (waited < 0 && errno == EINTR), "Cannot wait for review policy CLI");
        CHECK(!clock_gettime(CLOCK_MONOTONIC, &now), "Cannot read CLI test time");
        last_cli_elapsed_ms = (long long)(now.tv_sec - started.tv_sec) * 1000 +
            (now.tv_nsec - started.tv_nsec) / 1000000;
        if (last_cli_elapsed_ms >= TEST_CLI_TIMEOUT_MS) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            if (last_cli_saw_transport_group) (void)kill((pid_t)-last_transport_group, SIGKILL);
            long descendant = read_pid_file("transport-grandchild.pid");
            if (descendant > 1) (void)kill((pid_t)descendant, SIGKILL);
            if (transport_pid > 1) (void)kill((pid_t)transport_pid, SIGKILL);
            return TEST_CLI_TIMED_OUT;
        }
        struct timespec delay = { .tv_sec = 0, .tv_nsec = 10000000 };
        nanosleep(&delay, NULL);
    }
    CHECK(!clock_gettime(CLOCK_MONOTONIC, &now), "Cannot read CLI test finish time");
    last_cli_elapsed_ms = (long long)(now.tv_sec - started.tv_sec) * 1000 +
        (now.tv_nsec - started.tv_nsec) / 1000000;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

static int run_payload_cli(const char *tool, const char *policy)
{
    char *argv[] = {
        (char *)tool, (char *)"protection-payload", (char *)"--repo", (char *)"wizardmanannan/CX16Agi",
        (char *)"--policy", (char *)policy, NULL
    };
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork branch protection payload command");
    if (!child) {
        int descriptor = open(cli_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
            dup2(descriptor, STDERR_FILENO) < 0) _Exit(125);
        close(descriptor);
        execvp(argv[0], argv);
        dprintf(STDERR_FILENO, "Cannot execute branch protection payload command: %s\n", strerror(errno));
        _Exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for branch protection payload command");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

static void expect_rejected(const char *tool, const char *policy, const char *diagnostic,
                            bool forged_approval_option)
{
    int status = run_cli(tool, policy, forged_approval_option);
    if (!status) {
        fprintf(stderr, "Review policy accepted a fixture expected to fail: %s\n", diagnostic);
        exit(EXIT_FAILURE);
    }
    char *output = read_text(cli_log);
    CHECK(strstr(output, diagnostic) != NULL, "Review policy returned the wrong rejection reason");
    free(output);
}

static size_t count_occurrences(const char *text, const char *needle)
{
    size_t count = 0, length = strlen(needle);
    if (!length) return 0;
    while ((text = strstr(text, needle)) != NULL) { ++count; text += length; }
    return count;
}

static void replace_fixture_text(const char *name, const char *before, const char *after)
{
    char path[4096];
    path_join(path, sizeof(path), fixture_directory, name);
    char *original = read_text(path);
    char *match = strstr(original, before);
    CHECK(match != NULL, "Expected text is missing from protocol fixture");
    size_t prefix = (size_t)(match - original);
    size_t after_length = strlen(after);
    size_t suffix = strlen(match + strlen(before));
    char *replacement = calloc(prefix + after_length + suffix + 1, 1);
    CHECK(replacement != NULL, "Cannot allocate protocol fixture replacement");
    memcpy(replacement, original, prefix);
    memcpy(replacement + prefix, after, after_length);
    memcpy(replacement + prefix + after_length, match + strlen(before), suffix);
    write_text(name, replacement);
    free(replacement);
    free(original);
}

int main(int argc, char **argv)
{
    bool witness_mode = argc == 4 && !strcmp(argv[3], "--witness");
    CHECK(argc == 3 || witness_mode, "Expected review-policy executable path and policy file path");
    suite_test_paths_init("review-policy-protocol");
    if (witness_mode) {
        fputs("WITNESS_EXECUTED: check-run-suite-head-optional\n", stderr);
        fflush(stderr);
    }
    CHECK(snprintf(fixture_directory, sizeof(fixture_directory), "%s", suite_fixture_directory) <
          (int)sizeof(fixture_directory), "Review policy fixture path is too long");
    CHECK(snprintf(output_directory, sizeof(output_directory), "%s/review-policy-test-XXXXXX", suite_output_directory) <
          (int)sizeof(output_directory), "Review policy output path is too long");
    CHECK(mkdtemp(output_directory) != NULL, "Cannot create review policy suite output");
    CHECK(!atexit(cleanup_output), "Cannot register review policy output cleanup");
    path_join(gh_log, sizeof(gh_log), output_directory, "gh-argv.log");
    path_join(cli_log, sizeof(cli_log), output_directory, "review-policy.log");

    char *policy_text = read_text(argv[2]);
    write_text("base-policy.json", policy_text);
    free(policy_text);

    write_protocol_fixture();
    install_fake_gh();
    const char *old_path = getenv("PATH");
    char path[8192];
    CHECK(snprintf(path, sizeof(path), "%s:%s", fixture_directory, old_path ? old_path : "") < (int)sizeof(path),
          "Cannot prepare offline GitHub fixture path");
    CHECK(!setenv("PATH", path, 1), "Cannot set offline GitHub fixture path");
    CHECK(!setenv("REVIEW_POLICY_FIXTURE_DIR", fixture_directory, 1), "Cannot select offline GitHub fixtures");
    CHECK(!setenv("REVIEW_POLICY_CALL_LOG", gh_log, 1), "Cannot record offline GitHub API requests");

    write_protocol_fixture();
    /* GitHub check-run responses expose a suite id, not a nested suite head SHA. */
    replace_fixture_text("checks.json", "\"check_suite\":{\"id\":70,\"head_sha\":\"4444444444444444444444444444444444444444\"}",
                         "\"check_suite\":{\"id\":70}");
    char *cli_argv[] = {
        argv[1], (char *)"check", (char *)"--repo", (char *)"wizardmanannan/CX16Agi",
        (char *)"--pr", (char *)"41", (char *)"--head", (char *)current_head,
        (char *)"--base", (char *)current_base, (char *)"--policy", argv[2], NULL
    };
    run_from(cli_argv, cli_log, 30, NULL);
    char *output = read_text(cli_log);
    if (witness_mode) {
        if (strstr(output, "NOT READY: check run has incomplete identity or App data")) {
            fputs(output, stderr);
            free(output);
            return EXIT_FAILURE;
        }
        if (strstr(output, "READY")) {
            free(output);
            puts("POLICY_WITNESS_PASS: check-run-suite-head-optional");
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "Unexpected review-policy witness output:\n%s", output);
        free(output);
        return 2;
    }
    CHECK(strstr(output, "READY") != NULL, "Review policy CLI omitted its ready result");
    free(output);
    output = read_text(gh_log);
    CHECK(strstr(output, "pulls/41/reviews") != NULL && strstr(output, "check-runs") != NULL,
          "Review policy CLI did not fetch reviews and checks from GitHub");
    CHECK(strstr(output, "contents/tools/review/branch-protection.json?ref=2222222222222222222222222222222222222222") != NULL,
          "Review policy CLI did not load its authority from the exact trusted base commit");
    CHECK(count_occurrences(output, "--paginate --slurp") >= 2,
          "Review policy CLI did not paginate every review and check-run page");
    CHECK(strstr(output, "--method") == NULL && strstr(output, "--input") == NULL,
          "Readiness command attempted a GitHub write");
    free(output);

    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"merge_commit_sha\":\"4444444444444444444444444444444444444444\"", "\"merge_commit_sha\":null");
    expect_rejected(argv[1], argv[2], "integration revision is unavailable", false);

    write_protocol_fixture();
    write_check_run(current_base, current_base, current_base, "completed", "success", 900001, false, false);
    CHECK(run_payload_cli(argv[1], argv[2]) == 0,
          "Protection payload command did not derive a payload from a live-shaped check-run fixture");
    output = read_text(cli_log);
    CHECK(strstr(output, "\"strict\":true") != NULL &&
          strstr(output, "\"context\":\"native-ready\",\"app_id\":900001") != NULL,
          "Protection payload omitted strict checks or the observed fixture App ID");
    free(output);
    write_protocol_fixture();
    write_check_run(current_base, current_base, current_base, "completed", "success", 900001, false, true);
    CHECK(run_payload_cli(argv[1], argv[2]) == 2,
          "Protection payload was generated without an observed required check run");
    output = read_text(cli_log);
    CHECK(strstr(output, "required check is missing") != NULL,
          "Missing live check run returned the wrong protection payload diagnostic");
    free(output);

    write_protocol_fixture();
    write_one_review("pull-author", "APPROVED", current_head, false, false);
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_one_review("outside-reader", "APPROVED", current_head, false, false);
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_one_review("outside-reader", "COMMENTED", current_head, false, true);
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_one_review("wizardmanannan", "APPROVED", "3333333333333333333333333333333333333333", false, false);
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_review_pair("APPROVED", "CHANGES_REQUESTED");
    expect_rejected(argv[1], argv[2], "active current-head changes request", false);

    write_protocol_fixture();
    write_review_pair("APPROVED", "DISMISSED");
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_text("decision.json",
        "{\"data\":{\"repository\":{\"pullRequest\":{\"reviewDecision\":\"CHANGES_REQUESTED\","
        "\"mergeable\":\"MERGEABLE\",\"mergeStateStatus\":\"CLEAN\","
        "\"baseRefOid\":\"2222222222222222222222222222222222222222\","
        "\"headRefOid\":\"1111111111111111111111111111111111111111\"}}}}");
    expect_rejected(argv[1], argv[2], "reviewDecision is not APPROVED", false);

    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"mergeable\":true", "\"mergeable\":false");
    expect_rejected(argv[1], argv[2], "pull request is not mergeable", false);
    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"mergeable\":true", "\"mergeable\":null");
    expect_rejected(argv[1], argv[2], "pull request mergeability is unknown", false);
    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"mergeable_state\":\"clean\"", "\"mergeable_state\":\"unknown\"");
    expect_rejected(argv[1], argv[2], "pull request merge state is not clean", false);
    write_protocol_fixture();
    replace_fixture_text("decision.json", "\"mergeable\":\"MERGEABLE\"", "\"mergeable\":\"CONFLICTING\"");
    expect_rejected(argv[1], argv[2], "GitHub reports pull request is not mergeable", false);
    write_protocol_fixture();
    replace_fixture_text("decision.json", "\"mergeStateStatus\":\"CLEAN\"", "\"mergeStateStatus\":\"UNKNOWN\"");
    expect_rejected(argv[1], argv[2], "GitHub merge state is not CLEAN", false);
    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"draft\":false", "\"draft\":true");
    expect_rejected(argv[1], argv[2], "pull request is not open and ready for review", false);
    write_protocol_fixture();
    replace_fixture_text("pull.json", "\"state\":\"open\"", "\"state\":\"closed\"");
    expect_rejected(argv[1], argv[2], "pull request is not open and ready for review", false);

    write_protocol_fixture();
    char late_pull_path[4096];
    path_join(late_pull_path, sizeof(late_pull_path), fixture_directory, "pull.json");
    char *late_pull = read_text(late_pull_path);
    write_text("late-pull.json", late_pull);
    free(late_pull);
    replace_fixture_text("late-pull.json", current_head, "3333333333333333333333333333333333333333");
    expect_rejected(argv[1], argv[2], "head or base changed", false);

    write_protocol_fixture();
    late_pull = read_text(late_pull_path);
    write_text("late-pull.json", late_pull);
    free(late_pull);
    replace_fixture_text("late-pull.json", current_merge, "5555555555555555555555555555555555555555");
    expect_rejected(argv[1], argv[2], "integration revision changed", false);

    write_protocol_fixture();
    write_check_run(current_head, current_head, current_base, "completed", "success", 900001, false, false);
    expect_rejected(argv[1], argv[2], "stale or replayed run", false);

    write_protocol_fixture();
    char late_decision_path[4096];
    path_join(late_decision_path, sizeof(late_decision_path), fixture_directory, "decision.json");
    char *late_decision = read_text(late_decision_path);
    write_text("late-decision.json", late_decision);
    free(late_decision);
    replace_fixture_text("late-decision.json", "\"reviewDecision\":\"APPROVED\"",
                         "\"reviewDecision\":\"CHANGES_REQUESTED\"");
    expect_rejected(argv[1], argv[2], "GitHub server reviewDecision is no longer APPROVED", false);

    write_protocol_fixture();
    char late_reviews_path[4096];
    path_join(late_reviews_path, sizeof(late_reviews_path), fixture_directory, "reviews.json");
    char *late_reviews = read_text(late_reviews_path);
    write_text("late-reviews.json", late_reviews);
    free(late_reviews);
    replace_fixture_text("late-reviews.json", "\"state\":\"APPROVED\"",
                         "\"state\":\"DISMISSED\"");
    expect_rejected(argv[1], argv[2], "no trusted, non-author approval", false);

    write_protocol_fixture();
    write_text("transport-continuous", "stream\n");
    int transport_status = run_cli(argv[1], argv[2], false);
    CHECK(transport_status == 2, "Review policy did not fail closed on a continuous GitHub CLI response");
    CHECK(last_cli_elapsed_ms < 3000, "Review policy did not apply its wall-clock transport deadline during output");
    output = read_text(cli_log);
    CHECK(strstr(output, "could not read GitHub CLI response") != NULL,
          "Continuous GitHub CLI output returned the wrong transport diagnostic");
    free(output);
    expect_transport_group_gone();

    write_protocol_fixture();
    write_text("transport-oversize-hold", "oversize\n");
    transport_status = run_cli(argv[1], argv[2], false);
    CHECK(transport_status == 2, "Review policy did not return after an oversized GitHub CLI response");
    CHECK(last_cli_elapsed_ms < 3000, "Review policy blocked while reaping an oversized-response child");
    output = read_text(cli_log);
    CHECK(strstr(output, "could not read GitHub CLI response") != NULL,
          "Oversized GitHub CLI output returned the wrong transport diagnostic");
    free(output);
    expect_transport_group_gone();

    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base, "completed", "skipped", 900001, false, false);
    expect_rejected(argv[1], argv[2], "required check is not successful", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base, "completed", "cancelled", 900001, false, false);
    expect_rejected(argv[1], argv[2], "required check is not successful", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base, "completed", "neutral", 900001, false, false);
    expect_rejected(argv[1], argv[2], "required check is not successful", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base, "completed", "success", 900001, false, true);
    expect_rejected(argv[1], argv[2], "required check is missing", false);

    write_protocol_fixture();
    write_check_run("3333333333333333333333333333333333333333", "3333333333333333333333333333333333333333",
                    current_base, "completed", "success", 900001, false, false);
    expect_rejected(argv[1], argv[2], "stale or replayed run", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base,
                    "completed", "success", 900001, false, false);
    replace_fixture_text("checks.json", "\"check_suite\":{\"id\":70", "\"check_suite\":{\"id\":0");
    expect_rejected(argv[1], argv[2], "incomplete identity or App data", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, "3333333333333333333333333333333333333333",
                    "completed", "success", 900001, false, false);
    expect_rejected(argv[1], argv[2], "not tied to the current pull request base and head", false);
    write_protocol_fixture();
    write_check_run(current_merge, current_merge, current_base, "completed", "success", 900001, true, false);
    expect_rejected(argv[1], argv[2], "multiple current runs", false);

    write_protocol_fixture();
    write_check_run(current_base, current_base, current_base, "completed", "success", 900001, false, false);
    replace_fixture_text("checks.json", "\"slug\":\"github-actions\"", "\"slug\":\"third-party-app\"");
    CHECK(run_payload_cli(argv[1], argv[2]) == 2,
          "Protection payload accepted a successful required context from an untrusted provider");
    output = read_text(cli_log);
    CHECK(strstr(output, "check run provider does not match trusted policy") != NULL,
          "Untrusted check provider returned the wrong protection payload diagnostic");
    free(output);

    write_protocol_fixture();
    replace_fixture_text("checks.json", "\"slug\":\"github-actions\"", "\"slug\":\"third-party-app\"");
    expect_rejected(argv[1], argv[2], "check run provider does not match trusted policy", false);
    write_protocol_fixture();
    replace_fixture_text("checks.json", "\"slug\":\"github-actions\",", "");
    expect_rejected(argv[1], argv[2], "check run is missing its provider slug", false);

    write_protocol_fixture();
    write_text("protection_404", "missing\n");
    expect_rejected(argv[1], argv[2], "branch policy is not active", false);
    write_protocol_fixture();
    replace_fixture_text("protection.json", "900001", "900002");
    expect_rejected(argv[1], argv[2], "does not match the current GitHub check run", false);
    write_protocol_fixture();
    replace_fixture_text("protection.json", "\"strict\":true", "\"strict\":false");
    expect_rejected(argv[1], argv[2], "required checks are missing or not strict", false);
    write_protocol_fixture();
    replace_fixture_text("protection.json", "\"apps\":[]", "\"apps\":[\"outside-app\"]");
    expect_rejected(argv[1], argv[2], "allow a bypass", false);

    write_protocol_fixture();
    write_text("pull.json",
        "{\"number\":41,\"state\":\"open\",\"draft\":false,\"mergeable\":true,\"mergeable_state\":\"clean\","
        "\"user\":{\"login\":\"pull-author\"},"
        "\"head\":{\"sha\":\"1111111111111111111111111111111111111111\"},"
        "\"base\":{\"sha\":\"3333333333333333333333333333333333333333\",\"ref\":\"main\"}}");
    expect_rejected(argv[1], argv[2], "head or base changed", false);

    write_protocol_fixture();
    write_text("pull.json", "{invalid-json");
    CHECK(run_cli(argv[1], argv[2], false) == 2, "Review policy accepted invalid GitHub JSON");
    output = read_text(cli_log);
    CHECK(strstr(output, "invalid JSON response") != NULL, "Invalid GitHub JSON returned the wrong diagnostic");
    free(output);

    write_protocol_fixture();
    write_text("reviews.json", "not-json");
    CHECK(run_cli(argv[1], argv[2], false) == 2, "Review policy accepted invalid paginated review JSON");
    output = read_text(cli_log);
    CHECK(strstr(output, "invalid JSON response") != NULL, "Invalid review JSON returned the wrong diagnostic");
    free(output);

    write_protocol_fixture();
    policy_text = read_text(argv[2]);
    write_text("modified-policy.json", policy_text);
    free(policy_text);
    replace_fixture_text("modified-policy.json", "wizardmanannan", "outside-reader");
    char modified_policy[4096];
    path_join(modified_policy, sizeof(modified_policy), fixture_directory, "modified-policy.json");
    expect_rejected(argv[1], modified_policy, "differs from the trusted base policy", false);

    write_protocol_fixture();
    write_policy_variant(argv[2], "unknown-policy.json", ",\"unexpected\":true");
    char unknown_policy[4096];
    path_join(unknown_policy, sizeof(unknown_policy), fixture_directory, "unknown-policy.json");
    CHECK(run_cli(argv[1], unknown_policy, false) == 2, "Review policy accepted an unexpected policy field");
    output = read_text(cli_log);
    CHECK(strstr(output, "unexpected policy field") != NULL, "Unexpected policy field returned the wrong diagnostic");
    free(output);

    write_protocol_fixture();
    expect_rejected(argv[1], argv[2], "unknown or duplicate option: --approved", true);

    puts("PASS review-policy CLI handles current-head review authority, paginated checks, branch policy, and rejection fixtures (offline protocol fixtures; not live evidence)");
    return EXIT_SUCCESS;
}
