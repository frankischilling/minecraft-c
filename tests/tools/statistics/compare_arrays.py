#!/usr/bin/env python3
"""Compare published native statistic graphs with untouched original JDI captures."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/statistics/bootstrap"
# These synthetic properties affect omitted display text, not the observed
# numeric predicates, IDs, flags, formatter owners or graph relationships.
ENGLISH = (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    provenance = json.loads((FIXTURES / "provenance.json").read_text(encoding="utf-8"))
    require(provenance["schema"] == "STAT_BOOTSTRAP_OBSERVATION_V1" and
            provenance["captures"] == 30 and provenance["three_modes_equal_per_entry"], "fixture scope")
    results = {}
    with tempfile.TemporaryDirectory(prefix="statistic-arrays-") as directory:
        resource = Path(directory)
        (resource / "lang").mkdir()
        (resource / "achievement").mkdir()
        (resource / "lang/en_US.lang").write_bytes(ENGLISH)
        (resource / "lang/stats_US.lang").write_bytes(b"synthetic=one\n")
        (resource / "achievement/map.txt").write_bytes(b"1000,synthetic\n")
        for side in ("client", "server"):
            for entry in ("stats", "block", "item", "crafting", "smelting"):
                name = side + "-" + entry
                expected = (FIXTURES / (name + ".tsv")).read_bytes()
                fingerprint = {"bytes": len(expected), "sha256": hashlib.sha256(expected).hexdigest()}
                require(fingerprint == provenance["fixtures"][name + ".tsv"], "fixture fingerprint")
                result = subprocess.run([str(args.executable), side, entry, "--root-hex",
                                         str(resource).encode("utf-8").hex()],
                                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, timeout=30, check=False)
                require(result.returncode == 0 and not result.stderr, name + " exit/stderr")
                console = provenance["console"][name]
                original_output = bytes.fromhex(console["observed_hex"])
                original_separator = bytes.fromhex(console["line_separator_hex"])
                require(original_separator in (b"\r\n", b"\n"), "original console platform profile")
                prefix = b""
                if original_output:
                    require(original_output.endswith(original_separator), "actual println suffix")
                    require(not any(byte in original_output.replace(original_separator, b"")
                                    for byte in (10, 13)), "only evidenced println separators")
                    prefix = original_output.replace(original_separator, b"\r\n" if os.name == "nt" else b"\n")
                require(result.stdout == prefix + expected,
                        name + " actual console/graph differs from original prefix")
                results[name] = {**fingerprint, "rows": len(expected.splitlines())}
                if args.work_dir:
                    args.work_dir.mkdir(parents=True, exist_ok=True)
                    (args.work_dir / (name + ".stdout")).write_bytes(result.stdout)
    report = {"status": "passed", "prefixes": len(results), "graphs": results,
              "scope": "Published numeric graphs after ordinary Stats/Block return or actual null dereferences2/24/75; actual console retained"}
    if args.work_dir:
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
