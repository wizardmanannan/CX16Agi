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

static uint64_t monotonic_milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000u +
           (uint64_t)now.tv_nsec / 1000000u;
}

static int run_app_checkpoint(const char *name, const char *game_directory,
                              uint16_t seed,
                              const LogicAppReplayInput *events,
                              size_t event_count, uint32_t ticks,
                              uint32_t timeout_ms, uint8_t fault_flags,
                              uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE],
                              char *detail, size_t detail_capacity)
{
    uint64_t started = monotonic_milliseconds();
    uint64_t finished;
    int result = logic_app_replay_x16_run(
        game_directory, seed, events, event_count, ticks, timeout_ms,
        fault_flags, snapshot, detail, detail_capacity);
    finished = monotonic_milliseconds();
    if (result == 0) {
        printf("full-app checkpoint-run: %s wall-ms=%llu tick=%u elapsed-vsync=%u cycles=%u context-bytes=%u sampled-hw-call-stack-depth=%u sampled-sw-call-stack-growth=%u rng-calls=%u\n",
               name, (unsigned long long)(finished - started),
               (unsigned)get_u32(snapshot + 6),
               (unsigned)get_u32(snapshot +
                   LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET),
               (unsigned)snapshot[38], (unsigned)get_u16(snapshot + 53),
               (unsigned)snapshot[55], (unsigned)get_u16(snapshot + 56),
               (unsigned)get_u32(snapshot + 60));
    } else {
        printf("full-app checkpoint-run: %s wall-ms=%llu failed: %s\n",
               name, (unsigned long long)(finished - started),
               detail && *detail ? detail : "no runner detail");
    }
    return result;
}

static int create_app_replay_identity(
    const CorpusGame *game, const char *game_directory,
    const char *application_build, uint16_t seed,
    const LogicAppReplayInput *input, size_t input_count,
    LogicReplayManifest *manifest, LogicReplayEvent trace[],
    uint8_t manifest_hash[LOGIC_REPLAY_SHA256_SIZE],
    uint8_t input_hash[LOGIC_REPLAY_SHA256_SIZE])
{
    char manifest_path[PATH_CAPACITY], trace_path[PATH_CAPACITY];
    size_t i;
    int length;
    if (!game || !game_directory || !application_build || !manifest ||
        !trace || !manifest_hash || !input_hash ||
        input_count > LOGIC_APP_REPLAY_INPUT_MAX ||
        (input_count && !input))
        return -1;
    memset(manifest, 0, sizeof *manifest);
    uint16_t agi_version;
    char version_text[8];
    if (parse_agi_version(game->agi_version, &agi_version) != 0 ||
        copy_text(manifest->engine, sizeof manifest->engine,
                  !strcmp(game->id, "KQ1") ? "kq1" :
                  !strcmp(game->id, "KQ3") ? "kq3" : "") != 0 ||
        copy_text(manifest->rng_algorithm,
                  sizeof manifest->rng_algorithm,
                  LOGIC_APP_REPLAY_RNG_ALGORITHM) != 0)
        return -1;
    snprintf(version_text, sizeof version_text, "0x%04x", agi_version);
    if (copy_text(manifest->agi_version, sizeof manifest->agi_version,
                  version_text) != 0)
        return -1;
    if (!manifest->engine[0])
        return -1;
    {
        int build_length = snprintf(manifest->build,
                                    sizeof manifest->build,
                                    "cx16-full-app-%.16s", application_build);
        if (build_length < 0 || (size_t)build_length >= sizeof manifest->build)
            return -1;
    }
    manifest->version = LOGIC_REPLAY_MANIFEST_VERSION;
    manifest->resource_count = (uint16_t)game->resource_count;
    manifest->rng_seed = seed;
    manifest->tick_numerator = 10;
    manifest->tick_denominator = 1;
    for (i = 0; i < game->resource_count; ++i) {
        if (copy_text(manifest->resources[i].name,
                      sizeof manifest->resources[i].name,
                      game->resources[i].name) != 0)
            return -1;
        memcpy(manifest->resources[i].sha256, game->resources[i].sha256,
               sizeof manifest->resources[i].sha256);
    }
    if (game->resource_count + APP_OVERLAY_COUNT > LOGIC_REPLAY_MAX_RESOURCES)
        return -1;
    for (i = 0; i < APP_OVERLAY_COUNT; ++i) {
        char path[PATH_CAPACITY];
        LogicReplayResource *image = &manifest->resources[manifest->resource_count];
        if (copy_text(image->name, sizeof image->name, overlay_targets[i]) ||
            snprintf(path, sizeof path, "%s/%s", game_directory,
                     overlay_targets[i]) >= (int)sizeof path ||
            logic_replay_sha256_file(path, image->sha256))
            return -1;
        ++manifest->resource_count;
    }
    for (i = 0; i < input_count; ++i) {
        if (!input[i].frame || input[i].frame % 6u ||
            input[i].frame / 6u > UINT32_MAX)
            return -1;
        memset(&trace[i], 0, sizeof trace[i]);
        trace[i].sequence = (uint32_t)i;
        trace[i].tick = input[i].frame / 6u;
        trace[i].kind = LOGIC_TRACE_KIND_MARK;
        trace[i].field = input[i].petscii;
        trace[i].before = 0;
        trace[i].after = 1;
        trace[i].reason = LOGIC_REPLAY_REASON_SEMANTIC_KEYPRESS;
    }
    if (logic_replay_trace_validate(trace, input_count) != 0 ||
        logic_replay_manifest_validate(manifest) != 0)
        return -1;
    length = snprintf(manifest_path, sizeof manifest_path,
                       "%s/app.manifest", game_directory);
    if (length < 0 || (size_t)length >= sizeof manifest_path)
        return -1;
    length = snprintf(trace_path, sizeof trace_path,
                       "%s/app.ltrace", game_directory);
    if (length < 0 || (size_t)length >= sizeof trace_path)
        return -1;
    if (logic_replay_manifest_write(manifest_path, manifest) != 0 ||
        logic_replay_manifest_verify_resources(manifest, game_directory) != 0 ||
        logic_replay_trace_write(trace_path, trace, input_count) != 0 ||
        logic_replay_sha256_file(manifest_path, manifest_hash) != 0 ||
        logic_replay_sha256_file(trace_path, input_hash) != 0)
        return -1;
    return 0;
}

static void print_full_app_checkpoint(const CorpusGame *game,
                                      const uint8_t *snapshot)
{
    unsigned i;
    uint8_t loaded_count = 0;
    fprintf(stderr,
            "full-app checkpoint detail: %s room=%u logic=%u pc=%04x opcode=%u depth=%u error=%u tick=%u elapsed-vsync=%u input=%u/%u parsed=%u words=%u status=%02x trace=%u dropped=%u rng-calls=%u context-frames=%u\n",
            game->id, (unsigned)snapshot[15], (unsigned)snapshot[16],
            (unsigned)get_u16(snapshot + 17), (unsigned)snapshot[19],
            (unsigned)snapshot[20], (unsigned)snapshot[21],
            (unsigned)get_u32(snapshot + 6),
            (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET),
            (unsigned)get_u16(snapshot + 12),
            (unsigned)get_u16(snapshot + 10),
            (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET),
            (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_WORD_COUNT_OFFSET),
            (unsigned)snapshot[58], (unsigned)snapshot[41],
            (unsigned)get_u32(snapshot + 42), (unsigned)get_u32(snapshot + 60),
            (unsigned)snapshot[LOGIC_APP_REPLAY_CONTEXT_FRAME_COUNT_OFFSET]);
    for (i = 0; i < 256; ++i) {
        if (snapshot[LOGIC_APP_REPLAY_LOADED_LOGICS_OFFSET + i / 8] &
            (uint8_t)(1u << (i & 7))) {
            fprintf(stderr, "  loaded logic %u\n", i);
            ++loaded_count;
        }
    }
    for (i = 0; i < snapshot[41]; ++i) {
        uint16_t offset = (uint16_t)(LOGIC_APP_REPLAY_TRACE_OFFSET +
            i * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE);
        fprintf(stderr,
                "  trace seq=%u tick=%u kind=%u logic=%u pc=%04x opcode=%u depth=%u bank=%u field=%u before=%08x after=%08x reason=%u\n",
                (unsigned)get_u32(snapshot + offset),
                (unsigned)get_u32(snapshot + offset + 4),
                (unsigned)snapshot[offset + 8],
                (unsigned)snapshot[offset + 9],
                (unsigned)get_u16(snapshot + offset + 10),
                (unsigned)snapshot[offset + 12],
                (unsigned)snapshot[offset + 13],
                (unsigned)snapshot[offset + 14],
                (unsigned)snapshot[offset + 15],
                (unsigned)get_u32(snapshot + offset + 16),
                (unsigned)get_u32(snapshot + offset + 20),
                (unsigned)snapshot[offset + 24]);
    }
    for (i = 0; i < snapshot[46] && i < LOGIC_APP_REPLAY_MESSAGE_MAX; ++i) {
        size_t offset = LOGIC_APP_REPLAY_MESSAGES_OFFSET +
            i * LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE;
        size_t text_length = snapshot[offset + 2];
        fprintf(stderr, "  message logic=%u id=%u text=\"",
                (unsigned)snapshot[offset], (unsigned)snapshot[offset + 1]);
        fwrite(snapshot + offset + 3, 1, text_length, stderr);
        fputs("\"\n", stderr);
    }
    {
        const size_t parser = LOGIC_APP_REPLAY_PARSER_OFFSET;
        uint16_t words = get_u16(snapshot + parser);
        fprintf(stderr, "  parser words=%u", (unsigned)words);
        for (i = 0; i < words && i < 10; ++i)
            fprintf(stderr, " %u", (unsigned)get_u16(snapshot + parser + 2 + i * 2));
        fputc('\n', stderr);
    }
    fprintf(stderr, "  loaded logic count=%u\n", (unsigned)loaded_count);
}

