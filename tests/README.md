Run the native C formatter tests with `make string-format-test`.
No game files or generated fixtures are needed. Executables live under
`$CX16AGI_BUILD_DIR/obj/host/string-format` (default `~/.cache/cx16agi/build`).
Concurrent builds must use separate `CX16AGI_BUILD_DIR` values.

`make string-format-target-check` compiles and assembles the formatter with
cc65/ca65. Set `CC65_HOME` if the tools are outside PATH.
