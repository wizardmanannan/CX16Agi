#!/usr/bin/env python3
"""
Extract per-logic static dependencies from agikit-slim decompiled sources.

Output files are named (all uppercase):
    <GAMEID><ROOM><LOG|SND|VIW>META.BIN

Example for King's Quest 1, room 5:
    KQ15LOGMETA.BIN
    KQ15VIWMETA.BIN
    KQ15SNDMETA.BIN
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from collections import defaultdict

# ---------------------------------------------------------------------------
# Regexes
# ---------------------------------------------------------------------------
RE_SCRIPT = re.compile(
    r'\b(?:call|load\.logics|new\.room)\s*\(\s*(\d+)\s*\)',
    re.IGNORECASE
)
RE_VIEW = re.compile(
    r'\bload\.view\s*\(\s*(\d+)\s*\)',
    re.IGNORECASE
)
RE_SOUND = re.compile(
    r'\bload\.sound\s*\(\s*(\d+)\s*\)',
    re.IGNORECASE
)

# set.game.id("KQ1")  or  set.game.id('KQ1')
RE_GAME_ID = re.compile(
    r'\bset\.game\.id\s*\(\s*["\']([^"\']+)["\']\s*\)',
    re.IGNORECASE
)


def extract_deps(text: str):
    scripts = {int(m.group(1)) for m in RE_SCRIPT.finditer(text)}
    views   = {int(m.group(1)) for m in RE_VIEW.finditer(text)}
    sounds  = {int(m.group(1)) for m in RE_SOUND.finditer(text)}
    return scripts, views, sounds


def extract_game_id(logic0_text: str) -> str:
    m = RE_GAME_ID.search(logic0_text)
    if m:
        return m.group(1).strip().upper()
    return "UNK"


def run_agikit_extract(game_dir: Path, out_dir: Path) -> None:
    cmd = [
        "npx", "--yes", "@agikit-slim/cli",
        "extract",
        str(game_dir),
        str(out_dir),
    ]
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("agikit extract failed:", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    if result.stdout.strip():
        print(result.stdout)


def write_flat(path: Path, numbers: set[int]) -> None:
    data = bytes(sorted(n for n in numbers if 0 <= n <= 255))
    path.write_bytes(data)
    print(f"  {path.name:30s}  {len(data):3d} resources → {list(data)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("game_dir", type=Path,
                        help="Folder containing the original AGI game")
    parser.add_argument("output_dir", type=Path, nargs="?", default=None,
                        help="Where to write the .bin files (default = <game_dir>/deps)")
    parser.add_argument("--keep-extract", action="store_true")
    args = parser.parse_args()

    game_dir = args.game_dir.resolve()
    if not game_dir.is_dir():
        sys.exit(f"Error: {game_dir} is not a directory")

    output_dir = (args.output_dir or (game_dir / "deps")).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="agikit-") as tmp:
        extract_dir = Path(tmp)
        run_agikit_extract(game_dir, extract_dir)

        logic_files = list(extract_dir.rglob("*.agilogic"))
        if not logic_files:
            print("Error: no .agilogic files found", file=sys.stderr)
            sys.exit(1)

        logic_dir = logic_files[0].parent
        print(f"Found {len(logic_files)} logic files in {logic_dir}")

        # ----- Get game ID from logic 0 -----
        logic0_path = logic_dir / "0.agilogic"
        if not logic0_path.exists():
            candidates = list(logic_dir.glob("0*.agilogic"))
            logic0_path = candidates[0] if candidates else None

        if logic0_path and logic0_path.exists():
            logic0_text = logic0_path.read_text(encoding="utf-8", errors="replace")
            game_id = extract_game_id(logic0_text)
        else:
            game_id = "UNK"
            print("Warning: could not find logic 0 – using game ID 'UNK'")

        print(f"Game ID: '{game_id}'\n")

        # ----- Collect dependencies -----
        all_scripts = defaultdict(set)
        all_views   = defaultdict(set)
        all_sounds  = defaultdict(set)

        for logic_path in sorted(logic_files):
            try:
                room = int(logic_path.stem)
            except ValueError:
                continue

            text = logic_path.read_text(encoding="utf-8", errors="replace")
            scripts, views, sounds = extract_deps(text)

            all_scripts[room] = scripts
            all_views[room]   = views
            all_sounds[room]  = sounds

            print(f"Logic {room:3d}:  "
                  f"scripts={sorted(scripts)}  "
                  f"views={sorted(views)}  "
                  f"sounds={sorted(sounds)}")

        # ----- Write files (all uppercase names) -----
        print(f"\nWriting files to {output_dir}")
        for room in sorted(all_scripts):
            # Everything forced to uppercase for the CX16
            base = f"{game_id}{room}".upper()

            write_flat(output_dir / f"{base}LOGMETA.BIN", all_scripts[room])
            write_flat(output_dir / f"{base}VIWMETA.BIN", all_views[room])
            write_flat(output_dir / f"{base}SNDMETA.BIN", all_sounds[room])

        if args.keep_extract:
            keep = output_dir / "agikit_extract"
            if keep.exists():
                shutil.rmtree(keep)
            shutil.copytree(extract_dir, keep)
            print(f"\nFull extract kept at {keep}")

    print("\nDone.")


if __name__ == "__main__":
    main()