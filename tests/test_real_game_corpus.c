#define _POSIX_C_SOURCE 200809L

#include "logic_replay.h"
#include "logic_oracle.h"
#include "logic_reduce.h"
#include "logic_app_replay.h"
#include "logic_replay_runner.h"
#include "x16_replay_runner.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CORPUS_MAX_GAMES 8
#define CORPUS_MAX_RESOURCES 20
#define PATH_CAPACITY 1024
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "real-game corpus regression:%d: %s\n", \
                __LINE__, #condition); \
        failed = 1; \
        goto cleanup; \
    } \
} while (0)

typedef struct CorpusResource {
    char name[LOGIC_REPLAY_RESOURCE_NAME_MAX];
    char source_name[128];
    uint8_t sha256[LOGIC_REPLAY_SHA256_SIZE];
} CorpusResource;

typedef struct CorpusGame {
    char id[16];
    char directory[64];
    char title[64];
    char agi_version[LOGIC_REPLAY_AGI_VERSION_MAX];
    char compatibility_status[128];
    char version_evidence[160];
    CorpusResource resources[CORPUS_MAX_RESOURCES];
    size_t resource_count;
    uint8_t logic_volume[3];
    uint32_t logic_bytes[3];
    size_t logic_count;
    size_t directory_counts[4];
    uint16_t object_count;
} CorpusGame;

#define IMAGE_SOURCE(source, target) source,
static const char *const overlay_sources[] = {
    LOGIC_APP_REPLAY_IMAGES(IMAGE_SOURCE)
};
#undef IMAGE_SOURCE
#define IMAGE_TARGET(source, target) target,
static const char *const overlay_targets[] = {
    LOGIC_APP_REPLAY_IMAGES(IMAGE_TARGET)
};
#undef IMAGE_TARGET
#define APP_OVERLAY_COUNT (sizeof overlay_targets / sizeof overlay_targets[0])

static const char default_games_directory[] =
    "/home/arbitrator/.cache/cx16agi/tools/games";

static int copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (!destination || !capacity || !source)
        return -1;
    length = strlen(source);
    if (length >= capacity)
        return -1;
    memcpy(destination, source, length + 1);
    return 0;
}

static int parse_sha256(const char *hex, uint8_t digest[LOGIC_REPLAY_SHA256_SIZE])
{
    size_t i;
    if (strlen(hex) != LOGIC_REPLAY_SHA256_SIZE * 2)
        return -1;
    for (i = 0; i < LOGIC_REPLAY_SHA256_SIZE; ++i) {
        unsigned value;
        if (sscanf(hex + i * 2, "%2x", &value) != 1)
            return -1;
        digest[i] = (uint8_t)value;
    }
    return 0;
}

static int split_fields(char *line, char *fields[], size_t capacity,
                        size_t *count)
{
    char *save = NULL;
    char *field;
    *count = 0;
    for (field = strtok_r(line, "|\r\n", &save); field;
         field = strtok_r(NULL, "|\r\n", &save)) {
        if (*count == capacity)
            return -1;
        fields[(*count)++] = field;
    }
    return 0;
}

static CorpusGame *find_game(CorpusGame games[], size_t game_count,
                             const char *id)
{
    size_t i;
    for (i = 0; i < game_count; ++i)
        if (!strcmp(games[i].id, id))
            return &games[i];
    return NULL;
}

