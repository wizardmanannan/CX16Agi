#!/usr/bin/env bash
set -euo pipefail

build_root=${1:?build output root required}
repo_root=${2:?repository root required}
cycle_budget=${3:?cycle budget required}
cc65_home=${CC65_HOME:?CC65_HOME required}
x16emu=${X16EMU:?X16EMU required}
x16_rom=${X16_ROM:?X16_ROM required}

mkdir -p "$build_root"
fixture=$(mktemp -d "$build_root/target.XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM

cc65="$cc65_home/bin/cc65"
ca65="$cc65_home/bin/ca65"
cl65="$cc65_home/bin/cl65"
for tool in "$cc65" "$ca65" "$cl65" "$x16emu" "$x16_rom"; do
	if [[ ! -x "$tool" && ! -f "$tool" ]]; then
		echo "missing target test tool: $tool" >&2
		exit 1
	fi
done

"$cc65" -t cx16 -O -I "$repo_root/src" -o "$fixture/stringFormat.s" "$repo_root/src/stringFormat.c"
"$ca65" -t cx16 -o "$fixture/stringFormat.o" "$fixture/stringFormat.s"
"$cc65" -t cx16 -O -I "$repo_root/src" -o "$fixture/test-string-format-target.s" "$repo_root/tests/test-string-format-target.c"
"$ca65" -t cx16 -o "$fixture/test-string-format-target.o" "$fixture/test-string-format-target.s"
"$ca65" -t cx16 -o "$fixture/string-format-target-start.o" "$repo_root/tests/string-format-target-start.s"
"$cl65" -t cx16 -C "$repo_root/tests/string-format-target.cfg" \
	-m "$fixture/string-format-target.map" -Ln "$fixture/string-format-target.lbl" \
	-o "$fixture/string-format-target.prg" \
	"$fixture/stringFormat.o" "$fixture/test-string-format-target.o" \
	"$fixture/string-format-target-start.o"

label_address() {
	local raw
	raw=$(awk -v name=".$1" '$3 == name { print $2; found = 1; exit } END { if (!found) exit 1 }' "$fixture/string-format-target.lbl")
	printf '%04X' "$((16#$raw))"
}

prepare=$(label_address targetPrepareKq3Entry)
expand=$(label_address targetExpandKq3Entry)
validate=$(label_address targetValidateKq3Entry)
edges=$(label_address targetEdgeCasesEntry)
result=$(label_address _targetResult)

{
	printf 'RUN %s\n' "$prepare"
	printf 'RQC\n'
	printf 'RUN %s\n' "$expand"
	printf 'RQC\n'
	printf 'RUN %s\n' "$validate"
	printf 'RAM 00\nRQM %s\n' "$result"
	printf 'RUN %s\n' "$edges"
	printf 'RAM 00\nRQM %s\n' "$result"
} > "$fixture/testbench.in"

if ! timeout 30 "$x16emu" -testbench -rom "$x16_rom" -prg "$fixture/string-format-target.prg" \
	-warp -sound none < "$fixture/testbench.in" > "$fixture/testbench.out" 2>&1; then
	cat "$fixture/testbench.out" >&2
	echo "target emulator regression failed" >&2
	exit 1
fi

mapfile -t response < <(awk '/^[[:xdigit:]]+$/ { print tolower($0) }' "$fixture/testbench.out")
if [[ ${#response[@]} -ne 4 ]]; then
	cat "$fixture/testbench.out" >&2
	echo "target emulator returned ${#response[@]} values; expected 4" >&2
	exit 1
fi

before=$((16#${response[0]}))
after=$((16#${response[1]}))
cycles=$((after - before))
if [[ ${response[2]} != 0 || ${response[3]} != 0 ]]; then
	echo "target formatter regression failed: KQ3 result=${response[2]}, edge result=${response[3]}" >&2
	exit 1
fi
printf 'KQ3 formatter: input=106 bytes, messages=2, output=115 bytes, cycles=%d, budget=%s\n' "$cycles" "$cycle_budget"
if (( cycles >= cycle_budget )); then
	echo "KQ3 formatter exceeded the cycle budget" >&2
	exit 1
fi
