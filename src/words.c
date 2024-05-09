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

#define VERBOSE_WORDS

#pragma bss-name (push, "BANKRAM12")
#define WORDS_METADATA_SIZE 1500
wordType wordsMetadata[WORDS_METADATA_SIZE];
int numWords, numSynonyms;  /* Big difference between the two */
byte wordBank;
byte* wordsData;
byte wordsDataBank;
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM12")
/**************************************************************************
** calcNumWords
**
** Purpose: Calculate the number of different words (including synonyms)
** and the number of synonyms which are contained in the WORDS.TOK file.
** The number of words will determine how much memory to allocate for the
** words list. The number of synonyms will be used to check against the
** parameters to said() to determine whether those synonyms exist.
**************************************************************************/
int b12CalcNumWords()
{
    byte data[2];
    long startPos;
    int synMax, synNum, wordCount = 0;

    b6Cx16_fseek(FILE_OPEN_ADDRESS, 0);
    if (!cbm_read(SEQUENTIAL_LFN, &data[0], 2))
    {
        printf("Failed to read from file");
        exit(0);
    }

#ifdef VERBOSE_WORDS
    printf("the contents of data is %d and %d\n", data[0], data[1]);
#endif

#ifdef VERBOSE_WORDS
    printf("the start pos calc (%d * 256 + %d)\n", data[0], data[1]);
#endif

    startPos = (byte)data[0] * (long)256 + (byte)data[1];  

#ifdef VERBOSE_WORDS
    printf("start pos is %lu\n", startPos);
#endif

    b6Cx16_fseek(FILE_OPEN_ADDRESS, startPos);





    //data = fgetc(wordFile);
    //while (!feof(wordFile)) {
    //    if (data > 0x80) {  /* Top bit set marks the end of a word */
    //        wordCount++;
    //        synNum = (byte)fgetc(wordFile) * 256 + (byte)fgetc(wordFile);
    //        if ((synNum > synMax) && (synNum != 9999)) synMax = synNum;
    //    }
    //    data = fgetc(wordFile);
    //}

    while (cbm_read(SEQUENTIAL_LFN, &data, 1)) {
#ifdef VERBOSE_WORDS
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


#ifdef VERBOSE_WORDS
    printf("there are %d words\n", numWords);
#endif

    return 0;
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
    byte wordLength, lfn;

    lfn = b6Cbm_openForSeeking("words.tok");
    if (lfn == NULL) {
        printf("Cannot find file : WORDS.TOK\n");
        exit(1);
    }

#ifdef VERBOSE_WORDS
    printf("the lfn is %d\n", lfn);
#endif // VERBOSE_WORDS


    b12CalcNumWords();
    wordsData = b10BankedAlloc(LARGE_SIZE, &wordsDataBank);
    wordsDataPointer = wordsData;

    b6Cx16_fseek(FILE_OPEN_ADDRESS, 0);
    cbm_read(SEQUENTIAL_LFN, &data, 2);
    startPos = (byte)data[0] * (long)256 + (byte)data[1];
    b6Cx16_fseek(FILE_OPEN_ADDRESS, startPos);

    for (wordNum = 0; wordNum < numWords; wordNum++) {
        cbm_read(SEQUENTIAL_LFN, &wordPos, 1);
        do {
            cbm_read(SEQUENTIAL_LFN, &data, 1);
            newWord[wordPos++] = ((data[0] ^ 0x7F) & 0x7F);
        } while (data[0] < 0x80);
        newWord[wordPos] = NULL;
        synNum = (byte)cbm_read(SEQUENTIAL_LFN, &wordPos, 1) * 256 + (byte)cbm_read(SEQUENTIAL_LFN, &wordPos, 1);
        
        wordLength = strlen(newWord);
        memCpyBanked((byte*)wordsDataPointer, (byte*)&newWord, wordsDataBank, wordLength);
        
        wordsMetadata[wordNum].wordText = wordsDataPointer;
        wordsMetadata[wordNum].synonymNum = synNum;

        wordsDataPointer += wordLength;

        if (wordsDataPointer > (char*)BANK_MAX)
        {
            printf("Words overflow");
        }
    }

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
        strCompVal = strcmp(userWord, wordsMetadata[mid].wordText);
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

void b12ShowWords()
{
    int wordNum;

    for (wordNum = 0; wordNum < numWords; wordNum++)
        printf("%-14s%5d ", wordsMetadata[wordNum].wordText, wordsMetadata[wordNum].synonymNum);
}

#pragma code-name (pop)
