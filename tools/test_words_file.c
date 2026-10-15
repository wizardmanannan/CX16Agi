/* Exercise the production WORDS.TOK loader with owned, malformed inputs. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _CX16_H
#include "../src/words.h"
#include "test_words_fixture.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(EXIT_FAILURE); } \
} while (0)

static char fixture[4096];
static FILE *wordsFile;
static byte resourceWorkArea[80];
static byte *allocations[16];
static size_t allocationSizes[16];
static size_t outstandingAllocations;
static size_t allocationAttempts;
static size_t failAllocationAttempt;
static bool allocationFailureTriggered;
static unsigned openCount;
static unsigned activeOpen;
static unsigned failOpen;
static long failReadOffset = -1;
static bool readFailureTriggered;
static bool testCaseCompleted;
static bool cleanupRegistered;

extern char *wordsData;
extern char wordsTextStart[];
extern word *synonymsList;
extern char **wordPointers;
extern MemoryArea *_memoryAreas;
extern byte wordsDataBank, wordsPointersBank, synonymsListBank;

static bool load_words(void)
{
	return b12LoadWords();
}

/* Keep the C code real while replacing only KERNAL file and bank memory. */
byte b6Cbm_openForSeeking(const char *name)
{
    char path[4096];
    CHECK(!strcmp(name, "words.tok"), "Production WORDS.TOK loader opened another file");
    CHECK(snprintf(path, sizeof(path), "%s/words.tok", fixture) < (int)sizeof(path),
          "WORDS.TOK fixture path too long");
    wordsFile = fopen(path, "rb");
    activeOpen = ++openCount;
    return wordsFile ? SEQUENTIAL_LFN : 0;
}

int cbm_read(unsigned char lfn, void *buffer, unsigned int size)
{
    (void)lfn;
    if (!wordsFile)
        return -1;
    if (activeOpen == failOpen && failReadOffset >= 0) {
        long position = ftell(wordsFile);
        if (position <= failReadOffset && position + size > failReadOffset) {
            size_t beforeFailure = (size_t)(failReadOffset - position);
            readFailureTriggered = true;
            return (int)fread(buffer, 1, beforeFailure, wordsFile);
        }
    }
    return (int)fread(buffer, 1, size, wordsFile);
}

void cbm_close(unsigned char lfn)
{
    (void)lfn;
    if (wordsFile) {
        fclose(wordsFile);
        wordsFile = NULL;
    }
}

int8_t b6Cx16_fseek(uint8_t channel, uint32_t offset)
{
    if (!wordsFile || channel != FILE_OPEN_ADDRESS || fseek(wordsFile, (long)offset, SEEK_SET))
        return -1;
    return 0;
}

byte *b10BankedAlloc(int size, byte *bank)
{
    size_t slot;
    byte *result;
    ++allocationAttempts;
    if (allocationAttempts == failAllocationAttempt) {
        allocationFailureTriggered = true;
        return NULL;
    }
    /* The production source multiplies by host-sized pointers/ints here.
     * Accept those host allocations so tests can check target-sized parser
     * bounds without reporting a false allocator-capacity failure. */
    if (size < 0 || size > 100000)
        return NULL;
    for (slot = 0; slot < sizeof(allocations) / sizeof(allocations[0]); ++slot)
        if (!allocations[slot]) break;
    CHECK(slot < sizeof(allocations) / sizeof(allocations[0]), "Too many WORDS.TOK allocations");
    result = (byte *)calloc(size ? (size_t)size : 1, 1);
    CHECK(result, "Cannot allocate simulated WORDS.TOK bank memory");
    allocations[slot] = result;
    allocationSizes[slot] = size ? (size_t)size : 1;
    ++outstandingAllocations;
    *bank = 1;
    return result;
}

boolean b10BankedDealloc(byte *ptr, byte bank)
{
    (void)bank;
    for (size_t slot = 0; slot < sizeof(allocations) / sizeof(allocations[0]); ++slot) {
        if (allocations[slot] == ptr) {
            free(ptr);
            allocations[slot] = NULL;
            allocationSizes[slot] = 0;
            --outstandingAllocations;
            return TRUE;
        }
    }
    return FALSE;
}

