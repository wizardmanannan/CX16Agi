/***************************************************************************
** words.c
**
** These functions are used to load the WORDS.TOK file. The words are
** stored in a sorted list along with their synonyms numbers. This allows
** us to use a binary search when we need to determine a words synonym
** number.
**
** (c) 1997 Lance Ewing - Initial code (27 Aug 97)
***************************************************************************/

#include <stdio.h>

#include "general.h"
#include "words.h"

//#define VERBOSE_CALC_WORDS
//#define VERBOSE_LETTERS
//#define VERBOSE_WORDS
//#define VERBOSE_COMPRESS
//#define VERBOSE_UPDATE_WORD_POINTERS
//#define VERBOSE_COMPARE_WITH_WORD_NUMBER

//Requires lowercase asscii
extern int b12FindSynonymNumSearch(char* userWord);

//#define TEST_SYN

#pragma rodata (push, "BANKRAM12")
const char B12_WORDS_FILE[] = "words.tok";
const char B12_LOADING_WORDS[] = "Loading Words\n";
const char B12_CANNOT_FIND_FILE[] = "Cannot find file : WORDS.TOK\n";
const char B12_WORDS_OVERFLOW[] = "Too many words, overflowed\n";
#pragma rodata (pop)

#pragma bss-name (push, "BANKRAM12")
#define WORDS_TEXT_START_SIZE 4500
#define WORDS_MAX_COUNT (WORDS_TEXT_START_SIZE / 3)
char wordsTextStart[WORDS_TEXT_START_SIZE];
int numWords, numSynonyms;  /* Big difference between the two */
byte wordBank;
char* wordsData;
byte wordsDataBank;
char** wordPointers; //This will be used to store pointers to all of the words, in the same order as the words metadata
byte wordsPointersBank;
word* synonymsList;
byte synonymsListBank;

#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM12")
typedef struct {
    int wordCount;
    int synonymCount;
    int dataSize;
} WordsLoadInfo;

#pragma wrapped-call (push, trampoline, PALETTE_MANAGER_BANK)
boolean b0FReadWordsPass(char *stagedWords, byte stagedWordsBank,
                         char **stagedPointers, byte stagedPointersBank,
                         word *stagedSynonyms, byte stagedSynonymsBank,
                         const WordsLoadInfo *capacity, WordsLoadInfo *result);
#pragma wrapped-call (pop)

