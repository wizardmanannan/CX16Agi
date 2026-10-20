#ifndef CX16AGI_X16_REPLAY_RUNNER_H
#define CX16AGI_X16_REPLAY_RUNNER_H

#include "logic_replay_runner.h"
#include "logic_app_replay.h"

typedef struct LogicAppReplayInput {
    /* VERA VSYNC frame offset from completed application boot. */
    uint32_t frame;
    uint8_t petscii;
} LogicAppReplayInput;

/* Drives the replay target through the X16 emulator testbench. The target
 * exports the labels documented by x16_replay_runner.c. */
int logic_replay_x16_run(const LogicReplayManifest *manifest,
                         const LogicReplayEvent *events, size_t event_count,
                         const char *resource_dir, uint32_t timeout_ms,
                         unsigned fault_flags, LogicReplayResult *result);

int logic_replay_x16_run_ticks(const LogicReplayManifest *manifest,
                               const LogicReplayEvent *events,
                               size_t event_count, const char *resource_dir,
                               uint32_t tick_count, uint32_t timeout_ms,
                               unsigned fault_flags,
                               LogicReplayResult *result);

/* Runs the regular CX16 application image in the normal, video-enabled
 * emulator against a staged HostFS game directory. Test-target request and
 * checkpoint files bound the real VSync-paced application loop. */
int logic_app_replay_x16_run(const char *game_directory, uint16_t seed,
                             const LogicAppReplayInput *events,
                             size_t event_count, uint32_t tick_count,
                             uint32_t timeout_ms, uint8_t fault_flags,
                             uint8_t output[LOGIC_APP_REPLAY_OUTPUT_SIZE],
                             char *detail, size_t detail_capacity);

/* Capture each completed cycle from one clean production application boot. */
int logic_app_replay_x16_series(const char *game_directory, uint16_t seed,
    const LogicAppReplayInput *events, size_t event_count, uint32_t ticks,
    uint32_t timeout_ms, uint8_t fault_flags,
    uint8_t snapshots[][LOGIC_APP_REPLAY_OUTPUT_SIZE],
    size_t capacity, char *detail, size_t detail_capacity);

#endif
