#ifndef _SOUND_H_
#define _SOUND_H_

#include "general.h"
#include "memoryManager.h"
#include "agifiles.h"
#include "irq.h"

#define MAX_SOUNDS 256
#define MAX_LOADED_SOUNDS 15

typedef struct {
	byte* ch0;
	byte* ch1;
	byte* ch2;
	byte* chNoise;
	byte soundBank;
	byte* soundResource; 
} SoundFile;

#pragma wrapped-call (push, trampoline, SOUND_BANK)
void b1DiscardSoundFile(int soundNum);
void b1InitSound();
void b1LoadSoundFile(int soundNum);
void b1PlaySound(byte soundNum, byte endSoundFlag);
void b1StopSound();
#pragma wrapped-call (pop)

extern SoundFile b1LoadedSounds[];
extern int soundEndFlag;
extern boolean checkForEnd;

#endif
