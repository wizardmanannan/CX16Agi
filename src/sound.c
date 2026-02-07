#include "sound.h"

#define NOISE_CHANNEL 3                 // Noise channel index
#define NO_CHANNELS 4                  // Total number of sound channels
#define NO_NOTE_BYTES 5                // Number of bytes per note
#define DURATION_BYTE 0                // Index of duration byte in note
#define FREQUENCY_BYTE 2               // Index of frequency byte in note
#define NOISE_FREQUENCY_BYTE 3         // Index of noise frequency byte in note
#define VOLUME_BYTE 4                  // Index of volume byte in note
#define FREQUENCY_NUMERATOR 111860     // Frequency conversion numerator

#define LFSR_INITIAL_SEED 0xACE3       // Initial seed for Linear Feedback Shift Register (LFSR)

int soundEndFlag;                      // Flag indicating sound end

extern int soundEndFlag;               // External reference to the sound end flag

#pragma bss-name (push, "BANKRAM01")
SoundFile b1LoadedSounds[MAX_LOADED_SOUNDS];         // Array of loaded sound files
SoundFile* b1LoadedSoundsPointer[MAX_SOUNDS];        // Pointers to loaded sounds indexed by sound number
byte soundLoadCounter;                                 // Counter of loaded sounds
unsigned long totalSoundSize, totalBeats;             // Total size and beats for current sound
static unsigned int b1LFSR = LFSR_INITIAL_SEED;       // Static LFSR state used in noise generation

// Predefined noise frequencies for noise channel
const uint16_t b7NoiseFreq[] = { 2230, 1115, 557 };

// Volume conversion table from SN76489 format to CX16 volume scale
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

#define LATCH_TO_CH2 0x0               // Latch flag for channel 2

#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM01")

// Calculate one-third buffer size, adjusted to nearest factor of 3,
// by subtracting 2 from local work area size and division by 3.
#define ONETHIRDBUFFERSIZE (LOCAL_WORK_AREA_SIZE - 2) / 3 

// Writes a byte (toWrite) to the next position in the note generation buffer,
// reallocating or flushing buffers as necessary.
void b1WriteNextNG(byte** memoryBlock, byte* buffer, byte** dataPtr, byte toWrite, BufferStatus* bufferStatus, unsigned int* oldBlockSize, unsigned int newBlockSize, byte* bank, unsigned int bufferSize)
{
	// If pointer exceeds buffer size, flush and reset pointer.
	if (*dataPtr >= buffer + bufferSize)
	{
		b5FlushBufferNonGolden(bufferStatus, buffer, bufferSize, bufferSize);
		*dataPtr = buffer;
	}

	// Check if current position exceeds allocated block size, reallocate if needed.
	if (bufferStatus->bufferCounter * bufferSize + (*dataPtr - buffer) > *(oldBlockSize))
	{
		b5ReallocateBiggerMemoryBlock(memoryBlock, newBlockSize, oldBlockSize, bank);
	}

	// Write the byte and move data pointer forward
	*((*dataPtr)++) = toWrite;
}

#define MIN_LFSR 8                   // Minimum length of LFSR duration
#define MAX_LFSR 2400                // Maximum length of LFSR duration

// Converts CX16 frequency to sound divider value for PSG tone generation.
uint16_t b1GetDividerFromCx16(uint16_t cx16_freq)
{
	unsigned long numerator = 241699UL; // (7734375 / 32), clock related constant
	unsigned long denom = (2 * (uint32_t)cx16_freq) - 1;

	if (denom == 0) denom = 1; // Prevent division by zero

	return (uint16_t)(numerator / denom);
}

#define CALC_C16_FREQ(freq) ((freq * 176026) / 65536)  // Approximate frequency calculation macro for CX16
#define FREQ_TO_CX_16(freq) freq = CALC_C16_FREQ(freq); // Convert freq to CX16 scale

