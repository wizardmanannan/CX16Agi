#ifndef _SOUND_H_
#define _SOUND_H_

#include "general.h"
#include "memoryManager.h"
#include "agifiles.h"
#include "irq.h"
#include <limits.h>

#define MAX_SOUNDS 256
#define MAX_LOADED_SOUNDS 10

typedef struct {
	byte* ch0;
	byte* ch1;
	byte* ch2;
	byte* chNoise;
	byte soundBank;
	byte* soundResource; 
} SoundFile;

#pragma wrapped-call (push, trampoline, SOUND_BANK)
void bBDiscardSoundFile(int soundNum);
void bBInitSound();
void bBLoadSoundFile(int soundNum);
void bBPlaySound(byte soundNum, byte endSoundFlag);
void bBStopSound();
#pragma wrapped-call (pop)

extern SoundFile bBLoadedSounds[];
extern int soundEndFlag;
extern boolean checkForEnd;

#endif
