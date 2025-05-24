#include "sound.h"

#define NO_CHANNELS 4
#define NO_NOTE_BYTES 5
#define FREQUENCY_BYTE 2
#define FREQUENCY_NUMERATOR 111860

int soundEndFlag;

extern int soundEndFlag;

#pragma bss-name (push, "BANKRAM01")
SoundFile b1LoadedSounds[MAX_LOADED_SOUNDS];
SoundFile* b1LoadedSoundsPointer[MAX_SOUNDS];
byte soundLoadCounter;
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM01")

void b1ClearLoadedSound(byte soundFileNumber)
{
	b1LoadedSounds[soundFileNumber].ch1 = NULL;
	b1LoadedSounds[soundFileNumber].ch2 = NULL;
	b1LoadedSounds[soundFileNumber].ch3 = NULL;
	b1LoadedSounds[soundFileNumber].chNoise = NULL;
	b1LoadedSounds[soundFileNumber].soundBank = 0;
	b1LoadedSounds[soundFileNumber].soundResource = NULL;
}

void b1InitSound() {
	int i;
	for (i = 0; i < MAX_LOADED_SOUNDS; i++)
	{
		b1ClearLoadedSound(i);
	}

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		b1LoadedSoundsPointer[i] = NULL;
	}
}

void b1DiscardSoundFile(int soundNum)
{
	SoundFile* sound;
	byte* soundResource; 
	byte loadedSoundNum;

	sound = b1LoadedSoundsPointer[soundNum];
	
	loadedSoundNum = sound - &b1LoadedSounds[0];

	soundResource = sound->soundResource;
	if (sound && soundResource != NULL)
	{
		b10BankedDealloc(soundResource, sound->soundBank);
		b1ClearLoadedSound(loadedSoundNum);
		b1LoadedSoundsPointer[soundNum] = NULL;
	}

	for (loadedSoundNum = soundLoadCounter - 1; loadedSoundNum != 0xFF && b1LoadedSounds[loadedSoundNum].soundResource == NULL; loadedSoundNum--)
	{
		soundLoadCounter--;
	}
}