void *memCpyBanked(byte *destination, byte *source, byte bank, size_t size)
{
    (void)bank;
    if (!destination || !source)
        return NULL;
    return memcpy(destination, source, size);
}

void memCpyBankedBetween(byte *destination, byte destinationBank, byte *source, byte sourceBank, size_t size)
{
    (void)destinationBank;
    (void)sourceBank;
    if (!size)
        return;
    CHECK(destination && source, "Null WORDS.TOK bank copy");
    memcpy(destination, source, size);
}

MemoryArea testMemoryAreas[NO_SIZES];
MemoryArea *_memoryAreas = testMemoryAreas;

static void cleanup(void)
{
    char path[4096];
    if (wordsFile) {
        fclose(wordsFile);
        wordsFile = NULL;
    }
    for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); ++i) {
        free(allocations[i]);
        allocations[i] = NULL;
        allocationSizes[i] = 0;
    }
    outstandingAllocations = 0;
    if (snprintf(path, sizeof(path), "%s/words.tok", fixture) >= (int)sizeof(path)) {
        fprintf(stderr, "WORDS.TOK cleanup path too long\n");
        _Exit(EXIT_FAILURE);
    }
    if (unlink(path) && errno != ENOENT) {
        fprintf(stderr, "Could not remove WORDS.TOK fixture: %s\n", strerror(errno));
        _Exit(EXIT_FAILURE);
    }
    if (fixture[0] && rmdir(fixture) && errno != ENOENT) {
        fprintf(stderr, "Could not remove WORDS.TOK fixture directory: %s\n", strerror(errno));
        _Exit(EXIT_FAILURE);
    }
    if (cleanupRegistered && !testCaseCompleted) {
        fprintf(stderr, "WORDS.TOK loader exited before the selected case completed\n");
        _Exit(EXIT_FAILURE);
    }
}

static void write_words_file(const char *name, const uint8_t *bytes, size_t size)
{
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/%s", fixture, name) < (int)sizeof(path),
          "WORDS.TOK fixture path too long");
    FILE *file = fopen(path, "wb");
    CHECK(file, "Cannot create WORDS.TOK fixture");
    CHECK(fwrite(bytes, 1, size, file) == size, "Cannot write WORDS.TOK fixture");
    CHECK(!fclose(file), "Cannot close WORDS.TOK fixture");
}

static size_t make_words_file(uint8_t *bytes, size_t headerSize, unsigned int firstLetter,
                              const uint8_t *record, size_t recordSize)
{
    CHECK(firstLetter < 26, "WORDS.TOK fixture letter index out of range");
    memset(bytes, 0, headerSize + recordSize);

    for (size_t letter = 0; letter < 26; ++letter) {
        size_t offsetByte = letter * 2;
        if (offsetByte + 1 < headerSize) {
            uint16_t offset = letter == firstLetter ? (uint16_t)headerSize : 0;
            bytes[offsetByte] = (uint8_t)(offset >> 8);
            bytes[offsetByte + 1] = (uint8_t)offset;
        }
    }
    memcpy(bytes + headerSize, record, recordSize);
    return headerSize + recordSize;
}

typedef struct {
    char *wordsData;
    char **wordPointers;
    word *synonymsList;
    int numWords;
    int numSynonyms;
    byte wordsDataBank;
    byte wordsPointersBank;
    byte synonymsListBank;
    char text[6];
    size_t allocationCount;
} PublishedWordsState;

static PublishedWordsState snapshot_words_state(void)
{
    PublishedWordsState state;
    state.wordsData = wordsData;
    state.wordPointers = wordPointers;
    state.synonymsList = synonymsList;
    state.numWords = numWords;
    state.numSynonyms = numSynonyms;
    state.wordsDataBank = wordsDataBank;
    state.wordsPointersBank = wordsPointersBank;
    state.synonymsListBank = synonymsListBank;
    memcpy(state.text, wordsTextStart, sizeof(state.text));
    state.allocationCount = outstandingAllocations;
    return state;
}

