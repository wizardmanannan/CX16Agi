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
#include "../src/spriteUpdates.h"
#include "test_paths.h"

extern struct regs regs;
static uint8_t ram[65536], vram[131072];
static unsigned addr[2], stride[2], writes, data_port_reads, acks;
static unsigned write_addresses[8192];
static uint8_t write_values[8192];
struct symbol { char name[100]; unsigned addr; } symbols[128];
static unsigned symbol_count;
static bool track_sprite_reads;
static unsigned sprite_read_start, sprite_read_end, sprite_read_limit, sprite_reads;
static bool fx_write;
static uint8_t fx_cache[4];

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (track_sprite_reads && address >= sprite_read_start - 1 && address < sprite_read_end + 2) {
        CHECK(address >= sprite_read_start && address < sprite_read_limit,
              "Sprite consumer read beyond its accepted records/terminator");
        ++sprite_reads;
    }
    if (address >= 0x9F20 && address <= 0x9F22) {
        unsigned port = ram[0x9F25] & 1;
        if (address == 0x9F20) return (uint8_t)addr[port];
        if (address == 0x9F21) return (uint8_t)(addr[port] >> 8);
        return (uint8_t)((ram[address] & 0xFE) | ((addr[port] >> 16) & 1));
    }
    if (address == 0x9F23 || address == 0x9F24) {
        unsigned port = address - 0x9F23;
        ++data_port_reads;
        uint8_t value = vram[addr[port]];
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        return value;
    }
    return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if (address >= 0x9F29 && address <= 0x9F2C && (ram[0x9F25] >> 1) == 6) {
        fx_cache[address - 0x9F29] = value;
        return;
    }
    if (address == 0x9F29 && (ram[0x9F25] >> 1) == 2) {
        fx_write = (value & 0x40) != 0;
        return;
    }
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
        unsigned count = fx_write ? 4 : 1;
        CHECK(writes + count <= sizeof(write_addresses) / sizeof(write_addresses[0]), "Too many VERA writes");
        if (fx_write) {
            CHECK(value == 0 && (addr[port] & 3) == 0, "Unexpected unaligned VERA FX cache write");
            for (unsigned i = 0; i < 4; ++i) {
                write_addresses[writes + i] = (addr[port] + i) & 0x1FFFF;
                write_values[writes + i] = fx_cache[i];
                vram[(addr[port] + i) & 0x1FFFF] = fx_cache[i];
            }
        } else {
            write_addresses[writes] = addr[port];
            write_values[writes] = value;
            vram[addr[port]] = value;
        }
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        writes += count;
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
    char *match = strstr(text, needle);
    if (!match) { fprintf(stderr, "Source marker missing: %s\n", needle); exit(1); }
    return match;
}

static bool has_code_line(const char *start, const char *end, const char *text) {
    for (const char *match = start; (match = strstr(match, text)) && match < end; ++match) {
        const char *line = match;
        while (line > start && line[-1] != '\n') --line;
        const char *p = line;
        while (p < match && (*p == ' ' || *p == '\t')) ++p;
        if (p == match) return true;
    }
    return false;
}

static unsigned occurrences(const char *text, const char *needle) {
    unsigned count = 0;
    size_t length = strlen(needle);
    for (const char *match = text; (match = strstr(match, needle)); match += length) ++count;
    return count;
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
        execvp(executable, argv);
        perror(executable);
        _exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        if (!strcmp(arguments[0], "ca65") && arguments[5]) {
            FILE *source = fopen(arguments[5], "r");
            if (source) {
                char line[1024];
                unsigned number = 0;
                fprintf(stderr, "Failed assembler input %s:\n", arguments[5]);
                while (fgets(line, sizeof(line), source))
                    fprintf(stderr, "%4u %s", ++number, line);
                fclose(source);
            }
        }
        CHECK(false, "Assembler or linker failed");
    }
}

static void append_menu_fixture(const char *path) {
    char *menu = read_file("src/menuAsm.s");
    char *constants = find(menu, "MENU_SREG =");
    char *constants_end = find(constants, ".segment \"CODE\"");
    char *allowed = find(menu, "isMenuAllowed:");
    char *allowed_end = find(allowed, "\n\n.segment \"BANKRAM0A\"");
    char *helpers = find(menu, "bAClearTopLine:");
    char *helpers_end = find(helpers, "bAPrintMenuText:");
    char *printer = find(menu, "bAPrintMenuText:");
    char *printer_end = find(printer, "MENU_BAR_LOCATION =");
    char *locations = printer_end;
    char *locations_end = find(locations, "_bAInitMenus:");
    char *display = find(menu, "bADisplayMenu:");
    char *display_end = find(display, "\n.endif");
    FILE *file = fopen(path, "a"); CHECK(file, "Cannot extend owned IRQ fixture");
    fprintf(file, "\n.segment \"ZEROPAGE\"\n%.*s\n.segment \"CODE\"\n%.*s\n.segment \"BANKRAM0A\"\n%.*s\n%.*s\n%.*s\n%.*s\n",
        (int)(constants_end - constants), constants,
        (int)(allowed_end - allowed), allowed,
        (int)(helpers_end - helpers), helpers,
        (int)(printer_end - printer), printer,
        (int)(locations_end - locations), locations,
        (int)(display_end - display), display);
    CHECK(!fclose(file), "Cannot close menu fixture");
    free(menu);
}

