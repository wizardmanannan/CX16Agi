/* Native checks for the production bulk sprite allocator and its real table. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "registers.h"
#include "fake6502.h"

extern struct regs regs;
static uint8_t ram[65536];
static char temporary[4096];
static struct { char name[100]; unsigned address; } symbols[32];
static unsigned symbol_count;
static uint8_t image[0x6000];
static size_t image_size;
static unsigned allocation_table_start, bulk_results_start;
static bool cleanup_failed;

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

static void check_banked_data_access(uint16_t address) {
    if (allocation_table_start && address >= allocation_table_start && address <= allocation_table_start + 2160)
        CHECK(ram[0] == 0x0D, "Allocator table accessed outside RAM bank $0D");
    if (bulk_results_start && address >= bulk_results_start && address < bulk_results_start + 256)
        CHECK(ram[0] == 0x0E, "Bulk result buffer accessed outside RAM bank $0E");
}

uint8_t read6502(uint16_t address, uint8_t bank) { (void)bank; check_banked_data_access(address); return ram[address]; }
void write6502(uint16_t address, uint8_t bank, uint8_t value) { (void)bank; check_banked_data_access(address); ram[address] = value; }
uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank) { (void)address; (void)bank; CHECK(false, "Allocator fixture stopped the CPU"); }

static unsigned symbol(const char *name) {
    for (unsigned i = 0; i < symbol_count; ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    fprintf(stderr, "Missing assembler symbol: %s\n", name);
    exit(1);
}

static void cleanup(void) {
    DIR *directory = opendir(temporary);
    if (!directory) {
        if (errno != ENOENT) {
            fprintf(stderr, "Cannot inspect allocator fixture directory %s\n", temporary);
            _Exit(EXIT_FAILURE);
        }
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char path[4096];
        if (snprintf(path, sizeof(path), "%s/%s", temporary, entry->d_name) >= (int)sizeof(path) ||
            (unlink(path) && errno != ENOENT)) cleanup_failed = true;
    }
    if (closedir(directory)) cleanup_failed = true;
    if (rmdir(temporary) && errno != ENOENT) cleanup_failed = true;
    if (cleanup_failed) {
        fprintf(stderr, "Could not fully clean allocator fixture directory %s\n", temporary);
        _Exit(EXIT_FAILURE);
    }
}

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb"); CHECK(file, "Cannot create allocator fixture");
    CHECK(fwrite(contents, 1, strlen(contents), file) == strlen(contents), "Cannot write allocator fixture");
    CHECK(!fclose(file), "Cannot close allocator fixture");
}

static void output_path(char *path, size_t capacity, const char *name) {
    const char *directory = getenv("TEST_OUTPUT_DIR");
    CHECK(directory && *directory, "TEST_OUTPUT_DIR is unavailable");
    CHECK(snprintf(path, capacity, "%s/%s", directory, name) < (int)capacity, "Output path too long");
}

static void run_tool(const char *name, char *const arguments[]) {
    char executable[4096];
    const char *home = getenv("CC65_HOME");
    CHECK(home && snprintf(executable, sizeof(executable), "%s/bin/%s", home, name) < (int)sizeof(executable), "CC65_HOME is unavailable or too long");
    pid_t child = fork(); CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        char *argv[16]; unsigned count = 0;
        while (arguments[count]) { CHECK(count < 15, "Too many assembler arguments"); argv[count] = arguments[count]; ++count; }
        argv[0] = executable; argv[count] = NULL;
        execv(executable, argv); perror(executable); _exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(void) {
    char source[4096], stub[4096], config[4096], object[4096], stub_object[4096], binary[4096], labels[4096];
    CHECK(snprintf(source, sizeof(source), "%s/allocator.s", temporary) < (int)sizeof(source), "Fixture path too long");
    CHECK(snprintf(stub, sizeof(stub), "%s/runtime.s", temporary) < (int)sizeof(stub), "Fixture path too long");
    CHECK(snprintf(config, sizeof(config), "%s/allocator.cfg", temporary) < (int)sizeof(config), "Fixture path too long");
    output_path(object, sizeof(object), "sprite_allocator.o");
    output_path(stub_object, sizeof(stub_object), "sprite_allocator_runtime.o");
    output_path(binary, sizeof(binary), "sprite_allocator.bin");
    output_path(labels, sizeof(labels), "sprite_allocator.lbl");

    write_file(source,
        ".include \"x16.inc\"\n"
        "ZP_TMP_24=$CB\nZP_TMP_25=$CD\n"
        "SPRITE_START=$EA00\nNOP_IMP=$EA\nBRA_ABS=$80\n"
        ".importzp sreg,sreg2\n.import popax,popa\n"
        ".macro TRAMPOLINE BANK, TARGET\n"
        "  sta trampolineInputA\n  stx trampolineInputX\n"
        "  lda RAM_BANK\n  pha\n  lda BANK\n  sta RAM_BANK\n"
        "  lda trampolineInputA\n  ldx trampolineInputX\n  jsr TARGET\n"
        "  sta trampolineResult\n  stx trampolineResultX\n"
        "  pla\n  sta RAM_BANK\n  lda trampolineResult\n  ldx trampolineResultX\n"
        ".endmacro\n"
        ".export _bEAllocateSpriteMemoryBulk,_bEBulkAllocatedAddresses,_bDSpriteAllocTable,_bDBlocksBySizeFastLookup\n"
        ".export findFreeVRamLowByteLoop,findFirstFreeVRamBlock_optimisticSkip\n"
        ".export BLOCKS_TO_FIND,bDDeleteAllocationAsmCall,_bDResetSpriteTablePointer\n"
        ".segment \"CODE\"\ntrampolineInputA: .byte 0\ntrampolineInputX: .byte 0\ntrampolineResult: .byte 0\ntrampolineResultX: .byte 0\n"
        ".segment \"BANKRAM0E\"\n"
        ".include \"spriteAllocatorAsm.s\"\n"
        ".segment \"CODE\"\n.export done\ndone: brk\n");
    write_file(stub,
        ".segment \"ZEROPAGE\"\n.exportzp sp,c_sp,sreg,sreg2\n"
        "sp: .res 2\nc_sp: .res 2\nsreg: .res 2\nsreg2: .res 2\n"
        ".segment \"CODE\"\n.export popax,popa\n"
        "popax:\n  ldy #0\n  lda (c_sp),y\n  pha\n  iny\n  lda (c_sp),y\n  tax\n  pla\n"
        "  inc c_sp\n  bne :+\n  inc c_sp+1\n:\n  inc c_sp\n  bne :+\n  inc c_sp+1\n:\n  rts\n"
        "popa:\n  ldy #0\n  lda (c_sp),y\n  inc c_sp\n  bne :+\n  inc c_sp+1\n:\n  rts\n");
    write_file(config,
        "MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp; CODE:load=RAM,type=rw; BANKRAM0E:load=RAM,type=rw; BANKRAM0D:load=RAM,type=rw; }\n");

    char *ca65[] = {"ca65", "-t", "cx16", "-I", "src", source, "-o", object, NULL};
    char *ca65_stub[] = {"ca65", "-t", "cx16", stub, "-o", stub_object, NULL};
    char *ld65[] = {"ld65", "-C", config, object, stub_object, "-o", binary, "-Ln", labels, NULL};
    run_tool("ca65", ca65); run_tool("ca65", ca65_stub); run_tool("ld65", ld65);

    FILE *file = fopen(binary, "rb"); CHECK(file, "Cannot read linked allocator fixture");
    image_size = fread(image, 1, sizeof(image), file);
    CHECK(image_size > 0 && !ferror(file), "Linked allocator fixture is empty or unreadable");
    CHECK(!fclose(file), "Cannot close linked allocator fixture");
    file = fopen(labels, "r"); CHECK(file, "Cannot read allocator labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
        CHECK(symbol_count < sizeof(symbols) / sizeof(symbols[0]), "Too many allocator symbols");
        strcpy(symbols[symbol_count].name, name[0] == '.' ? name + 1 : name);
        symbols[symbol_count++].address = address;
    }
    CHECK(!fclose(file), "Cannot close allocator labels");
    allocation_table_start = symbol("_bDSpriteAllocTable");
    bulk_results_start = symbol("_bEBulkAllocatedAddresses");
}

static void prepare(void) {
    memset(ram, 0, sizeof(ram));
    memcpy(ram + 0x2000, image, image_size);
    static const uint8_t sizes[] = {8, 16, 32, 64};
    unsigned lookup = symbol("_bDBlocksBySizeFastLookup");
    memset(ram + lookup, 0, 130);
    for (unsigned width = 0; width < 4; ++width) {
        for (unsigned height = 0; height < 4; ++height) {
            unsigned sum = sizes[width] + sizes[height];
            uint8_t blocks = (uint8_t)((sizes[width] / 8) * (sizes[height] / 8));
            ram[lookup + sum] = blocks;
            ram[lookup + sum + 1] = blocks;
        }
    }
    memset(ram + symbol("_bDSpriteAllocTable"), 0, 2160);
    memset(ram + symbol("_bEBulkAllocatedAddresses"), 0, 256);
    ram[0] = 0x0E; ram[1] = 0x74;
}

static uint8_t allocate_dimensions(uint8_t count, uint8_t width, uint8_t height) {
    unsigned stack = 0x0400, done = symbol("done");
    ram[symbol("c_sp")] = (uint8_t)stack;
    ram[symbol("c_sp") + 1] = (uint8_t)(stack >> 8);
    ram[stack] = height; /* The one-byte enum arguments are on cc65's software stack. */
    ram[stack + 1] = width;
    reset6502(false);
    regs.pc = (uint16_t)symbol("_bEAllocateSpriteMemoryBulk");
    regs.sp = 0x1FD;
    regs.a = count;
    regs.status = FLAG_CONSTANT | FLAG_INTERRUPT;
    ram[0x1FE] = (uint8_t)(done - 1);
    ram[0x1FF] = (uint8_t)((done - 1) >> 8);
    for (unsigned step = 0; step < 3000000 && regs.pc != done; ++step) step6502();
    CHECK(regs.pc == done, "Bulk allocator did not return");
    CHECK(regs.sp == 0x1FF, "Bulk allocator changed the hardware stack");
    CHECK(ram[0] == 0x0E && ram[1] == 0x74, "Bulk allocator did not preserve caller banks");
    CHECK((regs.status & FLAG_INTERRUPT) != 0, "Bulk allocator changed the caller interrupt flag");
    CHECK(regs.x == 0, "Bulk allocator did not sign-extend its boolean return into X");
    CHECK(((regs.status & FLAG_ZERO) != 0) == (regs.a == 0), "Bulk allocator did not leave Z consistent with its boolean result");
    CHECK(ram[symbol("c_sp")] == 0x02 && ram[symbol("c_sp") + 1] == 0x04, "Bulk allocator did not consume exactly two enum arguments");
    return regs.a;
}

