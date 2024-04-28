/***************************************************************************
** words.h
***************************************************************************/

#ifndef _WORDS_H_
#define _WORDS_H_

#include "helpers.h"

typedef struct {
	char* b7WordText;
	int synonymNum;
} wordType;

extern wordType* words;
extern int numWords;
extern int numSynonyms;

#pragma wrapped-call (push, trampoline, WORD_BANK)
extern void b7LoadWords();
extern void b7DiscardWords();
#pragma wrapped-call (pop)
int b7FindSynonymNum(char* userWord); //Leaving out of the tramp as it is on the same bank


#endif /* _WORDS_H_ */