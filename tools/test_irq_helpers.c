/* Native IRQ-helper regression harness; build like test_screen_shake.c. */
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
static unsigned addr[2], stride[2], writes, acks;
static char temporary[4096];
struct symbol { char name[100]; unsigned addr; } symbols[64];
static unsigned symbol_count;

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

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
    if (address == 0x9F27) {
        CHECK(value == 1, "VSYNC acknowledgement must write only bit zero");
        ++acks;
        ram[address] &= (uint8_t)~value;
        return;
    }
    ram[address] = value;
}

uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank) {
    (void)address; (void)bank;
    CHECK(false, "CPU stopped unexpectedly");
}

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].addr;
    fprintf(stderr, "Missing assembler symbol: %s\n", name);
    exit(1);
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot open assembly source");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek assembly source");
    long length = ftell(file); CHECK(length >= 0, "Cannot size assembly source");
    rewind(file);
    char *contents = malloc((size_t)length + 1); CHECK(contents, "Out of memory");
    CHECK(fread(contents, 1, (size_t)length, file) == (size_t)length, "Cannot read assembly source");
    contents[length] = 0;
    CHECK(!fclose(file), "Cannot close assembly source");
    return contents;
}

static char *find(char *text, const char *needle) {
    char *match = strstr(text, needle); CHECK(match, "Assembly source marker missing"); return match;
}

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create fixture");
    CHECK(fwrite(contents, 1, strlen(contents), file) == strlen(contents), "Cannot write fixture");
    CHECK(!fclose(file), "Cannot close fixture");
}