static uint8_t allocate(uint8_t count) { return allocate_dimensions(count, 8, 16); }

static void call_asm(uint16_t entry, uint8_t a, uint8_t x, uint8_t high) {
    unsigned done = symbol("done");
    reset6502(false);
    regs.pc = entry;
    regs.sp = 0x1FD;
    regs.a = a;
    regs.x = x;
    regs.status = FLAG_CONSTANT | FLAG_INTERRUPT;
    ram[symbol("sreg")] = high;
    ram[0x1FE] = (uint8_t)(done - 1);
    ram[0x1FF] = (uint8_t)((done - 1) >> 8);
    for (unsigned step = 0; step < 3000000 && regs.pc != done; ++step) step6502();
    CHECK(regs.pc == done, "Direct allocator helper did not return");
    CHECK(regs.sp == 0x1FF, "Direct allocator helper changed the hardware stack");
}

static uint32_t bulk_result(unsigned index) {
    unsigned results = symbol("_bEBulkAllocatedAddresses") + index * 4;
    return (uint32_t)ram[results] | ((uint32_t)ram[results + 1] << 8) |
           ((uint32_t)ram[results + 2] << 16);
}

static void delete_blocks(uint32_t address, uint8_t blocks) {
    ram[symbol("BLOCKS_TO_FIND")] = blocks;
    ram[0] = 0x0D;
    call_asm((uint16_t)symbol("bDDeleteAllocationAsmCall"), (uint8_t)address,
             (uint8_t)(address >> 8), (uint8_t)(address >> 16));
    ram[0] = 0x0E;
}

