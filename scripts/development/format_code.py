#!/usr/bin/env python3
"""Format project-owned C source/headers and selected handwritten fragments."""

import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys


def command_size(command):
    if os.name == "nt":
        return len(subprocess.list2cmdline(command).encode("utf-16-le")) // 2 + 1
    return sum(len(os.fsencode(arg)) + 1 for arg in command) + (len(command) + 1) * struct.calcsize("P")


def command_limit():
    if os.name == "nt":
        return 30000  # Below CreateProcessW's 32767 UTF-16 code units.
    try:
        available = os.sysconf("SC_ARG_MAX")
    except (AttributeError, ValueError):
        return 24000
    environment = sum(len(os.fsencode(key)) + len(os.fsencode(value)) + 2
                      for key, value in os.environ.items())
    environment += (len(os.environ) + 1) * struct.calcsize("P")
    return min(24000, max(0, available - environment - 2048))


def commands(command, files):
    limit = command_limit()
    batch = []
    for path in files:
        argument = str(path)
        if batch and (len(batch) == 128 or command_size([*command, *batch, argument]) > limit):
            yield [*command, *batch]
            batch = []
        # An oversized singleton is the original invocation; let the OS report
        # its actual limit rather than reject a previously supported path.
        batch.append(argument)
    if batch:
        yield [*command, *batch]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail when formatting differs")
    parser.add_argument("--clang-format", default="clang-format",
                        help="Formatter executable, e.g. clang-format-18")
    args = parser.parse_args()
    formatter = shutil.which(args.clang_format)
    if not formatter:
        print(f"UNAVAILABLE: formatter not found: {args.clang_format}", file=sys.stderr)
        return 77
    root = Path(__file__).resolve().parents[2]
    fragments = {"src/storage/nbt/stream_support.inc",
                 "src/storage/nbt/stream_owner.inc",
                 "src/client/display/source_world_list_surface.inc",
                 "src/client/display/world_frame_surface.inc",
                 "src/client/settings/source_input.inc",
                 "src/world/saved_data/source_storage.inc",
                 "src/world/storage/source_game_stage.inc"}
    files = sorted(path for directory in ("src", "include", "tests")
                   for path in (root / directory).rglob("*")
                   if path.is_file() and (path.suffix in (".c", ".h") or
                                          path.relative_to(root).as_posix() in fragments))
    options = ["--dry-run", "--Werror"] if args.check else ["-i"]
    for command in commands([formatter, "--style=file", *options], files):
        result = subprocess.run(command, cwd=root)
        if result.returncode:
            return result.returncode
    print(f"{'Checked' if args.check else 'Formatted'} {len(files)} C source/header/fragment files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
