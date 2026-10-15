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
#define _MEMORYMANAGER_H_
#define _LOGIC_H_
#define _PARSER_H_
#include "../src/menu.h"
#include "test_paths.h"

extern struct regs regs;
static uint8_t ram[65536], vram[131072];
static unsigned address[2], stride[2], writes;
static struct { char name[100]; unsigned address; } symbols[64];
static unsigned symbol_count;
static bool fx_write;
static uint8_t fx_cache[4];
static bool track_menu_reads;
static unsigned menu_records, menu_record_limit, menu_record_end;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s (%s:%d)\n", m, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t location, uint8_t bank) {
    (void)bank;
    if (track_menu_reads && location >= menu_records && location < menu_record_end)
        CHECK(location < menu_record_limit, "Menu renderer read beyond its published child count");
    if (location >= 0x9F20 && location <= 0x9F22) {
        unsigned port = ram[0x9F25] & 1;
        if (location == 0x9F20) return address[port];
        if (location == 0x9F21) return address[port] >> 8;
        return (ram[0x9F22] & 0xFE) | (address[port] >> 16);
    }
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
    if (location >= 0x9F29 && location <= 0x9F2C && (ram[0x9F25] >> 1) == 6) {
        fx_cache[location - 0x9F29] = value; return;
    }
    if (location == 0x9F29 && (ram[0x9F25] >> 1) == 2) {
        fx_write = (value & 0x40) != 0; return;
    }
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
        if (fx_write) {
            CHECK(value == 0 && (address[port] & 3) == 0, "Unexpected masked/unaligned FX menu write");
            for (unsigned i = 0; i < 4; ++i) vram[(address[port] + i) & 0x1FFFF] = fx_cache[i];
            writes += 4;
        } else {
            vram[address[port]] = value; ++writes;
        }
        address[port] = (address[port] + stride[port]) & 0x1FFFF;
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

static char *read_source(const char *path) {
    FILE *file = fopen(path, "rb"); CHECK(file, "Cannot read production assembly");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek production assembly");
    long length = ftell(file); CHECK(length >= 0, "Cannot size production assembly"); rewind(file);
    char *text = malloc((size_t)length + 1); CHECK(text, "Out of memory");
    CHECK(fread(text, 1, length, file) == (size_t)length, "Cannot load production assembly");
    text[length] = 0; CHECK(!fclose(file), "Cannot close production assembly"); return text;
}

