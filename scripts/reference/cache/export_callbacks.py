#!/usr/bin/env python3
"""Export callback fixtures from complete original-only observations.

Original method bytecode remains in private captures. Public records retain its
pinned identity, length and SHA-256 together with complete behavioral evidence.
"""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import re
import sys

import compare_callbacks as callbacks
import export_acquire as shared


MAX_FIXTURE = 8 * 1024 * 1024
MAX_ORIGINAL = callbacks.MAX_RECORD
MAX_REPLAY = callbacks.MAX_C_RECORD
HELPERS = tuple(Path(name).name for name in callbacks.SOURCES)
require = callbacks.require
digest = shared.digest
fingerprint = shared.fingerprint
checked_bytes = shared.checked_bytes
packed = shared.packed


def successful(process):
    require(type(process) is dict and type(process.get("returncode")) is int and
            process["returncode"] == 0 and process.get("timed_out") is False and
            process.get("stop_reason") is None, "successful complete original process")


def artifact_bytes(common, path, limit):
    common.no_link_components(path)
    record = common.file_record(path, byte_limit=limit)
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit and len(data) == record["bytes"] and digest(data) == record["sha256"],
            "bounded stable retained artifact: " + str(path))
    return data


def provenance(report, directory, common, executed, finish):
    before = report["inputs_before"]
    require(type(before) is dict and before and callbacks.same_json(before, report["inputs_after"]),
            "all attributed inputs preserved")
    sources, jars = {}, {}
    for path, record in before.items():
        require(type(path) is str and record.get("path") == path, "complete input path attribution")
        expected = fingerprint(record, 512 * 1024 * 1024)
        actual = Path(path)
        common.no_link_components(actual)
        require(fingerprint(common.file_record(actual, byte_limit=512 * 1024 * 1024),
                            512 * 1024 * 1024) == expected,
                "attributed input bytes changed: " + path)
        name = shared.source_name(path)
        if name is not None:
            require(name not in sources, "unique source attribution")
            sources[name] = expected
        for side, jar_hash in finish.JAR_HASHES.items():
            if expected["sha256"] == jar_hash:
                require(side not in jars, "unique selected original archive")
                jars[side] = actual
    require(set(jars) == {"client", "server"}, "both pinned original archives attributed")
    require(sources["scripts/reference/cache/compare_callbacks.py"]["sha256"] ==
            digest(Path(callbacks.__file__).read_bytes()), "executing callback driver differs from capture")
    copied = directory / "source"
    common.no_link_components(copied)
    require({path.name for path in copied.iterdir()} == set(HELPERS),
            "complete copied helper source inventory")
    helper_sources = {}
    for relative in callbacks.SOURCES:
        name = Path(relative).name
        common.no_link_components(copied / name)
        checked_bytes(copied / name, sources[relative], 2 * 1024 * 1024)
        helper_sources[name] = sources[relative]
    classes = report["classes_before"]
    owners = tuple(name[:-5] for name in HELPERS)
    common.no_link_components(directory / "classes")
    require(type(classes) is dict and callbacks.same_json(classes, report["classes_after"]) and
            3 <= len(classes) <= 16 and
            {path.name for path in (directory / "classes").iterdir()} == set(classes),
            "preserved complete helper class inventory")
    helper_classes = {}
    for name, record in classes.items():
        require(re.fullmatch(r"[A-Za-z0-9_$]+\.class", name) and
                any(name == owner + ".class" or name.startswith(owner + "$") for owner in owners),
                "known project helper class family")
        common.no_link_components(directory / "classes" / name)
        data = checked_bytes(directory / "classes" / name, record, 2 * 1024 * 1024)
        require(len(data) >= 8 and data[:4] == b"\xca\xfe\xba\xbe" and data[6:8] == b"\x00\x3d",
                "Java17 project helper class")
        helper_classes[name] = fingerprint(record, 2 * 1024 * 1024)
    require(all(owner + ".class" in helper_classes for owner in owners),
            "all three helper entry classes required")
    attributed = {}
    for record in report["executed_source_modules"]:
        name = shared.source_name(record["path"])
        require(name is not None and name not in attributed and
                sources[name] == fingerprint(record, 2 * 1024 * 1024),
                "executed source module attribution")
        attributed[name] = sources[name]
    current = {shared.source_name(record["path"]): fingerprint(record, 2 * 1024 * 1024)
               for record in executed}
    require(attributed == current, "executing export interpretation differs from capture")
    successful(report["compile"])
    require(report["compile"]["command"][0] in before, "attributed Java compiler")
    modules = report["jdk_module_process"]
    successful(modules)
    java = modules["command"][0]
    require(java in before and modules["command"] == [java, "--list-modules"],
            "exact attributed JDK module preflight")
    raw_modules = artifact_bytes(common, directory / "jdk-modules-out.txt", MAX_ORIGINAL)
    available = {line.split("@", 1)[0] for line in raw_modules.decode("utf-8").splitlines()}
    require({"jdk.jdi", "jdk.jdwp.agent"}.issubset(available), "required actual debugger modules")
    versions = {}
    for name in ("java", "javac", "javap"):
        record = report["versions"][name]
        successful(record)
        require(record["command"] == [record["command"][0], "-version"] and
                record["command"][0] in before and type(record["major"]) is int and
                record["major"] >= 17, "attributed JDK17 tools")
        output = (artifact_bytes(common, directory / (name + "-version-stdout.txt"), 65536) + b"\n" +
                  artifact_bytes(common, directory / (name + "-version-stderr.txt"), 65536))
        require(len(output) <= 131073 and output.decode("utf-8") == record["output"],
                "actual retained tool version output")
        versions[name] = {"major": record["major"], "output": record["output"],
                          "executable": fingerprint(before[record["command"][0]], 512 * 1024 * 1024)}
    compiler = report["versions"]["javac"]["command"][0]
    require(report["compile"]["command"] == [compiler, "-J-Xmx512m", "--release", "17",
        "--add-modules", "jdk.jdi", "-proc:none", "-implicit:none", "-encoding", "UTF-8",
        "-Xlint:all", "-Werror", "-classpath", str(directory / "empty"),
        "-sourcepath", str(directory / "empty"), "--module-path", str(directory / "empty"),
        "-d", str(directory / "classes"),
        *[str(directory / "source" / name) for name in HELPERS]],
        "exact bounded compile command and three owned copied sources")
    return {"mode": "original-only", "jar_sha256": finish.JAR_HASHES,
        "source_fingerprints": dict(sorted(sources.items())), "helper_sources": helper_sources,
        "helper_classes": helper_classes, "helper_class_version": 61,
        "executed_source_modules": attributed, "tool_versions": versions,
        "jdk_modules": {"required": ["jdk.jdi", "jdk.jdwp.agent"],
                        "bytes": len(raw_modules), "sha256": digest(raw_modules)},
        "exporter_sha256": digest(Path(__file__).read_bytes())}, jars, java