#pragma code-name (push, "BANKRAM0F")
boolean b0FReadWordsPass(char *stagedWords, byte stagedWordsBank,
                         char **stagedPointers, byte stagedPointersBank,
                         word *stagedSynonyms, byte stagedSynonymsBank,
                         const WordsLoadInfo *capacity, WordsLoadInfo *result)
{
    byte header[52];
    byte offsetSeen[26];
    byte prefixLength;
    byte encoded;
    byte terminal;
    boolean endOfWords;
    byte synonymBytes[2];
    word offsets[26];
    word synonym;
    word maxSynonym = 0;
    long position;
    long recordPosition;
    word firstOffset = 52;
    word previousOffset = 0;
    int previousLetter = -1;
    int previousWordLength = 0;
    char previousWord[MAX_WORD_SIZE];
    int wordCount = 0;
    int synonymCount = 0;
    int dataSize = 0;
    int newWordLength;
    int wordSize;
    int letter;
    int i;
    int readResult;
    int bytesRead;
    char *newWord = (char *)GOLDEN_RAM_WORK_AREA;
    char *wordsDataPointer = stagedWords;
    char **wordPointersPointer = stagedPointers;
    word *synonymsPointer = stagedSynonyms;

    if (!result || ((stagedWords || stagedPointers || stagedSynonyms) &&
                    (!stagedWords || !stagedPointers || !stagedSynonyms)))
        return FALSE;
    if ((capacity && (!stagedWords || !stagedPointers || !stagedSynonyms)) ||
        (!capacity && (stagedWords || stagedPointers || stagedSynonyms)))
        return FALSE;
    if (b6Cx16_fseek(FILE_OPEN_ADDRESS, 0) != 0 ||
        cbm_read(SEQUENTIAL_LFN, header, sizeof(header)) != (int)sizeof(header))
        return FALSE;

    memset(offsetSeen, 0, sizeof(offsetSeen));
    for (i = 0; i < 26; ++i) {
        offsets[i] = (word)((unsigned long)header[i * 2] * 256UL + header[i * 2 + 1]);
        if (!offsets[i])
            continue;
        if (offsets[i] < sizeof(header) || offsets[i] <= previousOffset)
            return FALSE;
        if (previousOffset == 0)
            firstOffset = offsets[i];
        previousOffset = offsets[i];
    }

    if (b6Cx16_fseek(FILE_OPEN_ADDRESS, firstOffset) != 0)
        return FALSE;
    position = firstOffset;
    for (;;) {
        readResult = cbm_read(SEQUENTIAL_LFN, &prefixLength, 1);
        if (readResult == 0)
            break;
        if (readResult != 1 || position > 65535L)
            return FALSE;
        recordPosition = position;
        if (prefixLength > previousWordLength || (!wordCount && prefixLength))
            return FALSE;
        ++position;
        newWordLength = prefixLength;
        endOfWords = FALSE;
        do {
            readResult = cbm_read(SEQUENTIAL_LFN, &encoded, 1);
            if (readResult == 0 && !prefixLength && wordCount) {
                endOfWords = TRUE;
                break;
            }
            if (readResult != 1 || newWordLength >= MAX_WORD_SIZE - 1)
                return FALSE;
            ++position;
            terminal = encoded & 0x80;
            encoded ^= 0x7F;
            encoded &= 0x7F;
            if (!encoded)
                return FALSE;
            newWord[newWordLength++] = (char)encoded;
        } while (!terminal);
        if (endOfWords)
            break;
        if (wordCount >= WORDS_MAX_COUNT ||
            (capacity && wordCount >= capacity->wordCount))
            return FALSE;
        if (newWordLength <= prefixLength)
            return FALSE;
        newWord[newWordLength] = '\0';

        /* wordsAsm searches the fully reconstructed words in lexical order. */
        if (wordCount) {
            for (i = 0; i < newWordLength && i < previousWordLength; ++i) {
                if (newWord[i] < previousWord[i])
                    return FALSE;
                if (newWord[i] > previousWord[i])
                    break;
            }
            if (i == newWordLength && newWordLength < previousWordLength)
                return FALSE;
        }

        letter = newWord[0] - ASCIIa;
        if (letter < 0 || letter >= 26 || letter < previousLetter)
            return FALSE;
        if (letter != previousLetter) {
            if (prefixLength || offsets[letter] != (word)recordPosition || offsetSeen[letter])
                return FALSE;
            offsetSeen[letter] = 1;
        } else if (!offsets[letter] || !offsetSeen[letter]) {
            return FALSE;
        }

        bytesRead = cbm_read(SEQUENTIAL_LFN, synonymBytes, sizeof(synonymBytes));
        if (bytesRead != (int)sizeof(synonymBytes))
            return FALSE;
        position += sizeof(synonymBytes);
        synonym = (word)((unsigned long)synonymBytes[0] * 256UL + synonymBytes[1]);
        if (synonym != 9999 && synonym > 32767U)
            return FALSE;
        if (synonym != 9999 && synonym > maxSynonym)
            maxSynonym = synonym;

        wordSize = newWordLength + 1;
        if (wordSize > LARGE_SIZE - dataSize ||
            (capacity && dataSize + wordSize > capacity->dataSize))
            return FALSE;
        if (stagedWords) {
            if (memCpyBanked((byte *)wordsDataPointer, (byte *)newWord,
                             stagedWordsBank, (size_t)wordSize) != wordsDataPointer ||
                memCpyBanked((byte *)wordPointersPointer, (byte *)&wordsDataPointer,
                             stagedPointersBank, sizeof(char *)) != wordPointersPointer ||
                memCpyBanked((byte *)synonymsPointer, (byte *)&synonym,
                             stagedSynonymsBank, sizeof(word)) != synonymsPointer)
                return FALSE;
            wordsDataPointer += wordSize;
            ++wordPointersPointer;
            ++synonymsPointer;
        }

        dataSize += wordSize;
        ++wordCount;
        for (i = 0; i <= newWordLength; ++i)
            previousWord[i] = newWord[i];
        previousWordLength = newWordLength;
        previousLetter = letter;
    }

    for (i = 0; i < 26; ++i)
        if (offsets[i] && !offsetSeen[i])
            return FALSE;

    synonymCount = (int)maxSynonym;
    if (capacity && (wordCount != capacity->wordCount ||
                     synonymCount != capacity->synonymCount ||
                     dataSize != capacity->dataSize))
        return FALSE;
    result->wordCount = wordCount;
    result->synonymCount = synonymCount;
    result->dataSize = dataSize;
    return TRUE;
}
#pragma code-name (pop)