static void assemble(void) {
    char source[65536], stub[256], menu_stub[1024], path[4096], stub_path[4096], menu_stub_path[4096], config_path[4096], object_path[4096], stub_object_path[4096], menu_stub_object_path[4096], binary_path[4096], labels_path[4096];
    suite_test_path(path, sizeof(path), suite_fixture_directory, "test.s");
    suite_test_path(stub_path, sizeof(stub_path), suite_fixture_directory, "sound_stub.s");
    suite_test_path(menu_stub_path, sizeof(menu_stub_path), suite_fixture_directory, "menu_stub.s");
    suite_test_path(config_path, sizeof(config_path), suite_fixture_directory, "test.cfg");
    suite_test_path(object_path, sizeof(object_path), suite_output_directory, "irq_helpers.o");
    suite_test_path(stub_object_path, sizeof(stub_object_path), suite_output_directory, "sound_stub.o");
    suite_test_path(menu_stub_object_path, sizeof(menu_stub_object_path), suite_output_directory, "menu_stub.o");
    suite_test_path(binary_path, sizeof(binary_path), suite_output_directory, "irq_helpers.bin");
    suite_test_path(labels_path, sizeof(labels_path), suite_output_directory, "irq_helpers.lbl");

    char *irq = read_file("src/irqAsm.s");
    char *input = read_file("src/inputIrqHandler.s");
    char *sprite = read_file("src/spriteIrqHandler.s");
    char *sprite_snapshot_reset = strstr(sprite, "bEBeginSpriteSnapshot:\n");
    if (!sprite_snapshot_reset) {
        fprintf(stderr, "FAIL sprite_snapshot_reset: production reset routine is missing\n");
        exit(EXIT_FAILURE);
    }
    char *sprite_snapshot_reset_end = find(sprite_snapshot_reset, "\nrts") + strlen("\nrts");
    char *text_state = find(irq, "_b3PaletteAddress: .res 4");
    char *text_palette_code = find(text_state, "_b3InitLayer1Mapbase:");
    char *text_copy_code = find(text_palette_code, "handleDisplayTextSlice:");
    char *text_data = find(text_copy_code, "_displayTextAddressToCopyTo: .word $0");
    char *text_end = find(text_data, ".segment \"BANKRAM06\"");
    char *snapshot_helper = find(irq, "_b6CopyVSyncCounter32:\n");
    char *snapshot_helper_end = find(snapshot_helper, "\nrts") + strlen("\nrts");
    char *owner_helpers = find(irq, "_b6EnterForegroundVera:\n");
    char *owner_leave = find(owner_helpers, "_b6LeaveForegroundVera:\n");
    char *owner_helpers_end = find(owner_leave, "\nrts") + strlen("\nrts");
    char *irq_constants = find(irq, "IRQ_CMD_DONTCHANGE = 0");
    char *irq_constants_end = find(irq_constants, "sendIrqCommand: .byte");
    char *irq_handler = find(irq, "IRQ_WORK_CLEAR = 1");
    char *irq_handler_end = find(irq_handler, "\n.endif ; IRQ_INC");
    char *irq_macros = find(irq, ".macro SEND_IRQ_COMMAND");
    char *irq_end = find(irq_macros, ";Handlers");
    char *input_body = find(input, "b7HandleInputLine:");
    char *input_end = find(input_body, "\n.endif");
    char *sprite_start = find(sprite, ".macro SET_VERA_START_SPRITE_ATTRS");
    char *sprite_start_end = find(sprite_start, ".endmacro") + strlen(".endmacro");
    char *sprite_clear = find(sprite, ".macro CLEAR_SPRITE_ATTRS");
    char *sprite_clear_end = find(sprite_clear, ".endmacro") + strlen(".endmacro");
    char *sprite_get = find(sprite, ".macro GET_NEXT_FROM_SPRITE_UPDATE_BUFFER");
    char *sprite_get_end = find(sprite_get, ".endmacro") + strlen(".endmacro");
    char *sprite_append = find(sprite, "_bECanAppendSpriteUpdate:\n");
    char *sprite_append_end = find(sprite_append, "\n;void bEClearSpriteAttributes");
    char *sprite_handler = find(sprite, "bEHandleSpriteUpdates:");
    char *sprite_handler_end = find(sprite_handler, "\n.endif");

    int length = snprintf(source, sizeof(source),
        ".include \"x16.inc\"\nVIEW_TABLE_SIZE=1\n.include \"globalGraphics.s\"\n.include \"spriteUpdates.inc\"\n.include \"global.s\"\n"
        ".macro TRAMPOLINE BANK, JMPTO\njsr JMPTO\n.endmacro\nMAX_INPUT_STRING_LENGTH=40\n"
        "INPUT_LINE_RENDER_CELLS=MAX_INPUT_STRING_LENGTH+1\n"
        ".assert INPUT_LINE_COL + INPUT_LINE_RENDER_CELLS <= TILE_LAYER_WIDTH, error, \"Input line rendering exceeds tile map row\"\n"
        ".segment \"CODE\"\n"
        ".export input,enable,send,wait,done,snapshot,snapshot32,snapshotOutput,_vSyncCounter,_vSyncCounterHigh,sendIrqCommand,_inputLineDisplayed,_inputLineDirty,_b7CurrentInputStr,_foregroundVeraDepth,_b6EnterForegroundVera,_b6LeaveForegroundVera\n"
        ".export clearSprites,inputStringAddress,spriteFirstZ,spriteCount\n"
        ".export handleSpriteUpdates,checkSpriteUpdateCapacity,bEBeginSpriteSnapshot,_bESpritesUpdatedBuffer,_bESpritesUpdatedBufferPointer,_bESpriteUpdateOverflowed,spriteBufferBefore,spriteBufferEnd,spriteBufferGuard,spriteUpdateBufferBytes,spriteUpdateRecordBytes,spriteUpdateMaxRecords,spriteUpdateBank,irqScratchPointer\n"
        ".export bBSoundHandler,_bBChannelsPlaying,_bBEndSoundFlag,_bBIsPlaying,_bBCh1Ticks,_bBCh2Ticks,_bBCh3Ticks,_bBCh4Ticks\n"
        ".export _ZP_CURRENTLY_PLAYING_NOTE_1,_ZP_CURRENTLY_PLAYING_NOTE_2,_ZP_CURRENTLY_PLAYING_NOTE_3,_ZP_CURRENTLY_PLAYING_NOTE_NOISE,SOUND_SREG,_bBPsgClear,soundCriticalSection\n"
        ".import _charSetInited,_numOfMenus,_the_menu,_sizeOfMenu,_offsetOfText,_menuAllowed,_menuShown,_bAMenuSelected,_bAMenuChildSelected,_bAMenuChildWidth,_bAMenuChildCount,_bAFirstMenuChild,_bAMenuChildShiftBack,_bAEnabledMenuControllers,_offsetOfController,_menuDirty\n"
        "ZP_SPRITE_UPDATE_READ_POINTER=IRQ_TMP_6\nZP_LOW_BYTE=ZP_TMP_5+1\n"
        "%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n"
        "spriteBufferBefore: .byte $3D\n_bESpritesUpdatedBuffer: .res SPRITE_UPDATED_BUFFER_SIZE\n"
        "spriteBufferEnd:\nspriteBufferGuard: .byte $A6,$5B\n"
        "_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer\n"
        "_bESpriteUpdateOverflowed: .byte 0\n"
        "handleSpriteUpdates: jmp bEHandleSpriteUpdates\n"
        "checkSpriteUpdateCapacity: jmp _bECanAppendSpriteUpdate\n"
        "spriteBufferEndAddress = _bESpritesUpdatedBuffer + SPRITE_UPDATED_BUFFER_SIZE\n"
        "spriteUpdateBufferBytes = SPRITE_UPDATED_BUFFER_SIZE\nspriteUpdateRecordBytes = SPRITE_UPDATE_RECORD_BYTES\nspriteUpdateMaxRecords = SPRITE_UPDATE_MAX_RECORDS\nirqScratchPointer = IRQ_TMP_6\n"
        "spriteUpdateBank = SPRITE_UPDATES_BANK\n"
        "input:\n%.*s\n"
        "enable: REENABLE_INTERRUPTS\nrts\n"
        "send: SEND_IRQ_COMMAND command,snapshot\nrts\n"
        "wait: WAIT_FOR_NEXT_IRQ snapshot\nrts\n"
        "clearSprites: CLEAR_SPRITE_ATTRS\nrts\n"
        "done: brk\ncommand: .byte 4\nsendIrqCommand: .byte 0\nsnapshot: .word 0\n_vSyncCounter: .word 0\n_vSyncCounterHigh: .word 0\n_foregroundVeraDepth: .byte 0\nsnapshotOutput: .res 4\nsnapshot32: jmp _b6CopyVSyncCounter32\n"
        "_inputLineDisplayed: .byte 0\n_inputLineDirty: .byte 0\n_b7CurrentInputStr: .res 41\n"
        ".include \"soundAsm.s\"\n.segment \"CODE\"\n"
        "soundCriticalSection: php\nsei\nplp\nrts\n"
        "%.*s\n%.*s\n"
        ".export custom_irq_handler,irqEntry,irqDefault,default_irq_vector,irqWorkKind,irqTextStage,irqCompletionGeneration,lastMainLoopCounter,_mainLoopCounter\n"
        ".export b3InitDisplayTextSlice,b3SetPaletteSlice,handleDisplayTextSlice,_b3PaletteAddress,_b3PaletteRows,_b3PaletteNumber,_displayTextAddressToCopyTo,_currentTextBuffer,displayTextSourcePointer,displayPaletteCursor\n"
        "currentIrqState: .byte 0\nlastMainLoopCounter: .byte 0\n_mainLoopCounter: .byte 0\n"
        "irqEntry: pha\nphx\nphy\njmp custom_irq_handler\n"
        "irqDefault: ply\nplx\npla\nrti\n"
        "handleScreenShake: rts\n_b3InitLayer1Mapbase: rts\n_b6InitInput: rts\n_b6SetBackgroundColor: rts\n"
        "b11ShowObjIrqHandler: rts\nb11ClearObjIrqHandler: rts\nbDHighlightInventoryRow: rts\n"
        "_bEClearSpriteAttributes: rts\n_b8ClearBackgroundSliceReset: rts\n_b8ClearPrioritySliceReset: rts\n_b8ClearBackgroundSlice: rts\n_b8ClearPrioritySlice: rts\n"
        ".segment \"BANKRAM03\"\n%.*s\n%.*s\n%.*s\n"
        ".segment \"CODE\"\n"
        "inputStringAddress=INPUT_STRING_ADDRESS\nspriteFirstZ=SPRITE_ATTR_START+SPRITE_ATTR_SIZE+SA_VERA_ZORDER\nspriteCount=NO_SPRITES\n"
        ".include \"statusBar.s\"\n.export b10StatusBarShowing\n",
        (int)(irq_end - irq_macros), irq_macros,
        (int)(snapshot_helper_end - snapshot_helper), snapshot_helper,
        (int)(owner_helpers_end - owner_helpers), owner_helpers,
        (int)(sprite_start_end - sprite_start), sprite_start,
        (int)(sprite_clear_end - sprite_clear), sprite_clear,
        (int)(sprite_get_end - sprite_get), sprite_get,
        (int)(sprite_snapshot_reset_end - sprite_snapshot_reset), sprite_snapshot_reset,
        (int)(sprite_append_end - sprite_append), sprite_append,
        (int)(sprite_handler_end - sprite_handler), sprite_handler,
        (int)(input_end - input_body), input_body,
        (int)(irq_constants_end - irq_constants), irq_constants,
        (int)(irq_handler_end - irq_handler), irq_handler,
        (int)(text_palette_code - text_state), text_state,
        (int)(text_data - text_copy_code), text_copy_code,
        (int)(text_end - text_data), text_data);
    CHECK(length >= 0 && length < (int)sizeof(source), "Fixture assembly is too large");
    CHECK(snprintf(stub, sizeof(stub), ".segment \"CODE\"\n.export _bBLoadedSoundsPointer,_sResetCounter\n_bBLoadedSoundsPointer: .res 512\n_sResetCounter: .byte 0\n") < (int)sizeof(stub), "Fixture stub is too large");
    CHECK(snprintf(menu_stub, sizeof(menu_stub),
        ".export _charSetInited,_numOfMenus,_the_menu,_the_menuChildren,_menuTextBuffer,_sizeOfMenu,_offsetOfText,_offsetOfController,_menuAllowed,_menuShown,_menuDirty,_bAMenuSelected,_bAMenuChildSelected,_bAMenuChildWidth,_bAMenuChildCount,_bAFirstMenuChild,_bAMenuChildShiftBack,_bAEnabledMenuControllers\n"
        ".segment \"BSS\"\n_charSetInited: .res 1\n_numOfMenus: .res 2\n_menuShown: .res 1\n_menuAllowed: .res 1\n_menuDirty: .res 1\n"
        ".segment \"DATA\"\n_sizeOfMenu: .byte 3\n_offsetOfText: .byte 0\n_offsetOfController: .byte 2\n"
        ".segment \"BANKRAM0A\"\n_the_menu: .res 30\n_the_menuChildren: .res 450\n_bAFirstMenuChild: .res 20\n_menuTextBuffer: .res 500\n"
        "_bAMenuChildWidth: .res 10\n_bAMenuChildShiftBack: .res 10\n_bAMenuChildCount: .res 10\n_bAEnabledMenuControllers: .res 256\n"
        "_nextMenuTextBufferAddr: .word _menuTextBuffer\n_bAMenuSelected: .res 1\n_bAMenuChildSelected: .res 1\n_bAChildMenuToClear: .byte $FF\n"
        "") < (int)sizeof(menu_stub), "Menu fixture globals are too large");
    free(irq); free(input); free(sprite);

    write_file(path, source);
    append_menu_fixture(path);
    write_file(stub_path, stub);
    write_file(menu_stub_path, menu_stub);
    write_file(config_path,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; BSS:load=RAM,type=rw,optional=yes; BANKRAM03:load=RAM,type=rw,optional=yes; BANKRAM07:load=RAM,type=rw,optional=yes; BANKRAM0A:load=RAM,type=rw,optional=yes; BANKRAM0B:load=RAM,type=rw,optional=yes; BANKRAM10:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object_path, NULL};
    char *ca65_stub[] = {"ca65", "-t", "cx16", stub_path, "-o", stub_object_path, NULL};
    char *ca65_menu_stub[] = {"ca65", "-t", "cx16", menu_stub_path, "-o", menu_stub_object_path, NULL};
    char *ld65[] = {"ld65", "-C", config_path, object_path, stub_object_path, menu_stub_object_path, "-o", binary_path, "-Ln", labels_path, NULL};
    run_tool(ca65); run_tool(ca65_stub); run_tool(ca65_menu_stub); run_tool(ld65);

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
    waiting = 0; writes = data_port_reads = acks = 0;
}