def public_snapshot(value, side):
    result = copy.deepcopy(value)
    result["class_origins"] = {name: "supplied:" + side for name in result["class_origins"]}
    return result


def public_protocol(value, trace, side):
    snapshot, measured = public_snapshot(value, side), copy.deepcopy(trace)
    command = measured["child_command"]
    require(type(command) is list and len(command) == 13, "complete original child command")
    for index, logical in ((0, "tool:java"), (5, "helper:classes"), (7, "supplied:" + side),
                           (11, "case:world"), (12, "case:snapshots.json")):
        command[index] = logical
    for binding in measured["bindings"].values():
        require("bytecode_hex" in binding, "complete private original bytecode proof")
        del binding["bytecode_hex"]
    result = {"binding_representation": "fingerprints-only"}
    for name, record in (("snapshot", snapshot), ("trace", measured)):
        encoded = (json.dumps(record, separators=(",", ":"), allow_nan=False) + "\n").encode("ascii")
        require(re.search(rb"[A-Za-z]:[\\/]", encoded) is None and b"file:/" not in encoded and
                b'"bytecode_hex"' not in encoded,
                "public protocol contains a host path or original method bytes")
        result[name] = packed(encoded, MAX_ORIGINAL)
    return result


def retained_inventory(common, record, path):
    value = shared.public_inventory(record)
    require(value["files"] + value["directories"] <= 256 and value["bytes"] <= MAX_ORIGINAL,
            "bounded complete callback world inventory")
    current = shared.public_inventory(callbacks.fingerprint_inventory(common, path))
    require(callbacks.same_json(current, value), "retained world inventory changed")
    return value


