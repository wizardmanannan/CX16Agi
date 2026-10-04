/* Native execution check for the assembled show_obj IRQ handler. */
#define _POSIX_C_SOURCE 200809L
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
static unsigned address[2], stride[2], writes;
static char temporary[4096];
static struct { char name[100]; unsigned address; } symbols[64];
static unsigned symbol_count;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s (%s:%d)\n", m, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t location, uint8_t bank) {
    (void)bank;
    if (location == 0x9F23 || location == 0x9F24) {
        unsigned port = location - 0x9F23;
        uint8_t value = vram[address[port]];
        address[port] = (address[port] + stride[port]) & 0x1FFFF;
        return value;
    }
    return ram[location];
}

void write6502(uint16_t location, uint8_t bank, uint8_t value) {
    (void)bank;
    if (location >= 0x9F20 && location <= 0x9F22) {
        static const unsigned increments[16] = {0,1,2,4,8,16,32,64,128,256,512,40,80,160,320,640};
        unsigned port = ram[0x9F25] & 1;
        ram[location] = value;
        address[port] = ram[0x9F20] | (ram[0x9F21] << 8) | ((ram[0x9F22] & 1) << 16);
        stride[port] = increments[ram[0x9F22] >> 4];
        return;
    }
    if (location == 0x9F23 || location == 0x9F24) {
        unsigned port = location - 0x9F23;
        vram[address[port]] = value;
        address[port] = (address[port] + stride[port]) & 0x1FFFF;
        ++writes;
        return;
    }
    ram[location] = value;
}

uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t location, uint8_t bank) { (void)location; (void)bank; CHECK(false, "show_obj IRQ handler stopped the CPU"); }

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create fixture");
    CHECK(fwrite(contents, 1, strlen(contents), file) == strlen(contents), "Cannot write fixture");
    CHECK(!fclose(file), "Cannot close fixture");
}

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i) if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    fprintf(stderr, "Missing assembler symbol: %s\n", name); exit(1);
}

