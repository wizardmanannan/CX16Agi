#include "sound.h"

#define NOISE_CHANNEL 3
#define NO_CHANNELS 4
#define NO_NOTE_BYTES 5
#define DURATION_BYTE 0
#define FREQUENCY_BYTE 2
#define NOISE_FREQUENCY_BYTE 3
#define VOLUME_BYTE 4
#define FREQUENCY_NUMERATOR 111860

#define LFSR_INITIAL_SEED 0xACE3

int soundEndFlag;

extern int soundEndFlag;

#pragma bss-name (push, "BANKRAM01")
SoundFile b1LoadedSounds[MAX_LOADED_SOUNDS];
SoundFile* b1LoadedSoundsPointer[MAX_SOUNDS];
byte soundLoadCounter;
unsigned long totalSoundSize, totalBeats;
static unsigned int b1LFSR = LFSR_INITIAL_SEED;
const uint16_t b7NoiseFreq[] = { 2230, 1115, 557 };

const unsigned char SN76489_to_CX16_Volume[16] = {
	63, // SN76489 volume 0  - Loudest
	59, // SN76489 volume 1
	55, // SN76489 volume 2
	51, // SN76489 volume 3
	47, // SN76489 volume 4
	43, // SN76489 volume 5
	39, // SN76489 volume 6
	35, // SN76489 volume 7
	31, // SN76489 volume 8
	27, // SN76489 volume 9
	23, // SN76489 volume 10
	19, // SN76489 volume 11
	15, // SN76489 volume 12
	11, // SN76489 volume 13
	7,  // SN76489 volume 14
	0   // SN76489 volume 15 - Silent
};

#define LATCH_TO_CH2 0x0

#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM01")

//We minus 2 to take us to the nearest factor of 3
#define ONETHIRDBUFFERSIZE (LOCAL_WORK_AREA_SIZE - 2) / 3 

void b1WriteNextNG(byte** memoryBlock, byte* buffer, byte** dataPtr, byte toWrite, BufferStatus* bufferStatus, unsigned int* oldBlockSize, unsigned int newBlockSize, byte* bank, unsigned int bufferSize) 
{                          
       if(*dataPtr >= buffer + bufferSize) 
		{ 
			b5FlushBufferNonGolden(bufferStatus, buffer, bufferSize, bufferSize); 
            *dataPtr = buffer; 
		} 
	   
	   //printf("we are checking that %p + %p (%p - %p) > %p\n", bufferStatus->bufferCounter * bufferSize, (*dataPtr - buffer), *dataPtr, buffer, *(oldBlockSize));
        if(bufferStatus->bufferCounter * bufferSize + (*dataPtr - buffer) > *(oldBlockSize)) 
        { 
			//printf("previously %p %p bank %p. Try to set %p\n", *memoryBlock, *oldBlockSize, *bank, newBlockSize);
            b5ReallocateBiggerMemoryBlock(memoryBlock, newBlockSize, oldBlockSize, bank);
			//printf("new %p %p %p\n", *memoryBlock, *oldBlockSize, *bank);
        } 


		 *((*dataPtr)++) = toWrite; 
		 //printf("we are storing %p in %p\n", toWrite, *dataPtr);
  } 

#define MIN_LFSR 8
#define MAX_LFSR 2400

uint16_t b1GetDividerFromCx16(uint16_t cx16_freq)
{
	unsigned long numerator = 241699UL; // (7734375 / 32)
	unsigned long denom = (2 * (uint32_t)cx16_freq) - 1;
	if (denom == 0) denom = 1; // protect from division by zero
	
	
	//printf("input %lx output %lx \n", cx16_freq, (uint16_t)(numerator / denom));

	return (uint16_t)(numerator / denom);
}

