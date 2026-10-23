#ifndef CX16AGI_TEST_PATHS_H
#define CX16AGI_TEST_PATHS_H

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char suite_fixture_directory[4096];
static char suite_output_directory[4096];

static bool suite_test_paths_remove(const char *path) {
    struct stat info;
    if (lstat(path, &info)) return errno == ENOENT;
    if (!S_ISDIR(info.st_mode)) return !unlink(path) || errno == ENOENT;

    DIR *directory = opendir(path);
    if (!directory) return errno == ENOENT;
    bool failed = false;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            if (errno) failed = true;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[4096];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            !suite_test_paths_remove(child)) failed = true;
    }
    if (closedir(directory)) failed = true;
    if (rmdir(path) && errno != ENOENT) failed = true;
    return !failed;
}

static void suite_test_paths_cleanup(void) {
    if (!suite_fixture_directory[0]) return;
    if (!suite_test_paths_remove(suite_fixture_directory)) {
        fprintf(stderr, "Could not clean test fixture directory %s\n", suite_fixture_directory);
        _Exit(EXIT_FAILURE);
    }
    suite_fixture_directory[0] = '\0';
}

static void suite_test_paths_init(const char *prefix) {
    const char *fixture_root = getenv("TMPDIR");
    const char *output_root = getenv("TEST_OUTPUT_DIR");
    struct stat output_info;
    if (!fixture_root || !*fixture_root) fixture_root = "/tmp";
    if (!output_root || !*output_root || stat(output_root, &output_info) || !S_ISDIR(output_info.st_mode)) {
        fprintf(stderr, "TEST_OUTPUT_DIR must name the suite-owned output directory\n");
        exit(EXIT_FAILURE);
    }
    if (snprintf(suite_output_directory, sizeof(suite_output_directory), "%s", output_root) >= (int)sizeof(suite_output_directory)) {
        fprintf(stderr, "TEST_OUTPUT_DIR path is too long\n");
        exit(EXIT_FAILURE);
    }
    if (snprintf(suite_fixture_directory, sizeof(suite_fixture_directory), "%s/cx16agi-%s-XXXXXX", fixture_root, prefix) >= (int)sizeof(suite_fixture_directory)) {
        fprintf(stderr, "Test fixture path is too long\n");
        exit(EXIT_FAILURE);
    }
    if (!mkdtemp(suite_fixture_directory)) {
        perror("Cannot create test fixture directory");
        suite_fixture_directory[0] = '\0';
        exit(EXIT_FAILURE);
    }
    if (atexit(suite_test_paths_cleanup)) {
        suite_test_paths_cleanup();
        fprintf(stderr, "Cannot register test fixture cleanup\n");
        exit(EXIT_FAILURE);
    }
}

static void suite_test_path(char *destination, size_t capacity, const char *directory, const char *name) {
    if (snprintf(destination, capacity, "%s/%s", directory, name) >= (int)capacity) {
        fprintf(stderr, "Test path is too long: %s/%s\n", directory, name);
        exit(EXIT_FAILURE);
    }
}

#endif
