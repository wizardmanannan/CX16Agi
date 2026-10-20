# Commander X16 AGI (Under Construction)

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
- Screen shaking through VERA's display window (bitmap and sprites cannot use layer scrolling)
  

High Level Things To Be Done:
- Load and save
- String interpolation
- Vertical split sprites
- Support for games with resources larger than 8k
- Support for AGI 3 games which compress resources
- LFSR sound (I currently have a very basic stand in)



# CX 16 Iteration 16 Goals (Current)
- Implement string interpolation (string token replacement) 
- Continuing to work towards playability of King's Quest I:
- Screen shaking implemented using VERA display-window registers
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

## Repeatable C regression suite

The audit experiments are represented by checked-in C tests under `tools/`. Generated `.audit-tmp` binaries, assembly listings, emulator clones, logs, and captures are evidence, not test inputs. Tests generate their own fixtures from current production source; no audit directory is needed.

The focused C suite covers screen shake, random range/ABI behavior, exact graphics clears, sliced text, command completion, IRQ/input/sprite helpers, show-object attributes, elapsed-VSYNC timer arithmetic, palette-cache lifecycle, controller indexing, menu storage/rendering/selection, text preparation, dictionary search bounds, and bulk allocation rollback. Native scenarios additionally exercise production sprite updates and picture drawing, compare rendering against an IRQ-suppressed reference, and check allocation failure cleanup. Native runs need your own compatible ROM and game data; the review gate requires both KQ1 and KQ3. These inputs are not bundled.

Run from the repository root (or use `make -C tools`):

The suite defaults to `.tools/cc65-latest` and `.tools/x16-emulator` beside Git's common directory in the main checkout. Linked T3 worktrees share those dependencies automatically; sources and generated output stay in the current worktree. Explicit environment or Make command-line values override the defaults. Missing dependencies produce an error; the suite does not download them.

```sh
make -C tools test
make -C tools production-test
make -C tools build-test
make -C tools staging-test
make -C tools metadata-test
make -C tools irq-test
# Required before developer handoff (both original games, no skipped games):
make -C tools review-check ROM=/path/to/rom.bin \
  GAME_KQ1=/path/to/Kings-Quest-1 GAME_KQ3=/path/to/Kings-Quest-3
# With a compatible ROM and King's Quest I data:
make -C tools integration-test ROM=/path/to/rom.bin GAME=/path/to/KQ1
make -C tools picture-test ROM=/path/to/rom.bin GAME=/path/to/KQ1
make -C tools loader-test ROM=/path/to/rom.bin GAME=/path/to/KQ1
# For dependencies installed elsewhere:
# CC65_HOME=/path/to/cc65 X16_EMULATOR_SOURCE=/path/to/x16-emulator make -C tools test
```

`test` compiles and runs the focused C regressions, including shared dependency discovery and override checks. `production-test` builds a fresh application image and runs the compiled foreground sound routines on the emulator CPU core, without ROM or game data. Each run prints its fresh output directory under the project's `obj/` build tree; `TEST_OUTPUT_ROOT` can select another build output tree. Tests clean their owned output on exit. Set `KEEP_TEST_OUTPUT=1` only when you explicitly need diagnostic captures. Cleanup failure is an error. `HOST_CC` and `HOST_CFLAGS` configure the host compiler. Dependencies are checked explicitly.

`build-test` runs the shipped Makefile with its normal compiler flags, placing
the program, bank files and map in the suite output tree. `games-test` builds
that same image and runs both games with normal emulator RAM initialization.
Game staging retains `meta/`. For the shared cache layout it maps the six
`*-{logic,sound,view}-ids.{idx,bin}` files from `extracted/` into `meta/`.
Both game cases require validated metadata and actual dependency traversal;
missing metadata can no longer produce a passing acceptance result.
It sends Return through the emulator keyboard to leave the introduction,
requires KQ1 room 1 and KQ3 room 7, completed picture rendering, visible sprites,
and ten more interpreter/input cycles, then sends Right and checks that the
player actually moves. Before sending KQ3 a key, it requires all three title/credit pictures in intro
room 45, the natural transition into story room 46, its first picture, and both
chicken sprites to be published. It then waits 120 more VSYNC frames with no
injected input. KQ3's
game script skips its intro on any key, including Return.
The KQ3 case also walks outside, checks Gwydion's published sprite palette
against the game's base colours in all four walking directions, then goes
through the front door and returns to the courtyard to check the colours again.
It does not replace a full playthrough or a visual/audio
fidelity check. `review-check` runs `staging-test`, `test`, `production-test`, `games-test`,
`intro-test`, and `emulator-audio-test` sequentially and fails if either game
or any required input is missing.

