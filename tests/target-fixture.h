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