byte trap = FALSE;
void b1PrecomputeValues(SoundFile* soundFile)
{
	BufferStatus localBufferStatus;
	byte seenFFFFCounter = 0;
	boolean endByteDetected = FALSE;
	byte noteByteCounter = 0;
	byte readByte, readAheadByte;
	byte* buffer = GOLDEN_RAM_WORK_AREA;
	byte** data = &buffer;
	BufferStatus* bufferStatus;
	unsigned int frequency, adjustedFrequency, bytePerBufferCounter = 0;
	unsigned long frequencyDivisor;
	boolean firstRun = TRUE;

	unsigned int totalCounter = 8;

	bufferStatus = &localBufferStatus;

	localBufferStatus.bank = soundFile->soundBank;
	localBufferStatus.bankedData = soundFile->ch1;
	localBufferStatus.bufferCounter = 0;

	b5RefreshBuffer(&localBufferStatus);

	//printf("the bank is %p and data %p\n", soundFile->soundBank, soundFile->ch1);

	while (seenFFFFCounter != NO_CHANNELS)
	{
		if (trap)
		{
			//asm("stp");
		}
	

		/*if (totalCounter >= 0x222)
		{
			printf("data is %p\n", *data);
			asm("stp");
		}*/
		GET_NEXT(readByte);
		//printf("%p is freq %d will next read %p from golden ram %p, buffer counter %d data: %p bbb counter %d read from %p\n", totalCounter++, noteByteCounter == FREQUENCY_BYTE || noteByteCounter == FREQUENCY_BYTE + 1, readByte, GOLDEN_RAM_WORK_AREA, localBufferStatus.bufferCounter, *data, bytePerBufferCounter, localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE + bytePerBufferCounter);

	/*	if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE)
		{
			bytePerBufferCounter = 0;
		}
		else
		{
			bytePerBufferCounter++;
		}*/

		//printf("notes counter %d bb %d\n", noteByteCounter, bytePerBufferCounter);

		if (!endByteDetected)
		{
			if (readByte == 0xFF)
			{
				//printf("ff detected \n");

				if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
				{
					asm("stp");
					memCpyBanked(&readAheadByte, bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, soundFile->soundBank, 1);
				}
				else
				{
					readAheadByte = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];

					//printf("we see %x , we read ahead to %x, from addr %p, counter %p\n", readByte, readAheadByte, &GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], bytePerBufferCounter);
				}

				if (readAheadByte == 0xFF)
				{
					endByteDetected = TRUE;
				}
			}
			if (!endByteDetected)
			{
				if (noteByteCounter == FREQUENCY_BYTE)
				{

					*((byte*)&frequency) = readByte;
					//printf("the readByte is %p\n", readByte);

					if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
					{
						memCpyBanked((byte*)&frequency + 1, bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, soundFile->soundBank, 1);
					}
					else
					{
						*((byte*)&frequency + 1) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1];
						//printf("the readByte 2 is %p. frequency  address %p\n", GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], &frequency);
					}
					*((byte*)&frequency) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];

					frequencyDivisor = ((*((byte*)&frequency) & 0x3F) << 4) + (*((byte*)&frequency + 1) & 0x0F) & 0xFFFF;
					adjustedFrequency = (FREQUENCY_NUMERATOR / frequencyDivisor) + 1;

					//printf("bbc %d\n", bytePerBufferCounter);
					//printf("the frequency is %p and adjusted is %p for divisor %lu\n", frequency, adjustedFrequency, frequencyDivisor);

					if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
					{
						memCpyBanked(bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, (byte*)&adjustedFrequency + 1, soundFile->soundBank, 1);
					}
					else
					{
						GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1] = *((byte*)&adjustedFrequency + 1);
					}
					GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = *((byte*)&adjustedFrequency);
				}

				noteByteCounter = (noteByteCounter + 1) % NO_NOTE_BYTES;
			}
		}
		else
		{
			//printf("detected end byte\n");
			endByteDetected = FALSE;
			seenFFFFCounter++;
			noteByteCounter = 0;
		}

		//printf("result %d, checking that %d == %d\n", bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1 && !firstRun, bytePerBufferCounter, LOCAL_WORK_AREA_SIZE - 1);
		
		bytePerBufferCounter++;
		if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE)
		{
			//printf("dest %p source %p bank %p size %d\n", localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE);
			//printf("dest %p source %p bank %p size %d offset %p\n", localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE, (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE);
			//printf("%d %p\n", localBufferStatus.bufferCounter, localBufferStatus.bankedData);
			memCpyBanked(localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE);

			//printf("here is the stop and the buffer counter is %p\n", bytePerBufferCounter);
		
			bytePerBufferCounter = 0;
		
			trap = TRUE;
		}
	}
	
	if (bytePerBufferCounter)
	{
		memCpyBanked(localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, bytePerBufferCounter);
	}
	//printf("you are finalising with %p  and size %d end result %p\n", localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, bytePerBufferCounter, localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE + bytePerBufferCounter);

	//printf("the bank is %p and data %p\n", soundFile->soundBank, soundFile->ch1, bytePerBufferCounter);
}

void b1LoadSoundFile(int soundNum) {

	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte i;
	byte** currentChannel;
	unsigned int soundChannelOffSets[NO_CHANNELS];

	getLogicDirectory(&agiFilePosType, &snddir[soundNum]);
	b6LoadAGIFile(SOUND, &agiFilePosType, &tempAGI);

	b1LoadedSounds[soundLoadCounter].soundResource = tempAGI.code;
	b1LoadedSounds[soundLoadCounter].soundBank = tempAGI.codeBank;
	b1LoadedSoundsPointer[soundNum] = &b1LoadedSounds[soundLoadCounter];

	memCpyBanked((byte*)soundChannelOffSets, tempAGI.code, tempAGI.codeBank, NO_CHANNELS * 2);

	for (i = 0, currentChannel = &b1LoadedSounds[soundLoadCounter].ch1; i < NO_CHANNELS; i++, currentChannel++)
	{
		*currentChannel = tempAGI.code + soundChannelOffSets[i];
	}
	
	//printf("sound num is %d\n", soundNum);
	b1PrecomputeValues(&b1LoadedSounds[soundLoadCounter]);

	if (soundLoadCounter < MAX_LOADED_SOUNDS - 1)
	{
		soundLoadCounter++;
	}
}
#pragma code-name (pop)
