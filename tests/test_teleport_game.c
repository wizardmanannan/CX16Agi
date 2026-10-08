/* Real KQ1 input, room logic and VERA output. No mocked interpreter services. */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char fixture[4096], labels[4096];
static pid_t child;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static void stop(void)
{
    if (child > 0) { kill(child, SIGKILL); while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {} child = 0; }
}
static void cleanup(void)
{
    DIR *d; struct dirent *e; char p[8192];
    stop();
    if (!fixture[0]) return;
    d = opendir(fixture);
    if (d) {
        while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            snprintf(p, sizeof p, "%s/%s", fixture, e->d_name);
            if (unlink(p)) perror(p);
        }
        closedir(d);
    }
    if (rmdir(fixture)) perror(fixture);
}
static void timeout_handler(int sig) { (void)sig; exit(124); }
static void stage(const char *source, int program)
{
    DIR *d = opendir(source); struct dirent *e; char from[8192], to[8192]; struct stat st;
    CHECK(d);
    while ((e = readdir(d))) {
        if (program && strncmp(e->d_name, "agi.cx16", 8)) continue;
        if (!program && !strncmp(e->d_name, "agi.cx16", 8)) continue;
        snprintf(from, sizeof from, "%s/%s", source, e->d_name);
        CHECK(!stat(from, &st));
        if (!S_ISREG(st.st_mode)) continue;
        snprintf(to, sizeof to, "%s/%s", fixture, e->d_name);
        {
            FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
            char buffer[16384]; size_t n;
            CHECK(in && out);
            while ((n = fread(buffer, 1, sizeof buffer, in))) CHECK(fwrite(buffer, 1, n, out) == n);
            CHECK(!ferror(in)); CHECK(!fclose(in)); CHECK(!fclose(out));
        }
    }
    closedir(d);
}
static unsigned symbol(const char *name)
{
    FILE *f = fopen(labels, "r"); char line[256], key[128]; unsigned n;
    CHECK(f);
    while (fgets(line, sizeof line, f)) if (sscanf(line, "al %x .%127s", &n, key) == 2 && !strcmp(key, name)) { fclose(f); return n; }
    fclose(f); CHECK(!"missing label"); return 0;
}
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
typedef struct State { unsigned char *bytes, *ram, *bram, *video; } State;
static State replay(const char *root, const char *input, int walk)
{
    char path[8192], emulator[8192], rom[8192], line[2048];
    FILE *f; int out[2]; long size; State s; unsigned header;
    snprintf(path, sizeof path, "%s/input.tas", fixture);
    f = fopen(path, "w"); CHECK(f);
    fprintf(f, "80000000:Return\n"); /* Dismiss KQ1's title screen. */
    if (input[0]) {
        const char *p;
        fputs("80000000:", f);
        /* TAS injects raw KERNAL bytes; keyboard letters are PETSCII 65..90. */
        for (p = input; *p; ++p) fputc(*p >= 'a' && *p <= 'z' ? *p - 32 : *p, f);
        fputs("\n20000000:Return\n", f);
    }
    if (walk) fputs("40000000:\\X9D\n4000000:\\X9D\n", f); /* Walk left, then stop. */
    fprintf(f, "80000000:__SNAPSHOT_SAVE__\n"); CHECK(!fclose(f));
    CHECK(!pipe(out));
    snprintf(emulator, sizeof emulator, "%s/.tools/x16-emulator/build/x16emu", root);
    snprintf(rom, sizeof rom, "%s/.tools/rom-r49/rom.bin", root);
    child = fork(); CHECK(child >= 0);
    if (!child) {
        CHECK(!chdir(fixture)); CHECK(dup2(out[1], STDOUT_FILENO) >= 0);
        close(out[0]); close(out[1]);
        setenv("SDL_VIDEODRIVER", "dummy", 1); setenv("SDL_AUDIODRIVER", "dummy", 1);
        execl(emulator, emulator, "-rom", rom, "-prg", "agi.cx16", "-run", "-fsroot", fixture,
              "-startin", fixture, "-warp", "-sound", "none", "-tas", "input.tas",
              "-snapshot-save", "state.bin", (char *)NULL);
        _exit(127);
    }
    close(out[1]); f = fdopen(out[0], "r"); CHECK(f); alarm(60);
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, "TAS action=")) fputs(line, stdout);
        if (strstr(line, "snapshot=saved") && strstr(line, "cycle=")) break;
    }
    CHECK(!feof(f)); alarm(0); stop(); fclose(f);
    snprintf(path, sizeof path, "%s/state.bin", fixture);
    f = fopen(path, "rb"); CHECK(f); CHECK(!fseek(f, 0, SEEK_END)); size = ftell(f); CHECK(size > 40); rewind(f);
    s.bytes = malloc((size_t)size); CHECK(s.bytes); CHECK(fread(s.bytes, 1, (size_t)size, f) == (size_t)size); fclose(f);
    /* Pinned emulator snapshot v6: common header prefix, then RAM, BRAM,
       VideoSnapshotState (VRAM first, palette, then sprite registers). */
    CHECK(u32(s.bytes + 8) == 6 && u32(s.bytes + 20) == 0xa000);
    CHECK(u32(s.bytes + 32) < (unsigned long)size);
    header = (unsigned)size - u32(s.bytes + 32);
    s.ram = s.bytes + header; s.bram = s.ram + u32(s.bytes + 20);
    s.video = s.bram + u32(s.bytes + 24);
    CHECK(s.video + 0x20600 < s.bytes + size);
    return s;
}
static unsigned room(State s) { return s.ram[u16(s.ram + symbol("_var"))]; }
static unsigned ego_field(State s, const char *offset)
{ return s.bram[9 * 8192 + symbol("_viewtab") - 0xa000 + s.ram[symbol(offset)]]; }
static void ground_landing(State s)
{
    unsigned x = ego_field(s, "_offsetOfXPos"), y = ego_field(s, "_offsetOfYPos");
    unsigned end = x + ego_field(s, "_offsetOfXSize");
    for (; x < end; ++x) {
        unsigned packed = s.video[38400 + y * 80 + x / 2];
        unsigned priority = x & 1 ? packed & 15 : packed >> 4;
        if (priority != 4) fprintf(stderr, "Landing on scenery: (%u,%u) priority=%u\n", x, y, priority);
        CHECK(priority == 4);
    }
}
static unsigned visible(State s)
{
    unsigned i, j, pixels = 0, x = ego_field(s, "_offsetOfXPos") * 2;
    unsigned y = ego_field(s, "_offsetOfYPos") - ego_field(s, "_offsetOfYSize") + 1 + 36;
    for (i = 0; i < 128; ++i) {
        unsigned char *a = s.video + 0x20200 + i * 8;
        unsigned address, width, height;
        width = 8u << ((a[7] >> 4) & 3); height = 8u << (a[7] >> 6);
        if (!(a[6] & 12) || (u16(a + 4) & 1023) != y) continue;
        if ((u16(a + 2) & 1023) != (a[6] & 1 ? (x - width + 2 * ego_field(s, "_offsetOfXSize")) & 1023 : x)) continue;
        address = (u16(a) & 4095) * 32;
        CHECK(!(a[1] & 128)); /* Interpreter uses 4bpp sprite bitmaps. */
        CHECK(address + width * height / 2 <= 0x20000);
        for (j = 0; j < width * height / 2; ++j) pixels += !!s.video[address + j];
    }
    return pixels;
}
int main(int argc, char **argv)
{
    char program[4096], game[4096], root[4096], build[4096]; State before, after, walked; unsigned pixels;
    CHECK(argc == 4); CHECK(realpath(argv[1], root)); CHECK(realpath(argv[2], game)); CHECK(realpath(argv[3], program));
    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(realpath("../build/tests/teleport", build));
    CHECK(snprintf(fixture, sizeof fixture, "%s/game-XXXXXX", build) < (int)sizeof fixture);
    CHECK(mkdtemp(fixture)); atexit(cleanup); signal(SIGALRM, timeout_handler);
    CHECK(snprintf(labels, sizeof labels, "%s/agi.cx16.lbl", program) < (int)sizeof labels);
    stage(game, 0); stage(program, 1);
    if (getenv("TELEPORT_TEST_FAIL")) CHECK(!"injected cleanup failure");
    before = replay(root, "", 0); printf("baseline: room=%u var=%04x ptr=%04x ego=(%u,%u) pixels=%u\n", room(before), symbol("_var"), u16(before.ram + symbol("_var")), ego_field(before, "_offsetOfXPos"), ego_field(before, "_offsetOfYPos"), visible(before)); CHECK(room(before) == 1); CHECK(visible(before) > 0);
    after = replay(root, "teleport 1 80 100", 0); pixels = visible(after);
    printf("input=%s pending=%u control=%u\n", after.bram + 7 * 8192 + symbol("_b7CurrentInputStr") - 0xa000, after.ram[symbol("_teleportPending")], after.ram[symbol("_controlMode")]);
    printf("same-room: room=%u ego=(%u,%u) visible sprite bytes=%u\n", room(after), ego_field(after, "_offsetOfXPos"), ego_field(after, "_offsetOfYPos"), pixels);
    CHECK(room(after) == 1 && pixels > 0);
    CHECK(ego_field(after, "_offsetOfXPos") < ego_field(before, "_offsetOfXPos"));
    CHECK(after.ram[symbol("_controlMode")] == 0); free(after.bytes);
    after = replay(root, "teleport 2 100 100", 0);
    printf("cross-room: room=%u ego=(%u,%u) visible sprite bytes=%u control=%u\n", room(after), ego_field(after, "_offsetOfXPos"), ego_field(after, "_offsetOfYPos"), visible(after), after.ram[symbol("_controlMode")]);
    CHECK(room(after) == 2); CHECK(memcmp(before.video, after.video, 38400)); CHECK(visible(after) > 0);
    CHECK(after.ram[symbol("_controlMode")] == 0);
    ground_landing(after);
    walked = replay(root, "teleport 2 100 100", 1);
    CHECK(room(walked) == 2 && visible(walked) > 0);
    printf("walk away: x=%u -> %u\n", ego_field(after, "_offsetOfXPos"), ego_field(walked, "_offsetOfXPos"));
    CHECK(ego_field(walked, "_offsetOfXPos") < ego_field(after, "_offsetOfXPos"));
    CHECK(walked.ram[symbol("_controlMode")] == 0); free(walked.bytes);
    free(before.bytes); free(after.bytes);
    puts("PASS: KQ1 teleport keeps ego visible and draws the destination room"); return 0;
}
