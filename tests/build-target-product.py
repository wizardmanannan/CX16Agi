"""Build the unmodified product plan with explicit cc65/ca65 intermediates."""
import pathlib
import shlex
import subprocess
import sys

repository, output, toolchain = map(pathlib.Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
bins = toolchain / "bin"
flags = (repository / "makefile").read_text().split("CFLAGS  = ", 1)[1].split("\n", 1)[0].split("#", 1)[0]
# Current cc65 removed this optimizer pass; preserve all other product options.
flags = flags.replace(" -Wc --disable-opt,OptStackOps", "").strip()
plan = subprocess.check_output([
    "make", "-B", "-n", "all", f"CC65_HOME={toolchain}",
    f"OBJDIR={output}/obj", f"PROGRAM={output}/agi", f"CFLAGS={flags}",
], cwd=repository, text=True)
for line in plan.splitlines():
    args = shlex.split(line)
    if not args or args[0] != str(bins / "cl65"):
        continue
    if "-c" in args:
        source = pathlib.Path(args[-1])
        target = pathlib.Path(args[args.index("-o") + 1])
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.suffix == ".c":
            assembly = target.with_suffix(".s")
            subprocess.run([
                str(bins / "cc65"), "-t", "cx16",
                *shlex.split(flags.replace("-Wc --disable-opt,", "--disable-opt ")),
                "-o", str(assembly), str(source),
            ], cwd=repository, check=True)
        else:
            assembly = source
        subprocess.run([str(bins / "ca65"), "-t", "cx16", "-o", str(target), str(assembly)],
                       cwd=repository, check=True)
    else:
        args[args.index("--mapfile") + 1] = str(output / "agi.map")
        if "-Ln" in args:
            args[args.index("-Ln") + 1] = str(output / "agi.lbl")
        else:
            args += ["-Ln", str(output / "agi.lbl")]
        subprocess.run(args, cwd=repository, check=True)
