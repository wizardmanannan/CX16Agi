Run the native C interpolation regression with `make string-format-test`.
It covers token types, escapes, recursion, cycles, output bounds and explicit
line breaks. It compiles the production display and prompt paths with hardware
assembly and the IRQ
handoff stubbed. The IRQ stub checks the text submitted for display, including
the KQ3 wizard's `%m6`/`%m7` dialogue. Long prompts must fit the input display line.
Logic metadata and banked copy helpers use in-memory fixtures; these tests do
not emulate the X16 memory map.
No game files or generated fixtures are needed. All executables live under
`$CX16AGI_BUILD_DIR/obj/host/string-format` (default `~/.cache/cx16agi/build`).
Concurrent builds must use separate `CX16AGI_BUILD_DIR` values.
The host suite requires a C compiler, a linker supporting `--gc-sections`, and
`flock`. To enable sanitizers, set `HOSTCC='cc -fsanitize=address,undefined'`.

`make string-format-target-check` compiles and assembles the three changed
production modules with cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
This checks target compilation, not a full interpreter link or emulator replay.

The KQ3 warning is 106 input bytes with two message references, expanding to
115 output bytes. Per display, network and disk do no I/O (zero bytes and
requests). Memory traffic includes scanning the input and two replacement
strings, then three output chunks (50, 50, 15 bytes) and one terminator; the
formatter buffers 50 bytes and tracks at most eight source pointer/bank pairs.
CPU work is linear scanning and copying per displayed character, plus two
message lookups; it runs once per displayed message, not per frame. Bank helper
calls are the repeated latency cost; output is batched into 50-byte chunks.
No network/disk wait or speedup claim is involved.
