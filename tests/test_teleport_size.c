/* Validate od65 reports from the real CX16 compiler. Object headers/debug
   metadata occupy disk space but are not emitted target code or RAM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#error These tests require assertions
#endif

static void segments(const char* path, int enabled)
{
    FILE* file = fopen(path, "r");
    char line[1024], name[128] = "";
    unsigned long size, total = 0, bss = 0;
    unsigned seen = 0;
    assert(file);
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, " Name: \"%127[^\"]\"", name) == 1) {
            if (!enabled) {
                assert(!strstr(name, "teleport") && !strstr(name, "Teleport"));
            }
        } else if (sscanf(line, " Size: %lu", &size) == 1) {
            ++seen;
            total += size;
            if (!strcmp(name, "BSS")) bss = size;
            if (!enabled || (!strcmp(name, "ZEROPAGE") || !strcmp(name, "DATA")))
                assert(size == 0);
        }
    }
    assert(!ferror(file) && seen >= 6);
    fclose(file);
    if (enabled) {
        assert(bss == 3 && total > bss);
        printf("PASS: enabled cc65 object: %lu bytes persistent RAM, %lu bytes code/data\n",
               bss, total - bss);
    } else {
        assert(total == 0);
        puts("PASS: disabled cc65 object: 0 emitted bytes across all segments");
    }
}

static void no_references(const char* path)
{
    FILE* file = fopen(path, "r");
    char line[1024], name[128];
    unsigned seen = 0;
    assert(file);
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, " Name: \"%127[^\"]\"", name) == 1) {
            ++seen;
            assert(!strstr(name, "teleport") && !strstr(name, "Teleport"));
        }
    }
    assert(!ferror(file) && seen);
    fclose(file);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    segments(argv[1], 1);
    segments(argv[2], 0);
    no_references(argv[3]);
    no_references(argv[4]);
    puts("PASS: disabled parser and interpreter have no teleport symbol references");
    return 0;
}