`make -C tools kq3-intro-test ROM=/path/to/rom.bin GAME_KQ3=/path/to/KQ3`
checks the complete animated title/credit sequence and the first story scene
without injecting a key. For an on-screen diagnostic using the interactive
Release/LTO emulator and buffered audio, prefix that command with
`CX16AGI_VISIBLE_STARTUP=1`. It logs real key events and closes automatically
after verifying the story picture remains visible for 120 frames. Leave the
keyboard untouched during that run.
For interactive play that protects the requested opening, use
`CX16AGI_WATCH_OPENING=1` instead: it ignores early keys until the chickens/story
scene has been visible for 120 frames, then restores normal controls and keeps
the game running. It never changes rooms or game variables to skip ahead.

`make -C tools intro-test ROM=/path/to/rom.bin GAME_KQ1=/path/to/KQ1`
retains KQ1's natural title and checks a real 13-row credit redraw. The batch
must finish within three display frames without per-row IRQ waits; active sound
updates must arrive on every VSYNC. The case reports emulated cycles and host
time for the repeatable workload; the initial failure exposed partial credits
for 27 frames.

For an interactive check, use `make -C tools play ROM=/path/to/rom.bin
GAME=/path/to/game`. It builds the shipped image and a Release emulator with
link-time optimization, and stages a private copy of the game. Closing the
window cleans the staged files. The suite emulator primes half of its existing
audio queue before playback; this adds about 84 ms of headroom with the default
HostFS buffers. Shared emulator sources are not modified.
This headroom handles brief scheduling delays; it cannot sustain clean playback
when the emulator runs slower than real time. Audible crackling remains under
investigation even when the automated timing checks pass.

`make -C tools emulator-audio-test` compiles that adapter with the actual
emulator audio producer and callback. Its deterministic three-second workload
delays one producer frame every twenty frames and rejects inserted silence
after startup. Chip synthesis is replaced with constant input to isolate host
buffering; this check does not establish musical fidelity.

For a normal build, run `CC65_HOME=/path/to/cc65 make` (or `make` when `cl65`
is on PATH). The map is written beside the program as `agi.cx16.map`.
Deploy `agi.cx16`, all numbered `agi.cx16.01` through `agi.cx16.12` banks,
and the `.INIT`, `.DEBUG`, and `.SPRITEDEBUG` files from the same build into
the game's directory. The game checks stage that complete set automatically.
Keep the game's matching dependency index/data pairs in its `meta/` directory
when launching an emulator directly. Invalid files disable dependency traversal
and produce a diagnostic; the review gate rejects that configuration.

`staging-test` checks both metadata directory layouts using owned fixtures.
`metadata-test` runs the production reader against missing, truncated,
out-of-range, oversized and failed reads, empty lists and valid retries. It also
runs within `test`. `production-test` includes compiled real-allocator cases for
repeated sound discard, 30-slot cache capacity and hole reuse, output relocation
across banks, zero through four sound channels, end flags after rejected play,
and ownership during periodic conversion. View cases load real generated AGI
resources with 7/8/14/15 loops, reject invalid headers without retained
allocations, and run cached `show.obj` through room reset, palette exhaustion,
retry, IRQ publication and repeated input/cleanup cycles.

For native rendering and input integration:

```sh
make -f tools/test-image.mk OUT=/path/to/test-image
make -C tools modes ROM=/path/to/rom.bin GAME=/path/to/KQ1 \
  BUILD=/path/to/test-image SYMBOLS=/path/to/test-image/final.dbg \
  SDL_PREFIX=/optional/SDL2/install
```

`modes` runs both foreground and picture scenarios. It also requires CMake, ImageMagick (`magick`), libpng development files, and SDL2 development files, supplied by pkg-config or `SDL_PREFIX`. The isolated image build preserves production optimizer exclusions and includes private assembly debug symbols. No test reads `.audit-tmp`; old evidence can be archived independently.

The rendering scenarios finish natural KQ1 startup with real keyboard input before
saving interpreter pointers for command injection. They allow 300 seconds of host
time while retaining emulated cycle/frame bounds. The full foreground IRQ budget
is 4,096 cycles for the ROM path plus the independently tested 2,000-cycle sound
budget; the measured real-game maximum is 4,670 cycles.

