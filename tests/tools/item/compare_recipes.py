#!/usr/bin/env python3
"""Compare actual native recipe objects with published original numeric catalogs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/item/recipes"
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
    require(provenance["schema"] == "RECIPE_BOOTSTRAP_OBSERVATION_V1" and
            provenance["captures"] == 30 and provenance["three_modes_equal_per_entry"], "fixture scope")
    results = {}
    with tempfile.TemporaryDirectory(prefix="recipe-catalog-") as directory:
        resource = Path(directory)
        (resource / "lang").mkdir()
        (resource / "achievement").mkdir()
        (resource / "lang/en_US.lang").write_bytes(ENGLISH)
        (resource / "lang/stats_US.lang").write_bytes(b"synthetic=one\n")
        (resource / "achievement/map.txt").write_bytes(b"1000,synthetic\n")
        for side in ("client", "server"):
            for entry in ("stats", "block", "item", "crafting", "smelting"):
                label = side + "-" + entry
                expected = (FIXTURES / (label + ".tsv")).read_bytes()
                fingerprint = {"bytes": len(expected), "sha256": hashlib.sha256(expected).hexdigest()}
                require(fingerprint == provenance["fixtures"][label + ".tsv"], "fixture fingerprint")
                result = subprocess.run([str(args.executable), side, entry, "--root-hex",
                                         str(resource).encode("utf-8").hex()],
                                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, timeout=30, check=False)
                binding = result.stderr.decode("ascii").splitlines()
                require(result.returncode == 0 and len(binding) == 1, label + " exit/native binding")
                fields = binding[0].split("\t")
                require(len(fields) == 10 and fields[0] == "NATIVE" and
                        all(str(int(field)) == field for field in fields[1:8] + fields[9:]),
                        label + " native diagnostic shape")
                console = provenance["console"][label]
                output = bytes.fromhex(console["observed_hex"])
                separator = bytes.fromhex(console["line_separator_hex"])
                require(separator in (b"\r\n", b"\n"), "original console platform")
                require(not output or output.endswith(separator), "actual println suffix")
                require(not any(byte in output.replace(separator, b"") for byte in (10, 13)),
                        "only evidenced println separators")
                prefix = output.replace(separator, b"\r\n" if os.name == "nt" else b"\n")
                require(result.stdout == prefix + expected, label + " catalog/console differs")
                results[label] = {**fingerprint, "rows": expected.count(b"\n"), "native": fields[1:]}
                if args.work_dir:
                    args.work_dir.mkdir(parents=True, exist_ok=True)
                    (args.work_dir / (label + ".stdout")).write_bytes(result.stdout)
                    (args.work_dir / (label + ".stderr")).write_bytes(result.stderr)
    report = {"status": "passed", "catalogs": len(results), "results": results,
              "scope": "Published recipe order, cached shaped fields, every ingredient, smelting iteration, Stack identities and fields; no crafting gameplay"}
    if args.work_dir:
        (args.work_dir / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
