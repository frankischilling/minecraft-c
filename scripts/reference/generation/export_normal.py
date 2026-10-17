#!/usr/bin/env python3
"""Export lossless normal-generation fixtures from a complete original-only capture."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import sys
import zlib

import compare_normal as normal


MAX_BYTES = 32 * 1024 * 1024
MAX_FIXTURE = 4 * 1024 * 1024
HELPERS = ("ChunkObservation.java", "NormalGenerationObservation.java", "NormalGenerationController.java")
require = normal.require


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(record, limit=MAX_BYTES):
    require(type(record) is dict and type(record["bytes"]) is int and 0 <= record["bytes"] <= limit and
            type(record["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is not None,
            "bounded complete fingerprint")
    return {"bytes": record["bytes"], "sha256": record["sha256"]}


def checked_bytes(path, record, limit=MAX_BYTES):
    expected = fingerprint(record, limit)
    require(not path.is_symlink() and path.is_file() and path.stat().st_size == expected["bytes"],
            "original file size/type: " + str(path))
    data = path.read_bytes()
    require(len(data) == expected["bytes"] and digest(data) == expected["sha256"], "original file fingerprint: " + str(path))
    return data


def packed(data):
    require(len(data) <= MAX_BYTES, "expanded fixture byte bound")
    return {"bytes": len(data), "sha256": digest(data),
            "zlib_base64": base64.b64encode(zlib.compress(data, 9)).decode("ascii")}


def source_name(path):
    name = str(path).replace("\\", "/")
    for prefix in ("scripts/", "tests/", "src/", "include/"):
        marker = "/" + prefix
        if marker in name:
            return prefix + name.rsplit(marker, 1)[1]
    return "CMakeLists.txt" if name.endswith("/CMakeLists.txt") else None


def validate_capture_cases(report):
    rows = report["cases"]
    require(type(rows) is list and len(rows) == len(normal.SEEDS), "all four fixed seeds required")
    seeds = [row["seed"] for row in rows]
    require(all(type(seed) is int for seed in seeds) and set(seeds) == set(normal.SEEDS), "unique complete fixed seeds")
    require(report["selected_seeds"] == seeds, "captured seed selection/order mismatch")
    for row in rows:
        require(row["coordinates"] == [list(pair) for pair in normal.COORDINATES], "all nine fixed ordered coordinates required")
        require(type(row["sides"]) is dict and set(row["sides"]) == {"client", "server"}, "both original sides required")
        require(row["comparisons"] == {"client/server": "pass"}, "complete original-only comparisons required")


def public_inventory(value):
    require(value["complete"] is True and value["error"] is None and type(value["entries"]) is list and
            len(value["entries"]) <= 4096, "complete bounded world inventory")
    return {key: value[key] for key in ("exists", "entries", "files", "directories", "bytes", "complete", "error")}


def provenance(report, directory, common, executed, finish):
    before, after = report["inputs_before"], report["inputs_after"]
    require(type(before) is dict and before and before == after, "all attributed capture inputs preserved")
    sources = {}
    for path, record in before.items():
        item = fingerprint(record, 512 * 1024 * 1024)
        name = source_name(path)
        if name is not None:
            require(name not in sources, "unique source attribution")
            sources[name] = item
    for side, expected in finish.JAR_HASHES.items():
        require(any(record["sha256"] == expected for record in before.values()), side + " original archive fingerprint missing")
    require(sources["scripts/reference/generation/compare_normal.py"]["sha256"] == digest(Path(normal.__file__).read_bytes()),
            "export projection/validation source differs from capture")
    copied_sources = directory / "source"
    require({path.name for path in copied_sources.iterdir()} == set(HELPERS), "complete copied helper source inventory")
    helper_sources = {}
    for name in HELPERS:
        relative = "tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "generation/") + name
        checked_bytes(copied_sources / name, sources[relative], 2 * 1024 * 1024)
        helper_sources[name] = sources[relative]
    classes = report["classes_before"]
    require(classes == report["classes_after"] and 3 <= len(classes) <= 16 and
            {path.name for path in (directory / "classes").iterdir()} == set(classes), "preserved complete helper class inventory")
    helper_classes = {}
    owners = tuple(name[:-5] for name in HELPERS)
    for name, record in classes.items():
        require(name.endswith(".class") and any(name == owner + ".class" or name.startswith(owner + "$") for owner in owners),
                "known helper class family")
        data = checked_bytes(directory / "classes" / name, record, 2 * 1024 * 1024)
        require(len(data) >= 8 and data[:4] == b"\xca\xfe\xba\xbe" and data[6:8] == b"\x00\x3d", "Java17 helper class")
        helper_classes[name] = fingerprint(record, 2 * 1024 * 1024)
    require(all(owner + ".class" in helper_classes for owner in owners), "all three helper entry classes required")
    attributed = {}
    for record in report["executed_source_modules"]:
        name = source_name(record["path"])
        require(name is not None and name not in attributed and sources[name] == fingerprint(record, 2 * 1024 * 1024),
                "executed helper source attribution")
        attributed[name] = sources[name]
    current = {source_name(record["path"]): fingerprint(record, 2 * 1024 * 1024) for record in executed}
    require(attributed == current, "executed export helpers differ from capture")
    return {"mode": "original-only", "jar_sha256": finish.JAR_HASHES, "source_fingerprints": dict(sorted(sources.items())),
            "helper_sources": helper_sources, "helper_classes": helper_classes, "executed_source_modules": attributed,
            "generation_calls": 72, "height_query_pairs": 73728, "stage_records": 1224}


def export(manifest):
    require(manifest.is_file() and not manifest.is_symlink() and manifest.stat().st_size <= MAX_FIXTURE, "capture manifest size/type")
    common, old, finish, executed = normal.helpers()
    raw_manifest = manifest.read_bytes()
    report = common.strict_json(raw_manifest)
    require(type(report["schema"]) is int and report["schema"] == 1 and report["status"] == "pass" and
            report["errors"] == [] and report["mode"] == "original-only", "successful original-only capture required")
    validate_capture_cases(report)
    origin = provenance(report, manifest.parent, common, executed, finish)
    origin["manifest_sha256"] = digest(raw_manifest)
    result = {"schema": 1, "encoding": "zlib/base64; exact MCG1 bytes and full c_projection JSON, without behavioral normalization",
              "provenance": origin, "cases": []}
    for row in report["cases"]:
        seed, per_side, expected = row["seed"], {}, None
        directory = manifest.parent / f"seed-{seed}"
        for side in ("client", "server"):
            captured = row["sides"][side]
            require(captured["control_equal"] is True, "untouched control must match")
            records = {}
            for kind, suffix in (("snapshot", "snapshots"), ("trace", "trace"), ("control", "control")):
                records[kind] = checked_bytes(directory / f"{side}-{suffix}.json", captured[kind + "_record"])
            require(records["snapshot"] == records["control"], "entire untouched control must be byte-equal")
            value, trace = common.strict_json(records["snapshot"]), common.strict_json(records["trace"])
            normal.validate_original(value, trace, seed, side, old, finish)
            projection = normal.c_projection(value, trace, finish)
            require(expected is None or projection == expected, "both original full projections must agree")
            expected = projection
            side_result = {kind + "_record": fingerprint(captured[kind + "_record"]) for kind in records}
            side_result.update(control_equal=True, jar_sha256=finish.JAR_HASHES[side], class_origins=value["class_origins"],
                               calls=9, stage_records=153, height_query_pairs=9216, input=packed(normal.replay_input(value, finish)))
            for kind in ("traced_world", "control_world"):
                inventory = public_inventory(captured[kind])
                world = directory / (side + ("-world" if kind == "traced_world" else "-control-world"))
                require(public_inventory(common.world_inventory(world, hashes=True)) == inventory, "captured world inventory changed")
                side_result[kind] = inventory
            per_side[side] = side_result
        encoded = (json.dumps(expected, separators=(",", ":"), allow_nan=False) + "\n").encode("ascii")
        result["cases"].append({"seed": seed, "seed_hex": f"{seed:016x}", "coordinates": [list(pair) for pair in normal.COORDINATES],
                                "sides": per_side, "expected": packed(encoded)})
    require(len(json.dumps(result, separators=(",", ":"))) <= MAX_FIXTURE, "public fixture file bound")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = export(args.manifest.absolute())
        encoded = (json.dumps(result, indent=2, allow_nan=False) + "\n").encode("ascii")
        require(len(encoded) <= MAX_FIXTURE, "formatted fixture file bound")
        with args.output.open("xb") as output:
            output.write(encoded)
    except Exception as error:
        print("FAILURE: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
