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

#pragma bss-name (push, "BANKRAM12")
#define WORDS_TEXT_START_SIZE 4500
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
char* b12SwapWordsTextStartWithWordPointers(byte* tempWordsTextStartBank) //WordsTextStart is stored in a temp place on the bank, and wordPointers is stored in its place
{
    int wordsTextStartSize = WORDS_TEXT_START_SIZE;
    char* tempWordStore;
    char** tempWordsPointersStore = (char**)&wordsTextStart[0];
    int wordPointersSize = numWords * sizeof(char*);

    tempWordStore = (char*)b10BankedAlloc(wordsTextStartSize, tempWordsTextStartBank);
    memCpyBankedBetween((byte*)tempWordStore, *tempWordsTextStartBank, (byte*)wordsTextStart, WORD_BANK, wordsTextStartSize);

    memCpyBankedBetween((byte*)tempWordsPointersStore, WORD_BANK, (byte*)wordPointers, wordsPointersBank, wordPointersSize);

    return tempWordStore;
}

void b12RecoverWordsTextStart(char* tempWordsTextStart, byte tempWordsTextStartBank)
{
    int wordsTextStartSize = WORDS_TEXT_START_SIZE;
    char* tempWordStore;
    char** tempWordsPointersStore = (char**)&wordsTextStart[0];
    int wordPointersSize = numWords * sizeof(char*);

    memCpyBankedBetween((byte*)wordPointers, wordsPointersBank, (byte*)tempWordsPointersStore, WORD_BANK, wordPointersSize);
    
    memCpyBankedBetween((byte*)wordsTextStart, WORD_BANK, (byte*)tempWordsTextStart, tempWordsTextStartBank, wordsTextStartSize);

    b10BankedDealloc((byte*)tempWordsTextStart, tempWordsTextStartBank);
}

long b12CalculateStartPosition()
{
    byte data[2];

    b6Cx16_fseek(FILE_OPEN_ADDRESS, 0);
    if (!cbm_read(SEQUENTIAL_LFN, &data[0], 2))
    {
        printf("Failed to read from file");
        exit(0);
    }

#ifdef VERBOSE_CALC_WORDS
    printf("the contents of data is %d and %d\n", data[0], data[1]);
#endif

#ifdef VERBOSE_CALC_WORDS
    printf("the start pos calc (%d * 256 + %d)\n", data[0], data[1]);
#endif

#ifdef VERBOSE_CALC_WORDS
    printf("start pos is %lu\n", (byte)data[0] * (long)256 + (byte)data[1]);
#endif

    return (byte)data[0] * (long)256 + (byte)data[1];
}

/**************************************************************************
** calcNumWords
**
** Purpose: Calculate the number of different words (including synonyms)
** and the number of synonyms which are contained in the WORDS.TOK file.
** The number of words will determine how much memory to allocate for the
** words list. The number of synonyms will be used to check against the
** parameters to said() to determine whether those synonyms exist.
**************************************************************************/
int b12CalcNumWords(long startPos)
{
    byte data[2];
    int synMax, synNum, wordCount = 0;

    
    b6Cx16_fseek(FILE_OPEN_ADDRESS, startPos);

    while (cbm_read(SEQUENTIAL_LFN, &data, 1)) {
#ifdef VERBOSE_CALC_WORDS
        printf("data[0] is %p\n", data[0]);
#endif
        if (data[0] > 0x80) {  /* Top bit set marks the end of a word */
            cbm_read(SEQUENTIAL_LFN, &data, 2);
            wordCount++;
            synNum = (byte)data[0] * 256 + (byte)data[1];
            if ((synNum > synMax) && (synNum != 9999)) synMax = synNum;
        }
    }

    numWords = wordCount;
    numSynonyms = synMax;


#ifdef VERBOSE_CALC_WORDS
    printf("there are %d words\n", numWords);
#endif

    return 0;
}

byte b12OpenWords()
{
    byte lfn = b6Cbm_openForSeeking("words.tok");
    if (lfn == NULL) {
        printf("Cannot find file : WORDS.TOK\n");
        exit(1);
    }

    return lfn;
}