static void reset_table_pointer(void) {
    ram[0] = 0x0D;
    call_asm((uint16_t)symbol("_bDResetSpriteTablePointer"), 0, 0, 0);
    ram[0] = 0x0E;
}

static void fill_to_scan_mode(void) {
    prepare();
    CHECK(allocate_dimensions(33, 64, 64) == 1,
          "Real 64x64 allocations must cross the allocator's optimistic-scan threshold");
    unsigned branch = symbol("findFirstFreeVRamBlock_optimisticSkip");
    CHECK(ram[branch] == 0xEA && ram[branch + 1] == 0xEA,
          "Production allocation did not disable optimistic skipping at its threshold");
}

static void test_zero_count(void) {
    prepare();
    memset(ram + symbol("_bEBulkAllocatedAddresses"), 0xA5, 256);
    uint8_t before[2160]; memcpy(before, ram + symbol("_bDSpriteAllocTable"), sizeof(before));
    CHECK(allocate(0) == 0, "A zero-count bulk allocation must be rejected");
    CHECK(!memcmp(before, ram + symbol("_bDSpriteAllocTable"), sizeof(before)), "Zero-count allocation changed the allocator table");
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[symbol("_bEBulkAllocatedAddresses") + i] == 0xA5, "Zero-count allocation wrote a result address");
}

