/* Exercise the production binary search assembly with native 6502 execution. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
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

enum { WORDS_TEXT_CAPACITY = 4500, WORD_METADATA_BYTES = 3 };
static uint8_t ram[65536];
static char fixture[4096];
static struct { char name[100]; unsigned address; } labels[64];
static unsigned label_count;
static bool watch_search_reads;
static bool invalid_data_read;
static unsigned invalid_data_address;
static unsigned invalid_data_pc;
static unsigned query_reads;

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } \
} while (0)

static bool in_range(unsigned address, unsigned start, unsigned size) {
    return address >= start && address - start < size;
}

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    /* The emulator reads the opcode after incrementing PC, but absolute
       addressing modes fetch their operand bytes before advancing PC. */
    bool instruction_stream_read = address == (uint16_t)(regs.pc - 1) ||
        address == regs.pc || address == (uint16_t)(regs.pc + 1);
    if (watch_search_reads && !instruction_stream_read) {
        unsigned words = 0, count = 0, query = 0;
        unsigned comparator_calls = 0, comparator_results = 0;
        for (unsigned i = 0; i < label_count; ++i) {
            if (!strcmp(labels[i].name, "_wordsTextStart")) words = labels[i].address;
            else if (!strcmp(labels[i].name, "_numWords")) count = labels[i].address;
            else if (!strcmp(labels[i].name, "queryText")) query = labels[i].address;
            else if (!strcmp(labels[i].name, "comparatorCalls")) comparator_calls = labels[i].address;
            else if (!strcmp(labels[i].name, "comparatorResults")) comparator_results = labels[i].address;
        }
        unsigned word_count = ram[count] | ((unsigned)ram[count + 1] << 8);
        unsigned metadata_bytes = word_count * WORD_METADATA_BYTES;
        if (metadata_bytes > WORDS_TEXT_CAPACITY) metadata_bytes = WORDS_TEXT_CAPACITY;
        if (in_range(address, query, 3)) ++query_reads;
        bool expected_data = address < 0x0200 ||
            in_range(address, words, metadata_bytes) ||
            in_range(address, count, 2) || in_range(address, query, 3) ||
            in_range(address, comparator_calls, 1) || in_range(address, comparator_results, 4);
        if (!expected_data) {
            invalid_data_read = true;
            invalid_data_address = address;
            invalid_data_pc = regs.pc;
            fprintf(stderr, "Unexpected read $%04X; metadata $%04X+%u, count $%04X=%u, query $%04X\n",
                    address, words, metadata_bytes, count, word_count, query);
        }
    }
    return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
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
    for (unsigned i = 0; i < label_count; ++i)
        if (!strcmp(name, labels[i].name)) return labels[i].address;
    fprintf(stderr, "Missing assembler symbol: %s\n", name);
    exit(1);
}

static void remove_fixture(void) {
    const char *names[] = { "test.s", "words_stub.s", "test.cfg", "test.o", "words_stub.o", "test.bin", "test.lbl" };
    char path[4096];
    bool failed = false;
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (snprintf(path, sizeof(path), "%s/%s", fixture, names[i]) >= (int)sizeof(path) ||
            (unlink(path) && errno != ENOENT)) failed = true;
    }
    if (fixture[0] && rmdir(fixture)) failed = true;
    if (failed) {
        fprintf(stderr, "Could not clean words assembly fixture: %s\n", fixture);
        _Exit(EXIT_FAILURE);
    }
}

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");
    CHECK(file, "Cannot create assembler fixture");
    size_t length = strlen(contents);
    CHECK(fwrite(contents, 1, length, file) == length, "Cannot write assembler fixture");
    CHECK(!fclose(file), "Cannot close assembler fixture");
}

static void run_tool(char *const arguments[]) {
    char executable[4096];
    const char *home = getenv("CC65_HOME");
    if (home) CHECK(snprintf(executable, sizeof(executable), "%s/bin/%s", home, arguments[0]) < (int)sizeof(executable), "CC65 tool path too long");
    else CHECK(snprintf(executable, sizeof(executable), "%s", arguments[0]) < (int)sizeof(executable), "CC65 tool path too long");
    pid_t child = fork();
    CHECK(child >= 0, "Cannot fork assembler");
    if (!child) {
        char *argv[16]; unsigned count = 0;
        while (arguments[count]) {
            CHECK(count < 15, "Too many assembler arguments");
            argv[count] = arguments[count];
            ++count;
        }
        argv[0] = executable;
        argv[count] = NULL;
        execvp(executable, argv);
        perror(executable);
        _exit(127);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child, "Cannot wait for assembler");
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "Assembler or linker failed");
}

