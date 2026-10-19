#!/usr/bin/env python3
"""Replay actual captured String/null Java Formatter observations."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import format_protocol as protocol

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/java/string-format/observations.tsv"
SHA256 = "da16aa66a9394d1d36456a5971ce6138166859d6da78ad54bbb8769af145301e"


def run(executable, declared):
    result = subprocess.run([str(executable)], input=declared, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=30, check=False)
    protocol.require(len(result.stdout) <= 4 * 1024 * 1024 and len(result.stderr) <= 65536,
                     "bounded native output")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    raw = FIXTURE.read_bytes()
    protocol.require(hashlib.sha256(raw).hexdigest() == SHA256, "captured fixture fingerprint")
    declared, observed = protocol.parse(raw)
    # Changing an observed outcome never changes input fed to the native owner.
    changed = raw.replace(b"RESULT\t2\tOK\tTEXT\t0\t-\n",
                          b"RESULT\t2\tOK\tTEXT\t1\t0078\n", 1)
    changed_input, changed_observed = protocol.parse(changed)
    protocol.require(changed != raw and changed_input == declared and changed_observed != observed,
                     "outcomes are separate from declarations")
    result = run(args.executable, declared)
    protocol.require(result.returncode == 0 and not result.stderr, "native exit or stderr")
    counts = protocol.compare(result.stdout, observed)
    try:
        protocol.compare(result.stdout, changed_observed)
    except ValueError:
        pass
    else:
        raise ValueError("changed oracle outcome was accepted")
    malformed = [declared.replace(b"RUN\n", b"RESULT\t0\tOK\t-\nRUN\n", 1),
                 declared.replace(b"CASE\t0\t", b"CASE\t00\t", 1),
                 declared + b"TRAILING\n"]
    for data in malformed:
        rejected = run(args.executable, data)
        protocol.require(rejected.returncode == 1 and rejected.stdout == b"" and
                         rejected.stderr == b"invalid format input\n", "malformed input rejection")
    report = {"status": "passed", **counts, "malformed_inputs_rejected": len(malformed),
              "outcome_separation_checks": 2, "fixture_sha256": SHA256}
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        (args.work_dir / "native-input.tsv").write_bytes(declared)
        (args.work_dir / "native-output.tsv").write_bytes(result.stdout)
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