static int read_corpus(const char *path, CorpusGame games[], size_t *game_count)
{
    char line[512];
    FILE *file = fopen(path, "r");
    int result = -1;
    *game_count = 0;
    if (!file) {
        fprintf(stderr, "real-game corpus: cannot read %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    while (fgets(line, sizeof line, file)) {
        char *fields[8];
        size_t count;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        if (split_fields(line, fields, sizeof fields / sizeof fields[0],
                         &count) != 0)
            goto done;
        if (count == 7 && !strcmp(fields[0], "game")) {
            CorpusGame *game;
            if (*game_count == CORPUS_MAX_GAMES ||
                find_game(games, *game_count, fields[1]))
                goto done;
            game = &games[(*game_count)++];
            memset(game, 0, sizeof *game);
            if (copy_text(game->id, sizeof game->id, fields[1]) != 0 ||
                copy_text(game->directory, sizeof game->directory,
                          fields[2]) != 0 ||
                copy_text(game->title, sizeof game->title, fields[3]) != 0 ||
                copy_text(game->agi_version, sizeof game->agi_version,
                          fields[4]) != 0 ||
                copy_text(game->compatibility_status,
                          sizeof game->compatibility_status, fields[5]) != 0 ||
                copy_text(game->version_evidence,
                          sizeof game->version_evidence, fields[6]) != 0)
                goto done;
        } else if (count == 5 && !strcmp(fields[0], "resource")) {
            CorpusGame *game = find_game(games, *game_count, fields[1]);
            CorpusResource *resource;
            size_t i;
            if (!game || game->resource_count == CORPUS_MAX_RESOURCES)
                goto done;
            for (i = 0; i < game->resource_count; ++i)
                if (!strcmp(game->resources[i].name, fields[2]))
                    goto done;
            resource = &game->resources[game->resource_count++];
            if (copy_text(resource->name, sizeof resource->name,
                          fields[2]) != 0 ||
                copy_text(resource->source_name,
                          sizeof resource->source_name, fields[3]) != 0 ||
                parse_sha256(fields[4], resource->sha256) != 0)
                goto done;
        } else {
            goto done;
        }
    }
    if (ferror(file))
        goto done;
    result = 0;
done:
    if (fclose(file) != 0)
        result = -1;
    if (result != 0)
        fprintf(stderr, "real-game corpus: malformed manifest %s\n", path);
    return result;
}

static int resource_name_valid(const char *name)
{
    char *end;
    unsigned long volume;
    static const char *const fixed[] = {
        "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "OBJECT", "WORDS.TOK",
        "KQ1.wag", "KQ3.wag"
    };
    size_t i;
    for (i = 0; i < sizeof fixed / sizeof fixed[0]; ++i)
        if (!strcmp(name, fixed[i]))
            return 1;
    if (!strncmp(name, "meta/", 5) && !strstr(name + 5, "..") &&
        !strchr(name + 5, '/') && strlen(name) < LOGIC_REPLAY_RESOURCE_NAME_MAX)
        return 1;
    if (strncmp(name, "VOL.", 4) != 0 || !name[4])
        return 0;
    errno = 0;
    volume = strtoul(name + 4, &end, 10);
    return errno == 0 && *end == '\0' && volume <= 15;
}

static int path_join(char *path, size_t capacity,
                     const char *a, const char *b, const char *c)
{
    int length = snprintf(path, capacity, "%s/%s/%s", a, b, c);
    return length > 0 && (size_t)length < capacity ? 0 : -1;
}

static int copy_file(const char *source_path, const char *destination_path)
{
    unsigned char buffer[8192];
    FILE *source = fopen(source_path, "rb");
    FILE *destination;
    int result = -1;
    if (!source)
        return -1;
    destination = fopen(destination_path, "wb");
    if (!destination) {
        fclose(source);
        return -1;
    }
    for (;;) {
        size_t count = fread(buffer, 1, sizeof buffer, source);
        if (count && fwrite(buffer, 1, count, destination) != count)
            goto done;
        if (count < sizeof buffer) {
            if (ferror(source))
                goto done;
            break;
        }
    }
    result = 0;
done:
    if (fclose(source) != 0)
        result = -1;
    if (fclose(destination) != 0)
        result = -1;
    return result;
}

static int stage_game(const char *games_directory, const char *fixture,
                      CorpusGame *game, char *staged_directory,
                      size_t staged_directory_capacity)
{
    char source_path[PATH_CAPACITY], destination_path[PATH_CAPACITY];
    size_t i;
    if (path_join(staged_directory, staged_directory_capacity, fixture,
                  game->id, "") != 0)
        return -1;
    staged_directory[strlen(staged_directory) - 1] = '\0';
    if (mkdir(staged_directory, 0700) != 0)
        return -1;
    for (i = 0; i < game->resource_count; ++i) {
        CorpusResource *resource = &game->resources[i];
        uint8_t digest[LOGIC_REPLAY_SHA256_SIZE];
        size_t hash_index;
        char actual_hex[LOGIC_REPLAY_SHA256_SIZE * 2 + 1];
        char expected_hex[LOGIC_REPLAY_SHA256_SIZE * 2 + 1];
        if (!resource_name_valid(resource->name) ||
            path_join(source_path, sizeof source_path, games_directory,
                      game->directory, resource->source_name) != 0 ||
            path_join(destination_path, sizeof destination_path,
                      staged_directory, resource->name, "") != 0)
            return -1;
        destination_path[strlen(destination_path) - 1] = '\0';
        if (!strncmp(resource->name, "meta/", 5)) {
            char metadata_directory[PATH_CAPACITY];
            if (path_join(metadata_directory, sizeof metadata_directory,
                          staged_directory, "meta", "") != 0)
                return -1;
            metadata_directory[strlen(metadata_directory) - 1] = '\0';
            if (mkdir(metadata_directory, 0700) != 0 && errno != EEXIST)
                return -1;
        }
        if (copy_file(source_path, destination_path) != 0) {
            fprintf(stderr, "real-game corpus: cannot stage %s to %s: %s\n",
                    source_path, destination_path, strerror(errno));
            return -1;
        }
        if (logic_replay_sha256_file(destination_path, digest) != 0) {
            fprintf(stderr, "real-game corpus: cannot hash staged %s: %s\n",
                    destination_path, strerror(errno));
            return -1;
        }
        if (memcmp(digest, resource->sha256, sizeof digest) != 0) {
            for (hash_index = 0; hash_index < sizeof digest; ++hash_index) {
                snprintf(actual_hex + hash_index * 2, 3, "%02x",
                         digest[hash_index]);
                snprintf(expected_hex + hash_index * 2, 3, "%02x",
                         resource->sha256[hash_index]);
            }
            fprintf(stderr,
                    "real-game corpus: %s %s staged at %s has SHA-256 %s, manifest expects %s (first differing byte ",
                    game->id, resource->name, destination_path,
                    actual_hex, expected_hex);
            for (hash_index = 0; hash_index < sizeof digest; ++hash_index)
                if (digest[hash_index] != resource->sha256[hash_index]) {
                    fprintf(stderr, "%lu: %02x != %02x)\n",
                            (unsigned long)hash_index,
                            digest[hash_index], resource->sha256[hash_index]);
                    break;
                }
            if (hash_index == sizeof digest)
                fprintf(stderr, "none; memcmp returned nonzero)\n");
            return -1;
        }
    }
    return 0;
}

static int stage_app_overlays(const char *application_directory,
                              const char *staged_directory)
{
    char source[PATH_CAPACITY], destination[PATH_CAPACITY];
    size_t i;
    for (i = 0; i < APP_OVERLAY_COUNT; ++i) {
        if (path_join(source, sizeof source, application_directory,
                      overlay_sources[i], "") != 0 ||
            path_join(destination, sizeof destination, staged_directory,
                      overlay_targets[i], "") != 0)
            return -1;
        source[strlen(source) - 1] = '\0';
        destination[strlen(destination) - 1] = '\0';
        if (copy_file(source, destination) != 0) {
            fprintf(stderr, "real-game corpus: missing application overlay %s\n",
                    source);
            return -1;
        }
    }
    return 0;
}

static const CorpusResource *find_resource(const CorpusGame *game,
                                           const char *name)
{
    size_t i;
    for (i = 0; i < game->resource_count; ++i)
        if (!strcmp(game->resources[i].name, name))
            return &game->resources[i];
    return NULL;
}

static int read_file(const char *path, uint8_t **bytes, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    size_t read_count;
    uint8_t *buffer;
    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file)
            fclose(file);
        return -1;
    }
    buffer = (uint8_t *)malloc(length ? (size_t)length : 1);
    if (!buffer) {
        fclose(file);
        return -1;
    }
    read_count = fread(buffer, 1, (size_t)length, file);
    if (fclose(file) != 0 || read_count != (size_t)length) {
        free(buffer);
        return -1;
    }
    *bytes = buffer;
    *size = (size_t)length;
    return 0;
}