static void assemble(void) {
    char path[4096], stub_path[4096], config[4096], object[4096], stub_object[4096], binary[4096], labels_path[4096];
    const char *names[] = { "test.s", "words_stub.s", "test.cfg", "test.o", "words_stub.o", "test.bin", "test.lbl" };
    char *paths[] = { path, stub_path, config, object, stub_object, binary, labels_path };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        CHECK(snprintf(paths[i], 4096, "%s/%s", fixture, names[i]) < 4096, "Fixture path too long");

    write_file(path,
        ".include \"global.s\"\n"
        ".import pushax\n"
        ".export _b12FindSynonymNumSearch\n"
        ".include \"wordsAsm.s\"\n"
        "searchSyn = ZP_TMP_10\n"
        ".export searchSyn\n"
        );
    write_file(stub_path,
        ".segment \"BANKRAM12\"\n"
        ".export _numWords,_wordsTextStart,queryText,comparatorCalls,comparatorResults\n"
        "_numWords: .word 0\n"
        "_wordsTextStart: .res 4500,$0\n"
        "queryText: .res 3,$0\n"
        "comparatorCalls: .byte 0\n"
        "comparatorResults: .res 4,$0\n"
        ".segment \"CODE\"\n"
        ".export _memcpyBanked,_b12CompareWithWordNumber,pushax,done\n"
        "_memcpyBanked: rts\n"
        "_b12CompareWithWordNumber:\n"
        "ldx comparatorCalls\n"
        "inc comparatorCalls\n"
        "lda comparatorResults,x\n"
        "rts\n"
        "pushax: rts\n"
        "done: brk\n");
    write_file(config,
        "MEMORY { ZP:start=$22,size=$100,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\n"
        "SEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; BANKRAM12:load=RAM,type=rw,optional=yes; }\n");

    char *ca65[] = { "ca65", "-t", "cx16", "-I", "src", path, "-o", object, NULL };
    char *ca65_stub[] = { "ca65", "-t", "cx16", stub_path, "-o", stub_object, NULL };
    char *ld65[] = { "ld65", "-C", config, stub_object, object, "-o", binary, "-Ln", labels_path, NULL };
    run_tool(ca65);
    run_tool(ca65_stub);
    run_tool(ld65);

    FILE *file = fopen(binary, "rb");
    CHECK(file, "Cannot open linked words fixture");
    CHECK(fread(ram + 0x2000, 1, 0x6000, file) > 0, "Linked words fixture is empty");
    CHECK(!ferror(file) && !fclose(file), "Cannot read linked words fixture");
    file = fopen(labels_path, "r");
    CHECK(file, "Cannot open words fixture labels");
    char line[256], kind[16], name[100]; unsigned address;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
        CHECK(label_count < sizeof(labels) / sizeof(labels[0]), "Too many words fixture symbols");
        strcpy(labels[label_count].name, name[0] == '.' ? name + 1 : name);
        labels[label_count++].address = address;
    }
    CHECK(!fclose(file), "Cannot close words fixture labels");
}

