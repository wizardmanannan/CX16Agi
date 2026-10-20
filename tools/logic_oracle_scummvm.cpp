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

class OracleNullBackend;
static OracleNullBackend *activeOracleBackend = nullptr;

class OracleNullBackend : public OSystem_NULL {
public:
    explicit OracleNullBackend(const LogicReplayEvent *events, size_t eventCount,
                               const LogicReplayManifest &manifest,
                               const uint8_t manifestHash[LOGIC_REPLAY_SHA256_SIZE],
                               const uint8_t inputHash[LOGIC_REPLAY_SHA256_SIZE],
                               uint32 tickCount,
                               LogicOracleSnapshot *snapshots, bool completedCycle)
        : OSystem_NULL(true), _events(events), _eventCount(eventCount),
          _manifest(manifest), _tickCount(tickCount), _rngSeed(0),
          _completedCycle(completedCycle),
        _snapshots(snapshots), _clock(0), _origin(0), _audioClock(0),
          _audioSampleRemainder(0), _nextEvent(0),
          _nextCheckpoint(0), _previousRngState(0), _rngCallCount(0),
          _engine(nullptr), _started(false), _quitQueued(false), _failed(false),
          _logicZeroLoaded(false), _versionOverridden(false), _keyDownCount(0),
          _detectedAgiVersion(0), _parsedLook(false),
          _maxLoadedLogicCount(0),
          _maxParserWordCount(0), _maxGameSeconds(0), _printedMessageCount(0) {
        std::memcpy(_manifestHash, manifestHash, sizeof _manifestHash);
        std::memcpy(_inputHash, inputHash, sizeof _inputHash);
        _gameId[0] = '\0';
    }

    uint32 getMillis(bool skipRecord = false) override {
        (void)skipRecord;
        return static_cast<uint32>(_clock);
    }

    void delayMillis(uint msecs) override {
        activateIfReady();
        if (!_started) {
            _clock += msecs;
            return;
        }

        const uint64 endClock = _clock + msecs;
        const uint64 endElapsed = endClock - _origin;
        for (;;) {
            uint64 next = endElapsed + 1;
            if (_nextEvent < _eventCount)
                next = MIN<uint64>(next, inputTime(_events[_nextEvent]));
            if (!_completedCycle && _nextCheckpoint < _tickCount)
                next = MIN<uint64>(next,
                                   checkpointTime(_nextCheckpoint + 1));
            if (!_completedCycle && !_quitQueued && _nextCheckpoint == _tickCount)
                next = MIN<uint64>(next, checkpointTime(_tickCount) + 1);
            if (next > endElapsed)
                break;
            _clock = _origin + next;
            advanceAudio();
            processDueEventsAndCheckpoints();
            if (_failed)
                break;
        }
        _clock = endClock;
        advanceAudio();
        processDueEventsAndCheckpoints();
    }

    bool pollEvent(Common::Event &event) override {
        activateIfReady();
        processDueEventsAndCheckpoints();
        (void)event;
        static_cast<DefaultTimerManager *>(getTimerManager())->checkTimers();
        return false;
    }

    void attachEngine(Agi::AgiEngine *engine) {
        _engine = engine;
        _previousRngState = engine->_rnd->getSeed();
        _rngSeed = _previousRngState;
    }

    bool setDetectedIdentity(const char *gameId, uint16 detectedAgiVersion) {
        if (std::strlen(gameId) >= sizeof _gameId)
            return false;
        std::snprintf(_gameId, sizeof _gameId, "%s", gameId);
        _detectedAgiVersion = detectedAgiVersion;
        return true;
    }

    bool failed() const { return _failed; }
    uint32 completedCheckpoints() const { return _nextCheckpoint; }
    bool logicZeroLoaded() const { return _logicZeroLoaded; }
    bool versionOverridden() const { return _versionOverridden; }
    uint32 keyDownCount() const { return _keyDownCount; }
    uint32 maxLoadedLogicCount() const { return _maxLoadedLogicCount; }
    uint32 maxParserWordCount() const { return _maxParserWordCount; }
    uint32 maxGameSeconds() const { return _maxGameSeconds; }
    uint16 detectedAgiVersion() const { return _detectedAgiVersion; }
    bool parsedLook() const { return _parsedLook; }
    void setVersionOverridden() { _versionOverridden = true; }