static void check_words_state_unchanged(const PublishedWordsState *state, const char *message)
{
    CHECK(wordsData == state->wordsData && wordPointers == state->wordPointers &&
          synonymsList == state->synonymsList && numWords == state->numWords &&
          numSynonyms == state->numSynonyms && wordsDataBank == state->wordsDataBank &&
          wordsPointersBank == state->wordsPointersBank &&
          synonymsListBank == state->synonymsListBank &&
          !memcmp(wordsTextStart, state->text, sizeof(state->text)) &&
          outstandingAllocations == state->allocationCount, message);
}

static int suppress_stdout(void)
{
    int savedStdout, nullFile;
    CHECK(fflush(stdout) == 0, "Could not flush stdout before suppressing count-test output");
    savedStdout = dup(STDOUT_FILENO);
    CHECK(savedStdout >= 0, "Could not save stdout for count-test output");
    nullFile = open("/dev/null", O_WRONLY);
    CHECK(nullFile >= 0 && dup2(nullFile, STDOUT_FILENO) >= 0,
          "Could not suppress count-test output");
    close(nullFile);
    return savedStdout;
}

static void restore_stdout(int savedStdout)
{
    CHECK(fflush(stdout) == 0 && dup2(savedStdout, STDOUT_FILENO) >= 0,
          "Could not restore stdout after count-test output");
    close(savedStdout);
}

static PublishedWordsState load_valid_prefix_words(void)
{
    static const uint8_t antAnteAnte[] = {
        0x00, 0x1E, 0x11, 0x8B, 0x00, 0x07,
        0x03, 0x9A, 0x00, 0x08,
        0x03, 0x9A, 0x00, 0x08
    };
    uint8_t prefixWords[52 + sizeof(antAnteAnte)];
    size_t prefixSize = make_words_file(prefixWords, 52, 0, antAnteAnte, sizeof(antAnteAnte));

    write_words_file("words.tok", prefixWords, prefixSize);
    CHECK(load_words(), "Valid shared-prefix WORDS.TOK load reported failure");
    CHECK(numWords == 3 && !memcmp(wordsData, "ant\0ante\0ante\0", 14) &&
          !memcmp(wordsTextStart, "antant", 6) &&
          synonymsList[0] == 7 && synonymsList[1] == 8 && synonymsList[2] == 8,
          "Valid shared-prefix WORDS.TOK entries were not reconstructed");
    return snapshot_words_state();
}

static void check_discarded_words_state(const char *message)
{
    CHECK(!wordsData && !wordPointers && !synonymsList &&
          numWords == 0 && numSynonyms == 0 && !outstandingAllocations,
          message);
    CHECK(!wordsDataBank && !wordsPointersBank && !synonymsListBank,
          "Discard left stale WORDS.TOK allocation-bank metadata");
    for (size_t i = 0; i < 4500; ++i)
        CHECK(wordsTextStart[i] == 0, "Discard left stale WORDS.TOK search-cache entries");
}

