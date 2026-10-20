#define _POSIX_C_SOURCE 200809L
#include "../tools/logic_oracle.h"
#include "../tools/logic_replay.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "test_logic_oracle:%d: %s\n", __LINE__, #condition); \
        failed = 1; \
        goto cleanup; \
    } \
} while (0)

static const char *const kq1_assets[] = {
    "KQ1.wag", "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "VOL.0",
    "VOL.1", "VOL.2", "OBJECT", "WORDS.TOK"
};

static const char *const kq3_assets[] = {
    "KQ3.wag", "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "VOL.0",
    "VOL.1", "VOL.2", "VOL.3", "OBJECT", "WORDS.TOK"
};

static void init_snapshot(LogicOracleSnapshot *snapshot, const char *provenance)
{
    memset(snapshot, 0, sizeof *snapshot);
    snapshot->checkpoint_index = 0;
    snapshot->trace_sequence = 20;
    snapshot->has_trace = 1;
    snapshot->agi_version = 0x0300;
    snapshot->timing_profile = LOGIC_ORACLE_TIMING_AGI_TICK;
    snapshot->control_flow_reliable = 1;
    memset(snapshot->manifest_sha256, 0x11,
           sizeof snapshot->manifest_sha256);
    memset(snapshot->input_sha256, 0x22, sizeof snapshot->input_sha256);
    snapshot->rng_seed = 0x12345678;
    snapshot->rng_call_count = 3;
    snapshot->rng_final_state = 0x9abcdef0;
    strcpy(snapshot->rng_algorithm, "test-rng-v1");
    snapshot->tick_numerator = 1;
    snapshot->tick_denominator = 1;
    strcpy(snapshot->game_id, "test-game");
    strcpy(snapshot->checkpoint, "room-entry");
    strcpy(snapshot->provenance, provenance);
    snapshot->room = 7;
    snapshot->variables[42] = 9;
    snapshot->flags[13] = 1;
    snapshot->logic = 2;
    snapshot->logical_pc = 0x1234;
    snapshot->object_count = 2;
    snapshot->objects[0] = (LogicOracleObject){3, 10, 20, 1, 2, 3, 4};
    snapshot->objects[1] = (LogicOracleObject){8, 30, 40, 2, 1, 5, 6};
    snapshot->message_count = 1;
    snapshot->messages[0].logic = 2;
    snapshot->messages[0].id = 17;
    strcpy(snapshot->messages[0].text, "You found a key.");
}

static int run_oracle_cli(int cross_engine_semantic,
                          const char *reference_path,
                          const char *candidate_path)
{
    char *arguments[5];
    size_t count = 0;
    pid_t child = fork();
    int status;
    arguments[count++] = "obj/cx16/logic-oracle";
    if (cross_engine_semantic)
        arguments[count++] = "--cross-engine-semantic";
    arguments[count++] = (char *)reference_path;
    arguments[count++] = (char *)candidate_path;
    arguments[count] = NULL;
    if (child < 0) return -1;
    if (child == 0) {
        int output = open("/dev/null", O_WRONLY);
        if (output < 0 || dup2(output, STDOUT_FILENO) < 0) _exit(126);
        close(output);
        execv(arguments[0], arguments);
        perror("test_logic_oracle: exec logic-oracle semantic comparison");
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int check_cross_engine_cli_policy(const LogicOracleSnapshot *reference,
                                         const LogicOracleSnapshot *candidate)
{
    char directory[] = "obj/cx16/oracle-policy-test-XXXXXX";
    char reference_path[512] = "", candidate_path[512] = "";
    LogicOracleSnapshot candidate_copy = *candidate;
    int reference_length, candidate_length;
    int okay = 0, cleanup_ok = 1;

    if (!mkdtemp(directory)) {
        fprintf(stderr, "test_logic_oracle: policy fixture: %s\n",
                strerror(errno));
        return -1;
    }
    candidate_copy.timing_profile = reference->timing_profile;
    reference_length = snprintf(reference_path, sizeof reference_path,
                                "%s/reference.lors", directory);
    candidate_length = snprintf(candidate_path, sizeof candidate_path,
                                 "%s/candidate.lors", directory);
    if (reference_length < 0 || reference_length >= (int)sizeof reference_path ||
        candidate_length < 0 || candidate_length >= (int)sizeof candidate_path ||
        logic_oracle_snapshot_write(reference_path, reference, 1) != 0 ||
        logic_oracle_snapshot_write(candidate_path, &candidate_copy, 1) != 0)
        goto done;

    if (run_oracle_cli(0, reference_path, candidate_path) != 2) {
        fprintf(stderr,
                "test_logic_oracle: default CLI policy accepted distinct RNG/timing identities\n");
        goto done;
    }
    if (run_oracle_cli(1, reference_path, candidate_path) != 0) {
        fprintf(stderr,
                "test_logic_oracle: explicit cross-engine CLI policy did not match equivalent snapshots\n");
        goto done;
    }
    candidate_copy.tick++;
    if (logic_oracle_snapshot_write(candidate_path, &candidate_copy, 1) != 0 ||
        run_oracle_cli(1, reference_path, candidate_path) != 1) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine CLI policy accepted mismatched tick labels\n");
        goto done;
    }
    candidate_copy.tick--;
    candidate_copy.tick_numerator++;
    if (logic_oracle_snapshot_write(candidate_path, &candidate_copy, 1) != 0 ||
        run_oracle_cli(1, reference_path, candidate_path) != 2) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine CLI policy accepted a mismatched tick rate\n");
        goto done;
    }
    candidate_copy.tick_numerator--;
    candidate_copy.variables[42]++;
    if (logic_oracle_snapshot_write(candidate_path, &candidate_copy, 1) != 0 ||
        run_oracle_cli(1, reference_path, candidate_path) != 1) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine CLI policy missed an observable gameplay divergence\n");
        goto done;
    }
    candidate_copy.variables[42]--;
    candidate_copy.rng_seed++;
    if (logic_oracle_snapshot_write(candidate_path, &candidate_copy, 1) != 0 ||
        run_oracle_cli(1, reference_path, candidate_path) != 2) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine CLI policy accepted a different RNG seed\n");
        goto done;
    }
    okay = 1;

