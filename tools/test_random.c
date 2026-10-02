/* Native regression check using the project's X16 W65C02 core.
 * Build with X16_EMULATOR_SOURCE and CC65_HOME set:
 * cc -std=c11 -O2 -Wall -Wextra -I "$X16_EMULATOR_SOURCE/src/cpu" \
 *   tools/test_random.c "$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o /tmp/test-random
 * TMPDIR=${TMPDIR:-/tmp} CC65_HOME=/path/to/cc65 /tmp/test-random
 */
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
static uint8_t ram[65536], banks[256][0x2000], binary[8192];
static size_t binary_size;
static uint16_t binary_start;
static char temporary[4096];
static unsigned minimum_cycles = UINT32_MAX, maximum_cycles;
struct label { char name[100]; uint16_t address; };
static struct label labels[64];
static unsigned label_count;

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (address >= 0xA000 && address < 0xC000) return banks[ram[0]][address - 0xA000];
    return ram[address];
}
void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if (address >= 0xA000 && address < 0xC000) banks[ram[0]][address - 0xA000] = value;
    else ram[address] = value;
}
uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank) { (void)address; (void)bank; CHECK(false, "CPU stopped unexpectedly"); }

static uint16_t label(const char *name) {
    for (unsigned i = 0; i < label_count; ++i)
        if (!strcmp(labels[i].name, name)) return labels[i].address;
    fprintf(stderr, "Missing assembler label: %s\n", name); exit(1);
}
static void write_file(const char *path, const char *data) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create assembler input");
    CHECK(fwrite(data, 1, strlen(data), file) == strlen(data), "Cannot write assembler input");
    CHECK(!fclose(file), "Cannot close assembler input");
}
static int run_tool(char *const arguments[]) {
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) { execvp(arguments[0], arguments); perror(arguments[0]); _exit(127); }
    int status; CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}