static int trace_window_is_honest(const uint8_t *snapshot)
{
    uint8_t count = snapshot[41];
    uint32_t dropped = get_u32(snapshot + 42);
    uint8_t status = snapshot[58];
    uint16_t capacity =
        get_u16(snapshot + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET);
    unsigned i;
    if (count > LOGIC_APP_REPLAY_TRACE_MAX ||
        (count == LOGIC_APP_REPLAY_TRACE_MAX
             ? dropped != get_u32(snapshot + LOGIC_APP_REPLAY_TRACE_OFFSET)
             : dropped != 0) ||
        (!!(status & LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED) != !!dropped) ||
        (!!(capacity & LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED) != !!dropped) ||
        (!!(status & LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY) != !!capacity) ||
        (status & (uint8_t)~(
            LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY)) ||
        (capacity & (uint16_t)~LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED))
        return 0;
    if (count && !dropped &&
        get_u32(snapshot + LOGIC_APP_REPLAY_TRACE_OFFSET) != 0)
        return 0;
    for (i = 1; i < count; ++i) {
        size_t previous = LOGIC_APP_REPLAY_TRACE_OFFSET +
            (i - 1) * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE;
        size_t current = LOGIC_APP_REPLAY_TRACE_OFFSET +
            i * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE;
        if (get_u32(snapshot + current) != get_u32(snapshot + previous) + 1u)
            return 0;
    }
    return 1;
}

static int verify_full_app_logic_trace(const CorpusGame *game,
                                       const uint8_t *snapshot,
                                       int require_non_whitelisted_write,
                                       int report_missing)
{
    uint8_t count = snapshot[41];
    uint32_t dropped = get_u32(snapshot + 42);
    int has_call = 0, has_return = 0, has_other_write = 0;
    unsigned i;
    if (!trace_window_is_honest(snapshot)) {
        fprintf(stderr,
                "full-app trace regression: %s has inconsistent recent-ring count=%u dropped=%u first-sequence=%u status=%02x capacity=%04x\n",
                game->id, (unsigned)count, (unsigned)dropped,
                count ? (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_TRACE_OFFSET) : 0,
                (unsigned)snapshot[58],
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET));
        return -1;
    }
    for (i = 0; i < count; ++i) {
        size_t offset = LOGIC_APP_REPLAY_TRACE_OFFSET +
            i * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE;
        uint8_t kind = snapshot[offset + 8];
        uint8_t field = snapshot[offset + 15];
        uint32_t tick = get_u32(snapshot + offset + 4);
        if (!tick || tick > get_u32(snapshot + 6) ||
            get_u16(snapshot + offset + 10) >= 0x8000u) {
            fprintf(stderr,
                    "full-app trace provenance regression: %s tick=%u checkpoint=%u offset=%04x\n",
                    game->id, (unsigned)tick,
                    (unsigned)get_u32(snapshot + 6),
                    (unsigned)get_u16(snapshot + offset + 10));
            return -1;
        }
        if (kind == LOGIC_TRACE_KIND_CALL)
            has_call = 1;
        else if (kind == LOGIC_TRACE_KIND_RETURN)
            has_return = 1;
        else if (kind == LOGIC_TRACE_KIND_WRITE && field != 0 && field != 11 &&
                 field != 39 && field != 101 && field != 229)
            has_other_write = 1;
    }
    if (!has_call || !has_return ||
        (require_non_whitelisted_write && !has_other_write)) {
        if (report_missing)
            fprintf(stderr,
                    "full-app trace regression: %s recent trace lacks production logic path events (CALL=%u RETURN=%u non-whitelisted-WRITE=%u count=%u dropped=%u)\n",
                    game->id, has_call, has_return, has_other_write,
                    (unsigned)count, (unsigned)dropped);
        return -1;
    }
    return 0;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int snapshot_has_message(const uint8_t *snapshot,
                                const char *expected_text)
{
    uint8_t message_count = snapshot[46];
    unsigned message_index;
    if (message_count > LOGIC_APP_REPLAY_MESSAGE_MAX)
        return 0;
    for (message_index = 0; message_index < message_count; ++message_index) {
        size_t offset = LOGIC_APP_REPLAY_MESSAGES_OFFSET +
            message_index * LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE;
        size_t text_length = snapshot[offset + 2];
        size_t expected_length = strlen(expected_text);
        size_t start;
        if (expected_length > text_length)
            continue;
        for (start = 0; start + expected_length <= text_length; ++start)
            if (memcmp(snapshot + offset + 3 + start, expected_text,
                       expected_length) == 0)
                return 1;
    }
    return 0;
}

static int loaded_logic(const uint8_t *snapshot, uint8_t logic)
{
    return (snapshot[LOGIC_APP_REPLAY_LOADED_LOGICS_OFFSET + logic / 8] &
            (uint8_t)(1u << (logic & 7))) != 0;
}

static int verify_app_checkpoint_header(const CorpusGame *game,
                                        const uint8_t *snapshot,
                                        uint32_t expected_ticks,
                                        size_t expected_input_events)
{
    uint32_t elapsed = get_u32(
        snapshot + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET);
    if (snapshot[0] != 'X' || snapshot[1] != '1' || snapshot[2] != '6' ||
        snapshot[3] != 'R' ||
        snapshot[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION || snapshot[5] != 1 ||
        get_u32(snapshot + 6) != expected_ticks ||
        snapshot[38] != (uint8_t)expected_ticks ||
        get_u16(snapshot + 10) != expected_input_events ||
        get_u16(snapshot + 12) != expected_input_events || snapshot[14] ||
        !trace_window_is_honest(snapshot) ||
        elapsed > UINT16_MAX ||
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 10] != 2) {
        fprintf(stderr,
                "full-app checkpoint regression: %s tick=%u/%u input=%u/%lu missed=%u status=%02x capacity=%04x elapsed-vsync=%u var10=%u\n",
                game->id, (unsigned)get_u32(snapshot + 6),
                (unsigned)expected_ticks, (unsigned)get_u16(snapshot + 12),
                (unsigned long)expected_input_events,
                (unsigned)snapshot[14], (unsigned)snapshot[58],
                (unsigned)get_u16(snapshot +
                    LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET),
                (unsigned)elapsed,
                (unsigned)snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 10]);
        return -1;
    }
    return 0;
}

static void print_prefix_comparison(FILE *output, const char *game,
                                    uint32_t prefix,
                                    const LogicOracleResult *result)
{
    if (result->status == LOGIC_ORACLE_DIVERGED)
        fprintf(output, "full semantic divergence: %s first completed-cycle=%u",
                game, (unsigned)prefix);
    else if (result->status == LOGIC_ORACLE_INVALID)
        fprintf(output, "full semantic comparison invalid: %s cycle=%u status=%s",
                game, (unsigned)prefix,
                logic_oracle_status_name(result->status));
    else if (result->status == LOGIC_ORACLE_INCOMPATIBLE)
        fprintf(output, "full semantic comparison incompatible: %s cycle=%u status=%s",
                game, (unsigned)prefix,
                logic_oracle_status_name(result->status));
    else
        fprintf(output, "full semantic comparison failed: %s cycle=%u status=%s",
                game, (unsigned)prefix,
                logic_oracle_status_name(result->status));
    if (result->field[0]) fprintf(output, " field=%s", result->field);
    if (result->reference_text[0] || result->candidate_text[0])
        fprintf(output, " reference-text=%s candidate-text=%s",
                result->reference_text, result->candidate_text);
    else if (result->field[0])
        fprintf(output, " reference=%lld candidate=%lld",
                (long long)result->reference_value,
                (long long)result->candidate_value);
    if (result->detail[0]) fprintf(output, " detail=%s", result->detail);
    fprintf(output, "\n  ScummVM: %s\n  CX16: %s\n",
            result->reference_provenance, result->candidate_provenance);
}

static int capture_prefix_comparison(const LogicOracleResult *result,
                                     char output[], size_t output_size)
{
    FILE *stream = tmpfile();
    size_t length;
    int okay = 0;
    if (!stream) return -1;
    print_prefix_comparison(stream, "KQ1", 12, result);
    if (fflush(stream) == 0 && fseek(stream, 0, SEEK_SET) == 0) {
        length = fread(output, 1, output_size - 1, stream);
        if (!ferror(stream)) {
            output[length] = '\0';
            okay = 1;
        }
    }
    if (fclose(stream) != 0) okay = 0;
    return okay ? 0 : -1;
}

static int check_prefix_comparison_diagnostics(void)
{
    LogicOracleResult result = {0};
    char output[1024];
    result.status = LOGIC_ORACLE_INVALID;
    strcpy(result.field, "message[2:4]");
    strcpy(result.reference_text, "reference sentence");
    strcpy(result.candidate_text, "candidate sentence");
    strcpy(result.detail, "checkpoint identity mismatch");
    strcpy(result.reference_provenance, "ScummVM");
    strcpy(result.candidate_provenance, "CX16");
    if (capture_prefix_comparison(&result, output, sizeof output) != 0 ||
        !strstr(output, "full semantic comparison invalid") ||
        !strstr(output, "status=invalid") ||
        strstr(output, "first completed-cycle") ||
        !strstr(output, "reference-text=reference sentence") ||
        !strstr(output, "candidate-text=candidate sentence"))
        return -1;
    result.status = LOGIC_ORACLE_INCOMPATIBLE;
    if (capture_prefix_comparison(&result, output, sizeof output) != 0 ||
        !strstr(output, "full semantic comparison incompatible") ||
        !strstr(output, "status=incompatible") ||
        strstr(output, "first completed-cycle"))
        return -1;
    result.status = LOGIC_ORACLE_DIVERGED;
    if (capture_prefix_comparison(&result, output, sizeof output) != 0 ||
        !strstr(output, "full semantic divergence") ||
        !strstr(output, "first completed-cycle=12") ||
        !strstr(output, "reference-text=reference sentence") ||
        !strstr(output, "candidate-text=candidate sentence"))
        return -1;
    return 0;
}

