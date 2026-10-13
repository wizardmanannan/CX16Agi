/* Native CPU regression for the assembled display-text IRQ helper. */
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

#define NEW_LINE 10

extern struct regs regs;
static uint8_t ram[65536], vram[131072], text[531], image[0x6000];
static size_t image_size;
static unsigned addr[2], stride[2], writes;
static uint32_t max_text_slice_cycles, max_palette_slice_cycles;
static bool wrapped_text_overlay;
struct symbol { char name[100]; unsigned addr; } symbols[64];
static unsigned symbol_count;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s (%s:%d)\n", m, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    /* Supply wrapped string bytes without overwriting the CPU's bank registers. */
    if (wrapped_text_overlay && address < 0x80) return text[384 + address];
    if (address >= 0x9F20 && address <= 0x9F22) {
        unsigned port = ram[0x9F25] & 1;
        if (address == 0x9F20) return (uint8_t)addr[port];
        if (address == 0x9F21) return (uint8_t)(addr[port] >> 8);
        return (uint8_t)((ram[address] & 0xFE) | ((addr[port] >> 16) & 1));
    }
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

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i) if (!strcmp(name, symbols[i].name)) return symbols[i].addr;
    fprintf(stderr, "Missing assembler symbol: %s\n", name); exit(1);
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot open assembly source");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek assembly source");
    long length = ftell(file); CHECK(length >= 0, "Cannot size assembly source"); rewind(file);
    char *contents = malloc((size_t)length + 1); CHECK(contents, "Out of memory");
    CHECK(fread(contents, 1, (size_t)length, file) == (size_t)length, "Cannot read assembly source");
    contents[length] = 0; CHECK(!fclose(file), "Cannot close assembly source"); return contents;
}

static char *find(char *text, const char *needle) {
    char *match = strstr(text, needle); CHECK(match, "Assembly source marker missing"); return match;
}

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create assembler fixture");
    CHECK(fwrite(contents, 1, strlen(contents), file) == strlen(contents), "Cannot write assembler fixture");
    CHECK(!fclose(file), "Cannot close assembler fixture");
}

static void run_tool(char **arguments) {
    char executable[4096]; const char *home = getenv("CC65_HOME");
    if (home) CHECK(snprintf(executable, sizeof(executable), "%s/bin/%s", home, arguments[0]) < (int)sizeof(executable), "Tool path too long");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        char *argv[12]; unsigned count = 0;
        while (arguments[count]) { CHECK(count < 11, "Too many assembler arguments"); argv[count] = arguments[count]; ++count; }
        if (home) argv[0] = executable;
        argv[count] = NULL;
        if (home) execv(executable, argv); else execvp(arguments[0], argv);
        perror(arguments[0]); _exit(127);
    }
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(void) {
    char path[8192], cfg[8192], object[8192], binary[8192], labels[8192], fixture[16000];
    suite_test_path(path, sizeof(path), suite_fixture_directory, "text.s"); suite_test_path(cfg, sizeof(cfg), suite_fixture_directory, "text.cfg");
    suite_test_path(object, sizeof(object), suite_output_directory, "text.o"); suite_test_path(binary, sizeof(binary), suite_output_directory, "text.bin");
    suite_test_path(labels, sizeof(labels), suite_output_directory, "text.lbl");
    char *irq = read_file("src/irqAsm.s");
    char *data = find(irq, "_b3PaletteAddress: .res 4");
    char *palette_end = find(data, "_b3InitLayer1Mapbase:");
    char *text_code = find(palette_end, "handleDisplayTextSlice:");
    char *text_data = find(text_code, "_displayTextAddressToCopyTo: .word $0");
    char *end = find(text_data, ".segment \"BANKRAM06\"");
    int n = snprintf(fixture, sizeof(fixture),
        "VIEW_TABLE_SIZE=1\n.include \"global.s\"\n.include \"globalGraphics.s\"\n"
        ".export runInit,runText,runPalette,done,b3InitDisplayTextSlice,handleDisplayTextSlice,b3SetPaletteSlice\n"
        ".export _b3PaletteAddress,_b3PaletteRows,_b3PaletteNumber,displayTextSourcePointer,displayTextVeraAddress,displayPaletteCursor\n"
        ".export _displayTextAddressToCopyTo,_displayTextAddressToCopyToHigh,_currentTextBuffer\n"
        "ZP_DISPLAY_TEXT_SOURCE_POINTER=$D3\n"
        ".segment \"BANKRAM03\"\n"
        "%.*s\n%.*s\n%.*s"
        ".segment \"CODE\"\nrunInit: jsr b3InitDisplayTextSlice\nrts\n"
        "runText: jsr handleDisplayTextSlice\nrts\n"
        "runPalette: jsr b3SetPaletteSlice\nrts\n"
        "done: rts\n",
        (int)(palette_end - data), data,
        (int)(text_data - text_code), text_code, (int)(end - text_data), text_data);
    CHECK(n >= 0 && n < (int)sizeof(fixture), "Assembler fixture too large");
    write_file(path, fixture);
    write_file(cfg,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; BANKRAM03:load=RAM,type=rw; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object, NULL};
    char *ld65[] = {"ld65", "-C", cfg, object, "-o", binary, "-Ln", labels, NULL};
    run_tool(ca65); run_tool(ld65);
    FILE *file = fopen(binary, "rb"); CHECK(file, "Cannot read linked fixture");
    image_size = fread(image, 1, sizeof(image), file);
    CHECK(image_size > 0, "Linked fixture is empty");
    memcpy(ram + 0x2000, image, image_size);
    CHECK(!fclose(file), "Cannot close linked fixture");
    file = fopen(labels, "r"); CHECK(file, "Cannot read assembler labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) if (sscanf(line, "%15s %x %99s", kind, &address, name) == 3) {
        CHECK(symbol_count < sizeof(symbols) / sizeof(symbols[0]), "Too many assembler symbols");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name); symbols[symbol_count++].addr = address;
    }
    CHECK(!fclose(file), "Cannot close assembler labels"); free(irq);
}

