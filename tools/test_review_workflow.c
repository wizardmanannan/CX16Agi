#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test_paths.h"
#include "test_run_from.h"

#define CHECK(c, m) do { if (!(c)) run_from_fail("%s", m); } while (0)

static char log_path[4096];

static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    CHECK(file, "Cannot create workflow input");
    CHECK(fwrite(text, 1, strlen(text), file) == strlen(text), "Cannot write workflow input");
    CHECK(!fclose(file), "Cannot close workflow input");
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file, "Cannot read workflow input");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek workflow input");
    long size = ftell(file);
    CHECK(size >= 0 && size < 1024 * 1024, "Workflow input too large");
    rewind(file);
    char *text = calloc((size_t)size + 1, 1);
    CHECK(text, "Cannot allocate workflow input");
    CHECK(fread(text, 1, (size_t)size, file) == (size_t)size, "Cannot read workflow input");
    CHECK(!fclose(file), "Cannot close workflow input");
    return text;
}

static bool runner_temp_in_job_environment(const char *yaml)
{
    char *copy = strdup(yaml);
    CHECK(copy, "Cannot allocate workflow context check");
    bool in_job_env = false;
    char *state = NULL;
    for (char *line = strtok_r(copy, "\n", &state); line; line = strtok_r(NULL, "\n", &state)) {
        size_t indent = 0;
        while (line[indent] == ' ') ++indent;
        if (indent == 4 && !strcmp(line + indent, "env:")) {
            in_job_env = true;
            continue;
        }
        if (in_job_env && indent <= 4) in_job_env = false;
        if (in_job_env && strstr(line, "runner.temp")) {
            free(copy);
            return true;
        }
    }
    free(copy);
    return false;
}

static size_t count_text(const char *text, const char *needle)
{
    size_t count = 0;
    size_t length = strlen(needle);
    for (const char *match = text; (match = strstr(match, needle)); match += length) ++count;
    return count;
}

/* Execute the production block itself; duplicating its predicates would hide drift. */
static char *aggregate_script(const char *path)
{
    char *yaml = read_text(path);
    char *step = strstr(yaml, "      - name: Require complete and current evidence\n");
    CHECK(step, "Workflow readiness step missing");
    char *start = strstr(step, "        run: |\n");
    CHECK(start, "Workflow readiness script missing");
    start += strlen("        run: |\n");
    char *script = calloc(strlen(start) + 1, 1);
    CHECK(script, "Cannot allocate production readiness script");
    size_t used = 0;
    for (char *line = start; *line;) {
        char *end = strchr(line, '\n');
        CHECK(end, "Unterminated workflow script");
        size_t length = (size_t)(end - line);
        if (length && (length < 10 || strncmp(line, "          ", 10))) break;
        if (length) {
            memcpy(script + used, line + 10, length - 10);
            used += length - 10;
        }
        script[used++] = '\n';
        line = end + 1;
    }
    free(yaml);
    CHECK(used, "Empty readiness script");
    return script;
}

static void env(const char *name, const char *value)
{
    CHECK(!setenv(name, value, 1), "Cannot set workflow environment");
}