def export(manifest):
    require(manifest.is_file() and not manifest.is_symlink() and manifest.stat().st_size <= MAX_FIXTURE,
            "bounded ordinary capture manifest")
    common, old, finish, normal, acquire, executed = callbacks.helpers()
    protocol, source = callbacks.load_protocol()
    executed.append(source)
    common.no_link_components(manifest)
    raw_manifest = manifest.read_bytes()
    report = common.strict_json(raw_manifest)
    require(type(report.get("schema")) is int and report["schema"] == 1 and
            report.get("scope") == "cache-callback-boundary" and report.get("status") == "pass" and
            report.get("errors") == [] and report.get("mode") == "original-only" and
            report.get("selected_cases") == [0, 1, 2, 3] and
            all(type(index) is int for index in report["selected_cases"]) and
            report.get("client/server") == {"load": "pass", "population": "pass"},
            "complete successful original-only callback capture required")
    require(type(report["cases"]) is list and len(report["cases"]) == 4 and
            type(report["preparations"]) is dict and set(report["preparations"]) == {"client", "server"},
            "all four contexts and both genuine saved baselines required")
    origin, jars, java = provenance(report, manifest.parent, common, executed, finish)
    origin["manifest_sha256"] = digest(raw_manifest)
    result = {"schema": 1, "format": "MCC1", "scope": "cache-callback-boundary",
        "encoding": "zlib/base64; exact MCC1 inputs, complete projections and behavioral protocol",
        "external_effects": "Original loader, generator and terrain-feature bodies are observed callback effects; C computes coordinator transitions only.",
        "projection": "Signed-key sorted map/pending membership; exact insertion and queue order; blank backing arrays are outside C payload dispatch.",
        "provenance": origin, "preparations": {}, "cases": []}
    prepared = {}
    classes = str(manifest.parent / "classes")
    for side, row in report["preparations"].items():
        directory = manifest.parent / ("prepare-" + side)
        arguments = [str(jars[side]), side, "prepare", callbacks.SEED,
                     str(directory / "world"), str(directory / "snapshots.json")]
        successful(row["process"])
        require(row["process"]["command"] == [java, "-Xint", "-Xmx512m", "-cp", classes,
                "CacheCallbacksObservation", *arguments], "exact actual world creator command")
        raw = checked_bytes(directory / "snapshots.json", row["snapshot_record"], MAX_ORIGINAL)
        value = common.strict_json(raw)
        callbacks.validate_observation(value, side, "prepare", jars[side], acquire, finish)
        require(callbacks.same_json(row["baseline_before"], row["baseline_after"]),
                "saved baseline unchanged")
        world = retained_inventory(common, row["world_after"], directory / "world")
        baseline = retained_inventory(common, row["baseline_before"], directory / "baseline")
        require(callbacks.same_json(world, baseline), "complete saved world copied to immutable baseline")
        encoded = (json.dumps(public_snapshot(value, side), separators=(",", ":"), allow_nan=False) + "\n").encode("ascii")
        require(re.search(rb"[A-Za-z]:[\\/]", encoded) is None and b"file:/" not in encoded,
                "public creator snapshot contains a host path")
        result["preparations"][side] = {"snapshot": packed(encoded, MAX_ORIGINAL),
            "snapshot_record": fingerprint(row["snapshot_record"]), "world": world, "baseline": baseline}
        prepared[side] = value
    shared_cases, total_events = {}, 0
    for index, (row, (side, scenario)) in enumerate(zip(report["cases"], callbacks.CASES)):
        require(type(row["index"]) is int and row["index"] == index and
                (row["side"], row["scenario"]) == (side, scenario) and
                row["comparisons"] == {} and row["control_equal"] is True and
                not any(name in row for name in ("c_process", "c_record", "unavailable")),
                "ordered complete original-only callback case")
        directory = manifest.parent / f"case-{index}-{side}-{scenario}"
        common.no_link_components(directory)
        records = {}
        for kind, name, limit in (("snapshot_record", "snapshots.json", MAX_ORIGINAL),
                ("trace_record", "trace.json", MAX_ORIGINAL), ("control_record", "control.json", MAX_ORIGINAL),
                ("replay_input", "input.mcc", MAX_REPLAY), ("replay_record", "replay.json", MAX_REPLAY),
                ("projection_record", "expected.json", MAX_REPLAY)):
            common.no_link_components(directory / name)
            records[kind] = checked_bytes(directory / name, row[kind], limit)
        require(records["snapshot_record"] == records["control_record"], "entire untouched control differs")
        value, trace = common.strict_json(records["snapshot_record"]), common.strict_json(records["trace_record"])
        for name in ("controller", "control"):
            successful(row[name])
        arguments = [str(jars[side]), side, scenario, callbacks.SEED,
                     str(directory / "world"), str(directory / "snapshots.json")]
        require(row["controller"]["command"] == [java, "--add-modules", "jdk.jdi", "-Xmx512m",
            "-cp", classes, "CacheCallbacksController", *arguments, str(directory / "trace.json")],
            "exact attributed original controller command")
        callbacks.validate_command(trace["child_command"], arguments, {"java": java}, Path(classes))
        callbacks.check_child_streams(common, trace, directory)
        controls = [str(jars[side]), side, scenario, callbacks.SEED,
                    str(directory / "control-world"), str(directory / "control.json")]
        require(row["control"]["command"] == [java, "-Xint", "-Xmx512m", "-cp", classes,
            "CacheCallbacksObservation", *controls], "exact untouched original command")
        expected, ids = callbacks.validate_observation(value, side, scenario, jars[side], acquire, finish)
        replay_model, expected = protocol.validate_trace(value, trace, expected, ids, acquire, finish)
        if scenario == "load":
            callbacks.validate_saved_payload(prepared[side], value)
        replay = callbacks.encode_replay(replay_model)
        require(replay == records["replay_input"] and
                callbacks.same_json(common.strict_json(records["replay_record"]), replay_model) and
                callbacks.same_json(common.strict_json(records["projection_record"]), expected),
                "retained replay/projection differs from revalidated original data")
        operations, scripts = len(expected["operations"]), len(replay_model["scripts"])
        events = sum(len(operation["events"]) for operation in trace["operations"])
        require((operations, scripts, events) == ((2, 1, 42) if scenario == "load" else (4, 7, 155)) and
                all(type(row[name]) is int for name in ("operations", "callbacks", "stage_events")) and
                (row["operations"], row["callbacks"], row["stage_events"]) == (operations, scripts, events),
                "complete actual operation/callback/stage counts")
        public = {"index": index, "side": side, "scenario": scenario, "seed_hex": callbacks.SEED,
            "schedule": [list(entry) for entry in callbacks.operation_plan(scenario)],
            "operations": operations, "callbacks": scripts, "stage_events": events,
            "marker_inventories": 2 * operations, "control_equal": True,
            "jar_sha256": finish.JAR_HASHES[side], "bindings": value["bindings"],
            "input": packed(replay, MAX_REPLAY), "expected": packed(records["projection_record"], MAX_REPLAY),
            "protocol": public_protocol(value, trace, side)}
        for name, data in records.items():
            public[name] = {"bytes": len(data), "sha256": digest(data)}
        for name, path in (("world_after", directory / "world"),
                           ("control_world_after", directory / "control-world")):
            public[name] = retained_inventory(common, row[name], path)
        if scenario == "load":
            for before, after in (("world_before", "world_after"),
                                  ("control_world_before", "control_world_after")):
                callbacks.preserved_reload_files(row[before], row[after])
                public[before] = shared.public_inventory(row[before])
        shared_cases.setdefault(scenario, {})[side] = expected
        total_events += events
        result["cases"].append(public)
    require(all(set(sides) == {"client", "server"} and
                callbacks.same_json(sides["client"], sides["server"])
                for sides in shared_cases.values()) and set(shared_cases) == {"load", "population"},
            "both complete shared client/server profiles")
    origin.update(cases=4, operations=12, external_callbacks=16, successful_loads=2,
                  opaque_generations=6, opaque_populations=2, stage_events=total_events,
                  marker_inventories=24, complete_untouched_controls=4,
                  binding_representation="fingerprints-only")
    require(total_events == 394, "all actual lifecycle events retained")
    require(len(json.dumps(result, separators=(",", ":"), allow_nan=False).encode("ascii")) <= MAX_FIXTURE,
            "bounded public callback fixture")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=callbacks.ROOT / "tests/reference/cache/callback_fixtures.json")
    args = parser.parse_args(argv)
    try:
        result = export(args.manifest.absolute())
        encoded = (json.dumps(result, indent=2, allow_nan=False) + "\n").encode("ascii")
        require(len(encoded) <= MAX_FIXTURE, "formatted public fixture size bound")
        require(not args.output.is_symlink(), "new ordinary public output")
        with args.output.open("xb") as output:
            output.write(encoded)
    except Exception as error:
        print("FAILURE: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