void b12UpdateWordPointersAfterCompress(char* oldWordsAddress, char* newWordsAddress)
{
    int difference = newWordsAddress - oldWordsAddress;
    
    //This method will temporarily clobber the words metadata store so we can have the word pointers on this bank to update them.
    //It will store them somewhere else and then put them back
    char* tempWordsTextStart;
    byte tempWordsTextStartBank;
    char** tempWordsPointersStore = (char**) & wordsTextStart[0];
    int i;
    
#ifdef VERBOSE_UPDATE_WORD_POINTERS
    printf("the difference is %p - %p = %p\n", oldWordsAddress, newWordsAddress, oldWordsAddress - newWordsAddress);
#endif

    tempWordsTextStart = b12SwapWordsTextStartWithWordPointers(&tempWordsTextStartBank);

    for (i = 0; i < numWords; i++, tempWordsPointersStore++)
    {
#ifdef VERBOSE_UPDATE_WORD_POINTERS
        printf("we will update %p by %d\n", tempWordsPointersStore, difference);
#endif
        *tempWordsPointersStore += difference;

#ifdef VERBOSE_UPDATE_WORD_POINTERS
        printf("we updated %p by %d\n", tempWordsPointersStore, difference);
#endif
    }

    b12RecoverWordsTextStart(tempWordsTextStart, tempWordsTextStartBank);
}