// Determines duration for LFSR noise processing based on channel 2 and channel 3 durations,
// noise frequency byte, current channel 2 frequency, and beats remaining.
unsigned int b1DetermineLsfrDuration(unsigned int ch2Duration, unsigned int ch3Duration, byte frequencyByte, unsigned int currentCh2Frequency, unsigned int* lsfrFrequencyToPlay, unsigned long remainingBeats)
{
	byte noiseFrequency = frequencyByte & 3;  // Extract low two bits for noise frequency

	unsigned int result = ch3Duration, divider;

	if (noiseFrequency != 3)
	{
		// If noise frequency is not periodic noise (frequency != 3)
		ch2Duration = UINT_MAX;  // Invalidate ch2Duration for comparison
		*lsfrFrequencyToPlay = CALC_C16_FREQ(b7NoiseFreq[noiseFrequency]);  // Get CX16 freq for noise
		divider = b1GetDividerFromCx16(*lsfrFrequencyToPlay);
	}
	else
	{
		// For periodic noise, use 2/3 of current channel 2 frequency
		*lsfrFrequencyToPlay = ((unsigned long)currentCh2Frequency * 2) / 3;
		divider = b1GetDividerFromCx16(currentCh2Frequency);
	}

	// Find minimum time slice for LFSR based on durations and divider
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
		result = (unsigned int)remainingBeats;
	}

	// Clamp result between min and max LFSR durations
	if (result < MIN_LFSR)
	{
		result = MIN_LFSR;
	}

	if (result > MAX_LFSR)
	{
		result = MAX_LFSR;
	}

	return result;
}

// Clears the loaded sound data for the specified sound file number, resetting all pointers and bank.
void b1ClearLoadedSound(byte soundFileNumber)
{
	b1LoadedSounds[soundFileNumber].ch0 = NULL;
	b1LoadedSounds[soundFileNumber].ch1 = NULL;
	b1LoadedSounds[soundFileNumber].ch2 = NULL;
	b1LoadedSounds[soundFileNumber].chNoise = NULL;
	b1LoadedSounds[soundFileNumber].soundBank = 0;
	b1LoadedSounds[soundFileNumber].soundResource = NULL;
}

// Initializes the loaded sound arrays by clearing all sound entries and pointers.
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

// Discards/unloads a sound file by releasing its allocated memory and clearing references.
void b1DiscardSoundFile(int soundNum)
{
	SoundFile* sound;
	byte* soundResource;
	byte loadedSoundNum;

	// Get pointer to loaded sound structure
	sound = b1LoadedSoundsPointer[soundNum];

	// Calculate index from pointer subtracting base array pointer
	loadedSoundNum = sound - &b1LoadedSounds[0];

	soundResource = sound->soundResource;

	// Only proceed if valid sound and resource
	if (sound && soundResource != NULL)
	{
		// Deallocate banked sound resource
		b10BankedDealloc(soundResource, sound->soundBank);

		// Clear loaded sound info and pointer
		b1ClearLoadedSound(loadedSoundNum);
		b1LoadedSoundsPointer[soundNum] = NULL;
	}

	// Reduce soundLoadCounter by trimming any trailing unused loaded sounds
	// Keep keep going while the loaded sound at loadedSoundNum is null, because that means 'empty'
	// We only have 10 loaded sounds so if the counter gets to FF just stop, that will never be a valid value
	for (loadedSoundNum = soundLoadCounter - 1; loadedSoundNum != 0xFF && b1LoadedSounds[loadedSoundNum].soundResource == NULL; loadedSoundNum--)
	{
		soundLoadCounter--;
	}
}

// Reads (peeks) the next byte ahead in the buffer from the sound file, supporting banked memory.
unsigned int b1ReadAhead(SoundFile* soundFile, unsigned int bytePerBufferCounter, BufferStatus* bufferStatus)
{
	byte readAheadByte;

	// If at end of local buffer, read one byte from banked memory
	if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
	{
		memCpyBanked(&readAheadByte, bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, soundFile->soundBank, 1);
	}
	else
	{
		// Otherwise, read next byte from local work area buffer
		readAheadByte = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1];
	}

	return readAheadByte;
}

// Copies (writes) a byte ahead in the buffer supporting banked and local memory.
unsigned int b1CopyAhead(SoundFile* soundFile, unsigned int bytePerBufferCounter, BufferStatus* bufferStatus, byte toCopy)
{
	if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE - 1)
	{
		// Copy byte to banked memory area just past buffer
		memCpyBanked(bufferStatus->bankedData + LOCAL_WORK_AREA_SIZE, &toCopy, soundFile->soundBank, 1);
	}
	else
	{
		// Copy byte to local work area buffer next position
		GOLDEN_RAM_WORK_AREA[bytePerBufferCounter + 1] = toCopy;
	}
}

