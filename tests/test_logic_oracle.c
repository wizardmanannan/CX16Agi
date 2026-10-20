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

static int patch_u16(const char *path, long offset, unsigned value)
{
    FILE *file = fopen(path, "r+b");
    unsigned char bytes[2];
    int ok, close_result;
    if (!file) return -1;
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
    ok = fseek(file, offset, SEEK_SET) == 0 &&
         fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes;
    close_result = fclose(file);
    if (close_result != 0) ok = 0;
    return ok ? 0 : -1;
}

static int patch_u32(const char *path, long offset, unsigned value)
{
    FILE *file = fopen(path, "r+b");
    unsigned char bytes[4];
    int ok, close_result;
    if (!file) return -1;
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
    bytes[2] = (unsigned char)(value >> 16);
    bytes[3] = (unsigned char)(value >> 24);
    ok = fseek(file, offset, SEEK_SET) == 0 &&
         fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes;
    close_result = fclose(file);
    if (close_result != 0) ok = 0;
    return ok ? 0 : -1;
}

static int write_bytes(const char *path, const void *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    int ok, close_result;
    if (!file) return -1;
    ok = fwrite(bytes, 1, length, file) == length;
    close_result = fclose(file);
    if (close_result != 0) ok = 0;
    return ok ? 0 : -1;
}

