#define _POSIX_C_SOURCE 200809L

#include "logic_replay.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Natural failure reduction must accept a request without test fault
 * injection. A deliberately absent manifest lets us verify this through
 * the real CLI without starting an emulator or modifying game resources. */
int main(int argc, char **argv)
{
    char directory[] = "obj/cx16/app-reduce-request-XXXXXX";
    char log_path[256], manifest_path[256], trace_path[256], reduced_path[256];
    char resource_path[256], checkpoint_manifest_path[256];
    char checkpoint_trace_path[256], checkpoint_output_path[256];
    char output[4096];
    pid_t child;
    int fd = -1, status = 0, failed = 1, intentional = 0;
    ssize_t size;
    LogicReplayManifest checkpoint_manifest = {0};
    uint8_t resource_hash[LOGIC_REPLAY_SHA256_SIZE];
    FILE *resource_file = NULL;
    static const char resource_bytes[] = "replay checkpoint test resource";
    if (argc != 2 || !mkdtemp(directory))
        return 1;
    snprintf(log_path, sizeof log_path, "%s/result", directory);
    snprintf(manifest_path, sizeof manifest_path, "%s/absent.manifest", directory);
    snprintf(trace_path, sizeof trace_path, "%s/absent.trace", directory);
    snprintf(reduced_path, sizeof reduced_path, "%s/reduced.trace", directory);
    snprintf(resource_path, sizeof resource_path, "%s/resource.bin", directory);
    snprintf(checkpoint_manifest_path, sizeof checkpoint_manifest_path,
             "%s/checkpoint.manifest", directory);
    snprintf(checkpoint_trace_path, sizeof checkpoint_trace_path,
             "%s/input.trace", directory);
    snprintf(checkpoint_output_path, sizeof checkpoint_output_path,
             "%s/output.checkpoint", directory);
    fd = open(log_path, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd < 0)
        goto cleanup;
    resource_file = fopen(resource_path, "wb");
    if (!resource_file)
        goto cleanup;
    if (fwrite(resource_bytes, 1, sizeof resource_bytes - 1, resource_file) !=
        sizeof resource_bytes - 1) {
        fclose(resource_file);
        resource_file = NULL;
        goto cleanup;
    }
    if (fclose(resource_file) != 0) {
        resource_file = NULL;
        goto cleanup;
    }
    resource_file = NULL;
    checkpoint_manifest.version = LOGIC_REPLAY_MANIFEST_VERSION;
    strcpy(checkpoint_manifest.engine, "checkpoint-regression");
    strcpy(checkpoint_manifest.build, "test");
    strcpy(checkpoint_manifest.agi_version, "0x2917");
    strcpy(checkpoint_manifest.rng_algorithm, "cc65-table-v1");
    checkpoint_manifest.resource_count = 1;
    strcpy(checkpoint_manifest.resources[0].name, "resource.bin");
    checkpoint_manifest.checkpoint_length = 1;
    checkpoint_manifest.initial_checkpoint[0] = 0x42;
    checkpoint_manifest.rng_seed = 1;
    checkpoint_manifest.tick_numerator = 10;
    checkpoint_manifest.tick_denominator = 1;
    if (logic_replay_sha256_file(resource_path, resource_hash) != 0 ||
        logic_replay_trace_write(checkpoint_trace_path, NULL, 0) != 0) {
        fprintf(stderr, "could not create full-app checkpoint test inputs\n");
        goto cleanup;
    }
    memcpy(checkpoint_manifest.resources[0].sha256, resource_hash,
           sizeof resource_hash);
    /* Write the final manifest only after populating its verified resource hash. */
    if (logic_replay_manifest_write(checkpoint_manifest_path,
                                    &checkpoint_manifest) != 0) {
        fprintf(stderr, "could not write full-app checkpoint test manifest\n");
        goto cleanup;
    }
    if (!strcmp(argv[1], "--fail-after-fixtures")) {
        intentional = 1;
        failed = 0;
        goto cleanup;
    }
    for (int semantic = 0; semantic < 2; ++semantic) {
        if (ftruncate(fd, 0) || lseek(fd, 0, SEEK_SET) < 0) goto cleanup;
        child = fork();
        if (child < 0)
            goto cleanup;
        if (!child) {
            if (dup2(fd, STDERR_FILENO) < 0)
                _exit(126);
            close(fd);
            if (semantic)
                execl(argv[1], argv[1], "app-reduce", "--failure", "semantic",
                      "--game-dir", directory, "--manifest", manifest_path,
                      "--input-trace", trace_path, "--ticks", "1",
                      "--output-trace", reduced_path, (char *)NULL);
            else execl(argv[1], argv[1], "app-reduce", "--game-dir", directory,
                  "--manifest", manifest_path, "--input-trace", trace_path,
                  "--ticks", "1", "--output-trace", reduced_path, (char *)NULL);
            _exit(127);
        }
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR)
                goto cleanup;
        }
        if (lseek(fd, 0, SEEK_SET) < 0)
            goto cleanup;
        size = read(fd, output, sizeof output - 1);
        if (size < 0)
            goto cleanup;
        output[size] = '\0';
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 ||
            strstr(output, "usage:") ||
            !strstr(output, "invalid full-app manifest")) {
            fprintf(stderr, "app-reduce request rejected before manifest validation:\n%s", output);
            goto cleanup;
        }
    }
    for (unsigned rejection = 0; rejection < 3; ++rejection) {
        static const char *const reasons[] = {
            "initial checkpoint restore is unsupported",
            "unsupported full-app RNG algorithm",
            "missing full-app application image identity"
        };
        if (rejection) {
            checkpoint_manifest.checkpoint_length = 0;
            strcpy(checkpoint_manifest.rng_algorithm, rejection == 1 ?
                   "cc65-table-v1" : "scummvm-randomsource-v1");
            if (logic_replay_manifest_write(checkpoint_manifest_path,
                                            &checkpoint_manifest)) goto cleanup;
        }
        if (ftruncate(fd, 0) || lseek(fd, 0, SEEK_SET) < 0)
            goto cleanup;
        child = fork();
        if (child < 0)
            goto cleanup;
        if (!child) {
            if (dup2(fd, STDERR_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0)
                _exit(126);
            close(fd);
            execl(argv[1], argv[1], "full-app", "--game-dir", directory,
                  "--manifest", checkpoint_manifest_path,
                  "--input-trace", checkpoint_trace_path, "--ticks", "1",
                  "--timeout-ms", "1", "--output", checkpoint_output_path,
                  (char *)NULL);
            _exit(127);
        }
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR)
                goto cleanup;
        }
        if (lseek(fd, 0, SEEK_SET) < 0)
            goto cleanup;
        size = read(fd, output, sizeof output - 1);
        if (size < 0)
            goto cleanup;
        output[size] = '\0';
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 ||
            !strstr(output, reasons[rejection]) ||
            (access(checkpoint_output_path, F_OK) == 0 || errno != ENOENT)) {
            fprintf(stderr, "full-app CLI did not reject invalid replay identity:\n%s", output);
            goto cleanup;
        }
    }
    failed = 0;
    puts("app-reduce requests and unsupported full-app checkpoint rejection passed");
cleanup:
    if (resource_file && fclose(resource_file) != 0)
        failed = 1;
    if (fd >= 0 && close(fd) != 0)
        failed = 1;
    if (unlink(log_path) != 0 && errno != ENOENT)
        failed = 1;
    if (unlink(resource_path) != 0 && errno != ENOENT)
        failed = 1;
    if (unlink(checkpoint_manifest_path) != 0 && errno != ENOENT)
        failed = 1;
    if (unlink(checkpoint_trace_path) != 0 && errno != ENOENT)
        failed = 1;
    if (unlink(checkpoint_output_path) != 0 && errno != ENOENT)
        failed = 1;
    if (rmdir(directory) != 0)
        failed = 1;
    return failed ? 1 : intentional ? 2 : 0;
}