#define CALC_C16_FREQ(freq) ((freq * 176026) / 65536);
#define FREQ_TO_CX_16(freq) freq = CALC_C16_FREQ(freq);
unsigned int b1DetermineLsfrDuration(unsigned int ch2Duration, unsigned int ch3Duration, byte frequencyByte, unsigned int currentCh2Frequency, unsigned int* lsfrFrequencyToPlay, unsigned long remainingBeats)
{
	byte noiseFrequency = frequencyByte & 3;
	unsigned int result = ch3Duration, divider;
	if (noiseFrequency != 3)
	{
		ch2Duration = UINT_MAX;
		*lsfrFrequencyToPlay = CALC_C16_FREQ(b7NoiseFreq[noiseFrequency]);
		divider = b1GetDividerFromCx16(*lsfrFrequencyToPlay);
	}
	else
	{
		*lsfrFrequencyToPlay = ((unsigned long)currentCh2Frequency * 2) / 3;
		//printf("we try to play %p\n", *lsfrFrequencyToPlay);
		divider = b1GetDividerFromCx16(currentCh2Frequency);
	}

	if (ch2Duration < ch3Duration)
	{
		result = ch2Duration;
	}
	
	if (divider < result)
	{
		result = divider;
	}

	if (remainingBeats < result)
	{
		result = remainingBeats;
	}

	if (result < MIN_LFSR)
	{
		result = MIN_LFSR;
	}

	if (result > MAX_LFSR)
	{
		result = MAX_LFSR;
	}

	//printf("compare vals: %u, %u, %u, %lu\n", ch2Duration, ch3Duration, *lsfrFrequencyToPlay, totalBeats);

	return result;
}

void b1ClearLoadedSound(byte soundFileNumber)
{
	b1LoadedSounds[soundFileNumber].ch0 = NULL;
	b1LoadedSounds[soundFileNumber].ch1 = NULL;
	b1LoadedSounds[soundFileNumber].ch2 = NULL;
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

unsigned int b1ReadAhead(SoundFile* soundFile, unsigned int bytePerBufferCounter, BufferStatus* bufferStatus)
{
	byte readAheadByte;

	if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
	{
		memCpyBanked(&readAheadByte, bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, soundFile->soundBank, 1);
	}
	else
	{
		readAheadByte = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1];

		//printf("we read ahead to %x, from addr %p, counter %p\n", readAheadByte, &GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], bytePerBufferCounter);
	}

	return readAheadByte;
}

unsigned int b1CopyAhead(SoundFile* soundFile, unsigned int bytePerBufferCounter, BufferStatus* bufferStatus, byte toCopy)
{
	if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
	{
		memCpyBanked(bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, &toCopy, soundFile->soundBank, 1);
	}
	else
	{
		GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1] = toCopy;
		//printf("tocopy is %x\n", toCopy);
	}
}

#define FREQUENCY_NUMERATOR 111860

#define ORIGINAL_CHNOISEBUFFER (GOLDEN_RAM_WORK_AREA + ONETHIRDBUFFERSIZE * 2)

#define INCREASE_BLOCK_SIZE_AMOUNT 30
#define GET_CH(i) GET_NEXT_NG(channelBytes[i], oldBuffer, oldChDataPtr, bufferStatus, ONETHIRDBUFFERSIZE);
#define WRITENOISE(toWrite) b1WriteNextNG(&newSoundFile.soundResource, ORIGINAL_CHNOISEBUFFER, newChNoiseDataPtr, toWrite, &newChNoiseLocalBufferStatus, &allocatedBlockSize, allocatedBlockSize + INCREASE_BLOCK_SIZE_AMOUNT, &newSoundFile.soundBank, ONETHIRDBUFFERSIZE);

void b1SetChannelOffsets(byte* codePtr, SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	byte i, ** currentChannel;

	for (i = 0, currentChannel = &soundFile->ch0; i < NO_CHANNELS; i++, currentChannel++)
	{
		*currentChannel = codePtr + soundChannelOffSets[i];
	}
}

//#define GET_CH_NOISE GET_NEXT_NG(oldChNoiseByte, oldChNoiseBuffer, oldChNoiseDataPtr, &oldChNoiseLocalBufferStatus, ONETHIRDBUFFERSIZE);

boolean b1FillNoteBuffer(byte* oldBuffer, byte** oldChDataPtr, BufferStatus* bufferStatus, byte* channelBytes)
{
	byte i;

	for (i = 0; i < NO_NOTE_BYTES; i++)
	{
		GET_CH(i)

			if (i == 1 && channelBytes[0] == 0xFF && channelBytes[1] == 0xFF)
			{
				return FALSE;
			}
	}

	return TRUE;
}