static void append_menu_fixture(const char *path) {
    char *text = read_source("src/menuAsm.s");
    char *constants = strstr(text, "MENU_SREG ="), *constants_end = constants ? strstr(constants, ".segment \"CODE\"") : NULL;
    char *helpers = strstr(text, "bAClearTopLine:"), *helpers_end = helpers ? strstr(helpers, "bAPrintMenuChildText:") : NULL;
    char *printer = strstr(text, "bAPrintMenuText:"), *printer_end = printer ? strstr(printer, "MENU_BAR_LOCATION =") : NULL;
    char *locations_end = printer_end ? strstr(printer_end, "\n\n") : NULL;
    char *bar = strstr(text, "bADisplayMenuBar:"), *bar_end = bar ? strstr(bar, ".endif") : NULL;
    CHECK(constants_end && helpers_end && locations_end && bar_end, "Menu assembly boundaries changed");
    FILE *file = fopen(path, "a"); CHECK(file, "Cannot extend owned IRQ fixture");
    fprintf(file, ".scope MenuFixture\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n",
        (int)(constants_end - constants), constants,
        (int)(locations_end - printer_end), printer_end,
        (int)(helpers_end - helpers), helpers,
        (int)(printer_end - printer), printer,
        (int)(bar_end - bar), bar);
    fprintf(file,
        "_bAMenuSelected: .byte 0\n_bAMenuChildSelected: .byte 0\n_bAMenuChildWidth: .res 10\n"
        "_bAMenuChildShiftBack: .res 10\n_bAMenuChildCount: .res 10\n_bAFirstMenuChild: .res 20\n"
        "_bAEnabledMenuControllers: .res 256\n_sizeOfMenu: .byte 3\n_offsetOfText: .byte 0\n_offsetOfController: .byte 2\n"
        "_numOfMenus: .word 0\n_the_menu: .res 33\n_menuDirty: .byte 1\n_menuShown: .byte 1\n"
        "isMenuAllowed: lda #1\nrts\nbAPrintMenuChildText: rts\n.endscope\n.segment \"CODE\"\n"
        ".export runMenuClear,runMenuChildren,runMenuBar,menuWidth,menuChildCount,menuFirstChild,menuSelected,menuBarCount,menuBarRecords,menuEnabled,menuOpenAddress\n"
        "runMenuClear: jmp MenuFixture::bAClearMenuChildren\nrunMenuChildren: jmp MenuFixture::bADisplayChildMenu\n"
        "runMenuBar: jmp MenuFixture::bADisplayMenuBar\nmenuWidth=MenuFixture::_bAMenuChildWidth\n"
        "menuChildCount=MenuFixture::_bAMenuChildCount\nmenuFirstChild=MenuFixture::_bAFirstMenuChild\n"
        "menuSelected=MenuFixture::_bAMenuSelected\nmenuBarCount=MenuFixture::_numOfMenus\n"
        "menuBarRecords=MenuFixture::_the_menu\nmenuEnabled=MenuFixture::_bAEnabledMenuControllers\n"
        "menuOpenAddress=MenuFixture::OPEN_MENU_ADDRESS\n");
    CHECK(!fclose(file), "Cannot close menu fixture"); free(text);
}