`integration-test` builds the same isolated image and runs foreground, combined IRQ/text, and picture scenarios. `picture-test` runs the picture scenarios alone. The combined IRQ workload records two production dispatch rows, total IRQ cycles, the continuous I-masked interval from IRQ acceptance through the observed RTI, and two-frame text completion. Its current finite KQ1 regression reference is 29,987 cycles for the first row, 34,076 total IRQ cycles, and 29,987 maximum I-masked cycles, each with a 4,096-cycle tolerance; this is not maximum-menu or all-game coverage.

`loader-test` runs 14 generated startup scenarios through the compiled X16 loader and WORDS search routines. The native C suite also runs the real WORDS parser with platform file/bank calls replaced by test doubles. Cases cover malformed headers and records, descending full-word order, transaction rollback, empty/single/shared-prefix dictionaries (including equal adjacent words), a 513-word dictionary, missing files, and retries. A successful load retains exactly three banked allocations; a rejected load preserves published WORDS pointers, the search cache, and the allocator map. Each suite owns and removes its generated fixtures on success and failure.

Audit artifact disposition:

| Historical artifact family | Durable case or disposition |
| --- | --- |
| `random-test-*`, `negative/src` | `test_random.c`: exhaustive byte/range checks, resident modulus placement |
| `shake-current*`, `shake-test-c-*` | `test_screen_shake.c`: operand reader, zero/max counts, page alignment, restoration, rejected invalid assembly |
| `irq-helpers-*`, `sound-counter-check`, `input_sound` | `test_irq_helpers.c` and production sound tests: pending IRQ and state preservation, sound/input/sprite behavior |
| `graphics-clear-*`, background/map patches | `test_graphics_clear.c`: exact boundaries, bounded slices and cycle limits |
| `irq-completion-*` | `test_irq_completion.c`: multi-frame completion and display ownership |
| `text-irq-*` | `test_text_irq.c`: cursor, palette, page/wrap/newline boundaries |
| `show-obj-irq-*` | `test_show_obj_irq.c`: exact sprite attributes and sentinels |
| Timer binaries and clock patch | `test_timer.c`: elapsed frames, fractions, wrap and game-clock rollover |
| Native profiles, foreground and picture investigations | `test_full_game_irq.c --foreground-test` and `--picture-test`: production execution and rendering comparisons |
| RTC bridge, RTC traces and unavailable-RTC repro | Obsolete removed workaround; its clock-loss requirement is covered by production rendering VSYNC assertions |
| Five-improvement/RTC build scripts | `tools/test-image.mk`: fresh production image and debug symbols without private paths |
| `test runner space`, compiler scratch, cached emulator builds | Regenerated build products; paths with spaces belong in suite portability verification |

Do not promote historical compiler output or removed RTC functionality into fixtures. Keep assertions against live production behavior and regenerate outputs for each run.

## Screen Shake Verification

`tools/test_screen_shake.c` checks the assembled command, operand reader, register restoration, edge cases, and cycle budget using the project's emulator CPU core. `tools/test_full_game_irq.c` runs King’s Quest I in a temporary copy of that emulator and measures the complete IRQ through the final KERNAL return. These are host tools, built with your system C compiler. The full-game tool requires CMake, SDL2 development files, ImageMagick, libpng development files, your game data, and a matching build with assembly debug symbols (`ASFLAGS=-g` and a linker `.dbg` file).

Run `make -C tools test` for the focused case and `make -C tools modes` for native scenarios, using the dependency variables documented above.

The output contains IRQ timings, opcode results, a recording, and a report. The finite KQ1 emulator sample recorded 1,123 IRQs; the maximum complete IRQ through the final KERNAL return was 32,067 cycles, the active-shake maximum was 12,929 cycles, and the maximum VSYNC-to-window-write delay was 276 cycles. All five command checks restored the display, code window, and cursor, and queued input was processed after the shake. This is not an all-game worst case or a hardware measurement; this emulator renders a 0-to-4 VSTART change as seven pixels while the registers retain their correct hardware units.

Pass `--foreground-test` to instrument the real `b11DrawPic` and `b9UpdateObjects` callers when the game exercises them. In this KQ1 sample, `b11DrawPic` is not called; a real `b9UpdateObjects` workload crosses five VSYNCs. The check compares asserted VSYNCs with the production 32-bit serviced counter and verifies that IRQs during foreground VERA ownership preserve CPU registers, banks, and VERA address/control registers. It also reports the maximum fast-path IRQ duration.