unsigned int b1Advance2(byte* oldBuffer, byte** oldChDataPtr, BufferStatus* bufferStatus, byte* channelBytes, boolean* moreTwoToRead, unsigned int advancement)
{
	long currentDuration;
	boolean finished = FALSE;
	byte i;
	unsigned int result;

	currentDuration = *((unsigned int*)&channelBytes[DURATION_BYTE]);

	//printf("the current duration is %lu. more to to go %d. byte 0 %p, byte 1 %p\n", currentDuration, *moreTwoToRead, channelBytes[0], channelBytes[1]);

	while (!finished && *moreTwoToRead)
	{
		currentDuration -= advancement;

		//printf("we advance by %d leaving a current duration of %ld\n", advancement, currentDuration);

		if (currentDuration < 0 && (*moreTwoToRead = b1FillNoteBuffer(oldBuffer, oldChDataPtr, bufferStatus, channelBytes)))
		{
			advancement = currentDuration * -1;

			//printf("it is less than 0, we next advance by %d\n", advancement);

			currentDuration = *((unsigned int*)&channelBytes[DURATION_BYTE]);

			//printf("we now have an duration of %ld\n", currentDuration);
			//printf("channel bytes address %p\n", channelBytes);

			finished = FALSE;
		}
		else
		{
			finished = TRUE;
		}
	}

	*((unsigned int*)&channelBytes[DURATION_BYTE]) = currentDuration;


	result = *((unsigned int*)&channelBytes[FREQUENCY_BYTE]);

	//printf("the result is %u. channel bytes address %p\n", result, channelBytes);

	return result;
}