    void printedMessage(Agi::AgiEngine *engine, int number, const char *text) {
        if (_failed || _quitQueued) return;
        if (engine != _engine || !text || number < 1 || number > 255 ||
            engine->_game.curLogicNr < 0 || engine->_game.curLogicNr > 255 ||
            std::strlen(text) >= LOGIC_ORACLE_MESSAGE_TEXT_SIZE) {
            std::fprintf(stderr, "ScummVM printed message exceeds canonical capture limits\n");
            _failed = true;
            queueQuit();
            return;
        }
        if (_printedMessageCount == LOGIC_ORACLE_MAX_MESSAGES) {
            std::fprintf(stderr, "ScummVM printed message set exceeds canonical capacity\n");
            _failed = true;
            queueQuit();
            return;
        }
        LogicOracleMessage &message = _printedMessages[_printedMessageCount++];
        message.logic = static_cast<uint8_t>(engine->_game.curLogicNr);
        message.id = static_cast<uint8_t>(number);
        std::memcpy(message.text, text, std::strlen(text) + 1);
    }

    bool usesSemanticClock(const Agi::AgiEngine *engine) const {
        return _completedCycle && engine == _engine;
    }

    void completedCycle(Agi::AgiEngine *engine) {
        if (!_completedCycle || _failed || _quitQueued) return;
        activateIfReady();
        if (!_started || engine != _engine) {
            _failed = true;
            queueQuit();
            return;
        }
        if (!captureCheckpoint(_clock - _origin)) return;
        observeEngine();
        if (_nextCheckpoint == _tickCount) queueQuit();
    }

private:
    bool captureCheckpoint(uint64 elapsed) {
        uint32 calls = 0;
        const uint32 currentState = _engine->_rnd->getSeed();
        if (!countRandomCalls(_previousRngState, currentState, calls) ||
            UINT32_MAX - _rngCallCount < calls) {
            std::fprintf(stderr,
                         "cannot derive bounded ScummVM RNG state transitions: 0x%08x -> 0x%08x\n",
                         _previousRngState, currentState);
            _failed = true;
            queueQuit();
            return false;
        }
        _rngCallCount += calls;
        _previousRngState = currentState;
        if (!captureSnapshot(*_engine, _manifest, _manifestHash,
                             _inputHash, _gameId,
                             _detectedAgiVersion, _versionOverridden,
                             _completedCycle, _nextCheckpoint + 1,
                             elapsed,
                             _rngSeed, _rngCallCount,
                             _printedMessages, _printedMessageCount,
                             _snapshots[_nextCheckpoint])) {
            std::fprintf(stderr,
                         "ScummVM state or identity exceeds canonical snapshot limits\n");
            _failed = true;
            queueQuit();
            return false;
        }
        ++_nextCheckpoint;
        return true;
    }

    uint64 checkpointTime(uint64 tick) const {
        const uint64 scaled = tick * 1000ULL * _manifest.tick_denominator;
        /* Round fractional milliseconds upward, then sample one millisecond
         * later so the scheduled boundary has passed on ScummVM's virtual
         * millisecond clock before the snapshot is taken.
         */
        return (scaled + _manifest.tick_numerator - 1) /
               _manifest.tick_numerator + 1;
    }

    uint64 inputTime(const LogicReplayEvent &event) const {
        const uint64 scaled = static_cast<uint64>(event.tick) *
                              1000ULL * _manifest.tick_denominator;
        return (scaled + _manifest.tick_numerator - 1) /
               _manifest.tick_numerator;
    }

    void activateIfReady() {
        if (!_started && _engine && _engine->_game.logics[0].data) {
            _started = true;
            _origin = _clock;
            _audioClock = _clock;
        }
    }

    void advanceAudio() {
        if (!_started)
            return;
        /* NullMixerManager renders 2048 frames at 22.05 kHz per callback;
           pollEvent is per opcode here, so callback timing follows virtual time. */
        _audioSampleRemainder += (_clock - _audioClock) * 22050;
        _audioClock = _clock;
        while (_audioSampleRemainder >= 2048ULL * 1000ULL) {
            static_cast<NullMixerManager *>(getMixerManager())->update(1);
            _audioSampleRemainder -= 2048ULL * 1000ULL;
        }
    }