static int app_prefix_snapshot_to_oracle(
    const char *game_id, uint16_t agi_version,
    const LogicReplayManifest *manifest,
    const uint8_t manifest_hash[LOGIC_REPLAY_SHA256_SIZE],
    const uint8_t input_hash[LOGIC_REPLAY_SHA256_SIZE],
    const uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE], uint32_t prefix,
    size_t event_count, uint16_t consumed, uint32_t pending_frame,
    LogicOracleSnapshot *snapshot, char *detail, size_t detail_size)
{
    (void)pending_frame;
    if (raw[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION || raw[5] != 1 ||
        get_u32(raw + 6) != prefix || raw[38] != (uint8_t)prefix ||
        get_u16(raw + 10) != event_count ||
        get_u16(raw + 12) != consumed || consumed > event_count || raw[14]) {
        if (detail && detail_size)
            snprintf(detail, detail_size,
                     "incomplete or missed-input semantic prefix");
        return -1;
    }
    return logic_app_replay_snapshot_to_oracle(
        game_id, agi_version, manifest, manifest_hash, input_hash, raw,
        LOGIC_APP_REPLAY_OUTPUT_SIZE, snapshot, detail, detail_size);
}

static int check_bounded_prefix_conversion(void)
{
    LogicReplayManifest manifest = {0};
    LogicOracleSnapshot snapshot;
    uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    uint8_t hash[LOGIC_REPLAY_SHA256_SIZE] = {0};
    char detail[128] = {0};
    strcpy(manifest.engine, "kq1");
    strcpy(manifest.rng_algorithm, LOGIC_APP_REPLAY_RNG_ALGORITHM);
    manifest.tick_numerator = 10;
    manifest.tick_denominator = 1;
    memcpy(raw, "X16R", 4);
    raw[4] = LOGIC_APP_REPLAY_SNAPSHOT_VERSION;
    raw[5] = raw[6] = raw[38] = raw[LOGIC_APP_REPLAY_VARS_OFFSET] = raw[15] = 1;
    raw[41] = LOGIC_APP_REPLAY_TRACE_MAX;
    raw[42] = 1;
    raw[58] = LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
              LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] =
        LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    if (app_prefix_snapshot_to_oracle(
            "kq1", 0x2917, &manifest, hash, hash, raw, 1, 0, 0, 180,
            &snapshot, detail, sizeof detail) != 0 ||
        snapshot.tick != 1 || !snapshot.control_flow_reliable || snapshot.has_trace) {
        fprintf(stderr,
                "real-game corpus regression: valid bounded trace prefix rejected: %s\n",
                detail);
        return -1;
    }
    return 0;
}

static int compare_app_prefixes_with_scummvm(
    const CorpusGame *game, const char *game_directory, uint32_t ticks,
    const LogicAppReplayInput *events, size_t event_count,
    uint32_t timeout_ms, const LogicReplayManifest *manifest,
    const uint8_t manifest_hash[LOGIC_REPLAY_SHA256_SIZE],
    const uint8_t input_hash[LOGIC_REPLAY_SHA256_SIZE])
{
    char reference_path[PATH_CAPACITY], scummvm_trace_path[PATH_CAPACITY];
    LogicOracleSnapshot *reference = NULL, candidate;
    uint8_t (*series)[LOGIC_APP_REPLAY_OUTPUT_SIZE] = NULL;
    LogicOracleResult result;
    uint8_t scummvm_input_hash[LOGIC_REPLAY_SHA256_SIZE];
    char conversion_detail[LOGIC_REPLAY_DETAIL_MAX] = {0};
    uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE];
    size_t reference_count = 0;
    uint16_t expected_version;
    uint32_t previous_elapsed_vsync = 0;
    int bounded_trace_seen = 0;
    int length, status = -1;
    length = snprintf(reference_path, sizeof reference_path,
                      "%s/reference.lors", game_directory);
    if (!events || !event_count || event_count > LOGIC_APP_REPLAY_INPUT_MAX ||
        length < 0 || (size_t)length >= sizeof reference_path ||
        run_scummvm_oracle(game_directory, ticks, reference_path) != 0 ||
        logic_oracle_snapshot_load(reference_path, &reference,
                                   &reference_count) != 0 ||
        reference_count != ticks ||
        (length = snprintf(scummvm_trace_path, sizeof scummvm_trace_path,
                           "%s/scummvm.ltrace", game_directory)) < 0 ||
        (size_t)length >= sizeof scummvm_trace_path ||
        logic_replay_sha256_file(scummvm_trace_path, scummvm_input_hash) != 0 ||
        parse_agi_version(game->agi_version, &expected_version) != 0) {
        fprintf(stderr, "real ScummVM comparison setup failed for %s: %s\n",
                game->id, conversion_detail[0] ? conversion_detail : "invalid reference capture");
        goto done;
    }
    {
        const LogicOracleSnapshot *last = &reference[reference_count - 1];
        if (last->agi_version != expected_version || last->tick != ticks ||
            last->timing_profile != LOGIC_ORACLE_TIMING_AGI_TICK ||
            last->tick_numerator != manifest->tick_numerator ||
            last->tick_denominator != manifest->tick_denominator ||
            memcmp(last->manifest_sha256, manifest_hash,
                   LOGIC_REPLAY_SHA256_SIZE) != 0 ||
            memcmp(last->input_sha256, scummvm_input_hash,
                   LOGIC_REPLAY_SHA256_SIZE) != 0 ||
            (strcmp(game->id, "KQ3") == 0 &&
             (!strstr(last->provenance, "det=0x2936") ||
              !strstr(last->provenance, "manifest-compat"))) ||
            (strcmp(game->id, "KQ1") == 0 &&
             (!strstr(last->provenance, "det=0x2917") ||
              strstr(last->provenance, "manifest-compat")))) {
            fprintf(stderr,
                    "ScummVM identity/version/boundary regression: %s version=%04x tick=%u profile=%u provenance=%s\n",
                    game->id, last->agi_version, last->tick,
                    last->timing_profile, last->provenance);
            goto done;
        }
    }
    /* The app trace records one semantic press. ScummVM's event API needs a
       same-tick down/up pair. After verifying that generated expansion, give
       both snapshots the canonical app schedule hash. */
    for (size_t i = 0; i < reference_count; ++i)
        memcpy(reference[i].input_sha256, input_hash,
               LOGIC_REPLAY_SHA256_SIZE);
    series = calloc(ticks, sizeof *series);
    if (!series || logic_app_replay_x16_series(game_directory,
            UINT16_C(0xA61B), events, event_count, ticks, timeout_ms, 0,
            series, ticks, conversion_detail, sizeof conversion_detail)) {
        fprintf(stderr, "application semantic series failed for %s: %s\n",
                game->id, conversion_detail);
        goto done;
    }
    for (uint32_t prefix = 1; prefix <= ticks; ++prefix) {
        uint32_t elapsed;
        uint16_t consumed;
        uint32_t pending_frame = 0;
        int run_status;
        memset(raw, 0, sizeof raw);
        memcpy(raw, series[prefix - 1], sizeof raw);
        run_status = 0;
        if (raw[0] == 'X' && raw[1] == '1' && raw[2] == '6' &&
            raw[3] == 'R' &&
            (raw[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED)) {
            fprintf(stderr,
                    "full semantic prefix incomplete cycle: %s attempted tick=%u after %u completed cycles, input=%u/%u frame=%u key=%02x polls=%u elapsed-vsync=%u detail=%s; candidate was not compared\n",
                    game->id, (unsigned)get_u32(raw + 6),
                    (unsigned)raw[38], (unsigned)get_u16(raw + 12),
                    (unsigned)get_u16(raw + 10),
                    (unsigned)get_u32(raw +
                        LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET),
                    (unsigned)raw[LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET],
                    (unsigned)get_u16(raw +
                        LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET),
                    (unsigned)get_u32(raw +
                        LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET),
                    conversion_detail[0] ? conversion_detail : "none");
            goto done;
        }
        elapsed = get_u32(raw + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET);
        consumed = get_u16(raw + 12);
        if (consumed < event_count)
            pending_frame = events[consumed].frame;
        if (run_status != 0 || raw[0] != 'X' || raw[1] != '1' ||
            raw[2] != '6' || raw[3] != 'R' ||
            app_prefix_snapshot_to_oracle(
                manifest->engine, expected_version, manifest, manifest_hash,
                input_hash, raw, prefix, event_count, consumed, pending_frame,
                &candidate, conversion_detail, sizeof conversion_detail) != 0) {
            fprintf(stderr,
                    "full semantic prefix regression: %s tick=%u completed=%u input=%u/%u pending-frame=%u elapsed-vsync=%u missed=%u status=%02x returned=%d detail=%s\n",
                    game->id, (unsigned)get_u32(raw + 6),
                    (unsigned)raw[38], (unsigned)consumed,
                    (unsigned)get_u16(raw + 10), (unsigned)pending_frame,
                    (unsigned)elapsed, (unsigned)raw[14],
                    (unsigned)raw[58], run_status,
                    conversion_detail[0] ? conversion_detail : "invalid completed checkpoint");
            if (raw[0] == 'X' && raw[1] == '1' &&
                raw[2] == '6' && raw[3] == 'R')
                print_full_app_checkpoint(game, raw);
            goto done;
        }
        /* Conversion owns exact trace-overwrite validation. */
        if (raw[58] & LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED)
            bounded_trace_seen = 1;
        if (reference[prefix - 1].tick != prefix) {
            fprintf(stderr,
                    "ScummVM prefix reference regression: %s index=%u tick=%u\n",
                    game->id, (unsigned)(prefix - 1),
                    (unsigned)reference[prefix - 1].tick);
            goto done;
        }
        if (prefix == 2 && elapsed <= previous_elapsed_vsync + 1u) {
            fprintf(stderr,
                    "full-app clock regression: %s completed tick 2 at VSYNC %u after tick 1 at VSYNC %u; a real six-frame cycle interval must advance the counter\n",
                    game->id, (unsigned)elapsed,
                    (unsigned)previous_elapsed_vsync);
            goto done;
        }
        previous_elapsed_vsync = elapsed;
        status = logic_oracle_compare_with_policy(
            &reference[prefix - 1], &candidate,
            LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC, &result);
        if (status != LOGIC_ORACLE_MATCH) {
            unsigned variable;
            print_prefix_comparison(stderr, game->id, prefix, &result);
            fprintf(stderr, "  target virtual-frame=%u speed=%u seconds=%u\n",
                    (unsigned)get_u32(raw + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET),
                    raw[LOGIC_APP_REPLAY_VARS_OFFSET + 10],
                    raw[LOGIC_APP_REPLAY_VARS_OFFSET + 11]);
            for (uint32_t t = 0; t < prefix; ++t) {
                if (t >= 4 && t + 8 < prefix) continue;
                fprintf(stderr, "  clock cycle=%u target-frame=%u speed=%u seconds=%u; %s\n",
                    t + 1, (unsigned)get_u32(series[t] + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET),
                    series[t][LOGIC_APP_REPLAY_VARS_OFFSET + 10],
                    series[t][LOGIC_APP_REPLAY_VARS_OFFSET + 11], reference[t].provenance);
            }
            fputs("  differing variables:", stderr);
            for (variable = 0; variable < 256; ++variable) {
                if (reference[prefix - 1].variables[variable] !=
                    candidate.variables[variable])
                    fprintf(stderr, " v%u=%u/%u", variable,
                            (unsigned)reference[prefix - 1].variables[variable],
                            (unsigned)candidate.variables[variable]);
            }
            fputc('\n', stderr);
            fprintf(stderr,
                    "  RNG reference=%s calls=%u state=%08x candidate=%s calls=%u state=%08x seed=%08x\n",
                    reference[prefix - 1].rng_algorithm,
                    (unsigned)reference[prefix - 1].rng_call_count,
                    (unsigned)reference[prefix - 1].rng_final_state,
                    candidate.rng_algorithm,
                    (unsigned)candidate.rng_call_count,
                    (unsigned)candidate.rng_final_state,
                    (unsigned)candidate.rng_seed);
            fprintf(stderr,
                    "  control variables v0=%u/%u v39=%u/%u v101=%u/%u\n",
                    (unsigned)reference[prefix - 1].variables[0],
                    (unsigned)candidate.variables[0],
                    (unsigned)reference[prefix - 1].variables[39],
                    (unsigned)candidate.variables[39],
                    (unsigned)reference[prefix - 1].variables[101],
                    (unsigned)candidate.variables[101]);
            for (unsigned object_id = 0; object_id < 256; ++object_id) {
                const LogicOracleObject *ref_object = NULL;
                const LogicOracleObject *app_object = NULL;
                for (unsigned object_index = 0;
                     object_index < reference[prefix - 1].object_count;
                     ++object_index)
                    if (reference[prefix - 1].objects[object_index].id == object_id)
                        ref_object = &reference[prefix - 1].objects[object_index];
                for (unsigned object_index = 0;
                     object_index < candidate.object_count; ++object_index)
                    if (candidate.objects[object_index].id == object_id)
                        app_object = &candidate.objects[object_index];
                if (!!ref_object != !!app_object ||
                    (ref_object &&
                     (ref_object->x != app_object->x ||
                      ref_object->y != app_object->y ||
                      ref_object->view != app_object->view ||
                      ref_object->loop != app_object->loop ||
                      ref_object->cel != app_object->cel ||
                      ref_object->motion != app_object->motion)))
                    fprintf(stderr,
                            "  object[%u] ref=%s(%d,%d v%u l%u c%u m%u) app=%s(%d,%d v%u l%u c%u m%u)\n",
                            object_id, ref_object ? "present" : "absent",
                            ref_object ? ref_object->x : 0,
                            ref_object ? ref_object->y : 0,
                            ref_object ? ref_object->view : 0,
                            ref_object ? ref_object->loop : 0,
                            ref_object ? ref_object->cel : 0,
                            ref_object ? ref_object->motion : 0,
                            app_object ? "present" : "absent",
                            app_object ? app_object->x : 0,
                            app_object ? app_object->y : 0,
                            app_object ? app_object->view : 0,
                            app_object ? app_object->loop : 0,
                            app_object ? app_object->cel : 0,
                            app_object ? app_object->motion : 0);
            }
            for (unsigned object_index = 0;
                 object_index < LOGIC_APP_REPLAY_OBJECT_MAX; ++object_index) {
                const uint8_t *object = raw + LOGIC_APP_REPLAY_OBJECTS_OFFSET +
                    object_index * LOGIC_APP_REPLAY_OBJECT_RECORD_SIZE;
                if (object[1] || object[2] || object[7] || object[8] ||
                    object[9] || object[10] || object[11])
                    fprintf(stderr,
                            "  app object[%u] flags=%04x view=%u loop=%u cel=%u motion=%u cycle=%u noAdvance=%u param2=%u\n",
                            object[0],
                            (unsigned)object[1] | ((unsigned)object[2] << 8),
                            object[7], object[8], object[9], object[11],
                            object[12] & 0x7fu, object[12] >> 7,
                            object[13]);
            }
            print_full_app_checkpoint(game, raw);
            goto done;
        }
    }
    if (!bounded_trace_seen) {
        fprintf(stderr,
                "full semantic prefix regression: %s did not exercise bounded trace history\n",
                game->id);
        goto done;
    }
    printf("ScummVM semantic prefix comparison: %s matched completed cycles 1..%u across variables, flags, room, objects, inventory and messages; reference=%s",
           game->id, (unsigned)ticks,
           reference[reference_count - 1].provenance);
    if (conversion_detail[0])
        printf(" capture=%s", conversion_detail);
    putchar('\n');
    status = 0;
done:
    free(series);
    free(reference);
    return status;
}

