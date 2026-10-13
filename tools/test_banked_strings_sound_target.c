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
unsigned int testSoundBoundsResource;
unsigned int testSoundBoundsResourceEnd;
unsigned int testSoundBoundsAllocationEnd;
unsigned int testSoundBoundsChannel2Start;
unsigned int testSoundBoundsChannel2End;
unsigned int testSoundBoundsNoiseStart;
unsigned int testSoundBoundsNoiseEnd;
byte testSoundBoundsBank;
unsigned int testSoundBoundsOutputResource;
unsigned int testSoundBoundsOutputNoise;
byte testSoundBoundsOutputBank;
byte testSoundBoundsOutputReleased;

#define TEST_SOUND_LONG_NOISE_NOTES 150
#define TEST_SOUND_LONG_RESOURCE_SIZE (24 + TEST_SOUND_LONG_NOISE_NOTES * 5 + 2)

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

void testPrepareSoundBounds(void)
{
	byte previousBank = RAM_BANK;
	byte sourceBank;
	byte *sourceResource;
	unsigned int soundChannelOffsets[4];

	soundChannelOffsets[0] = 8;
	soundChannelOffsets[1] = 10;
	soundChannelOffsets[2] = 12;
	soundChannelOffsets[3] = 24;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	sourceResource = b10BankedAlloc(sizeof(testSoundConversionRawResource), &sourceBank);
	RAM_BANK = sourceBank;
	memcpy(sourceResource, testSoundConversionRawResource, sizeof(testSoundConversionRawResource));

	RAM_BANK = SOUND_BANK;
	memset(&bBLoadedSounds[0], 0, sizeof(SoundFile));
	soundLoadCounter = 0;
	bBLoadedSounds[0].soundBank = sourceBank;
	bBLoadedSounds[0].soundResource = sourceResource;
	totalSoundSize = sizeof(testSoundConversionRawResource);
	bBSetChannelOffsets(sourceResource, &bBLoadedSounds[0], soundChannelOffsets);

	testSoundBoundsResource = (unsigned int)sourceResource;
	testSoundBoundsResourceEnd = testSoundBoundsResource + sizeof(testSoundConversionRawResource);
	testSoundBoundsAllocationEnd = testSoundBoundsResource + TINY_SIZE;
	testSoundBoundsChannel2Start = testSoundBoundsResource + soundChannelOffsets[2];
	testSoundBoundsChannel2End = testSoundBoundsResource + soundChannelOffsets[3];
	testSoundBoundsNoiseStart = testSoundBoundsChannel2End;
	testSoundBoundsNoiseEnd = testSoundBoundsResourceEnd;
	testSoundBoundsBank = sourceBank;
	testSoundBoundsOutputResource = 0;
	testSoundBoundsOutputNoise = 0;
	testSoundBoundsOutputBank = 0;
	testSoundBoundsOutputReleased = FALSE;
	RAM_BANK = previousBank;
}

void testPrepareSoundRefill(void)
{
	byte previousBank = RAM_BANK;
	byte sourceBank;
	byte *sourceResource;
	unsigned int soundChannelOffsets[4];
	unsigned int i;

	soundChannelOffsets[0] = 8;
	soundChannelOffsets[1] = 10;
	soundChannelOffsets[2] = 12;
	soundChannelOffsets[3] = 24;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	sourceResource = b10BankedAlloc(TEST_SOUND_LONG_RESOURCE_SIZE, &sourceBank);
	RAM_BANK = sourceBank;
	memset(sourceResource, 0, TEST_SOUND_LONG_RESOURCE_SIZE);
	sourceResource[0] = 8;
	sourceResource[1] = 0;
	sourceResource[2] = 10;
	sourceResource[3] = 0;
	sourceResource[4] = 12;
	sourceResource[5] = 0;
	sourceResource[6] = 24;
	sourceResource[7] = 0;
	sourceResource[8] = 0xFF;
	sourceResource[9] = 0xFF;
	sourceResource[10] = 0xFF;
	sourceResource[11] = 0xFF;
	sourceResource[12] = 0xE8;
	sourceResource[13] = 0x03;
	sourceResource[14] = 0x1F;
	sourceResource[15] = 0x04;
	sourceResource[16] = 0x00;
	sourceResource[17] = 0xFF;
	sourceResource[18] = 0xFF;
	for (i = 0; i < TEST_SOUND_LONG_NOISE_NOTES; ++i)
	{
		byte *note = sourceResource + 24 + i * 5;
		note[0] = (byte)(i == 50 ? 0 : 1);
		note[1] = (byte)(i == 50 ? 1 : 0);
		note[2] = 0;
		note[3] = (byte)(i == TEST_SOUND_LONG_NOISE_NOTES - 1 ? 3 : 4);
		note[4] = 12;
	}
	sourceResource[TEST_SOUND_LONG_RESOURCE_SIZE - 2] = 0xFF;
	sourceResource[TEST_SOUND_LONG_RESOURCE_SIZE - 1] = 0xFF;

	RAM_BANK = SOUND_BANK;
	memset(&bBLoadedSounds[0], 0, sizeof(SoundFile));
	soundLoadCounter = 0;
	bBLoadedSounds[0].soundBank = sourceBank;
	bBLoadedSounds[0].soundResource = sourceResource;
	totalSoundSize = TEST_SOUND_LONG_RESOURCE_SIZE;
	bBSetChannelOffsets(sourceResource, &bBLoadedSounds[0], soundChannelOffsets);

	testSoundBoundsResource = (unsigned int)sourceResource;
	testSoundBoundsResourceEnd = testSoundBoundsResource + TEST_SOUND_LONG_RESOURCE_SIZE;
	testSoundBoundsAllocationEnd = testSoundBoundsResource + SMALL_SIZE;
	testSoundBoundsChannel2Start = testSoundBoundsResource + soundChannelOffsets[2];
	testSoundBoundsChannel2End = testSoundBoundsResource + soundChannelOffsets[3];
	testSoundBoundsNoiseStart = testSoundBoundsChannel2End;
	testSoundBoundsNoiseEnd = testSoundBoundsResourceEnd;
	testSoundBoundsBank = sourceBank;
	testSoundBoundsOutputResource = 0;
	testSoundBoundsOutputNoise = 0;
	testSoundBoundsOutputBank = 0;
	testSoundBoundsOutputReleased = FALSE;
	RAM_BANK = previousBank;
}

void testRunSoundBounds(void)
{
	byte previousBank = RAM_BANK;
	unsigned int soundChannelOffsets[4];

	soundChannelOffsets[0] = 8;
	soundChannelOffsets[1] = 10;
	soundChannelOffsets[2] = 12;
	soundChannelOffsets[3] = 24;
	RAM_BANK = SOUND_BANK;
	soundLoadCounter = 0;
	bBPrecomputeValues(&bBLoadedSounds[0], soundChannelOffsets);
	testSoundBoundsOutputResource = (unsigned int)bBLoadedSounds[0].soundResource;
	testSoundBoundsOutputNoise = (unsigned int)bBLoadedSounds[0].chNoise;
	testSoundBoundsOutputBank = bBLoadedSounds[0].soundBank;
	RAM_BANK = previousBank;
}

void testCleanupSoundBounds(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	testSoundBoundsOutputReleased = b10BankedDealloc(
		(byte *)testSoundBoundsOutputResource, testSoundBoundsOutputBank);
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