static void run(void) {
    const unsigned start_pc = regs.pc;
    uint16_t trace_pc[32];
    uint8_t trace_op[32];
    unsigned steps;
    for (steps = 0; steps < 20000 && regs.pc != symbol("done"); ++steps) {
        CHECK(!waiting, "Unexpected WAI");
        trace_pc[steps % 32] = regs.pc;
        trace_op[steps % 32] = ram[regs.pc];
        step6502();
    }
    if (regs.pc != symbol("done")) {
        const struct symbol *nearest = NULL;
        for (unsigned i = 0; i < symbol_count; ++i)
            if (symbols[i].addr <= regs.pc && (!nearest || symbols[i].addr > nearest->addr)) nearest = &symbols[i];
        fprintf(stderr, "Assembly helper start=$%04X final=$%04X (%s=$%04X) bank=$%02X sp=$%04X status=$%02X after %u steps; sound count=%u flags=%u%u%u%u ticks=%u/%u\n",
                start_pc, regs.pc, nearest ? nearest->name : "?", nearest ? nearest->addr : 0,
                regs.k, regs.sp, regs.status, steps, ram[symbol("_bBChannelsPlaying")],
                ram[symbol("_bBIsPlaying")], ram[symbol("_bBIsPlaying") + 1],
                ram[symbol("_bBIsPlaying") + 2], ram[symbol("_bBIsPlaying") + 3],
                ram[symbol("_bBCh1Ticks")], ram[symbol("_bBCh1Ticks") + 1]);
        const unsigned shown = steps < 32 ? steps : 32;
        const unsigned begin = steps < 32 ? 0 : steps % 32;
        for (unsigned i = 0; i < shown; ++i) {
            const unsigned trace = (begin + i) % 32;
            fprintf(stderr, "  step %u pc=$%04X op=$%02X\n", steps - shown + i,
                    trace_pc[trace], trace_op[trace]);
        }
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
    ram[symbol("_inputLineDirty")] = 1;
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
    CHECK(!ram[symbol("_inputLineDirty")], "Input redraw did not consume its dirty flag");
    check_input_map(shown, length);
}

static void test_input_dirty(void) {
    const unsigned base = symbol("inputStringAddress"), dirty = symbol("_inputLineDirty");
    seed_input_map();
    set_input(1, 4);
    init_cpu(symbol("input"), 0);
    run();
    check_input_map(1, 4);

    unsigned saved_address = addr[0], saved_stride = stride[0];
    uint8_t saved_low = ram[0x9F20], saved_high = ram[0x9F21], saved_bank = ram[0x9F22];
    init_cpu(symbol("input"), 255);
    uint32_t clean_start = clockticks6502;
    run();
    CHECK(!writes && addr[0] == saved_address && stride[0] == saved_stride,
          "Clean input line still accessed VERA");
    CHECK(clockticks6502 - clean_start <= 14,
          "Clean input line fast path exceeded 14 cycles");
    CHECK(ram[0x9F20] == saved_low && ram[0x9F21] == saved_high && ram[0x9F22] == saved_bank,
          "Clean input line changed the VERA address registers");

    ram[symbol("_b7CurrentInputStr") + 1] = 'Z'; /* Parser mutation. */
    ram[dirty] = 1;
    init_cpu(symbol("input"), 0);
    run();
    CHECK(writes > 0, "Input mutation invalidation did not redraw");
    CHECK(vram[base + 4] == 'Z', "Input mutation was not rendered");

    ram[symbol("_inputLineDisplayed")] = 0; /* prevent.input / text-screen transition */
    ram[dirty] = 1;
    init_cpu(symbol("input"), 0);
    run();
    check_input_map(0, 0);

    ram[symbol("_inputLineDisplayed")] = 1; /* accept.input / graphics transition */
    ram[dirty] = 1;
    init_cpu(symbol("input"), 0);
    run();
    check_input_map(1, 4);

    for (unsigned cell = 0; cell < INPUT_RENDER_CELLS; ++cell)
        vram[base + cell * 2] = 0x80; /* Full map clear / text overlay. */
    ram[dirty] = 1;
    init_cpu(symbol("input"), 0);
    run();
    check_input_map(1, 4);
}

static void test_input_dirty_sources(void) {
    char *parser = read_file("src/parser.c");
    char *commands = read_file("src/commands.c");
    char *graphics_c = read_file("src/graphics.c");
    char *picture = read_file("src/picture.c");
    char *irq = read_file("src/irqAsm.s");
    char *graphics = read_file("src/graphicsAsm.s");
    char *map_clear = find(irq, "_b3InitLayer1Mapbase:");
    char *map_clear_dirty = find(map_clear, "sta _inputLineDirty");
    char *display = find(irq, "@finishDisplayText:");
    char *display_dirty = find(display, "sta _inputLineDirty");
    char *display_finish = find(display, "stz irqWorkKind");
    char *input_init = find(graphics, "_b6InitInput:");
    char *input_init_dirty = find(input_init, "sta _inputLineDirty");

    CHECK(occurrences(parser, "inputLineDirty = TRUE;") >= 5,
          "Parser edits/reset do not all invalidate the input row");
    CHECK(occurrences(commands, "inputLineDirty = TRUE;") >= 2 &&
          occurrences(graphics_c, "inputLineDirty = TRUE;") >= 2,
          "Input visibility and mode transitions do not all invalidate the row");
    CHECK(strstr(picture, "volatile byte inputLineDirty = TRUE;"),
          "Input dirty state is not initialized in resident storage");
    CHECK(map_clear_dirty < display && display_dirty < display_finish,
          "Map clear/text display does not invalidate the row");
    CHECK(input_init_dirty < find(input_init, ".segment \"BANKRAM08\""),
          "Input row initialization does not invalidate the row");

    free(parser); free(commands); free(graphics_c); free(picture); free(irq); free(graphics);
}

static void test_sound_source_contract(void) {
    char *source = read_file("src/sound.c");
    char *init = find(source, "void bBInitSound()");
    char *play = find(source, "void bBPlaySound(");
    char *stop = find(source, "void bBStopSound()");
    char *init_end = find(init, "\n}");
    char *play_end = find(play, "\n}");
    char *stop_end = find(stop, "\n}");
    char *init_save = find(init, "asm(\"php\")");
    char *init_sei = find(init_save, "asm(\"sei\")");
    char *init_clear = find(init_sei, "bBPsgClear();");
    char *init_restore = find(init_clear, "asm(\"plp\")");
    char *play_disabled = find(play, "if(!flag[9]");
    char *play_save = find(play, "asm(\"php\")");
    char *play_sei = find(play_save, "asm(\"sei\")");
    char *play_restore = find(play_sei, "asm(\"plp\")");
    char *stop_save = find(stop, "asm(\"php\")");
    char *stop_sei = find(stop_save, "asm(\"sei\")");
    char *stop_count = find(stop_sei, "bBChannelsPlaying = 0;");
    char *stop_restore = find(stop_count, "asm(\"plp\")");

    CHECK(init_save < init_sei && init_sei < init_clear && init_clear < init_restore && init_restore < init_end,
          "Sound startup must mute PSG and restore caller interrupt state");
    CHECK(play_disabled < play_save && play_save < play_sei && play_sei < play_restore && play_restore < play_end,
          "Disabled sound must return before masking IRQs; play must preserve caller interrupt state");
    CHECK(find(play, "memset(bBIsPlaying, TRUE, NO_CHANNELS)") < find(play, "bBChannelsPlaying = NO_CHANNELS"),
          "Sound start did not publish all playing channels before its count");
    CHECK(stop_count < stop_restore && find(stop, "memset(bBIsPlaying, FALSE, NO_CHANNELS)") < stop_count,
          "Sound stop did not clear flags and channel count before restoring IRQ state");
    CHECK(stop_restore < stop_end, "Sound stop did not restore caller interrupt state");
    CHECK(!strstr(play, "REENABLE_INTERRUPTS()") && !strstr(stop, "REENABLE_INTERRUPTS()"),
          "Sound transitions must not discard a pending VSYNC acknowledgement");
    free(source);
}

static void test_agi_blit_owner_cleanup_contract(void) {
    char *source = read_file("src/view.c");
    char *assembly = read_file("src/viewAsm.s");
    char *blit = find(source, "boolean agiBlit(");
    char *blit_end = find(blit, "\n}\n");
    char *enter = find(blit, "b6EnterForegroundVera();");
    char *set_loop_failure = find(blit, "if (!bESetLoop(");
    char *allocation_failure = find(blit, "if (!bEAllocateSpriteMemory(");
    char *cleanup = find(blit, "endAgiBlit:");
    char *bank_restore = find(cleanup, "RAM_BANK = previousBank;");
    char *leave = find(bank_restore, "b6LeaveForegroundVera();");
    char *result = find(leave, "return result;");

    CHECK(blit_end && enter < set_loop_failure && set_loop_failure < allocation_failure &&
          allocation_failure < cleanup && bank_restore < leave && leave < result,
          "AGI blit ownership exits do not use the shared bank/owner cleanup");
    char *set_loop_goto = strstr(set_loop_failure, "goto endAgiBlit;");
    char *allocation_goto = strstr(allocation_failure, "goto endAgiBlit;");
    CHECK(set_loop_goto && set_loop_goto < cleanup && allocation_goto && allocation_goto < cleanup,
          "AGI blit failure exit bypasses owner cleanup");
    CHECK(!has_code_line(blit, blit_end, "return FALSE;") && !has_code_line(blit, blit_end, "return TRUE;"),
          "AGI blit has an unbalanced direct return after ownership entry");

    char *set_loop = find(assembly, "_bESetLoop:\n");
    char *save_bank = find(set_loop, "lda RAM_BANK\n    sta PREVIOUS_BANK");
    char *set_loop_jump = find(set_loop, "jmp bEAllocateSpriteMemory");
    char *allocator = find(assembly, "bEAllocateSpriteMemory:\n");
    char *allocator_call = find(allocator, "jsr bEAllocateSpriteMemoryBulkAsm");
    char *failed_branch = find(allocator_call, "bne success\n    jmp restoreBankFailure");
    char *success_return = find(allocator_call, "restoreBank:\n    lda PREVIOUS_BANK\n    sta RAM_BANK\n\n    lda #TRUE");
    char *failure_return = find(success_return, "restoreBankFailure:\n    lda PREVIOUS_BANK\n    sta RAM_BANK\n\n    lda #FALSE\n    rts");
    CHECK(save_bank < set_loop_jump && allocator_call < failed_branch && failed_branch < success_return && failure_return,
          "bESetLoop does not save/restore the caller bank and return FALSE on allocation failure");
    char *cel_to_vera = read_file("src/celToVera.s");
    char *cel_entry = find(cel_to_vera, "_b9CelToVera:\n");
    char *slot_argument = find(cel_entry, "sta NO_SPLIT_SEGMENTS");
    char *owner_entry = find(cel_entry, "jsr _b6EnterForegroundVera");
    CHECK(slot_argument < owner_entry,
          "Cel-to-VERA split-slot argument must be saved before the owner helper clobbers A");
    free(cel_to_vera);
    free(source); free(assembly);
}

static void test_get_priority_condition_codes(void) {
    char *priority = read_file("src/priority.s");
    char *view_asm = read_file("src/viewAsm.s");
    char *macro = find(priority, ".macro GET_PRIORITY");
    char *macro_end = find(macro, ".endmacro");
    char *save_flags = find(macro, "php\nsei");
    char *restore_flags = find(macro, "plp");
    char *control_call = find(priority, "GET_PRIORITY\n\ncmp #NOT_AN_OBSTACLE");
    char *object_call = find(view_asm, "GET_PRIORITY                      ; A = priority at (X_VAL,Y_VAL)");

    CHECK(save_flags && save_flags < restore_flags && restore_flags < macro_end,
          "GET_PRIORITY does not restore caller condition codes after its VERA read");
    CHECK(control_call && object_call &&
          find(control_call, "cmp #NOT_AN_OBSTACLE") < find(control_call, "bcc @return") &&
          find(object_call, "cmp #WATER") < find(object_call, "beq @checkAgainstControlLoopCheck"),
          "GET_PRIORITY caller branches on stale condition codes instead of comparing its result");
    free(priority); free(view_asm);
}

static void test_nested_foreground_owner(void) {
    const unsigned depth = symbol("_foregroundVeraDepth");
    const unsigned preserved_flags = FLAG_CARRY | FLAG_ZERO | FLAG_INTERRUPT |
        FLAG_DECIMAL | FLAG_CONSTANT | FLAG_OVERFLOW | FLAG_SIGN;

    for (unsigned irq_masked = 0; irq_masked <= 1; ++irq_masked) {
        const unsigned entries[] = {
            symbol("_b6EnterForegroundVera"), symbol("_b6EnterForegroundVera"),
            symbol("_b6LeaveForegroundVera"), symbol("_b6LeaveForegroundVera")
        };
        const uint8_t depths[] = {0, 1, 2, 1};
        const uint8_t expected_depths[] = {1, 2, 1, 0};

        ram[depth] = 0;
        for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
            const uint8_t saved_status = FLAG_CONSTANT | FLAG_DECIMAL | FLAG_CARRY |
                FLAG_ZERO | FLAG_OVERFLOW | FLAG_SIGN |
                (irq_masked ? FLAG_INTERRUPT : 0);
            ram[depth] = depths[i];
            init_cpu(entries[i], 0);
            regs.a = (uint8_t)(0xA5 + i);
            regs.x = (uint8_t)(0x5A + i);
            regs.y = (uint8_t)(0xC3 + i);
            regs.status = saved_status;
            run();

            CHECK(ram[depth] == expected_depths[i],
                  "Nested foreground VERA owner depth was not retained");
            CHECK(regs.a == (uint8_t)(0xA5 + i) && regs.x == (uint8_t)(0x5A + i) &&
                  regs.y == (uint8_t)(0xC3 + i),
                  "Foreground VERA owner changed a live CPU register");
            CHECK((regs.status & preserved_flags) == (saved_status & preserved_flags),
                  "Foreground VERA owner changed caller processor flags");
            CHECK(regs.sp == 0x1FF, "Foreground VERA owner changed the hardware stack");
        }
    }
}