/* All generated Logic0 bytes stay in this suite's staged game. Restore them
   before hash-verified gameplay, including when the emulator or assertion fails. */
static int run_logic_fixture(const char *name, const char *game_directory,
                              const uint8_t *code, size_t code_size,
                              uint32_t ticks, uint32_t timeout_ms,
                              const LogicAppReplayInput *events, size_t event_count,
                              uint8_t *snapshot, char *detail, size_t detail_size)
{
    static const char text[] = "Replay checkpoint";
    static const char key[] = "Avis Durgan";
    uint8_t replacement[256] = {0x12, 0x34}, saved[256];
    uint8_t *directory = NULL;
    size_t directory_size = 0, length = 7 + code_size + 5 + sizeof text, i, section;
    char path[PATH_CAPACITY];
    FILE *volume = NULL;
    long offset = 0;
    int dirty = 0, result = -1;
    memset(snapshot, 0, LOGIC_APP_REPLAY_OUTPUT_SIZE);
    if (!code_size || length > sizeof replacement ||
        snprintf(path, sizeof path, "%s/LOGDIR", game_directory) >= (int)sizeof path ||
        read_file(path, &directory, &directory_size) || directory_size < 3)
        goto done;
    offset = ((long)(directory[0] & 15) << 16) |
             ((long)directory[1] << 8) | directory[2];
    replacement[2] = directory[0] >> 4;
    replacement[3] = length - 5;
    replacement[5] = code_size;
    memcpy(replacement + 7, code, code_size);
    section = 7 + code_size;
    replacement[section] = 1;
    replacement[section + 1] = 4 + sizeof text;
    replacement[section + 3] = 4;
    for (i = 0; i < sizeof text; ++i)
        replacement[section + 5 + i] = text[i] ^ key[i % (sizeof key - 1)];
    if (snprintf(path, sizeof path, "%s/VOL.%u", game_directory, replacement[2]) >= (int)sizeof path ||
        !(volume = fopen(path, "r+b")) || fseek(volume, offset, SEEK_SET) ||
        fread(saved, 1, length, volume) != length || fseek(volume, offset, SEEK_SET))
        goto done;
    dirty = 1;
    if (fwrite(replacement, 1, length, volume) != length || fflush(volume)) goto done;
    result = run_app_checkpoint(name, game_directory, UINT16_C(0xA61B),
        events, event_count, ticks, timeout_ms, 0, snapshot, detail, detail_size);
done:
    if (dirty && (fseek(volume, offset, SEEK_SET) ||
        fwrite(saved, 1, length, volume) != length || fflush(volume)))
        result = -1;
    if (volume && fclose(volume)) result = -1;
    free(directory);
    return result;
}

/* The parser/said workload uses production dispatch. Gate the release image
   so optional write tracing cannot mask changes to emitted release code. */
static int verify_parser_footprint(const char *program)
{
    char path[PATH_CAPACITY], line[256], segment[64];
    unsigned size, total = 0, found = 0;
    int module = 0;
    if (snprintf(path, sizeof path, "%s.map", program) >= (int)sizeof path)
        return -1;
    FILE *map = fopen(path, "r");
    if (!map) return -1;
    while (fgets(line, sizeof line, map)) {
        if (line[0] != ' ' && line[0] != '\t') {
            module = !strcmp(line, "commands.o:\n") ? 1 :
                     !strcmp(line, "parser.o:\n") ? 2 : 0;
        } else if (module && sscanf(line, " %63s Offs=%*x Size=%x", segment, &size) == 2 &&
                   ((module == 1 && !strcmp(segment, "BANKRAM0F")) ||
                    (module == 2 && !strcmp(segment, "BANKRAM07")))) {
            total += size;
            found |= (unsigned)module;
        }
    }
    int failed = ferror(map);
    if (fclose(map)) failed = 1;
    if (failed || found != 3 || total > 6879) {
        fprintf(stderr, "release parser footprint regression: %u bytes, budget6879; modules=%u\n",
                total, found);
        return -1;
    }
    printf("release parser/conditional C bank contribution: %u bytes (budget6879)\n", total);
    return 0;
}