    void observeEngine() {
        if (!_engine)
            return;
        const Agi::AgiGame &game = _engine->_game;
        _logicZeroLoaded = _logicZeroLoaded || game.logics[0].data != nullptr;
        uint32 loadedLogicCount = 0;
        for (unsigned i = 0; i < MAX_DIRECTORY_ENTRIES; ++i)
            loadedLogicCount += game.logics[i].data != nullptr;
        _maxLoadedLogicCount = MAX<uint32>(_maxLoadedLogicCount,
                                           loadedLogicCount);
        if (_engine->_words) {
            _maxParserWordCount = MAX<uint32>(
                _maxParserWordCount, _engine->_words->getEgoWordCount());
            for (uint16 word = 0;
                 word < _engine->_words->getEgoWordCount(); ++word) {
                if (std::strcmp(_engine->_words->getEgoWord(word), "look") == 0)
                    _parsedLook = true;
            }
        }
        _maxGameSeconds = MAX<uint32>(_maxGameSeconds,
            game.vars[Agi::VM_VAR_SECONDS]);
    }

    void pushEvent(const Common::Event &event) {
        Common::EventManager *manager = getEventManager();
        if (manager)
            manager->pushEvent(event);
    }

    void processDueEventsAndCheckpoints() {
        if (!_started || !_engine || _failed)
            return;

        observeEngine();
        const uint64 elapsed = _clock - _origin;
        while (_nextEvent < _eventCount &&
               inputTime(_events[_nextEvent]) <= elapsed) {
            const LogicReplayEvent &scheduled = _events[_nextEvent++];
            if (scheduled.reason == LOGIC_REPLAY_REASON_RNG)
                continue;
            Common::Event event;
            event.type = scheduled.reason == LOGIC_REPLAY_REASON_INPUT_DOWN
                ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
            event.kbd.ascii = scheduled.field;
            switch (scheduled.field) {
            case 0x80:
                event.kbd.ascii = 0;
                event.kbd.keycode = Common::KEYCODE_UP;
                break;
            case 0x81:
                event.kbd.ascii = 0;
                event.kbd.keycode = Common::KEYCODE_DOWN;
                break;
            case 0x82:
                event.kbd.ascii = 0;
                event.kbd.keycode = Common::KEYCODE_LEFT;
                break;
            case 0x83:
                event.kbd.ascii = 0;
                event.kbd.keycode = Common::KEYCODE_RIGHT;
                break;
            default:
                event.kbd.keycode = static_cast<Common::KeyCode>(scheduled.field);
                break;
            }
            event.kbd.flags = 0;
            event.kbdRepeat = false;
            pushEvent(event);
            if (scheduled.reason == LOGIC_REPLAY_REASON_INPUT_DOWN)
                ++_keyDownCount;
        }

        while (!_completedCycle && _nextCheckpoint < _tickCount &&
               checkpointTime(_nextCheckpoint + 1) <= elapsed) {
            if (!captureCheckpoint(elapsed)) return;
        }

        observeEngine();

        if (!_completedCycle && !_quitQueued && _nextCheckpoint == _tickCount &&
            elapsed >= checkpointTime(_tickCount) + 1)
            queueQuit();
    }

    void queueQuit() {
        if (_quitQueued)
            return;
        Common::Event event;
        event.type = Common::EVENT_QUIT;
        pushEvent(event);
        _quitQueued = true;
    }

    const LogicReplayEvent *_events;
    size_t _eventCount;
    LogicReplayManifest _manifest;
    uint32 _tickCount;
    uint32 _rngSeed;
    bool _completedCycle;
    LogicOracleSnapshot *_snapshots;
    uint8_t _manifestHash[LOGIC_REPLAY_SHA256_SIZE];
    uint8_t _inputHash[LOGIC_REPLAY_SHA256_SIZE];
    char _gameId[LOGIC_ORACLE_ID_SIZE];
    uint64 _clock;
    uint64 _origin;
    uint64 _audioClock;
    uint64 _audioSampleRemainder;
    size_t _nextEvent;
    uint32 _nextCheckpoint;
    uint32 _previousRngState;
    uint32 _rngCallCount;
    Agi::AgiEngine *_engine;
    bool _started;
    bool _quitQueued;
    bool _failed;
    bool _logicZeroLoaded;
    bool _versionOverridden;
    uint32 _keyDownCount;
    uint16 _detectedAgiVersion;
    bool _parsedLook;
    uint32 _maxLoadedLogicCount;
    uint32 _maxParserWordCount;
    uint32 _maxGameSeconds;
    LogicOracleMessage _printedMessages[LOGIC_ORACLE_MAX_MESSAGES];
    uint16 _printedMessageCount;
};