void b1PreComputePeriodicSound(SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	BufferStatus oldCh2LocalBufferStatus, oldChNoiseLocalBufferStatus, newChNoiseLocalBufferStatus;
	byte* oldCh2Buffer = GOLDEN_RAM_WORK_AREA, * oldChNoiseBuffer = GOLDEN_RAM_WORK_AREA + ONETHIRDBUFFERSIZE, * newChNoiseBuffer = ORIGINAL_CHNOISEBUFFER;
	byte** oldCh2DataPtr = &oldCh2Buffer, ** oldChNoiseDataPtr = &oldChNoiseBuffer, ** newChNoiseDataPtr = &newChNoiseBuffer;
	BufferStatus* ch2BufferStatus;
	SoundFile newSoundFile;
	byte oldCh2Bytes[NO_NOTE_BYTES], oldChNoiseBytes[NO_NOTE_BYTES];
	boolean moreTwoToRead;
	unsigned int i, noiseFrequency, advancement, ch2CurrentFreq, allocatedBlockSize = totalSoundSize + totalSoundSize / 2, currentPeriodicDuration = 0, lfsrDuration, lsfrFrequencyToPlay, lfsr = LFSR_INITIAL_SEED, feedback; //unsigned int instead of long here, because in this place it will never be negative
	unsigned long remainingBeats = totalBeats;


	ch2BufferStatus = &oldCh2LocalBufferStatus;

	oldCh2LocalBufferStatus.bank = soundFile->soundBank;
	oldCh2LocalBufferStatus.bankedData = soundFile->ch2;
	oldCh2LocalBufferStatus.bufferCounter = 0;

	oldChNoiseLocalBufferStatus.bank = soundFile->soundBank;
	oldChNoiseLocalBufferStatus.bankedData = soundFile->chNoise;
	oldChNoiseLocalBufferStatus.bufferCounter = 0;

	b5RefreshBufferNonGolden(&oldCh2LocalBufferStatus, oldCh2Buffer, ONETHIRDBUFFERSIZE);
	b5RefreshBufferNonGolden(&oldChNoiseLocalBufferStatus, oldChNoiseBuffer, ONETHIRDBUFFERSIZE);

	newSoundFile.soundResource = b10BankedAlloc(allocatedBlockSize, &newSoundFile.soundBank);

	memCpyBankedBetween(newSoundFile.soundResource, newSoundFile.soundBank, soundFile->soundResource, soundFile->soundBank, soundFile->chNoise - soundFile->soundResource);

	b1SetChannelOffsets(newSoundFile.soundResource, &newSoundFile, soundChannelOffSets);

	newChNoiseLocalBufferStatus.bank = newSoundFile.soundBank;

	//printf("we set the sound bank to %p\n", newSoundFile.soundBank);

	newChNoiseLocalBufferStatus.bankedData = newSoundFile.chNoise;
	newChNoiseLocalBufferStatus.bufferCounter = 0;

	//printf("the data is at %p. ch0 %p ch1 %p ch2 %p ch3 %p. bank %p. block size %p\n", newSoundFile.soundResource, newSoundFile.ch0, newSoundFile.ch1, newSoundFile.ch2, newSoundFile.chNoise, newSoundFile.soundBank, allocatedBlockSize);
	//printf("the data is at %p. ch0 %p ch1 %p ch2 %p ch3 %p bank %p\n", soundFile->soundResource, soundFile->ch0, soundFile->ch1, soundFile->ch2, soundFile->chNoise, soundFile->soundBank);

	//printf("buffer three is at %p\n", newChNoiseBuffer);

	moreTwoToRead = b1FillNoteBuffer(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes);

	if (moreTwoToRead)
	{
		ch2CurrentFreq = *((unsigned int*)oldCh2Bytes[FREQUENCY_BYTE]);
	}
	while (b1FillNoteBuffer(oldChNoiseBuffer, oldChNoiseDataPtr, &oldChNoiseLocalBufferStatus, oldChNoiseBytes))
	{
		if ((oldChNoiseBytes[NOISE_CHANNEL] & 4) >> 2) //white noise
		{
			advancement = *((unsigned int*)oldChNoiseBytes[DURATION_BYTE]);
			b1Advance2(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes, &moreTwoToRead, advancement);

			for (i = 0; i < NO_NOTE_BYTES; i++)
			{
				WRITENOISE(oldChNoiseBytes[i]);
			}

			if (remainingBeats > *((unsigned int*)&oldCh2Bytes[DURATION_BYTE]))
			{
				remainingBeats -= *((unsigned int*)&oldCh2Bytes[DURATION_BYTE]);
			}
		}
		else //periodic
		{
			//printf("the address of the result set is %p\n", oldCh2Bytes);
			lfsrDuration = b1DetermineLsfrDuration(*((unsigned int*)&oldCh2Bytes[DURATION_BYTE]), *((unsigned int*)&oldChNoiseBytes[DURATION_BYTE]), oldChNoiseBytes[NOISE_CHANNEL], *((unsigned int*)&oldCh2Bytes[FREQUENCY_BYTE]), &lsfrFrequencyToPlay, remainingBeats);

			//printf("the f to play is %p\n", lsfrFrequencyToPlay);

			WRITENOISE((*((byte*)&lfsrDuration)));
			WRITENOISE((*((byte*)&lfsrDuration + 1)));
			WRITENOISE((*((byte*)&lsfrFrequencyToPlay)));
			WRITENOISE((*((byte*)&lsfrFrequencyToPlay + 1)));
			WRITENOISE((lfsr & 1 ? SN76489_to_CX16_Volume[oldChNoiseBytes[VOLUME_BYTE]] & 0xF : 0) | 0X80);

			feedback = (lfsr ^ (lfsr >> 14)) & 1;
			lfsr = (lfsr >> 1) | (feedback << 14);   // keep it 15-bit

			b1Advance2(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes, &moreTwoToRead, lfsrDuration);
			if (remainingBeats > lfsrDuration)
			{
				remainingBeats -= lfsrDuration;
			}

			//printf("the address of the noise bytes is %p\n", newChNoiseDataPtr);
		}
	}

	WRITENOISE(0xFF);
	WRITENOISE(0xFF);

	b5FlushBufferNonGolden(&newChNoiseLocalBufferStatus, ORIGINAL_CHNOISEBUFFER, ONETHIRDBUFFERSIZE, newChNoiseBuffer - ORIGINAL_CHNOISEBUFFER);
	//printf("we flush to %p bank %p. addr %p\n", newChNoiseLocalBufferStatus.bankedData, newChNoiseLocalBufferStatus.bank, &newChNoiseLocalBufferStatus);

	b10BankedDealloc(soundFile->soundResource, soundFile->soundBank);
	*soundFile = newSoundFile;
}

