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

extern struct regs regs;
static uint8_t ram[65536], vram[131072];
static unsigned addr[2], stride[2], writes, data_port_reads, acks;
static unsigned write_addresses[2048];
static uint8_t write_values[2048];
static char temporary[4096];
struct symbol { char name[100]; unsigned addr; } symbols[128];
static unsigned symbol_count;
static bool track_sprite_reads;
static unsigned sprite_read_start, sprite_read_end, sprite_read_limit, sprite_reads;

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (track_sprite_reads && address >= sprite_read_start - 1 && address < sprite_read_end + 2) {
        CHECK(address >= sprite_read_start && address < sprite_read_limit,
              "Sprite consumer read beyond its accepted records/terminator");
        ++sprite_reads;
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
        CHECK(writes < sizeof(write_addresses) / sizeof(write_addresses[0]), "Too many VERA writes");
        write_addresses[writes] = addr[port];
        write_values[writes] = value;
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
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(void) {
    char source[65536], stub[256], path[4096], stub_path[4096], config_path[4096], object_path[4096], stub_object_path[4096], binary_path[4096], labels_path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/test.s", temporary) < (int)sizeof(path), "Fixture path too long");
    CHECK(snprintf(stub_path, sizeof(stub_path), "%s/sound_stub.s", temporary) < (int)sizeof(stub_path), "Fixture path too long");
    CHECK(snprintf(config_path, sizeof(config_path), "%s/test.cfg", temporary) < (int)sizeof(config_path), "Fixture path too long");
    CHECK(snprintf(object_path, sizeof(object_path), "%s/test.o", temporary) < (int)sizeof(object_path), "Fixture path too long");
    CHECK(snprintf(stub_object_path, sizeof(stub_object_path), "%s/sound_stub.o", temporary) < (int)sizeof(stub_object_path), "Fixture path too long");
    CHECK(snprintf(binary_path, sizeof(binary_path), "%s/test.bin", temporary) < (int)sizeof(binary_path), "Fixture path too long");
    CHECK(snprintf(labels_path, sizeof(labels_path), "%s/test.lbl", temporary) < (int)sizeof(labels_path), "Fixture path too long");

    char *irq = read_file("src/irqAsm.s");
    char *input = read_file("src/inputIrqHandler.s");
    char *sprite = read_file("src/spriteIrqHandler.s");
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
        ".include \"x16.inc\"\nVIEW_TABLE_SIZE=1\n.include \"globalGraphics.s\"\n.include \"spriteUpdates.inc\"\n.include \"global.s\"\nMAX_INPUT_STRING_LENGTH=40\n"
        "INPUT_LINE_RENDER_CELLS=MAX_INPUT_STRING_LENGTH+1\n"
        ".assert INPUT_LINE_COL + INPUT_LINE_RENDER_CELLS <= TILE_LAYER_WIDTH, error, \"Input line rendering exceeds tile map row\"\n"
        ".segment \"CODE\"\n"
        ".export input,enable,send,wait,done,snapshot,snapshot32,snapshotOutput,_vSyncCounter,_vSyncCounterHigh,sendIrqCommand,_inputLineDisplayed,_inputLineDirty,_b7CurrentInputStr,_foregroundVeraDepth,_b6EnterForegroundVera,_b6LeaveForegroundVera\n"
        ".export clearSprites,inputStringAddress,spriteFirstZ,spriteCount\n"
        ".export handleSpriteUpdates,checkSpriteUpdateCapacity,_bESpritesUpdatedBuffer,_bESpritesUpdatedBufferPointer,_bESpriteUpdateOverflowed,spriteBufferBefore,spriteBufferEnd,spriteBufferGuard,spriteUpdateBufferBytes,spriteUpdateRecordBytes,spriteUpdateMaxRecords,spriteUpdateBank,irqScratchPointer\n"
        ".export bBSoundHandler,_bBChannelsPlaying,_bBEndSoundFlag,_bBIsPlaying,_bBCh1Ticks,_ZP_CURRENTLY_PLAYING_NOTE_1,SOUND_SREG,_bBPsgClear,soundCriticalSection\n"
        "ZP_SPRITE_UPDATE_READ_POINTER=IRQ_TMP_6\nZP_LOW_BYTE=ZP_TMP_5+1\n"
        "%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n"
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
        ".export custom_irq_handler,irqEntry,irqDefault,default_irq_vector,irqWorkKind,lastMainLoopCounter,_mainLoopCounter,irqNormalWork\n"
        "currentIrqState: .byte 0\nlastMainLoopCounter: .byte 0\n_mainLoopCounter: .byte 0\n_menuDirty: .byte 0\n_b3PaletteRows: .byte 0\nirqNormalWork: .byte 0\n"
        "irqEntry: pha\nphx\nphy\njmp custom_irq_handler\n"
        "irqDefault: ply\nplx\npla\nrti\n"
        "b10DisplayStatusBar: inc irqNormalWork\nlda #$91\nldx #$82\nldy #$73\nrts\n"
        "handleScreenShake: rts\nbADisplayMenu: rts\nb3InitDisplayTextSlice: rts\n_b3InitLayer1Mapbase: rts\n_b6InitInput: rts\nb11ShowObjIrqHandler: rts\n"
        "_bEClearSpriteAttributes: rts\n_b8ClearBackgroundSliceReset: rts\n_b8ClearPrioritySliceReset: rts\n_b8ClearBackgroundSlice: rts\n_b8ClearPrioritySlice: rts\nb3SetPaletteSlice: rts\nhandleDisplayTextSlice: rts\n"
        "inputStringAddress=INPUT_STRING_ADDRESS\nspriteFirstZ=SPRITE_ATTR_START+SPRITE_ATTR_SIZE+SA_VERA_ZORDER\nspriteCount=NO_SPRITES\n",
        (int)(irq_end - irq_macros), irq_macros,
        (int)(snapshot_helper_end - snapshot_helper), snapshot_helper,
        (int)(owner_helpers_end - owner_helpers), owner_helpers,
        (int)(sprite_start_end - sprite_start), sprite_start,
        (int)(sprite_clear_end - sprite_clear), sprite_clear,
        (int)(sprite_get_end - sprite_get), sprite_get,
        (int)(sprite_append_end - sprite_append), sprite_append,
        (int)(sprite_handler_end - sprite_handler), sprite_handler,
        (int)(input_end - input_body), input_body,
        (int)(irq_constants_end - irq_constants), irq_constants,
        (int)(irq_handler_end - irq_handler), irq_handler);
    CHECK(length >= 0 && length < (int)sizeof(source), "Fixture assembly is too large");
    CHECK(snprintf(stub, sizeof(stub), ".segment \"CODE\"\n.export _bBLoadedSoundsPointer,_sResetCounter\n_bBLoadedSoundsPointer: .res 512\n_sResetCounter: .byte 0\n") < (int)sizeof(stub), "Fixture stub is too large");
    free(irq); free(input); free(sprite);

    write_file(path, source);
    write_file(stub_path, stub);
    write_file(config_path,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; BANKRAM07:load=RAM,type=rw,optional=yes; BANKRAM0B:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", path, "-o", object_path, NULL};
    char *ca65_stub[] = {"ca65", "-t", "cx16", stub_path, "-o", stub_object_path, NULL};
    char *ld65[] = {"ld65", "-C", config_path, object_path, stub_object_path, "-o", binary_path, "-Ln", labels_path, NULL};
    run_tool(ca65); run_tool(ca65_stub); run_tool(ld65);

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
    CHECK(occurrences(commands, "inputLineDirty = TRUE;") >= 4,
          "Input visibility/screen transitions do not all invalidate the row");
    CHECK(strstr(picture, "volatile byte inputLineDirty = TRUE;"),
          "Input dirty state is not initialized in resident storage");
    CHECK(map_clear_dirty < display && display_dirty < display_finish,
          "Map clear/text display does not invalidate the row");
    CHECK(input_init_dirty < find(input_init, ".segment \"BANKRAM08\""),
          "Input row initialization does not invalidate the row");

    free(parser); free(commands); free(picture); free(irq); free(graphics);
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
    char *play_disabled = find(play, "if(!flag[9])");
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
    const unsigned ticks = symbol("_bBCh1Ticks");
    const unsigned psg = 0x1F9C0;

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

    memset(ram + playing, 0, 4);
    ram[channel_count] = 1;
    ram[playing] = 1;
    ram[ticks] = 1;
    ram[ticks + 1] = 0;
    memset(vram + psg, 0xA5, 64);
    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(writes == 12 && ram[ticks] == 0 && ram[channel_count] == 1 && ram[playing] == 1,
          "Partially idle sound lost its active channel or silence writes");
    for (unsigned i = 0; i < 12; ++i)
        CHECK(write_addresses[i] == 0x1F9C6 + i && write_values[i] == 0,
              "Partially idle channels did not retain four-register VERA stride");

    memset(ram + playing, 0, 4);
    ram[playing] = 1;
    ram[channel_count] = 1;
    ram[symbol("_bBEndSoundFlag")] = 3;
    ram[0x504] = 0;
    ram[ticks] = ram[ticks + 1] = 0;
    const unsigned note = 0x9000;
    write_word(symbol("_ZP_CURRENTLY_PLAYING_NOTE_1"), note - 5);
    ram[note] = ram[note + 1] = 0xFF;
    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!ram[channel_count] && !ram[playing] && ram[0x504] == 1,
          "Final channel did not clear ownership and set its end flag");

    init_cpu(symbol("bBSoundHandler"), 0);
    run();
    CHECK(!writes, "Naturally ended sound did not reach the idle fast path");

    memset(ram + playing, 1, 4);
    ram[channel_count] = 4;
    for (unsigned i = 0; i < 4; ++i) {
        ram[ticks + i * 2] = 1;
        ram[ticks + i * 2 + 1] = 0;
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
    for (unsigned owner = 0; owner <= 2; ++owner)
        for (unsigned i = 0; i < sizeof(starts)/sizeof(starts[0]); ++i)
            for (unsigned pending = 2; pending <= 3; ++pending) {
                uint32_t before = starts[i], expected = before + (pending & 1);
                ram[counter] = (uint8_t)before; ram[counter + 1] = (uint8_t)(before >> 8);
                ram[high] = (uint8_t)(before >> 16); ram[high + 1] = (uint8_t)(before >> 24);
                ram[symbol("_foregroundVeraDepth")] = (uint8_t)owner;
                ram[symbol("irqWorkKind")] = ram[symbol("sendIrqCommand")] = 0;
                ram[symbol("lastMainLoopCounter")] = ram[symbol("_mainLoopCounter")] = 0;
                ram[symbol("_inputLineDirty")] = ram[symbol("_bBChannelsPlaying")] = 0;
                ram[symbol("irqNormalWork")] = 0;
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
                CHECK(ram[symbol("irqNormalWork")] == (!owner && (pending & 1)) && !writes,
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

int main(void) {
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(temporary, sizeof(temporary), "%s/irq-helpers-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create temporary directory");
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
    test_agi_blit_owner_cleanup_contract();
    test_get_priority_condition_codes();
    test_nested_foreground_owner();
    test_sound_source_contract();
    test_sound_handler();
    test_sound_interrupt_restore();
    printf("IRQ helpers pass: input lengths/dirty fast path, sound lifecycle/idle stride, VSYNC helpers/production IRQ carries, and sprite bounds (sprite consumer max %u cycles)\n",
           max_sprite_consumer_cycles);
    return 0;
}
