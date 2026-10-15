/* Host-side writer for real, sorted WORDS.TOK loader fixtures. */
#ifndef TEST_WORDS_FIXTURE_H
#define TEST_WORDS_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_WORDS_FIXTURE_HEADER_SIZE = 52,
    TEST_WORDS_FIXTURE_MAX_WORD_SIZE = 41
};

typedef struct {
    FILE *file;
    uint16_t first_letter_offsets[26];
    char previous_word[TEST_WORDS_FIXTURE_MAX_WORD_SIZE];
    size_t previous_length;
    bool has_previous_word;
    bool failed;
} TestWordsFixtureWriter;

static inline bool test_words_fixture_begin(TestWordsFixtureWriter *writer, FILE *file)
{
    static const uint8_t empty_header[TEST_WORDS_FIXTURE_HEADER_SIZE] = { 0 };

    if (!writer || !file)
        return false;
    memset(writer, 0, sizeof(*writer));
    writer->file = file;
    if (fwrite(empty_header, 1, sizeof(empty_header), file) != sizeof(empty_header)) {
        writer->failed = true;
        return false;
    }
    return true;
}

static inline bool test_words_fixture_add(TestWordsFixtureWriter *writer,
                                           const char *word, uint16_t synonym)
{
    size_t length, prefix_length = 0;
    long record_position;
    unsigned letter;

    if (!writer || !writer->file || writer->failed || !word || synonym > 32767U)
        return false;
    length = strlen(word);
    if (!length || length >= TEST_WORDS_FIXTURE_MAX_WORD_SIZE ||
        word[0] < 'a' || word[0] > 'z')
        goto fail;
    for (size_t i = 0; i < length; ++i)
        if (word[i] < 'a' || word[i] > 'z')
            goto fail;
    if (writer->has_previous_word && strcmp(writer->previous_word, word) > 0)
        goto fail;

    letter = (unsigned)(word[0] - 'a');
    record_position = ftell(writer->file);
    if (record_position < TEST_WORDS_FIXTURE_HEADER_SIZE || record_position > UINT16_MAX)
        goto fail;
    if (!writer->first_letter_offsets[letter])
        writer->first_letter_offsets[letter] = (uint16_t)record_position;

    while (prefix_length < writer->previous_length && prefix_length < length &&
           writer->previous_word[prefix_length] == word[prefix_length])
        ++prefix_length;
    if (prefix_length == length && writer->has_previous_word)
        --prefix_length;
    if (fputc((int)prefix_length, writer->file) == EOF)
        goto fail;
    for (size_t i = prefix_length; i < length; ++i) {
        uint8_t encoded = (uint8_t)word[i] ^ 0x7f;
        if (i + 1 == length)
            encoded |= 0x80;
        if (fputc(encoded, writer->file) == EOF)
            goto fail;
    }
    if (fputc((int)(synonym >> 8), writer->file) == EOF ||
        fputc((int)(synonym & 0xff), writer->file) == EOF)
        goto fail;

    memcpy(writer->previous_word, word, length + 1);
    writer->previous_length = length;
    writer->has_previous_word = true;
    return true;

fail:
    writer->failed = true;
    return false;
}

static inline bool test_words_fixture_finish(TestWordsFixtureWriter *writer)
{
    uint8_t header[TEST_WORDS_FIXTURE_HEADER_SIZE] = { 0 };
    long end_position;

    if (!writer || !writer->file || writer->failed)
        return false;
    end_position = ftell(writer->file);
    if (end_position < TEST_WORDS_FIXTURE_HEADER_SIZE || end_position > UINT16_MAX)
        goto fail;
    for (size_t letter = 0; letter < 26; ++letter) {
        uint16_t offset = writer->first_letter_offsets[letter];
        header[letter * 2] = (uint8_t)(offset >> 8);
        header[letter * 2 + 1] = (uint8_t)offset;
    }
    if (fseek(writer->file, 0, SEEK_SET) != 0 ||
        fwrite(header, 1, sizeof(header), writer->file) != sizeof(header) ||
        fseek(writer->file, end_position, SEEK_SET) != 0)
        goto fail;
    return true;

fail:
    writer->failed = true;
    return false;
}

/* The writer enforces the production format's sorted-input contract. Build a
 * sorted pair first, then alter only the two terminal encoded letters to make
 * a syntactically valid but descending full-word stream for rejection tests. */
static inline bool test_words_fixture_write_descending_pair(FILE *file)
{
    TestWordsFixtureWriter writer;
    int first_terminal, second_terminal;

    if (!test_words_fixture_begin(&writer, file) ||
        !test_words_fixture_add(&writer, "aa", 11) ||
        !test_words_fixture_add(&writer, "az", 12) ||
        !test_words_fixture_finish(&writer))
        return false;

    if (fseek(file, TEST_WORDS_FIXTURE_HEADER_SIZE + 2, SEEK_SET) != 0 ||
        (first_terminal = fgetc(file)) != 0x9E ||
        fseek(file, TEST_WORDS_FIXTURE_HEADER_SIZE + 6, SEEK_SET) != 0 ||
        (second_terminal = fgetc(file)) != 0x85 ||
        fseek(file, TEST_WORDS_FIXTURE_HEADER_SIZE + 2, SEEK_SET) != 0 ||
        fputc(0x85, file) == EOF ||
        fseek(file, TEST_WORDS_FIXTURE_HEADER_SIZE + 6, SEEK_SET) != 0 ||
        fputc(0x9E, file) == EOF || fflush(file) != 0)
        return false;

    return true;
}

#endif
