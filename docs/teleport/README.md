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
trailing `s` (`king`, `kings`, `king's`).

Matching and ranking follow Meilisearch's defaults:

- the last word of a name is also a prefix: `tp cast` finds the castle;
- two neighbouring words joined may equal a text word: `tp bean stalk`
  finds "beanstalk" (room 6), counted as one typo;
- a word of 5 letters or more allows one typo, of 9 or more two; a changed,
  missing or extra letter, or two swapped neighbours, each count one, and a
  wrong first letter counts two: `tp castel`, `tp dragn`, `tp kitchn` (KQ3)
  find their rooms;
- rooms are ranked bucket by bucket: most words of the name found, then
  fewest typos, then how close the words appear (adjacent and in order is
  best; a sentence end separates them), then, for game text, the most
  matches; ties go to the lower room number.

No index is kept: one pass over each room's text yields, per word of the
name, its matches, its fewest typos and its closeness to the next word.
Each text word is compared only with the name's words it could match:
those starting with its first character, and words of 9 or more letters,
whose two typos allow a different first letter. A word none can match is
only looked through for its end, and each run of text ends in a mark byte,
so most bytes cost one decryption, two compares and the key's countdown.
Over every room's messages in KQ1 and KQ3 the pass takes 38-39 cycles per
byte when no word of the name starts a text word - the same as a loop that
only decrypts and tells letters from the rest - 44-45 for `castle`, and 94
for a 12-word name.

In KQ1, `kings room` finds King Edward's throne room (53), `moat` the castle
moat (1), `castle` the castle entrance (2), `dragon` 51 and `goat` 11. Names
are heuristics: `throne room` finds the leprechaun king's throne room (77),
which mentions its throne more often. A tour shows what each name became
before it starts, for example
`Teleport tour, 5 seconds per stop: kings room=53, moat=1, castle=2`;
press Enter to start. An unmatched name is reported and nothing happens.

A search reads each VOL file once, while "Teleport: searching rooms..." is
shown. In the X16 emulator (8 MHz), KQ1 has chosen the room for `tp castle`
0.6 seconds after the line is typed.

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
(`call`, `call.v`; up to four). Each `if` whose condition contains `posn`,
`obj.in.box`, `center.posn` or `right.posn` on ego gives a box. The rest of
the condition, and every enclosing `if`/`else`, is judged against the
current variables and flags: variable and flag tests exactly; `said`,
`have.key` and `controller` as false, since they need the player; anything
else as "might". A box counts if its condition might hold. For example,
KQ1's throne room (53) arms two boxes near the throne, and many rooms arm
their screen-edge exits. Aiming at `tp 1 80 160` (the moat) lands on dry
ground; `tp 53 40 100` lands just outside the throne boxes.

Scripts also test positions through variables:

- `get.posn(ego, vx, vy)`, then `lessn`, `greatern` or `equaln` (or `not`
  of them) on `vx` or `vy`, narrows a box on ego's X or Y;
- `distance(ego, object, v)`, then `lessn`, `equaln` or `not greatern` on
  `v`, gives the box around the object: ego within that distance of it
  (`|dx| + |dy|`, inside `x ± r`, `y ± r`).

Such tests also steer paths and priorities, so their box is kept only if
its block, or a block inside it, does something: flags, `new.room`,
inventory, sound, `print` or `display`, `program.control`. They are always
"might" - never judged by the variables' stale values, which could
otherwise rule out a whole block. In KQ1 47 of these boxes guard nothing
and are dropped; 1 is kept.

A `goto` that ends a block and jumps forward within the enclosing block
starts an `else`; one that leaves the enclosing block (Sierra scripts end
many blocks with a jump to the end of the logic) is an early exit.

If a room has no quiet spot at all, ego lands on the nearest walkable spot
and "No quiet spot here: placed where the room may react." is shown (not
during tours). If the walk could not see the whole room - more than 63
boxes, four called logics or four `distance` variables, `if`s nested deeper
than 16, a command it does not know, code cut short - the landing shows
"Room too complex to check fully: it may react." No room of KQ1 or KQ3
does.

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
narrating on arrival.

The room's own initialization runs, as on any entry, and what it records -
visited flags, an arrival score - stays, as in a save made just after
walking in. That is deliberate: undoing it would leave the game
inconsistent (a score taken back while the flag saying it was given stays
set means those points can never be earned). For the same reason teleport
does not set story flags, inventory or other prerequisites; they depend on
the game.

