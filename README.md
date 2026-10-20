# Commander X16 AGI (Under Construction)

## Regression tests

Run the native C suite from the repository root:

```sh
make test CC65_HOME=/home/arbitrator/Documents/1-Projects/CX16Agi/.tools/cc65-latest X16ROM=/home/arbitrator/Documents/1-Projects/CX16Agi/.tools/rom-r49/rom.bin CX16_GAMES_DIR=/home/arbitrator/.cache/cx16agi/tools/games CX16_AGI_TEST_GAMES_DIR=/home/arbitrator/.cache/cx16agi/tools/games
```

Test binaries, emulator products, and temporary fixtures live under `obj/cx16`;
the fixture checks remove their unique directories on both passing and forced
failure paths.

The suite builds X16Community/x16-emulator revision
`77f2bab332e24c5456f9844a6c7ddd203f6198e6` under
`obj/cx16/x16-emulator-source`, with `tools/x16_testbench_video.patch` to
advance real VERA timing and VSYNC IRQs during testbench execution. The first
build needs network access, CMake, a C/C++ compiler, Python, SDL2, and zlib
development packages. SDL uses its dummy video/audio drivers. `make
test-emulator` builds this dependency alone; `X16EMU` can override the binary.
Older headless testbench builds stop video and hang real IRQ waits.

The release context stack uses a shared assembly copier and byte-indexed
33-byte frames, avoiding general multiplication and cc65 stack temporaries.
Its self-modified copy operands live in writable common RAM. Only foreground
call/return code may invoke the copier; IRQ code must not call it. C layout
checks and linker assertions guard the serialized layout. Debug builds retain
the independent C shadow inventory and return-time checks.

Measured with the normal release compiler flags and the permanent production
dispatch workload:

| Context implementation | Code bytes | RAM bytes |
| --- | ---: | ---: |
| C preservation and diagnostic bookkeeping baseline | 734 | 327 |
| Shared assembly copier, compact release frames | 406 | 266 |
| Assembly release push/pop and frame indexing, no diagnostic stubs | 260 | 266 |

The indexed assembly variable/flag clear also saves 62 emitted bytes in the
release application compared with the complete 256-entry C loop. The native
release dispatch case exercises the same nested/repeated/refill/abandonment
workload with diagnostics compiled out. These measurements cover the stated
routines; they do not establish a global size optimum for the interpreter.
Compiling only the ring operations into the target removes another 2,266
bytes of unused host trace codecs and validators (3,046 to 780 code bytes),
while keeping the ring capacity and the complete host CLI API.
Removing the unused C `said` implementations and their unused word getter
saves 751 more emitted bytes. The native parser workload checks its combined
conditional/parser C bank contribution against a 6,879-byte release limit.
The historical instrumented comparison was 7,671 to 6,920 bytes; upstream's
larger inventory object ABI subsequently added four bytes to the conditionals.
`make test-games` also builds the release application under
`obj/cx16/logic-app-release`, so optional tracing cannot weaken this release
size check. The workload executes the active assembly `said`
path with uppercase/lowercase input, OR short-circuiting, and rest-of-line
matching.

`obj/cx16/test-app-replay-requests` exercises the full-app runner's public C
request validation. It rejects over-capacity event lists, malformed event
frames and keys, invalid timeout/fault arguments, and frame-range overflow
before starting an emulator; each rejected call must also clear stale output.
The full `make test` target builds and runs this case.

The host tools are built by `make test`:

```sh
obj/cx16/logic-trace recent TRACE [--count N] [--kind write|call|return|mark] [--logic N] [--field N]
obj/cx16/logic-trace last-writer TRACE LOGIC FIELD [--space variable|flag]
obj/cx16/logic-trace check TRACE [LOGIC FIELD]
obj/cx16/logic-replay validate MANIFEST TRACE RESOURCE_DIR
obj/cx16/logic-replay snapshot --game-dir DIR --manifest FILE --input-trace FILE --ticks N --snapshot CANDIDATE.lors
obj/cx16/logic-replay full-app --game-dir DIR --manifest FILE --input-trace FILE --ticks N --output CHECKPOINT.x16r
obj/cx16/logic-replay app-reduce --game-dir DIR --manifest FILE --input-trace FILE --ticks N --fault omit-metadata-save --output-trace REDUCED.ltrace
obj/cx16/logic-replay app-reduce --failure semantic --game-dir DIR --manifest FILE --input-trace FILE --ticks N --output-trace REDUCED.ltrace
obj/cx16/logic-oracle-scummvm --game-dir DIR --manifest FILE --input-trace FILE --ticks N --checkpoint-profile completed-cycle --snapshot REFERENCE.lors
obj/cx16/logic-oracle --cross-engine-semantic REFERENCE.snap CANDIDATE.snap
```

