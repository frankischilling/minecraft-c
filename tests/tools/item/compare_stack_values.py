#!/usr/bin/env python3
"""Compare actual native Stack constructor/copy fields with original observations."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/item/stack_values"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    provenance = json.loads((FIXTURES / "provenance.json").read_text(encoding="utf-8"))
    require(provenance["schema"] == "STACK_VALUES_OBSERVATION_V1" and
            provenance["captures"] == 6 and provenance["three_modes_equal_per_side"], "fixture scope")
    results = {}
    for side in ("client", "server"):
        expected = (FIXTURES / (side + ".tsv")).read_bytes()
        fingerprint = {"bytes": len(expected), "sha256": hashlib.sha256(expected).hexdigest()}
        require(fingerprint == provenance["fixtures"][side + ".tsv"], "fixture fingerprint")
        rows = [line.split("\t") for line in expected.decode("ascii").splitlines()]
        require(len(rows) == 74 and rows[0] == ["STACK_VALUES_V1", side, "iz" if side == "client" else "fy"]
                and rows[-1] == ["END", "12", "24", "73"], "actual original protocol")
        projected = []
        for row in rows[1:-1]:
            if row[0] == "INPUT":
                projected.append(row[:-1])
            elif row[0] in ("CONSTRUCTED", "COPY"):
                projected.append(row)
        result = subprocess.run([str(args.executable), side], stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, check=False)
        require(result.returncode == 0 and not result.stderr, side + " exit/stderr")
        require(result.stdout.endswith(b"\n") and b"\r" not in result.stdout, "literal LF native protocol")
        native = [line.split("\t") for line in result.stdout.decode("ascii").splitlines()]
        require(native[0] == ["STACK_NATIVE_VALUES_V1", side] and
                native[-1] == ["END", "12", "24", "37"] and native[1:-1] == projected,
                side + " actual native constructor/copy projection differs")
        results[side] = {**fingerprint, "compared_rows": len(projected)}
        if args.work_dir:
            args.work_dir.mkdir(parents=True, exist_ok=True)
            (args.work_dir / (side + ".stdout")).write_bytes(result.stdout)
    report = {"status": "passed", "results": results,
              "scope": "Actual constructor inputs and CONSTRUCTED/COPY fields/identities; original-only explicit animation mutation and SOURCE rows excluded"}
    if args.work_dir:
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
