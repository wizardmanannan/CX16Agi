/* Anonymous packets stay inside the suite's unique build-root fixture. */
static FILE* targetFixtureFile(void)
{
    const char* root = getenv("FORMAT_TARGET_FIXTURE");
    char* path;
    int descriptor;
    FILE* file;
    assert(root);
    path = malloc(strlen(root) + sizeof("/packet.XXXXXX"));
    assert(path);
    sprintf(path, "%s/packet.XXXXXX", root);
    descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(unlink(path) == 0);
    free(path);
    file = fdopen(descriptor, "w+");
    assert(file);
    return file;
}

static unsigned int targetFixtureSymbol(const char* labels, const char* wanted)
{
    FILE* file = fopen(labels, "r");
    char line[256], name[128];
    unsigned int address;
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "al %x %127s", &address, name) == 2 &&
            name[0] == '.' && strcmp(name + 1, wanted) == 0) {
            fclose(file);
            return address;
        }
    }
    abort();
}