#define FREQUENCY_NUMERATOR 111860       // Redefinition, original frequency numerator constant
#define ORIGINAL_CHNOISEBUFFER (GOLDEN_RAM_WORK_AREA + ONETHIRDBUFFERSIZE * 2)  // Noise channel buffer start in local work area
#define INCREASE_BLOCK_SIZE_AMOUNT 30    // Memory block growth size constant

// Macro to get the next note generation byte for channel i
#define GET_CH(i) GET_NEXT_NG(channelBytes[i], oldBuffer, oldChDataPtr, bufferStatus, ONETHIRDBUFFERSIZE);

// Macro to write noise byte using the noise channel generator function
#define WRITENOISE(toWrite) b1WriteNextNG(&newSoundFile.soundResource, ORIGINAL_CHNOISEBUFFER, newChNoiseDataPtr, toWrite, &newChNoiseLocalBufferStatus, &allocatedBlockSize, allocatedBlockSize + INCREASE_BLOCK_SIZE_AMOUNT, &newSoundFile.soundBank, ONETHIRDBUFFERSIZE);

// Sets the channel data pointers in the SoundFile struct based on offsets
// Accepts as an argument the 'codePtr' which is a pointer to the sounds allocated resource block
void b1SetChannelOffsets(byte* codePtr, SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	byte i, ** currentChannel;

	// Iterate through all channels
	for (i = 0, currentChannel = &soundFile->ch0; i < NO_CHANNELS; i++, currentChannel++)
	{
		// Set pointer for each channel start position by adding offset to base pointer
		*currentChannel = codePtr + soundChannelOffSets[i];
	}
}

// Fills a note buffer from old channel data pointers into channelBytes, returns FALSE if end marker found.
boolean b1FillNoteBuffer(byte* oldBuffer, byte** oldChDataPtr, BufferStatus* bufferStatus, byte* channelBytes)
{
	byte i;

	for (i = 0; i < NO_NOTE_BYTES; i++)
	{
		GET_CH(i) // Use macro to copy note byte from old buffer

			// If first two bytes signal end of stream (0xFF 0xFF), stop reading.
			// Only the first two bytes together can signal the end, which is why we can only detect the end when we have read 1.
			if (i == 1 && channelBytes[0] == 0xFF && channelBytes[1] == 0xFF)
			{
				return FALSE;
			}
	}

	return TRUE;
}

// Advances the note data pointer by given duration advancement, updating byte counters.
// Returns frequency value from the updated note.
// Used particularly to advance our buffer of channel 2, used in LSFR
// The idea is to advance the duration by the set amount, and when we run out of duration go to the next note
unsigned int b1Advance2(byte* oldBuffer, byte** oldChDataPtr, BufferStatus* bufferStatus, byte* channelBytes, boolean* moreTwoToRead, unsigned int advancement)
{
	long currentDuration;              // Long because it can be decremented to less than zero (temporarily)
	boolean finished = FALSE;          // Loop control flag
	byte i;
	unsigned int result;

	// Get current duration value from note bytes, assuming little endian 2-byte int.
	currentDuration = *((unsigned int*)&channelBytes[DURATION_BYTE]);

	// Process through notes until advancement is applied or no more notes
	while (!finished && *moreTwoToRead)
	{
		currentDuration -= advancement;

		// If current duration expires, load next note and reset advancement accordingly
		if (currentDuration < 0 && (*moreTwoToRead = b1FillNoteBuffer(oldBuffer, oldChDataPtr, bufferStatus, channelBytes)))
		{
			advancement = currentDuration * -1; // Negative remainder is turned positive
			currentDuration = *((unsigned int*)&channelBytes[DURATION_BYTE]); // Reset duration for next note
			finished = FALSE; // Continue loop
		}
		else
		{
			finished = TRUE; // Completed advancement
		}
	}

	// Store updated remaining duration back to note bytes
	*((unsigned int*)&channelBytes[DURATION_BYTE]) = currentDuration;

	// Return frequency parameter from updated note bytes
	result = *((unsigned int*)&channelBytes[FREQUENCY_BYTE]);

	return result;
}

