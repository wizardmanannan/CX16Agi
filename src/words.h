/***************************************************************************
** words.h
***************************************************************************/

#ifndef _WORDS_H_
#define _WORDS_H_

#include "helpers.h"

typedef struct {
	char* wordText;
	int synonymNum;
} wordType;

extern wordType* words;
extern int numWords;
extern int numSynonyms;

#pragma wrapped-call (push, trampoline, WORD_BANK)
extern void b1LoadWords();
extern void b1DiscardWords();
#pragma wrapped-call (pop)


#endif /* _WORDS_H_ */