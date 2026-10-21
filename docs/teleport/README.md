# Teleport for testing

Teleport jumps ego to any room, by number or by name, and can tour several
rooms in a row, so a game's pictures, sounds and room scripts can be spot
checked without playing to them. Type it at the normal input prompt.

```
tp castle                       go to the room that best matches "castle"
tp moat 80 120                  ... and stand at X 80, Y 120
tp 2                            room logic 2, landing chosen automatically
teleport 1 80 100               room 1 at (80, 100)
tp:5:kings room,moat,castle     tour: each room for 5 seconds, then the next
tp:dragon,9,goat                tour with the default 5 seconds
tp stop                         cancel a request or a tour (also: teleport 0 0 0)
```

`tp` and `teleport` are the same command. While a line starting with one of
them is typed, keys go to the line, not to the game's controller bindings
(KQ1 binds `0` to Jump), and `:` and `,` can be typed. The input line holds
39 characters.

## Destinations

A destination is `ROOM [X Y]` or `NAME [X Y]`.

- **ROOM**: a room logic number, 1 to the game's last logic, present in
  LOGDIR. Choose a *room* logic: AGI does not mark which logics are rooms.
- **NAME**: one or more words of letters and digits. See below.
- **X Y**: 0–159 and 0–167; Y is the baseline (ego's feet). Without them the
  landing is chosen automatically: a search outward from the middle of the
  walkable area (X 76, halfway between the horizon and the bottom), with no
  notice. A request without X Y for the current room leaves ego where it is.

A malformed command shows a usage line and changes nothing.

## How names are found

AGI games carry no room names, so a name is matched against the text each
room's script can print. Each room logic with a picture of the same number
(which skips most helper scripts) has its message text read from the VOL
file and decrypted as it streams by. Words are letters and digits,
lowercased, with apostrophes dropped; a word also matches with or without a
trailing `s` (`king`, `kings`, `king's`). The room with the most *different*
words of the name wins, then the most matches, then the lower room number.

In KQ1, `kings room` finds King Edward's throne room (53), `moat` the castle
moat (1), `castle` the castle entrance (2), `dragon` 51 and `goat` 11. Names
are heuristics: `throne room` finds the leprechaun king's throne room (77),
which mentions its throne more often. A tour shows what each name became
before it starts, for example
`Teleport tour, 5 seconds per stop: kings room=53, moat=1, castle=2`;
press Enter to start. An unmatched name is reported and nothing happens.

A search reads each VOL file once, about 1–1.5 seconds for KQ1 on an 8 MHz
X16, while "Teleport: searching rooms..." is shown.

## Tours

`tp:SECONDS:DEST,DEST,...` visits up to 8 stops in order. After each
arrival - placed, rejected or redirected - it waits SECONDS (0–255; default
5) on the KERNAL clock, then requests the next stop. Stops do not show
notices, so a tour never waits for a key. Any new teleport command, valid or
not, ends a running tour.

## Quiet landing: arriving starts nothing

Where teleport puts ego should not change the game: no scene, speech, death
or exit should start because of the landing spot, as if a saved game had
been loaded there. Ego therefore lands on a *quiet* spot:

- every pixel under ego's feet is plain background: no trigger line (the
  control colour scripts use to start events) and no water;
- ego is outside every position trigger of the room's script that could
  fire now.

For the second, teleport reads the room's logic from its VOL file and walks
its code without running it (`teleportTriggers.s`), then the logics it calls
(`call`, `call.v`; up to four). Each `if` whose
condition contains `posn`, `obj.in.box`, `center.posn` or `right.posn` on ego
gives a box. The rest of the condition, and every enclosing `if`/`else`, is
judged against the current variables and flags: variable and flag tests
exactly; `said`, `have.key` and `controller` as false, since they need the
player; anything else as "might". A box counts if its condition might hold.
For example, KQ1's throne room (53) arms two boxes near the throne, and many
rooms arm their screen-edge exits. Aiming at `tp 1 80 160` (the moat) lands
on dry ground; `tp 53 40 100` lands just outside the throne boxes.

If a room has no quiet spot at all, ego lands on the nearest walkable spot
and "No quiet spot here: placed where the room may react." is shown (not
during tours).

Arriving in another room is handled the same way. `new.room` keeps ego's
old-room coordinates, and the new room's script runs a whole cycle before
teleport places ego, so before entering, ego is parked outside the
destination's armed boxes (read as if already there) and the old room's
water and trigger flags are cleared (`teleportPark.s`). The new picture is
not drawn yet, so the parked spot may be water. Until placement, the
interpreter's position check therefore does not set ego's water or trigger
flags: teleport patches its "is this ego?" branch, so nothing runs during
normal play. During that cycle the room's message boxes are skipped. In KQ1
this is what stops the river room (34) drowning ego, the water rooms (4, 5,
17, 20, 41, 43, 47, 72) taking control for a swim, and the gnome's room (40)
narrating on arrival. What the room's initialization records - visited
flags, an arrival score - stays, as it would in a save made just after
arriving.

Limits: the room's own initialization still runs, as on any entry. The engine
has no save/restore to arrive any other way. Positions compared through
variables (`get.posn`, then `lessn`...) are not treated as triggers: in KQ1
(112 uses) they mostly reposition objects, steer paths or set priorities,
and counting them would fence off whole rooms (KQ1's dragon cave
entirely). Distance tests against other objects, logics called more than
four deep, and more than 32 boxes in a room are not seen either. Rooms whose
script always drives ego (KQ1's sky, 80) reject the landing; ego stays where
the room put it.

## Placement

A request for another room enters it through the engine's own `new.room`,
so the room script draws its picture and initializes, then places ego. A
redirect by the room script cancels the placement. A request for the current
room walks a straight line (diagonal first) toward the target, without
crossing anything that is not walkable, and lands on the last quiet point it
passed - usually the target. A new room has no path from the old room's
coordinates; the landing is the quiet point nearest to the target by
Manhattan distance.

Walkable follows the interpreter's own movement rules (screen, horizon,
other objects, control lines, water) plus teleport's rule that no pixel
under ego's feet is raised scenery, so ego never lands in front of a wall.
Placement needs normal player control and motion; otherwise it is rejected
and nothing changes. Probes have no side effects. A landing away from the
requested X Y shows "Placed at nearest reachable point" (not for automatic
landings or tours). Teleport does not set story flags, inventory or other
prerequisites.

