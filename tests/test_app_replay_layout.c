#include "logic_app_replay.h"
#include "logic_replay_runner.h"
#include <string.h>

#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "full-app layout regression:%d: %s\n", \
                __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int check_conversion(void)
{
    uint8_t raw[LOGIC_APP_REPLAY_OUTPUT_SIZE] = {0};
    uint8_t hash[32] = {1};
    LogicReplayManifest manifest = {0};
    LogicOracleSnapshot snapshot;
    char detail[128];
    uint8_t *object = raw + LOGIC_APP_REPLAY_OBJECTS_OFFSET;
    uint8_t *inventory = raw + LOGIC_APP_REPLAY_INVENTORY_OFFSET;
    uint8_t *message = raw + LOGIC_APP_REPLAY_MESSAGES_OFFSET;
    strcpy(manifest.engine, "KQ1");
    strcpy(manifest.rng_algorithm, LOGIC_APP_REPLAY_RNG_ALGORITHM);
    manifest.tick_numerator = 10;
    manifest.tick_denominator = 1;
    memcpy(raw, "X16R", 4);
    raw[4] = LOGIC_APP_REPLAY_SNAPSHOT_VERSION;
    raw[5] = raw[6] = 1;
    raw[15] = raw[LOGIC_APP_REPLAY_VARS_OFFSET] = 8;
    raw[LOGIC_APP_REPLAY_FLAGS_OFFSET + 9] = 1;
    raw[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET] = 1;
    object[0] = 3; object[1] = 1; object[3] = 42; object[5] = 99;
    object[7] = 2; object[8] = 4; object[9] = 6; object[11] = 1;
    raw[51] = 2;
    inventory[0] = 1; inventory[1] = 255;
    inventory[2] = 0; inventory[3] = 8;
    raw[46] = 1; message[0] = 7; message[1] = 3; message[2] = 5;
    memcpy(message + 3, "hello", 5);
#define CONVERT() logic_app_replay_snapshot_to_oracle("KQ1", 0x2917, \
    &manifest, hash, hash, raw, sizeof raw, &snapshot, detail, sizeof detail)