extern MemoryArea* _memoryAreas;
//Doesn't compress the data but rather moves to the smallest allocation slot practical
void b12CompressWordsAllocation(int wordsLength)
{
    int secondBiggestSegment;
    byte i;
    char* newWordsData;
    byte newWordsDataBank;

    //If the words are small enough to fit into the second biggest segment, then that is our justification to call bankedAlloc and move them to a smaller segment. Note, bankedAlloc will take care of figuring out which segment the words belong in
    memCpyBanked((byte*)&secondBiggestSegment, (byte*)&_memoryAreas[NO_SIZES - 2].segmentSize, MEMORY_MANAGEMENT_BANK, sizeof(int));

    if (wordsLength <= secondBiggestSegment) //Check to 
    {
#ifdef VERBOSE_COMPRESS
        printf("wordsLength %d < %d (%d)\n", wordsLength, secondBiggestSegment, wordsLength <= secondBiggestSegment);
#endif

            newWordsData = b10BankedAlloc(wordsLength, &newWordsDataBank);
            memCpyBankedBetween((byte*)newWordsData, newWordsDataBank, (byte*) wordsData, wordsDataBank, wordsLength);

#ifdef VERBOSE_UPDATE_WORD_POINTERS
            printf("the address of wordTypes is %p and the address of wordPointers is %p on bank %p and the the address of word data is %p on bank %p\n", &wordsTextStart[0], wordPointers, wordsPointersBank, newWordsData, newWordsDataBank);
#endif // VERBOSE_UPDATE_WORD_POINTERS
            b12UpdateWordPointersAfterCompress(wordsData, newWordsData);

            //printf("new words segment %p on bank %p wordpointers %d on bank %d, wordTypeAddress %p\n", newWordsData, newWordsDataBank, wordPointers, wordsPointersBank, &wordsTextStart[0]);

            b10BankedDealloc((byte*)wordsData, wordsDataBank);

            wordsData = newWordsData;
            wordsDataBank = newWordsDataBank;

#ifdef VERBOSE_COMPRESS
            printf("compressed down to size %d address %p bank %p\n", secondBiggestSegment, wordsData, wordsDataBank);
#endif
        }
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
void b12LoadWords()
{
#define NEW_WORD_ADDRESS GOLDEN_RAM_WORK_AREA
#define NEW_WORD_SIZE 80

    byte wordPos;
    byte data[2];
    long startPos;
    word wordNum,synNum;
    char* wordsDataPointer;
    char* newWord = (char*) NEW_WORD_ADDRESS;
    char** wordPointersPointer;
    byte wordLength, lfn;
    word* synonymsListPointer;

    lfn = b12OpenWords();

#ifdef VERBOSE_CALC_WORDS
    printf("the lfn is %d\n", lfn);
#endif // VERBOSE_WORDS

    startPos = b12CalculateStartPosition();

    b12CalcNumWords(startPos);
    wordsData = b10BankedAlloc(LARGE_SIZE, data); //This allocation will be swapped by a smaller one if practical in b12CompressAllocation
    wordsDataBank = data[0];

    wordPointers = (char**)b10BankedAlloc(numWords * sizeof(char*), data);

    wordsPointersBank = data[0];
    wordPointersPointer = wordPointers;

    synonymsList = (word*) b10BankedAlloc(numWords * sizeof(int), data);
    synonymsListBank = data[0];
    synonymsListPointer = synonymsList;

#ifdef VERBOSE_WORDS
    printf("the word pointers are at %p on bank %p\n", wordPointersPointer, wordsPointersBank);
    printf("the words are stored at %p on bank %p\n", wordsData, wordsDataBank);
#endif

    wordsDataPointer = wordsData;

    cbm_close(SEQUENTIAL_LFN);

    lfn = b12OpenWords();
    b6Cx16_fseek(FILE_OPEN_ADDRESS, startPos);

#ifdef VERBOSE_WORDS
    printf("the start pos is %lu\n", startPos);
#endif

    for (wordNum = 0; wordNum < numWords; wordNum++) {
        cbm_read(SEQUENTIAL_LFN, &wordPos, 1);
        do {
            cbm_read(SEQUENTIAL_LFN, &data[0], 1);
            newWord[wordPos++] = ((data[0] ^ 0x7F) & 0x7F);

#ifdef VERBOSE_LETTERS
            printf("we read %d\n", ((data[0] ^ 0x7F) & 0x7F));
#endif

        } while (data[0] < 0x80);
        newWord[wordPos] = NULL;

#ifdef VERBOSE_WORDS
        printf("word: %s\n", newWord);
#endif

        cbm_read(SEQUENTIAL_LFN, &data[0], 1);
        synNum = (word) data[0] * 256;

        cbm_read(SEQUENTIAL_LFN, &data[0], 1);
        synNum += data[0];

        wordLength = strlen(newWord) + 1;


//#ifdef VERBOSE_WORDS
//        printf("copying from %p to %p on bank %p length %p\n", &newWord);
//#endif
        memCpyBanked((byte*)wordsDataPointer, (byte*)newWord, wordsDataBank, wordLength);
      
        wordsTextStart[wordNum * 3] = newWord[0];
        wordsTextStart[wordNum * 3 + 1] = newWord[1];
        wordsTextStart[wordNum * 3 + 2] = newWord[2];

        memCpyBanked((byte*)wordPointersPointer, (byte*)&wordsDataPointer, wordsPointersBank, 2);

        wordsDataPointer += wordLength;
        wordPointersPointer++;
        
        memCpyBanked((byte*)synonymsListPointer, (byte*)&synNum, synonymsListBank, sizeof(word));
        synonymsListPointer++;

        if (wordsDataPointer > (char*)BANK_MAX)
        {
            printf("Words overflow");
        }
    }

    b12CompressWordsAllocation(wordsDataPointer - wordsData - 1); //Words data pointer is pointing to a blank byte read for another word. Therefore -1 from the length

    //printf("wordsTextStart %p wordsPointers %p on bank %p wordData %p on bank %p\n", wordsTextStart, wordPointers, wordsPointersBank, wordsData, wordsDataBank);

#ifdef VERBOSE_WORDS
    printf("end of function reached\n");
#endif
    cbm_close(SEQUENTIAL_LFN);

#ifdef TEST_SYN
    b12TestSyn();
#endif
}

/***************************************************************************
** discardWords
**
** Purpose: To deallocate all memory associated with the words array.
***************************************************************************/
void b12DiscardWords()
{
    b10BankedDealloc((byte*)wordsData, wordsDataBank);
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