// Precomputes periodic sound data, modifying noise channel data for periodic noise types, leaving white noise alone as this can be handled by the VERA.
void b1PreComputePeriodicSound(SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	BufferStatus oldCh2LocalBufferStatus, oldChNoiseLocalBufferStatus, newChNoiseLocalBufferStatus;
	byte* oldCh2Buffer = GOLDEN_RAM_WORK_AREA, * oldChNoiseBuffer = GOLDEN_RAM_WORK_AREA + ONETHIRDBUFFERSIZE, * newChNoiseBuffer = ORIGINAL_CHNOISEBUFFER;
	byte** oldCh2DataPtr = &oldCh2Buffer, ** oldChNoiseDataPtr = &oldChNoiseBuffer, ** newChNoiseDataPtr = &newChNoiseBuffer;
	BufferStatus* ch2BufferStatus;
	SoundFile newSoundFile;
	byte oldCh2Bytes[NO_NOTE_BYTES], oldChNoiseBytes[NO_NOTE_BYTES];
	boolean moreTwoToRead;
	unsigned int i, noiseFrequency, advancement, ch2CurrentFreq = 0, allocatedBlockSize = totalSoundSize + totalSoundSize / 2;
	unsigned int lfsrDuration, lsfrFrequencyToPlay, lfsr = LFSR_INITIAL_SEED, feedback;
	unsigned long remainingBeats = totalBeats;

	// Setup buffer statuses for channel 2 and noise channel
	ch2BufferStatus = &oldCh2LocalBufferStatus;

	oldCh2LocalBufferStatus.bank = soundFile->soundBank;
	oldCh2LocalBufferStatus.bankedData = soundFile->ch2;
	oldCh2LocalBufferStatus.bufferCounter = 0;

	oldChNoiseLocalBufferStatus.bank = soundFile->soundBank;
	oldChNoiseLocalBufferStatus.bankedData = soundFile->chNoise;
	oldChNoiseLocalBufferStatus.bufferCounter = 0;

	// Refresh local buffers from banked data for channel 2 and noise channel
	b5RefreshBufferNonGolden(&oldCh2LocalBufferStatus, oldCh2Buffer, ONETHIRDBUFFERSIZE);
	b5RefreshBufferNonGolden(&oldChNoiseLocalBufferStatus, oldChNoiseBuffer, ONETHIRDBUFFERSIZE);

	// Allocate new memory block for new sound resource with increased size
	newSoundFile.soundResource = b10BankedAlloc(allocatedBlockSize, &newSoundFile.soundBank);

	// Copy portion of old sound resource related to noise channel into the new sound resource.
	memCpyBankedBetween(newSoundFile.soundResource, newSoundFile.soundBank, soundFile->soundResource, soundFile->soundBank, soundFile->chNoise - soundFile->soundResource);

	// Set channel offsets for new sound file based on copied data pointers
	b1SetChannelOffsets(newSoundFile.soundResource, &newSoundFile, soundChannelOffSets);

	// Initialize noise buffer status struct for writing new noise channel data
	newChNoiseLocalBufferStatus.bank = newSoundFile.soundBank;
	newChNoiseLocalBufferStatus.bankedData = newSoundFile.chNoise;
	newChNoiseLocalBufferStatus.bufferCounter = 0;

	// Read first notes from channel 2 buffer
	moreTwoToRead = b1FillNoteBuffer(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes);

	if (moreTwoToRead)
	{
		ch2CurrentFreq = *((unsigned int*)oldCh2Bytes[FREQUENCY_BYTE]);
	}

	// Main loop processing noise channel notes
	while (b1FillNoteBuffer(oldChNoiseBuffer, oldChNoiseDataPtr, &oldChNoiseLocalBufferStatus, oldChNoiseBytes))
	{
		// Check if noise is white noise (noise channel flags bit 2 set)
		if ((oldChNoiseBytes[NOISE_CHANNEL] & 4) >> 2) // white noise
		{
			// Advance channel 2 note position by noise channel note duration
			advancement = *((unsigned int*)oldChNoiseBytes[DURATION_BYTE]);
			b1Advance2(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes, &moreTwoToRead, advancement);

			// Write noise channel note bytes verbatim into new noise channel buffer
			for (i = 0; i < NO_NOTE_BYTES; i++)
			{
				WRITENOISE(oldChNoiseBytes[i]);
			}

			// Update remaining beats accounting for duration advanced
			if (remainingBeats > *((unsigned int*)&oldCh2Bytes[DURATION_BYTE]))
			{
				remainingBeats -= *((unsigned int*)&oldCh2Bytes[DURATION_BYTE]);
			}
		}
		else // Periodic noise channel processing
		{
			// Determine LFSR duration and frequency to play for periodic noise
			lfsrDuration = b1DetermineLsfrDuration(*((unsigned int*)&oldCh2Bytes[DURATION_BYTE]), *((unsigned int*)&oldChNoiseBytes[DURATION_BYTE]), oldChNoiseBytes[NOISE_CHANNEL], *((unsigned int*)&oldCh2Bytes[FREQUENCY_BYTE]), &lsfrFrequencyToPlay, remainingBeats);

			// Write computed LFSR parameters to new noise channel buffer
			WRITENOISE((*((byte*)&lfsrDuration)));
			WRITENOISE((*((byte*)&lfsrDuration + 1)));
			WRITENOISE((*((byte*)&lsfrFrequencyToPlay)));
			WRITENOISE((*((byte*)&lsfrFrequencyToPlay + 1)));
			//If the lsfr ends in 1 we will hear this note, otherwise we won't
			WRITENOISE((lfsr & 1 ? SN76489_to_CX16_Volume[oldChNoiseBytes[VOLUME_BYTE]] & 0xF : 0) | 0X80); //High byte of volume is used to indicate periodic sound

			// Update LFSR feedback according to noise generator algorithm
			feedback = (lfsr ^ (lfsr >> 14)) & 1;
			lfsr = (lfsr >> 1) | (feedback << 14);   // keep it 15-bit state

			// Advance channel 2 notes by the lfsrDuration used
			b1Advance2(oldCh2Buffer, oldCh2DataPtr, &oldCh2LocalBufferStatus, oldCh2Bytes, &moreTwoToRead, lfsrDuration);

			// Decrement remaining beats accordingly
			if (remainingBeats > lfsrDuration)
			{
				remainingBeats -= lfsrDuration;
			}
		}
	}

	// Append terminator bytes for noise channel notes to indicate end
	WRITENOISE(0xFF);
	WRITENOISE(0xFF);

	// Flush any remaining buffered noise channel data to banked memory
	b5FlushBufferNonGolden(&newChNoiseLocalBufferStatus, ORIGINAL_CHNOISEBUFFER, ONETHIRDBUFFERSIZE, newChNoiseBuffer - ORIGINAL_CHNOISEBUFFER);

	// Deallocate previous sound resource memory
	b10BankedDealloc(soundFile->soundResource, soundFile->soundBank);

	// Replace old sound file struct with newly built sound file for periodic noise
	*soundFile = newSoundFile;
}

