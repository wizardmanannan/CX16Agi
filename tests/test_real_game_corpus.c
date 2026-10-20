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