byte b12OpenWords()
{
    byte lfn = b6Cbm_openForSeeking(B12_WORDS_FILE);    
    if (!lfn) {
        printf(B12_CANNOT_FIND_FILE);
        return 0;
    }

    return lfn;
}

#ifdef TEST_SYN //Designed for King's Quest 1
void b12TestSyn()
{
    char* testBuffer;
    byte testBufferBank;
    char test1[5] = { 0x67, 0x69, 0x72, 0x6C, 0x0 }; //girl 
    char test2[5] = { 0x73, 0x70, 0x0 }; //sp
    char test3[6] = { 0x67, 0x69, 0x72, 0x6C, 0x70, 0x0 }; //girlp Note: should fail
    char test4[2] = { 0x61, 0x0 }; //a
    char test5[4] = { 0x6C, 0x6D, 0x6E, 0x0 }; //lmn Note: should fail
    char test6[3] = { 0x61, 0x6E, 0x0 }; //an
    char test7[5] = { 0x79, 0x6f, 0x75, 0x72, 0x0 }; //your
    char test8[6] = { 0x79, 0x6f, 0x75, 0x72, 0x72, 0x0 }; //yourr Note: should fail
    char test9[2] = { 0x80, 0x0 }; //z Note: should fail
    char test10[3] = { 0x79, 0x79, 0x0 }; //yy Note: should fail
    char test11[4] = { 0x79, 0x75, 0x61, 0x0 }; //yoa Note: should fail
    char test12[2] = { 0x60, 0x0 }; // ` Note: should fail
    char test13[5] = { 0x6C, 0x6F, 0x6F, 0x6B, 0x0 };
    char test14[9] = { 0x6C, 0x6F, 0x6F, 0x6B, 0x20, 0x66, 0x6F, 0x72, 0x0 }; //Look for 
    char test15[12] = { 0x6C, 0x6F, 0x6F, 0x6B, 0x20, 0x61, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x0 }; //Look around

    if (b12FindSynonymNumSearch(test1) != 178)
    {
        printf("fail test 1\n");
    }
    if (b12FindSynonymNumSearch(test2) != 226)
    {
        printf("fail test 2\n");
    }
    if (b12FindSynonymNumSearch(test3) != -1)
    {
        printf("fail test 3\n");
    }
    if (b12FindSynonymNumSearch(test4) != 0)
    {
        printf("fail test 4\n");
    }
    if (b12FindSynonymNumSearch(test5) != -1)
    {
        printf("fail test 5\n");
    }
    if (b12FindSynonymNumSearch(test6) != 0)
    {
        printf("fail test 6\n");
    }
    if (b12FindSynonymNumSearch(test7) != 0)
    {
        printf("fail test 7\n");
    }
    if (b12FindSynonymNumSearch(test8) != -1)
    {
        printf("fail test 8\n");
    }
    if (b12FindSynonymNumSearch(test9) != -1)
    {
        printf("fail test 9\n");
    }
    if (b12FindSynonymNumSearch(test10) != -1)
    {
        printf("fail test 10\n");
    }
    if (b12FindSynonymNumSearch(test11) != -1)
    {
        printf("fail test 11\n");
    }
    if (b12FindSynonymNumSearch(test12) != -1)
    {
        printf("fail test 12\n");
    }
    if (b12FindSynonymNumSearch(test13) != 2)
    {
        printf("fail test 13\n");
    }
    if (b12FindSynonymNumSearch(test14) != 2)
    {
        printf("fail test 14\n");
    }
    if (b12FindSynonymNumSearch(test15) != 2)
    {
        printf("fail test 15\n");
    }

    testBuffer = (char*) b10BankedAlloc(strlen(test1) + 1, &testBufferBank);
    strcpyBanked(testBuffer, test1, testBufferBank);

    if (b12FindSynonymNum(testBuffer, testBufferBank) != 178)
    {
        printf("fail test wrapper test\n");
    }

    b10BankedDealloc((byte*)testBuffer, testBufferBank);

    asm("stp");
}
#endif
/**************************************************************************
** loadWords
**
** Purpose: Load words in from the WORDS.TOK file.
**************************************************************************/
static void b12FreeWordAllocations(char *data, byte dataBank,
                                   char **pointers, byte pointersBank,
                                   word *synonyms, byte synonymsBank)
{
    if (data)
        b10BankedDealloc((byte *)data, dataBank);
    if (pointers)
        b10BankedDealloc((byte *)pointers, pointersBank);
    if (synonyms)
        b10BankedDealloc((byte *)synonyms, synonymsBank);
}

