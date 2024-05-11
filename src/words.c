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


#pragma bss-name (push, "BANKRAM12")
#define WORDS_METADATA_SIZE 1500
wordType wordsMetadata[WORDS_METADATA_SIZE];
int numWords, numSynonyms;  /* Big difference between the two */
byte wordBank;
char* wordsData;
byte wordsDataBank;
char** wordPointers; //This will be used to store pointers to all of the words, in the same order as the words metadata
byte wordsPointersBank;

#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM07")
#pragma wrapped-call (push, trampoline, 7)

//Putting this here saves two hundred bytes on bank 12
wordType* b7SwapWordTypeWithWordPointers(byte* tempWordTypeBank) //WordType is stored in a temp place on the bank, and wordPointers is stored in its place
{
    byte wordsMetadataSize = WORDS_METADATA_SIZE * sizeof(wordType);
    wordType* tempWordStore;
    char** tempWordsPointersStore = (char**)&wordsMetadata[0];
    int wordPointersSize;
    byte* localWordPointers;
    byte localWordPointersBank;
    int localNumWords;

    memCpyBanked((byte*) & localWordPointers, (byte*)&wordPointers, WORD_BANK, sizeof(char**));
    memCpyBanked(&localWordPointersBank, (byte*)&wordsPointersBank, WORD_BANK, sizeof(byte));
    memCpyBanked(&localNumWords, (byte*)&numWords, WORD_BANK, sizeof(int));

    wordPointersSize = localNumWords * sizeof(char*);

    tempWordStore = (wordType*)b10BankedAlloc(wordsMetadataSize, tempWordTypeBank);
    memCpyBankedBetween((byte*)tempWordStore, *tempWordTypeBank, &wordsMetadata[0], WORD_BANK, wordsMetadataSize);

    memCpyBankedBetween((byte*)tempWordsPointersStore, WORD_BANK, (byte*)localWordPointers, localWordPointersBank, wordPointersSize);

    return tempWordStore;
}

void b7RecoverWordType(wordType* tempWordType, byte tempWordTypeBank)
{
    byte wordsMetadataSize = WORDS_METADATA_SIZE * sizeof(wordType);
    wordType* tempWordStore;
    char** tempWordsPointersStore = (char**)&wordsMetadata[0];
    int wordPointersSize;
    byte* localWordPointers;
    byte localWordPointersBank;
    int localNumWords;

    memCpyBanked((byte*)&localWordPointers, (byte*)&wordPointers, WORD_BANK, sizeof(char**));
    memCpyBanked(&localWordPointersBank, (byte*)&wordsPointersBank, WORD_BANK, sizeof(byte));
    memCpyBanked(&localNumWords, (byte*)&numWords, WORD_BANK, sizeof(int));

    wordPointersSize = localNumWords * sizeof(char*);

    memCpyBankedBetween((byte*)localWordPointers, localWordPointersBank, (byte*)tempWordsPointersStore, WORD_BANK, wordPointersSize);
    
    memCpyBankedBetween(&wordsMetadata[0], WORD_BANK, (byte*)tempWordType, tempWordTypeBank, wordsMetadataSize);

    b10BankedDealloc((byte*)tempWordType, tempWordTypeBank);
}

#pragma wrapped-call (pop)
#pragma code-name (pop)

#pragma code-name (push, "BANKRAM12")
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
    wordType* tempWordType;
    byte tempWordTypeBank;
    char** tempWordsPointersStore = (char**) & wordsMetadata[0];
    int i;
    
#ifdef VERBOSE_UPDATE_WORD_POINTERS
    printf("the difference is %p - %p = %p\n", oldWordsAddress, newWordsAddress, oldWordsAddress - newWordsAddress);
#endif

    tempWordType = b7SwapWordTypeWithWordPointers(&tempWordTypeBank);

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

    b7RecoverWordType(tempWordType, tempWordTypeBank);
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
            printf("the address of wordTypes is %p and the address of wordPointers is %p on bank %p and the the address of word data is %p on bank %p\n", &wordsMetadata[0], wordPointers, wordsPointersBank, newWordsData, newWordsDataBank);
#endif // VERBOSE_UPDATE_WORD_POINTERS
            b12UpdateWordPointersAfterCompress(wordsData, newWordsData);

            printf("new words segment %p on bank %p wordpointers %d on bank %d, wordTypeAddress %p\n", newWordsData, newWordsDataBank, wordPointers, wordsPointersBank, &wordsMetadata[0]);

            b10BankedDealloc((byte*)wordsData, wordsDataBank);

            wordsData = newWordsData;
            wordsDataBank = newWordsDataBank;

#ifdef VERBOSE_COMPRESS
            printf("compressed down to size %d address %p bank %p\n", secondBiggestSegment, wordsData, wordsDataBank);
#endif
        }
}

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
    word wordNum, synNum;
    char* wordsDataPointer;
    char* newWord = (char*) NEW_WORD_ADDRESS;
    char** wordPointersPointer;
    byte wordLength, lfn;

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

        synNum = (byte)cbm_read(SEQUENTIAL_LFN, &wordPos, 1) * 256 + (byte)cbm_read(SEQUENTIAL_LFN, &wordPos, 1);
        
        wordLength = strlen(newWord) + 1;


//#ifdef VERBOSE_WORDS
//        printf("copying from %p to %p on bank %p length %p\n", &newWord);
//#endif
        memCpyBanked((byte*)wordsDataPointer, (byte*)newWord, wordsDataBank, wordLength);
        
        wordsMetadata[wordNum].wordTextStart[0] = newWord[0];
        wordsMetadata[wordNum].wordTextStart[1] = newWord[1];

        wordsMetadata[wordNum].synonymNum = synNum;

        memCpyBanked((byte*)wordPointersPointer, (byte*)&wordsDataPointer, wordsPointersBank, 2);

        wordsDataPointer += wordLength;
        wordPointersPointer++;

        if (wordsDataPointer > (char*)BANK_MAX)
        {
            printf("Words overflow");
        }
    }

    b12CompressWordsAllocation(wordsDataPointer - wordsData - 1); //Words data pointer is pointing to a blank byte read for another word. Therefore -1 from the length

#ifdef VERBOSE_WORDS
    printf("end of function reached\n");
#endif
    cbm_close(SEQUENTIAL_LFN);
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

/***************************************************************************
** findSynonymNum
**
** Purpose: Returns the synonym number for the given word if it is
** contained in WORDS.TOK, otherwise it returns -1. This function performs
** a binary search to locate the correct word entry. Some games would have
** a search depth of about 10 or 11 (1000+ words).
***************************************************************************/
int b12FindSynonymNum(char* userWord)
{
    boolean found = FALSE;
    int top = numWords - 1, bottom = 0, mid, strCompVal;

    while ((!found) && (bottom <= top)) {
        mid = (top + bottom) / 2;
        strCompVal = strcmp(userWord, wordsMetadata[mid].wordTextStart);
        if (strCompVal == 0)
            found = TRUE;
        else if (strCompVal < 0)
            top = mid - 1;
        else
            bottom = mid + 1;
    }

    if (found) return (wordsMetadata[mid].synonymNum);
    else return (-1);
}

#pragma code-name (pop)
