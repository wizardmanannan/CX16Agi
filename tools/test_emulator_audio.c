/* Exercise the emulator's real producer, chip renderers, resampler and callback. */
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include "vera_pcm.h"
#include "vera_psg.h"

static SDL_AudioDeviceID startup_locked_audio_device;
static SDL_AudioCallback emulator_audio_callback;
static atomic_uint sdl_callback_count = ATOMIC_VAR_INIT(0);

static void count_sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    emulator_audio_callback(userdata, stream, len);
    atomic_fetch_add_explicit(&sdl_callback_count, 1, memory_order_relaxed);
}

static SDL_AudioDeviceID test_open_audio_device(const char *device, int iscapture,
    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes)
{
    SDL_AudioSpec observed = *desired;
    emulator_audio_callback = desired->callback;
    observed.callback = count_sdl_audio_callback;
    SDL_AudioDeviceID opened = SDL_OpenAudioDevice(device, iscapture, &observed,
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

#define MAX_MANUAL_CALLBACKS 1024
#define AUDIO_BUFFER_COUNT 32
/* The suite adapter primes half of this queue with silence before the tone. */
#define STARTUP_PRIMING_CALLBACKS (AUDIO_BUFFER_COUNT / 2)
#define NEGATIVE_CONTROL_CALLBACKS 64
#define AUDIO_SAMPLE_THRESHOLD 32
#define MIN_SAMPLE_DENSITY_PERCENT 75
#define MIN_ACTIVE_CALLBACK_PERCENT 98
#define MAX_SILENT_CALLBACK_RUN 2
#define MIN_SDL_CALLBACKS 3
#define PSG_FREQUENCY_INITIAL 0x1000
#define PSG_FREQUENCY_CHANGED 0x0800
#define PSG_SAMPLE_CLOCK_NUMERATOR 25000000ULL
#define PSG_SAMPLE_CLOCK_DENOMINATOR 512ULL
#define PSG_PHASE_PERIOD 131072ULL
#define MAX_TONE_CROSSING_ERROR_PERCENT 8
#define MIN_TONE_CALLBACKS 64
#define PSG_FREQUENCY_CHANGE_FRAME 90
#define PSG_CHANGE_SETTLE_CALLBACKS 2

extern void fx_video_space_write(uint32_t address, bool nibble, uint8_t value);

enum {
    AUDIO_ORACLE_SAMPLE_DENSITY = 1,
    AUDIO_ORACLE_CALLBACK_CONTINUITY = 2,
    AUDIO_ORACLE_FREQUENCY = 4,
    AUDIO_ORACLE_POLARITY = 8
};

typedef struct {
    unsigned callbacks;
    unsigned active_callbacks;
    unsigned sample_density_percent;
    unsigned active_callback_percent;
    unsigned longest_silent_run;
    uint64_t zero_crossings;
    unsigned estimated_frequency_hz;
    unsigned expected_frequency_hz;
    unsigned rising_steps;
    unsigned falling_steps;
    unsigned failures;
} AudioMetrics;

static unsigned callback_count;
static unsigned callback_underflows;
static int16_t manual_samples[MAX_MANUAL_CALLBACKS][SAMPLES_PER_BUFFER * 2];

static AudioMetrics analyze_audio(
    const int16_t samples[][SAMPLES_PER_BUFFER * 2], size_t count,
    uint16_t programmed_frequency)
{
    AudioMetrics metrics = {0};
    size_t active_sample_frames = 0;
    size_t active_frames = 0;
    size_t sample_frames = count * SAMPLES_PER_BUFFER;
    int64_t sample_sum = 0;
    unsigned silent_run = 0;

    metrics.callbacks = (unsigned)count;
    for (size_t callback = 0; callback < count; ++callback) {
        bool active = false;
        size_t callback_sample_frames = 0;
        for (unsigned frame = 0; frame < SAMPLES_PER_BUFFER; ++frame) {
            int32_t left = samples[callback][frame * 2];
            int32_t right = samples[callback][frame * 2 + 1];
            sample_sum += left;
            if (left < 0) left = -left;
            if (right < 0) right = -right;
            int32_t peak = left > right ? left : right;
            if (peak > AUDIO_SAMPLE_THRESHOLD) {
                active = true;
                callback_sample_frames++;
            }
        }
        if (active) {
            metrics.active_callbacks++;
            active_sample_frames += callback_sample_frames;
            active_frames += SAMPLES_PER_BUFFER;
            silent_run = 0;
        } else {
            silent_run++;
            if (silent_run > metrics.longest_silent_run) {
                metrics.longest_silent_run = silent_run;
            }
        }
    }

    if (active_frames) {
        metrics.sample_density_percent =
            (unsigned)(active_sample_frames * 100 / active_frames);
    }
    if (count) {
        metrics.active_callback_percent =
            (unsigned)((size_t)metrics.active_callbacks * 100 / count);
    }
    if (metrics.sample_density_percent < MIN_SAMPLE_DENSITY_PERCENT) {
        metrics.failures |= AUDIO_ORACLE_SAMPLE_DENSITY;
    }
    if (metrics.active_callback_percent < MIN_ACTIVE_CALLBACK_PERCENT ||
        metrics.longest_silent_run > MAX_SILENT_CALLBACK_RUN) {
        metrics.failures |= AUDIO_ORACLE_CALLBACK_CONTINUITY;
    }
    if (programmed_frequency && sample_frames) {
        int32_t mean = (int32_t)(sample_sum / (int64_t)sample_frames);
        int previous_sign = 0;
        int32_t previous_sample = samples[0][0];
        for (size_t frame = 0; frame < sample_frames; ++frame) {
            size_t callback = frame / SAMPLES_PER_BUFFER;
            size_t offset = (frame % SAMPLES_PER_BUFFER) * 2;
            int32_t sample = samples[callback][offset];
            int32_t centered = sample - mean;
            int sign = centered > 0 ? 1 : centered < 0 ? -1 : 0;
            if (sign && previous_sign && sign != previous_sign) {
                metrics.zero_crossings++;
            }
            if (sign) previous_sign = sign;

            int32_t delta = sample - previous_sample;
            if (delta > 8) metrics.rising_steps++;
            if (delta < -8) metrics.falling_steps++;
            previous_sample = sample;
        }

        if (host_sample_rate > 0) {
            uint64_t expected_crossings = (uint64_t)sample_frames * 2 *
                programmed_frequency * PSG_SAMPLE_CLOCK_NUMERATOR /
                (PSG_PHASE_PERIOD * PSG_SAMPLE_CLOCK_DENOMINATOR * host_sample_rate);
            uint64_t difference = metrics.zero_crossings > expected_crossings ?
                metrics.zero_crossings - expected_crossings :
                expected_crossings - metrics.zero_crossings;
            metrics.estimated_frequency_hz = (unsigned)(
                metrics.zero_crossings * host_sample_rate / (2 * sample_frames));
            metrics.expected_frequency_hz = (unsigned)(
                (uint64_t)programmed_frequency * PSG_SAMPLE_CLOCK_NUMERATOR /
                (PSG_PHASE_PERIOD * PSG_SAMPLE_CLOCK_DENOMINATOR));
            if (!expected_crossings ||
                difference * 100 > expected_crossings * MAX_TONE_CROSSING_ERROR_PERCENT) {
                metrics.failures |= AUDIO_ORACLE_FREQUENCY;
            }
        } else {
            metrics.failures |= AUDIO_ORACLE_FREQUENCY;
        }
        /* Sawtooth output falls as the VERA phase advances; this checks polarity without modeling samples. */
        if (metrics.falling_steps <= metrics.rising_steps) {
            metrics.failures |= AUDIO_ORACLE_POLARITY;
        }
    }
    return metrics;
}

static bool build_negative_controls(void)
{
    int16_t steady[NEGATIVE_CONTROL_CALLBACKS][SAMPLES_PER_BUFFER * 2];
    int16_t control[NEGATIVE_CONTROL_CALLBACKS][SAMPLES_PER_BUFFER * 2];
    size_t captured = 0;
    int64_t absolute_sum = 0;

    for (size_t i = STARTUP_PRIMING_CALLBACKS;
         i < callback_count && captured < NEGATIVE_CONTROL_CALLBACKS; ++i) {
        int32_t peak = 0;
        for (unsigned sample = 0; sample < SAMPLES_PER_BUFFER * 2; ++sample) {
            int32_t value = manual_samples[i][sample];
            if (value < 0) value = -value;
            if (value > peak) peak = value;
        }
        if (peak > AUDIO_SAMPLE_THRESHOLD) {
            memcpy(steady[captured], manual_samples[i], sizeof(steady[captured]));
            for (unsigned sample = 0; sample < SAMPLES_PER_BUFFER * 2; ++sample) {
                int32_t value = steady[captured][sample];
                absolute_sum += value < 0 ? -value : value;
            }
            captured++;
        }
    }
    if (captured != NEGATIVE_CONTROL_CALLBACKS) {
        fprintf(stderr, "Could not capture %u real steady-state callbacks for negative controls\n",
            NEGATIVE_CONTROL_CALLBACKS);
        return false;
    }

    memcpy(control, steady, sizeof(steady));
    for (size_t callback = 0; callback < NEGATIVE_CONTROL_CALLBACKS; ++callback) {
        int32_t peak = 0;
        unsigned peak_frame = 0;
        for (unsigned frame = 0; frame < SAMPLES_PER_BUFFER; ++frame) {
            int32_t left = steady[callback][frame * 2];
            int32_t right = steady[callback][frame * 2 + 1];
            if (left < 0) left = -left;
            if (right < 0) right = -right;
            int32_t frame_peak = left > right ? left : right;
            if (frame_peak > peak) {
                peak = frame_peak;
                peak_frame = frame;
            }
        }
        int16_t left = steady[callback][peak_frame * 2];
        int16_t right = steady[callback][peak_frame * 2 + 1];
        memset(control[callback], 0, sizeof(control[callback]));
        control[callback][peak_frame * 2] = left;
        control[callback][peak_frame * 2 + 1] = right;
    }

    AudioMetrics click = analyze_audio(control, NEGATIVE_CONTROL_CALLBACKS,
        PSG_FREQUENCY_INITIAL);
    if (!(click.failures & AUDIO_ORACLE_SAMPLE_DENSITY) ||
        (click.failures & AUDIO_ORACLE_CALLBACK_CONTINUITY)) {
        fprintf(stderr, "Click-only control failed for wrong reasons: mask=%u density=%u%% active=%u%%\n",
            click.failures, click.sample_density_percent, click.active_callback_percent);
        return false;
    }

    memcpy(control, steady, sizeof(steady));
    for (size_t callback = 1; callback < NEGATIVE_CONTROL_CALLBACKS; callback += 2) {
        memset(control[callback], 0, sizeof(control[callback]));
    }
    AudioMetrics half = analyze_audio(control, NEGATIVE_CONTROL_CALLBACKS,
        PSG_FREQUENCY_INITIAL);
    if (!(half.failures & AUDIO_ORACLE_CALLBACK_CONTINUITY) ||
        (half.failures & AUDIO_ORACLE_SAMPLE_DENSITY)) {
        fprintf(stderr, "Half-silent control failed for wrong reasons: mask=%u density=%u%% active=%u%%\n",
            half.failures, half.sample_density_percent, half.active_callback_percent);
        return false;
    }

    int16_t dc_level = (int16_t)(absolute_sum /
        (NEGATIVE_CONTROL_CALLBACKS * SAMPLES_PER_BUFFER * 2));
    for (size_t callback = 0; callback < NEGATIVE_CONTROL_CALLBACKS; ++callback) {
        for (unsigned sample = 0; sample < SAMPLES_PER_BUFFER * 2; ++sample) {
            control[callback][sample] = dc_level;
        }
    }
    AudioMetrics nonzero_dc = analyze_audio(control, NEGATIVE_CONTROL_CALLBACKS,
        PSG_FREQUENCY_INITIAL);
    if (!(nonzero_dc.failures & AUDIO_ORACLE_FREQUENCY) ||
        (nonzero_dc.failures & (AUDIO_ORACLE_SAMPLE_DENSITY | AUDIO_ORACLE_CALLBACK_CONTINUITY))) {
        fprintf(stderr, "Nonzero DC control failed for wrong reasons: mask=%u level=%d density=%u%% active=%u%%\n",
            nonzero_dc.failures, dc_level, nonzero_dc.sample_density_percent,
            nonzero_dc.active_callback_percent);
        return false;
    }

    size_t control_frames = NEGATIVE_CONTROL_CALLBACKS * SAMPLES_PER_BUFFER;
    for (size_t frame = 0; frame < control_frames; ++frame) {
        size_t source_frame = frame / 2;
        size_t source_callback = source_frame / SAMPLES_PER_BUFFER;
        size_t source_offset = (source_frame % SAMPLES_PER_BUFFER) * 2;
        size_t output_callback = frame / SAMPLES_PER_BUFFER;
        size_t output_offset = (frame % SAMPLES_PER_BUFFER) * 2;
        control[output_callback][output_offset] = steady[source_callback][source_offset];
        control[output_callback][output_offset + 1] = steady[source_callback][source_offset + 1];
    }
    AudioMetrics wrong_frequency = analyze_audio(control, NEGATIVE_CONTROL_CALLBACKS,
        PSG_FREQUENCY_INITIAL);
    if (!(wrong_frequency.failures & AUDIO_ORACLE_FREQUENCY) ||
        (wrong_frequency.failures & (AUDIO_ORACLE_SAMPLE_DENSITY |
            AUDIO_ORACLE_CALLBACK_CONTINUITY | AUDIO_ORACLE_POLARITY))) {
        fprintf(stderr, "Wrong-frequency control failed for wrong reasons: mask=%u measured=%u Hz expected=%u Hz\n",
            wrong_frequency.failures, wrong_frequency.estimated_frequency_hz,
            wrong_frequency.expected_frequency_hz);
        return false;
    }

    puts("Audio oracle controls: click-only rejected for density, half-silent for continuity, DC and wrong tone for frequency");
    return true;
}

static bool callback_delivery_observed(unsigned callbacks)
{
    return callbacks != 0;
}

static bool test_real_sdl_callback_delivery(void)
{
    SDL_PauseAudioDevice(audio_dev, 1);
    atomic_store_explicit(&sdl_callback_count, 0, memory_order_relaxed);
    SDL_Delay(50);
    unsigned paused_delivered = atomic_load_explicit(&sdl_callback_count, memory_order_relaxed);
    if (paused_delivered != 0 || callback_delivery_observed(paused_delivered)) {
        fprintf(stderr, "Paused SDL device delivered %u callback(s) in its bounded negative control\n",
            paused_delivered);
        return false;
    }
    puts("SDL paused-device control: observed zero callbacks in 50 ms; delivery decision rejected it");

    atomic_store_explicit(&sdl_callback_count, 0, memory_order_relaxed);
    SDL_PauseAudioDevice(audio_dev, 0);
    unsigned waited_ms = 0;
    unsigned delivered = 0;
    while (waited_ms < 1000 && delivered < MIN_SDL_CALLBACKS) {
        SDL_Delay(5);
        waited_ms += 5;
        delivered = atomic_load_explicit(&sdl_callback_count, memory_order_relaxed);
    }
    SDL_PauseAudioDevice(audio_dev, 1);

    printf("Real SDL dummy callback thread: %u callbacks within %u ms\n",
        delivered, waited_ms);
    if (!callback_delivery_observed(delivered) || delivered < MIN_SDL_CALLBACKS) {
        fprintf(stderr, "SDL dummy audio thread delivered fewer than %u callbacks within the bound\n",
            MIN_SDL_CALLBACKS);
        return false;
    }
    return true;
}

static void service_callbacks(unsigned frame)
{
    const unsigned due = (uint64_t)frame * AUDIO_SAMPLERATE /
        (60 * SAMPLES_PER_BUFFER);
    int16_t samples[SAMPLES_PER_BUFFER * 2];

    while (callback_count < due) {
        if (callback_count >= MAX_MANUAL_CALLBACKS) {
            fputs("Manual callback capture exceeded its fixed bound\n", stderr);
            exit(EXIT_FAILURE);
        }
        if (buffer_written < SAMPLES_PER_BUFFER * 2) callback_underflows++;
        audio_callback(NULL, (Uint8 *)samples, sizeof(samples));
        memcpy(manual_samples[callback_count], samples, sizeof(samples));
        callback_count++;
    }
}

int main(void)
{
    uint32_t pending_clocks = 0;
    uint32_t fractional_cycles = 0;
    size_t queued_callbacks_at_change = 0;
    size_t frequency_change_capture_callback = 0;
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

    audio_init(NULL, AUDIO_BUFFER_COUNT);
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
    fx_video_space_write(0x1f9c0, false, 0x00);
    fx_video_space_write(0x1f9c1, false, PSG_FREQUENCY_INITIAL >> 8);
    fx_video_space_write(0x1f9c2, false, 0xff);
    fx_video_space_write(0x1f9c3, false, 0x40);
    pcm_reset();

    for (unsigned frame = 1; frame <= 180; ++frame) {
        fractional_cycles += 8000000;
        uint32_t frame_clocks = fractional_cycles / 60;
        fractional_cycles %= 60;
        pending_clocks += frame_clocks;
        clockticks6502 += frame_clocks;

        if (frame == PSG_FREQUENCY_CHANGE_FRAME) {
            const size_t callback_samples = SAMPLES_PER_BUFFER * 2;
            queued_callbacks_at_change = ((size_t)buffer_written + callback_samples - 1) /
                callback_samples;
            frequency_change_capture_callback = callback_count + queued_callbacks_at_change +
                PSG_CHANGE_SETTLE_CALLBACKS;
            fx_video_space_write(0x1f9c0, false, PSG_FREQUENCY_CHANGED & 0xff);
            fx_video_space_write(0x1f9c1, false, PSG_FREQUENCY_CHANGED >> 8);
        }

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

    printf("Real emulator audio: 180 frames, %u callbacks, %u queue underruns\n",
        callback_count, callback_underflows);

    if (callback_count < 500 || callback_count <= STARTUP_PRIMING_CALLBACKS) {
        fputs("Real emulator audio producer did not deliver enough steady-state callbacks\n", stderr);
        goto cleanup;
    }
    AudioMetrics steady = analyze_audio(manual_samples + STARTUP_PRIMING_CALLBACKS,
        callback_count - STARTUP_PRIMING_CALLBACKS, 0);
    printf("Steady-state waveform after %u priming callbacks: %u/%u callbacks active "
        "(%u%%), sample density=%u%%, longest silent run=%u callback(s)\n",
        STARTUP_PRIMING_CALLBACKS, steady.active_callbacks, steady.callbacks,
        steady.active_callback_percent, steady.sample_density_percent,
        steady.longest_silent_run);
    if (steady.failures) {
        fprintf(stderr, "Programmed PSG tone failed waveform oracle: mask=%u (density=%u%%, callback continuity=%u%%)\n",
            steady.failures, steady.sample_density_percent, steady.active_callback_percent);
        goto cleanup;
    }
    if (frequency_change_capture_callback <= STARTUP_PRIMING_CALLBACKS ||
        frequency_change_capture_callback > callback_count ||
        frequency_change_capture_callback - STARTUP_PRIMING_CALLBACKS < MIN_TONE_CALLBACKS ||
        callback_count - frequency_change_capture_callback < MIN_TONE_CALLBACKS) {
        fprintf(stderr, "PSG frequency change left too few stable callbacks: boundary=%zu captured=%u\n",
            frequency_change_capture_callback, callback_count);
        goto cleanup;
    }
    AudioMetrics initial_tone = analyze_audio(manual_samples + STARTUP_PRIMING_CALLBACKS,
        MIN_TONE_CALLBACKS, PSG_FREQUENCY_INITIAL);
    size_t changed_callbacks = callback_count - frequency_change_capture_callback;
    AudioMetrics changed_tone = analyze_audio(manual_samples + frequency_change_capture_callback,
        changed_callbacks, PSG_FREQUENCY_CHANGED);
    printf("PSG tone frequency: 0x%04x -> measured %u Hz (expected %u), "
        "%llu crossings; 0x%04x -> measured %u Hz (expected %u), %llu crossings "
        "after %zu queued + %u settling callbacks; falling/rising steps %u/%u and %u/%u\n",
        PSG_FREQUENCY_INITIAL, initial_tone.estimated_frequency_hz,
        initial_tone.expected_frequency_hz, (unsigned long long)initial_tone.zero_crossings,
        PSG_FREQUENCY_CHANGED, changed_tone.estimated_frequency_hz,
        changed_tone.expected_frequency_hz, (unsigned long long)changed_tone.zero_crossings,
        queued_callbacks_at_change, PSG_CHANGE_SETTLE_CALLBACKS,
        initial_tone.falling_steps, initial_tone.rising_steps,
        changed_tone.falling_steps, changed_tone.rising_steps);
    if (((initial_tone.failures | changed_tone.failures) &
            (AUDIO_ORACLE_FREQUENCY | AUDIO_ORACLE_POLARITY)) ||
        initial_tone.estimated_frequency_hz == changed_tone.estimated_frequency_hz) {
        fprintf(stderr, "Programmed PSG tone failed frequency/polarity oracle: initial=%u changed=%u\n",
            initial_tone.failures, changed_tone.failures);
        goto cleanup;
    }
    bool controls_passed = build_negative_controls();
    bool sdl_delivery_passed = test_real_sdl_callback_delivery();
    if (!controls_passed || !sdl_delivery_passed) goto cleanup;
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