void b1PrecomputeValues(SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	BufferStatus localBufferStatus;
	byte seenFFFFCounter = 0;
	boolean endByteDetected = FALSE;
	byte noteByteCounter = 0;
	byte readByte, readAheadByte;
	byte* buffer = GOLDEN_RAM_WORK_AREA;
	byte** data = &buffer;
	BufferStatus* bufferStatus;
	unsigned int tenBitDivider, bytePerBufferCounter = 0, duration, adjustedDuration;
	unsigned long divider, adjustedFrequency, beatsForChannel = 0;
	boolean firstRun = TRUE, periodicSoundDetected = FALSE, noiseChHasVolume = FALSE;

	unsigned int totalCounter = 8;

	totalBeats = 0;

	bufferStatus = &localBufferStatus;

	localBufferStatus.bank = soundFile->soundBank;
	localBufferStatus.bankedData = soundFile->ch0;
	localBufferStatus.bufferCounter = 0;

	b5RefreshBuffer(&localBufferStatus);

	//printf("the bank is %p and data %p\n", soundFile->soundBank, soundFile->ch1);

	while (seenFFFFCounter != NO_CHANNELS)
	{
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
			if (readByte == 0xFF && noteByteCounter == 0)
			{
				//printf("ff detected \n");

				readAheadByte = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);


				if (readAheadByte == 0xFF)
				{
					endByteDetected = TRUE;
				}
			}
			if (!endByteDetected)
			{
				if (noteByteCounter == DURATION_BYTE)
				{
					*((byte*)&duration) = readByte;
					//printf("the readByte is %p\n", readByte);

					*((byte*)&duration + 1) = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);
					*((byte*)&duration) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];

					adjustedDuration = ((unsigned int)*((byte*)&duration)) | ((unsigned int)*((byte*)&duration + 1)) << 8;

					b1CopyAhead(soundFile, bytePerBufferCounter, bufferStatus, *((byte*)&adjustedDuration + 1));

					beatsForChannel += adjustedDuration;
					//	printf("setting beats to %lu\n", beatsForChannel);
				}
				else if (noteByteCounter == FREQUENCY_BYTE)
				{
					*((byte*)&tenBitDivider) = readByte;
					if (seenFFFFCounter != NOISE_CHANNEL)
					{
						*((byte*)&tenBitDivider + 1) = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);
						*((byte*)&tenBitDivider) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];


						divider = ((*((byte*)&tenBitDivider) & 0x3F) << 4) + (*((byte*)&tenBitDivider + 1) & 0x0F) & 0xFFFF;
												
						adjustedFrequency = ((FREQUENCY_NUMERATOR / divider) + 1) / 2;
						FREQ_TO_CX_16(adjustedFrequency);

						b1CopyAhead(soundFile, bytePerBufferCounter, bufferStatus, *((byte*)&adjustedFrequency + 1));
					}


					//printf("bbc %d\n", bytePerBufferCounter);
					//printf("the frequency is %p and adjusted is %p for divisor %lu\n", frequency, adjustedFrequency, frequencyDivisor);


					GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = *((byte*)&adjustedFrequency);
				}
				else if (noteByteCounter == NOISE_FREQUENCY_BYTE && !periodicSoundDetected && seenFFFFCounter == 3)
				{
					periodicSoundDetected = (~GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 4);
					//printf("val %p, &4 %p inverse %p\n", GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], ~GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], ~GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 4);
					//printf("the address is %p\n", &GOLDEN_RAM_WORK_AREA[bytePerBufferCounter]);
				}
				else if (noteByteCounter == VOLUME_BYTE)
				{
					//printf("dur is %p\n", adjustedDuration);
					if (adjustedDuration != 0xFFFF)
					{
						noiseChHasVolume = noiseChHasVolume || ((GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F) != 0xF) && seenFFFFCounter == 3;

						//printf("val is %p adjus %d %d\n", GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F, SN76489_to_CX16_Volume[GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F]);
						GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = SN76489_to_CX16_Volume[(GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F)];

						//printf("vol: %p %p %p %p\n", GOLDEN_RAM_WORK_AREA[bytePerBufferCounter], GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F, (GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F) != 0x0F, noiseChHasVolume);
					}
					else
					{
						GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = 0;
					}
				}
				//asm("stp");

				noteByteCounter = (noteByteCounter + 1) % NO_NOTE_BYTES;
			}
		}
		else
		{
			//printf("detected end byte\n");
			endByteDetected = FALSE;
			seenFFFFCounter++;
			noteByteCounter = 0;

			//printf("beats for channel %lu\n", beatsForChannel);
			if (beatsForChannel > totalBeats)
			{
				totalBeats = beatsForChannel;
			}
			beatsForChannel = 0;
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
		}
	}

	if (bytePerBufferCounter)
	{
		memCpyBanked(localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, bytePerBufferCounter);
	}

	if (periodicSoundDetected && noiseChHasVolume)
	{
		b1PreComputePeriodicSound(&b1LoadedSounds[soundLoadCounter], soundChannelOffSets);
	}
	//printf("you are finalising with %p  and size %d end result %p\n", localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, bytePerBufferCounter, localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE + bytePerBufferCounter);

	//printf("the bank is %p and data %p\n", soundFile->soundBank, soundFile->ch1, bytePerBufferCounter);
}

