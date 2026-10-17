#!/usr/bin/env python3
"""Compare the C region reader with previously recorded valid-file observations.

This helper starts only the project's C probe. It does not run an original
reader, Java, or the program that produced the supplied capture.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts/reference/nbt"))
from compare_nbt import compound_order_wire

HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
GROUPS = [(runtime, side) for runtime in ("temurin17", "oracle21") for side in HASHES]
CAP = 16 * 1024 * 1024


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_valid_file(data):
    """Independent format preflight; return only complete, nonoverlapping files."""
    if len(data) < 8192 or len(data) % 4096:
        return None
    entries, ranges = [], []
    for i in range(1024):
        packed = struct.unpack_from(">I", data, i * 4)[0]
        stamp = struct.unpack_from(">I", data, 4096 + i * 4)[0]
        entry = {"location": packed, "timestamp": stamp}
        if packed:
            start, count = packed >> 8, packed & 255
            offset, extent = start * 4096, count * 4096
            if start < 2 or not count or offset + extent > len(data):
                return None
            if any(offset < end and begin < offset + extent for begin, end in ranges):
                return None
            ranges.append((offset, offset + extent))
            length = struct.unpack_from(">i", data, offset)[0]
            if length < 1 or length + 4 > extent:
                return None
            selector = data[offset + 4]
            if selector not in (1, 2):
                return None
            payload = data[offset + 5:offset + 4 + length]
            try:
                decoder = zlib.decompressobj(31 if selector == 1 else 15)
                raw = decoder.decompress(payload, 1024 * 1024 + 1)
                if not decoder.eof or decoder.unconsumed_tail or len(raw) > 1024 * 1024:
                    return None
                if selector == 1 and decoder.unused_data.startswith(b"\x1f\x8b"):
                    return None  # This captured profile covers one gzip member only.
            except zlib.error:
                return None
            entry.update(offset=offset, extent=extent, length=length, selector=selector,
                         payload=payload, raw=raw,
                         consumed=len(payload) - len(decoder.unused_data))
        entries.append(entry)
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit must contain 40 hexadecimal characters")
    executable, capture, work = (path.resolve() for path in
                                 (args.executable, args.capture, args.work_dir))
    sources = [Path(__file__).resolve(), ROOT / "scripts/reference/nbt/compare_nbt.py",
               ROOT / "scripts/reference/nbt/fixtures.py"]
    fixture_root = (capture.parent / "fixtures").resolve()
    protected = [path.resolve() for path in [executable, capture, fixture_root, *sources]]
    if any(work == path or work in path.parents for path in protected):
        print("FAIL: output directory contains inputs or source", file=sys.stderr)
        return 1
    names = ["manifest.json", "requests.txt", "stdout.txt", "stderr.txt"]
    if any((work / name).resolve() in protected or
           fixture_root == (work / name).resolve() or
           fixture_root in (work / name).resolve().parents for name in names):
        print("FAIL: output path overlaps inputs", file=sys.stderr)
        return 1
    report = {"schema": 1, "status": "running", "mode": "recorded_observations",
              "live_original_region_check": "pending",
              "scope": "Recorded valid-file metadata, payload and canonical-root comparisons only",
              "tested_commit": args.tested_commit.lower() or None,
              "started_utc": datetime.now(timezone.utc).isoformat(), "errors": []}

    def save():
        (work / "manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
        if not executable.is_file() or not capture.is_file():
            report.update(status="unavailable", reason="C probe or recorded capture is missing")
            save()
            print("UNAVAILABLE: " + report["reason"], file=sys.stderr)
            return 77
        if capture.stat().st_size > CAP:
            raise ValueError("capture exceeds size limit")
        original = json.loads(capture.read_text(encoding="utf-8"))
        if not isinstance(original, dict):
            raise ValueError("capture must be a JSON object")
        jars, results = original.get("jars"), original.get("results")
        if not isinstance(jars, dict) or not isinstance(results, dict):
            raise ValueError("capture jars/results must be JSON objects")
        if any(not isinstance(jars.get(side), dict) or jars[side].get("sha256") != value
               for side, value in HASHES.items()):
            raise ValueError("recorded original fingerprints do not match")
        report.update(capture_sha256=sha(capture), executable_sha256=sha(executable),
                      source_hashes={str(path.relative_to(ROOT)): sha(path) for path in sources})
        plans, skipped = [], 0
        for runtime, side in GROUPS:
            group = runtime + "-" + side
            rows = results.get(group)
            if not isinstance(rows, list) or len(rows) > 1024:
                raise ValueError("missing or oversized recorded group")
            for row in rows:
                if not isinstance(row, dict):
                    raise ValueError("recorded query must be a JSON object")
                label, x, z = row.get("fixture"), row.get("x"), row.get("z")
                if (not isinstance(label, str) or not re.fullmatch(r"[a-z0-9-]{1,96}", label)
                        or type(x) is not int or type(z) is not int):
                    raise ValueError("invalid recorded query")
                if (not 0 <= x < 32 or not 0 <= z < 32 or row.get("constructor") != "OK"
                        or row.get("mutated") is not False):
                    skipped += 1
                    continue
                path = (fixture_root / runtime / side / f"{label}-{x}-{z}.fixture").resolve()
                if fixture_root.resolve() not in path.parents or "\t" in str(path) or "\n" in str(path):
                    raise ValueError("invalid fixture path")
                if not path.is_file():
                    raise ValueError("recorded fixture file is missing")
                if path.stat().st_size > CAP:
                    raise ValueError("recorded fixture file exceeds limit")
                digest = sha(path)
                if digest != row.get("before_sha256") or digest != row.get("after_sha256"):
                    raise ValueError("recorded fixture changed")
                data = path.read_bytes()
                entries = inspect_valid_file(data)
                if entries is None:
                    skipped += 1
                    continue
                entry = entries[x + 32 * z]
                if entry["location"]:
                    if row.get("root") != "ROOT" or row.get("full_drain") != "DRAIN":
                        skipped += 1
                        continue
                elif row.get("present") != "false":
                    raise ValueError("missing-entry capture disagrees")
                plans.append((group, row, path, digest, entries, entry))
        if any(not any(plan[0] == runtime + "-" + side and plan[5]["location"] for plan in plans)
               for runtime, side in GROUPS):
            raise ValueError("each recorded group needs a valid present entry")
        if len(plans) > 1024:
            raise ValueError("too many selected observations")
        requests = "".join(f"{path}\t{row['x']}\t{row['z']}\n" for _, row, path, _, _, _ in plans)
        (work / "requests.txt").write_text(requests, encoding="utf-8", newline="\n")
        with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
            result = subprocess.run([str(executable)], input=requests, text=True, encoding="utf-8",
                                    stdout=out, stderr=err, timeout=90, check=False)
            out.seek(0)
            err.seek(0)
            stdout, stderr = out.read(CAP + 1), err.read(CAP + 1)
        if len(stdout) > CAP or len(stderr) > CAP:
            raise ValueError("C probe output exceeds limit")
        (work / "stdout.txt").write_bytes(stdout)
        (work / "stderr.txt").write_bytes(stderr)
        if result.returncode:
            raise ValueError(f"C probe exited {result.returncode}: {stderr[:500]!r}")
        responses = [json.loads(line) for line in stdout.decode("utf-8").splitlines()]
        if len(responses) != len(plans):
            raise ValueError("C probe record count differs")
        records, order_only = [], 0
        for (group, row, path, digest, entries, expected), actual in zip(plans, responses):
            label = f"{group}/{row['fixture']}/{row['x']},{row['z']}"

            def check(condition, message):
                if not condition:
                    report["errors"].append(label + ": " + message)

            check(sha(path) == digest, "C inspection changed the file")
            check(actual["open_status"] == "ok", "open status")
            check(actual["view"] == {"length": path.stat().st_size, "trailing_bytes": 0,
                                     "present_entries": sum(bool(e["location"]) for e in entries),
                                     "invalid_entries": 0}, "view metadata")
            observed = actual["entry"]
            check(observed["location"] == int(row["packed"], 16) == expected["location"], "location bits")
            check(observed["timestamp"] == int(row["timestamp"], 16) == expected["timestamp"], "timestamp bits")
            check(observed["issues"] == 0, "unexpected diagnostics")
            if not expected["location"]:
                check(actual["record_status"] == "missing region entry", "missing status")
            else:
                check(actual["record_status"] == "ok", "record status")
                check(observed["byte_offset"] == expected["offset"] and
                      observed["extent_bytes"] == expected["extent"] and
                      observed["record_length"] == expected["length"] and
                      observed["selector"] == expected["selector"], "record boundaries")
                check(actual["payload_hex"] == expected["payload"].hex(), "declared payload")
                check(actual["compression_status"] == "ok", "compression status")
                check(actual["raw_hex"] == row["decompressed_hex"] == expected["raw"].hex(), "raw bytes")
                check(actual["compressed_consumed"] == expected["consumed"] and
                      actual["compressed_trailing_bytes"] == len(expected["payload"]) - expected["consumed"],
                      "compressed consumption")
                check(actual["nbt_status"] == "ok", "NBT status")
                canonical, recorded = bytes.fromhex(actual["canonical_hex"]), bytes.fromhex(row["canonical_root_hex"])
                if canonical != recorded:
                    same_ordered = compound_order_wire(canonical) == compound_order_wire(recorded)
                    check(same_ordered, "canonical bytes")
                    order_only += int(same_ordered)
                check(actual["nbt_consumed"] + actual["raw_trailing_bytes"] == len(expected["raw"]),
                      "raw consumption accounting")
            records.append({"group": group, "fixture": row["fixture"], "x": row["x"], "z": row["z"],
                            "input_sha256": digest, "C": actual})
        report.update(records=records, selected_records=len(plans), outside_profile=skipped,
                      compound_order_only=order_only)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report["errors"].append(str(error))
    report.update(status="fail" if report["errors"] else "pass",
                  finished_utc=datetime.now(timezone.utc).isoformat())
    try:
        save()
    except OSError as error:
        print("FAIL: cannot write result: " + str(error), file=sys.stderr)
        return 1
    if report["errors"]:
        for error in report["errors"][:20]:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print(f"PASS: {report['selected_records']} recorded observations; "
          f"{report['compound_order_only']} compound-order-only differences. Live region check pending.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