static void prepare_ram(void) {
    memset(ram, 0, sizeof(ram));
    memcpy(ram + 0x2000, image, image_size);
    memset(vram, 0x5A, sizeof(vram));
    memset(addr, 0, sizeof(addr));
    memset(stride, 0, sizeof(stride));
    writes = 0;
    wrapped_text_overlay = false;
    ram[0] = 3;
    ram[1] = 0x74;
}

static void set_long(unsigned address, unsigned value) {
    ram[address] = (uint8_t)value;
    ram[address + 1] = (uint8_t)(value >> 8);
    ram[address + 2] = (uint8_t)(value >> 16);
    ram[address + 3] = (uint8_t)(value >> 24);
}

static unsigned get_17bit(unsigned address) {
    return ram[address] | (ram[address + 1] << 8) | ((ram[address + 2] & 1) << 16);
}

static unsigned get_16bit(unsigned address) {
    return ram[address] | (ram[address + 1] << 8);
}

static uint32_t run_entry(const char *entry, uint8_t carry) {
    reset6502(false);
    regs.pc = symbol(entry);
    regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT | FLAG_INTERRUPT | (carry ? FLAG_CARRY : 0);
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    waiting = 0;
    uint32_t start = clockticks6502;
    for (unsigned step = 0; step < 500000 && regs.pc != symbol("done"); ++step) {
        CHECK(!waiting, "Unexpected WAI");
        step6502();
    }
    CHECK(regs.pc == symbol("done"), "Display-text slice did not return");
    return clockticks6502 - start;
}

static void load_text(uint16_t source, const uint8_t *bytes, unsigned length) {
    wrapped_text_overlay = source == 0xFE80;
    for (unsigned i = 0; i <= length; ++i) {
        uint16_t address = (uint16_t)(source + i);
        uint8_t value = i == length ? 0 : bytes[i];
        text[i] = value;
        if (!wrapped_text_overlay || address >= 0x80) ram[address] = value;
    }
}

static void clobber_irq_registers(void) {
    ram[0x9F20] = 0x67;
    ram[0x9F21] = 0x45;
    ram[0x9F22] = 0xF1;
    ram[0x9F25] = 1;
    addr[0] = 0x12345;
    stride[0] = 640;
    ram[0xD3] = 0x34;
    ram[0xD4] = 0x12;
}

static void set_text_config(uint16_t source, unsigned destination) {
    set_long(symbol("_currentTextBuffer"), source);
    set_long(symbol("_displayTextAddressToCopyTo"), destination | 0xA5000000u);
    set_long(symbol("_b3PaletteAddress"), 0x1F000 | 0x5A000000u);
    ram[symbol("_b3PaletteRows")] = 0;
    ram[symbol("_b3PaletteNumber")] = 0;
}

