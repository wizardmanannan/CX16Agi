# Teleport for testing

At the normal game input prompt, type `teleport ROOM X Y` and press Enter.
After `teleport `, command text takes priority over game controller bindings,
so digit keys such as KQ1's Jump key `0` can be entered normally.
For example, `teleport 1 80 100` requests room logic 1 and AGI coordinates
(80, 100). If room 1 is current, the request validates and places the ego in
that scene; otherwise it immediately enters the requested room through the
engine's normal room initialization and picture drawing. Use
the room numbers for the game you loaded. This command is intercepted before
the game's vocabulary parser, so it does not depend on game-specific debug
commands or WORDS.TOK entries.

- ROOM: 1–255, with an existing entry in the loaded game's LOGDIR. Room 0
  is reserved for the interpreter's global script; the only accepted room-0
  form is the special cancellation command `teleport 0 0 0`.
- X: 0–159, measured from the left; the current cel must fit horizontally.
- Y: 0–167, measured from the top; this is the cel's baseline.
- Invalid or incomplete teleport commands are ignored without changing rooms.

Same-room requests validate a monotone eight-direction path through the scene.
If the route hits a blocked point or raised scenery under the ego's
baseline, placement uses the last valid point and displays an adjustment
notice. Press Enter or Escape to dismiss it. A blocked origin or first step is
still rejected. Cross-room requests run normal room initialization first, then
try the requested coordinates. If unavailable, they search outward by Manhattan
distance for a valid landing outside raised scenery. Drawing priority alone
does not make scenery walkable: landing checks reject picture priorities
5–15 even if the ego would be drawn in front of them. This conservative rule
may skip valid raised platforms in some games. This avoids
using coordinates from the old room as a travel path in the new room. If
initialization enters a different room, the request is canceled without
placement. The command `teleport 0 0 0` cancels a pending request.

The same-room path check uses at most 168 collision/environment/visibility
probes. A cross-room search checks at most 26,880 screen positions and uses no
map allocation. A successful placement performs one additional environment
call to publish the destination's world flags. A rejected path leaves the ego
and world flags unchanged within the current room. A room change itself is not
rolled back if no valid landing exists. Placement is rejected while a game script controls
the ego or scripted movement is active; ordinary player control is allowed.
Queued requests are checked on arrival. On success, the ego's ordinary
direction is cleared, the normal collision/update frame remains in the engine
cycle, and placement state is reset for subsequent game logic. The probe calls
do not replay intermediate script triggers or other story effects, so this is a
bounded debug relocation rather than a replay of manual travel.
It is a one-time placement command, not persistent noclip.

The implementation is game-independent within the engine's supported AGI
versions. Resource directories do not distinguish room scripts from helper
scripts: choose a **room** logic, not an arbitrary existing logic. Teleport
does not invent inventory, puzzle flags, or story prerequisites. If room
initialization redirects elsewhere, the requested coordinates are discarded.
The normal input prompt must be active; modal dialogs and scripted sequences
that disable input cannot accept the command.

## Resource cost

Three bytes of persistent, unbanked RAM; no heap allocation, map table, resource
scan, or new disk access for validation. A bank-safe lookup checks one existing
directory entry. Parsing runs only on Enter. Idle cycles perform two byte
checks in bank 6, with no additional bank switches. Placement uses a temporary
ViewTable on the existing C stack. Code and command text live in banks 6, 7,
and 9.

Set `ENABLE_TELEPORT=0` for **all** C compilations to remove the feature and
its cycle checks. It defaults to enabled for testing. The destination room
doubles as the pending flag (zero means idle), saving a separate flag byte.
The three-byte request stores room, X and Y across room initialization; one
byte cannot represent all 255 × 160 × 168 destinations without storing the
remaining information elsewhere.

Use the footprint target below for current cc65 object sizes.
The assembly visibility helper remains linked when teleport C is disabled.
Disabled teleport.o emits **0 bytes in every target segment**, including
BSS and zero page. The disabled parser and interpreter contain no teleport
symbol references. Object-file headers and debug metadata are not target RAM
or executable bytes.

## Verification

The tests live in `tests/` and are written in C. They compile the production
teleport and keyboard-input paths directly, using the real data types and
host-only headers that substitute hardware services. No Python or generated C
is required.

Run the host tests with a C compiler and Make:

```sh
make -C tests test GAMES='/path/to/Kings-Quest-1 /path/to/Kings-Quest-3'
make -C tests sanitize GAMES='/path/to/Kings-Quest-1 /path/to/Kings-Quest-3'
make -C tests footprint CL65=/path/to/cl65 OD65=/path/to/od65
make -C tests cpu CL65=/path/to/cl65 SIM65=/path/to/sim65
make -C tests game ROOT=/path/to/main-checkout GAME=/path/to/Kings-Quest-1
```

Tests execute the production command and placement functions against mocked
bank, room-transition and view services. They check malformed input, bounds,
missing resources, same-room placement, nearest-reachable fallback,
direction reset, scripted-motion rejection, and redirects. Coverage includes
all 256 room bytes, all 65,536 X/Y
byte pairs, truncated input, overflow, queued-request preservation/replacement,
directory count boundaries, and cel-width and screen-bound rejection. The
sanitizer target runs AddressSanitizer and UndefinedBehaviorSanitizer; the
footprint target checks actual cc65 segment sizes and disabled integration
references. The CPU target links the production teleport C with the unchanged
65C02 collision assembly and checks walls, baselines, objects, water,
side-effect-free probes,
the 168-position path, foreground visibility, and atomic rejection.
Build artifacts go to `build/tests/teleport` by default (`BUILD` overrides the
location). The keyboard test checks that `0` remains a controller normally and
is accepted after the exact teleport prefix. Optional game directories
exercise every LOGDIR entry and validate referenced volume signatures. They do
not execute game scripts or emulate the display; passing them is not an
end-to-end gameplay test.

The `game` target runs the already linked `build/out/agi.cx16` in the pinned
X16 emulator under `ROOT/.tools`. It boots real KQ1, dismisses the title, enters
PETSCII commands through the KERNAL input queue, and checks room state, changed
picture pixels, player control, and visible ego sprite pixels in VERA. It also
checks that room 2's landing is on background ground and that Graham can walk
left afterward. Cases:
`teleport 1 80 100` (castle foreground) and `teleport 2 100 100` (room change).
The fixture owns a unique directory under `build/tests/teleport`, copies the
game and program there, and removes everything on success or failure.
`TELEPORT_TEST_FAIL=1 make -C tests game ROOT=... GAME=...` exercises failure
cleanup and must exit nonzero. KQ3 resource checks do not prove KQ3 gameplay.
