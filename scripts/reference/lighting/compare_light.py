#!/usr/bin/env python3
"""Compare actual original light regions and admitted queue storage with C."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import sys
import types
import uuid


ROOT = Path(__file__).resolve().parents[3]
MAX_STREAM = 32 * 1024 * 1024
MAX_REGIONS = 32768
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS",
                    "JDK_JAVAC_OPTIONS", "CLASSPATH")
JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def helpers():
    """Load existing bounded-process tooling from attributed source bytes."""
    path = ROOT / "scripts/reference/chunk/compare_nibbles.py"
    data = path.read_bytes()
    require(len(data) <= 1024 * 1024, "process helper source bound")
    module = types.ModuleType("_light_nibbles")
    module.__file__ = str(path)
    sys.modules[module.__name__] = module
    try:
        exec(compile(data, str(path), "exec"), module.__dict__)
        common, _, executed = module.load_modules(ROOT)
    finally:
        sys.modules.pop(module.__name__, None)
    executed.append({"path": str(path), "bytes": len(data),
                     "sha256": hashlib.sha256(data).hexdigest()})
    return common, module, executed


def bounds(value, width=6):
    require(isinstance(value, list) and len(value) == width, "region/bounds width")
    require(all(type(item) is int and -(1 << 31) <= item < (1 << 31) for item in value),
            "region/bounds integer domain")
    if width == 7:
        require(value[0] in (0, 1), "original light kind")
    return value


def regions(world):
    entries = world["queue"]["entries"]
    require(isinstance(entries, list) and len(entries) <= MAX_REGIONS, "queue region budget")
    result = []
    for ordinal, entry in enumerate(entries):
        require(entry["ordinal"] == ordinal and entry["kind"]["identity"] in ("a", "b"),
                "queue ordinal/kind")
        kind = 0 if entry["kind"]["identity"] == "a" else 1
        require(entry["kind"]["name"] == ("Sky" if kind == 0 else "Block") and
                entry["kind"]["ordinal"] == kind and entry["kind"]["default"] == (15 if kind == 0 else 0),
                "original light enum values")
        result.append([kind] + bounds(entry["bounds"]))
    return result


def validate_record(value, side):
    require(value["schema"] == 1 and value["side"] == side, "observation schema/side")
    merges = value["merges"]
    require(isinstance(merges, list) and 40 <= len(merges) <= 256, "merge corpus budget")
    for case in merges:
        bounds(case["initial"], 7)
        require(1 <= len(case["requests"]) <= 16 and len(case["requests"]) == len(case["steps"]),
                "merge request/step cardinality")
        for request, step in zip(case["requests"], case["steps"]):
            bounds(request)
            bounds(step["region"], 7)
            require(type(step["accepted"]) is bool and step["region"][0] == case["initial"][0],
                    "merge result/kind preservation")
    queue = value["queue"]
    before, after = queue["before_world"], queue["after_world"]
    regions(before)
    final = regions(after)
    # Admission reads must not change cache, chunk data, counters or identities.
    require({key: item for key, item in before.items() if key != "queue"} ==
            {key: item for key, item in after.items() if key != "queue"},
            "queue observation changed nonqueue world state")
    require(before["queue"]["object"] == after["queue"]["object"], "queue identity changed")
    require(1 <= len(queue["requests"]) <= 128 and len(queue["requests"]) == len(queue["steps"]),
            "world request/step cardinality")
    for request, step in zip(queue["requests"], queue["steps"]):
        bounds(request["region"], 7)
        require(type(request["admitted"]) is bool and type(request["coalesce"]) is bool,
                "queue request flags")
        require(isinstance(step, list) and len(step) <= MAX_REGIONS, "queue step budget")
        for entry in step:
            bounds(entry, 7)
    require(queue["steps"][-1] == final, "final queue snapshot mismatch")
    require(queue["eligibility_preserved"] is True and len(queue["setup_requests"]) == 2,
            "original admission preservation controls")
    for old, new in zip(before["queue"]["entries"], after["queue"]["entries"]):
        require({key: item for key, item in old.items() if key != "bounds"} ==
                {key: item for key, item in new.items() if key != "bounds"},
                "baseline region identity/order/kind changed")
    require(len(final) >= len(regions(before)), "queue unexpectedly shrank")
    limit = value["limit"]
    require(type(limit["initial_count"]) is int and limit["initial_count"] == len(final) and
            type(limit["appended_to_limit"]) is int and
            limit["initial_count"] + limit["appended_to_limit"] == 1000000 and
            limit["at_limit_count"] == 1000000 and limit["count_after_overflow"] == 0 and
            limit["queue_identity_retained"] is True and limit["enqueue_depth_restored"] is True,
            "original million-entry limit controls")
    require(bounds(limit["first_region"], 7) == final[0], "original limit prefix preservation")
    bounds(limit["last_region"], 7)
    return value


def replay_input(value):
    commands, expected = [], []
    for case in value["merges"]:
        current = case["initial"][1:]
        for request, step in zip(case["requests"], case["steps"]):
            commands.append("M " + " ".join(map(str, current + request)))
            expected.append(["M", int(step["accepted"])] + step["region"][1:])
            current = step["region"][1:]
    queue = value["queue"]
    baseline = regions(queue["before_world"])
    commands.append("Q " + " ".join(map(str, [len(baseline)] + [v for r in baseline for v in r])))
    expected.append(["Q", len(baseline)])
    previous = baseline
    for request, step in zip(queue["requests"], queue["steps"]):
        if request["admitted"]:
            commands.append("E " + " ".join(map(str, [int(request["coalesce"])] + request["region"])))
            expected.append(["E", len(step)] + [v for r in step for v in r])
        else:
            require(step == previous, "rejected world request mutated queue")
        previous = step
    return "\n".join(commands) + "\n", expected


def compare_output(data, expected):
    require(len(data) <= MAX_STREAM, "C output bound")
    lines = data.decode("ascii", errors="strict").splitlines()
    require(len(lines) == len(expected), "C record cardinality")
    for index, (line, wanted) in enumerate(zip(lines, expected)):
        tokens = line.split()
        require(tokens and tokens[0] == wanted[0], f"C operation at record {index}")
        actual = [tokens[0]] + [int(token) for token in tokens[1:]]
        require(actual == wanted, f"C state mismatch at record {index}")
    return len(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--capture-only", action="store_true")
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    report, output, inputs, common = None, None, {}, None
    status = 1
    try:
        common, helper, executed = helpers()
        require(args.capture_only or args.executable is not None, "C executable is required")
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit),
                "tested commit must be a full lowercase commit ID")
        require(not any(os.environ.get(name) for name in JAVA_ENVIRONMENT), "injected Java environment")
        tools = common.runtime_tools(args)
        try:
            helper.release_version(tools["jdk_release"])
        except helper.Unavailable as error:
            raise common.Unavailable(str(error)) from error
        jars = {side: getattr(args, side).resolve() for side in JAR_HASHES}
        for side, path in jars.items():
            if not path.is_file():
                raise common.Unavailable("missing supplied " + side + " JAR")
            require(common.file_record(path, byte_limit=2 * 1024 * 1024)["sha256"] == JAR_HASHES[side],
                    "supplied " + side + " JAR hash mismatch")
        sources = [ROOT / "tests/reference/lighting/LightObservation.java",
                   ROOT / "tests/reference/chunk/ChunkObservation.java"]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT / "CMakeLists.txt"] + sources
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [ROOT / "tests/world/lighting/queue_probe.c"]
        attributed += [Path(item["path"]) for item in executed]
        attributed += list(jars.values()) + list(tools.values())
        if not args.capture_only:
            attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024)
                  for path in attributed}
        for executed_source in executed:
            require(inputs[str(Path(executed_source["path"]).resolve())]["sha256"] == executed_source["sha256"],
                    "executed helper source changed before attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve(strict=True)
        report = {"schema": 1, "status": "running", "tested_commit": args.tested_commit or None,
                  "mode": "original-only" if args.capture_only else "three-way",
                  "commit_record": "Caller label; source and executable fingerprints recorded separately.",
                  "scope": "Original region merges and admitted ordered queue storage; no full world enqueue or finishing claim.",
                  "started_utc": common.utc_now(), "python": sys.version, "platform": platform.platform(),
                  "artifact_root": str(output), "inputs_before": inputs, "executed_source_modules": executed,
                  "limits": {"heap_mib": 512, "child_seconds": 90, "stream_bytes": MAX_STREAM,
                             "process_policy": "Bounded direct child, not an OS process-tree quota.",
                             "filesystem_policy": "Observed inventory, not a write quota."},
                  "sides": {}, "errors": [], "comparisons": {}}
        common.write_json(output / "manifest.json", report)
        for directory in ("source", "classes", "empty", "compile-work", "client-work", "server-work", "c-work"):
            (output / directory).mkdir()
        for path in sources:
            (output / "source" / path.name).write_bytes(path.read_bytes())
        common.tool_versions(tools, output, report)
        command = [str(tools["javac"]), "-J-Xmx512m", "--release", "11", "-proc:none", "-implicit:none",
                   "-encoding", "UTF-8", "-classpath", str(output / "empty"),
                   "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"),
                   "-d", str(output / "classes")] + [str(output / "source" / path.name) for path in sources]
        compiled = common.run_live(command, output / "compile-out.txt", output / "compile-err.txt",
                                   output / "compile-work", seconds=30,
                                   stdout_limit=MAX_STREAM, stderr_limit=MAX_STREAM)
        report["compile"] = asdict(compiled)
        require(compiled.returncode == 0 and not compiled.stop_reason, "observer compilation failed")
        class_files = sorted((output / "classes").glob("*.class"))
        require(2 <= len(class_files) <= 16 and
                all(path.stem == "ChunkObservation" or path.stem.startswith("ChunkObservation$") or
                    path.stem == "LightObservation" or path.stem.startswith("LightObservation$") for path in class_files),
                "helper class inventory")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                                    for path in class_files}
        records, unavailable = {}, []
        for side, jar in jars.items():
            world = output / (side + "-world")
            command = [str(tools["java"]), "-Xmx512m", "-XX:+DisableAttachMechanism", "-cp",
                       str(output / "classes"), "LightObservation", str(jar), side, str(world)]
            process = common.run_live(command, output / (side + "-record.json"), output / (side + "-err.txt"),
                                      output / (side + "-work"), seconds=90,
                                      stdout_limit=MAX_STREAM, stderr_limit=MAX_STREAM, world_root=world)
            report["sides"][side] = {"process": asdict(process)}
            require(not process.stop_reason, side + " process stopped: " + str(process.stop_reason))
            if process.returncode == 77:
                require(b"UNAVAILABLE: genuine original world setup" in (output / (side + "-err.txt")).read_bytes(),
                        "unclassified original unavailability")
                unavailable.append(side)
                continue
            require(process.returncode == 0, side + " original method failed")
            value = common.strict_json((output / (side + "-record.json")).read_bytes())
            records[side] = validate_record(value, side)
            report["sides"][side]["record"] = common.file_record(output / (side + "-record.json"), byte_limit=MAX_STREAM)
            report["sides"][side]["world_inventory"] = common.world_inventory(world, hashes=True)
            require(report["sides"][side]["world_inventory"]["complete"], side + " world inventory incomplete")
            request, expected = replay_input(value)
            if not args.capture_only:
                stdin = output / (side + "-c-input.txt")
                stdin.write_text(request, encoding="ascii")
                c_process = common.run_live([str(args.executable.resolve())], output / (side + "-c-out.txt"),
                                            output / (side + "-c-err.txt"), output / "c-work", seconds=30,
                                            stdout_limit=MAX_STREAM, stderr_limit=MAX_STREAM, stdin_path=stdin)
                report["sides"][side]["c_process"] = asdict(c_process)
                require(c_process.returncode == 0 and not c_process.stop_reason, side + " C probe failed")
                count = compare_output((output / (side + "-c-out.txt")).read_bytes(), expected)
                report["comparisons"]["C/" + side] = {"status": "pass", "records": count}
        if len(records) == 2:
            require(records["client"]["merges"] == records["server"]["merges"], "client/server region mismatch")
            report["comparisons"]["client/server regions"] = "pass"
            left, right = (records[side]["queue"] for side in ("client", "server"))
            same_context = (regions(left["before_world"]) == regions(right["before_world"]) and
                            left["requests"] == right["requests"])
            if same_context:
                require(left["steps"] == right["steps"], "client/server queue mismatch for identical context")
            report["comparisons"]["client/server queues"] = {
                "same_initial_queue_and_requests": same_context,
                "result": "pass" if same_context else "distinct observed contexts; compared independently with C"}
        report["status"] = "unavailable" if unavailable else "pass"
        status = 77 if unavailable else 0
    except Exception as error:
        is_unavailable = common is not None and isinstance(error, common.Unavailable)
        status = 77 if is_unavailable else 1
        if report is not None:
            report["status"] = "unavailable" if is_unavailable else "fail"
            report["errors"].append(str(error))
        print(("UNAVAILABLE: " if is_unavailable else "FAILURE: ") + str(error), file=sys.stderr)
    finally:
        if report is not None:
            report["inputs_after"], changed = {}, []
            for name, before in inputs.items():
                try:
                    after = common.file_record(Path(name), byte_limit=before["bytes"])
                    report["inputs_after"][name] = after
                    require(before == after, "input changed: " + name)
                except Exception as error:
                    changed.append(str(error))
            if "classes_before" in report:
                after = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                         for path in sorted((output / "classes").glob("*.class"))}
                report["classes_after"] = after
                if after != report["classes_before"]:
                    changed.append("helper class inventory changed")
            if changed:
                report["errors"].extend(changed)
                report["status"], status = "fail", 1
            report["finished_utc"] = common.utc_now()
            common.write_json(output / "manifest.json", report)
            print(report["status"].upper() + ": " + str(output / "manifest.json"))
    return status


if __name__ == "__main__":
    sys.exit(main())
