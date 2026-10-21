/* Teleport's footprint from real cc65 objects: per-bank bytes of the
   assembly modules against budgets, and nothing at all when disabled. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#error These tests require assertions
#endif

/* Budgets: today's size plus a little room, so growth is a decision. */
static const struct { const char* segment; unsigned long budget; } budgets[] = {
    { "BSS", 4 },           /* low RAM: the request, X, Y, tour flag */
    { "BANKRAM06", 224 },   /* interpreter cycle, room entry, unparking */
    { "BANKRAM07", 480 },   /* keyword, line hand-off, notices */
    { "BANKRAM09", 1000 },  /* placement, quiet-spot checks, parking */
    { "BANKRAM0F", 3328 },  /* grammar, room search, triggers, tours, replies */
};

static void sizes(const char* path)
{
    FILE* file = fopen(path, "r");
    char line[256], name[64];
    unsigned long size;
    unsigned i, seen = 0;
    assert(file);
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, " %63[A-Z0-9_]: %lu", name, &size) != 2) continue;
        for (i = 0; i < sizeof budgets / sizeof budgets[0]; ++i) {
            if (strcmp(name, budgets[i].segment)) continue;
            printf("%-10s %5lu bytes (budget %lu)\n", name, size, budgets[i].budget);
            assert(size <= budgets[i].budget);
            ++seen;
        }
    }
    assert(!ferror(file) && seen == sizeof budgets / sizeof budgets[0]);
    fclose(file);
    puts("PASS: teleport assembly within its per-bank budgets");
}

static void no_teleport(const char* path)
{
    FILE* file = fopen(path, "r");
    char line[1024];
    unsigned names = 0;
    assert(file);
    while (fgets(line, sizeof line, file)) {
        if (!strstr(line, "Name:")) continue; /* the path names the test directory */
        ++names;
        assert(!strstr(line, "teleport") && !strstr(line, "Teleport"));
    }
    assert(!ferror(file) && names);
    fclose(file);
}

int main(int argc, char** argv)
{
    int i;
    assert(argc >= 3);
    sizes(argv[1]);
    for (i = 2; i < argc; ++i) no_teleport(argv[i]);
    puts("PASS: with ENABLE_TELEPORT=0 the parser, interpreter and assembly have no teleport symbols");
    return 0;
}
