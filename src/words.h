/***************************************************************************
** words.h
***************************************************************************/

#ifndef _WORDS_H_
#define _WORDS_H_

#include "helpers.h"
#include "agifiles.h"
#include "memoryManager.h"
#include <cx16.h>
#include <cbm.h>
#define MAX_WORD_SIZE 20
extern int numWords;
extern int numSynonyms;

#pragma wrapped-call (push, trampoline, WORD_BANK)
extern void b12LoadWords();
extern void b12DiscardWords();
#pragma wrapped-call (pop)
int b12FindSynonymNum(char* userWord); //Leaving out of the tramp as it is on the same bank


#endif /* _WORDS_H_ */