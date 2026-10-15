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
#include "test_paths.h"

extern struct regs regs;
static uint8_t ram[65536], vram[131072];
static unsigned addr[2], stride[2], writes;
static struct { char name[100]; unsigned address; } symbols[16];
static unsigned symbol_count;
#define PRIORITY_START ((320 * 240) / 2)
#define PRIORITY_SIZE ((160 * 168) / 2)
static unsigned symbol(const char *name);

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

static void assemble(const char *graphics_path, unsigned priority_size, unsigned map_cells) {
    char path[8192], config[8192], object[8192], binary[8192], labels[8192], fixture[8192];
    suite_test_path(path, sizeof(path), suite_fixture_directory, "clear.s");
    suite_test_path(config, sizeof(config), suite_fixture_directory, "clear.cfg");
    suite_test_path(object, sizeof(object), suite_output_directory, "clear.o");
    suite_test_path(binary, sizeof(binary), suite_output_directory, "clear.bin");
    suite_test_path(labels, sizeof(labels), suite_output_directory, "clear.lbl");

    char *graphics = read_file(graphics_path);
    char *irq = read_file("src/irqAsm.s");
    char *map_start = strstr(irq, "_b3InitLayer1Mapbase:"); CHECK(map_start, "Map clear missing");
    char *map_end = strstr(map_start, "\nhandleDisplayTextSlice:"); CHECK(map_end, "Map clear end missing");
    char *priority_start = strstr(graphics, "_b8ClearPriority:"); CHECK(priority_start, "Full priority clear missing");
    char *priority_end = strstr(priority_start, "\n; IRQ clear uses row-aligned slices"); CHECK(priority_end, "Full priority clear end missing");
    char *slice_start = strstr(graphics, "_b8ClearBackgroundSliceReset:"); CHECK(slice_start, "Background slice clear missing");
    char *slice_end = strstr(slice_start, "_b8ClearBackground:"); CHECK(slice_end, "Background slice clear end missing");
    char *clear = slice_end;
    char *end = strstr(clear, "\n.endif"); CHECK(end, "Background clear end missing");
    int length = snprintf(fixture, sizeof(fixture),
        ".include \"global.s\"\n"
        "MAP_BASE=$DA00\nMENU_BAR_WIDTH=40\nTILE_LAYER_WIDTH=64\nTILE_LAYER_HEIGHT=32\n"
        "TILE_LAYER_NO_TILES=(MENU_BAR_WIDTH+%u)\nTRANSPARENT_CHAR=$80\nTILE_BYTE_2=$10\n"
        "BITMAP_WIDTH=320\nPICTURE_WIDTH=160\nPICTURE_HEIGHT=168\n"
        "STARTING_ROW=(240-PICTURE_HEIGHT)/2\nSTARTING_BYTE=STARTING_ROW*(BITMAP_WIDTH/2)\n"
        "BYTES_PER_ROW=BITMAP_WIDTH/2\nPRIORITY_START=(BITMAP_WIDTH*BITMAP_HEIGHT)/2\n"
        "PRIORITY_SIZE=%u\nPRIORITY_DEF=$44\nBITMAP_HEIGHT=240\n"
        "LEFT_BORDER=$A1\nRIGHT_BORDER=$B2\nDEFAULT_BACKGROUND_COLOR=$C3\n"
        ".macro SET_VERA_ADDRESS_IMMEDIATE VERA_ADDRESS, ADDRESS_SEL, STRIDE\n"
        "lda ADDRESS_SEL\nsta VERA_ctrl\nlda #^VERA_ADDRESS\nand #$1\nora STRIDE << 4\nsta VERA_addr_bank\n"
        "lda #<VERA_ADDRESS\nsta VERA_addr_low\nlda #>VERA_ADDRESS\nsta VERA_addr_high\n.endmacro\n"
        "ZP_TILE_LAYER_NO_TILES_HIGH=ZP_TMP_2\n"
        ".export runBackground,done,runPriority,runBackgroundSlice,resetBackgroundSlices,runPrioritySlice,resetPrioritySlices,runMap,sliceDone,_b3InitLayer1Mapbase,_inputLineDirty\n"
        ".segment \"CODE\"\n_inputLineDirty: .byte $0\n%.*s\n%.*s\n%.*s\n%.*s\n"
        "runBackground: jsr _b8ClearBackground\ndone: rts\n"
        "runPriority: jsr _b8ClearPriority\nrts\n"
        "resetBackgroundSlices: jsr _b8ClearBackgroundSliceReset\nrts\n"
        "runBackgroundSlice: jsr _b8ClearBackgroundSlice\nrts\n"
        "resetPrioritySlices: jsr _b8ClearPrioritySliceReset\nrts\n"
        "runPrioritySlice: jsr _b8ClearPrioritySlice\nrts\n"
        "runMap: lda #TILE_BYTE_2\njsr _b3InitLayer1Mapbase\nrts\n"
        "sliceDone: rts\n",
        map_cells, priority_size,
        (int)(map_end - map_start), map_start,
        (int)(priority_end - priority_start), priority_start,
        (int)(slice_end - slice_start), slice_start, (int)(end - clear), clear);
    CHECK(length >= 0 && length < (int)sizeof(fixture), "Fixture assembly too large");
    free(graphics); free(irq); write_file(path, fixture);
    write_file(config,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object, NULL};
    char *ld65[] = {"ld65", "-C", config, object, "-o", binary, "-Ln", labels, NULL};
    run_tool("ca65", ca65); run_tool("ld65", ld65);

    FILE *file = fopen(binary, "rb"); CHECK(file, "Cannot read linked fixture");
    memset(ram + 0x2000, 0, 0x6000);
    CHECK(fread(ram + 0x2000, 1, 0x6000, file) > 0, "Fixture binary is empty");
    CHECK(!fclose(file), "Cannot close fixture binary");
    symbol_count = 0;
    file = fopen(labels, "r"); CHECK(file, "Cannot read labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) if (sscanf(line, "%15s %x %99s", kind, &address, name) == 3) {
        CHECK(symbol_count < sizeof(symbols) / sizeof(symbols[0]), "Too many symbols");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name);
        symbols[symbol_count++].address = address;
    }
    CHECK(!fclose(file), "Cannot close labels");
}