// Performs precomputation of volume and frequency values of sound file's notes,
// converts frequencies/volumes, detects periodic sound, and adjusts sound data.
void b1PrecomputeValues(SoundFile* soundFile, unsigned int* soundChannelOffSets)
{
	BufferStatus localBufferStatus;
	byte seenFFFFCounter = 0;       // Tracks end marker count per channel
	boolean endByteDetected = FALSE;  // End-of-note sequence flag
	byte noteByteCounter = 0;         // Position within note bytes
	byte readByte, readAheadByte;
	byte* buffer = GOLDEN_RAM_WORK_AREA;
	byte** data = &buffer;
	BufferStatus* bufferStatus;
	unsigned int tenBitDivider, bytePerBufferCounter = 0, duration, adjustedDuration;
	unsigned long divider, adjustedFrequency, beatsForChannel = 0;
	boolean firstRun = TRUE, periodicSoundDetected = FALSE, noiseChHasVolume = FALSE;

	unsigned int totalCounter = 8;  // General purpose counter (usage unclear)

	totalBeats = 0;                 // Reset total beats counter

	bufferStatus = &localBufferStatus;

	// Initialize local buffer status for channel 0 sound data banked memory access
	localBufferStatus.bank = soundFile->soundBank;
	localBufferStatus.bankedData = soundFile->ch0;
	localBufferStatus.bufferCounter = 0;

	// Refresh local buffer with banked data for channel 0
	b5RefreshBuffer(&localBufferStatus);

	// Loop until end markers (0xFFFF) are found on all channels
	while (seenFFFFCounter != NO_CHANNELS)
	{
		GET_NEXT(readByte);  // Macro to get next byte from banked sound data

		// Only process if end marker has not been detected
		if (!endByteDetected)
		{
			// Check for potential channel end marker sequence (0xFF 0xFF)
			if (readByte == 0xFF && noteByteCounter == 0)
			{
				readAheadByte = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);

				if (readAheadByte == 0xFF)
				{
					endByteDetected = TRUE;  // Mark end of channel data encountered
				}
			}

			if (!endByteDetected)
			{
				if (noteByteCounter == DURATION_BYTE)
				{
					// Read two bytes of duration (little endian)
					*((byte*)&duration) = readByte;
					*((byte*)&duration + 1) = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);
					*((byte*)&duration) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];

					adjustedDuration = ((unsigned int)*((byte*)&duration)) | ((unsigned int)*((byte*)&duration + 1)) << 8;

					// Copy lookahead byte to buffer to maintain coherence
					b1CopyAhead(soundFile, bytePerBufferCounter, bufferStatus, *((byte*)&adjustedDuration + 1));

					// Accumulate beats for channel duration accounting
					beatsForChannel += adjustedDuration;
				}
				else if (noteByteCounter == FREQUENCY_BYTE)
				{
					// Handle the frequency bytes for tone channels, excluding noise channel (3)
					*((byte*)&tenBitDivider) = readByte;

					if (seenFFFFCounter != NOISE_CHANNEL)
					{
						// Read second frequency byte
						*((byte*)&tenBitDivider + 1) = b1ReadAhead(soundFile, bytePerBufferCounter, bufferStatus);
						*((byte*)&tenBitDivider) = GOLDEN_RAM_WORK_AREA[bytePerBufferCounter];

						// Compute linear frequency divider from frequency byte bits
						divider = ((*((byte*)&tenBitDivider) & 0x3F) << 4) + (*((byte*)&tenBitDivider + 1) & 0x0F) & 0xFFFF;

						// Convert divider into frequency using preset constant and convert to CX16 scale
						adjustedFrequency = ((FREQUENCY_NUMERATOR / divider) + 1) / 2;
						FREQ_TO_CX_16(adjustedFrequency);

						// Copy lookahead frequency byte back into buffer
						b1CopyAhead(soundFile, bytePerBufferCounter, bufferStatus, *((byte*)&adjustedFrequency + 1));
					}

					// Store low byte of adjusted frequency in local work area buffer
					GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = *((byte*)&adjustedFrequency);
				}
				else if (noteByteCounter == NOISE_FREQUENCY_BYTE && !periodicSoundDetected && seenFFFFCounter == 3)
				{
					// This detects if noise channel uses periodic noise type (bit 2 inverted)
					periodicSoundDetected = (~GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 4);
				}
				else if (noteByteCounter == VOLUME_BYTE)
				{
					if (adjustedDuration != 0xFFFF)
					{
						// Detect if noise channel volume is active for calculating periodic sound
						noiseChHasVolume = noiseChHasVolume || ((GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F) != 0xF) && seenFFFFCounter == 3;

						// Convert SN76489 volume to CX16 volume scale
						GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = SN76489_to_CX16_Volume[(GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] & 0x0F)];
					}
					else
					{
						// If duration is end marker, reset volume
						GOLDEN_RAM_WORK_AREA[bytePerBufferCounter] = 0;
					}
				}

				// Move to next byte within note
				noteByteCounter = (noteByteCounter + 1) % NO_NOTE_BYTES;
			}
		}
		else
		{
			// Reset end byte flag and move to next channel
			endByteDetected = FALSE;
			seenFFFFCounter++;
			noteByteCounter = 0;

			// Update totalBeats for the max duration across channels
			if (beatsForChannel > totalBeats)
			{
				totalBeats = beatsForChannel;
			}

			// Reset beats count for next channel
			beatsForChannel = 0;
		}

		// Increment buffer counter
		bytePerBufferCounter++;

		// If local buffer filled, copy back to banked data and reset counter
		if (bytePerBufferCounter == LOCAL_WORK_AREA_SIZE)
		{
			memCpyBanked(localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE);

			bytePerBufferCounter = 0;
		}
	}

	// Copy any remaining partial buffer data back to banked data
	if (bytePerBufferCounter)
	{
		memCpyBanked(localBufferStatus.bankedData + (localBufferStatus.bufferCounter - 1) * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, bytePerBufferCounter);
	}

	// If periodic sound is detected and noise channel has volume, perform precomputation for periodic sound.
	if (periodicSoundDetected && noiseChHasVolume)
	{
		b1PreComputePeriodicSound(&b1LoadedSounds[soundLoadCounter], soundChannelOffSets);
	}
}