static int usage(const char *program) {
    std::fprintf(stderr,
        "usage: %s --game-dir DIR --input-trace FILE --manifest FILE --ticks N --snapshot FILE [--milestones] [--checkpoint-profile wall|completed-cycle]\n",
        program);
    return 2;
}

static void printMilestones(const OracleNullBackend &backend,
                            const LogicOracleSnapshot &snapshot) {
    std::fprintf(stdout, "game_id=%s\n", snapshot.game_id);
    std::fprintf(stdout, "agi_version=0x%04x\n", snapshot.agi_version);
    std::fprintf(stdout, "detected_agi_version=0x%04x\n",
                 backend.detectedAgiVersion());
    std::fprintf(stdout, "version_override=%u\n",
                 backend.versionOverridden() ? 1U : 0U);
    std::fprintf(stdout, "logic0_loaded=%u\n",
                 backend.logicZeroLoaded() ? 1U : 0U);
    std::fprintf(stdout, "loaded_logic_count=%u\n",
                 backend.maxLoadedLogicCount());
    std::fprintf(stdout, "input_keydown_count=%u\n", backend.keyDownCount());
    std::fprintf(stdout, "max_parser_word_count=%u\n",
                 backend.maxParserWordCount());
    std::fprintf(stdout, "parsed_look=%u\n", backend.parsedLook() ? 1U : 0U);
    std::fprintf(stdout, "max_game_seconds=%u\n", backend.maxGameSeconds());
    std::fflush(stdout);
}

} // namespace

extern "C" void cx16_agi_oracle_completed_cycle(void *engine) {
    if (activeOracleBackend)
        activeOracleBackend->completedCycle(static_cast<Agi::AgiEngine *>(engine));
}

extern "C" int cx16_agi_oracle_uses_semantic_clock(void *engine) {
    return activeOracleBackend && activeOracleBackend->usesSemanticClock(
        static_cast<Agi::AgiEngine *>(engine));
}

extern "C" void cx16_agi_oracle_printed_message(void *engine, int number, const char *text) {
    if (activeOracleBackend)
        activeOracleBackend->printedMessage(static_cast<Agi::AgiEngine *>(engine), number, text);
}

