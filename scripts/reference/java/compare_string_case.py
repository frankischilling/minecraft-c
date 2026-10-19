#!/usr/bin/env python3
"""Compare all English uppercase code-point observations with the native formatter."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/java/string-case/observations.tsv"
SHA256 = "3af1cdd042a7c7725bc0b2128c1de89807d967cee45f8b50a3cb47a002b4dbe8"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    expected = FIXTURE.read_bytes()
    if hashlib.sha256(expected).hexdigest() != SHA256:
        raise ValueError("captured fixture fingerprint")
    result = subprocess.run([str(args.executable)], stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60, check=False)
    if result.returncode != 0 or result.stderr or result.stdout != expected:
        raise ValueError("native uppercase traversal differs from actual captured Java traversal")
    report = {"status": "passed", "code_point_values": 1114112, "changed": 1485,
              "fixture_sha256": SHA256}
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        (args.work_dir / "native-output.tsv").write_bytes(result.stdout)
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