void b1LoadSoundFile(int soundNum) {

	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte i;
	unsigned int soundChannelOffSets[NO_CHANNELS];

	//printf("you are loading sound %d\n", soundNum);

	//soundNum = 19;

	//printf("your sound file is %d\n", soundNum);

	getLogicDirectory(&agiFilePosType, &snddir[soundNum]);
	b6LoadAGIFile(SOUND, &agiFilePosType, &tempAGI);

	b1LoadedSounds[soundLoadCounter].soundResource = tempAGI.code;
	b1LoadedSounds[soundLoadCounter].soundBank = tempAGI.codeBank;
	b1LoadedSoundsPointer[soundNum] = &b1LoadedSounds[soundLoadCounter];
	totalSoundSize = tempAGI.totalSize;

	memCpyBanked((byte*)soundChannelOffSets, tempAGI.code, tempAGI.codeBank, NO_CHANNELS * 2);

	b1SetChannelOffsets(tempAGI.code, &b1LoadedSounds[soundLoadCounter], soundChannelOffSets);

	//printf("sound num is %d\n", soundNum);
	b1PrecomputeValues(&b1LoadedSounds[soundLoadCounter], soundChannelOffSets);
	
	if (soundLoadCounter < MAX_LOADED_SOUNDS - 1)
	{
		soundLoadCounter++;
	}
}

extern unsigned int b1Ch1Ticks;
extern unsigned int b1Ch2Ticks;
extern unsigned int b1Ch3Ticks;
extern unsigned int b1Ch4Ticks;
extern boolean b1IsPlaying[NO_CHANNELS];
extern byte b1SoundDataBank;
extern byte b1ChannelsPlaying;
extern byte b1EndSoundFlag;

extern byte* ZP_CURRENTLY_PLAYING_NOTE_1;
#pragma zpsym("ZP_CURRENTLY_PLAYING_NOTE_1")
extern byte* ZP_CURRENTLY_PLAYING_NOTE_2;
#pragma zpsym("ZP_CURRENTLY_PLAYING_NOTE_2")
extern byte* ZP_CURRENTLY_PLAYING_NOTE_3;
#pragma zpsym("ZP_CURRENTLY_PLAYING_NOTE_3")
extern byte* ZP_CURRENTLY_PLAYING_NOTE_NOISE;
#pragma zpsym("ZP_CURRENTLY_PLAYING_NOTE_NOISE")

extern void b1PsgClear();

void b1PlaySound(byte soundNum, byte endSoundFlag)
{
	byte testVal, i;
	unsigned int* ticksPointer;
	byte** channelPointer;

	asm("sei");

	//soundNum = 19;

	b1Ch1Ticks = 0;
	b1Ch2Ticks = 0;
	b1Ch3Ticks = 0;
	b1Ch4Ticks = 0;


	flag[endSoundFlag] = FALSE;

	ZP_CURRENTLY_PLAYING_NOTE_1 = b1LoadedSoundsPointer[soundNum]->ch0 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_2 = b1LoadedSoundsPointer[soundNum]->ch1 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_3 = b1LoadedSoundsPointer[soundNum]->ch2 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_NOISE = b1LoadedSoundsPointer[soundNum]->chNoise - NO_NOTE_BYTES;

	b1SoundDataBank = b1LoadedSoundsPointer[soundNum]->soundBank;

	memset(b1IsPlaying, TRUE, NO_CHANNELS);
	b1ChannelsPlaying = NO_CHANNELS;
	b1EndSoundFlag = endSoundFlag;
	//printf("you are playing %d the end sound flag is %x. the address is %p\n", soundNum, endSoundFlag, &flag[endSoundFlag]);


	REENABLE_INTERRUPTS();
}

void b1StopSound()
{
	asm("sei");
	b1PsgClear();
	memset(b1IsPlaying, FALSE, NO_CHANNELS);
	REENABLE_INTERRUPTS();
}

#pragma code-name (pop)