Pass `--picture-test` for a deterministic production rendering check. The harness dispatches real `load.pic(1); draw.pic;` commands, then runs `draw.pic` again with VSYNC IRQ service suppressed during foreground ownership as a reference. It requires `b11DrawPic` to service every asserted VSYNC, verifies owner release and IRQ preservation of CPU state, banks, VERA state, shared scratch, renderer work memory, and I2C state, and confirms the real flood-fill and priority-split paths ran. It compares all 51,840 captured VRAM bytes between the IRQ-enabled and reference draws. `picture.csv` records counters and timings; `picture-irq.vram` and `picture-reference.vram` contain the captures.

The same scenario dispatches real `load.view(0); set.view(0,0); draw(0);` commands to reach `agiBlit` with the loaded resource. Since KQ1 startup leaves the entry's DRAWN flag clear, the native harness sets DRAWN on that loaded view-table entry and sets its stale counter to exercise the later allocation path. At the real `agiBlit` entry it enables the function's ownership argument while b9 already owns VERA, then forces `bESetLoop` failure and C allocator failure separately. It checks depth 1→2→1→0 and bank/ROM/I-flag restoration, restores the allocator and the fixture's flags/stale counter, and records both results in `agi-blit-failures.csv`.

## IRQ Helper Verification

`tools/test_irq_helpers.c` assembles the IRQ macros, input/sprite helpers, sound handler, and 32-bit VSYNC snapshot into a small fixture, then runs them on the emulator CPU core. It checks every input length (0–40), clean-row no-write and invalidation paths, all initial X values, sound start/stop/end state and partial-channel stride, snapshot byte order/caller-I preservation, pending-VSYNC preservation across interrupt reenable and wait cases, and sprite clearing through the 127-entry table boundary.

`tools/test_timer.c` runs the production timer code in a host fixture through `make -C tools test`. It covers retained VSYNC fractions, 16-bit and 32-bit rollover, long blocking intervals, and calendar boundaries. The game clock has one source: the serviced VSYNC counter. Foreground code that owns VERA leaves IRQs enabled. Its IRQ counts VSYNC and services active sound while saving/restoring both VERA ports, FX control and non-playback scratch; input and display work remain deferred. `irq-test` (also part of `test`) verifies tick/refill/simultaneous endings, both selectors, nested owners, FX cache preservation and CPU/bank state. Its measured maximum is 1,438 CPU cycles against a 2,000-cycle ceiling, with nine temporary hardware-stack bytes for saved renderer state. The full-emulator renderer probes additionally compare FX cache/multiplier state. VERA exposes VSYNC as a pending status bit, so multiple frames coalesce if other code masks IRQs across them; the counter cannot reconstruct that interval without a separate elapsed-time source.

`tools/test_random.c` checks inclusive random ranges across all 256 source bytes, both assembly and C calling conventions, bank and stack restoration, and the exclusive upper bound of `rand8Bit`. `tools/test_text_irq.c` checks 512-byte source boundaries, NUL/newline edges, 16-bit source wrap, 17-bit VERA wrap, and text/palette slices through the maximum row count.

`tools/test_irq_completion.c` runs the completion-wait macro and clear start/final IRQ paths on the emulator CPU core. `tools/test_graphics_clear.c` checks exact VRAM bounds and per-call write/cycle limits for the clear slices. The clear takes 22 VSYNCs: a first IRQ runs map/sprite/input setup (the map portion clears 2,008 cells with 4,016 VERA writes), followed by 14 background slices (1,920 writes maximum each) and seven priority slices (2,048 writes maximum each). The map routine is a deliberate exception to the 2,048-write slice target; the fixture measures it at 18,708 cycles against the 134,400-cycle frame budget.

Run these cases together with `make -C tools test`; the suite owns compilation, fixtures, and cleanup.

`CC65_HOME` must point to a CC65 installation containing `bin/ca65` and `bin/ld65`; run once per compiler version to check compatibility.

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

`tools/test_sprite_allocator.c` checks the production bulk allocator’s 64-record capacity, bank ownership, partial-failure rollback and retry, including 64×64 allocations across 17-bit VRAM addresses. Run it with `make -C tools test`.

`tools/test_agifiles.c` covers exact resource-directory EOF, empty/full tables and rejection without partial publication. Multiline text cases check palette coverage and actual cleanup; ownership and allocator cases also check processor flags and the cc65 return ABI. These run in the same `make -C tools test` suite.
