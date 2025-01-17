#include "random.h"
#pragma bss-name (push, "BANKRAM06")
byte b6RandomNumbers[NO_RANDOM_NUMBERS];
#pragma bss-name (pop)

void b6InitRandom()
{
	int i;
	for (i = 0; i < NO_RANDOM_NUMBERS; i++)
	{
		b6RandomNumbers[i] = rand() % NO_RANDOM_NUMBERS;
	}
}