static bool search(const char *name, const char *query, unsigned word_count,
                   const uint8_t *metadata, const uint8_t *comparator_results,
                   unsigned expected, unsigned expected_comparator_calls,
                   unsigned expected_synonym) {
    const unsigned words_address = symbol("_wordsTextStart");
    const unsigned count_address = symbol("_numWords");
    const unsigned query_address = symbol("queryText");
    const unsigned comparator_calls_address = symbol("comparatorCalls");
    const unsigned comparator_results_address = symbol("comparatorResults");
    memset(ram + words_address, 0xA5, WORDS_TEXT_CAPACITY);
    ram[count_address] = (uint8_t)word_count;
    ram[count_address + 1] = (uint8_t)(word_count >> 8);
    if (metadata) memcpy(ram + words_address, metadata, word_count * WORD_METADATA_BYTES);
    ram[comparator_calls_address] = 0;
    memset(ram + comparator_results_address, 0, 4);
    if (comparator_results) memcpy(ram + comparator_results_address, comparator_results, 4);
    ram[symbol("searchSyn")] = (uint8_t)expected_synonym;
    ram[symbol("searchSyn") + 1] = (uint8_t)(expected_synonym >> 8);
    memset(ram + query_address, 0, 3);
    size_t length = strlen(query);
    CHECK(length <= 3, "Query prefix is too long");
    memcpy(ram + query_address, query, length);

    reset6502(false);
    regs.pc = symbol("_b12FindSynonymNumSearch");
    regs.sp = 0x1FD;
    regs.db = 0x0C;
    regs.k = 0x0C;
    regs.a = (uint8_t)query_address;
    regs.x = (uint8_t)(query_address >> 8);
    regs.status = FLAG_CONSTANT;
    ram[0x1FE] = (uint8_t)(symbol("done") - 1);
    ram[0x1FF] = (uint8_t)((symbol("done") - 1) >> 8);
    waiting = 0;

    invalid_data_read = false;
    invalid_data_address = 0;
    invalid_data_pc = 0;
    query_reads = 0;
    watch_search_reads = true;
    unsigned steps;
    for (steps = 0; steps < 10000 && regs.pc != symbol("done") && !invalid_data_read; ++steps) {
        if (waiting) {
            fprintf(stderr, "FAIL %s: words search unexpectedly waited\n", name);
            watch_search_reads = false;
            return false;
        }
        step6502();
    }
    watch_search_reads = false;
    bool passed = true;
    if (invalid_data_read) {
        fprintf(stderr, "FAIL %s: dictionary search read outside its valid metadata at $%04X (PC $%04X)\n",
                name, invalid_data_address, invalid_data_pc);
        passed = false;
    }
    if (regs.pc != symbol("done")) {
        fprintf(stderr, "FAIL %s: dictionary search did not return within %u steps\n", name, steps);
        passed = false;
    }
    if (regs.pc == symbol("done") && regs.sp != 0x1FF) {
        fprintf(stderr, "FAIL %s: dictionary search returned with stack pointer $%02X, expected $FF\n",
                name, regs.sp);
        passed = false;
    }
    if (regs.pc == symbol("done") && (regs.k != 0x0C || regs.db != 0x0C)) {
        fprintf(stderr, "FAIL %s: dictionary search changed code/data bank registers\n", name);
        passed = false;
    }
    if (!word_count && query_reads) {
        fprintf(stderr, "FAIL %s: empty dictionary read %u query bytes\n", name, query_reads);
        passed = false;
    }
    if (!invalid_data_read && regs.pc == symbol("done") &&
        (regs.a != (expected & 0xFF) || regs.x != ((expected >> 8) & 0xFF))) {
        fprintf(stderr, "FAIL %s: dictionary search returned $%02X%02X, expected $%04X\n",
                name, regs.x, regs.a, expected);
        passed = false;
    }
    if (ram[comparator_calls_address] != expected_comparator_calls) {
        fprintf(stderr, "FAIL %s: full comparator called %u times, expected %u\n",
                name, ram[comparator_calls_address], expected_comparator_calls);
        passed = false;
    }
    return passed;
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    CHECK(snprintf(fixture, sizeof(fixture), "%s/words-assembly-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(fixture), "Fixture path too long");
    CHECK(mkdtemp(fixture), "Cannot create words assembly fixture directory");
    atexit(remove_fixture);
    assemble();

    const uint8_t first_word[3] = { 'a', 0, 0 };
    const uint8_t exact_word[3] = { 'a', 'b', 'c' };
    const uint8_t duplicate_prefixes[9] = { 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c' };
    const uint8_t equal_result[4] = { 0, 0, 0, 0 };
    const uint8_t below_result[4] = { 0xFF, 0, 0, 0 };
    const uint8_t above_result[4] = { 1, 1, 0, 0 };
    const uint8_t duplicate_result[4] = { 1, 0, 0, 0 };
    uint8_t records_513[513 * WORD_METADATA_BYTES];
    memset(records_513, 'z', sizeof(records_513));

    bool passed = true;
    if (!search("empty dictionary", "!", 0, NULL, NULL, 0xFFFF, 0, 0)) passed = false;
    if (!search("prefix below first word", "!", 1, first_word, NULL, 0xFFFF, 0, 0)) passed = false;
    if (!search("prefix above last word", "z", 1, first_word, NULL, 0xFFFF, 0, 0)) passed = false;
    if (!search("midpoint $0100 borrow", "!", 513, records_513, NULL, 0xFFFF, 0, 0)) passed = false;
    if (!search("matching prefix", "abc", 1, exact_word, equal_result, 0x1234, 1, 0x1234)) passed = false;
    if (!search("full comparison below first word", "abc", 1, exact_word, below_result, 0xFFFF, 1, 0)) passed = false;
    if (!search("full comparison above last word", "abc", 1, exact_word, above_result, 0xFFFF, 1, 0)) passed = false;
    if (!search("later word with a duplicate prefix", "abc", 3, duplicate_prefixes, duplicate_result, 0x5678, 2, 0x5678)) passed = false;
    if (!passed) return 1;
    puts("Production words assembly passes empty/boundary and controlled prefix comparison searches");
    return 0;
}
