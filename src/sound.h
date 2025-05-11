#ifndef _SOUND_H_
#define _SOUND_H_

#include "general.h"
#include "memoryManager.h"

#define MAX_SOUNDS 256
#define MAX_LOADED_SOUNDS 30

typedef struct {
	unsigned int duration;
	unsigned int frequency;
	byte attenuation;
} SoundNote;



typedef struct {
	SoundNote* ch1;
	SoundNote* ch2;
	SoundNote* ch3;
	SoundNote* chNoise;
	byte soundBank;
} SoundFile;

#pragma wrapped-call (push, trampoline, SOUND_BANK)
void b8DiscardSoundFile(int soundNum);
void b8InitSound();
void b8LoadSoundFile(int soundNum);
#pragma wrapped-call (pop)

extern SoundFile b8LoadedSounds[];
extern int soundEndFlag;
extern boolean checkForEnd;

#endif
