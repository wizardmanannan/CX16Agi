Run the string regressions with `make string-format-test`. The formatter case
compiles the production path for the 65C02 and runs it in x16emu with banked
RAM. Its KQ3 workload checks the 106-byte wizard line, its two `%m` messages,
115-byte output, source preservation, bank restoration, and a cycle budget. The
target driver also covers tokens, escapes, recursion, cycles, depth limits,
chunked output, and destination capacities 0, 1, and truncated output. The
host display/parser regressions send their fixtures to that same target
formatter through a byte transport; there is no host expansion implementation.
Other host tests cover string commands and menu text/count capacity.
The fidelity cases also cover full-width dictionary IDs in `said`, positioned
`print.at` operands, PETSCII letter conversion, punctuation, command history,
the script's input limit, modal cancellation, and editing recalled input.
Message-command cases check elapsed timeout ticks across 16-bit wraparound,
Enter/Escape dismissal, timeout reset, and flag-15 nonblocking output.
Display cases include KQ3's clock and a message whose address matches the staging
buffer in another bank; only text already staged in the display bank bypasses
interpolation.

The target test creates its executable and emulator input in a unique directory
beneath `$CX16AGI_BUILD_DIR/obj/host/string-format`, then removes that directory
on success and failure. The suite requires cc65/ca65/cl65, x16emu, the X16 ROM,
a host C compiler, Python 3, a linker supporting `--gc-sections`, and `flock`. Set
`CC65_HOME`, `X16EMU`, and `X16_ROM` when the tools are elsewhere. Concurrent
builds must use separate `CX16AGI_BUILD_DIR` values.

The cycle tests require the `RQC` testbench command in
[`x16-testbench-cycles.patch`](x16-testbench-cycles.patch). It only exposes the
emulator's existing CPU counter; it does not change CPU emulation. To build the
tested emulator from a clean checkout (CMake, a C/C++ compiler and SDL2 development
headers are required), run from this repository:

```sh
export CX16AGI_BUILD_DIR="$HOME/.cache/cx16agi/build"
mkdir -p "$CX16AGI_BUILD_DIR/tools"
git clone https://github.com/X16Community/x16-emulator.git "$CX16AGI_BUILD_DIR/tools/x16-emulator"
git -C "$CX16AGI_BUILD_DIR/tools/x16-emulator" checkout 77f2bab332e24c5456f9844a6c7ddd203f6198e6
git -C "$CX16AGI_BUILD_DIR/tools/x16-emulator" apply "$PWD/tests/x16-testbench-cycles.patch"
cmake -S "$CX16AGI_BUILD_DIR/tools/x16-emulator" -B "$CX16AGI_BUILD_DIR/tools/x16-emulator/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$CX16AGI_BUILD_DIR/tools/x16-emulator/build"
export X16EMU="$CX16AGI_BUILD_DIR/tools/x16-emulator/build/x16emu"
export CC65_HOME=/path/to/cc65
export X16_ROM=/path/to/rom.bin
make string-format-test
```

The verified compiler is cc65 V2.19 at
`a6530f46914232c085c281c4bf84e87bd136596f`; the ROM is the R49 release image.
The suite needs no Sierra game files. Without the patch, the emulator reports
`ERR Unknown command` for `RQC` and the cycle tests cannot run.

`make string-format-target-check` compiles and assembles the six changed C
production modules and all five assembly modules with cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
This checks target compilation, not a full interpreter link or emulator replay.

The KQ3 warning workload is 106 input bytes, two message references, and 115
output bytes. The original C formatter baseline is 86,586 emulator CPU cycles;
the resident assembly formatter takes about 10,100 cycles with the same workload
(88% fewer). The suite enforces an 11,000-cycle budget and checks output bytes,
terminators, canaries, and bank restoration. Entry, flushing, literals, all token
lookups, recursion, and decimal conversion execute in assembly. The formatter
calls the existing bank-5 engine API for logic metadata; stringFormat.c only
checks packed structure offsets at compile time. Highest valid string, word,
object, and message indices exercise offsets across page boundaries.

Text-mode display cases execute the production control-character assembly through
the same bridge: backspace blanks the preceding character without crossing the
line start, CR returns to that start, and LF begins the next line. Graphics-mode
text is unchanged. The behavior is ported from the existing textControls.c in
the t3code-a0c2e42d worktree; only the assembly implementation is shipped here.

Layout cases retain blank/trailing dialogue rows, clip writes at the tile-map
boundary, and cover 1,200-byte unboxed messages. Cleanup cases check staging
canaries, chunked maximum-height clears, display origins, invalid ranges, and
repeated clearing. The cleanup chunking and contiguous buffer layout were
adapted from the related t3-text-cleanup-max-height and t3code-a0c2e42d worktrees.