static void test_single_count(void) {
    prepare();
    memset(ram + symbol("_bEBulkAllocatedAddresses"), 0xA5, 256);
    CHECK(allocate(1) == 1, "A one-entry bulk allocation must succeed");
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    CHECK(ram[table] == 1 && ram[table + 1] == 1 && ram[table + 2] == 0,
          "Single bulk allocation did not reserve exactly two blocks");
    CHECK(ram[results] == 0 && ram[results + 1] == 0xEA && ram[results + 2] == 0 && ram[results + 3] == 0,
          "Single bulk allocation returned an incorrect address record");
    for (unsigned i = 4; i < 256; ++i)
        CHECK(ram[results + i] == 0xA5, "Single bulk allocation wrote beyond its one-record result range");
}

static void test_all_geometry_combinations(void) {
    static const uint8_t sizes[] = {8, 16, 32, 64};
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    for (unsigned width = 0; width < 4; ++width) {
        for (unsigned height = 0; height < 4; ++height) {
            unsigned blocks = (sizes[width] / 8) * (sizes[height] / 8);
            prepare();
            memset(ram + results, 0xA5, 256);
            CHECK(allocate_dimensions(1, sizes[width], sizes[height]) == 1,
                  "A legal width/height geometry must allocate one exact record");
            unsigned address = 0xEA00;
            CHECK(ram[results] == (uint8_t)address && ram[results + 1] == (uint8_t)(address >> 8) &&
                  ram[results + 2] == 0 && ram[results + 3] == 0,
                  "A legal geometry returned the wrong first VERA address");
            for (unsigned block = 0; block < 2160; ++block)
                CHECK(ram[table + block] == (block < blocks ? 1 : 0),
                      "A legal geometry reserved the wrong number of blocks");
            for (unsigned byte = 4; byte < 256; ++byte)
                CHECK(ram[results + byte] == 0xA5, "A one-record geometry wrote beyond its result record");
        }
    }
}

