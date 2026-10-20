#define FORBIDDEN_SYMBOL_EXCEPTION_fprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "logic_oracle.h"
#include "logic_replay.h"
}

/* The null backend has no public class declaration. Include it before any
 * ScummVM header installs forbidden-symbol macros, so its system headers are
 * parsed in their intended order. The timed backend below derives from this
 * actual ScummVM null backend and changes only the clock and event feed.
 */
#define main logic_oracle_null_backend_main
#include "backends/platform/null/null.cpp"
#undef main

#include "agi/agi.h"
#include "agi/detection.h"
#include "agi/text.h"
#include "agi/words.h"
#include "base/commandLine.h"
#include "base/version.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/random.h"
#include "common/system.h"
#include "engines/advancedDetector.h"
#include "engines/game.h"
#include "engines/metaengine.h"
#include "base/plugins.h"

extern PluginObject *g_AGI_DETECTION_getObject();
extern PluginObject *g_AGI_getObject();

namespace {

static uint32 scrambleRandomSeed(uint32 seed) {
    seed ^= seed >> 13;
    seed ^= seed << 21;
    seed ^= seed >> 11;
    return seed;
}

/* RandomSource exposes its state, not a call counter. Count actual state
 * transitions between checkpoints; no manifest expectation is used to invent
 * or constrain reference-engine RNG calls.
 */
static bool countRandomCalls(uint32 before, uint32 after, uint32 &count) {
    for (count = 0; count < 65536; ++count) {
        if (before == after)
            return true;
        before = scrambleRandomSeed(before);
    }
    return before == after;
}

static bool parseUnsigned(const char *text, uint64 &value) {
    char *end = nullptr;
    errno = 0;
    unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno || !text[0] || !end || *end)
        return false;
    value = static_cast<uint64>(parsed);
    return true;
}

static bool validateInputEvents(const LogicReplayEvent *events, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const LogicReplayEvent &event = events[i];
        /* Input timestamps and completed-cycle counts use different units;
           later events remain queued when capturing an earlier prefix. */
        if (!event.tick || event.kind != LOGIC_TRACE_KIND_MARK ||
            event.logic != 0 || (i && event.tick < events[i - 1].tick)) {
            std::fprintf(stderr, "unsupported replay input at event %lu\n",
                         static_cast<unsigned long>(i));
            return false;
        }
        if (event.reason == LOGIC_REPLAY_REASON_RNG) {
            /* This is a candidate-side expected result, not an oracle input. */
            if (event.before != 0 || event.after > 255) {
                std::fprintf(stderr, "invalid RNG expectation at event %lu\n",
                             static_cast<unsigned long>(i));
                return false;
            }
        } else if (event.reason != LOGIC_REPLAY_REASON_INPUT_DOWN &&
                   event.reason != LOGIC_REPLAY_REASON_INPUT_UP) {
            std::fprintf(stderr, "unsupported replay input at event %lu\n",
                         static_cast<unsigned long>(i));
            return false;
        }
    }
    return true;
}