static void run_tool(char **arguments) {
    char executable[4096];
    const char *home = getenv("CC65_HOME");
    const char *tool = arguments[0];
    if (home) CHECK(snprintf(executable, sizeof(executable), "%s/bin/%s", home, tool) < (int)sizeof(executable), "CC65 tool path too long");
    else CHECK(snprintf(executable, sizeof(executable), "%s", tool) < (int)sizeof(executable), "CC65 tool path too long");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        char *argv[16]; unsigned count = 0;
        while (arguments[count]) { CHECK(count < 15, "Too many assembler arguments"); argv[count] = arguments[count]; ++count; }
        argv[0] = executable; argv[count] = NULL;
        execv(executable, argv);
        perror(executable);
        _exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(void) {
    char source[65536], path[4096], config_path[4096], object_path[4096], binary_path[4096], labels_path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/test.s", temporary) < (int)sizeof(path), "Fixture path too long");
    CHECK(snprintf(config_path, sizeof(config_path), "%s/test.cfg", temporary) < (int)sizeof(config_path), "Fixture path too long");
    CHECK(snprintf(object_path, sizeof(object_path), "%s/test.o", temporary) < (int)sizeof(object_path), "Fixture path too long");
    CHECK(snprintf(binary_path, sizeof(binary_path), "%s/test.bin", temporary) < (int)sizeof(binary_path), "Fixture path too long");
    CHECK(snprintf(labels_path, sizeof(labels_path), "%s/test.lbl", temporary) < (int)sizeof(labels_path), "Fixture path too long");

    char *global = read_file("src/global.s");
    char *irq = read_file("src/irqAsm.s");
    char *input = read_file("src/inputIrqHandler.s");
    char *sprite = read_file("src/spriteIrqHandler.s");
    char *ack_macro = find(global, ".macro REENABLE_INTERRUPTS");
    char *ack_end = find(ack_macro, ".endmacro") + strlen(".endmacro");
    char *irq_macros = find(irq, ".macro SEND_IRQ_COMMAND");
    char *irq_end = find(irq_macros, ";Handlers");
    char *input_body = find(input, "b7HandleInputLine:");
    char *input_end = find(input_body, "\n.endif");
    char *sprite_start = find(sprite, ".macro SET_VERA_START_SPRITE_ATTRS");
    char *sprite_start_end = find(sprite_start, ".endmacro") + strlen(".endmacro");
    char *sprite_clear = find(sprite, ".macro CLEAR_SPRITE_ATTRS");
    char *sprite_clear_end = find(sprite_clear, ".endmacro") + strlen(".endmacro");

    int length = snprintf(source, sizeof(source),
        ".include \"x16.inc\"\nVIEW_TABLE_SIZE=1\n.include \"globalGraphics.s\"\nVSYNC_BIT=$01\nMAX_INPUT_STRING_LENGTH=40\n"
        "INPUT_LINE_RENDER_CELLS=MAX_INPUT_STRING_LENGTH+1\n"
        ".assert INPUT_LINE_COL + INPUT_LINE_RENDER_CELLS <= TILE_LAYER_WIDTH, error, \"Input line rendering exceeds tile map row\"\n"
        ".segment \"CODE\"\n"
        ".export input,enable,send,wait,done,snapshot,_vSyncCounter,sendIrqCommand,_inputLineDisplayed,_b7CurrentInputStr\n"
        ".export clearSprites,inputStringAddress,spriteFirstZ,spriteCount\n"
        "%.*s\n%.*s\n%.*s\n%.*s\n"
        "input:\n%.*s\n"
        "enable: REENABLE_INTERRUPTS\nrts\n"
        "send: SEND_IRQ_COMMAND command,snapshot\nrts\n"
        "wait: WAIT_FOR_NEXT_IRQ snapshot\nrts\n"
        "clearSprites: CLEAR_SPRITE_ATTRS\nrts\n"
        "done: brk\ncommand: .byte 4\nsendIrqCommand: .byte 0\nsnapshot: .word 0\n_vSyncCounter: .word 0\n"
        "_inputLineDisplayed: .byte 0\n_b7CurrentInputStr: .res 41\n"
        "inputStringAddress=INPUT_STRING_ADDRESS\nspriteFirstZ=SPRITE_ATTR_START+SPRITE_ATTR_SIZE+SA_VERA_ZORDER\nspriteCount=NO_SPRITES\n",
        (int)(ack_end - ack_macro), ack_macro,
        (int)(irq_end - irq_macros), irq_macros,
        (int)(sprite_start_end - sprite_start), sprite_start,
        (int)(sprite_clear_end - sprite_clear), sprite_clear,
        (int)(input_end - input_body), input_body);
    CHECK(length >= 0 && length < (int)sizeof(source), "Fixture assembly is too large");
    free(global); free(irq); free(input); free(sprite);

    write_file(path, source);
    write_file(config_path,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object_path, NULL};
    char *ld65[] = {"ld65", "-C", config_path, object_path, "-o", binary_path, "-Ln", labels_path, NULL};
    run_tool(ca65); run_tool(ld65);

    FILE *file = fopen(binary_path, "rb"); CHECK(file, "Cannot read linked fixture");
    CHECK(fread(ram + 0x2000, 1, 0x6000, file) > 0, "Linked fixture is empty");
    CHECK(!ferror(file), "Cannot read linked fixture"); CHECK(!fclose(file), "Cannot close linked fixture");
    file = fopen(labels_path, "r"); CHECK(file, "Cannot read assembler labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
        CHECK(symbol_count < sizeof(symbols) / sizeof(symbols[0]), "Too many assembler symbols");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name);
        symbols[symbol_count++].addr = address;
    }
    CHECK(!fclose(file), "Cannot close assembler labels");
}

static void init_cpu(unsigned pc, unsigned x) {
    reset6502(false);
    regs.pc = pc; regs.sp = 0x1FD; regs.a = regs.y = 0; regs.x = (uint8_t)x; regs.status = FLAG_CONSTANT;
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    waiting = 0; writes = acks = 0;
}

static void run(void) {
    for (unsigned steps = 0; steps < 20000 && regs.pc != symbol("done"); ++steps) {
        CHECK(!waiting, "Unexpected WAI");
        step6502();
    }
    CHECK(regs.pc == symbol("done"), "Assembly helper did not return");
}

enum { INPUT_MAX_LENGTH = 40, INPUT_RENDER_CELLS = INPUT_MAX_LENGTH + 1 };