static int game_file(char *path, size_t size, const char *directory,
                     const char *name)
{
    int length = snprintf(path, size, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < size ? 0 : -1;
}

static int copy_file(const char *source_path, const char *target_path)
{
    unsigned char bytes[8192];
    FILE *source = fopen(source_path, "rb");
    FILE *target;
    int ok = 1;
    if (!source) return -1;
    target = fopen(target_path, "wb");
    if (!target) {
        fclose(source);
        return -1;
    }
    for (;;) {
        size_t count = fread(bytes, 1, sizeof bytes, source);
        if (count && fwrite(bytes, 1, count, target) != count) {
            ok = 0;
            break;
        }
        if (count < sizeof bytes) {
            if (ferror(source)) ok = 0;
            break;
        }
    }
    if (fclose(source) != 0) ok = 0;
    if (fclose(target) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int stage_game_assets(const char *source_directory,
                             const char *target_directory,
                             const char *const *names, size_t name_count)
{
    char source_path[1024], target_path[1024];
    size_t i;
    if (mkdir(target_directory, 0700) != 0) return -1;
    for (i = 0; i < name_count; ++i) {
        if (game_file(source_path, sizeof source_path, source_directory,
                      names[i]) != 0 ||
            game_file(target_path, sizeof target_path, target_directory,
                      names[i]) != 0 ||
            copy_file(source_path, target_path) != 0)
            return -1;
    }
    return 0;
}

static void remove_game_assets(const char *directory,
                               const char *const *names, size_t name_count)
{
    char path[1024];
    size_t i;
    if (!directory[0]) return;
    for (i = 0; i < name_count; ++i) {
        if (game_file(path, sizeof path, directory, names[i]) == 0 &&
            unlink(path) != 0 && errno != ENOENT)
            fprintf(stderr, "test_logic_oracle: cannot remove %s: %s\n",
                    path, strerror(errno));
    }
    if (rmdir(directory) != 0 && errno != ENOENT)
        fprintf(stderr, "test_logic_oracle: cannot remove %s: %s\n",
                directory, strerror(errno));
}

static int write_game_replay_manifest(const char *game_directory,
                                     const char *manifest_path,
                                     const char *game_id,
                                     const char *const *names,
                                     size_t name_count)
{
    LogicReplayManifest manifest;
    char path[1024];
    size_t i;
    memset(&manifest, 0, sizeof manifest);
    manifest.version = LOGIC_REPLAY_MANIFEST_VERSION;
    if (strlen(game_id) >= sizeof manifest.engine) return -1;
    strcpy(manifest.engine, game_id);
    strcpy(manifest.build, "scummvm-reference-kq-regression");
    strcpy(manifest.agi_version, "0x2917");
    manifest.resource_count = name_count;
    manifest.rng_seed = UINT32_C(0x4a474931);
    strcpy(manifest.rng_algorithm, "xorshift32-v1");
    manifest.tick_numerator = 20;
    manifest.tick_denominator = 1;
    for (i = 0; i < name_count; ++i) {
        if (strlen(names[i]) >= sizeof manifest.resources[i].name)
            return -1;
        strcpy(manifest.resources[i].name, names[i]);
        if (game_file(path, sizeof path, game_directory, names[i]) != 0 ||
            logic_replay_sha256_file(path,
                manifest.resources[i].sha256) != 0)
            return -1;
    }
    return logic_replay_manifest_write(manifest_path, &manifest);
}

static int append_key_event(LogicReplayEvent *events, size_t capacity,
                            size_t *count, uint32_t tick, uint8_t key,
                            uint8_t reason)
{
    LogicReplayEvent *event;
    if (*count >= capacity) return -1;
    event = &events[(*count)++];
    memset(event, 0, sizeof *event);
    event->sequence = (uint32_t)(*count - 1);
    event->tick = tick;
    event->kind = LOGIC_TRACE_KIND_MARK;
    event->field = key;
    event->before = reason == LOGIC_REPLAY_REASON_INPUT_UP;
    event->after = reason == LOGIC_REPLAY_REASON_INPUT_DOWN;
    event->reason = reason;
    return 0;
}

static int write_game_input_trace(const char *path)
{
    static const uint32_t intro_ticks[] = {2, 10, 18, 26, 34};
    static const uint32_t command_ticks[] = {50, 54, 58, 62, 66};
    static const uint8_t command_keys[] = {'l', 'o', 'o', 'k', '\r'};
    LogicReplayEvent events[32];
    size_t count = 0, i;
    for (i = 0; i < sizeof intro_ticks / sizeof intro_ticks[0]; ++i) {
        if (append_key_event(events, 32, &count, intro_ticks[i], '\r',
                             LOGIC_REPLAY_REASON_INPUT_DOWN) != 0 ||
            append_key_event(events, 32, &count, intro_ticks[i], '\r',
                             LOGIC_REPLAY_REASON_INPUT_UP) != 0)
            return -1;
    }
    for (i = 0; i < sizeof command_ticks / sizeof command_ticks[0]; ++i) {
        if (append_key_event(events, 32, &count, command_ticks[i],
                             command_keys[i],
                             LOGIC_REPLAY_REASON_INPUT_DOWN) != 0 ||
            append_key_event(events, 32, &count, command_ticks[i],
                             command_keys[i],
                             LOGIC_REPLAY_REASON_INPUT_UP) != 0)
            return -1;
    }
    return logic_replay_trace_write(path, events, count);
}

static int file_has_line(const char *path, const char *expected)
{
    char line[256];
    FILE *file = fopen(path, "r");
    int found = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, expected) == 0) {
            found = 1;
            break;
        }
    }
    if (fclose(file) != 0) return 0;
    return found;
}

static int read_milestone_uint(const char *path, const char *key,
                               unsigned long *value)
{
    char line[256], found_key[64], found_value[128];
    FILE *file = fopen(path, "r");
    int found = 0;
    if (!file) return -1;
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, "%63[^=]=%127s", found_key, found_value) == 2 &&
            strcmp(found_key, key) == 0) {
            char *end = NULL;
            errno = 0;
            unsigned long parsed = strtoul(found_value, &end, 10);
            if (!errno && end && *end == '\0') {
                *value = parsed;
                found = 1;
            }
            break;
        }
    }
    if (fclose(file) != 0) return -1;
    return found ? 0 : -1;
}

static void set_v2_directory_entry(unsigned char *directory, unsigned resource,
                                   unsigned offset)
{
    unsigned char *entry = directory + resource * 3;
    entry[0] = (unsigned char)((offset >> 16) & 0x0f);
    entry[1] = (unsigned char)(offset >> 8);
    entry[2] = (unsigned char)offset;
}

static uint8_t scummvm_random_1_to_10(uint32_t *state)
{
    uint32_t product;
    *state ^= *state >> 13;
    *state ^= *state << 21;
    *state ^= *state >> 11;
    product = *state * UINT32_C(0xDEADBF03);
    return (uint8_t)(product % 10 + 1);
}

