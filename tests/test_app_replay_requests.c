#include "x16_replay_runner.h"

#include <stdio.h>
#include <string.h>

/* Exercise the public runner, rather than duplicating its input validator.
 * Invalid requests must stop before reading labels or starting an emulator.
 * No files or subprocesses are needed by these cases. */
static int rejects(const LogicAppReplayInput *events, size_t count,
                   uint32_t ticks, uint32_t timeout, uint8_t faults,
                   const char *diagnostic)
{
    uint8_t checkpoint[LOGIC_APP_REPLAY_OUTPUT_SIZE];
    char detail[256];
    size_t i;
    memset(checkpoint, 0xa5, sizeof checkpoint);
    memset(detail, 0xa5, sizeof detail);
    if (logic_app_replay_x16_run("unused-game", 1, events, count, ticks,
                                timeout, faults, checkpoint, detail,
                                sizeof detail) != -1 ||
        !strstr(detail, diagnostic)) {
        fprintf(stderr, "app request regression: expected '%s', got '%s'\n",
                diagnostic, detail);
        return 1;
    }
    /* A rejected request cannot leave stale successful snapshot data. */
    for (i = 0; i < sizeof checkpoint; ++i)
        if (checkpoint[i] != 0) {
            fprintf(stderr, "app request regression: stale snapshot byte %zu\n", i);
            return 1;
        }
    return 0;
}

int main(void)
{
    LogicAppReplayInput events[LOGIC_APP_REPLAY_INPUT_MAX + 1];
    size_t i;
    int failed = 0;
    for (i = 0; i < sizeof events / sizeof events[0]; ++i) {
        events[i].frame = (uint32_t)i + 1;
        events[i].petscii = 'L';
    }
    failed |= rejects(events, LOGIC_APP_REPLAY_INPUT_MAX + 1, 1, 1000, 0,
                      "capacity exceeded");
    failed |= rejects(NULL, 1, 1, 1000, 0, "invalid full-app replay arguments");
    failed |= rejects(events, 1, 1, 0, 0, "invalid full-app replay arguments");
    failed |= rejects(events, 1, 1, 1000, 0x80,
                      "invalid full-app replay arguments");
    events[0].frame = 0;
    failed |= rejects(events, 1, 1, 1000, 0, "invalid semantic input event 0");
    events[0].frame = UINT32_MAX;
    failed |= rejects(events, 1, 1, 1000, 0, "invalid semantic input event 0");
    events[0].frame = 2;
    events[1].frame = 1;
    failed |= rejects(events, 2, 1, 1000, 0, "invalid semantic input event 1");
    events[0].frame = 1;
    events[0].petscii = 0;
    failed |= rejects(events, 1, 1, 1000, 0, "invalid semantic input event 0");
    events[0].petscii = 0xff;
    failed |= rejects(events, 1, 1, 1000, 0, "invalid semantic input event 0");
    events[0].petscii = 'L';
    events[0].frame = UINT16_MAX;
    failed |= rejects(events, 1, 1, 1000, 0, "virtual frame range exceeds");
    {
        uint8_t series[2][LOGIC_APP_REPLAY_OUTPUT_SIZE];
        char detail[256];
        memset(series, 0xa5, sizeof series);
        if (logic_app_replay_x16_series("unused-game", 1, NULL, 0, 3,
                1000, 0, series, 2, detail, sizeof detail) != -1 ||
            !strstr(detail, "series capacity")) failed = 1;
        for (i = 0; i < sizeof series; ++i)
            if (((uint8_t *)series)[i]) failed = 1;
        if (logic_app_replay_x16_series("unused-game", 1, NULL, 0, 0,
                1000, 0, series, 2, detail, sizeof detail) != -1 ||
            logic_app_replay_x16_series("unused-game", 1, NULL, 0, 1,
                1000, 0, NULL, 2, detail, sizeof detail) != -1) failed = 1;
    }
    if (!failed)
        puts("full-app request capacity and malformed-input checks passed");
    return failed;
}