`recent` and `last-writer` can inspect a decoded trace whose call/return history
is incomplete or inconsistent. `check` separately validates that history and
returns failure for an unmatched call.
`last-writer` selects the most recent recorded write in the retained window.
Use `--space` to distinguish a variable and flag with the same index. C opcode
writes carry an explicit space in their reason field; legacy assembly writes
are classified by opcode. Keyboard polling and asynchronous runtime changes
are outside this opcode trace; delegated parsing is recorded within `parse`.
Both inspection commands also accept raw X16R v4 application checkpoints.
For a bounded checkpoint, `check` reports incomplete history rather than
claiming that the full execution had a balanced stack. When `full-app`
captures a ready application failure, it retains that checkpoint at `--output`
and still exits with failure, so the recent events remain available to inspect.

The ScummVM AGI oracle is built from official ScummVM source at pinned revision
`e98793746f05344d0dad2c958a11921e14b34147`. On the first `make test` (or
`make test-games`), the Makefile shallow-fetches that revision from
`https://github.com/scummvm/scummvm.git`, then configures a null-backend,
AGI-only build. This first build needs network access. Source, build products,
and the adapter stay under `obj/cx16/scummvm-source`,
`obj/cx16/scummvm-build-current`, and `obj/cx16/logic-oracle-scummvm`; no
system ScummVM install is used. The pin and directories can be overridden with
`SCUMMVM_REVISION`, `SCUMMVM_SOURCE_DIR`, and `SCUMMVM_BUILD_DIR` when doing an
intentional reference-engine update.

`logic-replay validate` checks a manifest, resource hashes, and an event
stream without running the game. `logic-replay snapshot` replays clean boots
through each requested tick prefix in the headless X16 emulator, delivering
the recorded input, tick schedule, and RNG seed to the production dispatch
path with test resource loading. Full application loading is exercised by
`full-app` and `make test-games`.
Its normalized LORS v6 snapshots contain AGI variables, flags, room, screen
objects, inventory locations, RNG state, resource/input identities, and any
stable runtime failure signature. Inventory is explicitly marked unknown or
known, including known-empty inventories.

Production logic trace records are a bounded recent window. The `dropped`
count includes events evicted by the ring or omitted from the serialized
window; its status and capacity flags report that loss. In this trace producer,
`tick` is the completed-cycle replay counter (the active cycle while executing),
and `logical_pc` is the instruction offset from the owning logic's code start.
Saved-frame `logicalPc` uses the same offset convention.
The application trace reserves one 1,600-byte SMALL allocation on its first
event and retains its returned bank; it adds three common-RAM bytes for that
pointer and bank. It does not occupy the allocator's tiny-resource bank.
The native allocator regression fills all 80 tiny slots with live canaries,
wraps the 64-event ring, and checks the canaries, events, allocation ownership,
and runtime bank restoration. Release builds omit this instrumentation.

The `full-app` command launches the production CX16 application in the X16
emulator with the staged game, manifest, and input schedule. The real-game
manifest hashes all 22 executable images as well as game resources; the CLI
checks the actual launched main program against that identity and rejects
unsupported RNG algorithms before starting the emulator. The real-game
suite repeats each run, checks its checkpoint and real VSYNC progress, and
compares the full-app state with the same game, version, resource manifest,
input trace, and clock profile captured through ScummVM's normal AGI
interpreter. The reference uses a pinned ScummVM cycle hook to capture after
each completed AGI interpreter cycle, at the same boundary as the application
capture. Wall-clock samples remain a separate profile and cannot stand in for
completed cycles. This semantic profile disables ScummVM’s optional picture
and room presentation delays; the wall-clock profile retains them. Actual AGI
timed message waits advance both clocks. Main-loop polls advance virtual time
in 10 ms steps against 25 ms scheduling quanta, using variable 10's 50 ms speed
units (25 ms for unlimited speed). An otherwise idle first cycle starts at
30 ms; `have.key` can add further 10 ms polls. Modal waits advance to the next
scheduled input on that poll grid, and closing a blocking message restarts
the cycle delay. Input frame
numbers are virtual 60 Hz deadlines; physical rendering cost cannot consume
that schedule. X16R v4 records both virtual schedule frames and real elapsed
VSYNC. Repeated runs compare execution state and virtual time exactly while
keeping physical duration diagnostic. Full-app comparisons capture all cycle
boundaries in one boot. For each game, a permanent four-cycle case compares
those captures against four independent cold boots; real VSYNC duration is
the sole excluded diagnostic. At a completed RETURN boundary, both adapters normalize logical progress to
the RETURN instruction offset. Other capture boundaries leave progress marked
unreliable; comparison does not claim instruction-by-instruction equivalence. Cross-engine comparison permits engine-specific RNG
algorithms while still checking the seed and captured gameplay fields,
including inventory and ordered printed messages; deterministic replay
requires exact RNG algorithm compatibility.