static void test_mixed_geometry_fragmentation_and_rollback(void) {
    fill_to_scan_mode();
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    uint32_t large_extents[3];
    for (unsigned i = 0; i < 3; ++i) large_extents[i] = bulk_result(i);
    CHECK(allocate_dimensions(48, 8, 8) == 1,
          "Could not naturally fill the final 48 blocks before fragmenting the allocator");
    for (unsigned i = 0; i < 3; ++i) delete_blocks(large_extents[i], 64);
    reset_table_pointer();

    CHECK(allocate_dimensions(32, 16, 8) == 1, "Allocator missed the freed 64-block extent for 16x8 sprites");
    CHECK(bulk_result(0) == 0xEA00u, "The 16x8 allocations did not occupy the first freed extent");
    CHECK(allocate_dimensions(16, 16, 16) == 1, "Allocator missed the freed 64-block extent for 16x16 sprites");
    CHECK(bulk_result(0) == 0xF200u, "The 16x16 allocations did not occupy the second freed extent");
    uint32_t medium_extents[16];
    for (unsigned i = 0; i < 16; ++i) medium_extents[i] = bulk_result(i);
    CHECK(allocate_dimensions(4, 32, 32) == 1, "Allocator missed the freed 64-block extent for 32x32 sprites");
    CHECK(bulk_result(0) == 0xFA00u, "The 32x32 allocations did not occupy the third freed extent");

    uint32_t large_again[4];
    for (unsigned i = 0; i < 4; ++i) large_again[i] = bulk_result(i);
    for (unsigned i = 1; i < 16; i += 2) delete_blocks(medium_extents[i], 4);
    for (unsigned i = 0; i < 3; ++i) delete_blocks(large_again[i], 16);

    uint8_t before[2160];
    memcpy(before, ram + table, sizeof(before));
    memset(ram + results, 0xA5, 256);
    uint8_t allocation_result = allocate_dimensions(4, 32, 32);
    if (allocation_result) {
        fprintf(stderr, "Unexpected fragmented allocation addresses: %06X %06X %06X %06X; free table runs:",
                (unsigned)bulk_result(0), (unsigned)bulk_result(1),
                (unsigned)bulk_result(2), (unsigned)bulk_result(3));
        for (unsigned i = 0; i < 2160;) {
            if (ram[table + i]) { ++i; continue; }
            unsigned start = i;
            while (i < 2160 && !ram[table + i]) ++i;
            fprintf(stderr, " %u+%u", start, i - start);
        }
        fputc('\n', stderr);
    }
    CHECK(allocation_result == 0,
          "A mixed fragmented request must fail after reserving its three available extents");
    CHECK(!memcmp(before, ram + table, sizeof(before)),
          "Mixed-geometry rollback changed earlier allocations or fragmented free extents");
    for (unsigned byte = 0; byte < 16; ++byte)
        CHECK(ram[results + byte] == 0, "Mixed-geometry rollback left a result address published");
    for (unsigned byte = 16; byte < 256; ++byte)
        CHECK(ram[results + byte] == 0xA5, "Mixed-geometry rollback wrote beyond its four result records");
    CHECK(ram[table + 2160] == 0xFF, "Mixed-geometry rollback changed the allocator table terminator");

    CHECK(allocate_dimensions(1, 32, 32) == 1, "Allocator did not retry after mixed-geometry rollback");
    CHECK(bulk_result(0) == (uint32_t)(0xEA00 + 124 * 32),
          "Retry did not reuse the first restored fragmented extent");
    CHECK(ram[table + 123] == 1 && ram[table + 124] == 1 && ram[table + 139] == 1 &&
          ram[table + 140] == 0 && ram[table + 175] == 0 &&
          ram[table + 176] == 1 && ram[table + 191] == 1,
          "Retry failed to reserve one freed extent or changed adjacent reservations");
}