static int verify_production_commands(const char *game_directory,
                                        uint32_t timeout_ms)
{
    static const uint8_t code[] = {
        3, 250, 42, 4, 249, 250, /* differently sized writes, exact opcode PCs */
        12, 200, 73, 0, 200, /* set(f200); end.of.loop(0, f200) */
        12, 201, 75, 1, 201, /* set(f201); reverse.loop(1, f201) */
        12, 255, /* last flag crosses the flag table's low-byte boundary */
        255, 7, 255, 255, 3, 0, 3, 248, 1, /* if (isset(255)) v248=1 */
        3, 247, 255,
        255, 8, 247, 255, 3, 0, 3, 246, 1, /* if (isset.v(v247)) v246=1 */
        3, 244, 254,
        255, 8, 244, 255, 3, 0, 3, 245, 1, /* unset f254 must stay false */
        37, 0, 95, 100, 39, 0, 249, 244, /* position; get.posn outputs */
        0
    };
    uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE];
    char detail[256] = {0};
    int result = -1, writes_seen = 0, c_flags_seen = 0, c_vars_seen = 0;
    /* This case also verifies that tracing retains ordinary writes rather
       than a hand-picked variable whitelist. */
    if (run_logic_fixture("command-state-and-trace", game_directory,
            code, sizeof code, 1, timeout_ms, NULL, 0,
            snapshot, detail, sizeof detail) == 0 &&
        snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 200] == 0 &&
        snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 201] == 0 &&
        snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 255] == 1 &&
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 248] == 1 &&
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 246] == 1 &&
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 245] == 0 &&
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 249] == 95 &&
        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 244] == 100)
        result = 0;
    else
        fprintf(stderr, "production loop completion regression: f200=%u f201=%u f255=%u isset=%u isset.v=%u unset.v=%u detail=%s\n",
                snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 200],
                snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 201],
                snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 255],
                snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 248],
                snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 246],
                snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 245], detail);
    if (!result) {
        unsigned i;
        for (i = 0; i < snapshot[41]; ++i) {
            const uint8_t *event = snapshot + LOGIC_APP_REPLAY_TRACE_OFFSET +
                i * LOGIC_APP_REPLAY_TRACE_RECORD_SIZE;
            if (event[8] == LOGIC_TRACE_KIND_WRITE && event[9] == 0 &&
                get_u32(event + 4) == 1 &&
                ((event[12] == 3 && event[15] == 250 && get_u16(event + 10) == 0) ||
                 (event[12] == 4 && event[15] == 249 && get_u16(event + 10) == 3)))
                ++writes_seen;
            if (event[8] == LOGIC_TRACE_KIND_WRITE && event[9] == 0 &&
                get_u32(event + 4) == 1) {
                if (event[24] == 2 && get_u32(event + 16) == 1 &&
                    get_u32(event + 20) == 0 &&
                    ((event[12] == 73 && event[15] == 200 && get_u16(event + 10) == 8) ||
                     (event[12] == 75 && event[15] == 201 && get_u16(event + 10) == 13)))
                    ++c_flags_seen;
                if (event[24] == 1 && event[12] == 39 && get_u16(event + 10) == 55 &&
                    ((event[15] == 249 && get_u32(event + 16) == 42 && get_u32(event + 20) == 95) ||
                     (event[15] == 244 && get_u32(event + 16) == 254 && get_u32(event + 20) == 100)))
                    ++c_vars_seen;
            }
        }
        if (writes_seen != 2) {
            fprintf(stderr, "production write trace regression: exact opcode records=%d expected2\n", writes_seen);
            result = -1;
        }
        if (c_flags_seen != 2 || c_vars_seen != 2) {
            fprintf(stderr, "production C write trace regression: flags=%d/2 variables=%d/2\n",
                    c_flags_seen, c_vars_seen);
            result = -1;
        }
    }
    if (!result) {
        static const uint8_t wander[] = {
            255, 1, 251, 0, 255, 18, 0,
            30, 0, 33, 0, 41, 0, 0, 37, 0, 100, 100,
            35, 0, 84, 0, 3, 251, 1, 0
        };
        if (run_logic_fixture("wander-zero-counter", game_directory,
                wander, sizeof wander, 2, timeout_ms, NULL, 0, snapshot,
                detail, sizeof detail) || get_u32(snapshot + 60) != 1 ||
            get_u32(snapshot + LOGIC_APP_REPLAY_RNG_STATE_OFFSET) != UINT32_C(0xc3d8de0a) ||
            get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 3) != 100 ||
            get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 5) != 100) {
            fprintf(stderr, "production wander regression: draws=%u state=%08x ego=(%u,%u) detail=%s\n",
                    (unsigned)get_u32(snapshot + 60),
                    (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_RNG_STATE_OFFSET),
                    get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 3),
                    get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 5), detail);
            result = -1;
        }
    }
    {
        uint8_t bottom_edge[] = {
            255, 1, 251, 0, 255, 21, 0,
            30, 0, 33, 0, 41, 0, 0, 37, 0, 95, 167,
            /* Fixed priority 15 isolates borders from picture control lines. */
            54, 0, 15, 35, 0, 78, 0, 3, 251, 1, 3, 6, 5, 0
        };
        for (unsigned crossing = 0; crossing < 2; ++crossing) {
            bottom_edge[17] = crossing ? 167 : 166;
            if (run_logic_fixture(crossing ? "ego-bottom-crossing" : "ego-bottom-inside",
                    game_directory, bottom_edge, sizeof bottom_edge, 2, timeout_ms,
                    NULL, 0, snapshot, detail, sizeof detail) ||
                get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 5) != 167 ||
                snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 2] != (crossing ? 3 : 0)) {
                fprintf(stderr, "production bottom-edge regression: crossing=%u y=%u edge=%u dir=%u v6=%u expected167/%u; %s\n",
                        crossing, get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 5),
                        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 2],
                        snapshot[LOGIC_APP_REPLAY_OBJECTS_OFFSET + 10],
                        snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 6], crossing ? 3 : 0, detail);
                result = -1;
            }
        }
    }
    {
        static const uint8_t timed_print[] = {3, 21, 2, 101, 1, 0};
        if (run_logic_fixture("timed-print-clock", game_directory,
                timed_print, sizeof timed_print, 1, timeout_ms, NULL, 0, snapshot,
                detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 21] != 0 ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 11] != 1 ||
            get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET) != 61) {
            fprintf(stderr, "production timed-print regression: timer=%u seconds=%u frame=%u detail=%s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 21],
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 11],
                    (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET), detail);
            result = -1;
        }
    }
    {
        static const uint8_t timed_then_cycle[] = {
            255, 1, 251, 0, 255, 8, 0,
            3, 21, 2, 101, 1, 3, 251, 1, 0
        };
        if (run_logic_fixture("timed-print-next-cycle", game_directory,
                timed_then_cycle, sizeof timed_then_cycle, 2, timeout_ms,
                NULL, 0, snapshot, detail, sizeof detail) ||
            get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET) != 63) {
            fprintf(stderr, "production message cycle reset regression: frame=%u expected63 (1030 ms close, next 25 ms quantum); %s\n",
                    (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET), detail);
            result = -1;
        }
    }
    {
        uint8_t poll[101] = {0};
        for (unsigned i = 0; i < 20; ++i) {
            poll[i * 5] = 255;
            poll[i * 5 + 1] = 13; /* have.key */
            poll[i * 5 + 2] = 255;
        }
        if (run_logic_fixture("have-key-clock", game_directory,
                poll, sizeof poll, 1, timeout_ms, NULL, 0, snapshot, detail, sizeof detail) ||
            get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET) != 13) {
            fprintf(stderr, "production have.key clock regression: frame=%u expected13 (30 ms + 20 polls of 10 ms); %s\n",
                    (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET), detail);
            result = -1;
        }
    }
    {
        static const uint8_t modal_poll[] = {
            101, 1, 255, 13, 255, 3, 0, 254, 5, 0,
            1, 250, 254, 0xF3, 0xFF, 0
        };
        static const LogicAppReplayInput keys[] = {{7, 13}, {12, 13}};
        if (run_logic_fixture("modal-then-have-key", game_directory,
                modal_poll, sizeof modal_poll, 1, timeout_ms, keys, 2,
                snapshot, detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250] != 7) {
            fprintf(stderr, "production modal poll regression: empty polls=%u expected7; %s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250], detail);
            result = -1;
        }
    }
    {
        static const uint8_t said_or[] = {
            120, 255, 252,
            /* A skipped word ID contains the OR separator byte ($FC). */
            14, 2, 2, 0, 3, 0, 14, 1, 2, 0, 14, 2, 2, 0, 252, 0,
            252, 255, 3, 0, 3, 250, 1, 0
        };
        static const LogicAppReplayInput look[] = {
            {3, 'l'}, {3, 'o'}, {3, 'o'}, {3, 'k'}, {3, 13}
        };
        static const LogicAppReplayInput upper_look[] = {
            {3, 'L'}, {3, 'O'}, {3, 'O'}, {3, 'K'}, {3, 13}
        };
        if (run_logic_fixture("parser-said-or-uppercase", game_directory,
                said_or, sizeof said_or, 2, timeout_ms, upper_look, 5,
                snapshot, detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250] != 1) {
            fprintf(stderr, "production said OR uppercase regression: matched=%u expected1; %s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250], detail);
            result = -1;
        }
        if (run_logic_fixture("parser-said-or", game_directory,
                said_or, sizeof said_or, 2, timeout_ms, look, 5,
                snapshot, detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250] != 1) {
            fprintf(stderr, "production said OR regression: matched=%u expected1 inputs=%u words=%u first=%u; %s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250],
                    get_u16(snapshot + 12),
                    get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET),
                    get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET + 2), detail);
            result = -1;
        }
        static const uint8_t said_rest[] = {
            120, 255, 14, 2, 2, 0, 0x0F, 0x27, 255, 3, 0, 3, 250, 1,
            255, 7, 4, 255, 3, 0, 3, 251, 1, 0
        };
        if (run_logic_fixture("parser-said-rest", game_directory,
                said_rest, sizeof said_rest, 2, timeout_ms, upper_look, 5,
                snapshot, detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250] != 1 ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 251] != 1) {
            fprintf(stderr, "production said rest regression: matched=%u accepted=%u expected1/1; %s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250],
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 251], detail);
            result = -1;
        }
        static const uint8_t said_missing_word[] = {
            120, 255, 14, 3, 2, 0, 1, 0, 0x0F, 0x27, 255, 3, 0,
            3, 250, 1, 0
        };
        if (run_logic_fixture("parser-said-missing-word", game_directory,
                said_missing_word, sizeof said_missing_word, 2, timeout_ms,
                upper_look, 5, snapshot, detail, sizeof detail) ||
            snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250] != 0) {
            fprintf(stderr, "production said missing-word regression: matched=%u expected0; %s\n",
                    snapshot[LOGIC_APP_REPLAY_VARS_OFFSET + 250], detail);
            result = -1;
        }
    }
    return result;
}