static int run_process(char *const arguments[])
{
    pid_t child = fork();
    int status;
    if (child < 0)
        return -1;
    if (child == 0) {
        execvp(arguments[0], arguments);
        fprintf(stderr, "real-game corpus: cannot execute %s: %s\n",
                arguments[0], strerror(errno));
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int run_process_in_directory(char *const arguments[],
                                    const char *directory)
{
    pid_t child = fork();
    int status;
    if (child < 0)
        return -1;
    if (child == 0) {
        if (chdir(directory) != 0)
            _exit(126);
        execvp(arguments[0], arguments);
        fprintf(stderr, "real-game corpus: cannot execute %s: %s\n",
                arguments[0], strerror(errno));
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int absolute_path(char destination[], size_t capacity,
                         const char *path)
{
    char current_directory[PATH_CAPACITY];
    int length;
    if (!path || !*path)
        return -1;
    if (path[0] == '/')
        return copy_text(destination, capacity, path);
    if (!getcwd(current_directory, sizeof current_directory))
        return -1;
    length = snprintf(destination, capacity, "%s/%s", current_directory, path);
    return length < 0 || (size_t)length >= capacity ? -1 : 0;
}

static const char *logic_replay_cli(void)
{
    const char *path = getenv("CX16_LOGIC_REPLAY_CLI");
    return path && *path ? path : "obj/cx16/logic-replay";
}

static int run_full_app_cli(const char *game_directory, uint32_t ticks,
                            uint32_t timeout_ms, const char *output_path,
                            uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE])
{
    char manifest[PATH_CAPACITY], trace[PATH_CAPACITY];
    char tick_text[16], timeout_text[16];
    uint8_t *bytes = NULL;
    size_t size = 0;
    char *arguments[16];
    int count = 0, length;
    length = snprintf(manifest, sizeof manifest, "%s/app.manifest", game_directory);
    if (length < 0 || (size_t)length >= sizeof manifest)
        return -1;
    length = snprintf(trace, sizeof trace, "%s/app.ltrace", game_directory);
    if (length < 0 || (size_t)length >= sizeof trace)
        return -1;
    snprintf(tick_text, sizeof tick_text, "%u", (unsigned)ticks);
    snprintf(timeout_text, sizeof timeout_text, "%u", (unsigned)timeout_ms);
    arguments[count++] = (char *)logic_replay_cli();
    arguments[count++] = "full-app";
    arguments[count++] = "--game-dir";
    arguments[count++] = (char *)game_directory;
    arguments[count++] = "--manifest";
    arguments[count++] = manifest;
    arguments[count++] = "--input-trace";
    arguments[count++] = trace;
    arguments[count++] = "--ticks";
    arguments[count++] = tick_text;
    arguments[count++] = "--timeout-ms";
    arguments[count++] = timeout_text;
    arguments[count++] = "--output";
    arguments[count++] = (char *)output_path;
    arguments[count] = NULL;
    {
        int process_status = run_process(arguments);
        if (process_status != 0) {
            fprintf(stderr, "full-app CLI exited with status %d for %s\n",
                    process_status, output_path);
            return -1;
        }
    }
    if (read_file(output_path, &bytes, &size) != 0) {
        fprintf(stderr, "full-app CLI did not create %s: %s\n",
                output_path, strerror(errno));
        return -1;
    }
    if (size != LOGIC_APP_REPLAY_OUTPUT_SIZE) {
        fprintf(stderr, "full-app CLI wrote %lu checkpoint bytes to %s; expected %u\n",
                (unsigned long)size, output_path,
                (unsigned)LOGIC_APP_REPLAY_OUTPUT_SIZE);
        free(bytes);
        return -1;
    }
    memcpy(snapshot, bytes, size);
    free(bytes);
    return 0;
}

static int run_full_app_reducer_cli(const char *game_directory,
                                    uint32_t ticks, uint32_t timeout_ms,
                                    const char *output_path, int semantic)
{
    char manifest[PATH_CAPACITY], trace[PATH_CAPACITY];
    char tick_text[16], timeout_text[16];
    char *arguments[24];
    int count = 0, length;
    length = snprintf(manifest, sizeof manifest, "%s/app.manifest", game_directory);
    if (length < 0 || (size_t)length >= sizeof manifest)
        return -1;
    length = snprintf(trace, sizeof trace, "%s/app.ltrace", game_directory);
    if (length < 0 || (size_t)length >= sizeof trace)
        return -1;
    snprintf(tick_text, sizeof tick_text, "%u", (unsigned)ticks);
    /* A reduction boots many candidates; its total budget is separate from
       the timeout for one application replay. Keep the budget bounded. */
    uint32_t reduction_timeout = timeout_ms <= UINT32_MAX / 3u ?
                                 timeout_ms * 3u : UINT32_MAX;
    snprintf(timeout_text, sizeof timeout_text, "%u", (unsigned)reduction_timeout);
    arguments[count++] = (char *)logic_replay_cli();
    arguments[count++] = "app-reduce";
    if (semantic) {
        arguments[count++] = "--failure";
        arguments[count++] = "semantic";
    }
    arguments[count++] = "--game-dir";
    arguments[count++] = (char *)game_directory;
    arguments[count++] = "--manifest";
    arguments[count++] = manifest;
    arguments[count++] = "--input-trace";
    arguments[count++] = trace;
    arguments[count++] = "--ticks";
    arguments[count++] = tick_text;
    arguments[count++] = "--fault";
    arguments[count++] = semantic ? "semantic-write" : "omit-metadata-save";
    arguments[count++] = "--output-trace";
    arguments[count++] = (char *)output_path;
    arguments[count++] = "--max-attempts";
    arguments[count++] = "20";
    arguments[count++] = "--timeout-ms";
    arguments[count++] = timeout_text;
    arguments[count] = NULL;
    return run_process(arguments);
}

static int write_scummvm_input_trace(const char *game_directory,
                                     const LogicReplayEvent *events,
                                     size_t event_count)
{
    char path[PATH_CAPACITY];
    LogicReplayEvent expanded[LOGIC_APP_REPLAY_INPUT_MAX * 2u];
    size_t i;
    int length = snprintf(path, sizeof path, "%s/scummvm.ltrace",
                          game_directory);
    if (length < 0 || (size_t)length >= sizeof path ||
        event_count > LOGIC_APP_REPLAY_INPUT_MAX ||
        (event_count && !events))
        return -1;
    for (i = 0; i < event_count; ++i) {
        LogicReplayEvent *down = &expanded[i * 2];
        LogicReplayEvent *up = &expanded[i * 2 + 1];
        if (events[i].kind != LOGIC_TRACE_KIND_MARK ||
            events[i].reason != LOGIC_REPLAY_REASON_SEMANTIC_KEYPRESS ||
            events[i].logic != 0 || !events[i].tick)
            return -1;
        *down = events[i];
        down->sequence = (uint32_t)(i * 2);
        down->reason = LOGIC_REPLAY_REASON_INPUT_DOWN;
        down->before = 0;
        down->after = 1;
        *up = events[i];
        up->sequence = (uint32_t)(i * 2 + 1);
        up->reason = LOGIC_REPLAY_REASON_INPUT_UP;
        up->before = 1;
        up->after = 0;
    }
    return logic_replay_trace_write(path, expanded, event_count * 2);
}

static int run_scummvm_oracle(const char *game_directory, uint32_t ticks,
                              const char *snapshot_path)
{
    const char *oracle = getenv("CX16_SCUMMVM_ORACLE");
    char oracle_path[PATH_CAPACITY], tick_text[16];
    char *arguments[17];
    const char *snapshot_name;
    int count = 0;
    if (!oracle || !*oracle)
        oracle = "obj/cx16/logic-oracle-scummvm";
    if (absolute_path(oracle_path, sizeof oracle_path, oracle) != 0)
        return -1;
    snapshot_name = strrchr(snapshot_path, '/');
    snapshot_name = snapshot_name ? snapshot_name + 1 : snapshot_path;
    snprintf(tick_text, sizeof tick_text, "%u", (unsigned)ticks);
    arguments[count++] = oracle_path;
    arguments[count++] = "--game-dir";
    arguments[count++] = ".";
    arguments[count++] = "--manifest";
    arguments[count++] = "app.manifest";
    arguments[count++] = "--input-trace";
    arguments[count++] = "scummvm.ltrace";
    arguments[count++] = "--ticks";
    arguments[count++] = tick_text;
    arguments[count++] = "--snapshot";
    arguments[count++] = (char *)snapshot_name;
    arguments[count++] = "--milestones";
    arguments[count++] = "--checkpoint-profile";
    arguments[count++] = "completed-cycle";
    arguments[count] = NULL;
    return run_process_in_directory(arguments, game_directory);
}

static int inspect_logic_resources(CorpusGame *game,
                                   const char *staged_directory,
                                   uint32_t code_sizes[3],
                                   size_t *logic_count)
{
    char path[PATH_CAPACITY];
    uint8_t *logdir = NULL;
    size_t logdir_size = 0;
    static const char *const directory_names[4] = {
        "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR"
    };
    unsigned logic, directory_index;
    int result = -1;
    int needed_volumes[16] = {0};
    for (directory_index = 0; directory_index < 4; ++directory_index) {
        uint8_t *directory = NULL;
        size_t directory_size = 0;
        size_t entry_index;
        if (!find_resource(game, directory_names[directory_index]) ||
            path_join(path, sizeof path, staged_directory,
                      directory_names[directory_index], "") != 0)
            goto done;
        path[strlen(path) - 1] = '\0';
        if (read_file(path, &directory, &directory_size) != 0 ||
            directory_size % 3 != 0) {
            free(directory);
            goto done;
        }
        game->directory_counts[directory_index] = directory_size / 3;
        for (entry_index = 0; entry_index < directory_size / 3;
             ++entry_index) {
            const uint8_t *entry = directory + entry_index * 3;
            if (!(entry[0] == 0xFF && entry[1] == 0xFF &&
                  entry[2] == 0xFF))
                needed_volumes[entry[0] >> 4] = 1;
        }
        if (directory_index == 0) {
            logdir = directory;
            logdir_size = directory_size;
        } else {
            free(directory);
        }
    }
    *logic_count = logdir_size / 3;
    if (!*logic_count || game->resource_count < 12 ||
        !find_resource(game, "OBJECT") || !find_resource(game, "WORDS.TOK"))
        goto done;
    {
        static const char *const meta_suffixes[6] = {
            "logic-ids.idx", "logic-ids.bin", "sound-ids.idx",
            "sound-ids.bin", "view-ids.idx", "view-ids.bin"
        };
        char meta_name[64];
        size_t meta_index;
        for (meta_index = 0; meta_index < 6; ++meta_index) {
            if (snprintf(meta_name, sizeof meta_name, "meta/%s-%s",
                         game->id, meta_suffixes[meta_index]) <= 0 ||
                !find_resource(game, meta_name))
                goto done;
        }
    }
    for (logic = 0; logic < 3; ++logic) {
        const uint8_t *entry = logdir + logic * 3;
        unsigned volume_number;
        size_t offset;
        uint8_t *volume = NULL;
        size_t volume_size = 0;
        char volume_name[16];
        uint16_t payload_size, code_size;
        size_t code_at, i;
        int any_code = 0;
        if (entry[0] == 0xFF && entry[1] == 0xFF && entry[2] == 0xFF)
            goto done;
        volume_number = entry[0] >> 4;
        offset = ((size_t)(entry[0] & 0x0Fu) << 16) |
                 ((size_t)entry[1] << 8) | entry[2];
        if (snprintf(volume_name, sizeof volume_name, "VOL.%u",
                     volume_number) < 0 ||
            !find_resource(game, volume_name) ||
            path_join(path, sizeof path, staged_directory, volume_name,
                      "") != 0)
            goto done;
        path[strlen(path) - 1] = '\0';
        if (read_file(path, &volume, &volume_size) != 0)
            goto done;
        if (offset > volume_size || volume_size - offset < 7 ||
            volume[offset] != 0x12 || volume[offset + 1] != 0x34 ||
            volume[offset + 2] != volume_number) {
            free(volume);
            goto done;
        }
        payload_size = (uint16_t)(volume[offset + 3] |
                                  ((uint16_t)volume[offset + 4] << 8));
        code_at = offset + 7;
        code_size = (uint16_t)(volume[offset + 5] |
                               ((uint16_t)volume[offset + 6] << 8));
        if (!code_size || payload_size < 2 ||
            payload_size > volume_size - offset - 5 ||
            code_size > payload_size - 2 ||
            code_at > volume_size || code_size > volume_size - code_at) {
            free(volume);
            goto done;
        }
        for (i = 0; i < code_size; ++i)
            any_code |= volume[code_at + i] != 0;
        free(volume);
        if (!any_code)
            goto done;
        game->logic_volume[logic] = (uint8_t)volume_number;
        code_sizes[logic] = code_size;
        needed_volumes[volume_number] = 1;
    }
    for (logic = 0; logic < 16; ++logic) {
        char volume_name[16];
        if (!needed_volumes[logic])
            continue;
        if (snprintf(volume_name, sizeof volume_name, "VOL.%u", logic) < 0 ||
            !find_resource(game, volume_name))
            goto done;
    }
    {
        size_t minimum_resources = 12;
        for (logic = 0; logic < 16; ++logic)
            minimum_resources += (size_t)needed_volumes[logic];
        ++minimum_resources; /* The matching WinAGI version record is staged. */
        if (game->resource_count != minimum_resources)
            goto done;
    }
    result = 0;
done:
    free(logdir);
    if (result != 0)
        fprintf(stderr, "real-game corpus: %s LOGDIR or logic 0..2 volume records are invalid/incomplete\n",
                game->id);
    return result;
}

static int inspect_object_count(const char *game_directory, uint16_t *count)
{
    static const uint8_t key[] = "Avis Durgan";
    char path[PATH_CAPACITY];
    uint8_t *data = NULL;
    size_t size, i, check_length;
    uint32_t encoded_count;
    int encrypted = 0, length, result = -1;
    length = snprintf(path, sizeof path, "%s/OBJECT", game_directory);
    if (length < 0 || (size_t)length >= sizeof path ||
        read_file(path, &data, &size) != 0 || size < 2)
        goto done;
    check_length = size < 20 ? 10 : 20;
    for (i = size - 1; i > size - check_length; --i)
        if ((data[i] < 0x20 || data[i] > 0x7F) && data[i] != 0) {
            encrypted = 1;
            break;
        }
    if (encrypted)
        for (i = 0; i < size; ++i)
            data[i] ^= key[i % (sizeof key - 1)];
    encoded_count = ((uint32_t)data[1] << 8) | data[0];
    encoded_count /= 3;
    if (!encoded_count || encoded_count > LOGIC_APP_REPLAY_INVENTORY_MAX)
        goto done;
    *count = (uint16_t)encoded_count;
    result = 0;
done:
    free(data);
    return result;
}

static int unlink_if_present(const char *path)
{
    if (!path[0] || unlink(path) == 0 || errno == ENOENT)
        return 0;
    fprintf(stderr, "real-game corpus: cannot remove %s: %s\n",
            path, strerror(errno));
    return -1;
}

static int rmdir_if_present(const char *path)
{
    if (rmdir(path) == 0 || errno == ENOENT)
        return 0;
    fprintf(stderr, "real-game corpus: cannot remove %s: %s\n",
            path, strerror(errno));
    return -1;
}

static int clean_fixture(const char *fixture, const CorpusGame games[],
                         size_t game_count)
{
    char directory[PATH_CAPACITY], path[PATH_CAPACITY];
    size_t i, j;
    int failed = 0;
    if (!fixture[0])
        return 0;
    for (i = 0; i < game_count; ++i) {
        int length = snprintf(directory, sizeof directory, "%s/%s", fixture,
                              games[i].id);
        if (length < 0 || (size_t)length >= sizeof directory) {
            failed = 1;
            continue;
        }
        for (j = 0; j < games[i].resource_count; ++j) {
            length = snprintf(path, sizeof path, "%s/%s", directory,
                              games[i].resources[j].name);
            if (length < 0 || (size_t)length >= sizeof path ||
                unlink_if_present(path) != 0)
                failed = 1;
        }
        for (j = 0; j < APP_OVERLAY_COUNT; ++j) {
            length = snprintf(path, sizeof path, "%s/%s", directory,
                              overlay_targets[j]);
            if (length < 0 || (size_t)length >= sizeof path ||
                unlink_if_present(path) != 0)
                failed = 1;
        }
        {
            static const char *const replay_files[] = {
                "replay.in", "replay.out", "replay.done", "replay.stage",
                "app.manifest", "app.ltrace", "reference.lors",
                "scummvm.ltrace",
                "full-app-1.x16r", "full-app-2.x16r", "full-app-failure.x16r", "reduced.ltrace",
                "scummvm.ini"
            };
            for (j = 0; j < sizeof replay_files / sizeof replay_files[0];
                 ++j) {
                length = snprintf(path, sizeof path, "%s/%s", directory,
                                  replay_files[j]);
                if (length < 0 || (size_t)length >= sizeof path ||
                    unlink_if_present(path) != 0)
                    failed = 1;
            }
        }
        length = snprintf(path, sizeof path, "%s/meta", directory);
        if (length < 0 || (size_t)length >= sizeof path ||
            rmdir_if_present(path) != 0)
            failed = 1;
        if (rmdir_if_present(directory) != 0)
            failed = 1;
    }
    if (rmdir_if_present(fixture) != 0)
        failed = 1;
    return failed ? -1 : 0;
}

static uint16_t get_u16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int parse_agi_version(const char *text, uint16_t *version)
{
    char digits[8];
    const char *dot;
    size_t before, after;
    char *end;
    unsigned long value;
    if (!text || !*text || !version)
        return -1;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        value = strtoul(text, &end, 0);
        if (*end || !value || value > UINT16_MAX)
            return -1;
        *version = (uint16_t)value;
        return 0;
    }
    dot = strchr(text, '.');
    if (!dot)
        return -1;
    before = (size_t)(dot - text);
    after = strlen(dot + 1);
    if (!before || before > 2 || after != 3 || before + after >= sizeof digits)
        return -1;
    memcpy(digits, text, before);
    memcpy(digits + before, dot + 1, after + 1);
    for (size_t i = 0; i < before + after; ++i)
        if (!((digits[i] >= '0' && digits[i] <= '9') ||
              (digits[i] >= 'a' && digits[i] <= 'f') ||
              (digits[i] >= 'A' && digits[i] <= 'F')))
            return -1;
    value = strtoul(digits, &end, 16);
    if (*end || !value || value > UINT16_MAX)
        return -1;
    *version = (uint16_t)value;
    return 0;
}