static void test_failure_at_each_reachable_position(void) {
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    fill_to_scan_mode();
    uint32_t large_extents[2];
    large_extents[0] = bulk_result(0);
    large_extents[1] = bulk_result(1);
    CHECK(allocate_dimensions(48, 8, 8) == 1,
          "Could not naturally fill the final 48 blocks before creating scattered holes");
    delete_blocks(large_extents[0], 64);
    delete_blocks(large_extents[1], 64);
    reset_table_pointer();

    uint32_t first_region[64], second_region[64];
    CHECK(allocate_dimensions(64, 8, 8) == 1, "Could not fill the first freed extent with real 8x8 allocations");
    for (unsigned i = 0; i < 64; ++i) first_region[i] = bulk_result(i);
    CHECK(allocate_dimensions(64, 8, 8) == 1, "Could not fill the second freed extent with real 8x8 allocations");
    for (unsigned i = 0; i < 64; ++i) second_region[i] = bulk_result(i);
    for (unsigned i = 1; i < 64; i += 2) delete_blocks(first_region[i], 1);
    for (unsigned i = 1; i < 63; i += 2) delete_blocks(second_region[i], 1);

    uint8_t hole_map[2160];
    memcpy(hole_map, ram + table, sizeof(hole_map));
    unsigned holes = 0;
    for (unsigned i = 0; i < 2160; ++i) holes += hole_map[i] == 0;
    CHECK(holes == 63, "The scattered fixture must contain exactly 63 reachable one-block holes");

    for (unsigned successes_before_failure = 0; successes_before_failure < 64; ++successes_before_failure) {
        memcpy(ram + table, hole_map, sizeof(hole_map));
        reset_table_pointer();
        unsigned to_fill = 63 - successes_before_failure;
        if (to_fill) {
            CHECK(allocate_dimensions((uint8_t)to_fill, 8, 8) == 1,
                  "Could not generate the requested failure boundary using real allocations");
        }
        uint8_t before[2160];
        memcpy(before, ram + table, sizeof(before));
        unsigned free_before = 0;
        for (unsigned i = 0; i < 2160; ++i) free_before += before[i] == 0;
        CHECK(free_before == successes_before_failure,
              "Fixture did not expose the requested number of successful records before failure");
        reset_table_pointer();
        memset(ram + results, 0xA5, 256);
        CHECK(allocate_dimensions((uint8_t)(successes_before_failure + 1), 8, 8) == 0,
              "A one-block batch must fail at the first unavailable record position");
        CHECK(!memcmp(before, ram + table, sizeof(before)),
              "Rollback after a specific failure position changed the allocator table");
        for (unsigned byte = 0; byte < (successes_before_failure + 1) * 4; ++byte)
            CHECK(ram[results + byte] == 0, "Rollback left a successful or failed result record published");
        for (unsigned byte = (successes_before_failure + 1) * 4; byte < 256; ++byte)
            CHECK(ram[results + byte] == 0xA5, "Rollback crossed the requested result-record boundary");
        CHECK(ram[table + 2160] == 0xFF, "Rollback changed the allocator table terminator");
    }
}

static void test_maximum_count(void) {
    prepare();
    CHECK(allocate(64) == 1, "The 64-entry bulk allocation limit must be accepted");
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    for (unsigned block = 0; block < 128; ++block)
        CHECK(ram[table + block] == 1, "Maximum bulk allocation did not reserve every block");
    for (unsigned block = 128; block < 2160; ++block)
        CHECK(ram[table + block] == 0, "Maximum bulk allocation changed an unrelated block");
    for (unsigned i = 0; i < 64; ++i) {
        unsigned address = 0xEA00 + i * 64;
        CHECK(ram[results + i * 4] == (uint8_t)address && ram[results + i * 4 + 1] == (uint8_t)(address >> 8) &&
              ram[results + i * 4 + 2] == 0 && ram[results + i * 4 + 3] == 0,
              "Bulk allocation returned an incorrect address record");
    }
}

static void test_rejected_count(uint8_t count) {
    prepare();
    memset(ram + symbol("_bEBulkAllocatedAddresses"), 0xA5, 256);
    uint8_t before[2160]; memcpy(before, ram + symbol("_bDSpriteAllocTable"), sizeof(before));
    CHECK(allocate(count) == 0, "A count above 64 must be rejected");
    CHECK(!memcmp(before, ram + symbol("_bDSpriteAllocTable"), sizeof(before)), "Rejected count changed the allocator table");
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[symbol("_bEBulkAllocatedAddresses") + i] == 0xA5, "Rejected count wrote a result address");
}