boolean b12LoadWords()
{
    WordsLoadInfo counts;
    WordsLoadInfo stagedCounts;
    char *stagedWords = NULL;
    char **stagedPointers = NULL;
    word *stagedSynonyms = NULL;
    char *oldWords;
    char **oldPointers;
    word *oldSynonyms;
    char *stagedWordPointer;
    byte stagedWordsBank = 0;
    byte stagedPointersBank = 0;
    byte stagedSynonymsBank = 0;
    byte oldWordsBank;
    byte oldPointersBank;
    byte oldSynonymsBank;
    byte lfn;
    char firstThree[3];
    int pointerBytes;
    int synonymBytes;
    int i;
    int j;

    printf(B12_LOADING_WORDS);

    lfn = b12OpenWords();
    if (!lfn)
        return FALSE;
    if (!b0FReadWordsPass(NULL, 0, NULL, 0, NULL, 0, NULL, &counts)) {
        cbm_close(lfn);
        return FALSE;
    }
    cbm_close(lfn);

    pointerBytes = counts.wordCount * (int)sizeof(char *);
    synonymBytes = counts.wordCount * (int)sizeof(word);
    stagedWords = (char *)b10BankedAlloc(counts.dataSize ? counts.dataSize : 1,
                                         &stagedWordsBank);
    if (!stagedWords)
        goto failed;
    stagedPointers = (char **)b10BankedAlloc(pointerBytes ? pointerBytes : 1,
                                              &stagedPointersBank);
    if (!stagedPointers)
        goto failed;
    stagedSynonyms = (word *)b10BankedAlloc(synonymBytes ? synonymBytes : 1,
                                             &stagedSynonymsBank);
    if (!stagedSynonyms)
        goto failed;

    lfn = b12OpenWords();
    if (!lfn)
        goto failed;
    if (!b0FReadWordsPass(stagedWords, stagedWordsBank,
                          stagedPointers, stagedPointersBank,
                          stagedSynonyms, stagedSynonymsBank,
                          &counts, &stagedCounts)) {
        cbm_close(lfn);
        goto failed;
    }
    cbm_close(lfn);

    /* All file reads and allocations have succeeded; publish the staged copy. */
    memsetBanked(wordsTextStart, 0, WORDS_TEXT_START_SIZE, WORD_BANK);
    for (i = 0; i < stagedCounts.wordCount; ++i) {
        memset(firstThree, 0, sizeof(firstThree));
        memCpyBanked((byte *)&stagedWordPointer,
                     (byte *)(stagedPointers + i), stagedPointersBank, sizeof(char *));
        for (j = 0; j < (int)sizeof(firstThree); ++j) {
            memCpyBanked((byte *)&firstThree[j], (byte *)stagedWordPointer + j,
                         stagedWordsBank, 1);
            if (!firstThree[j])
                break;
        }
        memCpyBanked((byte *)&wordsTextStart[i * 3], (byte *)firstThree,
                     WORD_BANK, sizeof(firstThree));
    }

    oldWords = wordsData;
    oldPointers = wordPointers;
    oldSynonyms = synonymsList;
    oldWordsBank = wordsDataBank;
    oldPointersBank = wordsPointersBank;
    oldSynonymsBank = synonymsListBank;
    wordsData = stagedWords;
    wordPointers = stagedPointers;
    synonymsList = stagedSynonyms;
    wordsDataBank = stagedWordsBank;
    wordsPointersBank = stagedPointersBank;
    synonymsListBank = stagedSynonymsBank;
    numWords = stagedCounts.wordCount;
    numSynonyms = stagedCounts.synonymCount;

    b12FreeWordAllocations(oldWords, oldWordsBank, oldPointers, oldPointersBank,
                           oldSynonyms, oldSynonymsBank);

#ifdef TEST_SYN
    b12TestSyn();
#endif
    return TRUE;

failed:
    b12FreeWordAllocations(stagedWords, stagedWordsBank,
                           stagedPointers, stagedPointersBank,
                           stagedSynonyms, stagedSynonymsBank);
    return FALSE;
}