static void check_discard_reload_failure_retry(void)
{
    static const uint8_t antAnteAnte[] = {
        0x00, 0x1E, 0x11, 0x8B, 0x00, 0x07,
        0x03, 0x9A, 0x00, 0x08,
        0x03, 0x9A, 0x00, 0x08
    };
    static const uint8_t dog[] = { 0x00, 0x1B, 0x10, 0x98, 0x00, 0x0A };
    uint8_t words[52 + sizeof(antAnteAnte)];
    uint8_t dogWords[52 + sizeof(dog)];
    size_t wordsSize;
    size_t dogSize;
    PublishedWordsState published;

    (void)load_valid_prefix_words();
    CHECK(outstandingAllocations == 3, "Initial dictionary did not own three allocations");
    b12DiscardWords();
    check_discarded_words_state("Discard retained WORDS.TOK allocations or published state");
    b12DiscardWords();
    check_discarded_words_state("Repeated discard changed the empty WORDS.TOK state");

    dogSize = make_words_file(dogWords, 52, 3, dog, sizeof(dog));
    write_words_file("words.tok", dogWords, dogSize);
    CHECK(load_words(), "Valid WORDS.TOK reload after discard reported failure");
    CHECK(numWords == 1 && numSynonyms == 10 && !memcmp(wordsData, "dog", 4) &&
          !memcmp(wordsTextStart, "dog", 3),
          "Reload after discard published the wrong WORDS.TOK data");
    for (size_t i = 3; i < 4500; ++i)
        CHECK(wordsTextStart[i] == 0, "Reload after discard retained stale search-cache entries");
    published = snapshot_words_state();

    wordsSize = make_words_file(words, 52, 0, antAnteAnte, sizeof(antAnteAnte));
    write_words_file("words.tok", words, wordsSize);
    readFailureTriggered = false;
    failOpen = openCount + 2;
    failReadOffset = 52 + sizeof(antAnteAnte) - 1;
    CHECK(!load_words(), "Short WORDS.TOK reload reported success");
    failOpen = 0;
    failReadOffset = -1;
    CHECK(readFailureTriggered, "Could not inject WORDS.TOK read failure after discard/reload");
    check_words_state_unchanged(&published,
          "Failed WORDS.TOK reload changed the current dictionary after discard/reload");

    CHECK(load_words(), "Valid WORDS.TOK retry after discard reported failure");
    CHECK(numWords == 3 && !memcmp(wordsData, "ant\0ante\0ante\0", 14) &&
          !memcmp(wordsTextStart, "antant", 6) &&
          synonymsList[0] == 7 && synonymsList[1] == 8 && synonymsList[2] == 8 &&
          outstandingAllocations == published.allocationCount,
          "WORDS.TOK retry did not replace the same resource ID after discard and failure");

    b12DiscardWords();
    check_discarded_words_state("Final discard retained WORDS.TOK lifecycle state");
    b12DiscardWords();
    check_discarded_words_state("Repeated final discard changed the empty WORDS.TOK state");
}

