"""Replay eight original inputs through continuous nine-call C overworld generation."""

from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/generation/normal_fixtures.json"
MAX_BYTES = 32 * 1024 * 1024
MAX_FIXTURE = 4 * 1024 * 1024
SEEDS = (23, 24, 26, 51)
COORDINATES = ((1, 0), (0, 0), (1, 0), (-1, -1), (32768, 0), (2000000, 0),
               (-2000000, 0), (-2147483640, 0), (2147483638, 0))
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
HELPERS = ("ChunkObservation.java", "OverworldGenerationObservation.java", "OverworldGenerationController.java")
# Fixed fixtures retain their actually captured names and fingerprints. This
# admission selects one complete naming family; it does not attribute an old
# capture to the renamed helper sources or rewrite its recorded provenance.
LEGACY_HELPERS = ("ChunkObservation.java", "NormalGenerationObservation.java", "NormalGenerationController.java")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pairs(values):
    result = {}
    for key, value in values:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def strict_json(data):
    return json.loads(data, object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError("non-finite JSON number: " + value)))


def sha(value):
    return type(value) is str and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def validate_fingerprint(value):
    require(type(value) is dict and set(value) == {"bytes", "sha256"} and type(value["bytes"]) is int and
            0 <= value["bytes"] <= 512 * 1024 * 1024 and sha(value["sha256"]), "complete bounded fingerprint")


def unpack(value):
    require(type(value) is dict and set(value) == {"bytes", "sha256", "zlib_base64"} and type(value["bytes"]) is int and
            0 <= value["bytes"] <= MAX_BYTES and sha(value["sha256"]) and type(value["zlib_base64"]) is str and
            len(value["zlib_base64"]) <= MAX_FIXTURE, "bounded packed fixture")
    encoded = base64.b64decode(value["zlib_base64"], validate=True)
    decoder = zlib.decompressobj()
    result = decoder.decompress(encoded, value["bytes"] + 1)
    require(len(result) == value["bytes"] and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and
            hashlib.sha256(result).hexdigest() == value["sha256"], "complete fixture bytes/fingerprint")
    return result


def validate_inventory(value):
    require(type(value) is dict and value["complete"] is True and value["error"] is None and type(value["exists"]) is bool and
            type(value["entries"]) is list and len(value["entries"]) <= 4096, "complete bounded world inventory")
    seen, files, directories, total = set(), 0, 0, 0
    for entry in value["entries"]:
        name = entry["path"]
        require(type(name) is str and name and "\\" not in name and not PurePosixPath(name).is_absolute() and
                all(part not in ("", ".", "..") for part in name.split("/")) and name not in seen, "relative unique inventory entry")
        seen.add(name)
        if entry["kind"] == "file":
            validate_fingerprint({key: entry[key] for key in ("bytes", "sha256")})
            files += 1
            total += entry["bytes"]
        else:
            require(entry["kind"] == "directory", "known inventory entry kind")
            directories += 1
    require(all(type(value[key]) is int for key in ("files", "directories", "bytes")) and
            (value["files"], value["directories"], value["bytes"]) == (files, directories, total), "complete inventory counts")