static void run_helper(const char *entry, unsigned max_steps) {
    unsigned start = symbol(entry), done = symbol("sliceDone");
    reset6502(false); regs.pc = (uint16_t)start; regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT; ram[0x1FE] = (uint8_t)(done - 1); ram[0x1FF] = (uint8_t)((done - 1) >> 8);
    clockticks6502 = 0; waiting = 0;
    for (unsigned step = 0; step < max_steps && regs.pc != done; ++step) {
        CHECK(!waiting, "Unexpected WAI in clear helper"); step6502();
    }
    CHECK(regs.pc == done, "Clear helper did not return");
}

static void test_bounded_slices(void) {
    const unsigned start = 36 * 160, width = 160, height = 168;
    unsigned max_background_cycles = 0, max_priority_cycles = 0;
    memset(vram, 0x5A, sizeof(vram)); memset(addr, 0, sizeof(addr)); memset(stride, 0, sizeof(stride)); writes = 0;

    run_helper("resetBackgroundSlices", 1000);
    for (unsigned slice = 0; slice < 14; ++slice) {
        unsigned before = writes;
        run_helper("runBackgroundSlice", 300000);
        unsigned count = writes - before;
        CHECK(count == 12 * width && count <= 2048, "Background slice exceeded its write budget or missed rows");
        CHECK(regs.a == (slice == 13 ? 1 : 0), "Background slice returned the wrong completion byte");
        CHECK((regs.status & FLAG_ZERO) == (slice == 13 ? 0 : FLAG_ZERO), "Background slice completion flags disagree with A");
        CHECK(clockticks6502 < 25000, "Background slice exceeded its cycle budget");
        if (clockticks6502 > max_background_cycles) max_background_cycles = (unsigned)clockticks6502;
    }
    CHECK(writes == width * height, "Background slices wrote the wrong total byte count");
    CHECK(vram[start - 1] == 0x5A && vram[start + width * height] == 0x5A, "Background slices wrote outside the picture");
    for (unsigned row = 0; row < height; ++row) {
        unsigned offset = start + row * width;
        CHECK(vram[offset] == 0xA1 && vram[offset + width - 1] == 0xB2, "Sliced background border differs");
        for (unsigned col = 1; col < width - 1; ++col)
            CHECK(vram[offset + col] == 0xC3, "Sliced background interior differs");
    }

    writes = 0; memset(vram + PRIORITY_START, 0x5A, PRIORITY_SIZE + 1);
    run_helper("resetPrioritySlices", 1000);
    for (unsigned slice = 0; slice < 7; ++slice) {
        unsigned before = writes;
        run_helper("runPrioritySlice", 300000);
        unsigned count = writes - before;
        CHECK(count == (slice < 6 ? 2048 : 1152), "Priority slice wrote the wrong chunk size");
        CHECK(count <= 2048, "Priority slice exceeded its write budget");
        CHECK(regs.a == (slice == 6 ? 1 : 0), "Priority slice returned the wrong completion byte");
        CHECK((regs.status & FLAG_ZERO) == (slice == 6 ? 0 : FLAG_ZERO), "Priority slice completion flags disagree with A");
        CHECK(clockticks6502 < 25000, "Priority slice exceeded its cycle budget");
        if (clockticks6502 > max_priority_cycles) max_priority_cycles = (unsigned)clockticks6502;
    }
    CHECK(writes == PRIORITY_SIZE, "Priority slices wrote the wrong total byte count");
    for (unsigned i = 0; i < PRIORITY_SIZE; ++i) CHECK(vram[PRIORITY_START + i] == 0x44, "Priority byte differs");
    CHECK(vram[PRIORITY_START - 1] == 0x5A && vram[PRIORITY_START + PRIORITY_SIZE] == 0x5A, "Priority slices wrote outside the picture");

    memset(vram, 0x5A, sizeof(vram)); writes = 0; ram[symbol("_inputLineDirty")] = 0;
    run_helper("runMap", 300000);
    unsigned map_cycles = (unsigned)clockticks6502;
    CHECK(writes == (64 * 32 - 40) * 2, "Map clear wrote the wrong number of bytes");
    CHECK(vram[0xDA50 - 1] == 0x5A && vram[0xEA00] == 0x5A, "Map clear wrote outside its exact map range");
    for (unsigned address = 0xDA50; address < 0xEA00; address += 2)
        CHECK(vram[address] == 0x80 && vram[address + 1] == 0x10, "Map clear tile bytes differ");
    CHECK(ram[symbol("_inputLineDirty")] == 1, "Map clear did not invalidate the input row");
    /* Paired single-cell loop baseline: 26238 cycles. */
    CHECK(clockticks6502 < 22000, "Map clear exceeded 22000 cycles (baseline 26238)");
    printf("Clear slices: background max %u cycles/1920 writes, priority max %u cycles/2048 writes, map %u cycles/4016 writes; 22 VSYNCs total\n",
           max_background_cycles, max_priority_cycles, map_cycles);
    /* Prior priority IRQ loop measured 18,568 cycles; keep its replacement below 13,000. */
    CHECK(max_background_cycles < 9900, "Background slice exceeded 9900 cycles (baseline 10392)");
    CHECK(max_priority_cycles < 13000, "Priority slice exceeded the 13000-cycle target (baseline 18568)");
}

