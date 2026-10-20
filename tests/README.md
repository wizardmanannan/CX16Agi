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
a host C compiler, a linker supporting `--gc-sections`, and `flock`. Set
`CC65_HOME`, `X16EMU`, and `X16_ROM` when the tools are elsewhere. Concurrent
builds must use separate `CX16AGI_BUILD_DIR` values.

`make string-format-target-check` compiles and assembles the five changed
production modules with cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
This checks target compilation, not a full interpreter link or emulator replay.

The KQ3 warning workload is 106 input bytes, two message references, and 115
output bytes. The current C formatter baseline is 86,586 emulator CPU cycles;
the resident assembly formatter takes about 24,800 cycles with the same workload
(71% fewer). The suite retains the strict 86,585-cycle budget. The test checks
output copying in 50-byte chunks plus the
terminator.

Text-mode display cases execute the production control-character assembly through
the same bridge: backspace blanks the preceding character without crossing the
line start, CR returns to that start, and LF begins the next line. Graphics-mode
text is unchanged. The behavior is ported from the existing textControls.c in
the t3code-a0c2e42d worktree; only the assembly implementation is shipped here.
