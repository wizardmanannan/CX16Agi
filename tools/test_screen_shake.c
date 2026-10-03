/* Host regression harness; uses the project's native X16 W65C02 core.
 * From the repository root (set X16_EMULATOR_SOURCE to the emulator checkout):
 * cc -std=c11 -O2 -Wall -Wextra -I "$X16_EMULATOR_SOURCE/src/cpu"
 *   tools/test_screen_shake.c "$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o /tmp/test-screen-shake
 * CC65_HOME=/path/to/cc65 /tmp/test-screen-shake
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
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
static uint8_t ram[65536], allowed[65536], display[64][4];
static bool checking;
static unsigned vera_writes, window_writes, maximum_cycles, idle_min = 999, idle_max;
static char temporary[4096];
struct label { char name[100]; uint16_t address; };
static struct label labels[128];
static unsigned label_count;
static uint8_t binary[32768];
static size_t binary_size;
static uint16_t binary_start;

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (address >= 0x9F29 && address <= 0x9F2C)
        return display[ram[0x9F25] >> 1][address - 0x9F29];
    return ram[address];
}
void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if (checking) CHECK(allowed[address], "CPU wrote outside the permitted state");
    if (address >= 0x9F20 && address <= 0x9F3F) {
        CHECK(address == 0x9F25 || (address >= 0x9F29 && address <= 0x9F2C), "CPU touched VRAM or layer scroll");
        ++vera_writes;
        if (address >= 0x9F29 && address <= 0x9F2C) {
            unsigned selector = ram[0x9F25] >> 1;
            if (selector == 1) ++window_writes;
            display[selector][address - 0x9F29] = value;
            return;
        }
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
static uint16_t label(const char *name) {
    for (unsigned i = 0; i < label_count; ++i)
        if (!strcmp(labels[i].name, name)) return labels[i].address;
    fprintf(stderr, "Missing assembler label: %s\n", name); exit(1);
}
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot open input file");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek input");
    long length = ftell(file); CHECK(length >= 0, "Cannot size input");
    rewind(file);
    char *data = malloc((size_t)length + 1); CHECK(data, "Out of memory");
    CHECK(fread(data, 1, (size_t)length, file) == (size_t)length, "Cannot read input");
    data[length] = 0; CHECK(!fclose(file), "Cannot close input"); return data;
}
static void write_file(const char *path, const char *data) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot open output");
    CHECK(fwrite(data, 1, strlen(data), file) == strlen(data), "Cannot write output");
    CHECK(!fclose(file), "Cannot close output");
}
static char *find(char *text, const char *needle) {
    char *result = strstr(text, needle); CHECK(result, "Source marker missing"); return result;
}
static int run_tool(const char *name, char *const arguments[], bool reject) {
    char tool[4096]; const char *home = getenv("CC65_HOME");
    if (home) CHECK(snprintf(tool, sizeof(tool), "%s/bin/%s", home, name) < (int)sizeof(tool), "Tool path too long");
    else CHECK(snprintf(tool, sizeof(tool), "%s", name) < (int)sizeof(tool), "Tool path too long");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        if (reject) {
            char path[4096]; CHECK(snprintf(path, sizeof(path), "%s/rejected.log", temporary) < (int)sizeof(path), "Log path too long");
            CHECK(freopen(path, "w", stderr), "Cannot capture rejected build");
        }
        char *argv[16]; unsigned count = 0;
        while (arguments[count]) {
            CHECK(count < 15, "Too many tool arguments");
            argv[count] = arguments[count]; ++count;
        }
        argv[0] = tool; argv[count] = NULL;
        execvp(tool, argv); perror(tool); _exit(127);
    }
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}
static void assemble(uint16_t start, bool code_window, bool guards) {
    char source[65536], config[512], srcpath[4096], cfgpath[4096], objpath[4096], binpath[4096], lblpath[4096], logpath[4096];
    CHECK(snprintf(srcpath, sizeof(srcpath), "%s/test.s", temporary) < (int)sizeof(srcpath), "Path too long");
    CHECK(snprintf(cfgpath, sizeof(cfgpath), "%s/test.cfg", temporary) < (int)sizeof(cfgpath), "Path too long");
    CHECK(snprintf(objpath, sizeof(objpath), "%s/test.o", temporary) < (int)sizeof(objpath), "Path too long");
    CHECK(snprintf(binpath, sizeof(binpath), "%s/test.bin", temporary) < (int)sizeof(binpath), "Path too long");
    CHECK(snprintf(lblpath, sizeof(lblpath), "%s/test.lbl", temporary) < (int)sizeof(lblpath), "Path too long");
    CHECK(snprintf(logpath, sizeof(logpath), "%s/rejected.log", temporary) < (int)sizeof(logpath), "Path too long");
    char *irq = read_file("src/irqAsm.s"), *helper = read_file("src/screenShake.s");
    char *dispatch = find(find(irq, "custom_irq_handler:"), "lda VERA_isr");
    char *dispatch_branch = find(dispatch, "bne @callSoundHandler\njmp @defaultIqr");
    char *gate = find(irq, "@callSoundHandler:") + strlen("@callSoundHandler:");
    char *end = find(gate, "lda #SOUND_BANK");
    char *alias = find(end, "shakeCount = @shakeCountLoad");
    char *alias_end = find(alias, ".endif");
    const char *prefix = code_window ? ".segment \"CODE\"\n.include \"global.s\"\n.export codeWindow,cwCurrentCode,codeWindowInvalid,_codeBank\n" : ".include \"x16.inc\"\nDISPLAY_SCALE=64\nVSYNC_BIT=1\n";
    const char *reader = code_window ? ".include \"codeWindow.s\"\n" : "_loadAndIncWinCode:\ninc consumed\nlda operand\nldx #0\nrts\n";
    int length = snprintf(source, sizeof(source), "%s.segment \"CODE\"\n"
        ".export b4ShakeScreen,handleScreenShake,mainLoop,irqStart,irqDispatch,irqEntry,irqEnd,operand,consumed\n"
        ".export shakeCount,shakePhases,shakeFrames,shakeWindow,shakeShiftedWindow,shakeBorder\n"
        "irqStart:\npha\nphx\nphy\nlda $00\npha\nlda #3\nsta $00\nirqDispatch:\n"
        "%.*sbne irqEntry\njmp irqEnd\nirqEntry:\n"
        "%.*s@vSyncCounter:\n%.*sirqEnd:\npla\nsta $00\nply\nplx\npla\nrti\nmainLoop: brk\noperand: .byte 0\nconsumed: .byte 0\n_foregroundVeraDepth: .byte 0\n%s%s",
        prefix, (int)(dispatch_branch-dispatch), dispatch, (int)(end - gate), gate, (int)(alias_end - alias), alias, reader, helper);
    CHECK(length >= 0 && length < (int)sizeof(source), "Fixture source too large");
    free(irq); free(helper);
    snprintf(config, sizeof(config), "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$%04X,size=$6000,file=%%O; }\nSEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; BANKRAM04:load=RAM,type=rw; }\n", start);
    write_file(srcpath, source); write_file(cfgpath, config);
    char *ca[] = {"ca65", "-t", "cx16", "-I", "src", srcpath, "-o", objpath, NULL};
    char *ld[] = {"ld65", "-C", cfgpath, objpath, "-o", binpath, "-Ln", lblpath, NULL};
    CHECK(!run_tool("ca65", ca, false), "Assembly failed");
    CHECK(!run_tool("ld65", ld, false), "Link failed");
    FILE *file = fopen(binpath, "rb"); CHECK(file, "Cannot read assembled binary");
    binary_size = fread(binary, 1, sizeof(binary), file); CHECK(!ferror(file) && feof(file), "Fixture binary exceeds capacity"); fclose(file);
    binary_start = start;
    file = fopen(lblpath, "r"); CHECK(file, "Cannot read assembler labels");
    label_count = 0;
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
        CHECK(label_count < 128 && address < 65536, "Invalid assembler labels");
        strcpy(labels[label_count].name, name[0] == '.' ? name + 1 : name);
        labels[label_count++].address = (uint16_t)address;
    }
    fclose(file);
    if (guards) {
        char *zp = find(config, "start=$22"); memcpy(zp + 7, "7E", 2); write_file(cfgpath, config);
        CHECK(run_tool("ld65", ld, true), "KERNAL scratch placement linked");
        char *log = read_file(logpath); CHECK(strstr(log, "KERNAL scratch"), "Missing ZP assertion"); free(log);
        memcpy(zp + 7, "22", 2); write_file(cfgpath, config);
        char *scale = find(source, "DISPLAY_SCALE=64");
        size_t tail = strlen(scale + strlen("DISPLAY_SCALE=64"));
        memmove(scale + strlen("DISPLAY_SCALE=128"), scale + strlen("DISPLAY_SCALE=64"), tail + 1);
        memcpy(scale, "DISPLAY_SCALE=128", strlen("DISPLAY_SCALE=128")); write_file(srcpath, source);
        CHECK(run_tool("ca65", ca, true), "Wrong display scale assembled");
        log = read_file(logpath); CHECK(strstr(log, "Shake displacement"), "Missing scale assertion"); free(log);
        /* Restore the valid source/object before testing resident placement. */
        memmove(scale + strlen("DISPLAY_SCALE=64"), scale + strlen("DISPLAY_SCALE=128"), tail + 1);
        memcpy(scale, "DISPLAY_SCALE=64", strlen("DISPLAY_SCALE=64")); write_file(srcpath, source);
        CHECK(!run_tool("ca65", ca, false), "Cannot restore valid object");
        char *ramstart = find(config, "RAM:start=$2000"); memcpy(ramstart + strlen("RAM:start=$"), "A000", 4); write_file(cfgpath, config);
        CHECK(run_tool("ld65", ld, true), "Banked shake operand linked");
        log = read_file(logpath); CHECK(strstr(log, "resident RAM"), "Missing resident assertion"); free(log);
    }
}
static void init_memory(unsigned ctrl, const uint8_t window[4]) {
    checking = false; memset(ram, 0, sizeof(ram)); memset(display, 0, sizeof(display));
    memcpy(ram + binary_start, binary, binary_size);
    ram[0x9F25] = (uint8_t)ctrl;
    ram[0x9F27] = 1; /* VERA ISR: VSYNC pending. */
    const uint8_t control[4] = {17,64,64,37}; memcpy(display[0], control, 4); memcpy(display[1], window, 4);
}
static void init_cpu(uint16_t pc, unsigned flags) {
    ram[0xFFFC] = (uint8_t)pc; ram[0xFFFD] = (uint8_t)(pc >> 8);
    reset6502(false); regs.pc = pc; regs.sp = 0x1FF; regs.status = FLAG_CONSTANT | flags;
    regs.a = regs.x = regs.y = 0; clockticks6502 = 0; waiting = 0;
}
static unsigned interrupt(void) {
    struct regs saved = regs; uint8_t bank = ram[0];
    ram[0xFFFE] = (uint8_t)label("irqStart"); ram[0xFFFF] = (uint8_t)(label("irqStart") >> 8);
    irq6502(); CHECK(regs.sp == (uint16_t)(saved.sp - 3), "IRQ failed to stack CPU state");
    unsigned began = 0, elapsed = 0; bool entered = false, ended = false, returned = false;
    bool vsync = (ram[0x9F27] & 1) != 0;
    for (unsigned i = 0; i < 220; ++i) {
        if (!vsync && regs.pc == label("irqDispatch")) { began = clockticks6502; entered = true; }
        if (regs.pc == label("irqEntry")) { CHECK(!(regs.status & FLAG_DECIMAL), "IRQ did not clear D"); began = clockticks6502; entered = true; }
        if (regs.pc == label("irqEnd")) { elapsed = clockticks6502 - began; ended = true; }
        step6502();
        if (regs.pc == saved.pc) { returned = true; break; }
    }
    CHECK(returned && entered && ended, "IRQ failed to return through RTI");
    CHECK(regs.pc == saved.pc && regs.sp == saved.sp && regs.a == saved.a && regs.x == saved.x && regs.y == saved.y && (regs.status & 0xCF) == (saved.status & 0xCF), "IRQ corrupted foreground CPU state");
    CHECK(ram[0] == bank, "IRQ corrupted RAM bank"); CHECK(elapsed < 250, "IRQ cycle budget exceeded"); return elapsed;
}
static void permit(uint16_t address, unsigned count) { for (unsigned i = 0; i < count; ++i) allowed[address + i] = 1; }
static void run_case(unsigned count, unsigned ctrl, const uint8_t window[4], bool reuse, unsigned flags) {
    if (!reuse) init_memory(ctrl, window);
    checking = false; ram[label("shakeFrames")] = 0xA5; ram[label("consumed")] = 0; ram[label("operand")] = (uint8_t)count; ram[0] = 4;
    vera_writes = window_writes = 0; memset(allowed, 0, sizeof(allowed));
    permit(0x100, 256); permit(0, 1); permit(0xFFFE, 2); permit(0x9F25, 1); permit(0x9F29, 4);
    const char *states[] = {"operand","consumed","shakeCount","shakePhases","shakeFrames","shakeBorder"};
    for (unsigned i = 0; i < sizeof(states)/sizeof(*states); ++i) permit(label(states[i]), 1);
    permit(label("shakeWindow"), 4); permit(label("shakeShiftedWindow"), 4);
    init_cpu(label("b4ShakeScreen"), flags); checking = true;
    unsigned frames = 0, steps = 0;
    while (regs.pc != label("mainLoop")) {
        CHECK(++steps < 100000, "Command failed to return"); step6502();
        if (!waiting) continue;
        CHECK(regs.status & FLAG_INTERRUPT, "WAI must be masked");
        struct regs saved = regs; unsigned cycles = clockticks6502;
        irq6502();
        CHECK(regs.pc == saved.pc && regs.sp == saved.sp && regs.status == saved.status && clockticks6502 == cycles, "Masked IRQ changed CPU state");
        CHECK(!waiting, "Masked IRQ did not wake WAI"); CHECK(ram[regs.pc] == 0x58, "WAI must be followed by CLI"); step6502();
        CHECK(!(regs.status & FLAG_INTERRUPT), "CLI failed to enable IRQ"); regs.status = (regs.status & ~0xCB) | flags;
        if (count == 1) {
            uint8_t state[12]; state[0] = ram[label("shakeCount")]; state[1] = ram[label("shakeFrames")]; memcpy(state + 2, ram + label("shakePhases"), 10);
            unsigned writes = vera_writes;
            ram[0x9F27] = 2; /* Raster pending, without VSYNC. */
            CHECK(interrupt() == 11, "Non-VSYNC dispatch timing differs");
            ram[0x9F27] = 1;
            CHECK(state[0] == ram[label("shakeCount")] && state[1] == ram[label("shakeFrames")] && !memcmp(state+2, ram+label("shakePhases"), 10), "Unrelated IRQ advanced shake");
            CHECK(vera_writes == writes, "Unrelated IRQ touched VERA");
        }
        unsigned elapsed = interrupt(); if (elapsed > maximum_cycles) maximum_cycles = elapsed;
        CHECK(ram[0x9F25] == ctrl, "IRQ did not restore VERA CTRL");
        for (unsigned i = 0; i < 4; ++i) {
            unsigned expected = window[i];
            if (frames < count * 32 && (frames / 4) % 2 == 0 && window[(i/2)*2+1] <= 251) expected += 4;
            CHECK(display[1][i] == expected, "Shake phase window or duration differs");
        }
        CHECK(display[0][3] == (frames < count * 32 ? 0 : 37), "Shake border timing differs"); ++frames;
    }
    CHECK(ram[label("consumed")] == 1, "Operand was not consumed once");
    CHECK(!memcmp(display[1], window, 4), "Display window was not restored");
    const uint8_t control[4] = {17,64,64,37}; CHECK(!memcmp(display[0], control, 4), "Display controls were not restored");
    CHECK(frames == (count ? count * 32 + 1 : 0), "Shake repeat count differs");
    CHECK(window_writes == count * 32, "Window writes occurred outside phase changes");
    CHECK(count || !vera_writes, "Zero count touched VERA"); CHECK(regs.sp == 0x1FF, "Command leaked stack");
    unsigned idle = interrupt(); CHECK(idle >= 12 && idle <= 14, "Idle ownership/shake gate cost changed");
    if (idle < idle_min) idle_min = idle;
    if (idle > idle_max) idle_max = idle;
    checking = false;
}
static void check_code_window(void) {
    const unsigned indices[] = {0,47,48}; const uint8_t window[4] = {0,160,0,240};
    for (unsigned n = 0; n < 3; ++n) {
        unsigned index = indices[n]; init_memory(0, window); ram[0] = 4; ram[label("_codeBank")] = 7;
        ram[0xA9] = 0; ram[0xAA] = 0x60;
        for (unsigned i = 0; i < 100; ++i) ram[0x6000+i] = (uint8_t)i;
        ram[label("cwCurrentCode")] = (uint8_t)index; ram[label("codeWindowInvalid")] = 0;
        memset(ram + label("codeWindow"), 255, 50); ram[label("codeWindow")+index] = 2;
        init_cpu(label("b4ShakeScreen"), 0); unsigned frames = 0, steps = 0;
        while (regs.pc != label("mainLoop")) {
            CHECK(++steps < 10000, "Actual code-window command failed to return"); step6502();
            if (waiting) { irq6502(); step6502(); CHECK(ram[label("shakeCount")] > 0, "Nonzero operand did not block"); interrupt(); ++frames; }
        }
        CHECK(frames == 65 && !ram[label("shakeCount")] && regs.sp == 0x1FF && ram[0] == 4, "Actual reader shake count/bank/stack differs");
        CHECK(ram[label("cwCurrentCode")] == (index == 48 ? 0 : index + 1), "Reader cursor did not advance");
        CHECK(ram[0xA9] == (index == 48 ? 49 : 0), "Reader pointer did not advance");
        if (index == 48) for (unsigned i = 0; i < 50; ++i) CHECK(ram[label("codeWindow")+i] == i+49, "Reader window did not refresh");
    }
}
static void check_dispatch(void) {
    char *source = read_file("src/logicCommands.s"), *table = find(source, "jmpTableCommands1:");
    unsigned index = 0; char *cursor = table;
    while ((cursor = strstr(cursor, ".addr"))) {
        char name[100]; CHECK(sscanf(cursor + 5, "%99s", name) == 1, "Invalid command table");
        if (index++ == 110) { CHECK(!strcmp(name, "b4ShakeScreen"), "Shake opcode dispatch changed"); break; }
        cursor += 5;
    }
    CHECK(index == 111, "Shake opcode missing"); free(source);
    source = read_file("src/commandLoop.s"); cursor = find(source, "codeBankArray: .byte") + strlen("codeBankArray: .byte");
    for (unsigned i = 0; i < 110; ++i) cursor = find(cursor, ",") + 1;
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    CHECK(strtoul(cursor + (*cursor == '$'), NULL, *cursor == '$' ? 16 : 10) == 4, "Shake code bank changed"); free(source);
    source = read_file("src/irqAsm.s"); cursor = find(source, "@callSoundHandler:");
    CHECK(find(cursor, "jsr handleScreenShake") < find(cursor, "jsr bBSoundHandler"), "Shake must precede sound"); free(source);
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--repo-root")) {
        CHECK(!chdir(argv[2]), "Cannot enter repository root");
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--repo-root PATH]\n", argv[0]);
        return 2;
    }
    const char *temp = getenv("TMPDIR"); if (!temp) temp = "/tmp";
    CHECK(snprintf(temporary, sizeof(temporary), "%s/shake-test-c-XXXXXX", temp) < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory");
    const uint16_t pcs[] = {0x2000,0x2000,0x20FF};
    const uint8_t ops[][3] = {{0xCE,0,0x60},{0x80,0,0},{0x80,0xFD,0}};
    const unsigned costs[] = {6,3,4};
    for (unsigned i = 0; i < 3; ++i) { memset(ram,0,sizeof(ram)); memcpy(ram+pcs[i],ops[i],3); init_cpu(pcs[i],0); step6502(); CHECK(clockticks6502 == costs[i], "W65C02 instruction timing differs"); }
    check_dispatch(); assemble(0x2000, false, true);
    const uint8_t window[4] = {0,160,0,240}; const unsigned flag_bits[] = {1,2,8,64,128};
    for (unsigned combination = 0; combination < 32; ++combination) { unsigned flags = 0; for (unsigned i=0;i<5;++i) if(combination&(1u<<i)) flags|=flag_bits[i]; run_case(1,0,window,false,flags); }
    const unsigned controls[] = {0,1,2,3,126,127}, counts[] = {0,1,2,255}, sequential[] = {1,0,2,1};
    const uint8_t extremes[][4] = {{11,200,15,251},{0,252,0,255},{0,160,0,255}};
    for (unsigned c=0;c<6;++c) {
        for (unsigned i=0;i<4;++i) run_case(counts[i],controls[c],window,false,0);
        for (unsigned i=0;i<3;++i) run_case(2,controls[c],extremes[i],false,0);
        for (unsigned i=0;i<4;++i) run_case(sequential[i],controls[c],window,i!=0,0);
    }
    for (unsigned offset=0;offset<256;++offset) { assemble((uint16_t)(0x2000+offset),false,false); run_case(1,0,window,false,0); }
    assemble(0x2000,true,false); check_code_window();
    printf("Screen shake checks passed; idle %u–%u cycles, maximum %u cycles including gate and JSR, excluding IRQ wrapper.\n",idle_min,idle_max,maximum_cycles);
    return 0;
}