static void assemble(void) {
    char source[8192], config[8192], object[8192], binary[8192], labels[8192], fixture[8192];
    CHECK(snprintf(source, sizeof(source), "%s/show_obj.s", temporary) < (int)sizeof(source), "Path too long");
    CHECK(snprintf(config, sizeof(config), "%s/show_obj.cfg", temporary) < (int)sizeof(config), "Path too long");
    CHECK(snprintf(object, sizeof(object), "%s/show_obj.o", temporary) < (int)sizeof(object), "Path too long");
    CHECK(snprintf(binary, sizeof(binary), "%s/show_obj.bin", temporary) < (int)sizeof(binary), "Path too long");
    CHECK(snprintf(labels, sizeof(labels), "%s/show_obj.lbl", temporary) < (int)sizeof(labels), "Path too long");
    FILE *file = fopen("src/showObjAsm.s", "rb"); CHECK(file, "Cannot read show_obj assembly");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek show_obj assembly"); long length = ftell(file); CHECK(length >= 0, "Cannot size show_obj assembly"); rewind(file);
    char *assembly = malloc((size_t)length + 1); CHECK(assembly, "Out of memory");
    CHECK(fread(assembly, 1, (size_t)length, file) == (size_t)length, "Cannot read show_obj assembly"); assembly[length] = 0; fclose(file);
    char *begin = strstr(assembly, "_b11ShowObjSpriteAddressShifted: .res 2"); CHECK(begin, "Handler data missing");
    char *end = strstr(begin, "\n.endif"); CHECK(end, "Handler end missing");
    int count = snprintf(fixture, sizeof(fixture),
        "VIEW_TABLE_SIZE=1\n.include \"global.s\"\n.include \"globalGraphics.s\"\n"
        "SHOW_OBJ_ATTRIBUTE_ADDRESS=$1FFF8\n.segment \"CODE\"\n"
        ".export runHandler,done,_b11ShowObjSpriteAddressShifted,_b11ShowObjSprAttr7,_b11ShowObjX,_b11ShowObjY\n%.*s\n"
        "runHandler: jsr b11ShowObjIrqHandler\ndone: rts\n"
        ".scope StatusFixture\n.include \"statusBar.s\"\n.endscope\n.segment \"CODE\"\n"
        "_menuShown: .byte 0\nisMenuAllowed: lda #1\nrts\nbAClearTopLine: rts\n"
        ".export runStatus,statusDisplayed,statusShowing,statusInitialized,statusScore,statusMaxScore,statusSound\n"
        "runStatus: jmp StatusFixture::b10DisplayStatusBar\n"
        "statusDisplayed=StatusFixture::_statusLineDisplayed\nstatusShowing=StatusFixture::b10StatusBarShowing\n"
        "statusInitialized=StatusFixture::totalScoreSet\nstatusScore=SET_VAR_OFFSET+SCORE_VAR\n"
        "statusMaxScore=SET_VAR_OFFSET+MAX_SCORE_VAR\nstatusSound=FLAG_OFFSET+SOUND_ON_FLAG\n",
        (int)(end - begin), begin);
    CHECK(count >= 0 && count < (int)sizeof(fixture), "Fixture too large"); free(assembly); write_file(source, fixture);
    write_file(config,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; BANKRAM10:load=RAM,type=rw,optional=yes; }\n");
    const char *home = getenv("CC65_HOME"); char ca65[4096], ld65[4096];
    if (home) { snprintf(ca65, sizeof(ca65), "%s/bin/ca65", home); snprintf(ld65, sizeof(ld65), "%s/bin/ld65", home); }
    else { strcpy(ca65, "ca65"); strcpy(ld65, "ld65"); }
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) { char *argv[] = {ca65,"-t","cx16","-I","src",source,"-o",object,NULL}; execvp(ca65,argv); perror(ca65); _exit(127); }
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status), "Assembly failed");
    child = fork(); CHECK(child >= 0, "Cannot fork linker");
    if (!child) { char *argv[] = {ld65,"-C",config,object,"-o",binary,"-Ln",labels,NULL}; execvp(ld65,argv); perror(ld65); _exit(127); }
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status), "Link failed");
    file = fopen(binary, "rb"); CHECK(file, "Cannot read linked fixture"); CHECK(fread(ram + 0x2000, 1, 0x6000, file) > 0, "Fixture is empty"); fclose(file);
    file = fopen(labels, "r"); CHECK(file, "Cannot read labels"); char line[256], kind[16], name[100]; unsigned addr;
    while (fgets(line, sizeof(line), file)) if (sscanf(line, "%15s %x %99s", kind, &addr, name) == 3) {
        CHECK(symbol_count < sizeof(symbols)/sizeof(symbols[0]), "Too many labels");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name); symbols[symbol_count++].address = addr;
    }
    fclose(file);
}

static unsigned call_status(void) {
    reset6502(false); regs.pc = symbol("runStatus"); regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT | FLAG_INTERRUPT;
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    clockticks6502 = 0; waiting = 0; writes = 0;
    ram[0] = 0x10; ram[1] = 0x37;
    for (unsigned step = 0; step < 10000 && regs.pc != symbol("done"); ++step) step6502();
    CHECK(regs.pc == symbol("done"), "Status rendering did not return within its IRQ budget");
    CHECK(ram[0] == 0x10 && ram[1] == 0x37 && regs.sp == 0x1FF,
          "Status rendering changed caller banks/stack");
    CHECK(writes <= 80, "Status rendering overran its 40-cell row");
    return clockticks6502;
}