static void test_full_priority_clear(void) {
    memset(vram, 0x5A, sizeof(vram)); writes = 0;
    run_helper("runPriority", 1000000);
    unsigned cycles = (unsigned)clockticks6502;
    CHECK(writes == PRIORITY_SIZE, "Full priority clear wrote the wrong number of bytes");
    CHECK(vram[PRIORITY_START - 1] == 0x5A && vram[PRIORITY_START + PRIORITY_SIZE] == 0x5A,
          "Full priority clear wrote outside its exact range");
    for (unsigned i = 0; i < PRIORITY_SIZE; ++i)
        CHECK(vram[PRIORITY_START + i] == 0x44, "Full priority clear byte differs");
    printf("Full priority clear: %u cycles/%u writes\n", cycles, PRIORITY_SIZE);
    /* Baselines: original 148,203; countdown 121,218; next unroll target <70,000. */
    CHECK(cycles < 70000, "Full priority clear exceeded the 70000-cycle target (baseline 121218)");
}

static void test_priority_page_boundaries(const char *graphics_path) {
    static const unsigned sizes[] = {0, 1, 7, 8, 9, 128, 256, 384};
    for (unsigned test = 0; test < sizeof(sizes) / sizeof(sizes[0]); ++test) {
        unsigned size = sizes[test];
        assemble(graphics_path, size, 2008);
        memset(vram, 0x5A, sizeof(vram)); writes = 0;
        run_helper("runPriority", 1000000);
        CHECK(writes == size, "Priority clear page-boundary variant wrote the wrong byte count");
        CHECK(vram[PRIORITY_START - 1] == 0x5A && vram[PRIORITY_START + size] == 0x5A,
              "Priority clear page-boundary variant wrote outside its range");
        for (unsigned i = 0; i < size; ++i)
            CHECK(vram[PRIORITY_START + i] == 0x44, "Priority clear page-boundary byte differs");
    }
}