static void test_partial_failure_rolls_back(unsigned first_block) {
    fill_to_scan_mode();
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    memset(ram + table, 1, 2160);
    ram[table + first_block] = ram[table + first_block + 1] = 0;
    reset_table_pointer();
    memset(ram + results, 0xA5, 256);
    uint8_t before[2160]; memcpy(before, ram + table, sizeof(before));
    CHECK(allocate(2) == 0, "Bulk allocation must report the second allocation failure");
    CHECK(!memcmp(before, ram + table, sizeof(before)), "Partial bulk failure leaked allocated blocks");
    for (unsigned i = 0; i < 8; ++i)
        CHECK(ram[results + i] == 0, "Partial bulk failure left a valid-looking result address");
    for (unsigned i = 8; i < 256; ++i)
        CHECK(ram[results + i] == 0xA5, "Partial bulk failure wrote beyond its two result records");
    CHECK(allocate(1) == 1, "Allocator did not make forward progress after rollback");
    unsigned address = 0xEA00 + first_block * 32;
    CHECK(ram[results] == (uint8_t)address && ram[results + 1] == (uint8_t)(address >> 8) &&
          ram[results + 2] == (uint8_t)(address >> 16) && ram[results + 3] == 0,
          "Allocator retry did not reuse the freed pair");
    for (unsigned i = 0; i < 2160; ++i)
        CHECK(ram[table + i] == ((i == first_block || i == first_block + 1) ? 1 : before[i]),
              "Allocator retry changed unrelated blocks after rollback");
}

static void test_large_partial_failure_rolls_back(void) {
    prepare();
    unsigned table = symbol("_bDSpriteAllocTable"), results = symbol("_bEBulkAllocatedAddresses");
    uint8_t before[2160]; memcpy(before, ram + table, sizeof(before));
    CHECK(allocate_dimensions(64, 64, 64) == 0, "64 large allocations must fail when the real table runs out of space");
    CHECK(!memcmp(before, ram + table, sizeof(before)), "Large partial failure leaked allocated blocks");
    for (unsigned i = 0; i < 256; ++i)
        CHECK(ram[results + i] == 0, "Large partial failure left an unpublished address");
    CHECK(allocate_dimensions(1, 64, 64) == 1, "Allocator did not progress after large rollback");
    CHECK(ram[results] == 0 && ram[results + 1] == 0xEA && ram[results + 2] == 0 && ram[results + 3] == 0,
          "Large allocation retry did not reuse the first freed blocks");
    for (unsigned i = 0; i < 2160; ++i)
        CHECK(ram[table + i] == (i < 64 ? 1 : 0), "Large retry changed unrelated allocator blocks");
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(temporary, sizeof(temporary), "%s/sprite-allocator-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(temporary), "Temporary path too long");
    CHECK(mkdtemp(temporary), "Cannot create allocator fixture directory");
    if (atexit(cleanup) != 0) {
        cleanup();
        fprintf(stderr, "Cannot register allocator fixture cleanup\n");
        return EXIT_FAILURE;
    }
    assemble();
    test_single_count();
    test_all_geometry_combinations();
    test_mixed_geometry_fragmentation_and_rollback();
    test_failure_at_each_reachable_position();
    test_maximum_count();
    test_zero_count();
    test_rejected_count(65);
    test_rejected_count(255);
    test_partial_failure_rolls_back(100);
    test_partial_failure_rolls_back(255);
    test_partial_failure_rolls_back(600);
    test_large_partial_failure_rolls_back();
    cleanup();
    CHECK(!cleanup_failed, "Cannot clean allocator fixture directory");
    puts("Bulk sprite allocator checks passed: every geometry, fragmented rollback positions, bounds, and sentinels");
    return 0;
}