static size_t append_v2_logic(unsigned char *volume, size_t offset,
                              const unsigned char *code, size_t code_length)
{
    size_t payload_length = 2 + code_length + 3;
    volume[offset++] = 0x12;
    volume[offset++] = 0x34;
    volume[offset++] = 0;
    volume[offset++] = (unsigned char)payload_length;
    volume[offset++] = (unsigned char)(payload_length >> 8);
    volume[offset++] = (unsigned char)code_length;
    volume[offset++] = (unsigned char)(code_length >> 8);
    memcpy(volume + offset, code, code_length);
    offset += code_length;
    volume[offset++] = 0; /* no logic messages */
    volume[offset++] = 2;
    volume[offset++] = 0;
    return offset;
}

static int write_minimal_agi_game(const char *directory)
{
    static const unsigned char logic_zero[] = {
        4, 42, 19,       /* assignv v42, v19 (the delivered key) */
        0x82, 1, 10, 50, /* random 1..10, v50 */
        22, 1,           /* call logic 1 */
        5, 42, 3,         /* addn v42, 3 after the nested call */
        0                /* return */
    };
    static const unsigned char logic_one[] = {
        5, 42, 5,         /* addn v42, 5 */
        22, 2,            /* call logic 2 to exercise nested context restore */
        0
    };
    static const unsigned char logic_two[] = {
        5, 42, 7,         /* addn v42, 7 */
        0                /* return */
    };
    unsigned char logdir[256 * 3], empty_dir[256 * 3];
    unsigned char volume[64];
    unsigned char words[26 * 2] = {0};
    static const char wag[] =
        "[General]\n"
        "Interpreter=2.917\n"
        "GameID=agi-fanmade\n"
        "Description=Oracle fixture\n"
        "GameVersion=fixture-v1\n";
    char path[512];
    size_t volume_length = 0;

    memset(logdir, 0xff, sizeof logdir);
    memset(empty_dir, 0xff, sizeof empty_dir);
    set_v2_directory_entry(logdir, 0, (unsigned)volume_length);
    volume_length = append_v2_logic(volume, volume_length,
                                    logic_zero, sizeof logic_zero);
    set_v2_directory_entry(logdir, 1, (unsigned)volume_length);
    volume_length = append_v2_logic(volume, volume_length,
                                    logic_one, sizeof logic_one);
    set_v2_directory_entry(logdir, 2, (unsigned)volume_length);
    volume_length = append_v2_logic(volume, volume_length,
                                    logic_two, sizeof logic_two);

    if (game_file(path, sizeof path, directory, "LOGDIR") != 0 ||
        write_bytes(path, logdir, sizeof logdir) != 0 ||
        game_file(path, sizeof path, directory, "PICDIR") != 0 ||
        write_bytes(path, empty_dir, sizeof empty_dir) != 0 ||
        game_file(path, sizeof path, directory, "VIEWDIR") != 0 ||
        write_bytes(path, empty_dir, sizeof empty_dir) != 0 ||
        game_file(path, sizeof path, directory, "SNDDIR") != 0 ||
        write_bytes(path, empty_dir, sizeof empty_dir) != 0 ||
        game_file(path, sizeof path, directory, "VOL.0") != 0 ||
        write_bytes(path, volume, volume_length) != 0 ||
        game_file(path, sizeof path, directory, "OBJECT") != 0 ||
        write_bytes(path, "", 0) != 0 ||
        game_file(path, sizeof path, directory, "WORDS.TOK") != 0 ||
        write_bytes(path, words, sizeof words) != 0 ||
        game_file(path, sizeof path, directory, "oracle.wag") != 0 ||
        write_bytes(path, wag, sizeof wag - 1) != 0)
        return -1;
    return 0;
}