// Loads a sound file given a sound number, converting and precomputing values, and preparing for playback.
void b1LoadSoundFile(int soundNum) {

	AGIFile tempAGI;               // Temporary AGI file structure for sound file code
	AGIFilePosType agiFilePosType;
	byte i;
	unsigned int soundChannelOffSets[NO_CHANNELS];  // Offsets to channel data in sound file

	// Gets logic directory entry for sound file number and loads AGI file
	b10GetLogicDirectory(&agiFilePosType, &snddir[soundNum]);
	b6LoadAGIFile(SOUND, &agiFilePosType, &tempAGI);

	// Save references to loaded sound resource data and bank info
	b1LoadedSounds[soundLoadCounter].soundResource = tempAGI.code;
	b1LoadedSounds[soundLoadCounter].soundBank = tempAGI.codeBank;
	b1LoadedSoundsPointer[soundNum] = &b1LoadedSounds[soundLoadCounter];
	totalSoundSize = tempAGI.totalSize;

	// Copy sound channel offsets from the beginning of code resource
	memCpyBanked((byte*)soundChannelOffSets, tempAGI.code, tempAGI.codeBank, NO_CHANNELS * 2);

	// Set channel pointers in loaded sound struct
	b1SetChannelOffsets(tempAGI.code, &b1LoadedSounds[soundLoadCounter], soundChannelOffSets);

	// Precompute frequency and volume values for loaded sound
	b1PrecomputeValues(&b1LoadedSounds[soundLoadCounter], soundChannelOffSets);

	// Increment sound load counter if space available
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

// Plays a loaded sound by initializing playback pointers and state.
void b1PlaySound(byte soundNum, byte endSoundFlag)
{
	byte testVal, i;
	unsigned int* ticksPointer;
	byte** channelPointer;

	asm("sei"); // Disable interrupts to initialize sound playback safely

	// Reset tick counters for all channels
	b1Ch1Ticks = 0;
	b1Ch2Ticks = 0;
	b1Ch3Ticks = 0;
	b1Ch4Ticks = 0;

	// Clear the end sound flag in the system flag array
	flag[endSoundFlag] = FALSE;

	// Set current playing note pointers for each channel, offset by NO_NOTE_BYTES bytes before actual note start
	ZP_CURRENTLY_PLAYING_NOTE_1 = b1LoadedSoundsPointer[soundNum]->ch0 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_2 = b1LoadedSoundsPointer[soundNum]->ch1 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_3 = b1LoadedSoundsPointer[soundNum]->ch2 - NO_NOTE_BYTES;
	ZP_CURRENTLY_PLAYING_NOTE_NOISE = b1LoadedSoundsPointer[soundNum]->chNoise - NO_NOTE_BYTES;

	// Set sound bank for playback
	b1SoundDataBank = b1LoadedSoundsPointer[soundNum]->soundBank;

	// Mark all channels as playing
	memset(b1IsPlaying, TRUE, NO_CHANNELS);
	b1ChannelsPlaying = NO_CHANNELS;

	// Store end sound flag for signaling when sound finishes
	b1EndSoundFlag = endSoundFlag;

	REENABLE_INTERRUPTS(); // Re-enable interrupts for playback
}

// Stops current playing sound, clears PSG state, and cancels playing flags.
void b1StopSound()
{
	asm("sei");         // Disable interrupts to safely stop sound
	b1PsgClear();       // Clear PSG sound generator state
	memset(b1IsPlaying, FALSE, NO_CHANNELS); // Mark all channels as not playing
	REENABLE_INTERRUPTS(); // Re-enable interrupts after sound stop
}

#pragma code-name (pop)