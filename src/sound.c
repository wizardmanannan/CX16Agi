#include "sound.h"

#define NO_CHANNELS 4

int soundEndFlag;

extern int soundEndFlag;

#pragma bss-name (push, "BANKRAM08")
SoundFile b8LoadedSounds[MAX_LOADED_SOUNDS];
SoundFile* b8LoadedSoundsPointer[MAX_SOUNDS];
byte soundLoadCounter;
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM08")

void b8ClearLoadedSound(byte soundFileNumber)
{
	b8LoadedSounds[soundFileNumber].ch1 = NULL;
	b8LoadedSounds[soundFileNumber].ch2 = NULL;
	b8LoadedSounds[soundFileNumber].ch3 = NULL;
	b8LoadedSounds[soundFileNumber].chNoise = NULL;
	b8LoadedSounds[soundFileNumber].soundBank = 0;
	b8LoadedSounds[soundFileNumber].soundResource = NULL;
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
	SoundFile* sound;
	byte* soundResource; 
	byte loadedSoundNum;

	sound = b8LoadedSoundsPointer[soundNum];
	
	loadedSoundNum = sound - &b8LoadedSounds[0];

	soundResource = sound->soundResource;
	if (sound && soundResource != NULL)
	{
		b10BankedDealloc(soundResource, sound->soundBank);
		b8ClearLoadedSound(loadedSoundNum);
		b8LoadedSoundsPointer[soundNum] = NULL;
	}

	for (loadedSoundNum = soundLoadCounter - 1; loadedSoundNum != 0xFF && b8LoadedSounds[loadedSoundNum].soundResource == NULL; loadedSoundNum--)
	{
		soundLoadCounter--;
	}
}

void b8LoadSoundFile(int soundNum) {

	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte i;
	byte** currentChannel;
	unsigned int soundChannelOffSets[NO_CHANNELS];

	getLogicDirectory(&agiFilePosType, &snddir[soundNum]);
	b6LoadAGIFile(SOUND, &agiFilePosType, &tempAGI);

	b8LoadedSounds[soundLoadCounter].soundResource = tempAGI.code;
	b8LoadedSounds[soundLoadCounter].soundBank = tempAGI.codeBank;
	b8LoadedSoundsPointer[soundNum] = &b8LoadedSounds[soundLoadCounter];

	memCpyBanked((byte*)soundChannelOffSets, tempAGI.code, tempAGI.codeBank, NO_CHANNELS * 2);

	for (i = 0, currentChannel = &b8LoadedSounds[soundLoadCounter].ch1; i < NO_CHANNELS; i++, currentChannel++)
	{
		*currentChannel = tempAGI.code + soundChannelOffSets[i];
	}

	if (soundLoadCounter < MAX_LOADED_SOUNDS - 1)
	{
		soundLoadCounter++;
	}
}

#pragma code-name (pop)