static void seed_input_map(void) {
    unsigned base = symbol("inputStringAddress");
    for (unsigned cell = 0; cell < INPUT_RENDER_CELLS + 2; ++cell) {
        vram[base + cell * 2] = 0xA5;
        vram[base + cell * 2 + 1] = 0xA5;
    }
    vram[base + INPUT_RENDER_CELLS * 2] = 0xD1;
    vram[base + INPUT_RENDER_CELLS * 2 + 1] = 0xD2;
    vram[base + (INPUT_RENDER_CELLS + 1) * 2] = 0xE1;
    vram[base + (INPUT_RENDER_CELLS + 1) * 2 + 1] = 0xE2;
}

static void set_input(unsigned shown, unsigned length) {
    unsigned buffer = symbol("_b7CurrentInputStr");
    ram[symbol("_inputLineDisplayed")] = (uint8_t)shown;
    memset(ram + buffer, 0, INPUT_MAX_LENGTH + 1);
    for (unsigned i = 0; i < length; ++i)
        ram[buffer + i] = (uint8_t)('A' + i % 26);
}

static void check_input_map(unsigned shown, unsigned length) {
    unsigned base = symbol("inputStringAddress");
    unsigned buffer = symbol("_b7CurrentInputStr");
    for (unsigned cell = 0; cell < INPUT_RENDER_CELLS; ++cell) {
        uint8_t expected = 0x80;
        if (shown) {
            if (cell == 0) expected = 0x3E;
            else if (cell <= length) expected = ram[buffer + cell - 1];
            else if (length < INPUT_MAX_LENGTH && cell == length + 1) expected = 0x5F;
        }
        CHECK(vram[base + cell * 2] == expected, "Input line has a stale or misplaced tile");
        CHECK(vram[base + cell * 2 + 1] == 0xA5, "Input line changed a tile attribute");
    }
    CHECK(vram[base + INPUT_RENDER_CELLS * 2] == 0xD1 && vram[base + INPUT_RENDER_CELLS * 2 + 1] == 0xD2,
          "Input line overwrote the adjacent tile or attribute");
    CHECK(vram[base + (INPUT_RENDER_CELLS + 1) * 2] == 0xE1 && vram[base + (INPUT_RENDER_CELLS + 1) * 2 + 1] == 0xE2,
          "Input line overwrote the second adjacent tile or attribute");
}

static void render_input(unsigned shown, unsigned length, unsigned initial_x) {
    set_input(shown, length);
    init_cpu(symbol("input"), initial_x);
    run();
    CHECK(writes == INPUT_RENDER_CELLS + (shown ? length + 1 + (length < INPUT_MAX_LENGTH) : 0),
          "Input line clear/draw wrote the wrong number of cells");
    check_input_map(shown, length);
}

static void test_input(void) {
    for (unsigned shown = 0; shown <= 1; ++shown) {
        for (unsigned length = 0; length <= INPUT_MAX_LENGTH; ++length) {
            for (unsigned x = 0; x < 256; ++x) {
                seed_input_map();
                render_input(shown, length, x);
            }
        }
    }

    seed_input_map();
    render_input(1, INPUT_MAX_LENGTH, 0);
    render_input(1, 0, 255);
    seed_input_map();
    render_input(1, INPUT_MAX_LENGTH, 0);
    render_input(0, INPUT_MAX_LENGTH, 255);
}

static void test_acknowledgement(void) {
    for (unsigned bank = 0; bank < 256; ++bank) {
        ram[1] = (uint8_t)bank;
        ram[0x9F27] = 0xFF;
        init_cpu(symbol("enable"), 0); run();
        CHECK(acks == 1, "REENABLE_INTERRUPTS did not acknowledge VSYNC exactly once");
        CHECK(ram[0x9F27] == 0xFE, "VSYNC acknowledgement cleared unrelated pending ISR bits");
        CHECK(!(regs.status & FLAG_INTERRUPT), "REENABLE_INTERRUPTS left IRQ disabled");
    }
}