static void check_status(unsigned score, unsigned maximum, unsigned sound_on) {
    char prefix[64];
    snprintf(prefix, sizeof(prefix), " Score:%u of %u", score, maximum);
    for (unsigned i = 0; prefix[i]; ++i)
        CHECK(vram[0xDA00 + i * 2] == (uint8_t)prefix[i],
              "Status rendering dropped a decimal digit or zero maximum");
    const char *sound = sound_on ? "Sound:On " : "Sound:Off";
    bool found = false;
    for (unsigned i = 0; i + strlen(sound) <= 40; ++i) {
        bool match = true;
        for (unsigned j = 0; sound[j]; ++j)
            if (vram[0xDA00 + (i + j) * 2] != (uint8_t)sound[j]) match = false;
        found |= match;
    }
    CHECK(found, "Status rendering lost its sound state");
    CHECK(vram[0xD9FF] == 0x5A && vram[0xDA50] == 0x5A,
          "Status rendering overwrote an adjacent row cell");
}

static void test_status(void) {
    static const unsigned maxima[] = {255, 0, 1, 9, 10, 99, 100, 101, 109, 110, 199, 200, 209};
    for (unsigned m = 0; m < sizeof(maxima) / sizeof(maxima[0]); ++m)
        for (unsigned score = 0; score < 256; ++score)
            for (unsigned sound = 0; sound < 2; ++sound) {
                memset(vram, 0x5A, sizeof(vram));
                ram[symbol("statusDisplayed")] = 1;
                ram[symbol("statusShowing")] = ram[symbol("statusInitialized")] = 0;
                ram[symbol("statusScore")] = score;
                ram[symbol("statusMaxScore")] = maxima[m];
                ram[symbol("statusSound")] = sound;
                call_status(); check_status(score, maxima[m], sound);
                CHECK(ram[symbol("statusShowing")] && ram[symbol("statusInitialized")],
                      "Status initial rendering did not publish initialized visibility");
                call_status();
                CHECK(writes == 0, "Unchanged status row redrew");
                ram[symbol("statusMaxScore")] = (maxima[m] + 1) & 255;
                call_status();
                CHECK(writes > 0, "Maximum score edit did not invalidate the status row");
                check_status(score, (maxima[m] + 1) & 255, sound);
            }
    puts("Status checks pass: every byte score, zero/decimal boundaries, maximum edits and unchanged fast path");
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(temporary, sizeof(temporary), "%s/show-obj-irq-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory"); assemble();
    memset(vram, 0x5A, sizeof(vram)); memset(address, 0, sizeof(address)); memset(stride, 0, sizeof(stride)); writes = 0;
    ram[symbol("_b11ShowObjSpriteAddressShifted")] = 0x34; ram[symbol("_b11ShowObjSpriteAddressShifted") + 1] = 0x12;
    ram[symbol("_b11ShowObjSprAttr7")] = 0xA7; ram[symbol("_b11ShowObjX")] = 0xBC; ram[symbol("_b11ShowObjX") + 1] = 0x01; ram[symbol("_b11ShowObjY")] = 0x56;
    reset6502(false); regs.pc = (uint16_t)symbol("runHandler"); regs.sp = 0x1FD; regs.status = FLAG_CONSTANT; ram[0x1FE] = (uint8_t)(symbol("done") - 1); ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8); clockticks6502 = 0; waiting = 0;
    for (unsigned step = 0; step < 1000 && regs.pc != symbol("done"); ++step) step6502();
    CHECK(regs.pc == symbol("done"), "show_obj handler did not return");
    CHECK(writes == 8, "show_obj handler did not emit exactly eight attribute bytes");
    const uint8_t expected[8] = {0x34,0x12,0xBC,0x01,0x56,0x00,0x0C,0xA7};
    for (unsigned i = 0; i < 8; ++i) CHECK(vram[0x1FFF8 + i] == expected[i], "show_obj attribute byte differs");
    CHECK(vram[0x1FFF7] == 0x5A && vram[0] == 0x5A, "show_obj handler wrote outside its eight-byte slot");
    CHECK(clockticks6502 < 1000, "show_obj handler exceeded its cycle budget");
    printf("show_obj handler returns with exactly eight VRAM writes (%u cycles)\n", (unsigned)clockticks6502);
    test_status(); return 0;
}