static void test_map_boundaries(const char *graphics_path) {
    static const unsigned cells[] = {0, 1, 3, 4, 5, 256, 260};
    for (unsigned n = 0; n < sizeof(cells) / sizeof(cells[0]); ++n) {
        assemble(graphics_path, PRIORITY_SIZE, cells[n]);
        memset(vram, 0x5A, sizeof(vram)); writes = 0;
        ram[symbol("_inputLineDirty")] = 0;
        run_helper("runMap", 1000000);
        CHECK(writes == cells[n] * 2, "Map boundary variant has wrong write count");
        CHECK(vram[0xDA4F] == 0x5A && vram[0xDA50 + cells[n] * 2] == 0x5A,
              "Map boundary variant overwrote its sentinels");
        for (unsigned i = 0; i < cells[n]; ++i)
            CHECK(vram[0xDA50 + i * 2] == 0x80 && vram[0xDA51 + i * 2] == 0x10,
                  "Map boundary variant changed tile/attribute ordering");
        CHECK(ram[symbol("_inputLineDirty")] == 1, "Map variant did not invalidate input");
    }
}

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    fprintf(stderr, "Missing assembler symbol: %s\n", name); exit(1);
}

int main(int argc, char **argv) {
    const unsigned start = 36 * 160, width = 160, height = 168;
    suite_test_paths_init("graphics-clear");
    const char *graphics_path = argc > 1 ? argv[1] : "src/graphicsAsm.s";
    assemble(graphics_path, PRIORITY_SIZE, 2008); memset(vram, 0x5A, sizeof(vram));

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
    /* Four-store baseline: 142333 foreground cycles, 10392 per IRQ slice. */
    CHECK(clockticks6502 < 130000, "Background clear exceeded 130000 cycles (baseline 142333)");
    test_full_priority_clear();
    test_bounded_slices();
    test_priority_page_boundaries(graphics_path);
    test_map_boundaries(graphics_path);
    puts("Background and priority clear slices stay under 2048 writes/25000 cycles and preserve sentinels");
    return 0;
}