static int write_replay_manifest(const char *game_directory,
                                 const char *manifest_path)
{
    static const char *const names[] = {
        "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "VOL.0", "OBJECT",
        "WORDS.TOK", "oracle.wag"
    };
    LogicReplayManifest manifest;
    char path[512];
    size_t i;
    memset(&manifest, 0, sizeof manifest);
    manifest.version = LOGIC_REPLAY_MANIFEST_VERSION;
    strcpy(manifest.engine, "agi-fanmade");
    strcpy(manifest.build, "t3code-f508eb73");
    strcpy(manifest.agi_version, "0x2917");
    manifest.resource_count = sizeof names / sizeof names[0];
    manifest.rng_seed = 0x4a474931ULL;
    strcpy(manifest.rng_algorithm, "xorshift32-v1");
    manifest.tick_numerator = 20;
    manifest.tick_denominator = 1;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        strcpy(manifest.resources[i].name, names[i]);
        if (game_file(path, sizeof path, game_directory, names[i]) != 0 ||
            logic_replay_sha256_file(path, manifest.resources[i].sha256) != 0)
            return -1;
    }
    return logic_replay_manifest_write(manifest_path, &manifest);
}

static void remove_minimal_agi_game(const char *directory)
{
    static const char *const names[] = {
        "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "VOL.0", "OBJECT",
        "WORDS.TOK", "oracle.wag"
    };
    char path[512];
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        if (game_file(path, sizeof path, directory, names[i]) == 0 &&
            unlink(path) != 0 && errno != ENOENT)
            fprintf(stderr, "test_logic_oracle: cannot remove %s: %s\n",
                    path, strerror(errno));
    }
    if (rmdir(directory) != 0 && errno != ENOENT)
        fprintf(stderr, "test_logic_oracle: cannot remove %s: %s\n",
                directory, strerror(errno));
}

static const char *fixture_relative_path(const char *path,
                                         const char *fixture_directory)
{
    size_t length = strlen(fixture_directory);
    return strncmp(path, fixture_directory, length) == 0 && path[length] == '/'
        ? path + length + 1 : path;
}

