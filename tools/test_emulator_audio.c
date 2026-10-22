/* Run the actual emulator audio producer/callback with a deterministic host clock. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_OpenAudioDevice test_audio_open
#define SDL_CloseAudioDevice test_audio_close
#define SDL_PauseAudioDevice test_audio_pause
#define SDL_LockAudioDevice test_audio_lock
#define SDL_UnlockAudioDevice test_audio_unlock
#define CX16_UPSTREAM_AUDIO <audio.c>
#include "emulator-audio-buffer.c"

uint8_t MHZ = 8;
uint32_t clockticks6502 = 0;

int tas_capture_audio_samples(const TasAudioChunk *chunk)
{
    (void)chunk;
    return 0;
}

SDL_AudioDeviceID test_audio_open(const char *name, int capture,
    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int changes)
{
    (void)name; (void)capture; (void)changes;
    *obtained = *desired;
    return 1;
}
void test_audio_close(SDL_AudioDeviceID device) { (void)device; }
void test_audio_pause(SDL_AudioDeviceID device, int paused) { (void)device; (void)paused; }
void test_audio_lock(SDL_AudioDeviceID device) { (void)device; }
void test_audio_unlock(SDL_AudioDeviceID device) { (void)device; }
void YM_Create(int clock) { (void)clock; }
void YM_init(int rate, int frames) { (void)rate; (void)frames; }
void YM_stream_update(uint16_t *out, uint32_t count) { memset(out, 0, count * 4); }
void psg_render(int16_t *out, unsigned count)
{
    for (unsigned i = 0; i < count * 2; ++i) out[i] = 6000;
}
void pcm_render(int16_t *out, unsigned count) { memset(out, 0, count * 4); }
void midi_synth_render(int16_t *out, int count) { memset(out, 0, (size_t)count * 4); }
void wav_recorder_process(const int16_t *samples, int count) { (void)samples; (void)count; }

int main(void)
{
    unsigned callbacks = 0, gaps = 0, pending_clocks = 0;
    int16_t samples[SAMPLES_PER_BUFFER * 2];
    audio_init(NULL, 32);
    for (unsigned frame = 1; frame <= 180; ++frame) {
        pending_clocks += 8000000 / 60;
        /* One late video frame every third second, then the producer catches up. */
        if (frame % 20 != 10) {
            audio_step((int)pending_clocks);
            audio_render();
            pending_clocks = 0;
        }
        unsigned due = (uint64_t)frame * AUDIO_SAMPLERATE / (60 * SAMPLES_PER_BUFFER);
        while (callbacks < due) {
            memset(samples, 0x5A, sizeof(samples));
            audio_callback(NULL, (Uint8 *)samples, sizeof(samples));
            ++callbacks;
            if (frame > 30) {
                for (unsigned i = 0; i < SAMPLES_PER_BUFFER * 2; ++i)
                    if (!samples[i]) { ++gaps; break; }
            }
        }
    }
    audio_close();
    printf("Native audio jitter workload: 180 frames, %u callbacks, %u audible gaps\n", callbacks, gaps);
    if (gaps) {
        fputs("Audio callback inserted silence during bounded host jitter\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