The suite also links the complete product and runs its parser-cleanup routine in
x16emu. Its permanent 32-phrase workload measured 511,232 cycles in C and 40,608
cycles after extracting parserInputAsm.s from t3code-a0c2e42d (92% fewer); the
budget is 90,000 cycles. A page-crossing case checks compaction and high bytes.
The product build keeps the makefile's flags except the obsolete OptStackOps
option removed by current cc65, relocates the hardcoded map path into the build
root, and emits explicit compiler/assembler intermediates. The retained
`product.map` is under the suite output root; all transient fixtures are removed.

Full-product layout workloads run 32 calls each. Word wrapping takes about
42,200 cycles versus 222,784 in C for `ab cd ef gh ij` at width 5; its budget is
100,000 cycles. Border construction takes about 172,000 versus 1,980,480 in C
for a width-30 box containing `one\n\ntwo\n`; its budget is 250,000 cycles.
Both production loops are assembly, also exercised by the display suite through
the target bridge. Cases check hard breaks, page crossings, width clamps, exact
buffer fits, row limits, blank/trailing rows, terminators, and an emulator-side
border canary. Original C baselines were retained before migration.

The remaining C changes validate command operands and capacities, manage input
polling, or orchestrate display rows and menus. Their byte copies, fills, and
string lengths use the existing assembly runtime/banked helpers. Text controls,
map clipping, parser compaction, interpolation, wrapping, and border byte loops
have one production assembly implementation each.

The shared cross-bank copy workload runs 32 copies of 513 bytes through the
full product helper; the C baseline is 1,241,728 cycles and assembly takes about
562,500 (55% fewer), with a 700,000-cycle budget. Permanent cases cover
zero length, 49/50/51-byte chunk edges, page crossings, 8,176 bytes, same-bank
copies, fixed RAM, source preservation, canaries, and software-stack cleanup.
The banked strlen/strcpy/memcpy/memset entry points are resident assembly around
the cc65 runtime. Cases check page-crossing lengths, returned pointers (including
the formerly uninitialized strcpyBanked result), canaries, banks, and stack cleanup.

A second formatter workload expands literal word/object/global-message tokens
into 239 output bytes. Keeping its read cursor and buffer count in registers
reduced 30,464 cycles to about 14,200; its budget is 15,000. Percent signs and
backslashes remain literal in these replacements. A 300-byte global-message
case checks cursor wraparound, embedded metacharacters at byte 254, termination,
a destination canary, and bank restoration.

Dictionary comparison runs the full product's strcmpIgnoreSpace on 64 spaced
phrases: the C baseline is 273,152 cycles and resident assembly takes 40,832
(85% fewer), with a 90,000-cycle budget. Cases retain exact signed 16-bit results,
prefix ordering, high bytes, space-only inputs, tabs, bank/stack state, and source
preservation. Symmetric 305-byte inputs cross pages in both cursor paths.

Tokenization uses the full product image and parser bank. Its C/strtok baseline
is 1,516,864 cycles for 64 inputs of 20 words; the single-space assembly scanner
takes about 105,700 (93% fewer), with a 160,000-cycle budget. Cases check exact token pointers,
inserted NULs, empty/all-space inputs, tabs, table/source canaries, stack and bank
state, a full 20-entry table, and source/table addresses across page boundaries.
The display/parser host suite transports token bytes and pointer offsets through
that same target scanner; there is no host tokenizer implementation.

The formatter accounts for literal bytes once per bounded run rather than once
per character. The previous assembly baselines were 11,787 wizard cycles and
17,777 literal-token cycles; batching takes about 10,100 and 14,200. Permanent
capacity cases span 0/1/2, 49/50/51, 99/100/101, 255/256/257 and 301/302 bytes
for both literal and recursive messages. An unaligned 300-byte source checks
cursor preservation through indexed wrap, a format token, and an escape.

Input-line composition is one bounded assembly routine shared by get.string
and keyboard polling, replacing their repeated snprintf calls. Native get.string
cases cover platform (0x14) and ASCII (0x08) backspace, stored input, and cursor
cell erasure. Composition cases compare the former snprintf contract at exact
fits and truncation boundaries, with custom cursors, 40 input bytes and up to
255 prompt bytes. The emulator checks termination and an adjacent output canary.
The IRQ renderer is exercised in the complete product: assertions read actual
VERA tiles and palette attributes for custom prompts, row/column positioning,
tail erasure, right-edge clipping, hidden/incomplete publications, and the active
text palette. The native colour test checks all 256 legal colour-pair identities.

Inventory cases execute the C page composition and the complete product's
assembly name-copy, padding and highlighting routines. They cover empty and
zero-length names, cross-page staging flushes, 255-byte names clipped to 20
columns, canaries and stack restoration. Native page cases cover 0, 1, 52, 54,
61 and 100 carried objects, PgUp/PgDn navigation and selection by game object ID.
The opcode tests follow the actual dispatch vectors for `echo.line` and
`cancel.line`, including prompt interpolation and software-stack restoration.