Canonical messages record decoded source text emitted by `print`, `print.at`,
`display`, and their variable forms, in emission order including repetitions. The comparator checks
count, logic/message identity, and text at each position. Unprinted messages in
loaded resources do not count. Capacity loss is explicit and rejects semantic
comparison rather than silently treating a partial stream as complete.

The context frame owns the caller's code range/PC and bank, entry/file pointers
and copied metadata, current logic, execution RAM bank, code-window cache, and
stack positions. A normal return restores that state and its saved RAM bank;
room/restart abandonment keeps the new interpreter's bank. A banked opcode
adapter restores the bank needed by its own banked return address before its
RTS. The verifier keeps a separate shadow field list so the check remains
independent of the restore-frame definition. The nested-call suite covers
room changes and restart. `restore.game` is unimplemented in the product, so
restoration from a saved game has no passing production-path regression yet.

Full-app replay starts from a clean boot. A nonempty initial checkpoint is
rejected because the production application does not implement checkpoint
restore. The real-game suite needs the external KQ1/KQ3 assets listed in
`tests/real_game_corpus.tsv`; set `CX16_GAMES_DIR` and
`CX16_AGI_TEST_GAMES_DIR` to their installed asset directories when the
documented local defaults are unavailable.

The public `app-reduce` command reduces natural context failures; the optional
`--fault omit-metadata-save` injects a reproducible context failure. With
`--failure semantic`, each candidate runs both engines at the requested
completed-cycle prefixes and must retain the first divergence's cycle, field,
and values or text.
Timeouts and exhausted input cannot replace that mismatch. Each candidate is checked from the first completed cycle, so a later
reconvergence cannot hide an earlier divergence. The permanent KQ1 case injects
a transient variable change, reduces four inputs to the one trigger, compares
the reconverged final state against ScummVM, and verifies that deleting the
trigger removes the corruption. The context case reduces 15 inputs to one and
independently repeats the original and minimized failure signatures.

`tests/real_game_corpus.tsv` inventories installed KQ1 and KQ3 resources,
including the extracted dependency tables, with SHA-256 values. By default,
`CX16_GAMES_DIR` and `CX16_AGI_TEST_GAMES_DIR` use
`/home/arbitrator/.cache/cx16agi/tools/games`. The suite stages
only listed inputs under a unique `obj/cx16` directory and verifies their
hashes before replay. Corpus version evidence is `2.917`; the ScummVM adapter
records the detected version and any explicit KQ3 compatibility override.

## An AGI Interpreter For The CommanderX 16 Based On Meka By Lance Ewing

**Note: This Project Is Under Construction And Is Not Ready For General Use**