int main(int argc, char **argv) {
    const char *gameDirectory = nullptr;
    const char *inputPath = nullptr;
    const char *manifestPath = nullptr;
    const char *snapshotPath = nullptr;
    uint64 tickCount64 = 0;
    uint16 expectedAgiVersion = 0;
    bool reportMilestones = false;
    bool completedCycle = false;
    LogicReplayEvent *events = nullptr;
    size_t eventCount = 0;
    LogicOracleSnapshot *snapshots = nullptr;
    LogicReplayManifest manifest = {};
    uint8_t manifestHash[LOGIC_REPLAY_SHA256_SIZE];
    uint8_t inputHash[LOGIC_REPLAY_SHA256_SIZE];
    OracleNullBackend *clockBackend = nullptr;
    Common::FSList directoryEntries;
    DetectedGames detectedGames;
    DetectedGame identifiedGame;
    bool requestedGameDetected = false;
    const void *metaDescription = nullptr;
    Common::String detectedGameId;
    MetaEngineDetection *detection = nullptr;
    MetaEngine *metaEngine = nullptr;
    OSystem *system = nullptr;
    Engine *engine = nullptr;
    Agi::AgiEngine *agiEngine = nullptr;
    int result = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--milestones") == 0) {
            if (reportMilestones)
                return usage(argv[0]);
            reportMilestones = true;
            continue;
        }
        if (i + 1 >= argc)
            return usage(argv[0]);
        const char *option = argv[i++];
        const char *value = argv[i];
        if (std::strcmp(option, "--game-dir") == 0)
            gameDirectory = value;
        else if (std::strcmp(option, "--input-trace") == 0)
            inputPath = value;
        else if (std::strcmp(option, "--manifest") == 0)
            manifestPath = value;
        else if (std::strcmp(option, "--ticks") == 0) {
            if (!parseUnsigned(value, tickCount64) || tickCount64 == 0 ||
                tickCount64 > LOGIC_ORACLE_MAX_REFERENCE_TICKS)
                return usage(argv[0]);
        } else if (std::strcmp(option, "--checkpoint-profile") == 0) {
            if (std::strcmp(value, "completed-cycle") == 0) completedCycle = true;
            else if (std::strcmp(value, "wall") == 0) completedCycle = false;
            else return usage(argv[0]);
        } else if (std::strcmp(option, "--snapshot") == 0)
            snapshotPath = value;
        else
            return usage(argv[0]);
    }
    if (!gameDirectory || !inputPath || !manifestPath || !snapshotPath ||
        tickCount64 == 0 ||
        logic_replay_manifest_read(manifestPath, &manifest) != 0 ||
        logic_replay_manifest_verify_resources(&manifest, gameDirectory) != 0 ||
        logic_replay_sha256_file(manifestPath, manifestHash) != 0 ||
        logic_replay_sha256_file(inputPath, inputHash) != 0 ||
        logic_replay_trace_read(inputPath, &events, &eventCount) != 0 ||
        !validateInputEvents(events, eventCount)) {
        std::fprintf(stderr, "invalid replay manifest, resources, or input trace\n");
        logic_replay_events_free(events);
        return 2;
    }
    {
        char *end = nullptr;
        errno = 0;
        unsigned long version = std::strtoul(manifest.agi_version, &end, 0);
        if (errno || !end || *end || version == 0 || version > UINT16_MAX) {
            std::fprintf(stderr, "invalid AGI version in replay manifest\n");
            logic_replay_events_free(events);
            return 2;
        }
        expectedAgiVersion = static_cast<uint16>(version);
    }
    if (!manifest.tick_numerator || !manifest.tick_denominator ||
        static_cast<uint64>(manifest.tick_denominator) * 1000ULL <
            manifest.tick_numerator) {
        std::fprintf(stderr,
                     "ScummVM clock resolution is one millisecond; schedule rate is unsupported\n");
        logic_replay_events_free(events);
        return 2;
    }
    if (manifest.rng_seed > 0xffffffffULL) {
        std::fprintf(stderr, "ScummVM RandomSource accepts a 32-bit replay seed\n");
        logic_replay_events_free(events);
        return 2;
    }
    snapshots = static_cast<LogicOracleSnapshot *>(
        std::calloc(static_cast<size_t>(tickCount64), sizeof *snapshots));
    if (!snapshots) {
        logic_replay_events_free(events);
        return 1;
    }

    clockBackend = new OracleNullBackend(events, eventCount, manifest,
                                         manifestHash, inputHash,
                                         static_cast<uint32>(tickCount64), snapshots, completedCycle);
    if (!clockBackend) {
        std::fprintf(stderr, "ScummVM Null backend allocation failed\n");
        goto done;
    }
    activeOracleBackend = clockBackend;
    system = clockBackend;
    g_system = system;
    system->initBackend();
    Base::registerDefaults();
    ConfMan.setActiveDomain("logic-oracle");
    ConfMan.setPath("path", Common::Path(gameDirectory), "logic-oracle");
    ConfMan.setBool("mousesupport", false, "logic-oracle");
    ConfMan.setBool("predictivedlgonmouseclick", false, "logic-oracle");

    detection = dynamic_cast<MetaEngineDetection *>(g_AGI_DETECTION_getObject());
    if (!detection) {
        std::fprintf(stderr, "ScummVM AGI detection plugin is unavailable\n");
        goto done;
    }
    if (!Common::FSNode(Common::Path(gameDirectory)).getChildren(
            directoryEntries, Common::FSNode::kListAll)) {
        std::fprintf(stderr, "could not read staged AGI game directory\n");
        goto done;
    }
    detectedGames = detection->detectGames(directoryEntries);
    for (uint i = 0; i < detectedGames.size(); ++i) {
        if (detectedGames[i].gameId == manifest.engine) {
            requestedGameDetected = true;
            break;
        }
    }
    if (!requestedGameDetected) {
        std::fprintf(stderr,
                     "ScummVM did not detect requested AGI game %s (%u candidates)\n",
                     manifest.engine, static_cast<unsigned>(detectedGames.size()));
        goto done;
    }

    detectedGameId = manifest.engine;
    ConfMan.set("gameid", detectedGameId, "logic-oracle");
    if (detection->identifyGame(identifiedGame, &metaDescription).getCode() !=
            Common::kNoError || !metaDescription ||
        identifiedGame.gameId != detectedGameId) {
        std::fprintf(stderr,
                     "ScummVM could not identify the detected AGI descriptor\n");
        goto done;
    }
    {
        Agi::AGIGameDescription *description = const_cast<Agi::AGIGameDescription *>(
            static_cast<const Agi::AGIGameDescription *>(metaDescription));
        const uint16 detectedAgiVersion = description->version;
        if (description->version != expectedAgiVersion) {
            /* The installed KQ3 data hashes to a 2.936 table entry, while its
             * staged WAG metadata and this replay explicitly select 2.917.
             * Keep the detected game/platform/features, and apply that
             * requested v2 compatibility version before ScummVM's factory
             * constructs the native engine.
             */
            if (expectedAgiVersion != 0x2917 ||
                description->gameType != Agi::GType_V2) {
                std::fprintf(stderr,
                             "ScummVM detected AGI 0x%04X but manifest requires 0x%04X\n",
                             description->version, expectedAgiVersion);
                goto done;
            }
            description->version = expectedAgiVersion;
            clockBackend->setVersionOverridden();
        }
        if (!clockBackend->setDetectedIdentity(detectedGameId.c_str(),
                                               detectedAgiVersion)) {
            std::fprintf(stderr,
                         "detected AGI identity exceeds canonical snapshot limits\n");
            goto done;
        }
    }
    metaEngine = dynamic_cast<MetaEngine *>(g_AGI_getObject());
    if (!metaEngine) {
        std::fprintf(stderr, "ScummVM AGI engine plugin is unavailable\n");
        goto done;
    }
    if (metaEngine->createInstance(system, &engine, identifiedGame,
                                   metaDescription).getCode() != Common::kNoError ||
        !engine) {
        std::fprintf(stderr,
                     "ScummVM AGI meta-engine could not create the detected game\n");
        goto done;
    }
    agiEngine = dynamic_cast<Agi::AgiEngine *>(engine);
    if (!agiEngine) {
        std::fprintf(stderr,
                     "ScummVM AGI meta-engine created a non-AGI interpreter\n");
        goto done;
    }
    if (agiEngine->getVersion() != expectedAgiVersion) {
        std::fprintf(stderr,
                     "ScummVM detected AGI 0x%04X but manifest requires 0x%04X\n",
                     agiEngine->getVersion(), expectedAgiVersion);
        goto done;
    }
    engine->initializePath(Common::FSNode(Common::Path(gameDirectory)));
    agiEngine->_rnd->setSeed(static_cast<uint32>(manifest.rng_seed));
    clockBackend->attachEngine(agiEngine);
    /* Engine::run is public here; the virtual dispatch enters AgiBase::run,
     * then AgiEngine::go -> runGame -> agiInit/playGame via ScummVM's normal
     * lifecycle and the meta-engine's factory-created instance.
     */
    if (engine->run().getCode() != Common::kNoError) {
        std::fprintf(stderr, "ScummVM AGI engine run failed\n");
        goto done;
    }
    if (clockBackend->failed() ||
        clockBackend->completedCheckpoints() != tickCount64) {
        std::fprintf(stderr,
                     "ScummVM run ended before all wall-clock checkpoints were captured\n");
        goto done;
    }
    if (logic_oracle_snapshot_write(snapshotPath, snapshots,
                                    static_cast<size_t>(tickCount64)) != 0) {
        std::fprintf(stderr, "could not write canonical ScummVM snapshots\n");
        goto done;
    }
    if (reportMilestones)
        printMilestones(*clockBackend, snapshots[tickCount64 - 1]);
    result = 0;

done:
    activeOracleBackend = nullptr;
    if (metaEngine && metaDescription) {
        metaEngine->deleteInstance(engine, identifiedGame, metaDescription);
        engine = nullptr;
        metaDescription = nullptr;
    } else {
        delete engine;
    }
    if (metaDescription) {
        delete static_cast<const ADDynamicGameDescription<Agi::AGIGameDescription> *>(
            metaDescription);
    }
    delete metaEngine;
    delete detection;
    if (system)
        system->destroy();
    std::free(snapshots);
    logic_replay_events_free(events);
    return result;
}