static int verify_intro_checkpoint(const CorpusGame *game,
                                   const uint8_t *snapshot)
{
    uint16_t ego_x = get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 3);
    uint16_t ego_y = get_u16(snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET + 5);
    if (verify_app_checkpoint_header(game, snapshot, 1, 1) != 0 ||
        get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET) != 1) {
        fprintf(stderr, "intro clock regression: %s frame=%u expected1 after the initial 30 ms cycle\n",
                game->id, (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET));
        return -1;
    }
    if (!strcmp(game->id, "KQ1")) {
        if (snapshot[15] != 1 || snapshot[16] != 0 ||
            get_u16(snapshot + LOGIC_APP_REPLAY_VARS_OFFSET + 1) != 83 ||
            !loaded_logic(snapshot, 1) || loaded_logic(snapshot, 83) ||
            !snapshot_has_message(snapshot, "Press any key to continue.") ||
            !(snapshot[LOGIC_APP_REPLAY_OBJECTS_OFFSET + 1] & 1) ||
            ego_x < 108 || ego_x > 112 || ego_y < 98 || ego_y > 102) {
            fprintf(stderr,
                    "full-app intro-skip regression: KQ1 room=%u logic=%u previous=%u ego=(%u,%u) Logic83=%u\n",
                    (unsigned)snapshot[15], (unsigned)snapshot[16],
                    (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_VARS_OFFSET + 1),
                    (unsigned)ego_x, (unsigned)ego_y,
                    (unsigned)loaded_logic(snapshot, 83));
            return -1;
        }
    } else if (!strcmp(game->id, "KQ3")) {
        if (snapshot[15] != 7 || snapshot[16] != 0 ||
            get_u16(snapshot + LOGIC_APP_REPLAY_VARS_OFFSET + 1) != 34 ||
            (snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 42] & 1) ||
            !loaded_logic(snapshot, 7) ||
            !(snapshot[LOGIC_APP_REPLAY_OBJECTS_OFFSET + 1] & 1) ||
            ego_x < 93 || ego_x > 97 || ego_y < 163 || ego_y > 167) {
            fprintf(stderr,
                    "full-app intro-skip regression: KQ3 room=%u logic=%u previous=%u flag42=%u ego=(%u,%u) Logic7=%u\n",
                    (unsigned)snapshot[15], (unsigned)snapshot[16],
                    (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_VARS_OFFSET + 1),
                    (unsigned)(snapshot[LOGIC_APP_REPLAY_FLAGS_OFFSET + 42] & 1),
                    (unsigned)ego_x, (unsigned)ego_y,
                    (unsigned)loaded_logic(snapshot, 7));
            return -1;
        }
    } else {
        return -1;
    }
    return 0;
}

static int verify_parser_checkpoint(const CorpusGame *game,
                                    const uint8_t *snapshot,
                                    uint32_t ticks, size_t input_events,
                                    uint8_t room, uint8_t logic,
                                    const char *response)
{
    if (verify_app_checkpoint_header(game, snapshot, ticks, input_events) != 0 ||
        snapshot[15] != room || !loaded_logic(snapshot, logic) ||
        !snapshot_has_message(snapshot, response)) {
        fprintf(stderr,
                "full-app parser checkpoint regression: %s room=%u logic-%u=%u response=%u parsed=%u messages=%u\n",
                game->id, (unsigned)snapshot[15], (unsigned)logic,
                (unsigned)loaded_logic(snapshot, logic),
                (unsigned)snapshot_has_message(snapshot, response),
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET),
                (unsigned)snapshot[46]);
        return -1;
    }
    return 0;
}

static int verify_full_app_boot(const CorpusGame *game,
                                const uint8_t *snapshot,
                                uint32_t expected_ticks,
                                size_t expected_input_events,
                                uint16_t expected_inventory,
                                uint8_t expected_room,
                                uint16_t min_ego_x, uint16_t max_ego_x,
                                uint16_t min_ego_y, uint16_t max_ego_y,
                                uint8_t expected_logic,
                                const char *expected_message,
                                const char *expected_transition_message)
{
    static const char *const names[4] = {
        "LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR"
    };
    unsigned i;
    uint8_t logic_zero_loaded =
        (uint8_t)((snapshot[LOGIC_APP_REPLAY_LOADED_LOGICS_OFFSET] & 1) != 0);
    if (verify_app_checkpoint_header(game, snapshot, expected_ticks,
                                     expected_input_events) != 0)
        return -1;
    if (expected_ticks == 0 && snapshot[38] != 0) {
        fprintf(stderr, "full-app run regression: %s clean boot executed an unexpected interpreter cycle\n",
                game->id);
        return -1;
    }
    if (!logic_zero_loaded || get_u16(snapshot + 39) == 0) {
        fprintf(stderr, "full-app run regression: %s did not load real Logic 0 through b6InitLogics\n",
                game->id);
        return -1;
    }
    if (expected_ticks && get_u16(snapshot + 39) < 2) {
        fprintf(stderr,
                "full-app run regression: %s interpreter did not load a real nonzero logic during gameplay cycles\n",
                game->id);
        return -1;
    }
    for (i = 0; i < 4; ++i) {
        uint16_t actual = get_u16(snapshot +
            LOGIC_APP_REPLAY_DIRECTORY_COUNTS_OFFSET + i * 2);
        if (actual != game->directory_counts[i]) {
            fprintf(stderr,
                    "full-app run regression: %s normal loader reports %s count %u for %lu real entries\n",
                    game->id, names[i], (unsigned)actual,
                    (unsigned long)game->directory_counts[i]);
            return -1;
        }
    }
    if (get_u16(snapshot + LOGIC_APP_REPLAY_LOGIC0_SIZE_OFFSET) !=
            game->logic_bytes[0] ||
        get_u16(snapshot + 51) != expected_inventory ||
        get_u16(snapshot + LOGIC_APP_REPLAY_WORD_COUNT_OFFSET) < 100) {
        fprintf(stderr,
                "full-app run regression: %s Logic0=%u/%u inventory=%u/%u words=%u\n",
                game->id,
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_LOGIC0_SIZE_OFFSET),
                (unsigned)game->logic_bytes[0],
                (unsigned)get_u16(snapshot + 51),
                (unsigned)expected_inventory,
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_WORD_COUNT_OFFSET));
        return -1;
    }
    if (expected_input_events &&
        (get_u16(snapshot + 12) != expected_input_events || snapshot[14])) {
        fprintf(stderr,
                "full-app input regression: %s consumed %u/%lu events, missed=%u, parsed words=%u\n",
                game->id, (unsigned)get_u16(snapshot + 12),
                (unsigned long)expected_input_events, (unsigned)snapshot[14],
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET));
        return -1;
    }
    {
        const uint8_t *ego = snapshot + LOGIC_APP_REPLAY_OBJECTS_OFFSET;
        uint16_t ego_x = get_u16(ego + 3);
        uint16_t ego_y = get_u16(ego + 5);
        if (snapshot[15] != expected_room || !(ego[1] & 0x01u) ||
            ego_x < min_ego_x || ego_x > max_ego_x ||
            ego_y < min_ego_y || ego_y > max_ego_y ||
            !loaded_logic(snapshot, expected_logic)) {
            fprintf(stderr,
                    "full-app gameplay regression: %s room=%u ego=(%u,%u) flags=%02x logic-%u-loaded=%u; expected room=%u ego=(%u..%u,%u..%u) and real logic milestone\n",
                    game->id, (unsigned)snapshot[15], (unsigned)ego_x,
                    (unsigned)ego_y, (unsigned)ego[1],
                    (unsigned)expected_logic,
                    (unsigned)loaded_logic(snapshot, expected_logic),
                    (unsigned)expected_room, (unsigned)min_ego_x,
                    (unsigned)max_ego_x, (unsigned)min_ego_y,
                    (unsigned)max_ego_y);
            return -1;
        }
    }
    /* The live parser buffer may be cleared by later blank input or a room
       transition. The title-specific response message is the durable proof
       that normal parser dispatch accepted the command. */
    if (!snapshot[46] || !expected_message ||
        !snapshot_has_message(snapshot, expected_message) ||
        (expected_transition_message &&
         !snapshot_has_message(snapshot, expected_transition_message))) {
        fprintf(stderr,
                "full-app interaction regression: %s parser words=%u messages=%u response=%u transition=%u\n",
                game->id,
                (unsigned)get_u16(snapshot + LOGIC_APP_REPLAY_PARSER_OFFSET),
                (unsigned)snapshot[46],
                (unsigned)(expected_message &&
                    snapshot_has_message(snapshot, expected_message)),
                (unsigned)(!expected_transition_message ||
                    snapshot_has_message(snapshot,
                                         expected_transition_message)));
        return -1;
    }
    {
        uint8_t status = snapshot[58];
        uint16_t capacity =
            get_u16(snapshot + LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET);
        uint32_t dropped = get_u32(snapshot + 42);
        uint8_t unexpected_status = status & (uint8_t)~(
            LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
            LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY);
        uint16_t unexpected_capacity = capacity & (uint16_t)~
            LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
        if (!snapshot[41] || snapshot[21] || snapshot[22] ||
            unexpected_status || unexpected_capacity ||
            (!!(status & LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED) != !!dropped) ||
            (!!(capacity & LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED) != !!dropped) ||
            (!!(status & LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY) != !!capacity)) {
        fprintf(stderr,
                "full-app replay regression: %s logic error=%u verifier=%u status=%u trace=%u dropped=%u capacity=%04x\n",
                game->id, snapshot[21], snapshot[22], status,
                snapshot[41], (unsigned)dropped, (unsigned)capacity);
            return -1;
        }
    }
    return 0;
}

