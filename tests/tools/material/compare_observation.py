#!/usr/bin/env python3
"""Compare a fixed native material observation with the captured original rows."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

FIXTURE_SHA256 = "900bd25cc4e6f7857cd4faabda18d7b3baa075ae8672f5985152a45161ab831c"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    options = parser.parse_args()
    expected = options.fixture.read_bytes()
    if (hashlib.sha256(expected).hexdigest() != FIXTURE_SHA256
            or not expected.startswith(b"MATERIAL_V1\n")
            or not expected.endswith(b"END\t28\t14\t16\n")
            or expected.count(b"\n") != 116 or b"\r" in expected):
        raise ValueError("complete original observation required")
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        # Fixed no-input probe; these stream limits are post-child checks.
        result = subprocess.run([str(options.executable.resolve(strict=True))],
                                stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                timeout=15, check=False)
        if result.returncode != 0 or stderr.tell() != 0 or stdout.tell() != len(expected):
            raise ValueError("native observation failed or has an incomplete output")
        stdout.seek(0)
        if stdout.read(len(expected) + 1) != expected:
            raise ValueError("native material rows differ from the original observation")
    print("PASS: 116 original material rows; sha256=" + hashlib.sha256(expected).hexdigest())


if __name__ == "__main__":
    main()
