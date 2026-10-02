/* Native exact-VRAM and cycle check for the assembled picture clear. */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "registers.h"
#include "fake6502.h"

extern struct regs regs;
static uint8_t ram[65536], vram[131072];
static unsigned addr[2], stride[2], writes;
static char temporary[4096];
static struct { char name[100]; unsigned address; } symbols[16];
static unsigned symbol_count;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s (%s:%d)\n", m, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (address == 0x9F23 || address == 0x9F24) {
        unsigned port = address - 0x9F23;
        uint8_t value = vram[addr[port]];
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        return value;
    }
    return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if (address >= 0x9F20 && address <= 0x9F22) {
        static const unsigned increments[16] = {0,1,2,4,8,16,32,64,128,256,512,40,80,160,320,640};
        unsigned port = ram[0x9F25] & 1;
        ram[address] = value;
        addr[port] = ram[0x9F20] | (ram[0x9F21] << 8) | ((ram[0x9F22] & 1) << 16);
        stride[port] = increments[ram[0x9F22] >> 4];
        return;
    }
    if (address == 0x9F23 || address == 0x9F24) {
        unsigned port = address - 0x9F23;
        vram[addr[port]] = value;
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        ++writes;
        return;
    }
    ram[address] = value;
}

uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank) { (void)address; (void)bank; CHECK(false, "CPU stopped unexpectedly"); }

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot read graphics assembly");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek assembly");
    long size = ftell(file); CHECK(size >= 0, "Cannot size assembly"); rewind(file);
    char *contents = malloc((size_t)size + 1); CHECK(contents, "Out of memory");
    CHECK(fread(contents, 1, (size_t)size, file) == (size_t)size, "Cannot read assembly");
    contents[size] = 0; CHECK(!fclose(file), "Cannot close assembly"); return contents;
}

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create fixture");
    CHECK(fwrite(contents, 1, strlen(contents), file) == strlen(contents), "Cannot write fixture");
    CHECK(!fclose(file), "Cannot close fixture");
}

static void run_tool(const char *name, char *const arguments[]) {
    char tool[4096]; const char *home = getenv("CC65_HOME");
    if (home) CHECK(snprintf(tool, sizeof(tool), "%s/bin/%s", home, name) < (int)sizeof(tool), "Tool path too long");
    else CHECK(snprintf(tool, sizeof(tool), "%s", name) < (int)sizeof(tool), "Tool path too long");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        char *argv[12]; unsigned count = 0;
        while (arguments[count]) { CHECK(count < 11, "Too many assembler arguments"); argv[count] = arguments[count]; ++count; }
        argv[0] = tool; argv[count] = NULL;
        execv(tool, argv); perror(tool); _exit(127);
    }
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(const char *graphics_path) {
    char path[8192], config[8192], object[8192], binary[8192], labels[8192], fixture[8192];
    snprintf(path, sizeof(path), "%s/clear.s", temporary);
    snprintf(config, sizeof(config), "%s/clear.cfg", temporary);
    snprintf(object, sizeof(object), "%s/clear.o", temporary);
    snprintf(binary, sizeof(binary), "%s/clear.bin", temporary);
    snprintf(labels, sizeof(labels), "%s/clear.lbl", temporary);

    char *graphics = read_file(graphics_path);
    char *clear = strstr(graphics, "_b8ClearBackground:"); CHECK(clear, "Background clear missing");
    char *end = strstr(clear, "\n.endif"); CHECK(end, "Background clear end missing");
    int length = snprintf(fixture, sizeof(fixture),
        ".include \"global.s\"\n"
        "BITMAP_WIDTH=320\nPICTURE_WIDTH=160\nPICTURE_HEIGHT=168\n"
        "STARTING_ROW=(240-PICTURE_HEIGHT)/2\nSTARTING_BYTE=STARTING_ROW*(BITMAP_WIDTH/2)\n"
        "LEFT_BORDER=$A1\nRIGHT_BORDER=$B2\nDEFAULT_BACKGROUND_COLOR=$C3\n"
        ".export runBackground,done\n.segment \"CODE\"\n%.*s\n"
        "runBackground: jsr _b8ClearBackground\ndone: rts\n",
        (int)(end - clear), clear);
    CHECK(length >= 0 && length < (int)sizeof(fixture), "Fixture assembly too large");
    free(graphics); write_file(path, fixture);
    write_file(config,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object, NULL};
    char *ld65[] = {"ld65", "-C", config, object, "-o", binary, "-Ln", labels, NULL};
    run_tool("ca65", ca65); run_tool("ld65", ld65);

    FILE *file = fopen(binary, "rb"); CHECK(file, "Cannot read linked fixture");
    CHECK(fread(ram + 0x2000, 1, 0x6000, file) > 0, "Fixture binary is empty");
    CHECK(!fclose(file), "Cannot close fixture binary");
    file = fopen(labels, "r"); CHECK(file, "Cannot read labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) if (sscanf(line, "%15s %x %99s", kind, &address, name) == 3) {
        CHECK(symbol_count < sizeof(symbols) / sizeof(symbols[0]), "Too many symbols");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name);
        symbols[symbol_count++].address = address;
    }
    CHECK(!fclose(file), "Cannot close labels");
}

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    fprintf(stderr, "Missing assembler symbol: %s\n", name); exit(1);
}

int main(int argc, char **argv) {
    const unsigned start = 36 * 160, width = 160, height = 168;
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(temporary, sizeof(temporary), "%s/graphics-clear-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory");
    assemble(argc > 1 ? argv[1] : "src/graphicsAsm.s"); memset(vram, 0x5A, sizeof(vram));

    reset6502(false); regs.pc = (uint16_t)symbol("runBackground"); regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT; ram[0x1FE] = (uint8_t)(symbol("done") - 1); ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    clockticks6502 = 0; waiting = 0;
    for (unsigned step = 0; step < 3000000 && regs.pc != symbol("done"); ++step) {
        CHECK(!waiting, "Unexpected WAI"); step6502();
    }
    CHECK(regs.pc == symbol("done"), "Background clear did not return");
    CHECK(writes == width * height, "Background clear wrote the wrong byte count");
    CHECK(vram[start - 1] == 0x5A && vram[start + width * height] == 0x5A, "Background clear wrote outside the picture");
    for (unsigned row = 0; row < height; ++row) {
        unsigned offset = start + row * width;
        CHECK(vram[offset] == 0xA1 && vram[offset + width - 1] == 0xB2, "Background border byte differs");
        for (unsigned col = 1; col < width - 1; ++col)
            CHECK(vram[offset + col] == 0xC3, "Background interior byte differs");
    }
    printf("Background clear writes exact VRAM; %u cycles\n", (unsigned)clockticks6502);
    CHECK(clockticks6502 < 200000, "Background clear exceeded its cycle budget");
    return 0;
}