## Implementation

Teleport is 65C02 assembly, one module per question, grouped by concern
under `src/teleport/` and included from `src/main.s`:

| Folder | Module | Bank | Question |
|---|---|---|---|
| `core` | `teleportState.s` | low RAM | What request is active? |
| `command` | `teleportKeyword.s` | 7 | Is this input line a teleport command? |
| `command` | `teleportEntry.s` | 7 | How does a typed line reach the command bank? |
| `command` | `teleportNotice.s` | 7 | How is a teleport message shown? |
| `command` | `teleportGrammar.s` | F | What did the user ask for? |
| `command` | `teleportCommand.s` | F | What does a teleport command do? |
| `command` | `teleportText.s` | F | How is a reply composed? |
| `search` | `teleportDirectory.s` | F | Where is a room's logic or picture on disk? |
| `search` | `teleportRoomSearch.s` | F | Which room best matches each name? |
| `search` | `teleportTriggers.s` | F | Which of this room's position triggers could fire now? |
| `search` | `teleportScan.s` | F | How often does each word occur in this text? |
| `tour` | `teleportTour.s` | F | When does the next stop start? |
| `placement` | `teleportArrival.s` | 6 | What happens when the requested room is reached? |
| `placement` | `teleportPlace.s` | 9 | Can ego be placed now, and where? |
| `placement` | `teleportPark.s` | 9 | Where can ego wait while the new room sets itself up? |
| `placement` | `teleportPath.s` | 9 | How far can ego go along a straight line? |
| `placement` | `teleportLanding.s` | 9 | Where is the nearest landing in a new room? |
| `placement` | `teleportProbe.s` | 9 | Can ego stand here, and would that start anything? |
| `placement` | `teleportPublish.s` | 9 | How does ego take the accepted position? |

`src/teleport/teleport.h` is the C-visible surface used by `parser.c`,
`meka.c` and `commands.c`. Outside `src/teleport`, the engine has only its
hooks: the cycle calls in `meka.c`, the input hand-off in `parser.c`, skipped
message boxes in `commands.c`, the includes in `main.s` and the patchable
branch label in `viewAsm.s`.

Everything else for the feature lives beside it:

```
src/teleport/              the feature (core, command, search, placement, tour)
tests/teleport/            Makefile; unit/, integration/, footprint/, support/
docs/teleport/             this document
```

## Resource cost

| Where | Bytes |
|---|---|
| Low RAM (request, X, Y, tour flag) | 4 |
| Zero page | 0 |
| Bank 6 (interpreter cycle, room entry) | 183 |
| Bank 7 (keyword, hand-off, notices) | 453 |
| Bank 9 (placement, quiet-spot checks, parking) | 962 |
| Bank 0xF (grammar, search, triggers, tours, replies) | 3309 |

Gameplay cost is two byte tests per interpreter cycle. A command uses a
41-byte copy of the line and a 128-byte block on the C stack (directory
entries, file reads, replies) only while it runs; placement uses a 39-byte
copy of ego and a 129-byte box list the same way, and reading the room's
triggers a 144-byte block. There is no heap use, resource loading, table or
other persistent buffer. Files are read with the KERNAL block read (MACPTR),
byte by byte on devices without it.

To omit teleport entirely, build C with `-D ENABLE_TELEPORT=0` and assemble
`main.s` with `--asm-define ENABLE_TELEPORT=0`.

## Verification

Tests are in `tests/teleport/`. They run the production assembly on sim65
(cc65's 65C02 simulator) with the interpreter's own collision routines; only
VERA reads, message boxes, room entry and file services are replaced. They
need cc65 (`CC65_HOME`, or the tools on `PATH`) and, for `rooms`, the game
directories.

```sh
make -C tests/teleport cpu
make -C tests/teleport rooms GAME_KQ1=/path/to/Kings-Quest-1 GAME_KQ3=/path/to/Kings-Quest-3
make -C tests/teleport footprint
```

- `cpu`: ViewTable offsets the assembly assumes; walls, baselines, objects,
  water, scenery, triggers, script control and atomic rejection; room entry
  and redirects; automatic landing; the grammar's accepted and rejected forms;
  tour timing, clock wrap and cancellation; quiet landings against
  hand-assembled room logics (armed, flag-guarded, `said`, `not`, `or`,
  nested and `else` boxes, boxes in `call`ed and `call.v`ed logics, a box
  over the whole screen); parking before entry; the message box call.
- `rooms`: KQ1 and KQ3 names and tour plans, through both the block read and
  the byte read; the 65C02 word scanner against an independent C model over
  every room's message text in both games; and the 65C02 trigger reader
  against an independent C model over every room's code and the logics it
  calls (KQ1: 100 armed boxes in 81 rooms, 72 calls; KQ3: 157 in 86, 132
  calls). One game per sim65 run.
- `footprint`: each bank's teleport bytes against a budget, and no teleport
  symbol in the parser, interpreter or `main.s` built with
  `ENABLE_TELEPORT=0`.

`make -C tests/teleport test` runs `cpu` and `rooms`. Artifacts go to
`build/tests/teleport` (`BUILD` overrides it).
