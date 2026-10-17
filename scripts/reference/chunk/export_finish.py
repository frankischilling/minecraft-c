#!/usr/bin/env python3
"""Export lossless public finishing observations from an original-only capture."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import sys
import zlib

from compare_finish import cases, c_projection, replay_input, require


def digest(data):
    return hashlib.sha256(data).hexdigest()


def packed(data):
    require(len(data) <= 4 * 1024 * 1024, "public fixture expanded size bound")
    return {"bytes": len(data), "sha256": digest(data),
            "zlib_base64": base64.b64encode(zlib.compress(data, 9)).decode("ascii")}


def validate_capture_cases(report, corpus):
    rows = report["cases"]
    require(type(rows) is list and len(rows) == len(corpus), "full fixed corpus required for public fixtures")
    indices = [row["index"] for row in rows]
    require(all(type(number) is int for number in indices) and
            set(indices) == set(range(len(corpus))), "unique complete fixed case indices required")
    require(report["selected_cases"] == indices, "captured case selection/order mismatch")
    for row in rows:
        require(row["coordinates"] == corpus[row["index"]]["coordinates"], "fixed case coordinates changed")
        require(type(row["sides"]) is dict and set(row["sides"]) == {"client", "server"},
                "both original sides required for every case")


def export(manifest):
    require(manifest.stat().st_size <= 4 * 1024 * 1024, "manifest size bound")
    report = json.loads(manifest.read_bytes())
    require(report["status"] == "pass" and not report["errors"] and report["mode"] == "original-only",
            "successful original-only capture required")
    result = {"schema": 1, "manifest_sha256": digest(manifest.read_bytes()),
              "encoding": "zlib/base64; exact decoded bytes and JSON, without behavioral normalization",
              "cases": []}
    corpus = cases()
    validate_capture_cases(report, corpus)
    for case_record in report["cases"]:
        number = case_record["index"]
        require(type(number) is int and 0 <= number < len(corpus), "fixed case index")
        per_side = {}
        expected = None
        for side in ("client", "server"):
            directory = manifest.parent / f"case-{number}"
            snapshots_path = directory / (side + "-snapshots.json")
            trace_path = directory / (side + "-trace.json")
            side_report = case_record["sides"][side]
            require(side_report["control_equal"] is True, "untouched control must match")
            for path, record in ((snapshots_path, side_report["snapshot_record"]), (trace_path, side_report["trace_record"])):
                require(path.stat().st_size <= 32 * 1024 * 1024 and digest(path.read_bytes()) == record["sha256"],
                        "original record fingerprint")
            value, trace = json.loads(snapshots_path.read_bytes()), json.loads(trace_path.read_bytes())
            projection = c_projection(value, trace)
            if expected is not None:
                require(projection == expected, "both original projections must agree")
            expected = projection
            per_side[side] = {"snapshot_sha256": side_report["snapshot_record"]["sha256"],
                              "trace_sha256": side_report["trace_record"]["sha256"],
                              "input": packed(replay_input(value, corpus[number]))}
        encoded = (json.dumps(expected, separators=(",", ":"), allow_nan=False) + "\n").encode()
        result["cases"].append({"index": number, "coordinates": corpus[number]["coordinates"],
                                "sides": per_side, "expected": packed(encoded)})
    require(len(result["cases"]) == len(corpus), "full fixed corpus required for public fixtures")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        value = export(args.manifest.resolve(strict=True))
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            json.dump(value, output, indent=2, allow_nan=False)
            output.write("\n")
    except Exception as error:
        print("FAILURE: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