static void write_word(unsigned address, unsigned value) {
    ram[address] = (uint8_t)value;
    ram[address + 1] = (uint8_t)(value >> 8);
}

static void test_sound_handler(void) {
    const unsigned channel_count = symbol("_bBChannelsPlaying");
    const unsigned playing = symbol("_bBIsPlaying");
    const unsigned psg = 0x1F9C0;
    const char *tick_names[] = {"_bBCh1Ticks", "_bBCh2Ticks", "_bBCh3Ticks", "_bBCh4Ticks"};
    const char *note_names[] = {"_ZP_CURRENTLY_PLAYING_NOTE_1", "_ZP_CURRENTLY_PLAYING_NOTE_2",
                                "_ZP_CURRENTLY_PLAYING_NOTE_3", "_ZP_CURRENTLY_PLAYING_NOTE_NOISE"};
    enum { SPARSE_SOUND_BASELINE_CYCLES = 254, SPARSE_SOUND_BUDGET_CYCLES = 250 };
    unsigned tick_addresses[4], note_addresses[4];
    bool sparse_channels_wrote = false, sound_channel_state_changed = false;
    bool end_channel_state_wrong = false, end_writes_wrong = false;
    uint32_t sparse_handler_cycles = 0;

    for (unsigned i = 0; i < 4; ++i) {
        tick_addresses[i] = symbol(tick_names[i]);
        note_addresses[i] = symbol(note_names[i]);
    }

    vram[psg - 1] = 0x5A;
    memset(vram + psg, 0xA5, 64);
    vram[psg + 64] = 0xC7;
    init_cpu(symbol("_bBPsgClear"), 0);
    uint32_t psg_clear_start = clockticks6502;
    run();
    uint32_t psg_clear_cycles = clockticks6502 - psg_clear_start;
    CHECK(writes == 64, "Startup/stop PSG clear did not write every system register");
    CHECK(data_port_reads == 0, "PSG clear read from a VERA data port");
    /* Baseline before loop unrolling: 611 cycles in this native fixture. */
    fprintf(stderr, "PSG clear: %u cycles for 64 register writes (baseline 611; budget < 400 cycles)\n", psg_clear_cycles);
    CHECK(psg_clear_cycles < 400, "Bounded 64-register PSG clear exceeded its 400-cycle IRQ-mask budget");
    for (unsigned i = 0; i < 64; ++i)
        CHECK(vram[psg + i] == 0 && write_addresses[i] == psg + i && write_values[i] == 0,
              "PSG clear left a stale register or used the wrong stride");
    CHECK(vram[psg - 1] == 0x5A && vram[psg + 64] == 0xC7,
          "PSG clear overwrote an adjacent register");

    ram[channel_count] = 0;
    unsigned after_clear_address = addr[0];
    init_cpu(symbol("bBSoundHandler"), 0);
    uint32_t idle_start = clockticks6502;
    run();
    CHECK(!writes && addr[0] == after_clear_address, "Idle sound handler still accessed PSG/VERA");
    CHECK(clockticks6502 - idle_start == 12,
          "Idle sound handler fast path exceeded 12 cycles");

    /* Inactive channels are already silent after init/stop or their one end transition. */
    for (unsigned mask = 1; mask < 16; ++mask) {
        unsigned active_count = 0;
        uint8_t expected_psg[64];
        memset(vram + psg, 0xA5, 64); /* Guard the unmanaged channels after AGI's four. */
        for (unsigned channel = 0; channel < 4; ++channel) {
            bool active = (mask & (1u << channel)) != 0;
            ram[playing + channel] = active ? 1 : 0;
            ram[tick_addresses[channel]] = active ? 1 : 0;
            ram[tick_addresses[channel] + 1] = 0;
            if (active) {
                ++active_count;
                for (unsigned reg = 0; reg < 4; ++reg)
                    vram[psg + channel * 4 + reg] = (uint8_t)(0x40 + channel * 4 + reg);
            } else {
                memset(vram + psg + channel * 4, 0, 4);
            }
        }
        memcpy(expected_psg, vram + psg, sizeof(expected_psg));
        ram[channel_count] = (uint8_t)active_count;
        init_cpu(symbol("bBSoundHandler"), 0);
        uint32_t handler_start = clockticks6502;
        run();
        uint32_t handler_cycles = clockticks6502 - handler_start;
        if (mask == 1) {
            sparse_handler_cycles = handler_cycles;
        }
        if (writes) sparse_channels_wrote = true;
        CHECK(ram[channel_count] == active_count,
              "Sound handler changed the active channel count while ticking notes");
        CHECK(addr[0] == 0x1F9D2,
              "Sound handler did not advance four channel slots from the PSG volume base");
        CHECK(data_port_reads == active_count * 4,
              "Sound handler changed the four-byte PSG slot advancement for active ticks");
        for (unsigned channel = 0; channel < 4; ++channel) {
            bool active = (mask & (1u << channel)) != 0;
            CHECK(ram[playing + channel] == active,
                  "Sound handler changed a channel flag while ticking notes");
            CHECK(ram[tick_addresses[channel]] == 0 && ram[tick_addresses[channel] + 1] == 0,
                  "Sound handler did not consume one tick on each active channel");
        }
        if (memcmp(expected_psg, vram + psg, sizeof(expected_psg)))
            sound_channel_state_changed = true;
    }

    /* Ending one slot must clear exactly its own four registers and preserve the next active slot. */
    for (unsigned ending = 0; ending < 4; ++ending) {
        unsigned end_note = 0x9000;
        memset(vram + psg, 0xA5, 64);
        for (unsigned channel = 0; channel < 4; ++channel) {
            ram[playing + channel] = 1;
            ram[tick_addresses[channel]] = channel == ending ? 0 : 1;
            ram[tick_addresses[channel] + 1] = 0;
            for (unsigned reg = 0; reg < 4; ++reg)
                vram[psg + channel * 4 + reg] = (uint8_t)(0x50 + channel * 4 + reg);
        }
        ram[channel_count] = 4;
        write_word(note_addresses[ending], end_note - 5);
        ram[end_note] = ram[end_note + 1] = 0xFF;
        init_cpu(symbol("bBSoundHandler"), 0);
        run();

        if (writes != 4) end_writes_wrong = true;
        else for (unsigned reg = 0; reg < 4; ++reg)
            if (write_addresses[reg] != psg + ending * 4 + reg || write_values[reg] != 0)
                end_writes_wrong = true;
        if (ram[channel_count] != 3 || ram[playing + ending] != 0 ||
            addr[0] != 0x1F9D2 || data_port_reads != 12)
            end_channel_state_wrong = true;
        for (unsigned channel = 0; channel < 4; ++channel) {
            for (unsigned reg = 0; reg < 4; ++reg) {
                uint8_t expected = channel == ending ? 0 : (uint8_t)(0x50 + channel * 4 + reg);
                if (vram[psg + channel * 4 + reg] != expected)
                    end_channel_state_wrong = true;
            }
        }
        for (unsigned i = 16; i < 64; ++i)
            if (vram[psg + i] != 0xA5) end_channel_state_wrong = true;
    }

    memset(ram + playing, 0, 4);
    ram[playing] = 1;
    ram[channel_count] = 1;
    ram[symbol("_bBEndSoundFlag")] = 3;
    ram[0x504] = 0;
    ram[tick_addresses[0]] = ram[tick_addresses[0] + 1] = 0;
    const unsigned note = 0x9000;
    write_word(note_addresses[0], note - 5);
    ram[note] = ram[note + 1] = 0xFF;
    memset(vram + psg, 0xA5, 64);
    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!ram[channel_count] && !ram[playing] && ram[0x504] == 1,
          "Final channel did not clear ownership and set its end flag");
    CHECK(writes == 4 && write_addresses[0] == psg && write_addresses[1] == psg + 1 &&
          write_addresses[2] == psg + 2 && write_addresses[3] == psg + 3,
          "Final channel did not silence exactly its own PSG registers");
    for (unsigned i = 0; i < 4; ++i)
        CHECK(vram[psg + i] == 0, "Final channel left an audible PSG register");
    CHECK(vram[psg + 4] == 0xA5 && vram[psg + 5] == 0xA5,
          "Final channel silence overwrote the following channel's frequency");

    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!writes, "Naturally ended sound did not reach the idle fast path");

    memset(ram + playing, 1, 4);
    ram[channel_count] = 4;
    for (unsigned i = 0; i < 4; ++i) {
        ram[tick_addresses[i]] = 1;
        ram[tick_addresses[i] + 1] = 0;
    }
    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!writes && ram[channel_count] == 4 && ram[playing] && ram[playing + 3],
          "Restarted sound did not take the active-channel path");

    memset(ram + playing, 0, 4); /* bBStopSound publishes these states after clearing PSG. */
    ram[channel_count] = 0;
    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!writes, "Stopped sound did not reach the idle fast path");

    fprintf(stderr,
            "Sparse sound IRQ mask $01: %u cycles (baseline %u, budget <%u); inactive PSG writes %s; end mute %s\n",
            sparse_handler_cycles, SPARSE_SOUND_BASELINE_CYCLES, SPARSE_SOUND_BUDGET_CYCLES,
            sparse_channels_wrote ? "observed" : "none",
            (end_writes_wrong || end_channel_state_wrong) ? "incorrect" : "per-channel");
    CHECK(sparse_handler_cycles < SPARSE_SOUND_BUDGET_CYCLES,
          "Sparse sound handler exceeded its 250-cycle budget (baseline 254)");
    CHECK(!end_writes_wrong && !end_channel_state_wrong,
          "Sound end transition did not clear its own PSG slot or overwrote an adjacent active channel");
    CHECK(!sparse_channels_wrote && !sound_channel_state_changed,
          "Already silent sound channels rewrote PSG state or changed an active channel");
}

