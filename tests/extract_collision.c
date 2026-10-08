/* Copy the real assembly routines into the simulator build, unchanged. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char** argv)
{
    FILE *source, *output;
    char line[1024];
    int copying = 0, done = 0;
    assert(argc == 3);
    source = fopen(argv[1], "r"); output = fopen(argv[2], "w");
    assert(source && output);
    while (fgets(line, sizeof line, source)) {
        if (!strncmp(line, "_b9Collide:", 11)) copying = 1;
        if (copying && !strncmp(line, "b9GoodPositionAsm:", 18)) { done = 1; break; }
        if (copying) assert(fputs(line, output) >= 0);
    }
    assert(done && !ferror(source));
    assert(!fclose(source) && !fclose(output));
    return 0;
}