#define REJECT() do { CHECK(CONVERT() == -1); CHECK(detail[0]); \
    CHECK(snapshot.tick == 0 && snapshot.inventory_count == 0); } while (0)
    CHECK(CONVERT() == 0);
    CHECK(snapshot.room == 8 && snapshot.flags[9] == 1);
    CHECK(snapshot.object_count == 1 && snapshot.objects[0].id == 3);
    CHECK(snapshot.objects[0].x == 42 && snapshot.objects[0].y == 99);
    CHECK(snapshot.inventory_known && snapshot.inventory_count == 2);
    CHECK(snapshot.inventory_rooms[0] == 8 && snapshot.inventory_rooms[1] == 255);
    CHECK(snapshot.message_count == 1 && !strcmp(snapshot.messages[0].text, "hello"));
    /* Canonical messages retain observable print order and multiplicity. */
    memcpy(message + LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE, message,
           LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE);
    raw[46] = 2;
    CHECK(CONVERT() == 0);
    CHECK(snapshot.message_count == 2);
    message[LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE + 3] = 'j';
    CHECK(CONVERT() == 0);
    CHECK(!strcmp(snapshot.messages[1].text, "jello"));
    raw[46] = 1;
    CHECK(CONVERT() == 0);
    CHECK(snapshot.tick == 1 && snapshot.checkpoint_index == 0);
    CHECK(snapshot.control_flow_reliable);
    raw[41] = LOGIC_APP_REPLAY_TRACE_MAX;
    raw[42] = 17;
    raw[58] = LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED |
              LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    CHECK(CONVERT() == 0);
    CHECK(snapshot.control_flow_reliable && !snapshot.has_trace);
    CHECK(strstr(detail, "bounded") != NULL);
    raw[42] = 0; REJECT(); raw[42] = 17;
    raw[41]--; REJECT(); raw[41]++;
    raw[58] = LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED; REJECT();
    raw[58] |= LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] |= LOGIC_APP_REPLAY_CAPACITY_INVENTORY_TRUNCATED;
    REJECT();
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    raw[58] |= LOGIC_APP_REPLAY_STATUS_MESSAGE_OVERFLOW; REJECT();
    raw[58] = LOGIC_APP_REPLAY_STATUS_TRACE_DROPPED | LOGIC_APP_REPLAY_STATUS_CAPTURE_CAPACITY;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = 0; REJECT();
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = LOGIC_APP_REPLAY_CAPACITY_TRACE_DROPPED;
    raw[41] = raw[42] = raw[58] = 0;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = 0;
    CHECK(logic_app_replay_snapshot_to_oracle("KQ1", 0x2917, &manifest,
        hash, hash, raw, sizeof raw - 1, &snapshot, detail, sizeof detail) == -1);
    raw[58] = LOGIC_APP_REPLAY_STATUS_INPUT_WAIT_EXHAUSTED; REJECT(); raw[58] = 0;
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = 1; REJECT();
    raw[LOGIC_APP_REPLAY_CAPACITY_FLAGS_OFFSET] = 0;
    raw[51] = 0; raw[52] = 1; REJECT(); raw[51] = 2; raw[52] = 0;
    inventory[2] = 1; REJECT(); inventory[2] = 0;
    inventory[0] = 2; REJECT(); inventory[0] = 1;
    raw[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET] = LOGIC_APP_REPLAY_OBJECT_MAX + 1;
    REJECT(); raw[LOGIC_APP_REPLAY_VIEW_COUNT_OFFSET] = 1;
    object[0] = LOGIC_APP_REPLAY_OBJECT_MAX; REJECT(); object[0] = 3;
    raw[46] = LOGIC_APP_REPLAY_MESSAGE_MAX + 1; REJECT(); raw[46] = 1;
    raw[6] = 0; REJECT(); raw[6] = 1;
    raw[4]++; REJECT(); raw[4]--;
    raw[0] = 0; REJECT(); raw[0] = 'X';
    raw[LOGIC_APP_REPLAY_FLAGS_OFFSET + 9] = 2; REJECT();
    raw[LOGIC_APP_REPLAY_FLAGS_OFFSET + 9] = 1;
    memset(manifest.rng_algorithm, 'a', sizeof manifest.rng_algorithm); REJECT();
    strcpy(manifest.rng_algorithm, LOGIC_APP_REPLAY_RNG_ALGORITHM);
    manifest.tick_denominator = 0; REJECT(); manifest.tick_denominator = 1;
    manifest.rng_seed = 1; REJECT(); manifest.rng_seed = 0;
    memset(manifest.engine, 'a', sizeof manifest.engine); REJECT();
#undef REJECT
#undef CONVERT
    return 0;
}

int main(void)
{
    CHECK(check_conversion() == 0);
    CHECK(LOGIC_APP_REPLAY_TRACE_OFFSET ==
          LOGIC_APP_REPLAY_MESSAGES_OFFSET +
              LOGIC_APP_REPLAY_MESSAGE_MAX *
                  LOGIC_APP_REPLAY_MESSAGE_RECORD_SIZE);
    CHECK(LOGIC_APP_REPLAY_MESSAGE_TEXT_MAX == 255u);
    CHECK(LOGIC_APP_REPLAY_MESSAGES_OFFSET ==
          LOGIC_APP_REPLAY_INVENTORY_OFFSET +
              LOGIC_APP_REPLAY_INVENTORY_MAX *
                  LOGIC_APP_REPLAY_INVENTORY_RECORD_SIZE);
    CHECK(LOGIC_APP_REPLAY_PARSER_OFFSET ==
          LOGIC_APP_REPLAY_TRACE_OFFSET +
              LOGIC_APP_REPLAY_TRACE_MAX *
                  LOGIC_APP_REPLAY_TRACE_RECORD_SIZE);
    CHECK(LOGIC_APP_REPLAY_OUTPUT_SIZE ==
          LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET + 4u);
    CHECK(LOGIC_APP_REPLAY_SCHEDULE_FRAME_OFFSET ==
          LOGIC_APP_REPLAY_ELAPSED_VSYNC_OFFSET + 4u);
    return 0;
}
