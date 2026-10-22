/* Exercise the emulator's real producer, chip renderers, resampler and callback. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include "vera_pcm.h"
#include "vera_psg.h"

static SDL_AudioDeviceID startup_locked_audio_device;

static SDL_AudioDeviceID test_open_audio_device(const char *device, int iscapture,
    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes)
{
    SDL_AudioDeviceID opened = SDL_OpenAudioDevice(device, iscapture, desired,
        obtained, allowed_changes);
    if (opened) {
        SDL_LockAudioDevice(opened);
        startup_locked_audio_device = opened;
    }
    return opened;
}

#define SDL_OpenAudioDevice test_open_audio_device
#ifdef CX16_AUDIO_BUFFERED_TEST
#define CX16_UPSTREAM_AUDIO <audio.c>
#include "emulator-audio-buffer.c"
#else
#include <audio.c>
#endif
#undef SDL_OpenAudioDevice

uint8_t MHZ = 8;
uint32_t clockticks6502 = 0;

static unsigned callback_count;
static unsigned callback_underflows;
static unsigned audible_callbacks;

static void service_callbacks(unsigned frame)
{
    const unsigned due = (uint64_t)frame * AUDIO_SAMPLERATE /
        (60 * SAMPLES_PER_BUFFER);
    int16_t samples[SAMPLES_PER_BUFFER * 2];

    while (callback_count < due) {
        int32_t peak = 0;
        if (buffer_written < SAMPLES_PER_BUFFER * 2) callback_underflows++;
        audio_callback(NULL, (Uint8 *)samples, sizeof(samples));
        for (unsigned i = 0; i < SAMPLES_PER_BUFFER * 2; ++i) {
            int32_t value = samples[i];
            if (value < 0) value = -value;
            if (value > peak) peak = value;
        }
        if (peak > 32) audible_callbacks++;
        callback_count++;
    }
}

int main(void)
{
    uint32_t pending_clocks = 0;
    uint32_t fractional_cycles = 0;
    bool audio_initialized = false;
    const char *audio_driver;
    int result = EXIT_FAILURE;

    if (SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) != 0) {
        fputs("Could not select SDL dummy audio driver\n", stderr);
        return EXIT_FAILURE;
    }
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    audio_driver = SDL_GetCurrentAudioDriver();
    if (!audio_driver || strcmp(audio_driver, "dummy") != 0) {
        fputs("SDL did not select the dummy audio driver\n", stderr);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    audio_init(NULL, 32);
    audio_initialized = audio_dev != 0;
    if (!audio_initialized) {
        if (startup_locked_audio_device) {
            SDL_UnlockAudioDevice(startup_locked_audio_device);
            startup_locked_audio_device = 0;
        }
        fputs("Emulator did not open an SDL audio device\n", stderr);
        goto cleanup;
    }
    if (startup_locked_audio_device != audio_dev) {
        fputs("Emulator audio device was not held during initialization\n", stderr);
        goto cleanup;
    }
    SDL_PauseAudioDevice(audio_dev, 1);
    SDL_UnlockAudioDevice(audio_dev);
    startup_locked_audio_device = 0;

    psg_reset();
    psg_writereg(0, 0x00);
    psg_writereg(1, 0x10);
    psg_writereg(2, 0xff);
    psg_writereg(3, 0x40);
    pcm_reset();

    for (unsigned frame = 1; frame <= 180; ++frame) {
        fractional_cycles += 8000000;
        uint32_t frame_clocks = fractional_cycles / 60;
        fractional_cycles %= 60;
        pending_clocks += frame_clocks;
        clockticks6502 += frame_clocks;

        /* Delay one producer frame every third second while host callbacks continue. */
        if (frame % 20 == 10) {
            service_callbacks(frame);
            continue;
        }

        audio_step((int)pending_clocks);
        audio_render();
        pending_clocks = 0;
        service_callbacks(frame);
    }

    printf("Real emulator audio: 180 frames, %u callbacks, %u queue underruns, "
        "%u audible callbacks\n", callback_count, callback_underflows, audible_callbacks);

    if (callback_count < 500 || audible_callbacks * 2 < callback_count) {
        fputs("Real emulator audio producer did not sustain audible PSG output\n", stderr);
        goto cleanup;
    }
    if (callback_underflows) {
        fprintf(stderr, "Audio callback exhausted the producer queue %u times during bounded jitter\n",
            callback_underflows);
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (audio_initialized) audio_close();
    SDL_Quit();
    return result;
}
