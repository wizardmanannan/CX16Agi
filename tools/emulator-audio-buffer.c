/* Suite emulator adapter: keep the upstream audio engine and prime its queue
 * before its first device callback. Buffer capacity alone provides no headroom.
 */
#include <SDL.h>

static void host_audio_pause(SDL_AudioDeviceID device, int paused)
{
    SDL_PauseAudioDevice(device, paused);
}

static void buffered_audio_pause(SDL_AudioDeviceID device, int paused);
#ifdef SDL_PauseAudioDevice
#undef SDL_PauseAudioDevice
#endif
#define SDL_PauseAudioDevice buffered_audio_pause
#ifndef CX16_UPSTREAM_AUDIO
#define CX16_UPSTREAM_AUDIO "audio-upstream.c"
#endif
#include CX16_UPSTREAM_AUDIO
#undef SDL_PauseAudioDevice

static void buffered_audio_pause(SDL_AudioDeviceID device, int paused)
{
    if (!paused) {
        memset(buffer, 0, buffer_size * sizeof(*buffer));
        wridx = buffer_size / 2;
        buffer_written = wridx;
    }
    host_audio_pause(device, paused);
}