static int run_reference_engine_profile(const char *fixture_directory,
                                const char *game_directory,
                                const char *input_schedule,
                                const char *manifest_path,
                                const char *ticks,
                                const char *snapshot_path,
                                const char *milestones_path,
                                const char *profile)
{
    char *arguments[17];
    char executable_path[1024];
    size_t count = 0;
    static const char executable_suffix[] =
        "/obj/cx16/logic-oracle-scummvm";
    if (!getcwd(executable_path, sizeof executable_path))
        return -1;
    size_t cwd_length = strlen(executable_path);
    if (cwd_length + sizeof executable_suffix > sizeof executable_path)
        return -1;
    memcpy(executable_path + cwd_length, executable_suffix,
           sizeof executable_suffix);
    arguments[count++] = executable_path;
    arguments[count++] = "--game-dir";
    arguments[count++] = (char *)fixture_relative_path(
        game_directory, fixture_directory);
    arguments[count++] = "--input-trace";
    arguments[count++] = (char *)fixture_relative_path(
        input_schedule, fixture_directory);
    arguments[count++] = "--manifest";
    arguments[count++] = (char *)fixture_relative_path(
        manifest_path, fixture_directory);
    arguments[count++] = "--ticks";
    arguments[count++] = (char *)ticks;
    arguments[count++] = "--snapshot";
    arguments[count++] = (char *)fixture_relative_path(
        snapshot_path, fixture_directory);
    if (profile) {
        arguments[count++] = "--checkpoint-profile";
        arguments[count++] = (char *)profile;
    }
    if (milestones_path) {
        arguments[count++] = "--milestones";
    }
    arguments[count] = NULL;
    pid_t child = fork();
    int status;
    if (child < 0) return -1;
    if (child == 0) {
        if (chdir(fixture_directory) != 0) {
            perror("test_logic_oracle: chdir ScummVM fixture");
            _exit(126);
        }
        if (milestones_path) {
            int output = open(fixture_relative_path(
                                  milestones_path, fixture_directory),
                              O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (output < 0 || dup2(output, STDOUT_FILENO) < 0)
                _exit(126);
            close(output);
        }
        execv(arguments[0], arguments);
        perror("test_logic_oracle: exec logic-oracle-scummvm");
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int run_reference_engine(const char *fixture_directory,
    const char *game_directory, const char *input_schedule,
    const char *manifest_path, const char *ticks,
    const char *snapshot_path, const char *milestones_path)
{
    return run_reference_engine_profile(fixture_directory, game_directory,
        input_schedule, manifest_path, ticks, snapshot_path, milestones_path, NULL);
}

static int check_completed_cycle_capture(const char *fixture_directory,
    const char *game_directory, const char *input_schedule,
    const char *manifest_path, const char *snapshot_path)
{
    LogicOracleSnapshot *snapshots = NULL;
    size_t count = 0;
    int result = -1;
    if (run_reference_engine_profile(fixture_directory, game_directory,
            input_schedule, manifest_path, "1", snapshot_path, NULL,
            "completed-cycle") != 0 ||
        logic_oracle_snapshot_load(snapshot_path, &snapshots, &count) != 0 ||
        count != 1 || snapshots[0].timing_profile != LOGIC_ORACLE_TIMING_AGI_TICK ||
        snapshots[0].tick != 1 || strcmp(snapshots[0].checkpoint, "agi-tick-1") ||
        !snapshots[0].control_flow_reliable || snapshots[0].logic != 0 ||
        snapshots[0].logical_pc != 12 ||
        !strstr(snapshots[0].provenance, "cycle-hook") ||
        snapshots[0].variables[19] != 0 || snapshots[0].flags[2] ||
        snapshots[0].flags[4] || snapshots[0].rng_call_count != 1 ||
        !snapshots[0].inventory_known) {
        fprintf(stderr, "completed-cycle regression: actual post-cycle capture unavailable or invalid\n");
        goto done;
    }
    result = 0;
done:
    free(snapshots);
    return result;
}

/* A timed PRINT blocks the real interpreter beyond its first wall sample. */
static int check_timed_print_cycle_boundary(const char *fixture_directory)
{
    char game[512], manifest[512], input[512], wall_path[512], cycle_path[512];
    unsigned char logdir[256 * 3], volume[128];
    static const unsigned char code[] = {
        3, 21, 2, 101, 1, /* timed print1 */
        3, 21, 2, 101, 1, /* same source message printed again */
        103, 1, 1, 1, /* display source message, without a modal print */
        3, 42, 99, 0
    };
    static const char text[] = "Timed cycle boundary";
    static const char unused[] = "Loaded but never printed";
    static const char key[] = "Avis Durgan";
    LogicOracleSnapshot *wall = NULL, *cycle = NULL;
    size_t wall_count = 0, cycle_count = 0, length, section, i;
    char path[512];
    int result = -1;
    if (game_file(game, sizeof game, fixture_directory, "timed-game") ||
        game_file(manifest, sizeof manifest, fixture_directory, "timed.manifest") ||
        game_file(input, sizeof input, fixture_directory, "timed.ltrace") ||
        game_file(wall_path, sizeof wall_path, fixture_directory, "timed-wall.lors") ||
        game_file(cycle_path, sizeof cycle_path, fixture_directory, "timed-cycle.lors"))
        return -1;
    if (mkdir(game, 0700) || write_minimal_agi_game(game)) goto done;
    memset(logdir, 0xff, sizeof logdir);
    set_v2_directory_entry(logdir, 0, 0);
    length = append_v2_logic(volume, 0, code, sizeof code);
    section = length - 3;
    /* V2 message size excludes count byte, offsets relative to section+1. */
    volume[section] = 2;
    volume[section + 1] = (unsigned char)(6 + sizeof text + sizeof unused);
    volume[section + 2] = 0;
    volume[section + 3] = 6; volume[section + 4] = 0;
    volume[section + 5] = (unsigned char)(6 + sizeof text);
    volume[section + 6] = 0;
    for (i = 0; i < sizeof text; ++i)
        volume[section + 7 + i] = (unsigned char)text[i] ^ key[i % (sizeof key - 1)];
    for (i = 0; i < sizeof unused; ++i)
        volume[section + 7 + sizeof text + i] =
            (unsigned char)unused[i] ^ key[(sizeof text + i) % (sizeof key - 1)];
    length = section + 7 + sizeof text + sizeof unused;
    volume[3] = (unsigned char)(length - 5);
    volume[4] = (unsigned char)((length - 5) >> 8);
    if (game_file(path, sizeof path, game, "LOGDIR") || write_bytes(path, logdir, sizeof logdir) ||
        game_file(path, sizeof path, game, "VOL.0") || write_bytes(path, volume, length) ||
        write_replay_manifest(game, manifest) || logic_replay_trace_write(input, NULL, 0) ||
        run_reference_engine_profile(fixture_directory, game, input, manifest,
            "1", wall_path, NULL, "wall") ||
        run_reference_engine_profile(fixture_directory, game, input, manifest,
            "1", cycle_path, NULL, "completed-cycle") ||
        logic_oracle_snapshot_load(wall_path, &wall, &wall_count) ||
        logic_oracle_snapshot_load(cycle_path, &cycle, &cycle_count)) goto done;
    if (wall_count != 1 || cycle_count != 1 ||
        wall[0].timing_profile != LOGIC_ORACLE_TIMING_WALL_MILLISECONDS ||
        cycle[0].timing_profile != LOGIC_ORACLE_TIMING_AGI_TICK ||
        wall[0].variables[21] != 2 || cycle[0].variables[21] != 0 ||
        wall[0].variables[42] != 0 || cycle[0].variables[42] != 99 ||
        cycle[0].variables[19] || cycle[0].flags[2] || cycle[0].flags[4] ||
        cycle[0].message_count != 3 || strcmp(cycle[0].messages[0].text, text) ||
        strcmp(cycle[0].messages[1].text, text) ||
        strcmp(cycle[0].messages[2].text, text)) {
        fprintf(stderr, "timed-print regression: wall v42=%u/cycle v42=%u; emitted messages=%u expected3 (two loaded)\n",
                wall[0].variables[42], cycle[0].variables[42], cycle[0].message_count);
        goto done;
    }
    result = 0;
done:
    free(wall); free(cycle);
    if (unlink(manifest) && errno != ENOENT) result = -1;
    if (unlink(input) && errno != ENOENT) result = -1;
    if (unlink(wall_path) && errno != ENOENT) result = -1;
    if (unlink(cycle_path) && errno != ENOENT) result = -1;
    remove_minimal_agi_game(game);
    return result;
}

static int check_nonblocking_transition_clock(const char *fixture_directory)
{
    char game[512], manifest[512], input[512], capture[512], path[512];
    unsigned char logdir[768], volume[256];
    static const unsigned char code[] = {
        255, 1, 0, 0, 255, 6, 0, /* if v0==0 */
        12, 15, 101, 1, 18, 1, /* nonblocking print; new.room(1) */
        3, 42, 99, 0
    };
    static const unsigned char child[] = {0};
    static const char text[] = "Nonblocking transition";
    static const char key[] = "Avis Durgan";
    LogicOracleSnapshot *snapshots = NULL;
    size_t count = 0, length, section, i;
    int result = -1;
    if (game_file(game, sizeof game, fixture_directory, "transition-game") ||
        game_file(manifest, sizeof manifest, fixture_directory, "transition.manifest") ||
        game_file(input, sizeof input, fixture_directory, "transition.ltrace") ||
        game_file(capture, sizeof capture, fixture_directory, "transition.lors"))
        return -1;
    if (mkdir(game, 0700) || write_minimal_agi_game(game)) goto done;
    memset(logdir, 255, sizeof logdir);
    set_v2_directory_entry(logdir, 0, 0);
    length = append_v2_logic(volume, 0, code, sizeof code);
    section = length - 3;
    volume[section] = 1;
    volume[section + 1] = 4 + sizeof text;
    volume[section + 2] = 0;
    volume[section + 3] = 4;
    volume[section + 4] = 0;
    for (i = 0; i < sizeof text; ++i)
        volume[section + 5 + i] = text[i] ^ key[i % (sizeof key - 1)];
    length = section + 5 + sizeof text;
    volume[3] = length - 5;
    volume[4] = (length - 5) >> 8;
    set_v2_directory_entry(logdir, 1, length);
    length = append_v2_logic(volume, length, child, sizeof child);
    if (game_file(path, sizeof path, game, "LOGDIR") || write_bytes(path, logdir, sizeof logdir) ||
        game_file(path, sizeof path, game, "VOL.0") || write_bytes(path, volume, length) ||
        write_replay_manifest(game, manifest) || logic_replay_trace_write(input, NULL, 0) ||
        run_reference_engine_profile(fixture_directory, game, input, manifest,
            "1", capture, NULL, "completed-cycle") ||
        logic_oracle_snapshot_load(capture, &snapshots, &count)) goto done;
    if (count != 1 || snapshots[0].room != 1 || snapshots[0].variables[42] != 99 ||
        snapshots[0].message_count != 1 || strcmp(snapshots[0].messages[0].text, text) ||
        !strstr(snapshots[0].provenance, "presentation-delays=off") ||
        !strstr(snapshots[0].provenance, "elapsed-ms=30")) {
        fprintf(stderr, "nonblocking transition clock regression: %s\n",
                count ? snapshots[0].provenance : "missing checkpoint");
        goto done;
    }
    result = 0;
done:
    free(snapshots);
    if (unlink(manifest) && errno != ENOENT) result = -1;
    if (unlink(input) && errno != ENOENT) result = -1;
    if (unlink(capture) && errno != ENOENT) result = -1;
    remove_minimal_agi_game(game);
    return result;
}

static int check_real_game_run(const char *fixture_directory,
                               const char *game_directory,
                               const char *input_path,
                               const char *manifest_path,
                               const char *snapshot_path,
                               const char *repeat_snapshot_path,
                               const char *milestones_path,
                               const char *game_id,
                               unsigned detected_agi_version,
                               int expected_version_override)
{
    LogicOracleSnapshot *snapshots = NULL;
    uint8_t first_snapshot_hash[LOGIC_REPLAY_SHA256_SIZE];
    uint8_t repeat_snapshot_hash[LOGIC_REPLAY_SHA256_SIZE];
    size_t count = 0;
    unsigned long loaded_logic_count, input_count, parser_word_count;
    unsigned long game_seconds;
    int okay = 0;
    if (run_reference_engine(fixture_directory, game_directory, input_path,
                             manifest_path,
                             "140", snapshot_path, milestones_path) != 0 ||
        logic_oracle_snapshot_load(snapshot_path, &snapshots, &count) != 0 ||
        count != 140)
        goto done;
    if (!snapshots[0].inventory_known ||
        !snapshots[count - 1].inventory_known ||
        snapshots[0].inventory_count == 0 ||
        snapshots[0].inventory_count != snapshots[count - 1].inventory_count) {
        fprintf(stderr,
                "test_logic_oracle: ScummVM object locations unavailable or unstable: first known=%u count=%u last known=%u count=%u\n",
                snapshots[0].inventory_known,
                (unsigned)snapshots[0].inventory_count,
                snapshots[count - 1].inventory_known,
                (unsigned)snapshots[count - 1].inventory_count);
        goto done;
    }
    if (snapshots[0].timing_profile != LOGIC_ORACLE_TIMING_WALL_MILLISECONDS ||
        snapshots[count - 1].timing_profile != LOGIC_ORACLE_TIMING_WALL_MILLISECONDS ||
        snapshots[0].control_flow_reliable ||
        snapshots[count - 1].control_flow_reliable ||
        snapshots[0].tick_numerator != 20 ||
        snapshots[0].tick_denominator != 1 ||
        strcmp(snapshots[0].checkpoint, "wall-ms-51") != 0 ||
        strcmp(snapshots[count - 1].checkpoint, "wall-ms-7001") != 0 ||
        strcmp(snapshots[0].rng_algorithm,
               "scummvm-randomsource-v1") != 0) {
        fprintf(stderr,
                "test_logic_oracle: ScummVM checkpoint identity mismatch: timing=%u first=%s last=%s rng=%s\n",
                (unsigned)snapshots[0].timing_profile,
                snapshots[0].checkpoint, snapshots[count - 1].checkpoint,
                snapshots[0].rng_algorithm);
        goto done;
    }
    if (strcmp(snapshots[count - 1].game_id, game_id) != 0 ||
        snapshots[count - 1].agi_version != 0x2917 ||
        snapshots[count - 1].tick != 140 ||
        snapshots[count - 1].message_count == 0 ||
        snapshots[count - 1].variables[11] == 0 ||
        strstr(snapshots[count - 1].provenance, "ScummVM") == NULL ||
        strstr(snapshots[count - 1].provenance, "det=0x") == NULL ||
        (expected_version_override &&
         strstr(snapshots[count - 1].provenance,
                "manifest-compat") == NULL) ||
        !file_has_line(milestones_path, "agi_version=0x2917") ||
        !file_has_line(milestones_path,
                       expected_version_override
                           ? "version_override=1" : "version_override=0") ||
        !file_has_line(milestones_path, "logic0_loaded=1") ||
        read_milestone_uint(milestones_path, "loaded_logic_count",
                            &loaded_logic_count) != 0 ||
        loaded_logic_count == 0 ||
        read_milestone_uint(milestones_path, "input_keydown_count",
                            &input_count) != 0 ||
        input_count < 10 ||
        read_milestone_uint(milestones_path, "max_parser_word_count",
                            &parser_word_count) != 0 ||
        parser_word_count == 0 ||
        !file_has_line(milestones_path, "parsed_look=1") ||
        read_milestone_uint(milestones_path, "max_game_seconds",
                            &game_seconds) != 0 ||
        game_seconds == 0 ||
        snapshots[count - 1].variables[11] != game_seconds)
        goto done;
    {
        char expected_game_id[64];
        char expected_detected_version[64];
        char expected_detected_provenance[32];
        int length = snprintf(expected_game_id, sizeof expected_game_id,
                              "game_id=%s", game_id);
        if (length < 0 || (size_t)length >= sizeof expected_game_id ||
            !file_has_line(milestones_path, expected_game_id))
            goto done;
        length = snprintf(expected_detected_version,
                          sizeof expected_detected_version,
                          "detected_agi_version=0x%04x",
                          detected_agi_version);
        if (length < 0 ||
            (size_t)length >= sizeof expected_detected_version ||
            !file_has_line(milestones_path, expected_detected_version))
            goto done;
        length = snprintf(expected_detected_provenance,
                          sizeof expected_detected_provenance,
                          "det=0x%04x", detected_agi_version);
        if (length < 0 ||
            (size_t)length >= sizeof expected_detected_provenance ||
            strstr(snapshots[count - 1].provenance,
                   expected_detected_provenance) == NULL)
            goto done;
    }
    if (logic_replay_sha256_file(snapshot_path, first_snapshot_hash) != 0 ||
        run_reference_engine(fixture_directory, game_directory, input_path,
                             manifest_path,
                             "140", repeat_snapshot_path, NULL) != 0 ||
        logic_replay_sha256_file(repeat_snapshot_path,
                                 repeat_snapshot_hash) != 0 ||
        memcmp(first_snapshot_hash, repeat_snapshot_hash,
               sizeof first_snapshot_hash) != 0) {
        fprintf(stderr,
                "test_logic_oracle: ScummVM %s replay was not deterministic\n",
                game_id);
        goto done;
    }
    okay = 1;

done:
    if (!okay)
        fprintf(stderr,
                "test_logic_oracle: ScummVM %s game/resource/cycle/parser milestones failed\n",
                game_id);
    free(snapshots);
    return okay ? 0 : -1;
}

static int run_candidate_engine(const char *game_directory,
                                const char *input_trace,
                                const char *manifest_path,
                                const char *snapshot_path,
                                int inject_test_fault)
{
    char *arguments[15] = {
        "obj/cx16/logic-replay", "snapshot",
        "--game-dir", (char *)game_directory,
        "--manifest", (char *)manifest_path,
        "--input-trace", (char *)input_trace,
        "--ticks", "1",
        "--snapshot", (char *)snapshot_path,
        NULL
    };
    if (inject_test_fault) {
        arguments[12] = "--test-fault";
        arguments[13] = "omit-metadata-save";
        arguments[14] = NULL;
    }
    pid_t child = fork();
    int status;
    if (child < 0) return -1;
    if (child == 0) {
        execv(arguments[0], arguments);
        perror("test_logic_oracle: exec logic-replay snapshot");
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

