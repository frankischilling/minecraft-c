#!/usr/bin/env python3
"""Replay inferred original inputs through native ItemEntity scalar formulas only."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

FIXTURE_SHA256 = "3e08eca01a4cf327a58c68ab3e4742652f86debd10847f8f80ae643b941110d9"


def run(executable, arguments, expected=None):
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        child = subprocess.run([str(executable)] + arguments, stdin=subprocess.DEVNULL,
                               stdout=stdout, stderr=stderr, timeout=15, check=False)
        if expected is None:
            if child.returncode == 0 or stdout.tell() != 0 or stderr.tell() == 0:
                raise ValueError("invalid scalar probe request was accepted")
        else:
            if child.returncode != 0 or stderr.tell() != 0 or stdout.tell() != len(expected):
                raise ValueError("scalar probe failed or returned an incomplete row")
            stdout.seek(0)
            if stdout.read(len(expected) + 1) != expected:
                raise ValueError("native field formula differs from the original observation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    options = parser.parse_args()
    raw = options.fixture.read_bytes()
    if hashlib.sha256(raw).hexdigest() != FIXTURE_SHA256 or b"\r" in raw:
        raise ValueError("complete original scalar fixture required")
    lines = raw.decode("ascii").splitlines()
    if len(lines) != 12 or lines[0] != "ITEM_FORMULA_OBSERVATION_V1" or lines[-1] != "END\t10":
        raise ValueError("complete ten-case envelope required")
    executable = options.executable.resolve(strict=True)
    for ordinal, line in enumerate(lines[1:-1]):
        row = line.split("\t")
        case, side = ordinal % 5, "client" if ordinal < 5 else "server"
        if len(row) != 12 or row[:3] != [side, str(case), "WORLD" if case == 0 else "FULL"]:
            raise ValueError("fixed original case order required")
        count = 1 if case == 0 else 4
        arguments = [row[2]] + row[3:3 + count]
        expected = ("ITEM_FORMULA_V1\t" + "\t".join(row[7:]) + "\n").encode("ascii")
        run(executable, arguments, expected)
    # These exercise the exported CLI boundary, separately from original observations.
    run(executable, ["FULL", "0000000000000000"])
    run(executable, ["WORLD", "3ff0000000000000"])
    run(executable, ["WORLD", "3FD0000000000000"])
    print("PASS: 50 original scalar fields across 10 cases; 34 inferred inputs; 3 CLI refusals; sha256="
          + FIXTURE_SHA256)


if __name__ == "__main__":
    main()
