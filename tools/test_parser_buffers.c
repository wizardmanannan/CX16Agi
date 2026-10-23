/* Exercise the production internal-string getter with exact array bounds. */
#define _CX16_H
#define asm(...) ((void)0)
#include "../src/parser.c"

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s\n", m); exit(EXIT_FAILURE); } } while (0)

int main(int argc, char **argv)
{
    size_t length = 1234;
    strcpy(string[11], "last");
    CHECK(b7GetInternalStringPtr(11, &length) == string[11] && length == 4,
          "Final internal string was rejected or measured incorrectly");
    if (argc == 2 && !strcmp(argv[1], "--unterminated")) {
        memset(string[11], 'X', sizeof(string[11]));
        CHECK(b7GetInternalStringPtr(11, &length) == NULL && length == 0,
              "Unterminated internal string was scanned beyond its row");
    } else {
        CHECK(b7GetInternalStringPtr(12, &length) == NULL && length == 0,
              "Internal string index beyond the array was accepted");
        length = 1234;
        CHECK(b7GetInternalStringPtr(255, &length) == NULL && length == 0,
              "Internal string byte sentinel was accepted");
    }
    puts("Parser internal-string boundaries pass");
    return 0;
}
