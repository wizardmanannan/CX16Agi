/* Test caller for the production parser input path. */
#include "parser.h"
#include <cx16.h>

char *testInputPrompt;
byte testInputPromptBank;
char *testInputDestination;
byte testInputDestinationBank;
int testInputLimit;

void testRunGetString(void)
{
	b7GetString(testInputPrompt, testInputPromptBank, testInputDestination,
		testInputDestinationBank, 1, 23, testInputLimit);
}

void testRunGetInternalString(void)
{
	b7GetInternalString(testInputPrompt, testInputPromptBank, 0, 1, 23,
		testInputLimit);
}