static void assemble(void) {
    char source[8192], config[8192], object[8192], binary[8192], labels[8192], fixture[8192];
    suite_test_path(source, sizeof(source), suite_fixture_directory, "show_obj.s");
    suite_test_path(config, sizeof(config), suite_fixture_directory, "show_obj.cfg");
    suite_test_path(object, sizeof(object), suite_output_directory, "show_obj.o");
    suite_test_path(binary, sizeof(binary), suite_output_directory, "show_obj.bin");
    suite_test_path(labels, sizeof(labels), suite_output_directory, "show_obj.lbl");
    FILE *file = fopen("src/showObjAsm.s", "rb"); CHECK(file, "Cannot read show_obj assembly");
    CHECK(!fseek(file, 0, SEEK_END), "Cannot seek show_obj assembly"); long length = ftell(file); CHECK(length >= 0, "Cannot size show_obj assembly"); rewind(file);
    char *assembly = malloc((size_t)length + 1); CHECK(assembly, "Out of memory");
    CHECK(fread(assembly, 1, (size_t)length, file) == (size_t)length, "Cannot read show_obj assembly"); assembly[length] = 0; fclose(file);
    char *constant_begin = strstr(assembly, "SHOW_OBJ_ATTRIBUTE_ADDRESS ="); CHECK(constant_begin, "Show.obj address constant missing");
    char *begin = strstr(constant_begin, "_b11ShowObjSpriteAddressShifted: .res 2"); CHECK(begin, "Handler data missing");
    char *end = strstr(begin, "\n.endif"); CHECK(end, "Handler end missing");
    int count = snprintf(fixture, sizeof(fixture),
        "VIEW_TABLE_SIZE=1\nMAX_SPRITES_ROW_OR_COLUMN_SIZE=4\n.include \"global.s\"\n.include \"globalGraphics.s\"\n"
        "%.*s\n"
        ".segment \"CODE\"\n"
        ".export runHandler,runClearObj,done,_b11ShowObjSpriteAddressShifted,_b11ShowObjSprAttr7,_b11ShowObjX,_b11ShowObjY,_b11VeraSlots\n%.*s\n"
        ".export testShowObjAttributeAddress\ntestShowObjAttributeAddress=SHOW_OBJ_ATTRIBUTE_ADDRESS\n"
        "runHandler: jsr b11ShowObjIrqHandler\ndone: rts\nrunClearObj: jsr b11ClearObjIrqHandler\nrts\n"
        ".scope StatusFixture\n.include \"statusBar.s\"\n.endscope\n.segment \"CODE\"\n"
        "_menuShown: .byte 0\nisMenuAllowed: lda #1\nrts\nbAClearTopLine: rts\n"
        ".export runStatus,statusDisplayed,statusShowing,statusInitialized,statusScore,statusMaxScore,statusSound\n"
        "runStatus: jmp StatusFixture::b10DisplayStatusBar\n"
        "statusDisplayed=StatusFixture::_statusLineDisplayed\nstatusShowing=StatusFixture::b10StatusBarShowing\n"
        "statusInitialized=StatusFixture::totalScoreSet\nstatusScore=SET_VAR_OFFSET+SCORE_VAR\n"
        "statusMaxScore=SET_VAR_OFFSET+MAX_SCORE_VAR\nstatusSound=FLAG_OFFSET+SOUND_ON_FLAG\n",
        (int)(begin - constant_begin), constant_begin,
        (int)(end - begin), begin);
    CHECK(count >= 0 && count < (int)sizeof(fixture), "Fixture too large"); free(assembly); write_file(source, fixture);
    append_menu_fixture(source);
    write_file(config,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; BANKRAM10:load=RAM,type=rw,optional=yes; BANKRAM0A:load=RAM,type=rw,optional=yes; }\n");
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

static unsigned call_ui(const char *entry, unsigned max_writes) {
    reset6502(false); regs.pc = symbol(entry); regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT | FLAG_INTERRUPT;
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    clockticks6502 = 0; waiting = 0; writes = 0;
    ram[0] = 0x10; ram[1] = 0x37;
    for (unsigned step = 0; step < 15000 && regs.pc != symbol("done"); ++step) step6502();
    CHECK(regs.pc == symbol("done"), "Status rendering did not return within its IRQ budget");
    CHECK(ram[0] == 0x10 && ram[1] == 0x37 && regs.sp == 0x1FF,
          "Status rendering changed caller banks/stack");
    CHECK(writes <= max_writes, "UI rendering exceeded its VRAM write bound");
    return clockticks6502;
}

static unsigned call_status(void) { return call_ui("runStatus", 80); }

static void put_word(unsigned at, unsigned value) { ram[at] = value; ram[at + 1] = value >> 8; }

static void test_menu_clear(void) {
    const unsigned base = MENU_BAR_LOCATION + 128;
    const unsigned bytes = (MAX_MENU_CHILDREN + 2) * 128;
    memset(vram, 0x5A, sizeof(vram)); memset(fx_cache, 0xA5, sizeof(fx_cache)); fx_write = false;
    unsigned cycles = call_ui("runMenuClear", bytes);
    CHECK(writes == bytes, "Menu clear does not cover all C-declared children and both border rows");
    for (unsigned i = 0; i < bytes; ++i)
        CHECK(vram[base + i] == (i & 1 ? 0x10 : 0x80), "Menu clear used the wrong FX cache register bank/pattern");
    CHECK(vram[base - 1] == 0x5A && vram[base + bytes] == 0x5A,
          "Menu clear changed an adjacent row");
    CHECK(!fx_write && ram[0x9F25] == 0, "Menu clear left VERA FX cache writes enabled");
    CHECK(cycles < 9000, "Menu clear exceeded its bounded IRQ cycle budget");
    printf("Menu clear: %u cycles, %u bytes across %u rows\n", cycles, bytes, MAX_MENU_CHILDREN + 2);
}

static void test_menu_children(void) {
    enum { FULL_MENU_BASELINE_CYCLES = 42308, FULL_MENU_BUDGET_CYCLES = 60000 };
    menu_records = 0x8000; menu_record_end = menu_records + (MAX_MENU_CHILDREN + 1) * 3;
    ram[0x8100] = 'X'; ram[0x8101] = 0;
    ram[symbol("menuSelected")] = 0;
    ram[symbol("menuWidth")] = 5;
    memset(ram + symbol("menuEnabled"), 1, 256);
    put_word(symbol("menuFirstChild"), menu_records);
    for (unsigned count = 0; count <= MAX_MENU_CHILDREN; ++count) {
        memset(vram, 0x5A, sizeof(vram));
        for (unsigned i = 0; i <= MAX_MENU_CHILDREN; ++i) {
            put_word(menu_records + i * 3, 0x8100); ram[menu_records + i * 3 + 2] = i;
        }
        put_word(symbol("menuOpenAddress"), MENU_BAR_LOCATION);
        ram[symbol("menuChildCount")] = count;
        menu_record_limit = menu_records + count * 3;
        track_menu_reads = true;
        call_ui("runMenuChildren", (count + 2) * 10);
        track_menu_reads = false;
        CHECK(writes == (count ? (count + 2) * 10 : 0), "Menu child renderer ignored its empty/published count");
        for (unsigned row = 0; row < count + 2 && count; ++row) {
            unsigned base = MENU_BAR_LOCATION + (row + 1) * 128;
            CHECK(vram[base] != 0x5A && vram[base + 9] != 0x5A && vram[base + 10] == 0x5A,
                  "Menu child row stride/width overran its borders");
        }
    }
    memset(vram, 0x5A, sizeof(vram));
    memset(ram + 0x8100, 'T', MENU_BAR_WIDTH - 2);
    ram[0x8100 + MENU_BAR_WIDTH - 2] = 0;
    ram[symbol("menuWidth")] = MENU_BAR_WIDTH;
    ram[symbol("menuChildCount")] = MAX_MENU_CHILDREN;
    put_word(symbol("menuOpenAddress"), MENU_BAR_LOCATION);
    menu_record_limit = menu_records + MAX_MENU_CHILDREN * 3;
    track_menu_reads = true;
    unsigned maximum_cycles = call_ui("runMenuChildren", (MAX_MENU_CHILDREN + 2) * MENU_BAR_WIDTH * 2);
    track_menu_reads = false;
    CHECK(writes == (MAX_MENU_CHILDREN + 2) * MENU_BAR_WIDTH * 2 && maximum_cycles < FULL_MENU_BUDGET_CYCLES,
          "Maximum menu exceeded its declared extent or IRQ cycle budget");
    for (unsigned row = 0; row < MAX_MENU_CHILDREN + 2; ++row) {
        unsigned base = MENU_BAR_LOCATION + (row + 1) * 128;
        CHECK(vram[base - 1] == 0x5A && vram[base + MENU_BAR_WIDTH * 2] == 0x5A,
              "Maximum menu changed a cell outside its declared row width");
        if (row && row <= MAX_MENU_CHILDREN)
            for (unsigned column = 1; column < MENU_BAR_WIDTH - 1; ++column)
                CHECK(vram[base + column * 2] == 'T', "Maximum menu dropped content characters");
    }
    printf("Maximum menu children: %u cycles, %u writes (baseline %u, budget <%u)\n",
           maximum_cycles, writes, FULL_MENU_BASELINE_CYCLES, FULL_MENU_BUDGET_CYCLES);
    for (unsigned width = 0; width <= 41; ++width) {
        memset(vram, 0x5A, sizeof(vram));
        ram[symbol("menuWidth")] = width;
        ram[symbol("menuChildCount")] = 1;
        put_word(symbol("menuOpenAddress"), MENU_BAR_LOCATION);
        call_ui("runMenuChildren", width >= 2 && width <= 40 ? width * 6 : 0);
        CHECK(writes == (width >= 2 && width <= 40 ? width * 6 : 0),
              "Menu border width two underflowed or invalid width drew outside the bar");
    }

}

static void test_menu_bar(void) {
    puts("Menu children pass: published counts 0..15, widths 0..41 and bounded row strides");
    const unsigned records = symbol("menuBarRecords");
    ram[symbol("menuBarCount")] = 1;
    put_word(records, 0x8200); put_word(records + 3, 0);
    for (unsigned length = 0; length <= 255; ++length) {
        memset(vram, 0x5A, sizeof(vram));
        memset(ram + 0x8200, 'T', length); ram[0x8200 + length] = 0;
        call_ui("runMenuBar", 80);
        CHECK(writes == 80 && vram[MENU_BAR_LOCATION - 1] == 0x5A && vram[MENU_BAR_END] == 0x5A,
              "Menu title/padding overran the 40-cell bar");
    }
    ram[symbol("menuBarCount")] = MAX_MENUS;
    for (unsigned i = 0; i <= MAX_MENUS; ++i) put_word(records + i * 3, 0x8200);
    strcpy((char *)ram + 0x8200, "X");
    call_ui("runMenuBar", 80);
    CHECK(writes == 80, "Full menu bar did not stop at its published item count");
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

static void run_show_obj_handler(const char *entry, unsigned max_steps)
{
    reset6502(false);
    regs.pc = (uint16_t)symbol(entry);
    regs.sp = 0x1FD;
    regs.status = FLAG_CONSTANT;
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    clockticks6502 = 0;
    waiting = 0;
    writes = 0;
    for (unsigned step = 0; step < max_steps && regs.pc != symbol("done"); ++step)
        step6502();
    CHECK(regs.pc == symbol("done"), "show_obj handler did not return");
}

static void set_show_obj_fixture(uint8_t slots, uint16_t x)
{
    ram[symbol("_b11VeraSlots")] = slots;
    ram[symbol("_b11ShowObjX")] = (uint8_t)x;
    ram[symbol("_b11ShowObjX") + 1] = (uint8_t)(x >> 8);
    ram[symbol("_b11ShowObjY")] = 0x56;
    ram[symbol("_b11ShowObjSprAttr7")] = 0xA7;
    for (unsigned i = 0; i < 4; ++i) {
        ram[symbol("_b11ShowObjSpriteAddressShifted") + 2 * i] = (uint8_t)(0x34 + i * 0x10);
        ram[symbol("_b11ShowObjSpriteAddressShifted") + 2 * i + 1] = (uint8_t)(0x12 + i);
    }
}

static void check_show_obj_record(unsigned slot, uint16_t x)
{
    const unsigned at = (symbol("testShowObjAttributeAddress") + slot * 8) & 0x1FFFF;
    const uint8_t expected[8] = {
        (uint8_t)(0x34 + slot * 0x10), (uint8_t)(0x12 + slot),
        (uint8_t)x, (uint8_t)(x >> 8), 0x56, 0x00, 0x0C, 0xA7
    };
    for (unsigned i = 0; i < sizeof(expected); ++i)
        CHECK(vram[at + i] == expected[i], "show_obj slot attribute record differs");
}

static void test_show_obj_handlers(void)
{
    memset(vram, 0x5A, sizeof(vram));
    memset(address, 0, sizeof(address));
    memset(stride, 0, sizeof(stride));
    set_show_obj_fixture(0, 100);
    run_show_obj_handler("runHandler", 100000);
    CHECK(writes == 0, "zero-slot show_obj publication wrote sprite attributes");

    memset(vram, 0x5A, sizeof(vram));
    set_show_obj_fixture(1, 100);
    run_show_obj_handler("runHandler", 10000);
    CHECK(writes == 8, "one-slot show_obj publication did not write exactly one record");
    check_show_obj_record(0, 100);
    CHECK(ram[symbol("_b11VeraSlots")] == 1 &&
          ram[symbol("_b11ShowObjX")] == 100 && ram[symbol("_b11ShowObjX") + 1] == 0,
          "one-slot show_obj handler changed its published inputs");
    CHECK(vram[(symbol("testShowObjAttributeAddress") - 1) & 0x1FFFF] == 0x5A && vram[0] == 0x5A,
          "one-slot show_obj publication changed an adjacent record");

    memset(vram, 0x5A, sizeof(vram));
    set_show_obj_fixture(4, 100);
    run_show_obj_handler("runHandler", 10000);
    CHECK(writes == 32, "four-slot show_obj publication did not write exactly four records");
    for (unsigned i = 0; i < 4; ++i)
        check_show_obj_record(i, (uint16_t)(100 + 64 * i));
    CHECK(ram[symbol("_b11VeraSlots")] == 4 &&
          ram[symbol("_b11ShowObjX")] == 100 && ram[symbol("_b11ShowObjX") + 1] == 0,
          "four-slot show_obj handler changed its published inputs");
    CHECK(vram[(symbol("testShowObjAttributeAddress") - 1) & 0x1FFFF] == 0x5A && vram[0] == 0x5A,
          "four-slot show_obj publication changed an adjacent record");

    uint8_t published[32];
    unsigned show_obj_base = symbol("testShowObjAttributeAddress");
    memcpy(published, vram + show_obj_base, sizeof(published));
    run_show_obj_handler("runHandler", 10000);
    CHECK(writes == 32 && !memcmp(published, vram + show_obj_base, sizeof(published)),
          "repeated show_obj publication changed the same inputs into different attributes");
    CHECK(ram[symbol("_b11VeraSlots")] == 4 &&
          ram[symbol("_b11ShowObjX")] == 100 && ram[symbol("_b11ShowObjX") + 1] == 0,
          "repeated show_obj publication changed its count or origin");

    memset(vram, 0x5A, sizeof(vram));
    set_show_obj_fixture(5, 100);
    run_show_obj_handler("runHandler", 10000);
    CHECK(writes == 0, "oversize show_obj publication exceeded its four reserved records");
    CHECK(vram[0] == 0x5A, "oversize show_obj publication touched an unrelated record");
    CHECK(ram[symbol("_b11VeraSlots")] == 5 &&
          ram[symbol("_b11ShowObjX")] == 100 && ram[symbol("_b11ShowObjX") + 1] == 0,
          "oversize show_obj publication changed its invalid count or origin");

    memset(vram, 0x5A, sizeof(vram));
    set_show_obj_fixture(255, 100);
    run_show_obj_handler("runHandler", 10000);
    CHECK(writes == 0, "255-slot show_obj publication wrapped into reserved attributes");
    CHECK(ram[symbol("_b11VeraSlots")] == 255 &&
          ram[symbol("_b11ShowObjX")] == 100 && ram[symbol("_b11ShowObjX") + 1] == 0,
          "255-slot show_obj publication changed its invalid count or origin");

    memset(vram, 0x5A, sizeof(vram));
    run_show_obj_handler("runClearObj", 10000);
    CHECK(writes == 4, "CLEAR_OBJ did not disable exactly four reserved attributes");
    for (unsigned i = 0; i < 4; ++i)
        CHECK(vram[(show_obj_base + i * 8 + 6) & 0x1FFFF] == 0, "CLEAR_OBJ missed a reserved sprite enable byte");
    CHECK(vram[(show_obj_base + 4 * 8 + 6) & 0x1FFFF] == 0x5A,
          "CLEAR_OBJ changed the first sprite attribute outside its reserved range");
    puts("show_obj handlers pass zero/one/four/oversize/repeat and exact CLEAR_OBJ bounds");
}

int main(void) {
    suite_test_paths_init("show-obj-irq"); assemble();
    test_show_obj_handlers();
    test_status(); test_menu_clear(); test_menu_children(); test_menu_bar(); return 0;
}
