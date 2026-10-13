/* Exercise test_paths.h cleanup when an owned fixture contains subdirectories. */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_paths.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static int unlink_if_present(const char *path)
{
    if (!unlink(path) || errno == ENOENT) return 0;
    return -1;
}

static int rmdir_if_present(const char *path)
{
    if (!rmdir(path) || errno == ENOENT) return 0;
    return -1;
}

static int cleanup_child_fixture(const char *root)
{
    struct stat info;
    if (lstat(root, &info)) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(info.st_mode)) return -1;

    const char *parts[] = {
        "nested/deeper/marker", "nested/deeper/external-link",
        "nested/deeper", "nested", NULL
    };
    for (size_t i = 0; parts[i]; ++i) {
        char path[4096];
        if (snprintf(path, sizeof(path), "%s/%s", root, parts[i]) >= (int)sizeof(path) ||
            (i < 2 ? unlink_if_present(path) : rmdir_if_present(path))) return -1;
    }
    return rmdir_if_present(root);
}

static void cleanup_parent_fixture(void)
{
    if (!suite_fixture_directory[0]) return;
    DIR *directory = opendir(suite_fixture_directory);
    if (directory) {
        bool failed = false;
        struct dirent *entry;
        while ((entry = readdir(directory))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char path[4096];
            if (snprintf(path, sizeof(path), "%s/%s", suite_fixture_directory, entry->d_name) >= (int)sizeof(path)) {
                failed = true;
            } else if (!strcmp(entry->d_name, "sentinel")) {
                if (unlink_if_present(path)) failed = true;
            } else if (!strncmp(entry->d_name, "cx16agi-fixture-cleanup-child-", 30)) {
                if (cleanup_child_fixture(path)) failed = true;
            } else {
                fprintf(stderr, "Unexpected fixture cleanup regression artifact: %s\n", path);
                failed = true;
            }
        }
        if (closedir(directory)) failed = true;
        if (!failed && rmdir_if_present(suite_fixture_directory)) failed = true;
        if (failed) {
            fprintf(stderr, "Could not clean fixture cleanup regression root %s\n", suite_fixture_directory);
            _Exit(EXIT_FAILURE);
        }
        return;
    }
    if (errno != ENOENT) {
        fprintf(stderr, "Could not inspect fixture cleanup regression root %s\n", suite_fixture_directory);
        _Exit(EXIT_FAILURE);
    }
}

static void fixture_path(char *path, size_t capacity, const char *part)
{
    suite_test_path(path, capacity, suite_fixture_directory, part);
}

static int create_nested_child_fixture(void)
{
    suite_test_paths_init("fixture-cleanup-child");
    char nested[4096], deeper[4096], marker[4096];
    fixture_path(nested, sizeof(nested), "nested");
    fixture_path(deeper, sizeof(deeper), "nested/deeper");
    fixture_path(marker, sizeof(marker), "nested/deeper/marker");
    CHECK(!mkdir(nested, 0700), "Cannot create nested fixture directory");
    CHECK(!mkdir(deeper, 0700), "Cannot create deeper fixture directory");
    FILE *file = fopen(marker, "wb");
    CHECK(file != NULL, "Cannot create nested fixture marker");
    CHECK(fputs("owned fixture\n", file) >= 0, "Cannot write nested fixture marker");
    CHECK(!fclose(file), "Cannot close nested fixture marker");
    return EXIT_SUCCESS;
}

static bool child_fixture_is_clean(const char *sentinel)
{
    DIR *directory = opendir(suite_fixture_directory);
    if (!directory) return false;
    bool only_sentinel = true;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, "sentinel")) {
            only_sentinel = false;
            break;
        }
    }
    if (closedir(directory)) only_sentinel = false;

    FILE *file = fopen(sentinel, "rb");
    if (!file) return false;
    char contents[32] = { 0 };
    size_t length = fread(contents, 1, sizeof(contents), file);
    bool read_ok = !ferror(file);
    int close_status = fclose(file);
    bool intact = read_ok && !close_status && length == strlen("keep sentinel\n") &&
                  !memcmp(contents, "keep sentinel\n", length);
    return only_sentinel && intact;
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "--child")) {
        int result = create_nested_child_fixture();
        char link[4096];
        fixture_path(link, sizeof(link), "nested/deeper/external-link");
        CHECK(!symlink(argv[3], link), "Cannot create external sentinel symlink");
        if (!strcmp(argv[2], "failure")) exit(23);
        CHECK(!strcmp(argv[2], "success"), "Unknown fixture cleanup child mode");
        return result;
    }
    CHECK(argc == 1, "Unexpected fixture cleanup regression argument");

    suite_test_paths_init("fixture-cleanup-parent");
    CHECK(!atexit(cleanup_parent_fixture), "Cannot register fixture cleanup regression cleanup");
    CHECK(!setenv("TMPDIR", suite_fixture_directory, 1), "Cannot set child fixture root");

    char sentinel[4096];
    fixture_path(sentinel, sizeof(sentinel), "sentinel");
    FILE *file = fopen(sentinel, "wb");
    CHECK(file != NULL, "Cannot create external sentinel");
    CHECK(fputs("keep sentinel\n", file) >= 0, "Cannot write external sentinel");
    CHECK(!fclose(file), "Cannot close external sentinel");

    const char *modes[] = { "success", "failure" };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        pid_t child = fork();
        CHECK(child >= 0, "Cannot fork fixture cleanup regression child");
        if (!child) {
            execl(argv[0], argv[0], "--child", modes[i], sentinel, (char *)NULL);
            perror("Cannot start fixture cleanup regression child");
            _Exit(127);
        }

        int status;
        CHECK(waitpid(child, &status, 0) == child, "Cannot wait for fixture cleanup regression child");
        int expected = !strcmp(modes[i], "failure") ? 23 : EXIT_SUCCESS;
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == expected,
              "Owned fixture cleanup did not preserve the child exit status (see child cleanup diagnostic)");
        CHECK(child_fixture_is_clean(sentinel),
              "Nested fixture cleanup left files or followed a symlink outside its owned root");
    }

    puts("PASS nested fixture cleanup on success and failure without following symlinks");
    return EXIT_SUCCESS;
}