static void test_snapshot_and_wait(void) {
    ram[symbol("_vSyncCounter")] = 0xFF;
    ram[symbol("_vSyncCounter") + 1] = 0xAB;
    init_cpu(symbol("send"), 0); run();
    CHECK(ram[symbol("snapshot")] == 0xFF && ram[symbol("snapshot") + 1] == 0xAB, "SEND_IRQ_COMMAND omitted the snapshot high byte");

    /* A completion before WAI must return immediately, including low-byte rollover. */
    ram[symbol("snapshot")] = 0xFF; ram[symbol("snapshot") + 1] = 0x12;
    ram[symbol("_vSyncCounter")] = 0xFF; ram[symbol("_vSyncCounter") + 1] = 0x13;
    init_cpu(symbol("wait"), 0); run();
    CHECK(!acks, "WAIT_FOR_NEXT_IRQ acknowledged an ISR bit");

    /* A masked non-VSYNC wake retries; the following VSYNC rollover completes. */
    ram[symbol("snapshot")] = 0xFF; ram[symbol("snapshot") + 1] = 0x12;
    ram[symbol("_vSyncCounter")] = 0xFF; ram[symbol("_vSyncCounter") + 1] = 0x12;
    init_cpu(symbol("wait"), 0);
    unsigned wakes = 0;
    for (unsigned steps = 0; steps < 20000 && regs.pc != symbol("done"); ++steps) {
        if (!waiting) { step6502(); continue; }
        CHECK(regs.status & FLAG_INTERRUPT, "WAI must mask IRQ while waiting");
        struct regs before = regs;
        irq6502();
        CHECK(!waiting && regs.pc == before.pc && regs.sp == before.sp && regs.status == before.status, "Masked IRQ did not only wake WAI");
        ++wakes;
        if (wakes == 1) ram[0x9F27] = 0x02; /* Unrelated pending source; counter stays equal. */
        else {
            ram[symbol("_vSyncCounter")] = 0x00;
            ram[symbol("_vSyncCounter") + 1] = 0x13;
            ram[0x9F27] = 0x01;
        }
    }
    CHECK(regs.pc == symbol("done") && wakes == 2, "WAIT did not retry after a non-VSYNC wake and finish on rollover");
    CHECK(!acks, "WAIT_FOR_NEXT_IRQ acknowledged the pending ISR");
}

static void test_sprite_clear(void) {
    unsigned first_z = symbol("spriteFirstZ"), count = symbol("spriteCount");
    CHECK(count == 127 && first_z + (count - 1) * 8 == 0x1FFFE, "Fixture sprite table bounds differ from VERA");

    memset(vram, 0, sizeof(vram));
    vram[first_z] = 0x01; /* Flip only: no depth means disabled. */
    vram[first_z + 8] = 0x0C;
    init_cpu(symbol("clearSprites"), 0); run();
    CHECK(writes == 0 && vram[first_z] == 0x01 && vram[first_z + 8] == 0x0C, "Flip bits kept a disabled sprite active");

    memset(vram, 0, sizeof(vram));
    for (unsigned i = 0; i < count; ++i) vram[first_z + i * 8] = (uint8_t)(0x0C | (i & 1));
    vram[first_z - 8] = 0x05; /* Mouse attributes must be skipped. */
    vram[0x0006] = 0x0C; /* Active data at the wrapped address must remain untouched. */
    init_cpu(symbol("clearSprites"), 0); run();
    CHECK(writes == count, "Sprite clear exceeded the 127-entry table");
    for (unsigned i = 0; i < count; ++i)
        CHECK(vram[first_z + i * 8] == 0, "Sprite clear left an active depth entry");
    CHECK(vram[first_z - 8] == 0x05, "Sprite clear touched the mouse entry");
    CHECK(vram[0x0006] == 0x0C, "Sprite clear wrapped around VRAM");
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(temporary, sizeof(temporary), "%s/irq-helpers-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory");
    assemble();
    test_input();
    test_acknowledgement();
    test_snapshot_and_wait();
    test_sprite_clear();
    puts("IRQ helpers pass: input lengths/X/shrinking, VSYNC ack/snapshot/waits, and bounded sprite clear");
    return 0;
}