![image](https://github.com/wizardmanannan/CX16Agi/blob/main/docs/images/KingsQuestThreeImprovedIntro.gif)

See it running here on YouTube: \
[King's Quest III](https://youtu.be/NmkiB3tb6WM)

[King's Quest I](https://youtu.be/N0JRAPeKvf0)

[Space Quest II](https://youtu.be/Pxg_op2dU8Q)

[Black Cauldron](https://youtu.be/h7zuFAZj-MI)

![image](https://github.com/user-attachments/assets/444ff862-348a-492a-8962-a567565608af)
![image](https://github.com/user-attachments/assets/aeb64726-8bc4-4682-b7ba-f76894aec0fd)
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/95672f9e-32c5-4775-9ee3-5145495f0f11)
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/3fdfeadf-2016-48f1-a2fb-0f577838c710)  
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/3df45e82-c7d4-4b29-ae96-cdc5e560ff09)  
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/13d73012-a379-47b4-b18f-87e80225c55e)  

![image](https://github.com/user-attachments/assets/c1cdc287-a795-44fb-946b-8e7155d80fa1)
![image](https://github.com/user-attachments/assets/96817fbb-d173-4ca7-98f6-d2cb1458ef55)


High Level Things Completed:
- Views, backgrounds and priority
- Text parser
- Sound
- Interpreter
- Inventory
- Text boxes
- Horizontal split sprites
  

High Level Things To Be Done:
- Load and save
- String interpolation
- Vertical split sprites
- Screen shaking
- Support for games with resources larger than 8k
- Support for AGI 3 games which compress resources
- LFSR sound (I currently have a very basic stand in)



# CX 16 Iteration 16 Goals (Current)
- Implement string interpolation (string token replacement) 
- Continuing to work towards playability of King's Quest I:
- Implement screen shaking using the Vera scroll registers
- Continuing to squash bugs that would stop a player completing the game

# CX 16 Iteration 15 Goals (Completed)
- Implement Inventory
- Continuing to work towards playability of King's Quest I:
- Fix stalls that sometimes happen, such as those that occur when a new loop is loaded
- Continuing to squash bugs that would stop a player completing the game

# CX 16 Iteration 14 Goals (Complete)
- Continuing to work towards playability of King's Quest I:
- Fix stalls that sometimes happen, such as those that occur when a new loop is loaded
- Continuing to squash bugs that would stop a player completing the game
- Implement Menus

# CX 16 Iteration 13 Goals (Complete)
Working Towards Full Playability For King's Quest I:
- Fix broken and slow z-order algorithm
- Building a more reliable action handler algorithm (eg. Algorithm that acts on the parsed tokens of the user's input)
- Bug fixes

# CX 16 Iteration 12 Goals (Complete)
Fix object placement bugs, but trying to make code function exactly the same and a known good interpreter

# CX 16 Iteration 11 Goals (Complete)
- Build a reliable, fast and efficient sprite memory allocator
- Implement Audio

# CX 16 Iteration 10 Goals (Complete)
- Implement sprite garbage collection so that the backbuffer (see below) will fit.
- Implement backbuffer for '3D Affect' sprites, which will eliminate IRQ overruns for very large sprites

# CX 16 Iteration 9 Goals (Completed)
- Sprite to background priority
- Using priority screen in interpreter

# CX 16 Iteration 8 Goals (Completed)
- Incorporate CosmicR's fast drawing algorithm for both main and priority screen

# CX 16 Iteration 7 Goals (Completed)
- Allow the user to walk around the map
- Implement a parser and keyboard controls
- Objects

# CX 16 Iteration 6 Goals (Completed)
- Support more games than just King's Quest 3. 

# CX 16 Iteration 5 Goals (Completed)
- Sprite support
- Implement System Tick Replacing Inaccurate Inherited C One

**Only AGI 1 & 2 games are currently supported**

Sierra released many well-known adventure games, like King's Quest and Police Quest. For obvious reasons, these games were not coded from scratch but rather depended upon engines to run the games.

One such engine was the AGI engine, which was used to power games such as King's Quest 1 - 3 and Space Quest 1 and 2.

The goal of this project is to build an AGI Interpreter for the CommanderX16.

https://www.commanderx16.com/forum/index.php?/home/

This is based upon the Meka AGI Interpreter by Lance Ewing and utilizes the code freely available from his website.

Permission to use his code has been obtained in written form: https://github.com/lanceewing/agile/issues/65

The original MEKA source code is included in the root of this repo in an archive called Meka.zip

MEKA is written in C, hence this project depends on CC65. KickC may be considered in a future enhancement.

However extensive assembly is used where speed is required.

## Instructions For Building In Windows:
See: https://github.com/wizardmanannan/CX16Agi/blob/main/How%20To%20Setup%20Meka%20For%20Cx16.docx

This project requires extensive use of Banked RAM, for both code and data as the Meka source code is quite large.

Due to the sheer number of objects and amount of code to be stored, an allocation spreadsheet tracks this called docs\allocation.xlsx

Code which is stored in banks has a name starting with bX, where X is the bank in HEX where the code is stored, for example, bAFoo.

It is also not possible to keep every AGI object, such as views, in low memory either.

There are two broad kinds of AGI objects. Those of which the size and number are known at compile time and those which aren't.

For those which are, helper functions such as...
```c
void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
...exist for fetching and setting AGI objects to and from memory. These helper functions fetch the objects from the banked RAM and copy them into low memory or vice versa.
```

## Dynamic Memory Management

A system of dynamic banked memory allocation exists for managing objects which are dynamically loaded at runtime. This uses an implementation of the best-fit algorithm, with six sizes of memory allocation spots stored from banks 0x13 - 0x26. This is explained in detail in memorymanager.h and the related C file.

When dynamic memory is needed, `byte* banked_alloc(int size, byte* bank)` from `memorymanager.h` is called to allocate the memory. It works similarly to the standard `malloc` function, returning the address of the memory block and setting the bank at the address in the second argument.

The `boolean banked_dealloc(byte* ptr, byte bank)` function deallocates the memory. The available allocation sizes are described in `allocation.xlsx` in the 'dynamic' tab.

It's worth noting that the algorithm, implementation, and segmentation of memory might not be optimal. As more games are run, a better understanding of the appropriate segment sizes will be gained. Improvements to the segmentation sizes and overall memory management might be needed in the future.

All of the sizes and other numerous constants related to dynamic memory are located in `memorymanager.h`.