static void check_text(const uint8_t *bytes, unsigned length, uint16_t source,
                       unsigned destination, uint8_t carry) {
    prepare_ram();
    load_text(source, bytes, length);
    set_text_config(source, destination);
    wrapped_text_overlay = source == 0xFE80;
    run_entry("runInit", carry);

    unsigned cursor = destination & 0x1FFFF;
    unsigned origin = cursor;
    unsigned characters = 0, newlines = 0, calls = 0;
    for (;;) {
        if (calls) clobber_irq_registers();
        unsigned beforeWrites = writes;
        unsigned beforeSource = get_16bit(symbol("displayTextSourcePointer"));
        uint32_t cycles = run_entry("runText", carry);
        unsigned remaining = length - calls * 512;
        unsigned expectedConsumed = remaining > 512 ? 512 : remaining;
        CHECK(((get_16bit(symbol("displayTextSourcePointer")) - beforeSource) & 0xFFFF) == expectedConsumed,
              "Text slice consumed the wrong number of source bytes, including newlines");
        CHECK((regs.a != 0) == (remaining < 512), "Text slice completion disagrees with its byte budget");
        if (cycles > max_text_slice_cycles) max_text_slice_cycles = cycles;
        unsigned sliceWrites = writes - beforeWrites;
        CHECK(sliceWrites <= 512, "Text helper exceeded its 512-byte output budget");
        CHECK(cycles < 90000, "Text helper exceeded its per-slice cycle bound");
        ++calls;
        if (regs.a) break;
        CHECK(calls < 10, "Text helper did not consume a long string incrementally");
    }

    CHECK(calls == length / 512 + 1, "Text slice call count did not include the final NUL probe");
    for (unsigned i = 0; i < length; ++i) {
        if (bytes[i] == NEW_LINE) {
            ++newlines;
            origin = (origin + 128) & 0x1FFFF;
            cursor = origin;
        } else {
            CHECK(vram[cursor] == bytes[i], "Text tile differs across a page, VERA wrap, or slice boundary");
            CHECK(vram[(cursor + 1) & 0x1FFFF] == 0x5A, "Text helper changed a tile attribute");
            ++characters;
            cursor = (cursor + 2) & 0x1FFFF;
        }
    }
    wrapped_text_overlay = false;
    CHECK(writes == characters, "Text helper wrote the wrong number of tile bytes");
    CHECK(get_17bit(symbol("_displayTextAddressToCopyTo")) == ((destination + newlines * 128) & 0x1FFFF),
          "Newline origin did not persist across text slices");
    CHECK(ram[symbol("_displayTextAddressToCopyTo") + 3] == 0xA5,
          "Newline handling changed the unused fourth origin byte");
    CHECK(get_16bit(symbol("displayTextSourcePointer")) == ((source + length) & 0xFFFF),
          "Private source pointer did not persist to the terminator");
    CHECK(ram[symbol("_currentTextBuffer")] == (uint8_t)source &&
          ram[symbol("_currentTextBuffer") + 1] == (uint8_t)(source >> 8),
          "Text helper changed the public source pointer");
    CHECK(addr[0] == cursor, "Text helper did not restore and persist the VERA cursor");
    CHECK(get_17bit(symbol("displayTextVeraAddress")) == cursor,
          "Private text VERA cursor did not persist the hardware address");
    CHECK(ram[0] == 3 && ram[1] == 0x74, "Text helper changed RAM or ROM bank");
    CHECK((read6502(0x9F22, 0) & 0x31) == (0x20 | ((cursor >> 16) & 1)) && ram[0x9F25] == 0,
          "Text helper left the wrong VERA bank, stride, or address port");
    CHECK((regs.status & (FLAG_INTERRUPT | FLAG_DECIMAL)) == FLAG_INTERRUPT,
          "Text helper changed the IRQ/decimal flag contract");
}

static void test_text_slices(void) {
    uint8_t longText[531];
    for (unsigned i = 0; i < 530; ++i)
        longText[i] = i % 48 == 47 ? NEW_LINE : (uint8_t)('A' + i % 26);
    longText[255] = 1;
    longText[511] = 'Z';
    check_text(longText, 530, 0xFE80, 0x1FFF0, 0);
    check_text(longText, 530, 0xFE80, 0x1FFF0, 1);

    uint8_t boundary[515];
    memset(boundary, 'Q', sizeof(boundary));
    boundary[511] = NEW_LINE;
    check_text(boundary, 514, 0x8400, 0x1FFF0, 0);
    boundary[511] = 'Q';
    boundary[512] = NEW_LINE;
    check_text(boundary, 514, 0x8400, 0x1FFF0, 1);

    memset(boundary, 'R', sizeof(boundary));
    check_text(boundary, 511, 0x8400, 0x5000, 0); /* NUL at byte 511 */
    check_text(boundary, 512, 0x8400, 0x5000, 1); /* NUL at byte 512 */

    static const unsigned shortLengths[] = {0, 255, 256, 257};
    memset(boundary, 'S', sizeof(boundary));
    for (unsigned n = 0; n < sizeof(shortLengths) / sizeof(shortLengths[0]); ++n)
        check_text(boundary, shortLengths[n], 0x8400, 0x1FFF0, n & 1);
    boundary[255] = boundary[256] = NEW_LINE;
    check_text(boundary, 257, 0x8400, 0x1FFF0, 1);

    memset(boundary, NEW_LINE, 512);
    check_text(boundary, 512, 0x8400, 0x1FFF0, 0); /* Worst-case newline-heavy slice */
}

