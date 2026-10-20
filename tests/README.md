Run the string regressions with `make string-format-test`. The formatter case
compiles the production path for the 65C02 and runs it in x16emu with banked
RAM. Its KQ3 workload checks the 106-byte wizard line, its two `%m` messages,
115-byte output, source preservation, bank restoration, and a cycle budget. The
target driver also covers tokens, escapes, recursion, cycles, depth limits,
chunked output, and destination capacities 0, 1, and truncated output. The
host display/parser regressions send their fixtures to that same target
formatter through a byte transport; there is no host expansion implementation.
Other host tests cover string commands and menu text/count capacity.

The target test creates its executable and emulator input in a unique directory
beneath `$CX16AGI_BUILD_DIR/obj/host/string-format`, then removes that directory
on success and failure. The suite requires cc65/ca65/cl65, x16emu, the X16 ROM,
a host C compiler, Python 3, a linker supporting `--gc-sections`, and `flock`. Set
`CC65_HOME`, `X16EMU`, and `X16_ROM` when the tools are elsewhere. Concurrent
builds must use separate `CX16AGI_BUILD_DIR` values.

`make string-format-target-check` compiles and assembles the six changed C
production modules and all five assembly modules with cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
This checks target compilation, not a full interpreter link or emulator replay.

The KQ3 warning workload is 106 input bytes, two message references, and 115
output bytes. The original C formatter baseline is 86,586 emulator CPU cycles;
the resident assembly formatter takes about 11,800 cycles with the same workload
(86% fewer). The suite enforces a 12,000-cycle budget and checks output bytes,
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
reduced 30,464 cycles to about 17,800; its budget is 20,000. Percent signs and
backslashes remain literal in these replacements. A 300-byte global-message
case checks cursor wraparound, embedded metacharacters at byte 254, termination,
a destination canary, and bank restoration.

Dictionary comparison runs the full product's strcmpIgnoreSpace on 64 spaced
phrases: the C baseline is 273,152 cycles and resident assembly takes 40,832
(85% fewer), with a 90,000-cycle budget. Cases retain exact signed 16-bit results,
prefix ordering, high bytes, space-only inputs, tabs, bank/stack state, and source
preservation. Symmetric 305-byte inputs cross pages in both cursor paths.