/***************************************************************************
** discardWords
**
** Purpose: To deallocate all memory associated with the words array.
***************************************************************************/
void b12DiscardWords()
{
    b12FreeWordAllocations(wordsData, wordsDataBank,
                           wordPointers, wordsPointersBank,
                           synonymsList, synonymsListBank);
    wordsData = NULL;
    wordsDataBank = 0;
    wordPointers = NULL;
    wordsPointersBank = 0;
    synonymsList = NULL;
    synonymsListBank = 0;
    numWords = 0;
    numSynonyms = 0;
    memsetBanked(wordsTextStart, 0, WORDS_TEXT_START_SIZE, WORD_BANK);
}

#define USER_WORD_BUFFER ((char*) GOLDEN_RAM_WORK_AREA)
#define WORD_TO_COMPARE_BUFFER GOLDEN_RAM_WORK_AREA + MAX_WORD_SIZE

signed char b12CompareWithWordNumber(int wordNum, char* toCompare, int* synNum) //Performs a comparison between a word at a word number and a char string. This is called by _b12FindSynonymNumSearch in assembly which after completing its binary search on the first three letters calls this to know if it has found the right match. Returns the synNum through output param
{
    char* wordPointer;
    char* wordToCompareBuffer = (char*) WORD_TO_COMPARE_BUFFER;
    int compareResult;

#ifdef VERBOSE_COMPARE_WITH_WORD_NUMBER
    printf("copying pointer to %p from %p on bank %p\n", (byte*)&wordPointer,(byte*)wordPointers + wordNum * 2, wordsPointersBank);
#endif // DEBUG
    
    memCpyBanked((byte*)&wordPointer,wordPointers + wordNum, wordsPointersBank, sizeof(char*));

#ifdef VERBOSE_COMPARE_WITH_WORD_NUMBER
    printf("copying word to %p from %p on bank %p\n", wordToCompareBuffer, wordPointer, wordsDataBank);
#endif // DEBUG
    
    strcpyBanked(wordToCompareBuffer, wordPointer, wordsDataBank);
  
#ifdef VERBOSE_COMPARE_WITH_WORD_NUMBER
    printf("copying syn to %p from %p on bank %p\n", synNum, synonymsList, synonymsListBank);
#endif // DEBUG
    memCpyBanked(synNum, synonymsList + wordNum, synonymsListBank, sizeof(int));
#ifdef VERBOSE_COMPARE_WITH_WORD_NUMBER
    printf("the syn is %d\n", *synNum);
#endif
#ifdef VERBOSE_COMPARE_WITH_WORD_NUMBER
    printf("comparing %s with %s and the result is %d\n", toCompare, wordToCompareBuffer, strcmpIgnoreSpace(toCompare, wordToCompareBuffer));
    printf("comparing %p with %p and the result is %d\n", toCompare, wordToCompareBuffer, strcmpIgnoreSpace(toCompare, wordToCompareBuffer));
#endif
    compareResult = strcmpIgnoreSpace(toCompare, wordToCompareBuffer);

    if (compareResult)
    {
        if (compareResult > 0)
        {
            return 1;
        }
        return - 1;
    }
    return 0;
}

/***************************************************************************
** findSynonymNum
**
** Purpose: Returns the synonym number for the given word if it is
** contained in WORDS.TOK, otherwise it returns -1. This function performs
** a binary search to locate the correct word entry. Some games would have
** a search depth of about 10 or 11 (1000+ words).
***************************************************************************/
int b12FindSynonymNum(char* userWord, byte userWordBank)
{
    memCpyBanked((byte*)USER_WORD_BUFFER, (byte*)userWord, userWordBank, MAX_WORD_SIZE);
    
   return b12FindSynonymNumSearch(USER_WORD_BUFFER);
}

#pragma code-name (pop)
