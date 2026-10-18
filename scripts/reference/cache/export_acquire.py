#!/usr/bin/env python3
"""Export lossless cache acquisition fixtures from a complete original-only capture."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
import zlib

import compare_acquire as acquire


MAX_INPUT = 4 * 1024 * 1024
MAX_ORIGINAL = 32 * 1024 * 1024
MAX_EXPECTED = 64 * 1024 * 1024
MAX_FIXTURE = 8 * 1024 * 1024
HELPERS = ("ChunkObservation.java", "CacheAcquisitionObservation.java", "CacheAcquisitionController.java")
require = acquire.require


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(record, limit=MAX_ORIGINAL):
    require(type(record) is dict and type(record.get("bytes")) is int and 0 < record["bytes"] <= limit and
            type(record.get("sha256")) is str and re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is not None,
            "bounded complete fingerprint")
    return {"bytes": record["bytes"], "sha256": record["sha256"]}


def checked_bytes(path, record, limit=MAX_ORIGINAL):
    expected = fingerprint(record, limit)
    require(not path.is_symlink() and path.is_file() and path.stat().st_size == expected["bytes"],
            "retained file size/type: " + str(path))
    if "path" in record:
        require(Path(record["path"]).resolve() == path.resolve(), "retained record path mismatch")
    data = path.read_bytes()
    require(len(data) == expected["bytes"] and digest(data) == expected["sha256"],
            "retained file fingerprint: " + str(path))
    return data


def packed(data, limit):
    require(0 < len(data) <= limit, "expanded fixture byte bound")
    return {"bytes": len(data), "sha256": digest(data),
            "zlib_base64": base64.b64encode(zlib.compress(data, 9)).decode("ascii")}


def public_protocol(value, trace, side):
    """Retain every behavioral record; replace only machine-local provenance paths."""
    snapshot = json.loads(json.dumps(value, allow_nan=False))
    measured = json.loads(json.dumps(trace, allow_nan=False))
    snapshot["class_origins"] = {name: "supplied:" + side for name in snapshot["class_origins"]}
    command = measured["child_command"]
    require(type(command) is list and len(command) == 14, "complete original child command")
    for index, logical in ((0, "tool:java"), (5, "helper:classes"), (7, "supplied:" + side),
                           (11, "case:world"), (12, "case:snapshots.json")):
        command[index] = logical
    for name in ("child_stdout", "child_stderr"):
        file = measured[name]["file"]
        require(type(file) is str and Path(file).name == file and "/" not in file and "\\" not in file,
                "relative child stream filename")
    encoded = [(json.dumps(item, separators=(",", ":"), allow_nan=False) + "\n").encode("ascii")
               for item in (snapshot, measured)]
    require(sum(map(len, encoded)) <= MAX_ORIGINAL, "bounded complete public protocol pair")
    for data in encoded:
        require(re.search(rb"[A-Za-z]:[\\/]", data) is None and b"file:/" not in data,
                "public protocol contains a private host path")
    return {name: packed(data, MAX_ORIGINAL) for name, data in zip(("snapshot", "trace"), encoded)}


def source_name(path):
    name = str(path).replace("\\", "/")
    for prefix in ("scripts/", "tests/", "src/", "include/"):
        marker = "/" + prefix
        if marker in name:
            return prefix + name.rsplit(marker, 1)[1]
    return "CMakeLists.txt" if name.endswith("/CMakeLists.txt") else None


def validate_capture_cases(report):
    require(report.get("selected_cases") == [0, 1, 2] and all(type(index) is int for index in report["selected_cases"]) and
            type(report.get("cases")) is list and
            len(report["cases"]) == 3 and report.get("client/server") == "pass", "all three ordered acquisition contexts required")
    for index, (row, context) in enumerate(zip(report["cases"], acquire.CASES)):
        require(type(row) is dict and type(row.get("index")) is int and row["index"] == index and
                (row.get("side"), row.get("mode"), row.get("scenario")) == context and row.get("comparisons") == {} and
                row.get("control_equal") is True and row.get("operations") == len(acquire.plan(context[2])) and
                row.get("queries") == (2048 if index < 2 else 1024), "complete original-only fixed acquisition case")
        require(not any(name in row for name in ("c_process", "c_record", "unavailable")), "original-only case evidence")
        for name in ("controller", "control"):
            process = row[name]
            require(type(process) is dict and type(process.get("returncode")) is int and process["returncode"] == 0 and
                    process.get("timed_out") is False and process.get("stop_reason") is None,
                    "successful complete original process")


def public_inventory(value):
    require(type(value) is dict and value.get("complete") is True and value.get("error") is None and
            type(value.get("exists")) is bool and type(value.get("entries")) is list and
            len(value["entries"]) <= 4096, "complete bounded world inventory")
    seen, files, directories, total = set(), 0, 0, 0
    for entry in value["entries"]:
        name = entry["path"]
        require(type(name) is str and name and "\\" not in name and ":" not in name and
                not PurePosixPath(name).is_absolute() and all(part not in ("", ".", "..") for part in name.split("/")) and
                name not in seen, "unique relative world inventory path")
        seen.add(name)
        if entry["kind"] == "file":
            require(set(entry) == {"path", "kind", "bytes", "sha256"}, "complete file inventory entry")
            # Empty files are legitimate inventory entries.
            require(type(entry["bytes"]) is int and 0 <= entry["bytes"] <= 64 * 1024 * 1024 and
                    type(entry["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]), "world file fingerprint")
            files += 1
            total += entry["bytes"]
        else:
            require(entry == {"path": name, "kind": "directory"}, "known inventory directory")
            directories += 1
    require(all(type(value.get(name)) is int for name in ("files", "directories", "bytes")) and
            (value["files"], value["directories"], value["bytes"]) == (files, directories, total), "world inventory totals")
    return {key: value[key] for key in ("exists", "entries", "files", "directories", "bytes", "complete", "error")}


def validate_commands(report, row, trace, directory, jar, common):
    java = row["controller"]["command"][0]
    require(type(java) is str and java in report["inputs_before"], "attributed original Java executable")
    classes = str(directory.parent / "classes")
    arguments = [str(jar), row["side"], row["mode"], acquire.SEED,
                 str(directory / "world"), str(directory / "snapshots.json"), row["scenario"]]
    require(row["controller"]["command"] == [java, "--add-modules", "jdk.jdi", "-Xmx512m", "-cp", classes,
            "CacheAcquisitionController", *arguments, str(directory / "trace.json")], "exact attributed controller command")
    require(trace["child_command"] == [java, "-Xint", "-Xmx512m",
            "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", classes,
            "CacheAcquisitionObservation", *arguments], "exact attributed traced child command")
    control_arguments = [str(jar), row["side"], row["mode"], acquire.SEED,
                         str(directory / "control-world"), str(directory / "control.json"), row["scenario"]]
    require(row["control"]["command"] == [java, "-Xint", "-Xmx512m", "-cp", classes,
            "CacheAcquisitionObservation", *control_arguments], "exact attributed untouched child command")
    for name in ("child_stdout", "child_stderr"):
        record = trace[name]
        require(type(record) is dict and set(record) == {"file", "bytes", "sha256"} and
                record["file"] == "trace.json." + ("child-stdout.txt" if name == "child_stdout" else "child-stderr.txt"),
                "complete bounded original child stream attribution")
        actual = common.file_record(directory / record["file"], byte_limit=MAX_ORIGINAL)
        require({key: actual[key] for key in ("bytes", "sha256")} == {key: record[key] for key in ("bytes", "sha256")},
                "retained actual child stream fingerprint")


def provenance(report, directory, common, executed, finish):
    before = report["inputs_before"]
    require(type(before) is dict and before and before == report["inputs_after"], "all attributed inputs preserved")
    sources, jars = {}, {}
    for path, record in before.items():
        require(type(path) is str and record.get("path") == path, "complete input path attribution")
        expected = fingerprint(record, 512 * 1024 * 1024)
        actual = Path(path)
        common.no_link_components(actual)
        require(fingerprint(common.file_record(actual, byte_limit=512 * 1024 * 1024), 512 * 1024 * 1024) == expected,
                "attributed input bytes changed: " + path)
        name = source_name(path)
        if name is not None:
            require(name not in sources, "unique source attribution")
            sources[name] = expected
        for side, jar_hash in finish.JAR_HASHES.items():
            if expected["sha256"] == jar_hash:
                require(side not in jars, "unique selected original archive")
                jars[side] = actual
    require(set(jars) == {"client", "server"}, "both pinned original archives attributed")
    driver = "scripts/reference/cache/compare_acquire.py"
    require(sources[driver]["sha256"] == digest(Path(acquire.__file__).read_bytes()),
            "executing acquisition projection source differs from capture")
    copied = directory / "source"
    common.no_link_components(copied)
    require({path.name for path in copied.iterdir()} == set(HELPERS), "complete copied helper source inventory")
    helper_sources = {}
    for name in HELPERS:
        relative = "tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name
        common.no_link_components(copied / name)
        checked_bytes(copied / name, sources[relative], 2 * 1024 * 1024)
        helper_sources[name] = sources[relative]
    classes, owners = report["classes_before"], tuple(name[:-5] for name in HELPERS)
    common.no_link_components(directory / "classes")
    require(type(classes) is dict and classes == report["classes_after"] and 3 <= len(classes) <= 16 and
            {path.name for path in (directory / "classes").iterdir()} == set(classes), "preserved complete helper class inventory")
    helper_classes = {}
    for name, record in classes.items():
        require(re.fullmatch(r"[A-Za-z0-9_$]+\.class", name) and
                any(name == owner + ".class" or name.startswith(owner + "$") for owner in owners), "known helper class family")
        common.no_link_components(directory / "classes" / name)
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
    require(attributed == current, "executing export helpers differ from capture")
    return {"mode": "original-only", "jar_sha256": finish.JAR_HASHES,
            "source_fingerprints": dict(sorted(sources.items())), "helper_sources": helper_sources,
            "helper_classes": helper_classes, "helper_class_version": 61, "executed_source_modules": attributed,
            "cases": 3, "operations": 20, "generation_calls": 5, "height_query_pairs": 5120,
            "marker_inventories": 40}, jars


def export(manifest):
    require(manifest.is_file() and not manifest.is_symlink() and manifest.stat().st_size <= MAX_FIXTURE, "capture manifest size/type")
    common, old, finish, normal, executed = acquire.helpers()
    common.no_link_components(manifest)
    raw_manifest = manifest.read_bytes()
    report = common.strict_json(raw_manifest)
    require(type(report.get("schema")) is int and report["schema"] == 1 and report.get("status") == "pass" and
            report.get("errors") == [] and report.get("mode") == "original-only", "successful original-only capture required")
    validate_capture_cases(report)
    origin, jars = provenance(report, manifest.parent, common, executed, finish)
    origin["manifest_sha256"] = digest(raw_manifest)
    result = {"schema": 1,
              "encoding": "zlib/base64; exact MCA1 bytes and full acquisition projection JSON",
              "projection": "Signed-key sorted map/pending membership; exact insertion-list and queue order; no collection iteration-order claim.",
              "provenance": origin, "cases": []}
    shared, total_events = {}, 0
    for row in report["cases"]:
        index, side, mode, scenario = (row[name] for name in ("index", "side", "mode", "scenario"))
        directory = manifest.parent / f"case-{index}-{side}-{scenario}"
        common.no_link_components(directory)
        records = {}
        for kind, name, limit in (("snapshot_record", "snapshots.json", MAX_ORIGINAL),
                                  ("trace_record", "trace.json", MAX_ORIGINAL),
                                  ("control_record", "control.json", MAX_ORIGINAL),
                                  ("replay_input", "input.mca", MAX_INPUT),
                                  ("projection_record", "expected.json", MAX_EXPECTED)):
            common.no_link_components(directory / name)
            records[kind] = checked_bytes(directory / name, row[kind], limit)
        require(records["snapshot_record"] == records["control_record"], "entire untouched control must be byte-equal")
        value, trace = common.strict_json(records["snapshot_record"]), common.strict_json(records["trace_record"])
        validate_commands(report, row, trace, directory, jars[side], common)
        # Expected behavior is derived only from the original records, never C output.
        expected = acquire.validate_original(value, trace, side, mode, scenario, jars[side], normal, finish)
        replay = acquire.replay_input(value, finish)
        require(replay == records["replay_input"] and common.strict_json(records["projection_record"]) == expected,
                "saved replay input/projection differs from independently revalidated original")
        operations = len(expected["operations"])
        queries = sum(len(entry["queries"]) for operation in expected["operations"] for entry in operation["finishes"])
        generations = sum(len(operation["finishes"]) for operation in expected["operations"])
        events = sum(len(operation["events"]) for operation in trace["operations"])
        require((operations, queries, generations) == ((6, 2048, 2) if index < 2 else (8, 1024, 1)) and
                type(row["stage_events"]) is int and row["stage_events"] == events, "complete original operation/query/event counts")
        total_events += events
        if scenario == "shared":
            shared[side] = expected
        public = {name: row[name] for name in ("index", "side", "mode", "scenario")}
        public.update(seed_hex=acquire.SEED, schedule=[list(entry) for entry in acquire.plan(scenario)],
                      operations=operations, generation_calls=generations, height_query_pairs=queries,
                      stage_events=events, marker_inventories=2 * operations, control_equal=True,
                      jar_sha256=finish.JAR_HASHES[side], bindings=value["bindings"],
                      class_origins={name: finish.JAR_HASHES[side] for name in value["class_origins"]},
                      input=packed(replay, MAX_INPUT), expected=packed(records["projection_record"], MAX_EXPECTED),
                      protocol=public_protocol(value, trace, side))
        for kind in records:
            public[kind] = fingerprint(row[kind], MAX_EXPECTED if kind == "projection_record" else MAX_ORIGINAL)
        for kind in ("world", "control_world"):
            inventory = public_inventory(row[kind])
            path = directory / ("world" if kind == "world" else "control-world")
            require(public_inventory(common.world_inventory(path, hashes=True)) == inventory, "retained world inventory changed")
            public[kind] = inventory
        result["cases"].append(public)
    require(shared["client"] == shared["server"], "complete shared client/server original projection differs")
    origin["stage_events"] = total_events
    require(len(json.dumps(result, separators=(",", ":"), allow_nan=False).encode("ascii")) <= MAX_FIXTURE, "public fixture file bound")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=acquire.ROOT / "tests/reference/cache/acquire_fixtures.json")
    args = parser.parse_args(argv)
    try:
        result = export(args.manifest.absolute())
        encoded = (json.dumps(result, indent=2, allow_nan=False) + "\n").encode("ascii")
        require(len(encoded) <= MAX_FIXTURE, "formatted public fixture file bound")
        with args.output.open("xb") as output:
            output.write(encoded)
    except Exception as error:
        print("FAILURE: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