Not seen as triggers: tests between two position variables (`lessv`...),
position tests inside `or()`, being far from an object, positions read by
logic 0 (only the room and the logics it calls are walked), and, before
entry, `distance` to objects the new room has not placed yet. Rooms whose
script always drives ego (KQ1's sky, 80) reject the landing; ego stays
where the room put it.

## Placement

A request for another room enters it through the engine's own `new.room`,
so the room script draws its picture and initializes, then places ego. A
redirect by the room script cancels the placement. A request for the current
room walks a straight line (diagonal first) toward the target, without
crossing anything that is not walkable, and lands on the last quiet point it
passed - usually the target. A new room has no path from the old room's
coordinates; the landing is the quiet point nearest to the target by
Manhattan distance (ties: smaller X, then above), or failing that the
nearest walkable one.

That search (`teleportLanding.s`) probes the target first, then visits rows
outward from the target row and stops at the first row farther than the
best point found. Each row's priority line is read from VERA once and
reduced, with running counts, to the X positions whose whole baseline could
pass - quiet (minus the room's trigger boxes on that row) and merely
walkable - and only those get the full probe, nearest first. The walkable
fallback rides on the same sweep while no quiet point is known, so a room
without one is read once, not twice. The full probe decides, so the result is the one
probing every point would give. In a room with little open ground this is
about 65 to 130 times faster than probing point by point (2 x 26880 probes
at worst).

Walkable follows the interpreter's own movement rules (screen, horizon,
other objects, control lines, water) plus teleport's rule that no pixel
under ego's feet is raised scenery, so ego never lands in front of a wall.
Placement needs normal player control and motion; otherwise it is rejected
and nothing changes. Probes have no side effects. A landing away from the
requested X Y shows "Placed at nearest reachable point" (not for automatic
landings or tours).

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
| `search` | `teleportTriggers.s` | 1 | Which of this room's position triggers could fire now? |
| `search` | `teleportScan.s` | F | How often does each word occur in this text? |
| `tour` | `teleportTour.s` | F | When does the next stop start? |
| `placement` | `teleportArrival.s` | 6 | What happens when the requested room is reached? |
| `placement` | `teleportView.s` | 9 | What do ego's view entry and the collision rules say? |
| `placement` | `teleportPublish.s` | 9 | How does ego take the accepted position? |
| `placement` | `teleportPlace.s` | 0xA | Can ego be placed now, and where? |
| `placement` | `teleportPark.s` | 0xA | Where can ego wait while the new room sets itself up? |
| `placement` | `teleportPath.s` | 0xA | How far can ego go along a straight line? |
| `placement` | `teleportLanding.s` | 0xA | Where is the nearest landing in a new room? |
| `placement` | `teleportProbe.s` | 0xA | Can ego stand here, and would that start anything? |

Placement works on a copy of ego in bank 0xA and reaches bank 9 - the view
table and the interpreter's collision routines - only through
`teleportView.s`.

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
| Bank 6 (interpreter cycle, room entry) | 214 + 19 in `meka.c` |
| Bank 7 (keyword, hand-off, notices) | 502 + 80 in `parser.c` |
| Bank 3 (silent arrival) | 8 in `commands.c` |
| Bank 9 (view table, collision, parking flags) | 231 |
| Bank 0xA (placement, landing search, parking) | 1331 |
| Bank 1 (trigger reader) | 1721 |
| Bank 0xF (grammar, name search, file access, tours, replies) | 3685 |

Gameplay cost is one test (`lda`, `ora`, `beq`) per interpreter cycle, and
one more after the logics while a request is pending. While a command runs
it uses, on the C stack only: a 41-byte copy of the line and a 128-byte
block (directory entries, file reads, replies); for placement a 39-byte
copy of ego and a 253-byte box list, plus a 160-byte row for a new-room
landing; for reading triggers a 160-byte block. There is no heap use,
resource loading, table or other persistent buffer. Files are read with the
KERNAL block read (MACPTR), byte by byte on devices without it.

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
  over the whole screen); boxes from `get.posn` and `distance` variables,
  dropped when their block does nothing; a goto out of the enclosing block
  as an exit; walks that cannot see everything reported; parking before
  entry; the message box call; and the new-room landing search against
  fixed results and cycle ceilings (sim65 cycle counter).
- `rooms`: KQ1 and KQ3 names and tour plans, through both the block read and
  the byte read; the 65C02 word scanner against an independent C model over
  every room's message text in both games, for a 12-word name, `castle`
  and a name no word starts like, with its cycles per byte next to a loop
  that only decrypts; and the 65C02 trigger reader
  against an independent C model over every room's code and the logics it
  calls, with 15 objects drawn so `distance` boxes are compared too (KQ1:
  102 armed boxes in 81 rooms, 47 dropped, 72 calls; KQ3: 169 in 86, 52
  dropped, 132 calls; no partial walk). One game per sim65 run.
- `footprint`: each bank's teleport bytes against a budget, and no teleport
  symbol in the parser, interpreter or `main.s` built with
  `ENABLE_TELEPORT=0`.

`make -C tests/teleport test` runs `cpu` and `rooms`. Artifacts go to
`build/tests/teleport` (`BUILD` overrides it).
