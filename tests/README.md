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

`make string-format-target-check` compiles and assembles the five changed C
production modules and all three assembly modules with cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
This checks target compilation, not a full interpreter link or emulator replay.

The KQ3 warning workload is 106 input bytes, two message references, and 115
output bytes. The current C formatter baseline is 86,586 emulator CPU cycles;
the resident assembly formatter takes about 24,800 cycles with the same workload
(71% fewer). The suite enforces a 30,000-cycle budget to retain the improvement
while allowing minor code-layout changes. It checks output copying in 50-byte
chunks plus the terminator.

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

Additional full-product layout baselines (32 calls each): C word wrapping takes
222,784 cycles for `ab cd ef gh ij` at width 5, and C border construction takes
1,980,480 cycles for a width-30 box containing `one\n\ntwo\n`. The cases check
wrapped output, width, return values, empty rows, terminators and canaries.