static bool captureSnapshot(Agi::AgiEngine &engine,
                            const LogicReplayManifest &manifest,
                            const uint8_t manifestHash[LOGIC_REPLAY_SHA256_SIZE],
                            const uint8_t inputHash[LOGIC_REPLAY_SHA256_SIZE],
                            const char *gameId, uint16 detectedAgiVersion,
                            bool versionOverridden, bool completedCycle, uint64 tick,
                            uint64 wallMilliseconds,
                            uint32 rngSeed, uint32 rngCallCount,
                            const LogicOracleMessage *printedMessages,
                            uint16 printedMessageCount, LogicOracleSnapshot &snapshot) {
    const Agi::AgiGame &game = engine._game;
    if (std::strlen(gameId) >= sizeof snapshot.game_id) {
        std::fprintf(stderr, "ScummVM game ID exceeds snapshot identity limit\n");
        return false;
    }
    std::memset(&snapshot, 0, sizeof snapshot);
    snapshot.checkpoint_index = static_cast<uint32>(tick - 1);
    snapshot.tick = static_cast<uint32>(tick);
    snapshot.agi_version = engine.getVersion();
    snapshot.timing_profile = completedCycle ? LOGIC_ORACLE_TIMING_AGI_TICK
                                            : LOGIC_ORACLE_TIMING_WALL_MILLISECONDS;
    /* The optional engine hook marks an actual completed cycle. Wall mode
     * remains an elapsed-time sample and cannot be compared as a cycle. */
    snapshot.control_flow_reliable = 0;
    std::snprintf(snapshot.game_id, sizeof snapshot.game_id, "%s", gameId);
    std::snprintf(snapshot.checkpoint, sizeof snapshot.checkpoint,
                  completedCycle ? "agi-tick-%llu" : "wall-ms-%llu",
                  static_cast<unsigned long long>(completedCycle ? tick : wallMilliseconds));
    const int provenanceLength = std::snprintf(
        snapshot.provenance, sizeof snapshot.provenance,
        "ScummVM %s det=0x%04X AGI=0x%04X%s%s elapsed-ms=%llu p=%u",
        gScummVMVersion, detectedAgiVersion, engine.getVersion(),
        versionOverridden ? " manifest-compat" : "",
        completedCycle ? " cycle-hook presentation-delays=off" : "",
        static_cast<unsigned long long>(wallMilliseconds), engine.inGameTimerGet());
    if (provenanceLength < 0 ||
        static_cast<size_t>(provenanceLength) >= sizeof snapshot.provenance) {
        std::fprintf(stderr,
                     "ScummVM detection provenance exceeds snapshot limit (%d bytes) for %s\n",
                     provenanceLength, gameId);
        return false;
    }
    std::memcpy(snapshot.manifest_sha256, manifestHash,
                sizeof snapshot.manifest_sha256);
    std::memcpy(snapshot.input_sha256, inputHash,
                sizeof snapshot.input_sha256);
    snapshot.rng_seed = rngSeed;
    snapshot.rng_final_state = engine._rnd->getSeed();
    snapshot.rng_call_count = rngCallCount;
    std::snprintf(snapshot.rng_algorithm, sizeof snapshot.rng_algorithm,
                  "scummvm-randomsource-v1");
    snapshot.tick_numerator = manifest.tick_numerator;
    snapshot.tick_denominator = manifest.tick_denominator;
    std::memcpy(snapshot.variables, game.vars, sizeof snapshot.variables);
    for (unsigned i = 0; i < 256; ++i)
        snapshot.flags[i] = engine.getFlag(i) ? 1 : 0;
    snapshot.room = game.vars[Agi::VM_VAR_CURRENT_ROOM];
    if (game._curLogic) {
        if (game.curLogicNr < 0 || game.curLogicNr >= MAX_DIRECTORY_ENTRIES ||
            game._curLogic->cIP < 0 || game._curLogic->cIP > UINT16_MAX) {
            std::fprintf(stderr, "ScummVM logic/PC sample exceeds snapshot limits\n");
            return false;
        }
        snapshot.logic = static_cast<uint8_t>(game.curLogicNr);
        snapshot.logical_pc = static_cast<uint16_t>(game._curLogic->cIP);
        if (completedCycle && game._curLogic->cIP >= 3 &&
            game._curLogic->data[game._curLogic->cIP - 1] == 0) {
            /* cIP includes the two-byte header and has consumed RETURN. */
            snapshot.logical_pc = static_cast<uint16_t>(game._curLogic->cIP - 3);
            snapshot.control_flow_reliable = 1;
        }
    }

    if (game.numObjects > 256) {
        std::fprintf(stderr, "ScummVM inventory exceeds canonical capacity\n");
        return false;
    }
    snapshot.inventory_known = 1;
    snapshot.inventory_count = static_cast<uint16_t>(game.numObjects);
    for (unsigned i = 0; i < game.numObjects; ++i) {
        const int room = engine.objectGetLocation(static_cast<uint16>(i));
        if (room < 0 || room > 255) return false;
        snapshot.inventory_rooms[i] = static_cast<uint8_t>(room);
    }

    for (unsigned i = 0; i < SCREENOBJECTS_MAX; ++i) {
        const Agi::ScreenObjEntry &source = game.screenObjTable[i];
        if (!(source.flags & (Agi::fDrawn | Agi::fAnimated)))
            continue;
        if (source.objectNr < 0 || source.objectNr > 255 ||
            snapshot.object_count >= LOGIC_ORACLE_MAX_OBJECTS) {
            std::fprintf(stderr,
                         "ScummVM screen-object table exceeds snapshot limits at slot %u\n",
                         i);
            return false;
        }
        LogicOracleObject &object = snapshot.objects[snapshot.object_count++];
        object.id = static_cast<uint8_t>(source.objectNr);
        object.x = source.xPos;
        object.y = source.yPos;
        object.view = source.currentViewNr;
        object.loop = source.currentLoopNr;
        object.cel = source.currentCelNr;
        object.motion = static_cast<uint8_t>(source.motionType);
    }

    snapshot.message_count = printedMessageCount;
    std::memcpy(snapshot.messages, printedMessages,
                printedMessageCount * sizeof *printedMessages);
    return true;
}

} // namespace
