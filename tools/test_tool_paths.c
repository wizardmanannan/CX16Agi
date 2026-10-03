/* Exercise the real Makefile defaults from a linked Git worktree. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static void read_command(const char *command, char *output, size_t size)
{
    FILE *pipe = popen(command, "r");
    if (!pipe || !fgets(output, size, pipe)) {
        fprintf(stderr, "Cannot read command: %s\n", command);
        exit(1);
    }
    int status = pclose(pipe);
    if (status != 0) {
        fprintf(stderr, "Command failed: %s\n", command);
        exit(1);
    }
    output[strcspn(output, "\n")] = '\0';
}

int main(void)
{
    char common[4096], actual[16384], expected[16384];
    read_command("git rev-parse --path-format=absolute --git-common-dir", common, sizeof common);
    char *slash = strrchr(common, '/');
    if (!slash || strcmp(slash, "/.git")) return 1;
    *slash = '\0';
    snprintf(expected, sizeof expected, "%s/.tools/cc65-latest|%s/.tools/x16-emulator", common, common);
    read_command("env -u CC65_HOME -u X16_EMULATOR_SOURCE make --no-print-directory -s -f tools/Makefile --eval 'probe:;@printf \"%s|%s\\n\" \"$(CC65_HOME)\" \"$(X16_EMULATOR_SOURCE)\"' probe", actual, sizeof actual);
    if (strcmp(actual, expected)) {
        fprintf(stderr, "Shared dependency discovery: got [%s], expected [%s]\n", actual, expected);
        return 1;
    }
    read_command("env CC65_HOME=/explicit/compiler X16_EMULATOR_SOURCE=/explicit/emulator make --no-print-directory -s -f tools/Makefile --eval 'probe:;@printf \"%s|%s\\n\" \"$(CC65_HOME)\" \"$(X16_EMULATOR_SOURCE)\"' probe", actual, sizeof actual);
    if (strcmp(actual, "/explicit/compiler|/explicit/emulator")) return 1;
    puts("Shared tool paths and explicit overrides pass");
    return 0;
}