static void test_sound_interrupt_restore(void) {
    ram[0x9F27] = 1;
    init_cpu(symbol("soundCriticalSection"), 0);
    run();
    CHECK(!(regs.status & FLAG_INTERRUPT) && ram[0x9F27] == 1 && !acks,
          "Sound critical section failed to restore enabled IRQ state or discarded VSYNC");

    ram[0x9F27] = 1;
    init_cpu(symbol("soundCriticalSection"), 0);
    regs.status |= FLAG_INTERRUPT;
    run();
    CHECK((regs.status & FLAG_INTERRUPT) && ram[0x9F27] == 1 && !acks,
          "Sound critical section failed to preserve disabled IRQ state or discarded VSYNC");
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

static void test_reenable_preserves_pending_isr(void) {
    for (unsigned bank = 0; bank < 256; ++bank) {
        ram[1] = (uint8_t)bank;
        ram[0x9F27] = 0xFF;
        init_cpu(symbol("enable"), 0); run();
        CHECK(acks == 0, "REENABLE_INTERRUPTS acknowledged a pending VSYNC");
        CHECK(ram[0x9F27] == 0xFF, "REENABLE_INTERRUPTS changed pending ISR bits");
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

static void test_atomic_vsync_snapshot(void) {
    const unsigned counter = symbol("_vSyncCounter");
    const unsigned high = symbol("_vSyncCounterHigh");
    const unsigned output = symbol("snapshotOutput");

    ram[counter] = 0x34;
    ram[counter + 1] = 0x12;
    ram[high] = 0xCD;
    ram[high + 1] = 0xAB;
    ram[0x9F27] = 1;
    init_cpu(symbol("snapshot32"), 0);
    regs.a = (uint8_t)output;
    regs.x = (uint8_t)(output >> 8);
    run();
    CHECK(ram[output] == 0x34 && ram[output + 1] == 0x12 &&
          ram[output + 2] == 0xCD && ram[output + 3] == 0xAB,
          "Atomic VSYNC snapshot did not copy all four bytes in order");
    CHECK(!(regs.status & FLAG_INTERRUPT), "Snapshot did not restore enabled caller IRQ state");
    CHECK(ram[0x9F27] == 1 && !acks, "Snapshot acknowledged or cleared pending VSYNC");

    init_cpu(symbol("snapshot32"), 0);
    regs.a = (uint8_t)output;
    regs.x = (uint8_t)(output >> 8);
    regs.status |= FLAG_INTERRUPT;
    run();
    CHECK(regs.status & FLAG_INTERRUPT, "Snapshot did not preserve disabled caller IRQ state");
    CHECK(ram[0x9F27] == 1 && !acks, "Snapshot with IRQ disabled changed pending VSYNC");
}


/* Assert VSYNC at each instruction boundary, then service the real dispatcher
 * whenever I allows it. A snapshot may be old or new, but never torn. */
static void test_snapshot_interrupt_boundaries(void) {
    const unsigned counter = symbol("_vSyncCounter"), high = symbol("_vSyncCounterHigh");
    const unsigned output = symbol("snapshotOutput");
    const uint32_t starts[] = {0xFF, 0xFFFF, 0xFFFFFF, UINT32_MAX};
    unsigned instruction_count = 0;
    init_cpu(symbol("snapshot32"), output >> 8);
    regs.a = (uint8_t)output;
    while (regs.pc != symbol("done") && instruction_count < 100) {
        step6502(); ++instruction_count;
    }
    CHECK(regs.pc == symbol("done"), "Cannot determine snapshot instruction boundaries");

    for (unsigned masked = 0; masked < 2; ++masked)
        for (unsigned value = 0; value < sizeof(starts) / sizeof(starts[0]); ++value)
            for (unsigned boundary = 0; boundary <= instruction_count; ++boundary) {
                uint32_t before = starts[value];
                ram[counter] = (uint8_t)before; ram[counter + 1] = (uint8_t)(before >> 8);
                ram[high] = (uint8_t)(before >> 16); ram[high + 1] = (uint8_t)(before >> 24);
                memset(ram + output, 0xA5, 4);
                ram[symbol("_foregroundVeraDepth")] = 1;
                ram[symbol("default_irq_vector")] = (uint8_t)symbol("irqDefault");
                ram[symbol("default_irq_vector") + 1] = (uint8_t)(symbol("irqDefault") >> 8);
                ram[0xFFFE] = (uint8_t)symbol("irqEntry");
                ram[0xFFFF] = (uint8_t)(symbol("irqEntry") >> 8);
                ram[0] = 0x35; ram[1] = 0x74; ram[0x9F27] = 2;
                init_cpu(symbol("snapshot32"), output >> 8);
                regs.a = (uint8_t)output;
                regs.status = FLAG_CONSTANT | FLAG_DECIMAL | FLAG_CARRY |
                    (masked ? FLAG_INTERRUPT : 0);
                bool asserted = false, in_irq = false;
                unsigned foreground_steps = 0, resume_pc = 0, resume_sp = 0;
                for (unsigned steps = 0; steps < 20000; ++steps) {
                    if (!in_irq && !asserted && foreground_steps == boundary) {
                        ram[0x9F27] |= 1; asserted = true;
                    }
                    if (!in_irq && (ram[0x9F27] & 1) && !(regs.status & FLAG_INTERRUPT)) {
                        resume_pc = regs.pc; resume_sp = regs.sp;
                        irq6502(); in_irq = true;
                    } else if (!in_irq && regs.pc == symbol("done")) break;
                    else {
                        step6502();
                        if (in_irq) {
                            if (regs.pc == resume_pc && regs.sp == resume_sp) in_irq = false;
                        } else ++foreground_steps;
                    }
                }
                uint32_t snapshot_value = ram[output] | ((uint32_t)ram[output + 1] << 8) |
                    ((uint32_t)ram[output + 2] << 16) | ((uint32_t)ram[output + 3] << 24);
                uint32_t actual = ram[counter] | ((uint32_t)ram[counter + 1] << 8) |
                    ((uint32_t)ram[high] << 16) | ((uint32_t)ram[high + 1] << 24);
                CHECK(asserted && regs.pc == symbol("done") && !in_irq,
                      "Snapshot interleaving did not complete");
                CHECK(snapshot_value == before || (!masked && snapshot_value == before + 1),
                      "Interrupt at a snapshot boundary produced a torn counter");
                CHECK(actual == before + !masked && acks == !masked && ram[0x9F27] == (masked ? 3 : 2),
                      "Snapshot interleaving lost VSYNC or acknowledged an unrelated source");
                CHECK(regs.sp == 0x1FF && ram[0] == 0x35 && ram[1] == 0x74 &&
                    (regs.status & (FLAG_INTERRUPT | FLAG_DECIMAL | FLAG_CARRY)) ==
                    (FLAG_DECIMAL | FLAG_CARRY | (masked ? FLAG_INTERRUPT : 0)),
                      "Snapshot interleaving changed caller state");
            }
    ram[symbol("_foregroundVeraDepth")] = 0;
    printf("Atomic snapshot passes %u instruction boundaries, both I states and every carry boundary\n",
           instruction_count + 1);
}

/* Execute the entire production dispatcher, including its ownership fast path.
 * irqEntry/irqDefault provide only the ROM register-save/restore convention. */
static void test_irq_counter_carries(void) {
    const uint32_t starts[] = {0xFF, 0xFFFF, 0xFFFFFF, UINT32_MAX};
    unsigned counter = symbol("_vSyncCounter"), high = symbol("_vSyncCounterHigh");
    unsigned status_bar_showing = symbol("b10StatusBarShowing");
    unsigned sound_playing = symbol("_bBIsPlaying");
    unsigned sound_ticks = symbol("_bBCh1Ticks");
    for (unsigned owner = 0; owner <= 2; ++owner)
        for (unsigned i = 0; i < sizeof(starts)/sizeof(starts[0]); ++i)
            for (unsigned pending = 2; pending <= 3; ++pending) {
                uint32_t before = starts[i], expected = before + (pending & 1);
                ram[counter] = (uint8_t)before; ram[counter + 1] = (uint8_t)(before >> 8);
                ram[high] = (uint8_t)(before >> 16); ram[high + 1] = (uint8_t)(before >> 24);
                ram[symbol("_foregroundVeraDepth")] = (uint8_t)owner;
                ram[symbol("irqWorkKind")] = ram[symbol("sendIrqCommand")] = 0;
                ram[symbol("lastMainLoopCounter")] = ram[symbol("_mainLoopCounter")] = 0;
                ram[symbol("_inputLineDisplayed")] = ram[symbol("_inputLineDirty")] = 0;
                ram[symbol("_bBChannelsPlaying")] = ram[symbol("_bBEndSoundFlag")] = 0;
                memset(ram + sound_playing, 0, 4);
                memset(ram + sound_ticks, 0, 8);
                ram[symbol("_menuDirty")] = 0;
                ram[symbol("_menuAllowed")] = ram[symbol("_menuShown")] = 1;
                ram[0x400 + 257 + 14] = 1;
                ram[status_bar_showing] = 1;
                ram[symbol("default_irq_vector")] = (uint8_t)symbol("irqDefault");
                ram[symbol("default_irq_vector") + 1] = (uint8_t)(symbol("irqDefault") >> 8);
                init_cpu(symbol("done"), 0x42);
                regs.a = 0x31; regs.y = 0x53; regs.sp = 0x1FF;
                regs.status = FLAG_CONSTANT | FLAG_CARRY | FLAG_DECIMAL | FLAG_OVERFLOW;
                ram[0] = 0x35; ram[1] = 0x74; ram[0x9F27] = (uint8_t)pending;
                ram[0xFFFE] = (uint8_t)symbol("irqEntry"); ram[0xFFFF] = (uint8_t)(symbol("irqEntry") >> 8);
                irq6502(); run();
                uint32_t actual = ram[counter] | ((uint32_t)ram[counter+1] << 8) |
                    ((uint32_t)ram[high] << 16) | ((uint32_t)ram[high+1] << 24);
                CHECK(actual == expected, "Production IRQ counter carry/wrap or non-VSYNC accounting failed");
                CHECK(acks == (pending & 1) && ram[0x9F27] == 2,
                      "Production IRQ did not acknowledge exactly one VSYNC while preserving other pending bits");
                CHECK(ram[0] == 0x35 && ram[1] == 0x74 && regs.a == 0x31 && regs.x == 0x42 && regs.y == 0x53 && regs.sp == 0x1FF,
                      "Production IRQ did not restore caller registers, stack, or banks");
                /* B is not a physical W65C02 flag; the core sets it on RTI. */
                CHECK((regs.status & ~FLAG_BREAK) == (FLAG_CONSTANT | FLAG_CARRY | FLAG_DECIMAL | FLAG_OVERFLOW),
                      "Production IRQ did not restore caller flags");
                const unsigned expected_normal_work = !owner && (pending & 1);
                if (ram[status_bar_showing] != !expected_normal_work || writes)
                    fprintf(stderr, "IRQ carry owner=%u pending=%u statusBar=%u expected=%u writes=%u menuAllowed=%u menuShown=%u\n",
                            owner, pending, ram[status_bar_showing], !expected_normal_work, writes,
                            ram[symbol("_menuAllowed")], ram[symbol("_menuShown")]);
                if (writes) {
                    for (unsigned w = 0; w < writes && w < 16; ++w)
                        fprintf(stderr, "  VERA write[%u]=$%05X:$%02X\n", w, write_addresses[w], write_values[w]);
                }
                CHECK(ram[status_bar_showing] == !expected_normal_work && !writes,
                      "Foreground-owned or non-VSYNC IRQ executed normal work");
            }
    ram[symbol("_foregroundVeraDepth")] = 0;
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

static void seed_sprite_update_buffer(void) {
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    const unsigned end = symbol("spriteBufferEnd");
    CHECK(end - base == SPRITE_UPDATED_BUFFER_SIZE, "Assembly and C sprite buffer sizes differ");
    CHECK(symbol("spriteUpdateBufferBytes") == SPRITE_UPDATED_BUFFER_SIZE &&
          symbol("spriteUpdateRecordBytes") == SPRITE_UPDATE_RECORD_BYTES &&
          symbol("spriteUpdateMaxRecords") == SPRITE_UPDATE_MAX_RECORDS,
          "Assembly sprite update contract differs from the C header");
    CHECK(symbol("spriteBufferBefore") + 1 == base &&
          symbol("spriteBufferGuard") == end,
          "Sprite update buffer guards are not adjacent to the buffer");
    CHECK(SPRITE_UPDATED_BUFFER_SIZE == 891 && SPRITE_UPDATE_MAX_RECORDS == 127,
          "Sprite update contract no longer fits the VERA sprite limit");

    ram[symbol("spriteBufferBefore")] = 0x3D;
    memset(ram + base, 0xCC, SPRITE_UPDATED_BUFFER_SIZE);
    ram[symbol("spriteBufferGuard")] = 0xA6;
    ram[symbol("spriteBufferGuard") + 1] = 0x5B;
    write_word(symbol("_bESpritesUpdatedBufferPointer"), base);
    ram[symbol("_bESpriteUpdateOverflowed")] = 0;
}

static void stage_sprite_update(unsigned index) {
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    const unsigned pointer_address = symbol("_bESpritesUpdatedBufferPointer");
    unsigned pointer = ram[pointer_address] | ((unsigned)ram[pointer_address + 1] << 8);
    uint8_t record[SPRITE_UPDATE_RECORD_BYTES] = {
        (uint8_t)(0x80 | (index & 0x7F)),
        (uint8_t)(0x20 | (index >> 7)),
        (uint8_t)(index + 0x11),
        (uint8_t)(0x40 | index),
        (uint8_t)(index + 0x31),
        (uint8_t)(0x80 | (index & 1)),
        (uint8_t)(0x60 | (index & 0x1F))
    };

    CHECK(pointer == base + index * SPRITE_UPDATE_RECORD_BYTES,
          "Sprite producer pointer did not advance by one complete record");
    CHECK(pointer + SPRITE_UPDATE_RECORD_BYTES <= base + SPRITE_UPDATE_MAX_RECORDS * SPRITE_UPDATE_RECORD_BYTES,
          "Sprite record would overlap the terminator");
    memcpy(ram + pointer, record, sizeof(record));
    write_word(pointer_address, pointer + SPRITE_UPDATE_RECORD_BYTES);
}

static unsigned max_sprite_consumer_cycles;
static unsigned max_full_sprite_consumer_cycles;

static void test_sprite_update_batch(unsigned requested, bool write_terminator) {
    const unsigned pointer_address = symbol("_bESpritesUpdatedBufferPointer");
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    const unsigned sprite_bank = symbol("spriteUpdateBank");
    const unsigned accepted = requested < SPRITE_UPDATE_MAX_RECORDS ? requested : SPRITE_UPDATE_MAX_RECORDS;
    unsigned produced = 0;

    seed_sprite_update_buffer();
    ram[0] = (uint8_t)sprite_bank;

    /* Split work as the cel blitter does: no more than sixteen records per part. */
    while (produced < requested) {
        unsigned batch = requested - produced;
        if (batch > 16) batch = 16;
        for (unsigned i = 0; i < batch; ++i) {
            unsigned pointer = ram[pointer_address] | ((unsigned)ram[pointer_address + 1] << 8);
            init_cpu(symbol("checkSpriteUpdateCapacity"), 0);
            run();
            CHECK(ram[0] == sprite_bank, "Capacity check changed the RAM bank");
            if (produced < SPRITE_UPDATE_MAX_RECORDS) {
                CHECK(regs.status & FLAG_CARRY, "Capacity helper rejected a representable sprite");
                stage_sprite_update(produced);
            } else {
                CHECK(!(regs.status & FLAG_CARRY), "Capacity helper admitted a sprite past VERA's limit");
                CHECK((ram[pointer_address] | ((unsigned)ram[pointer_address + 1] << 8)) == pointer,
                      "Rejected sprite advanced the published buffer pointer");
            }
            ++produced;
        }
    }

    CHECK((ram[symbol("_bESpriteUpdateOverflowed")] != 0) == (requested > SPRITE_UPDATE_MAX_RECORDS),
          "Sprite overflow indicator does not match the unrepresentable tail");

    unsigned pointer = ram[pointer_address] | ((unsigned)ram[pointer_address + 1] << 8);
    CHECK(pointer == base + accepted * SPRITE_UPDATE_RECORD_BYTES,
          "Sprite producer pointer does not end after the accepted records");
    CHECK(pointer + SPRITE_UPDATE_TERMINATOR_BYTES <= base + SPRITE_UPDATED_BUFFER_SIZE,
          "Sprite terminator would exceed the buffer");
    if (write_terminator) {
        ram[pointer] = 0;
        ram[pointer + 1] = 0;
    }

    const unsigned first_z = symbol("spriteFirstZ");
    const unsigned first_sprite = first_z - 6;
    const unsigned mouse = first_sprite - 8;
    memset(vram, 0xE1, 8);
    memset(vram + mouse, 0xA5, 8);
    for (unsigned i = 0; i < SPRITE_UPDATE_MAX_RECORDS; ++i)
        vram[first_z + i * 8] = 0x0C;

    sprite_read_start = base;
    sprite_read_end = symbol("spriteBufferEnd");
    sprite_read_limit = base + accepted * SPRITE_UPDATE_RECORD_BYTES +
        (accepted < SPRITE_UPDATE_MAX_RECORDS ? SPRITE_UPDATE_TERMINATOR_BYTES : 0);
    sprite_reads = 0; track_sprite_reads = true;
    init_cpu(symbol("handleSpriteUpdates"), 0);
    uint32_t consumer_start = clockticks6502;
    run();
    uint32_t consumer_cycles = clockticks6502 - consumer_start;
    track_sprite_reads = false;
    CHECK(sprite_reads == sprite_read_limit - base, "Sprite consumer did not read exactly its complete records");
    if (consumer_cycles > max_sprite_consumer_cycles)
        max_sprite_consumer_cycles = consumer_cycles;
    if (accepted == SPRITE_UPDATE_MAX_RECORDS &&
        consumer_cycles > max_full_sprite_consumer_cycles)
        max_full_sprite_consumer_cycles = consumer_cycles;
    CHECK(ram[0] == sprite_bank, "Sprite consumer changed the RAM bank");
    CHECK((ram[pointer_address] | ((unsigned)ram[pointer_address + 1] << 8)) == base,
          "Sprite consumer did not reset the published pointer");
    CHECK(writes == SPRITE_UPDATE_MAX_RECORDS + accepted * 8 +
                   (write_terminator && accepted < SPRITE_UPDATE_MAX_RECORDS ? 2 : 0),
          "Sprite consumer wrote an unexpected number of VERA attributes");
    CHECK(consumer_cycles < 30000, "127-record sprite consumer exceeded its cycle budget");

    for (unsigned i = 0; i < accepted; ++i) {
        unsigned record = base + i * SPRITE_UPDATE_RECORD_BYTES;
        unsigned attributes = first_sprite + i * 8;
        CHECK(vram[attributes] == ram[record] && vram[attributes + 1] == ram[record + 1] &&
              vram[attributes + 2] == ram[record + 2] && vram[attributes + 3] == ram[record + 3] &&
              vram[attributes + 4] == ram[record + 4] && vram[attributes + 5] == 0 &&
              vram[attributes + 6] == ram[record + 5] && vram[attributes + 7] == ram[record + 6],
              "Sprite consumer lost or shifted an update record");
    }
    for (unsigned i = 0; i < 8; ++i)
        CHECK(vram[mouse + i] == 0xA5, "Sprite consumer clobbered the mouse attributes");
    for (unsigned i = 0; i < 8; ++i)
        CHECK(vram[i] == 0xE1, "Sprite consumer wrapped writes around VERA memory");
    CHECK(ram[symbol("spriteBufferBefore")] == 0x3D &&
          ram[symbol("spriteBufferGuard")] == 0xA6 &&
          ram[symbol("spriteBufferGuard") + 1] == 0x5B,
          "Sprite producer or consumer crossed the staged buffer bounds");
    if (write_terminator)
        CHECK(ram[pointer] == 0 && ram[pointer + 1] == 0,
              "Sprite terminator was overwritten while consuming the buffer");
    else
        CHECK(ram[pointer] == 0xCC && ram[pointer + 1] == 0xCC,
              "Sprite consumer read or wrote past the 127-record limit");
}

static void test_sprite_updates(void) {
    const unsigned requested[] = {36, 37, 127, 128, 200};
    for (unsigned i = 0; i < sizeof(requested) / sizeof(requested[0]); ++i)
        test_sprite_update_batch(requested[i], requested[i] != SPRITE_UPDATE_MAX_RECORDS);
    fprintf(stderr,
            "127-record sprite consumer: %u cycles (baselines 24276/19074; limit 18200)\n",
            max_full_sprite_consumer_cycles);
    CHECK(max_full_sprite_consumer_cycles <= 18200,
          "127-record sprite consumer exceeded the 18200-cycle budget (baseline 19074)");
}

static void test_sprite_snapshot_reset(void) {
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    const unsigned pointer = symbol("_bESpritesUpdatedBufferPointer");
    const unsigned overflow = symbol("_bESpriteUpdateOverflowed");
    seed_sprite_update_buffer();
    write_word(pointer, base + 4 * SPRITE_UPDATE_RECORD_BYTES);
    ram[overflow] = 1;
    ram[base + 7] = 0xC7;
    ram[0] = 0x14;
    ram[1] = 0x2A;

    init_cpu(symbol("bEBeginSpriteSnapshot"), 0);
    run();

    CHECK((ram[pointer] | ((unsigned)ram[pointer + 1] << 8)) == base,
          "sprite_snapshot_reset: publication pointer did not return to the empty snapshot base");
    CHECK(ram[overflow] == 0,
          "sprite_snapshot_reset: overflow flag was not cleared for the new snapshot");
    CHECK(ram[base + 7] == 0xC7,
          "sprite_snapshot_reset: reset changed stale payload outside the published extent");
    CHECK(ram[symbol("spriteBufferBefore")] == 0x3D &&
          ram[symbol("spriteBufferGuard")] == 0xA6 && ram[symbol("spriteBufferGuard") + 1] == 0x5B,
          "sprite_snapshot_reset: reset crossed a sprite buffer canary");
    CHECK(ram[0] == 0x14 && ram[1] == 0x2A,
          "sprite_snapshot_reset: reset changed caller RAM or ROM bank");
    puts("sprite_snapshot_reset passed");
}

static void stage_irq_sprite(void) {
    const unsigned first_z = symbol("spriteFirstZ");
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    seed_sprite_update_buffer();
    stage_sprite_update(0);
    ram[base + SPRITE_UPDATE_RECORD_BYTES] = 0;
    ram[base + SPRITE_UPDATE_RECORD_BYTES + 1] = 0;
    for (unsigned i = 0; i < SPRITE_UPDATE_MAX_RECORDS; ++i)
        vram[first_z + i * 8] = 0x0C;
}

static void check_irq_sprite(void) {
    const unsigned base = symbol("_bESpritesUpdatedBuffer");
    const unsigned first_sprite = symbol("spriteFirstZ") - 6;
    const unsigned pointer = symbol("_bESpritesUpdatedBufferPointer");
    CHECK((ram[pointer] | ((unsigned)ram[pointer + 1] << 8)) == base,
          "Mixed IRQ sprite batch was not consumed and reset");
    CHECK(vram[first_sprite] == ram[base] && vram[first_sprite + 1] == ram[base + 1] &&
          vram[first_sprite + 2] == ram[base + 2] && vram[first_sprite + 3] == ram[base + 3] &&
          vram[first_sprite + 4] == ram[base + 4] && vram[first_sprite + 5] == 0 &&
          vram[first_sprite + 6] == ram[base + 5] && vram[first_sprite + 7] == ram[base + 6],
          "Mixed IRQ lost or shifted its sprite update");
}

static unsigned run_mixed_irq(unsigned *masked_cycles) {
    ram[symbol("default_irq_vector")] = (uint8_t)symbol("irqDefault");
    ram[symbol("default_irq_vector") + 1] = (uint8_t)(symbol("irqDefault") >> 8);
    ram[0xFFFE] = (uint8_t)symbol("irqEntry");
    ram[0xFFFF] = (uint8_t)(symbol("irqEntry") >> 8);
    ram[0x9F27] = 1;
    uint32_t start = clockticks6502;
    irq6502();
    CHECK(regs.status & FLAG_INTERRUPT, "Combined production IRQ did not mask interrupts on entry");
    unsigned entry_cycles = (unsigned)(clockticks6502 - start);
    unsigned masked = 0;
    for (unsigned steps = 0; steps < 100000 && regs.pc != symbol("done"); ++steps) {
        CHECK(!waiting, "Unexpected WAI during combined IRQ workload");
        CHECK(regs.status & FLAG_INTERRUPT,
              "Combined production IRQ cleared the interrupt mask before RTI");
        uint32_t step_start = clockticks6502;
        step6502();
        masked += (unsigned)(clockticks6502 - step_start);
    }
    CHECK(regs.pc == symbol("done"), "Combined production IRQ did not return");
    unsigned elapsed = (unsigned)(clockticks6502 - start);
    CHECK(masked + entry_cycles == elapsed,
          "Combined production IRQ cycle measurement escaped the dynamically checked masked interval");
    *masked_cycles = masked + entry_cycles;
    CHECK(*masked_cycles == elapsed,
          "combined_irq_acceptance_to_rti_masked_cycles: reported interval omitted IRQ acceptance/entry cycles");
    return elapsed;
}

static void test_combined_irq_workload(void) {
    enum { TEXT_SOURCE = 0x9000, TEXT_DESTINATION = 0x5000, PALETTE_ADDRESS = 0x1F000,
           TEXT_LENGTH = 511, FRAME_CYCLES = 134400, PSG_ADDRESS = 0x1F9C0,
           MENU_MAX_TOP_LEVELS = 10, MENU_TEXT_BUFFER_BYTES = 500,
           MENU_MAX_CHILDREN = 15, MENU_MAX_WIDTH = 40,
           MENU_CLEAR_WRITES = 2176, MENU_BAR_WRITES = 80,
           MENU_CHILD_WRITES = (MENU_MAX_CHILDREN + 2) * MENU_MAX_WIDTH * 2,
           SPRITE_BATCH_WRITES = 127 + 8 + 2 };
    const unsigned counter = symbol("_vSyncCounter");
    const unsigned completion = symbol("irqCompletionGeneration");
    const unsigned playing = symbol("_bBIsPlaying");
    const unsigned ticks = symbol("_bBCh1Ticks");
    const unsigned status_bar_showing = symbol("b10StatusBarShowing");
    const unsigned palette_rows = symbol("_b3PaletteRows");
    const unsigned palette_number = symbol("_b3PaletteNumber");
    const unsigned palette_address = symbol("_b3PaletteAddress");
    const unsigned text_source = symbol("_currentTextBuffer");
    const unsigned text_destination = symbol("_displayTextAddressToCopyTo");
    const unsigned text_cursor = symbol("displayTextSourcePointer");
    const unsigned palette_cursor = symbol("displayPaletteCursor");
    const unsigned menu_records = symbol("_the_menu");
    const unsigned menu_children = symbol("_the_menuChildren");
    const unsigned menu_text_buffer = symbol("_menuTextBuffer");
    const unsigned menu_child_heads = symbol("_bAFirstMenuChild");
    const unsigned menu_child_widths = symbol("_bAMenuChildWidth");
    const unsigned menu_child_counts = symbol("_bAMenuChildCount");
    const unsigned menu_controllers = symbol("_bAEnabledMenuControllers");

    init_cpu(symbol("done"), 0x42);
    clockticks6502 = 0;
    fx_write = false;
    memset(fx_cache, 0xA5, sizeof(fx_cache));

    memset(vram, 0x5A, sizeof(vram));
    memset(ram + TEXT_SOURCE, 'A', TEXT_LENGTH);
    ram[TEXT_SOURCE + TEXT_LENGTH] = 0;
    write_word(text_source, TEXT_SOURCE);
    ram[text_source + 2] = ram[text_source + 3] = 0;
    ram[text_destination] = (uint8_t)TEXT_DESTINATION;
    ram[text_destination + 1] = (uint8_t)(TEXT_DESTINATION >> 8);
    ram[text_destination + 2] = 0;
    ram[text_destination + 3] = 0;
    ram[palette_address] = (uint8_t)PALETTE_ADDRESS;
    ram[palette_address + 1] = (uint8_t)(PALETTE_ADDRESS >> 8);
    ram[palette_address + 2] = (uint8_t)(PALETTE_ADDRESS >> 16);
    ram[palette_address + 3] = 0;
    ram[palette_rows] = 1;
    ram[palette_number] = 0x0A;
    ram[symbol("irqWorkKind")] = 0;
    ram[symbol("irqTextStage")] = 0;
    ram[symbol("sendIrqCommand")] = 4;
    ram[completion] = 0xFF;
    ram[completion + 1] = 0;
    ram[counter] = 0xFF;
    ram[counter + 1] = ram[symbol("_vSyncCounterHigh")] = ram[symbol("_vSyncCounterHigh") + 1] = 0;
    ram[symbol("_foregroundVeraDepth")] = 0;
    ram[symbol("_menuDirty")] = 1;
    ram[status_bar_showing] = 1;
    ram[symbol("_inputLineDisplayed")] = ram[symbol("_inputLineDirty")] = 0;
    ram[symbol("_mainLoopCounter")] = 1;
    ram[symbol("lastMainLoopCounter")] = 0;

    ram[symbol("_charSetInited")] = 1;
    ram[symbol("_numOfMenus")] = MENU_MAX_TOP_LEVELS;
    ram[symbol("_numOfMenus") + 1] = 0;
    ram[symbol("_menuAllowed")] = ram[symbol("_menuShown")] = 1;
    ram[0x400 + 257 + 14] = 1; /* ENABLE_MENU interpreter flag */
    ram[symbol("_bAMenuSelected")] = ram[symbol("_bAMenuChildSelected")] = 0;
    memset(ram + menu_child_widths, 0, 10);
    memset(ram + menu_child_counts, 0, 10);
    memset(ram + symbol("_bAMenuChildShiftBack"), 0, 10);
    memset(ram + menu_controllers, 1, 256);
    ram[menu_child_widths] = MENU_MAX_WIDTH;
    ram[menu_child_counts] = MENU_MAX_CHILDREN;
    unsigned menu_text_cursor = menu_text_buffer;
    for (unsigned i = 0; i < MENU_MAX_TOP_LEVELS; ++i) {
        write_word(menu_records + i * 3, menu_text_cursor);
        ram[menu_records + i * 3 + 2] = 0;
        ram[menu_text_cursor++] = (uint8_t)('A' + i);
        ram[menu_text_cursor++] = 0;
    }
    write_word(menu_child_heads, menu_children);
    memset(ram + menu_children, 0, 3 * MENU_MAX_CHILDREN);
    unsigned child_lengths[MENU_MAX_CHILDREN] = {38, 32, 32, 32, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31};
    for (unsigned i = 0; i < MENU_MAX_CHILDREN; ++i) {
        write_word(menu_children + i * 3, menu_text_cursor);
        ram[menu_children + i * 3 + 2] = 0;
        memset(ram + menu_text_cursor, 'T', child_lengths[i]);
        menu_text_cursor += child_lengths[i];
        ram[menu_text_cursor++] = 0;
    }
    CHECK(menu_text_cursor == menu_text_buffer + MENU_TEXT_BUFFER_BYTES,
          "Combined menu fixture no longer fits menu.c's 500-byte text buffer");

    memset(ram + playing, 0, 4);
    ram[playing] = 1;
    ram[symbol("_bBChannelsPlaying")] = 1;
    ram[ticks] = 3;
    ram[ticks + 1] = 0;
    for (unsigned i = 0; i < 4; ++i) vram[PSG_ADDRESS + i] = (uint8_t)(0x40 + i);
    stage_irq_sprite();
    ram[0] = 0x35;
    ram[1] = 0x74;
    regs.sp = 0x1FF;
    regs.status = FLAG_CONSTANT | FLAG_CARRY | FLAG_DECIMAL;
    const unsigned initial_vsync = ram[counter] | ((unsigned)ram[counter + 1] << 8);

    unsigned first_masked_cycles;
    unsigned first_cycles = run_mixed_irq(&first_masked_cycles);
    CHECK(first_masked_cycles < FRAME_CYCLES,
          "Mixed text-copy IRQ exceeded the 134400-cycle VSYNC frame budget");
    CHECK(acks == 1 && ram[0x9F27] == 0,
          "Mixed text-copy IRQ did not acknowledge its VSYNC exactly once");
    CHECK((ram[counter] | ((unsigned)ram[counter + 1] << 8)) == 0x0100,
          "Mixed text-copy IRQ lost the VSYNC counter carry");
    CHECK(ram[completion] == 0xFF && ram[completion + 1] == 0 &&
          ram[symbol("irqWorkKind")] == 2 && ram[symbol("irqTextStage")] == 1,
          "Text-copy IRQ completed early or lost its pending palette stage");
    const unsigned first_writes = MENU_CLEAR_WRITES + MENU_BAR_WRITES + MENU_CHILD_WRITES +
        SPRITE_BATCH_WRITES + TEXT_LENGTH;
    CHECK(writes == first_writes,
          "Text-copy IRQ lost real menu, sprite, or text writes");
    CHECK(data_port_reads == 127 + 4,
          "Text-copy IRQ did not perform the expected sprite-clear and sound PSG reads");
    CHECK(ram[ticks] == 2 && ram[ticks + 1] == 0,
          "Text-copy IRQ failed to consume exactly one active sound tick");
    CHECK(!ram[symbol("_menuDirty")] && !ram[status_bar_showing],
          "Text-copy IRQ skipped the real dirty-menu or status-bar dispatcher work");
    CHECK(ram[symbol("lastMainLoopCounter")] == 1,
          "Text-copy IRQ failed to service the pending sprite frame");
    check_irq_sprite();
    for (unsigned i = 0; i < MENU_MAX_TOP_LEVELS; ++i) {
        unsigned address = 0xDA00 + i * 4;
        CHECK(vram[address] == (uint8_t)('A' + i) &&
              vram[address + 1] == (i == 0 ? 0x20 : 0x10) &&
              vram[address + 2] == 0x20 && vram[address + 3] == 0x10,
              "Production menu renderer did not draw all nonempty top-level titles");
    }
    CHECK(vram[0xDA80] == 0x82 && vram[0xDA81] == 0x10 &&
          vram[0xDB00] == 0x84 && vram[0xDB01] == 0x10 &&
          vram[0xDB02] == 'T' && vram[0xDB03] == 0x20 &&
          vram[0xDB4E] == 0x84 && vram[0xDB4F] == 0x14,
          "Production menu renderer lost its maximum-width child row or border");
    for (unsigned row = 0; row < MENU_MAX_CHILDREN + 2; ++row) {
        for (unsigned column = MENU_MAX_WIDTH; column < 64; ++column) {
            const unsigned address = 0xDA80 + row * 128 + column * 2;
            CHECK(vram[address] == 0x80 && vram[address + 1] == 0x10,
                  "VERA FX cache clear did not expand its 4-byte pattern across the full child area");
        }
    }
    CHECK(ram[text_cursor] == (uint8_t)(TEXT_SOURCE + TEXT_LENGTH) &&
          ram[text_cursor + 1] == (uint8_t)((TEXT_SOURCE + TEXT_LENGTH) >> 8),
          "Text-copy IRQ did not persist the source cursor at its terminator");
    for (unsigned i = 0; i < TEXT_LENGTH; ++i)
        CHECK(vram[TEXT_DESTINATION + i * 2] == 'A' &&
              vram[TEXT_DESTINATION + i * 2 + 1] == 0x5A,
              "Text-copy IRQ dropped a tile or changed its attribute byte");
    CHECK(ram[0] == 0x35 && ram[1] == 0x74,
          "Text-copy IRQ did not restore the caller's RAM and ROM banks");
    CHECK(!fx_write && ram[0x9F25] == 0,
          "Production menu left VERA FX cache writes or a nondefault port enabled");

    ram[symbol("_mainLoopCounter")] = 2;
    ram[symbol("_menuDirty")] = 1;
    ram[status_bar_showing] = 1;
    stage_irq_sprite();
    const unsigned second_write_start = writes;
    const unsigned second_read_start = data_port_reads;
    const unsigned first_vsync = ram[counter] | ((unsigned)ram[counter + 1] << 8);
    unsigned palette_masked_cycles;
    unsigned palette_cycles = run_mixed_irq(&palette_masked_cycles);
    CHECK(palette_masked_cycles < FRAME_CYCLES,
          "Mixed palette IRQ exceeded the 134400-cycle VSYNC frame budget");
    CHECK(acks == 2 && ram[0x9F27] == 0 &&
          (ram[counter] | ((unsigned)ram[counter + 1] << 8)) == 0x0101,
          "Mixed palette IRQ lost or double-counted its serviced VSYNC");
    CHECK(ram[completion] == 0 && ram[completion + 1] == 1 &&
          ram[symbol("irqWorkKind")] == 0 && ram[symbol("_inputLineDirty")] == 1,
          "Following palette IRQ did not finish the text command exactly once");
    CHECK(writes - second_write_start == MENU_CLEAR_WRITES + MENU_BAR_WRITES +
          MENU_CHILD_WRITES + SPRITE_BATCH_WRITES + 64,
          "Palette IRQ lost real menu, sprite, or palette writes");
    CHECK(data_port_reads - second_read_start == 127 + 4,
          "Palette IRQ did not perform the expected sprite-clear and sound PSG reads");
    CHECK(ram[ticks] == 1 && ram[ticks + 1] == 0,
          "Palette IRQ failed to consume exactly one active sound tick");
    CHECK(!ram[symbol("_menuDirty")] && !ram[status_bar_showing] &&
          ram[symbol("lastMainLoopCounter")] == 2,
          "Palette IRQ skipped overlapped real menu, status-bar, or sprite work");
    check_irq_sprite();
    CHECK(ram[palette_rows] == 1 && ram[palette_cursor] == (uint8_t)(PALETTE_ADDRESS + 128) &&
          ram[palette_cursor + 1] == (uint8_t)((PALETTE_ADDRESS + 128) >> 8) &&
          ram[palette_cursor + 2] == 1,
          "Palette IRQ did not persist its completed 17-bit palette cursor");
    for (unsigned i = 0; i < 64; ++i)
        CHECK(vram[PALETTE_ADDRESS + i * 2] == 0xA0 &&
              vram[PALETTE_ADDRESS + i * 2 + 1] == 0x5A,
              "Palette IRQ dropped a color write or changed stride padding");
    CHECK(ram[0] == 0x35 && ram[1] == 0x74,
          "Palette IRQ did not restore the caller's RAM and ROM banks");

    const unsigned total_cycles = first_cycles + palette_cycles;
    const unsigned max_masked_cycles = first_masked_cycles > palette_masked_cycles ?
        first_masked_cycles : palette_masked_cycles;
    const unsigned final_vsync = ram[counter] | ((unsigned)ram[counter + 1] << 8);
    CHECK((uint16_t)(final_vsync - initial_vsync) == 2 &&
          (uint16_t)(final_vsync - first_vsync) == 1,
          "Text and palette work did not complete in exactly two consecutive VSYNCs");
    CHECK(total_cycles < FRAME_CYCLES * 2,
          "Combined text-copy/palette workload exceeded the two-frame cycle bound");
    CHECK(max_masked_cycles < FRAME_CYCLES,
          "Combined workload exceeded the frame's maximum IRQ-masked cycle bound");
    CHECK(!fx_write && ram[0x9F25] == 0,
          "Second production menu pass left VERA FX cache writes or a nondefault port enabled");
    fprintf(stderr,
            "Combined max-menu IRQ: text-copy %u cycles, palette %u cycles, max conservative acceptance-to-RTI I-masked bound %u, total %u cycles, completion in 2 VSYNCs (< %u/frame)\n",
            first_cycles, palette_cycles, max_masked_cycles, total_cycles, FRAME_CYCLES);
}

int main(void) {
    suite_test_paths_init("irq-helpers");
    assemble();
    test_input();
    test_input_dirty();
    test_input_dirty_sources();
    test_reenable_preserves_pending_isr();
    test_snapshot_and_wait();
    test_atomic_vsync_snapshot();
    test_snapshot_interrupt_boundaries();
    test_irq_counter_carries();
    test_sprite_clear();
    test_sprite_updates();
    test_sprite_snapshot_reset();
    test_agi_blit_owner_cleanup_contract();
    test_get_priority_condition_codes();
    test_nested_foreground_owner();
    test_sound_source_contract();
    test_sound_handler();
    test_sound_interrupt_restore();
    test_combined_irq_workload();
    printf("IRQ helpers pass: input lengths/dirty fast path, sound lifecycle/idle stride, VSYNC helpers/production IRQ carries, mixed text/palette workload, and sprite bounds (sprite consumer max %u cycles)\n",
           max_sprite_consumer_cycles);
    return 0;
}
