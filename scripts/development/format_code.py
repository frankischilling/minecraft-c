#!/usr/bin/env python3
"""Format project-owned C source and headers with the repository style."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


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
    files = sorted(path for directory in ("src", "include", "tests")
                   for path in (root / directory).rglob("*")
                   if path.is_file() and path.suffix in (".c", ".h"))
    options = ["--dry-run", "--Werror"] if args.check else ["-i"]
    for path in files:
        result = subprocess.run([formatter, "--style=file", *options, str(path)], cwd=root)
        if result.returncode:
            return result.returncode
    print(f"{'Checked' if args.check else 'Formatted'} {len(files)} C source/header files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