static void configure_palette(unsigned address, uint8_t rows, uint8_t number) {
    set_long(symbol("_currentTextBuffer"), 0x7000);
    set_long(symbol("_displayTextAddressToCopyTo"), 0x4000);
    set_long(symbol("_b3PaletteAddress"), address | 0x5A000000u);
    ram[symbol("_b3PaletteRows")] = rows;
    ram[symbol("_b3PaletteNumber")] = number;
}

static void test_palette(unsigned address, uint8_t rows, uint8_t carry) {
    prepare_ram();
    configure_palette(address, rows, 0xA);
    run_entry("runInit", carry);
    unsigned completedRows = 0, calls = 0;
    for (;;) {
        if (calls) clobber_irq_registers();
        unsigned beforeWrites = writes;
        uint32_t cycles = run_entry("runPalette", carry);
        if (cycles > max_palette_slice_cycles) max_palette_slice_cycles = cycles;
        unsigned expectedRows = rows - completedRows;
        if (expectedRows > 32) expectedRows = 32;
        CHECK(writes - beforeWrites == expectedRows * 64,
              "Palette helper wrote an incomplete or oversized row slice");
        CHECK(writes - beforeWrites <= 2048, "Palette helper exceeded 2048 writes in one slice");
        CHECK(cycles < 30000, "Palette helper exceeded its per-slice cycle bound");
        completedRows += expectedRows;
        CHECK(get_17bit(symbol("displayPaletteCursor")) == ((address + completedRows * 128) & 0x1FFFF),
              "Palette cursor did not advance by exactly the consumed row slice");
        ++calls;
        CHECK((regs.a != 0) == (completedRows == rows), "Palette completion result is wrong");
        if (regs.a) break;
        CHECK(calls < 10, "Palette helper did not make bounded progress");
    }

    unsigned cursor = address & 0x1FFFF;
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned byte = 0; byte < 64; ++byte) {
            unsigned at = (cursor + row * 128 + byte * 2) & 0x1FFFF;
            CHECK(vram[at] == 0xA0, "Palette helper wrote the wrong color at a wrapped address");
            CHECK(vram[(at + 1) & 0x1FFFF] == 0x5A, "Palette helper changed stride padding");
        }
    }
    CHECK(get_17bit(symbol("displayPaletteCursor")) == ((cursor + rows * 128) & 0x1FFFF),
          "Private palette cursor did not preserve 17-bit carry");
    CHECK(ram[0] == 3 && ram[1] == 0x74, "Palette helper changed RAM or ROM bank");
}

static void test_zero_palette(void) {
    prepare_ram();
    configure_palette(0x1F000, 0, 2);
    run_entry("runInit", 1);
    addr[0] = 0x12345;
    stride[0] = 4;
    ram[0x9F20] = 0x45;
    ram[0x9F21] = 0x23;
    ram[0x9F22] = 0x31;
    ram[0x9F25] = 1;
    unsigned beforeWrites = writes;
    run_entry("runPalette", 1);
    CHECK(regs.a == 1, "Zero-row palette did not report completion");
    CHECK(writes == beforeWrites, "Zero-row palette emitted VERA writes");
    CHECK(addr[0] == 0x12345 && stride[0] == 4 && ram[0x9F25] == 1,
          "Zero-row palette changed the VERA cursor or selected port");
}

int main(void) {
    suite_test_paths_init("text-irq");
    assemble();
    test_text_slices();
    test_palette(0x1F000, 255, 0);
    test_palette(0x1F000, 255, 1);
    test_palette(0xDA01, 64, 0);
    test_palette(0x1FFF1, 1, 0);
    test_palette(0x1FFF1, 31, 1);
    test_palette(0x1FFF1, 32, 0);
    test_palette(0x1FFF1, 33, 1);
    test_zero_palette();
    fprintf(stderr, "Text slice: %lu cycles (baselines 67215/56469; limit 55000)\n", (unsigned long)max_text_slice_cycles);
    CHECK(max_text_slice_cycles < 55000, "Text slice exceeded 55000 cycles (baseline 56469)");
    fprintf(stderr, "Palette slice: %lu cycles (baselines 20236/11276; limit 11000)\n", (unsigned long)max_palette_slice_cycles);
    CHECK(max_palette_slice_cycles < 11000, "Palette slice exceeded 11000 cycles (baseline 11276)");
    CHECK(max_text_slice_cycles + 38000 < 134400,
          "Text slice plus conservative other IRQ work exceeds a VSYNC frame");
    CHECK(max_palette_slice_cycles + 38000 < 134400,
          "Palette slice plus conservative other IRQ work exceeds a VSYNC frame");
    printf("Display-text and palette slices pass (max cycles: text %lu, palette %lu)\n",
           (unsigned long)max_text_slice_cycles, (unsigned long)max_palette_slice_cycles);
    return 0;
}
