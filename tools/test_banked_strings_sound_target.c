/* Test callers for production sound conversion and compiled playback. */
#include "helpers.h"
#include "logic.h"
#include "memoryManager.h"
#include "sound.h"
#include <cx16.h>

extern unsigned long totalSoundSize;
extern byte soundLoadCounter;
extern byte allocationArray[];
extern SoundFile *bBLoadedSoundsPointer[MAX_SOUNDS];

#pragma wrapped-call (push, trampoline, SOUND_BANK)
extern void bBSetChannelOffsets(byte *codePtr, SoundFile *soundFile, unsigned int *soundChannelOffSets);
extern void bBPrecomputeValues(SoundFile *soundFile, unsigned int *soundChannelOffSets);
#pragma wrapped-call (pop)

byte testSoundConversionNoiseBytes[2][12];
byte testSoundConversionBanks[2];
byte testSoundConversionSourceBanks[2];
unsigned int testSoundConversionResources[2];
unsigned int testSoundConversionNoiseAddresses[2];
byte testSoundConversionSourcesReleased[2];
byte testSoundConversionOutputsReleased[2];
static const byte testSoundConversionRawResource[36] = {
	8, 0, 10, 0, 12, 0, 24, 0,
	0xFF, 0xFF, 0xFF, 0xFF,
	1, 0, 0x3E, 0x08, 0,
	100, 0, 0x1F, 0x04, 0,
	0xFF, 0xFF,
	2, 0, 0, 4, 12,
	100, 0, 0, 3, 12,
	0xFF, 0xFF
};

static void testBuildSoundConversion(byte rawVolume, byte resultIndex)
{
	byte previousBank = RAM_BANK;
	byte sourceBank;
	byte outputBank;
	byte *sourceResource, *outputResource, *outputNoise;
	unsigned int soundChannelOffsets[4];

	soundChannelOffsets[0] = 8;
	soundChannelOffsets[1] = 10;
	soundChannelOffsets[2] = 12;
	soundChannelOffsets[3] = 24;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	sourceResource = b10BankedAlloc(sizeof(testSoundConversionRawResource), &sourceBank);
	RAM_BANK = sourceBank;
	memcpy(sourceResource, testSoundConversionRawResource, sizeof(testSoundConversionRawResource));
	sourceResource[28] = rawVolume;
	sourceResource[33] = rawVolume;

	RAM_BANK = SOUND_BANK;
	memset(&bBLoadedSounds[resultIndex], 0, sizeof(SoundFile));
	soundLoadCounter = resultIndex;
	bBLoadedSounds[resultIndex].soundBank = sourceBank;
	bBLoadedSounds[resultIndex].soundResource = sourceResource;
	totalSoundSize = sizeof(testSoundConversionRawResource);
	bBSetChannelOffsets(sourceResource, &bBLoadedSounds[resultIndex], soundChannelOffsets);
	bBPrecomputeValues(&bBLoadedSounds[resultIndex], soundChannelOffsets);

	RAM_BANK = SOUND_BANK;
	outputResource = bBLoadedSounds[resultIndex].soundResource;
	outputBank = bBLoadedSounds[resultIndex].soundBank;
	outputNoise = bBLoadedSounds[resultIndex].chNoise;
	testSoundConversionBanks[resultIndex] = outputBank;
	testSoundConversionSourceBanks[resultIndex] = sourceBank;
	testSoundConversionResources[resultIndex] = (unsigned int)outputResource;
	testSoundConversionNoiseAddresses[resultIndex] = (unsigned int)outputNoise;
	RAM_BANK = outputBank;
	memcpy(testSoundConversionNoiseBytes[resultIndex], outputNoise,
		sizeof(testSoundConversionNoiseBytes[resultIndex]));

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	testSoundConversionSourcesReleased[resultIndex] =
		(unsigned int)sourceResource == 0xA000 && allocationArray[0] == FALSE;
	RAM_BANK = previousBank;
}

void testRunSoundConversion(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	RAM_BANK = SOUND_BANK;
	memset(bBLoadedSoundsPointer, 0, sizeof(bBLoadedSoundsPointer));
	testBuildSoundConversion(12, 0);
	testBuildSoundConversion(0, 1);
	RAM_BANK = previousBank;
}

void testCleanupSoundConversion(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	testSoundConversionOutputsReleased[0] = b10BankedDealloc(
		(byte *)testSoundConversionResources[0], testSoundConversionBanks[0]);
	testSoundConversionOutputsReleased[1] = b10BankedDealloc(
		(byte *)testSoundConversionResources[1], testSoundConversionBanks[1]);
	RAM_BANK = previousBank;
}

static void testRunSoundPlay(byte soundNumber)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = SOUND_BANK;
	bBLoadedSoundsPointer[soundNumber] = &bBLoadedSounds[soundNumber];
	flag[9] = TRUE;
	bBPlaySound(soundNumber, soundNumber + 30);
	RAM_BANK = previousBank;
}

void testRunSoundPlayRaw12(void)
{
	testRunSoundPlay(0);
}

void testRunSoundPlayRaw0(void)
{
	testRunSoundPlay(1);
}
