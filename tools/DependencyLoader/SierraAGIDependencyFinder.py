#!/usr/bin/env python3
"""
Extract per-logic static dependencies from agikit-slim decompiled sources
and write consolidated files for the Commander X16 AGI interpreter.

Filenames are deliberately descriptive so it is obvious they contain only
resource-ID lists, not game code or assets.

For each game three pairs of files are produced (all uppercase):

    <GAMEID>-SCRIPT-IDS.BIN / <GAMEID>-SCRIPT-IDS.IDX
    <GAMEID>-VIEW-IDS.BIN   / <GAMEID>-VIEW-IDS.IDX
    <GAMEID>-SOUND-IDS.BIN  / <GAMEID>-SOUND-IDS.IDX

Index format (3 bytes per room, room 0 .. max_room):
    offset  (uint16 little-endian)  – start of this room’s list in the .BIN
    length  (uint8)                 – number of resource IDs

Data format:
    Concatenated lists of resource-ID bytes, in room-number order.
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

RE_GAME_ID = re.compile(
    r'\bset\.game\.id\s*\(\s*["\']([^"\']+)["\']\s*\)',
    re.IGNORECASE
)


def extract_deps(text: str):
    scripts = {int(m.group(1)) for m in RE_SCRIPT.finditer(text)}
    views   = {int(m.group(1)) for m in RE_VIEW.finditer(text)}
    sounds  = {int(m.group(1)) for m in RE_SOUND.finditer(text)}
    return scripts, views, sounds


def find_game_id(logic_files: list[Path]) -> str:
    """Search every logic file for the first set.game.id("XXXX")."""
    for path in sorted(logic_files):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        m = RE_GAME_ID.search(text)
        if m:
            return m.group(1).strip().upper()[:6]
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


def write_consolidated(output_dir: Path, game_id: str,
                       all_scripts: dict, all_views: dict, all_sounds: dict) -> None:
    """
    Write the three .BIN + .IDX pairs with clear, non-suspicious names.
    Index is dense from room 0 to max_room inclusive.
    """
    max_room = max(
        max(all_scripts.keys(), default=0),
        max(all_views.keys(), default=0),
        max(all_sounds.keys(), default=0)
    )

    def write_one(resource_name: str, deps: dict) -> None:
        data = bytearray()
        index = bytearray()

        for room in range(max_room + 1):
            ids = sorted(n for n in deps.get(room, set()) if 0 <= n <= 255)
            offset = len(data)
            length = len(ids)

            # 2-byte little-endian offset + 1-byte length
            index.append(offset & 0xFF)
            index.append((offset >> 8) & 0xFF)
            index.append(length & 0xFF)

            data.extend(ids)

        # Clear descriptive names
        bin_name = f"{game_id}-{resource_name}-IDS.BIN"
        idx_name = f"{game_id}-{resource_name}-IDS.IDX"

        bin_path = output_dir / bin_name
        idx_path = output_dir / idx_name

        bin_path.write_bytes(data)
        idx_path.write_bytes(index)

        print(f"  {bin_name:30s}  {len(data):5d} bytes   "
              f"({max_room + 1} rooms)")
        print(f"  {idx_name:30s}  {len(index):5d} bytes")

    print(f"\nWriting consolidated ID-list files for '{game_id}' "
          f"(rooms 0–{max_room}) to {output_dir}")
    write_one("SCRIPT", all_scripts)
    write_one("VIEW",   all_views)
    write_one("SOUND",  all_sounds)


def main():
    parser = argparse.ArgumentParser(
        description="Extract AGI room dependency ID lists (consolidated format)"
    )
    parser.add_argument("game_dir", type=Path,
                        help="Folder containing the original AGI game files")
    parser.add_argument("output_dir", type=Path, nargs="?", default=None,
                        help="Where to write the files (default = <game_dir>/deps)")
    parser.add_argument("--keep-extract", action="store_true",
                        help="Also keep the full agikit extract tree")
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

        print(f"Found {len(logic_files)} logic files")

        game_id = find_game_id(logic_files)
        print(f"Game ID: '{game_id}'\n")

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

        write_consolidated(output_dir, game_id,
                           all_scripts, all_views, all_sounds)

        if args.keep_extract:
            keep = output_dir / "agikit_extract"
            if keep.exists():
                shutil.rmtree(keep)
            shutil.copytree(extract_dir, keep)
            print(f"\nFull extract kept at {keep}")

    print("\nDone.")


if __name__ == "__main__":
    main()