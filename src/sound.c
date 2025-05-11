#include "sound.h"

int soundEndFlag;

extern int soundEndFlag;

#pragma bss-name (push, "BANKRAM08")
SoundFile b8LoadedSounds[MAX_LOADED_SOUNDS];
SoundFile* b8LoadedSoundsPointer[MAX_SOUNDS];
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM08")

void b8ClearLoadedSound(byte soundFileNumber)
{
	b8LoadedSounds[soundFileNumber].ch1 = NULL;
	b8LoadedSounds[soundFileNumber].ch2 = NULL;
	b8LoadedSounds[soundFileNumber].ch3 = NULL;
	b8LoadedSounds[soundFileNumber].chNoise = NULL;
	b8LoadedSounds[soundFileNumber].soundBank = 0;
}

void b8InitSound() {
	int i;
	for (i = 0; i < MAX_LOADED_SOUNDS; i++)
	{
		b8ClearLoadedSound(i);
	}

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		b8LoadedSoundsPointer[i] = NULL;
	}
}

void b8DiscardSoundFile(int soundNum)
{

}


void b8LoadSoundFile(int soundNum) {
}

#pragma code-name (pop)