done:
    if (reference_path[0] && unlink(reference_path) != 0 && errno != ENOENT)
        cleanup_ok = 0;
    if (candidate_path[0] && unlink(candidate_path) != 0 && errno != ENOENT)
        cleanup_ok = 0;
    if (rmdir(directory) != 0 && errno != ENOENT) cleanup_ok = 0;
    if (!cleanup_ok)
        fprintf(stderr, "test_logic_oracle: policy fixture cleanup failed: %s\n",
                directory);
    return okay && cleanup_ok ? 0 : -1;
}

static int patch_u16(const char *path, long offset, unsigned value);
static int patch_u32(const char *path, long offset, unsigned value);

/* Permanent wire-level inventory regression: v5 appends known/count/rooms. */
static int check_inventory_wire(void)
{
    char directory[] = "obj/cx16/logic-oracle-test-inventory-XXXXXX";
    char path[512];
    LogicOracleSnapshot snapshot, candidate, *loaded = NULL;
    LogicOracleResult comparison;
    size_t count = 0;
    int result = -1;
    if (!mkdtemp(directory)) return -1;
    snprintf(path, sizeof path, "%s/inventory.lors", directory);
    init_snapshot(&snapshot, "inventory wire regression");
    snapshot.inventory_known = 1;
    snapshot.inventory_count = 256;
    snapshot.inventory_rooms[255] = 255;
    if (logic_oracle_snapshot_write(path, &snapshot, 1) != 0 ||
        logic_oracle_snapshot_load(path, &loaded, &count) != 0 || count != 1 ||
        loaded[0].inventory_known != 1 || loaded[0].inventory_count != 256 ||
        memcmp(loaded[0].inventory_rooms, snapshot.inventory_rooms, 256) != 0) {
        fprintf(stderr, "inventory regression: canonical v5 inventory roundtrip failed\n");
        goto done;
    }
    candidate = loaded[0];
    candidate.inventory_rooms[255] = 42;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_DIVERGED ||
        strcmp(comparison.field, "inventory[255].room") != 0 ||
        logic_oracle_compare_with_policy(&snapshot, &candidate,
            LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC, &comparison) != LOGIC_ORACLE_DIVERGED)
        goto done;
    candidate = snapshot;
    candidate.inventory_count = 255;
    candidate.inventory_rooms[255] = 0;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_DIVERGED ||
        strcmp(comparison.field, "inventory_count") != 0) goto done;
    candidate.inventory_known = 0;
    candidate.inventory_count = 0;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_INCOMPATIBLE)
        goto done;
    snapshot.inventory_count = 0;
    snapshot.inventory_rooms[255] = 0;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_INCOMPATIBLE)
        goto done;
    candidate.inventory_known = 1;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_MATCH)
        goto done;
    candidate.inventory_count = 257;
    if (logic_oracle_compare(&snapshot, &candidate, &comparison) != LOGIC_ORACLE_INVALID ||
        logic_oracle_snapshot_write(path, &candidate, 1) == 0) goto done;
    result = 0;