static void assemble(void) {
    static const char source[] =
        ".include \"x16.inc\"\n"
        "RANDOM_BANK = $06\n"
        ".segment \"ZEROPAGE\"\nc_sp: .res 2\nsreg: .res 2\nsreg2: .res 2\n"
        ".segment \"CODE\"\n.import _b6RandomNumbers\n"
        ".segment \"BANKRAM0A\"\n.include \"modulus.s\"\n.include \"randomAsm.s\"\n"
        "popa:\nldy #0\nlda (c_sp),y\ninc c_sp\nbne popDone\ninc c_sp+1\npopDone:\nrts\n"
        ".export _rand8Bit,_randBetween,randBetweenAsmCall,mod8,c_sp\n"
        ".export rangeMin,rangeMax,cArgMin,readerResults,cResults,byteResults,loopIndex\n"
        ".export readerLoop,readerCall,readerAfter,readerDone,cLoop,cCall,cAfter,cDone\n"
        ".export byteLoop,byteCall,byteAfter,byteDone\n"
        "rangeMin: .byte 0\nrangeMax: .byte 0\ncArgMin: .byte 0\nloopIndex: .byte 0\n"
        "readerResults: .res 256\ncResults: .res 256\nbyteResults: .res 256\n"
        "readerLoop:\nldy #0\nreaderRepeat:\nlda rangeMin\nldx rangeMax\nreaderCall:\njsr randBetweenAsmCall\nreaderAfter:\nsta readerResults,y\niny\nbne readerRepeat\nreaderDone:\nnop\n"
        "cLoop:\nstz loopIndex\ncRepeat:\nlda #<cArgMin\nsta c_sp\nlda #>cArgMin\nsta c_sp+1\nlda rangeMax\ncCall:\njsr _randBetween\ncAfter:\nldx loopIndex\nsta cResults,x\ninc loopIndex\nbne cRepeat\ncDone:\nnop\n"
        "byteLoop:\nldy #0\nbyteRepeat:\nlda #9\nbyteCall:\njsr _rand8Bit\nbyteAfter:\nsta byteResults,y\niny\nbne byteRepeat\nbyteDone:\nnop\n"
        ".segment \"BANKRAM06\"\nfixturePadding: .res 256\n";
    static const char bank_source[] =
        ".segment \"BANKRAM06\"\n.export _b6RandomNumbers\n_b6RandomNumbers: .res 256\n";
    static const char config[] =
        "MEMORY { ZP: start=$0022,size=$005E,file=\"\"; RAM: start=$2000,size=$6000,file=%O; BANK6: start=$A000,size=$2000,file=\"\"; }\n"
        "SEGMENTS { ZEROPAGE: load=ZP,type=zp; CODE: load=RAM,type=rw; BANKRAM0A: load=BANK6,type=rw,optional=yes; BANKRAM06: load=BANK6,type=rw; }\n";
    char src[4096], bank_src[4096], cfg[4096], obj[4096], bank_obj[4096], bin[4096], lbl[4096];
    const char *home = getenv("CC65_HOME"); CHECK(home && *home, "Set CC65_HOME to a cc65 toolchain");
    CHECK(snprintf(src, sizeof(src), "%s/random.s", temporary) < (int)sizeof(src), "Path too long");
    CHECK(snprintf(bank_src, sizeof(bank_src), "%s/bank.s", temporary) < (int)sizeof(bank_src), "Path too long");
    CHECK(snprintf(cfg, sizeof(cfg), "%s/random.cfg", temporary) < (int)sizeof(cfg), "Path too long");
    CHECK(snprintf(obj, sizeof(obj), "%s/random.o", temporary) < (int)sizeof(obj), "Path too long");
    CHECK(snprintf(bank_obj, sizeof(bank_obj), "%s/bank.o", temporary) < (int)sizeof(bank_obj), "Path too long");
    CHECK(snprintf(bin, sizeof(bin), "%s/random.bin", temporary) < (int)sizeof(bin), "Path too long");
    CHECK(snprintf(lbl, sizeof(lbl), "%s/random.lbl", temporary) < (int)sizeof(lbl), "Path too long");
    write_file(src, source); write_file(bank_src, bank_source); write_file(cfg, config);
    char ca[4096], ld[4096];
    CHECK(snprintf(ca, sizeof(ca), "%s/bin/ca65", home) < (int)sizeof(ca), "Toolchain path too long");
    CHECK(snprintf(ld, sizeof(ld), "%s/bin/ld65", home) < (int)sizeof(ld), "Toolchain path too long");
    char *assemble_source[] = {ca,"-t","cx16","-I","src",src,"-o",obj,NULL};
    char *assemble_bank[] = {ca,"-t","cx16",bank_src,"-o",bank_obj,NULL};
    char *link[] = {ld,"-C",cfg,obj,bank_obj,"-o",bin,"-Ln",lbl,NULL};
    CHECK(!run_tool(assemble_source), "Random assembly failed");
    CHECK(!run_tool(assemble_bank), "Random bank fixture assembly failed");
    CHECK(!run_tool(link), "Random fixture link failed");
    FILE *file = fopen(bin, "rb"); CHECK(file, "Cannot read random fixture");
    binary_size = fread(binary, 1, sizeof(binary), file); CHECK(!ferror(file) && feof(file), "Fixture binary exceeds capacity");
    CHECK(!fclose(file), "Cannot close fixture binary"); binary_start = 0x2000;
    file = fopen(lbl, "r"); CHECK(file, "Cannot read assembler labels");
    char line[256], kind[16], name[100]; unsigned address; label_count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
        CHECK(label_count < sizeof(labels)/sizeof(*labels) && address < 65536, "Invalid assembler label");
        strcpy(labels[label_count].name, name[0] == '.' ? name + 1 : name);
        labels[label_count++].address = (uint16_t)address;
    }
    CHECK(!fclose(file), "Cannot close assembler labels");
    CHECK(label("mod8") >= 0x2000 && label("mod8") < 0x8000, "mod8 was not linked into resident CODE");
}
static void init_cpu(uint16_t pc) {
    ram[0xFFFC] = (uint8_t)pc; ram[0xFFFD] = (uint8_t)(pc >> 8);
    reset6502(false); regs.pc = pc; regs.sp = 0x1FF; regs.status = FLAG_CONSTANT;
    regs.a = regs.x = regs.y = regs.db = regs.k = 0; clockticks6502 = 0; waiting = 0;
}
static void init_case(uint8_t min, uint8_t max) {
    memset(ram, 0, sizeof(ram)); memcpy(ram + binary_start, binary, binary_size);
    memset(banks[6], 0, sizeof(banks[6]));
    const uint16_t table = label("_b6RandomNumbers"); CHECK(table >= 0xA000 && table + 256 <= 0xC000, "Random source is outside bank 6");
    for (unsigned i = 0; i < 256; ++i) banks[6][table - 0xA000 + i] = (uint8_t)i;
    ram[label("rangeMin")] = ram[label("cArgMin")] = min; ram[label("rangeMax")] = max;
    ram[0] = 0x35;
}
static void run_loop(const char *start, const char *entry, const char *after, const char *done, bool c_abi) {
    init_cpu(label(start)); bool measuring = false; uint32_t began = 0; unsigned steps = 0;
    while (regs.pc != label(done)) {
        CHECK(++steps < 200000, "Random routine failed to finish");
        if (regs.pc == label(entry)) { began = clockticks6502; measuring = true; }
        if (regs.pc == label(after)) {
            CHECK(measuring, "Random call timing state lost"); measuring = false;
            unsigned elapsed = clockticks6502 - began;
            if (!strcmp(start, "readerLoop")) {
                if (elapsed < minimum_cycles) minimum_cycles = elapsed;
                if (elapsed > maximum_cycles) maximum_cycles = elapsed;
            }
            CHECK(ram[0] == 0x35, "Random routine failed to restore RAM bank");
            CHECK(regs.sp == 0x1FF, "Random routine leaked hardware stack");
            if (c_abi) {
                uint16_t sp = label("c_sp"), arg = (uint16_t)(label("cArgMin") + 1);
                CHECK(ram[sp] == (uint8_t)arg && ram[sp + 1] == (uint8_t)(arg >> 8), "C ABI did not consume the min argument");
            }
        }
        step6502();
    }
    CHECK(!measuring && regs.sp == 0x1FF && ram[0] == 0x35, "Random loop did not preserve stack or bank");
}
static uint8_t expected(uint8_t min, uint8_t max, uint8_t value) {
    unsigned span = (unsigned)max - min + 1;
    return (uint8_t)(min + value % span);
}
static void check_range(uint8_t min, uint8_t max) {
    init_case(min, max);
    run_loop("readerLoop","readerCall","readerAfter","readerDone",false);
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[label("readerResults") + i] == expected(min, max, (uint8_t)i), "Reader ABI returned an incorrect inclusive value");
    run_loop("cLoop","cCall","cAfter","cDone",true);
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[label("cResults") + i] == expected(min, max, (uint8_t)i), "C ABI returned an incorrect inclusive value");
}
static void check_exclusive_rand8(void) {
    run_loop("byteLoop","byteCall","byteAfter","byteDone",false);
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[label("byteResults") + i] == i % 9, "rand8Bit stopped using an exclusive upper bound");
}
int main(void) {
    const char *temp = getenv("TMPDIR"); if (!temp) temp = "/tmp";
    CHECK(snprintf(temporary, sizeof(temporary), "%s/random-test-XXXXXX", temp) < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory"); assemble();
    const uint8_t ranges[][2] = {{0,0},{255,255},{0,1},{254,255},{0,255},{6,50}};
    for (unsigned i = 0; i < sizeof(ranges)/sizeof(*ranges); ++i) check_range(ranges[i][0], ranges[i][1]);
    check_exclusive_rand8();
    printf("Random checks passed across 256 source bytes; randBetweenAsmCall cost %u–%u cycles.\n", minimum_cycles, maximum_cycles);
    return 0;
}