static int verify_input_wait_failure(const CorpusGame *game,
                                     const char *game_directory,
                                     const char *application_build,
                                     const LogicAppReplayInput *events,
                                     size_t event_count,
                                     uint32_t tick_count,
                                     uint32_t timeout_ms)
{
    uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    char detail[256] = {0};
    int result = run_app_checkpoint("missing-input-watchdog", game_directory,
        UINT16_C(0xA61B), events, event_count, tick_count, timeout_ms, 0,
        snapshot, detail, sizeof detail);
    if (result == 0 || snapshot[0] != 'X' || snapshot[1] != '1' ||
        snapshot[2] != '6' || snapshot[3] != 'R' ||
        snapshot[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION || snapshot[5] != 1 ||
        !(snapshot[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED) ||
        get_u16(snapshot + 10) != event_count ||
        get_u16(snapshot + 12) != event_count ||
        get_u16(snapshot + LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET) <
            LOGIC_APP_REPLAY_INPUT_WAIT_LIMIT ||
        get_u32(snapshot + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET) > UINT16_MAX ||
        !strstr(detail, "input_wait_exhausted")) {
        fprintf(stderr,
                "full-app wait diagnostic regression: %s returned=%d detail=%s status=%02x events=%u/%u wait-polls=%u\n",
                game->id, result, detail, (unsigned)snapshot[58],
                (unsigned)get_u16(snapshot + 12),
                (unsigned)get_u16(snapshot + 10),
                (unsigned)get_u16(snapshot +
                    LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET));
        if (snapshot[0] == 'X' && snapshot[1] == '1' &&
            snapshot[2] == '6' && snapshot[3] == 'R')
            print_full_app_checkpoint(game, snapshot);
        return -1;
    }
    printf("full-app missing-input diagnostic: %s stopped at tick=%u after %u/%u delivered events, last frame=%u key=%02x, elapsed-vsync=%u polls=%u; rejected as input_wait_exhausted\n",
           game->id, (unsigned)get_u32(snapshot + 6),
           (unsigned)get_u16(snapshot + 12),
           (unsigned)get_u16(snapshot + 10),
           (unsigned)get_u32(snapshot +
               LOGIC_APP_REPLAY_LAST_INPUT_FRAME_OFFSET),
           (unsigned)snapshot[LOGIC_APP_REPLAY_LAST_INPUT_KEY_OFFSET],
           (unsigned)get_u32(snapshot + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET),
           (unsigned)get_u16(snapshot +
                         LOGIC_APP_REPLAY_INPUT_WAIT_POLLS_OFFSET));
    if (getenv("CX16_APP_REPLAY_DIAGNOSTICS"))
        print_full_app_checkpoint(game, snapshot);
    {
        LogicReplayManifest manifest;
        LogicReplayEvent trace[LOGIC_APP_REPLAY_INPUT_MAX];
        uint8_t manifest_hash[LOGIC_REPLAY_SHA256_SIZE], input_hash[LOGIC_REPLAY_SHA256_SIZE];
        uint8_t ignored[LOGIC_APP_REPLAY_OUTPUT_SIZE], *saved = NULL;
        size_t size = 0;
        char path[PATH_CAPACITY];
        if (snprintf(path, sizeof path, "%s/full-app-failure.x16r", game_directory) >=
                (int)sizeof path ||
            create_app_replay_identity(game, game_directory, application_build,
                UINT16_C(0xA61B), events, event_count, &manifest, trace,
                manifest_hash, input_hash)) return -1;
        int status = run_full_app_cli(game_directory, tick_count, timeout_ms, path, ignored);
        int valid = status != 0 && !read_file(path, &saved, &size) &&
            size == sizeof snapshot && !memcmp(saved, "X16R", 4) &&
            saved[4] == LOGIC_APP_REPLAY_SNAPSHOT_VERSION && saved[5] == 1 &&
            (saved[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED) &&
            get_u16(saved + 12) == event_count && saved[41] > 0;
        free(saved);
        if (!valid) {
            fprintf(stderr, "full-app CLI failure artifact regression: no complete input-exhaustion checkpoint retained\n");
            return -1;
        }
        char *arguments[] = {"obj/cx16/logic-trace", "recent", path, "--count", "2", NULL};
        if (run_process(arguments)) return -1;
        puts("full-app CLI failure checkpoint retained and recent trace decoded");
    }
    return 0;
}

static int verify_future_input_prefix(const CorpusGame *game,
                                      const char *game_directory,
                                      const LogicAppReplayInput *events,
                                      size_t event_count,
                                      uint32_t timeout_ms)
{
    uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    char detail[256] = {0};
    LogicAppReplayInput probe_event;
    uint32_t elapsed;
    uint16_t consumed;
    uint32_t pending_frame = 0;
    int result;
    if (!events || !event_count || event_count > LOGIC_APP_REPLAY_INPUT_MAX)
        return -1;
    /* Rendering must not consume the virtual input deadline: the first
       interpreter cycle completes before the scheduled frame. */
    probe_event.frame = 12;
    probe_event.petscii = events[0].petscii;
    result = run_app_checkpoint("future-input-prefix", game_directory,
        UINT16_C(0xA61B), &probe_event, 1, 1, timeout_ms, 0,
        snapshot, detail, sizeof detail);
    elapsed = get_u32(snapshot + LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET);
    consumed = get_u16(snapshot + 12);
    if (consumed < 1)
        pending_frame = probe_event.frame;
    if (result != 0 || snapshot[0] != 'X' || snapshot[1] != '1' ||
        snapshot[2] != '6' || snapshot[3] != 'R' ||
        snapshot[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION || snapshot[5] != 1 ||
        get_u32(snapshot + 6) != 1 || snapshot[38] != 1 ||
        get_u16(snapshot + 10) != 1 || consumed != 0 || pending_frame != 12 ||
        snapshot[14] || !trace_window_is_honest(snapshot) ||
        get_u32(snapshot + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET) >= pending_frame) {
        fprintf(stderr,
                "full-app future-input prefix regression: %s tick=%u cycles=%u input=%u/%u pending-frame=%u elapsed-vsync=%u missed=%u status=%02x returned=%d detail=%s\n",
                game->id, (unsigned)get_u32(snapshot + 6),
                (unsigned)snapshot[38], (unsigned)get_u16(snapshot + 12),
                (unsigned)get_u16(snapshot + 10), (unsigned)pending_frame,
                (unsigned)elapsed, (unsigned)snapshot[14],
                (unsigned)snapshot[58], result,
                detail[0] ? detail : "none");
        if (snapshot[0] == 'X' && snapshot[1] == '1' &&
            snapshot[2] == '6' && snapshot[3] == 'R')
            print_full_app_checkpoint(game, snapshot);
        return -1;
    }
    printf("full-app future-input clock origin: %s completed tick 1 at VSYNC %u with probe frame %u still pending\n",
           game->id, (unsigned)elapsed, (unsigned)pending_frame);
    return 0;
}

typedef struct FullAppFaultOracleContext {
    const char *game_directory;
    uint16_t seed;
    uint32_t tick_count;
} FullAppFaultOracleContext;

static int full_app_fault_oracle(void *opaque,
                                 const LogicReplayManifest *manifest,
                                 const LogicReplayEvent *events, size_t count,
                                 uint32_t timeout_ms,
                                 LogicFailureSignature *signature)
{
    FullAppFaultOracleContext *context = (FullAppFaultOracleContext *)opaque;
    LogicAppReplayInput input[LOGIC_APP_REPLAY_INPUT_MAX];
    uint8_t snapshot[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    char detail[256] = {0};
    size_t i;
    int result;
    if (!context || !manifest || !signature || count > LOGIC_APP_REPLAY_INPUT_MAX ||
        logic_replay_manifest_validate(manifest) != 0)
        return -1;
    for (i = 0; i < count; ++i) {
        if (events[i].kind != LOGIC_TRACE_KIND_MARK ||
            (events[i].reason != LOGIC_REPLAY_REASON_INPUT_DOWN &&
             events[i].reason != LOGIC_REPLAY_REASON_SEMANTIC_KEYPRESS) ||
            events[i].logic != 0 || !events[i].tick ||
            events[i].tick > UINT16_MAX / 6u)
            return -1;
        input[i].frame = events[i].tick * 6u;
        input[i].petscii = events[i].field;
    }
    result = logic_app_replay_x16_run(
        context->game_directory, context->seed, input, count,
        context->tick_count, timeout_ms,
        LOGIC_APP_REPLAY_FAULT_OMIT_METADATA_SAVE,
        snapshot, detail, sizeof detail);
    if (snapshot[0] != 'X' || snapshot[1] != '1' ||
        snapshot[2] != '6' || snapshot[3] != 'R' ||
        snapshot[4] != LOGIC_APP_REPLAY_SNAPSHOT_VERSION)
        return -1;
    if ((snapshot[58] & (LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED |
                         LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED)) ==
        (LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED |
         LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED)) {
        memset(signature, 0, sizeof *signature);
        signature->kind = LOGIC_TRACE_KIND_MARK;
        signature->logic = snapshot[25];
        signature->logical_pc = get_u16(snapshot + 27);
        signature->opcode = snapshot[26];
        signature->reason = LOGIC_REPLAY_REASON_CONTEXT_MISMATCH;
        signature->code = ((uint32_t)snapshot[23] << 8) | snapshot[24];
        return 0;
    }
    if (result != 0 &&
        (snapshot[58] & LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED))
        return 1;
    if (result != 0 && !snapshot[5])
        return -1;
    return 1;
}

static int same_failure_signature(const LogicFailureSignature *left,
                                 const LogicFailureSignature *right)
{
    return left->kind == right->kind && left->logic == right->logic &&
           left->logical_pc == right->logical_pc &&
           left->opcode == right->opcode && left->reason == right->reason &&
           left->code == right->code;
}

static int verify_full_app_fault_reducer(
    const CorpusGame *game, const char *game_directory,
    const char *application_build, const LogicAppReplayInput *input,
    size_t input_count, uint32_t tick_count, uint32_t timeout_ms)
{
    const uint16_t seed = UINT16_C(0xA61B);
    LogicReplayManifest manifest;
    LogicReplayEvent trace[LOGIC_APP_REPLAY_INPUT_MAX];
    LogicReplayEvent *minimized = NULL;
    LogicReplayEvent without_trigger[LOGIC_APP_REPLAY_INPUT_MAX];
    size_t minimized_count = 0, without_trigger_count = 0, i;
    LogicFailureSignature failure, original_repeat, minimized_repeat;
    FullAppFaultOracleContext context;
    uint8_t manifest_hash[LOGIC_REPLAY_SHA256_SIZE];
    uint8_t input_hash[LOGIC_REPLAY_SHA256_SIZE];
    char reduced_path[PATH_CAPACITY];
    int length;
    int status;
    if (create_app_replay_identity(game, game_directory, application_build,
            seed, input, input_count, &manifest, trace, manifest_hash,
            input_hash) != 0)
        return -1;
    if (logic_replay_manifest_verify_resources(&manifest, game_directory) != 0)
        return -1;
    context.game_directory = game_directory;
    context.seed = seed;
    context.tick_count = tick_count;
    length = snprintf(reduced_path, sizeof reduced_path,
                      "%s/reduced.ltrace", game_directory);
    if (length < 0 || (size_t)length >= sizeof reduced_path)
        return -1;
    status = run_full_app_reducer_cli(game_directory, tick_count, timeout_ms,
                                      reduced_path, 0);
    if (status != 0 ||
        logic_replay_trace_read(reduced_path, &minimized,
                                &minimized_count) != 0 ||
        !minimized || !minimized_count || minimized_count >= input_count) {
        fprintf(stderr,
                "full-app public reducer regression: %s status=%d original=%lu minimized=%lu\n",
                game->id, status, (unsigned long)input_count,
                (unsigned long)minimized_count);
        free(minimized);
        return -1;
    }
    if (full_app_fault_oracle(&context, &manifest, trace, input_count,
            timeout_ms, &failure) != 0) {
        fprintf(stderr, "full-app reducer regression: %s cannot reproduce original signature\n",
                game->id);
        free(minimized);
        return -1;
    }
    status = full_app_fault_oracle(&context, &manifest, trace, input_count,
                                   timeout_ms, &original_repeat);
    if (status != 0 || !same_failure_signature(&failure, &original_repeat)) {
        fprintf(stderr, "full-app reducer regression: %s original trace did not repeat signature\n",
                game->id);
        free(minimized);
        return -1;
    }
    status = full_app_fault_oracle(&context, &manifest, minimized,
                                   minimized_count, timeout_ms,
                                   &minimized_repeat);
    if (status != 0 || !same_failure_signature(&failure, &minimized_repeat)) {
        fprintf(stderr, "full-app reducer regression: %s minimized trace did not repeat signature\n",
                game->id);
        free(minimized);
        return -1;
    }
    for (i = 0; i < minimized_count; ++i) {
        if (minimized[i].field == 'L' && minimized[i].tick == 1)
            continue;
        without_trigger[without_trigger_count++] = minimized[i];
    }
    if (without_trigger_count == minimized_count ||
        full_app_fault_oracle(&context, &manifest, without_trigger,
            without_trigger_count, timeout_ms, &minimized_repeat) != 1) {
        fprintf(stderr,
                "full-app reducer regression: %s removing the delivered L event retained the injected signature\n",
                game->id);
        free(minimized);
        return -1;
    }
    printf("full-app public reducer: %s input-triggered context failure signature kind=%u logic=%u pc=%04x opcode=%u field-offset=%08x; events %lu -> %lu; original/minimized signatures repeated and deleting frame6 L removed failure\n",
           game->id, (unsigned)failure.kind, (unsigned)failure.logic,
           (unsigned)failure.logical_pc, (unsigned)failure.opcode,
           (unsigned)failure.code, (unsigned long)input_count,
           (unsigned long)minimized_count);
    free(minimized);
    return 0;
}

static int verify_semantic_reducer(const CorpusGame *game, const char *directory,
    const char *build, uint32_t timeout_ms)
{
    static const LogicAppReplayInput inputs[] = {{6, 'L'}, {12, 'C'}, {18, 'H'}, {24, 'E'}};
    LogicReplayManifest manifest;
    LogicReplayEvent trace[4], *reduced = NULL;
    LogicOracleSnapshot final, *reference = NULL;
    LogicOracleResult comparison;
    uint8_t manifest_hash[32], input_hash[32];
    uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE];
    char path[PATH_CAPACITY], detail[256];
    size_t count = 0, reference_count = 0;
    int result = -1;
    if (create_app_replay_identity(game, directory, build, UINT16_C(0xA61B),
            inputs, 4, &manifest, trace, manifest_hash, input_hash) ||
        snprintf(path, sizeof path, "%s/reduced.ltrace", directory) >= (int)sizeof path)
        return -1;
    /* A transient wrong variable must be found even after reconvergence. */
    if (logic_app_replay_x16_run(directory, UINT16_C(0xA61B), inputs, 4, 4,
            timeout_ms, LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE, raw, detail, sizeof detail) ||
        !(raw[58] & LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED) ||
        (raw[58] & LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED) ||
        raw[LOGIC_APP_REPLAY_VARS_OFFSET + 255] != 0) {
        fprintf(stderr, "semantic reconvergence fixture did not complete: %s\n", detail);
        return -1;
    }
    /* Confirm complete semantic reconvergence, not just the repaired byte. */
    raw[58] &= (uint8_t)~LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED;
    if (write_scummvm_input_trace(directory, trace, 4) ||
        snprintf(path, sizeof path, "%s/reference.lors", directory) >= (int)sizeof path ||
        run_scummvm_oracle(directory, 4, path) ||
        logic_oracle_snapshot_load(path, &reference, &reference_count) || reference_count != 4 ||
        logic_app_replay_snapshot_to_oracle(manifest.engine, 0x2917, &manifest,
            manifest_hash, input_hash, raw, sizeof raw, &final, detail, sizeof detail))
        goto done;
    memcpy(reference[3].input_sha256, input_hash, sizeof input_hash);
    if (logic_oracle_compare_with_policy(&reference[3], &final,
            LOGIC_ORACLE_POLICY_CROSS_ENGINE_SEMANTIC, &comparison) != LOGIC_ORACLE_MATCH) {
        fprintf(stderr, "transient fault did not semantically reconverge: %s\n", comparison.detail);
        goto done;
    }
    if (snprintf(path, sizeof path, "%s/reduced.ltrace", directory) >= (int)sizeof path)
        goto done;
    if (run_full_app_reducer_cli(directory, 4, timeout_ms, path, 1) ||
        logic_replay_trace_read(path, &reduced, &count) || count != 1 ||
        reduced[0].tick != 1 || reduced[0].field != 'L') {
        fprintf(stderr, "semantic reducer did not retain only the transient corruption trigger\n");
        goto done;
    }
    if (logic_app_replay_x16_run(directory, UINT16_C(0xA61B), inputs + 1, 3, 4,
            timeout_ms, LOGIC_APP_REPLAY_FAULT_SEMANTIC_WRITE, raw, detail, sizeof detail) != -1 ||
        memcmp(raw, "X16R", 4) || get_u32(raw + 6) != 4 ||
        (raw[58] & (LOGIC_APP_REPLAY_STATUS_FAULT_TRIGGERED |
                    LOGIC_APP_REPLAY_STATUS_CONTEXT_VERIFY_FAILED)) ||
        raw[LOGIC_APP_REPLAY_VARS_OFFSET + 255] != 0)
        goto done;
    puts("full-app semantic reducer: KQ1 transient variable divergence reduced 4 -> 1; reconverged final state retained, deleting trigger removed corruption");
    result = 0;
done:
    free(reference);
    free(reduced);
    return result;
}

static int verify_checkpoint_series(const char *directory,
    const LogicAppReplayInput *events, size_t count, uint32_t timeout_ms)
{
    uint8_t series[4][LOGIC_APP_REPLAY_OUTPUT_SIZE];
    uint8_t cold[LOGIC_APP_REPLAY_OUTPUT_SIZE];
    char detail[256] = {0};
    uint64_t start = monotonic_milliseconds(), series_ms, cold_ms;
    if (logic_app_replay_x16_series(directory, UINT16_C(0xA61B), events, count,
            4, timeout_ms, 0, series, 4, detail, sizeof detail)) {
        fprintf(stderr, "application checkpoint series regression: %s\n", detail);
        return -1;
    }
    series_ms = monotonic_milliseconds() - start;
    start = monotonic_milliseconds();
    for (uint32_t tick = 1; tick <= 4; ++tick) {
        if (run_app_checkpoint("series-cold-parity", directory,
                UINT16_C(0xA61B), events, count, tick, timeout_ms, 0,
                cold, detail, sizeof detail) ||
            memcmp(cold, series[tick - 1], LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET) ||
            memcmp(cold + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET,
                   series[tick - 1] + LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET, 4)) {
            fprintf(stderr, "application checkpoint series differs from cold boot at cycle %u: %s\n",
                    tick, detail);
            return -1;
        }
    }
    cold_ms = monotonic_milliseconds() - start;
    printf("application checkpoint series: four completed cycles match four cold boots; one-boot wall-ms=%llu cold-boots wall-ms=%llu\n",
        (unsigned long long)series_ms, (unsigned long long)cold_ms);
    return 0;
}