int main(int argc, char **argv)
{
    static const char *const caseNames[] = {
        "--truncated-synonym", "--prefix-bound", "--full-word-order", "--short-header", "--shared-prefix",
        "--allocation-failure", "--staging-read", "--retry", "--first-nonzero", "--word-count",
        "--discard-reload", "--missing-file", "--trailing-sentinel"
    };
    static const uint8_t validAnt[] = { 0x00, 0x1E, 0x11, 0x8B, 0x00, 0x07 };
    static const uint8_t truncatedAnt[] = { 0x00, 0x1E, 0x11, 0x8B, 0x00 };
    static const uint8_t badPrefixRecord[] = { 0x50, 0x80, 0x00, 0x09 };
    static const uint8_t validDog[] = { 0x00, 0x1B, 0x10, 0x98, 0x00, 0x0A };
    uint8_t validWords[52 + sizeof(validAnt)];
    size_t validSize;
    PublishedWordsState published;
    const char *tmp = getenv("TMPDIR");
    const char *selectedCase = argc == 2 ? argv[1] : "--truncated-synonym";
    bool recognized = false;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [WORDS.TOK case selector]\n", argv[0]);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof(caseNames) / sizeof(caseNames[0]); ++i)
        if (!strcmp(selectedCase, caseNames[i])) recognized = true;
    if (!recognized) {
        fprintf(stderr, "Unknown WORDS.TOK case: %s\n", selectedCase);
        return EXIT_FAILURE;
    }

    CHECK(snprintf(fixture, sizeof(fixture), "%s/words-file-XXXXXX", tmp ? tmp : "/tmp") < (int)sizeof(fixture),
          "WORDS.TOK fixture path too long");
    CHECK(mkdtemp(fixture), "Cannot create WORDS.TOK fixture directory");
    if (atexit(cleanup) != 0) {
        cleanup();
        fprintf(stderr, "Cannot register WORDS.TOK fixture cleanup\n");
        return EXIT_FAILURE;
    }
    cleanupRegistered = true;

    validSize = make_words_file(validWords, 52, 0, validAnt, sizeof(validAnt));
    write_words_file("words.tok", validWords, validSize);
    CHECK(load_words(), "Initial valid WORDS.TOK load reported failure");
    CHECK(numWords == 1 && wordsData && !memcmp(wordsData, "ant", 4) &&
          !memcmp(wordsTextStart, "ant", 3) && synonymsList[0] == 7,
          "Valid 52-byte WORDS.TOK header and first word were not loaded");

    published = snapshot_words_state();
    if (!strcmp(selectedCase, "--truncated-synonym")) {
        uint8_t truncatedSynonym[52 + sizeof(truncatedAnt)];
        size_t size = make_words_file(truncatedSynonym, 52, 0, truncatedAnt, sizeof(truncatedAnt));
        write_words_file("words.tok", truncatedSynonym, size);
        CHECK(!load_words(), "Truncated synonym data reported a successful WORDS.TOK load");
        check_words_state_unchanged(&published,
              "Truncated synonym data replaced the published WORDS.TOK state");
    } else if (!strcmp(selectedCase, "--prefix-bound")) {
        uint8_t invalidPrefix[52 + sizeof(badPrefixRecord)];
        size_t size = make_words_file(invalidPrefix, 52, 0, badPrefixRecord, sizeof(badPrefixRecord));
        write_words_file("words.tok", invalidPrefix, size);
        CHECK(!load_words(), "Invalid prefix reported a successful WORDS.TOK load");
        check_words_state_unchanged(&published,
              "Out-of-range WORDS.TOK prefix replaced the published dictionary");
    } else if (!strcmp(selectedCase, "--full-word-order")) {
        char path[4096];
        CHECK(snprintf(path, sizeof(path), "%s/words.tok", fixture) < (int)sizeof(path),
              "WORDS.TOK descending-order path too long");
        FILE *file = fopen(path, "w+b");
        CHECK(file, "Cannot create descending full-word WORDS.TOK fixture");
        CHECK(test_words_fixture_write_descending_pair(file),
              "Cannot create descending full-word WORDS.TOK fixture");
        CHECK(!fclose(file), "Cannot close descending full-word WORDS.TOK fixture");
        CHECK(!load_words(), "Descending full-word order reported a successful WORDS.TOK load");
        check_words_state_unchanged(&published,
              "Descending WORDS.TOK input replaced the published dictionary");
    } else if (!strcmp(selectedCase, "--short-header")) {
        uint8_t shortHeader[51] = { 0 };
        write_words_file("words.tok", shortHeader, sizeof(shortHeader));
        CHECK(!load_words(), "Short WORDS.TOK header reported a successful load");
        check_words_state_unchanged(&published,
              "Truncated 26-letter WORDS.TOK offset table replaced the published dictionary");
    } else if (!strcmp(selectedCase, "--shared-prefix")) {
        (void)load_valid_prefix_words();
    } else if (!strcmp(selectedCase, "--allocation-failure")) {
        published = load_valid_prefix_words();
        allocationFailureTriggered = false;
        failAllocationAttempt = allocationAttempts + 3;
        CHECK(!load_words(), "Injected WORDS.TOK allocation failure reported success");
        failAllocationAttempt = 0;
        CHECK(allocationFailureTriggered, "Could not inject WORDS.TOK staging allocation failure");
        check_words_state_unchanged(&published,
              "Partial WORDS.TOK allocation failure changed published state or leaked staging memory");
    } else if (!strcmp(selectedCase, "--staging-read") || !strcmp(selectedCase, "--retry")) {
        published = load_valid_prefix_words();
        readFailureTriggered = false;
        failOpen = openCount + 2;
        failReadOffset = 52 + 14 - 1; /* final synonym byte in the three-record fixture */
        CHECK(!load_words(), "Injected WORDS.TOK staging read failure reported success");
        failOpen = 0;
        failReadOffset = -1;
        CHECK(readFailureTriggered, "Could not inject short WORDS.TOK staging read");
        check_words_state_unchanged(&published,
              "Short WORDS.TOK read during staging changed published state or leaked allocations");
        if (!strcmp(selectedCase, "--retry")) {
            CHECK(load_words(), "Valid WORDS.TOK retry after staging failure reported failure");
            CHECK(numWords == 3 && !memcmp(wordsData, "ant\0ante\0ante\0", 14) &&
                  synonymsList[0] == 7 && synonymsList[1] == 8 && synonymsList[2] == 8 &&
                  outstandingAllocations == published.allocationCount,
                  "Valid WORDS.TOK retry failed after staged load errors");
        }
    } else if (!strcmp(selectedCase, "--first-nonzero")) {
        uint8_t dogWords[52 + sizeof(validDog)];
        size_t size = make_words_file(dogWords, 52, 3, validDog, sizeof(validDog));
        write_words_file("words.tok", dogWords, size);
        CHECK(load_words(), "Valid WORDS.TOK with first nonzero letter reported failure");
        CHECK(numWords == 1 && !memcmp(wordsData, "dog", 4) &&
              !memcmp(wordsTextStart, "dog", 3) && synonymsList[0] == 10,
              "Loader did not start at the first nonzero letter offset when `a` is absent");
    } else if (!strcmp(selectedCase, "--discard-reload")) {
        check_discard_reload_failure_retry();
    } else if (!strcmp(selectedCase, "--missing-file")) {
        char path[4096];
        CHECK(snprintf(path, sizeof(path), "%s/words.tok", fixture) < (int)sizeof(path),
              "WORDS.TOK fixture path too long");
        CHECK(!unlink(path), "Could not remove WORDS.TOK for missing-file case");
        CHECK(!load_words(), "Missing WORDS.TOK reported success");
        check_words_state_unchanged(&published,
              "Missing WORDS.TOK changed the published dictionary");
    } else if (!strcmp(selectedCase, "--trailing-sentinel")) {
        uint8_t terminatedWords[52 + sizeof(validAnt) + 1];
        size_t size = make_words_file(terminatedWords, 52, 0, validAnt, sizeof(validAnt));
        terminatedWords[size++] = 0;
        write_words_file("words.tok", terminatedWords, size);
        CHECK(load_words(), "WORDS.TOK trailing letter sentinel was rejected");
        CHECK(numWords == 1 && !memcmp(wordsData, "ant", 4) && synonymsList[0] == 7,
              "WORDS.TOK trailing sentinel changed the final dictionary entry");
    } else {
        const size_t tooManyWords = 1501; /* wordsTextStart has 4500 bytes / 3 per row */
        const size_t bytesPerWord = 4;
        size_t bodySize = tooManyWords * bytesPerWord;
        uint8_t *body = (uint8_t *)malloc(bodySize);
        uint8_t *oversized = (uint8_t *)malloc(52 + bodySize);
        int savedStdout;
        CHECK(body && oversized, "Cannot allocate oversized WORDS.TOK fixture");
        for (size_t i = 0; i < tooManyWords; ++i) {
            body[i * bytesPerWord] = 0;
            body[i * bytesPerWord + 1] = 0x9E; /* encrypted 'a' with end bit */
            body[i * bytesPerWord + 2] = 0;
            body[i * bytesPerWord + 3] = 7;
        }
        size_t oversizedSize = make_words_file(oversized, 52, 0, body, bodySize);
        write_words_file("words.tok", oversized, oversizedSize);
        free(body);
        free(oversized);
        savedStdout = suppress_stdout();
        CHECK(!load_words(), "Impossible WORDS.TOK word count reported success");
        restore_stdout(savedStdout);
        check_words_state_unchanged(&published,
              "Impossible WORDS.TOK word count replaced the dictionary or exceeded its text table");
    }

    printf("WORDS.TOK case passed: %s\n", selectedCase);
    testCaseCompleted = true;
    return 0;
}

/* Compile production words.c with its physical scratch RAM mapped to this fixture. */
#undef GOLDEN_RAM_WORK_AREA
#define GOLDEN_RAM_WORK_AREA resourceWorkArea
#include "../src/words.c"

/* wordsTextStart is fixed in WORD_BANK on target; map that exact banked write
 * to its real production array in this host test instead of silently no-oping. */
void memsetBanked(void *destination, int value, size_t size, byte bank)
{
    CHECK(destination == wordsTextStart && bank == WORD_BANK &&
          size <= sizeof(wordsTextStart),
          "WORDS.TOK clear escaped the published text table or its bank");
    memset(destination, value, size);
}