done:
    free(loaded);
    if (unlink(path) != 0 && errno != ENOENT) result = -1;
    if (rmdir(directory) != 0) result = -1;
    if (result) fprintf(stderr, "inventory regression: canonical comparison failed\n");
    return result;
}

static int check_unaligned_cycle_samples(void)
{
    LogicOracleSnapshot cycle, wall;
    LogicOracleResult result;
    init_snapshot(&cycle, "completed interpreter cycle");
    wall = cycle;
    wall.timing_profile = LOGIC_ORACLE_TIMING_WALL_MILLISECONDS;
    if (logic_oracle_compare_with_policy(&cycle, &wall,
            LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC, &result) !=
        LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr, "unaligned-cycle regression: wall sample matched completed-cycle sample\n");
        return -1;
    }
    return 0;
}

static int check_distinct_rng_comparison(void)
{
    LogicOracleSnapshot reference, candidate, same_rng;
    LogicOracleResult result;
    LogicOracleStatus status;

    init_snapshot(&reference, "ScummVM native AGI engine");
    candidate = reference;
    strcpy(candidate.provenance, "CX16 application replay");
    strcpy(candidate.rng_algorithm, "cc65-lcg32-rand-table256-v1");
    candidate.rng_call_count++;
    candidate.rng_final_state++;

    if (check_cross_engine_cli_policy(&reference, &candidate) != 0)
        return -1;

    status = logic_oracle_compare(&reference, &candidate, &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: deterministic comparison accepted distinct RNG identities\n");
        return -1;
    }

    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_MATCH) {
        fprintf(stderr,
                "test_logic_oracle: cross-RNG semantic comparison returned %s: %s\n",
                logic_oracle_status_name(status), result.detail);
        return -1;
    }

    same_rng = reference;
    same_rng.rng_call_count++;
    status = logic_oracle_compare_with_policy(
        &reference, &same_rng, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_DIVERGED ||
        strcmp(result.field, "rng_call_count") != 0) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine policy skipped telemetry for a matching RNG algorithm\n");
        return -1;
    }

    candidate.variables[42]++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_DIVERGED ||
        strcmp(result.field, "variable[42]") != 0) {
        fprintf(stderr,
                "test_logic_oracle: cross-RNG comparison missed first semantic divergence (%s, %s)\n",
                logic_oracle_status_name(status), result.field);
        return -1;
    }
    candidate.variables[42]--;
    candidate.flags[13]--;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_DIVERGED || strcmp(result.field, "flag[13]") != 0) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison missed an RNG-driven flag divergence\n");
        return -1;
    }
    candidate.flags[13]++;
    candidate.objects[0].x++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_DIVERGED ||
        strcmp(result.field, "object[3].x") != 0) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison missed an RNG-driven object divergence\n");
        return -1;
    }
    candidate.objects[0].x--;
    candidate.timing_profile = reference.timing_profile;
    strcpy(reference.checkpoint, "agi-tick-100");
    strcpy(candidate.checkpoint, "agi-tick-100");
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_MATCH) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine aligned sample profiles rejected matching ticks\n");
        return -1;
    }
    candidate.tick++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_DIVERGED || strcmp(result.field, "tick") != 0) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison missed mismatched tick identity\n");
        return -1;
    }
    candidate.tick--;
    candidate.tick_denominator++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison accepted a different tick rate\n");
        return -1;
    }
    candidate.tick_denominator--;
    candidate.timing_profile = LOGIC_ORACLE_TIMING_FRAME_STEP;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison accepted an unsupported timing profile pair\n");
        return -1;
    }
    candidate.timing_profile = reference.timing_profile;
    candidate.manifest_sha256[0]++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison accepted a different manifest hash\n");
        return -1;
    }
    candidate.manifest_sha256[0]--;
    candidate.input_sha256[0]++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison accepted a different input hash\n");
        return -1;
    }
    candidate.input_sha256[0]--;
    candidate.rng_seed++;
    status = logic_oracle_compare_with_policy(
        &reference, &candidate, LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC,
        &result);
    if (status != LOGIC_ORACLE_INCOMPATIBLE) {
        fprintf(stderr,
                "test_logic_oracle: cross-engine comparison accepted a different RNG seed\n");
        return -1;
    }
    return 0;
}

