#define _GNU_SOURCE
#include "test_paths.h"
#include "test_run_from.h"

/* Build the shipped makefile, rather than the separately maintained test image. */
int main(int argc, char **argv)
{
    if (argc != 2) return EXIT_FAILURE;
    suite_test_paths_init("build");
    char build[4096];
    suite_test_path(build, sizeof(build), suite_output_directory, "image");
    if (mkdir(build, 0700)) return EXIT_FAILURE;
    char program[8192], objects[8192], log[4096], image[4096];
    snprintf(program, sizeof(program), "PROGRAM=%s/agi", build);
    snprintf(objects, sizeof(objects), "OBJDIR=%s/objects", build);
    suite_test_path(log, sizeof(log), suite_output_directory, "build.log");
    char *command[] = {"make", "-C", argv[1], "-f", "makefile", program, objects, "ASFLAGS=-g", NULL};
    run_from(command, log, 120, NULL);
    const char *files[] = {"agi.cx16", "agi.cx16.01", "agi.cx16.0B", "agi.cx16.map"};
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        struct stat info;
        suite_test_path(image, sizeof(image), build, files[i]);
        if (stat(image, &info) || info.st_size <= 2) {
            fprintf(stderr, "Missing production build output: %s\n", image);
            return EXIT_FAILURE;
        }
    }
    puts("PASS shipped makefile produces the game image, banks and portable map");
    return EXIT_SUCCESS;
}