def validate_fixture(value):
    require(type(value["schema"]) is int and value["schema"] == 1, "overworld fixture schema")
    origin = value["provenance"]
    require(origin["mode"] == "original-only" and sha(origin["manifest_sha256"]) and origin["jar_sha256"] == JAR_HASHES and
            (origin["generation_calls"], origin["height_query_pairs"], origin["stage_records"]) == (72, 73728, 1224),
            "complete original-only provenance")
    sources = origin["source_fingerprints"]
    require(type(sources) is dict and sources and type(origin["helper_sources"]) is dict,
            "complete original helper sources")
    helper_names = set(origin["helper_sources"])
    require(helper_names in (set(HELPERS), set(LEGACY_HELPERS)), "one complete original helper naming family")
    helpers = HELPERS if helper_names == set(HELPERS) else LEGACY_HELPERS
    for name, record in sources.items():
        require(type(name) is str and not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts,
                "relative source attribution")
        validate_fingerprint(record)
    for name in helpers:
        relative = "tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "generation/") + name
        require(origin["helper_sources"][name] == sources[relative], "copied helper source fingerprint")
    classes = origin["helper_classes"]
    owners = tuple(name[:-5] for name in helpers)
    require(type(classes) is dict and 3 <= len(classes) <= 16 and all(owner + ".class" in classes for owner in owners),
            "complete original helper class inventory")
    for name, record in classes.items():
        require(name.endswith(".class") and any(name == owner + ".class" or name.startswith(owner + "$") for owner in owners),
                "known helper class family")
        validate_fingerprint(record)
    require(type(origin["executed_source_modules"]) is dict and origin["executed_source_modules"], "executed source fingerprints")
    for name, record in origin["executed_source_modules"].items():
        require(record == sources[name], "executed source fingerprint")
    rows = value["cases"]
    require(type(rows) is list and len(rows) == len(SEEDS), "all four fixed overworld seeds required")
    seeds = [row["seed"] for row in rows]
    require(all(type(seed) is int for seed in seeds) and set(seeds) == set(SEEDS), "unique complete fixed overworld seeds")
    for row in rows:
        require(row["seed_hex"] == f"{row['seed']:016x}" and row["coordinates"] == [list(pair) for pair in COORDINATES],
                "all nine fixed ordered overworld coordinates")
        require(type(row["sides"]) is dict and set(row["sides"]) == {"client", "server"}, "both original overworld sides required")
        for side, record in row["sides"].items():
            require(record["control_equal"] is True and record["jar_sha256"] == JAR_HASHES[side] and
                    (record["calls"], record["stage_records"], record["height_query_pairs"]) == (9, 153, 9216), "all original calls/traces/controls")
            for kind in ("snapshot", "trace", "control"):
                validate_fingerprint(record[kind + "_record"])
            require(record["snapshot_record"] == record["control_record"], "untouched full snapshot fingerprint equality")
            origins = record["class_origins"]
            require(type(origins) is dict and 29 <= len(origins) <= 40 and len(set(origins.values())) == 1,
                    "original isolated class origins")
            for kind in ("traced_world", "control_world"):
                validate_inventory(record[kind])


def validate_projection(value, seed):
    require(type(value["schema"]) is int and value["schema"] == 1 and value["seed_hex"] == f"{seed:016x}" and
            type(value["constructor"]) is dict and type(value["calls"]) is list and len(value["calls"]) == len(COORDINATES),
            "full nine-call C projection")
    for index, row in enumerate(value["calls"]):
        require(type(row["index"]) is int and row["index"] == index and row["coordinates"] == list(COORDINATES[index]),
                "ordered projection call/coordinates")
        require(type(row["trace"]) is list and len(row["trace"]) == 1024 and
                all(type(query) is list and len(query) == 4 and all(type(number) is int for number in query) for query in row["trace"]),
                "complete 1024 height query records")


def validate_input(data, seed):
    position = 0

    def take(count):
        nonlocal position
        require(count >= 0 and count <= len(data) - position, "complete MCG1 input")
        result = data[position:position + count]
        position += count
        return result

    def read(format):
        return struct.unpack(format, take(struct.calcsize(format)))

    require(take(4) == b"MCG1", "MCG1 input magic")
    actual_seed, no_sky, depth, chunks, mappings, insertions, regions = read(">qBiIIII")
    require(actual_seed == seed and no_sky <= 1 and max(chunks, mappings, insertions) <= 512 and regions <= 32768 - 9 * 1024,
            "fixed seed and bounded MCG1 header")
    require(all(value >= 0 for value in read(">256i")), "nonnegative initialized opacity")
    take(26)
    for name in ("a", "b", "c", "d"):
        length, = read(">i")
        require(-1 <= length <= 4096, "bounded MCG1 manager descriptor")
        if length >= 0:
            raw = take(length if name == "d" else length * 8)
            if name == "d":
                require(all(ord("a") <= value <= ord("m") for value in raw), "recorded manager biomes")
    for _ in range(chunks):
        x, z, blank, dirty = read(">iiBB")
        require(blank <= 1 and dirty <= 1, "actual MCG1 chunk booleans")
        take(256)
    keys = set()
    for _ in range(mappings):
        key, chunk = read(">iI")
        require(key not in keys and chunk < chunks, "unique packed cache keys and real chunk identity")
        keys.add(key)
    for _ in range(insertions):
        require(read(">I")[0] < chunks, "ordered insertion identity")
    for _ in range(regions):
        require(read(">Iiiiiii")[0] <= 1, "actual MCG1 light kind")
    require(position == len(data), "no trailing MCG1 bytes")