static void run_gate(char *script, bool success, const char *diagnostic)
{
    write_text(log_path, "");
    char *args[] = {"bash", "-c",
        "bash -c \"$1\"; status=$?; if [ \"$status\" -ne \"$2\" ]; then "
        "printf 'Expected workflow exit %s, got %s\\n' \"$2\" \"$status\"; exit 1; fi",
        "workflow-case", script, success ? "0" : "1", NULL};
    run_from(args, log_path, 15, suite_fixture_directory);
    char *log = read_text(log_path);
    if (!strstr(log, diagnostic)) run_from_fail_with_log(log_path, "Wrong workflow decision: %s", diagnostic);
    free(log);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2, "Expected native workflow path");
    suite_test_paths_init("review-workflow");
    CHECK(runner_temp_in_job_environment(
        "jobs:\n  check:\n    env:\n      EVIDENCE_DIR: ${{ runner.temp }}/evidence\n"),
        "Context regression check missed unavailable job-level runner.temp");
    CHECK(!runner_temp_in_job_environment(
        "jobs:\n  check:\n    env:\n      MODE: test\n    steps:\n"
        "      - uses: actions/upload-artifact\n        with:\n"
        "          path: ${{ runner.temp }}/evidence\n"),
        "Context regression check rejected permitted step-level runner.temp");
    char *yaml = read_text(argv[1]);
    CHECK(!runner_temp_in_job_environment(yaml), "runner.temp is unavailable in job-level env");
    CHECK(count_text(yaml, "path: ${{ runner.temp }}/cx16agi-native-") == 3,
          "runner.temp must remain available in the three step-level artifact paths");
    CHECK(count_text(yaml, "echo \"EVIDENCE_DIR=$EVIDENCE_DIR\" >> \"$GITHUB_ENV\"") == 3,
          "Each job must pass its runner-temp evidence path through GITHUB_ENV");
    CHECK(strstr(yaml, "EVIDENCE_DIR=\"$RUNNER_TEMP/cx16agi-native-head-$GITHUB_RUN_ID-$GITHUB_RUN_ATTEMPT\""),
          "Head evidence path must be unique per workflow run and attempt");
    CHECK(strstr(yaml, "EVIDENCE_DIR=\"$RUNNER_TEMP/cx16agi-native-integration-$GITHUB_RUN_ID-$GITHUB_RUN_ATTEMPT\""),
          "Integration evidence path must be unique per workflow run and attempt");
    CHECK(strstr(yaml, "EVIDENCE_DIR=\"$RUNNER_TEMP/cx16agi-native-ready-$GITHUB_RUN_ID-$GITHUB_RUN_ATTEMPT\""),
          "Readiness evidence path must be unique per workflow run and attempt");
    free(yaml);
    char *script = aggregate_script(argv[1]);
    suite_test_path(log_path, sizeof(log_path), suite_output_directory, "review-workflow.log");
    /* Real Git history provides every SHA/tree read by the production gate. */
    char *setup[] = {"bash", "-eu", "-c",
        "git init -q --initial-branch=main; "
        "git config user.name Workflow-Test; git config user.email workflow@example.invalid; "
        "printf base > input; git add input; git commit -qm base; "
        "git rev-parse HEAD > base; git rev-parse HEAD^{tree} > base-tree; "
        "git switch -qc feature; printf feature > feature-input; git add feature-input; git commit -qm feature; "
        "git rev-parse HEAD > head; git rev-parse HEAD^{tree} > head-tree; "
        "git switch -q main; git merge -q --no-ff --no-edit feature; "
        "git rev-parse HEAD > merge; git rev-parse HEAD^{tree} > merge-tree; "
        "git checkout -q --detach $(cat base)", NULL};
    run_from(setup, log_path, 15, suite_fixture_directory);
    const char *names[] = {"base", "base-tree", "head", "head-tree", "merge", "merge-tree"};
    char *values[6];
    for (size_t i = 0; i < 6; ++i) {
        char path[4096];
        suite_test_path(path, sizeof(path), suite_fixture_directory, names[i]);
        values[i] = read_text(path);
        values[i][strcspn(values[i], "\r\n")] = '\0';
    }
    env("GITHUB_EVENT_NAME", "pull_request"); env("GITHUB_SHA", values[4]);
    env("EVENT_BASE_SHA", values[0]); env("EXPECTED_HEAD_SHA", values[2]);
    env("HEAD_JOB_SHA", values[2]); env("HEAD_JOB_TREE", values[3]);
    env("INTEGRATION_HEAD_SHA", values[2]); env("INTEGRATION_HEAD_TREE", values[3]);
    env("INTEGRATION_BASE_SHA", values[0]); env("INTEGRATION_BASE_TREE", values[1]);
    env("INTEGRATION_MERGE_SHA", values[4]); env("INTEGRATION_MERGE_TREE", values[5]);
    env("HEAD_RESULT", "success"); env("INTEGRATION_RESULT", "success");
    env("EVIDENCE_DIR", suite_fixture_directory);
    char summary[4096];
    suite_test_path(summary, sizeof(summary), suite_output_directory, "review-workflow-summary.md");
    env("GITHUB_STEP_SUMMARY", summary);
    run_gate(script, true, "evidence is complete");
    const char *statuses[] = {"skipped", "cancelled", "neutral", "failure", ""};
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        env("HEAD_RESULT", statuses[i]);
        run_gate(script, false, "must both succeed");
    }
    env("HEAD_RESULT", "success");
    env("INTEGRATION_RESULT", "skipped"); run_gate(script, false, "must both succeed");
    env("INTEGRATION_RESULT", "success");
    env("HEAD_JOB_SHA", values[0]); run_gate(script, false, "same event head");
    env("HEAD_JOB_SHA", values[2]);
    env("INTEGRATION_HEAD_TREE", values[1]); run_gate(script, false, "different head trees");
    env("INTEGRATION_HEAD_TREE", values[3]);
    env("INTEGRATION_MERGE_SHA", values[0]); run_gate(script, false, "event check SHA");
    env("INTEGRATION_MERGE_SHA", values[4]);
    env("INTEGRATION_MERGE_TREE", values[1]); run_gate(script, false, "integration tree");
    env("INTEGRATION_MERGE_TREE", values[5]);
    env("HEAD_JOB_SHA", "11111111111111111111111111111111111111111");
    run_gate(script, false, "missing or malformed"); env("HEAD_JOB_SHA", values[2]);
    char *advance[] = {"git", "checkout", "--detach", values[4], NULL};
    run_from(advance, log_path, 15, suite_fixture_directory);
    run_gate(script, false, "main moved");
    for (size_t i = 0; i < 6; ++i) free(values[i]);
    free(script);
    puts("PASS actual workflow aggregate rejects incomplete jobs, stale identities and forged merge trees");
    return 0;
}
