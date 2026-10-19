#!/usr/bin/env python3
"""Compare the numeric native statistic prefixes with untouched original JDI captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/statistics/arrays"
# These synthetic properties affect omitted display text, not the observed
# numeric predicates, IDs, flags, formatter owners or graph relationships.
ENGLISH = b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\n"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    provenance = json.loads((FIXTURES / "provenance.json").read_text(encoding="utf-8"))
    require(provenance["schema"] == "STATISTIC_ARRAY_PREFIX_OBSERVATION_V1" and
            provenance["captures"] == 18 and provenance["three_modes_equal_per_entry"], "fixture scope")
    results = {}
    with tempfile.TemporaryDirectory(prefix="statistic-arrays-") as directory:
        resource = Path(directory)
        (resource / "lang").mkdir()
        (resource / "achievement").mkdir()
        (resource / "lang/en_US.lang").write_bytes(ENGLISH)
        (resource / "lang/stats_US.lang").write_bytes(b"synthetic=one\n")
        (resource / "achievement/map.txt").write_bytes(b"1000,synthetic\n")
        for side in ("client", "server"):
            for entry in ("stats", "block", "item"):
                name = side + "-" + entry
                expected = (FIXTURES / (name + ".tsv")).read_bytes()
                fingerprint = {"bytes": len(expected), "sha256": hashlib.sha256(expected).hexdigest()}
                require(fingerprint == provenance["fixtures"][name + ".tsv"], "fixture fingerprint")
                result = subprocess.run([str(args.executable), side, entry, str(resource)],
                                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, timeout=30, check=False)
                require(result.returncode == 0 and not result.stderr, name + " exit/stderr")
                require(result.stdout == expected, name + " actual graph differs from original prefix")
                results[name] = {**fingerprint, "rows": len(expected.splitlines())}
                if args.work_dir:
                    args.work_dir.mkdir(parents=True, exist_ok=True)
                    (args.work_dir / (name + ".tsv")).write_bytes(result.stdout)
    report = {"status": "passed", "prefixes": len(results), "graphs": results,
              "scope": "Numeric retained graph before unfinished recipe/achievement dependencies"}
    if args.work_dir:
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