class OverworldFixtureTests(unittest.TestCase):
    @staticmethod
    def minimal_fixture():
        record = {"bytes": 1, "sha256": "1" * 64}
        sources = {"tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "generation/") + name: record
                   for name in HELPERS}
        sources["scripts/reference/generation/compare_overworld.py"] = record
        inventory = {"exists": True, "entries": [], "files": 0, "directories": 0, "bytes": 0, "complete": True, "error": None}
        side = {"snapshot_record": record, "trace_record": record, "control_record": record, "control_equal": True,
                "calls": 9, "stage_records": 153, "height_query_pairs": 9216,
                "class_origins": {f"class{i}": "file:/synthetic.jar" for i in range(29)},
                "traced_world": inventory, "control_world": inventory}
        return {"schema": 1, "provenance": {"mode": "original-only", "manifest_sha256": "2" * 64,
            "jar_sha256": JAR_HASHES, "generation_calls": 72, "height_query_pairs": 73728, "stage_records": 1224,
            "source_fingerprints": sources, "helper_sources": {name: record for name in HELPERS},
            "helper_classes": {name[:-5] + ".class": record for name in HELPERS},
            "executed_source_modules": {"scripts/reference/generation/compare_overworld.py": record}},
            "cases": [{"seed": seed, "seed_hex": f"{seed:016x}", "coordinates": [list(pair) for pair in COORDINATES],
                       "sides": {name: dict(side, jar_sha256=JAR_HASHES[name]) for name in JAR_HASHES}} for seed in SEEDS]}

    def test_complete_fixed_fixture_required(self):
        fixture = self.minimal_fixture()
        validate_fixture(fixture)
        for mode in ("missing_seed", "duplicate_seed", "invalid_seed", "empty_sides", "missing_side", "extra_side", "coordinates", "missing_call",
                     "no_control", "control_digest", "source_omission", "class_omission", "trace_count"):
            value = copy.deepcopy(fixture)
            row = value["cases"][0]
            if mode == "missing_seed": value["cases"].pop()
            elif mode == "duplicate_seed": value["cases"][-1] = copy.deepcopy(row)
            elif mode == "invalid_seed": row["seed"] = 25
            elif mode == "empty_sides": row["sides"] = {}
            elif mode == "missing_side": del row["sides"]["server"]
            elif mode == "extra_side": row["sides"]["other"] = {}
            elif mode == "coordinates": row["coordinates"][2] = [2, 0]
            elif mode == "missing_call": row["coordinates"].pop()
            elif mode == "no_control": row["sides"]["client"]["control_equal"] = False
            elif mode == "control_digest": row["sides"]["client"]["control_record"] = {"bytes": 1, "sha256": "3" * 64}
            elif mode == "source_omission": del value["provenance"]["source_fingerprints"]
            elif mode == "class_omission": value["provenance"]["helper_classes"].clear()
            else: row["sides"]["client"]["height_query_pairs"] -= 1
            with self.subTest(mode=mode), self.assertRaises((ValueError, KeyError)):
                validate_fixture(value)

    def test_complete_projection_call_order_required(self):
        value = {"schema": 1, "seed_hex": "0000000000000017", "constructor": {}, "calls": [
            {"index": number, "coordinates": list(pair), "trace": [[0, 0, 0, 0]] * 1024} for number, pair in enumerate(COORDINATES)]}
        validate_projection(value, 23)
        for mode in ("missing", "duplicate", "coordinates", "short_trace"):
            changed = copy.deepcopy(value)
            if mode == "missing": changed["calls"].pop()
            elif mode == "duplicate": changed["calls"][2] = changed["calls"][0]
            elif mode == "coordinates": changed["calls"][0]["coordinates"] = [0, 1]
            else: changed["calls"][0]["trace"].pop()
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                validate_projection(changed, 23)

    def test_lossless_bytes_reject_corruption_and_incomplete_streams(self):
        raw = b"MCG1" + bytes(range(256))
        value = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
                 "zlib_base64": base64.b64encode(zlib.compress(raw)).decode("ascii")}
        self.assertEqual(unpack(value), raw)
        for mode in ("digest", "length", "base64", "truncated", "trailing", "second_stream", "corrupt", "bomb"):
            changed = dict(value)
            stream = base64.b64decode(value["zlib_base64"])
            if mode == "digest": changed["sha256"] = "0" * 64
            elif mode == "length": changed["bytes"] -= 1
            elif mode == "base64": changed["zlib_base64"] = "!"
            elif mode == "truncated": changed["zlib_base64"] = base64.b64encode(stream[:-1]).decode("ascii")
            elif mode == "trailing": changed["zlib_base64"] = base64.b64encode(stream + b"x").decode("ascii")
            elif mode == "second_stream": changed["zlib_base64"] = base64.b64encode(stream + stream).decode("ascii")
            elif mode == "corrupt": changed["zlib_base64"] = base64.b64encode(stream[:-1] + bytes((stream[-1] ^ 1,))).decode("ascii")
            else: changed["zlib_base64"] = base64.b64encode(zlib.compress(b"x" * 1048576)).decode("ascii")
            with self.subTest(mode=mode), self.assertRaises((ValueError, zlib.error)):
                unpack(changed)

    def test_input_seed_bytes_and_full_consumption(self):
        data = b"MCG1" + struct.pack(">qBiIIII", 23, 0, 0, 0, 0, 0, 0) + bytes(1024 + 26) + struct.pack(">4i", -1, -1, -1, -1)
        validate_input(data, 23)
        for changed, seed in ((data, 24), (b"BAD1" + data[4:], 23), (data[:-1], 23), (data + b"x", 23)):
            with self.subTest(seed=seed, size=len(changed)), self.assertRaises(ValueError):
                validate_input(changed, seed)

    def test_export_requires_complete_original_selection_and_record_fingerprints(self):
        def load(name, path):
            spec = importlib.util.spec_from_file_location(name, path)
            result = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(result)
            return result
        overworld = load("normal_export_driver_test", ROOT / "scripts/reference/generation/compare_overworld.py")
        with patch.dict(sys.modules, {"compare_overworld": overworld}):
            exporter = load("normal_export_test", ROOT / "scripts/reference/generation/export_overworld.py")
        report = {"selected_seeds": list(SEEDS), "cases": [
            {"seed": seed, "coordinates": [list(pair) for pair in COORDINATES], "sides": {"client": {}, "server": {}},
             "comparisons": {"client/server": "pass"}} for seed in SEEDS]}
        exporter.validate_capture_cases(report)
        for mode in ("missing", "duplicate", "invalid", "side", "coordinates", "order"):
            value = copy.deepcopy(report)
            if mode == "missing": value["cases"].pop()
            elif mode == "duplicate": value["cases"][-1] = value["cases"][0]
            elif mode == "invalid": value["cases"][0]["seed"] = 25
            elif mode == "side": del value["cases"][0]["sides"]["client"]
            elif mode == "coordinates": value["cases"][0]["coordinates"][0] = [0, 1]
            else: value["selected_seeds"].reverse()
            with self.subTest(mode=mode), self.assertRaises(overworld.Failure):
                exporter.validate_capture_cases(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "record"
            path.write_bytes(b"original record")
            record = {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            self.assertEqual(exporter.checked_bytes(path, record), b"original record")
            path.write_bytes(b"corrupt! record")
            with self.assertRaises(overworld.Failure):
                exporter.checked_bytes(path, record)
            manifest = Path(directory) / "manifest.json"
            for mode in ("failed", "three_way", "errors"):
                value = {"schema": 1, "status": "pass", "errors": [], "mode": "original-only"}
                if mode == "failed": value["status"] = "fail"
                elif mode == "three_way": value["mode"] = "three-way"
                else: value["errors"].append("original comparison failed")
                manifest.write_text(json.dumps(value), encoding="utf-8")
                with self.subTest(mode=mode), self.assertRaisesRegex(overworld.Failure, "successful original-only capture required"):
                    exporter.export(manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    require(FIXTURE.stat().st_size <= MAX_FIXTURE, "public fixture file bound")
    fixture = strict_json(FIXTURE.read_bytes())
    validate_fixture(fixture)
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="mc-overworld-") as directory:
        for row in fixture["cases"]:
            expected = strict_json(unpack(row["expected"]))
            validate_projection(expected, row["seed"])
            for side, record in row["sides"].items():
                raw = unpack(record["input"])
                validate_input(raw, row["seed"])
                source = Path(directory) / f"seed-{row['seed']}-{side}.bin"
                source.write_bytes(raw)
                # One adapter process owns all nine calls and retained results.
                process = subprocess.run([str(executable), str(source)], capture_output=True, timeout=45, check=True)
                require(len(process.stdout) <= MAX_BYTES and not process.stderr and strict_json(process.stdout) == expected,
                        f"complete original/C overworld generation mismatch: seed {row['seed']} {side}")
    print("overworld generation fixtures: 8 original inputs, 72 complete chunks, 73728 exact height queries")


if __name__ == "__main__":
    main()
