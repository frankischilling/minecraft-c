#!/usr/bin/env python3
"""Compare full normal finishing with untouched controls and original JDI traces."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import os
from pathlib import Path
import platform
import re
import struct
import sys
import types
import uuid


ROOT = Path(__file__).resolve().parents[3]
MAX_BYTES = 32 * 1024 * 1024
STAGES = ("before", "after", "repeat_before", "repeat_after")
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def helpers():
    path = ROOT / "scripts/reference/chunk/compare_chunk.py"
    data = path.read_bytes()
    require(len(data) <= 2 * 1024 * 1024, "helper source bound")
    module = types.ModuleType("_finish_chunk")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    common, executed = module.shared_helper(ROOT)
    executed.append({"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return common, module, executed


def cases():
    initial = bytearray(32768)
    repeated = bytearray(32768)
    for x in range(16):
        for z in range(16):
            base = (x << 11) | (z << 7)
            initial[base:base + 1 + (x * 7 + z * 11) % 48] = bytes([1]) * (1 + (x * 7 + z * 11) % 48)
            height = (x * 13 + z * 3) % 64
            repeated[base:base + height] = bytes([1]) * height
    coordinates = ((0, 0), (1, 0), (-1, 0), (32768, 0), (2147483647, -2147483648), (2000000, 0))
    return [{"coordinates": list(pair), "initial": bytes(initial), "repeat": bytes(repeated)} for pair in coordinates]


def i32(value):
    return (value + (1 << 31)) % (1 << 32) - (1 << 31)


def cache_key(x, z):
    return i32((0x80000000 if x < 0 else 0) | ((x & 32767) << 16) |
               (32768 if z < 0 else 0) | (z & 32767))


def queue_regions(world):
    return [[item["kind"]["ordinal"]] + item["bounds"] for item in world["queue"]["entries"]]


def world_height(world, x, z):
    if x < -32000000 or z < -32000000 or x >= 32000000 or z > 32000000:
        return 0
    entries = {entry["key"]: entry["value"] for entry in world["map"]["entries"]}
    tag = entries.get(cache_key(x >> 4, z >> 4))
    if tag is None:
        return 0
    chunks = {item["object"]["ref"]: item for item in world["chunks"]}
    chunk = chunks[tag]
    if chunk["empty"]:
        return 0
    return bytes.fromhex(chunk["heightmap"]["values"])[(z & 15) * 16 + (x & 15)]


def validate_original(value, trace, case, side, old):
    require(value["schema"] == 2 and value["side"] == side and value["seed_hex"] == "0000000000000017",
            "original schema/side/seed")
    require(value["coordinates"] == case["coordinates"] and value["jar_sha256"] == JAR_HASHES[side] and
            value["platform_parent"] is True and len(value["class_origins"]) == 16 and
            len(set(value["class_origins"].values())) == 1, "original input/class provenance")
    require(value["input_sha256"] == hashlib.sha256(case["initial"]).hexdigest() and
            value["repeat_input_sha256"] == hashlib.sha256(case["repeat"]).hexdigest(), "original input hashes")
    require(all(item is True for item in value["controls"].values()), "original preservation controls")
    expected_bindings = {"world": "fd", "chunk": "lm", "finish": "c"} if side == "client" else {
        "world": "dj", "chunk": "hi", "finish": "b"}
    require(value["bindings"] == expected_bindings, "original method bindings")
    old.validate_world(value["setup_world"], 32768, "setup")
    baseline = value["before"]
    old_cache = old.cache_projection(baseline["world"])
    require(old_cache == old.cache_projection(value["setup_world"]), "constructor changed cache/world context")
    for name in STAGES:
        stage = value[name]
        old.validate_world(stage["world"], 32768, name)
        old.validate_data(stage["local"]["data"], name)
        require(old.cache_projection(stage["world"]) == old_cache, name + ": nonqueue world changed")
        require(stage["opacity"] == baseline["opacity"] and stage["opacity_identity_retained"] is True,
                "opacity changed")
        opacity = old.blob(stage["opacity"], 1024, name)
        require(hashlib.sha256(opacity).hexdigest() == old.OPACITY_SHA, "original opacity hash")
        local = stage["local"]
        old.validate_flags(local["flags"], name)
        require(local["coordinates"] == case["coordinates"], "original chunk coordinates changed")
        require({key: item for key, item in local.items() if key not in ("data", "flags")} ==
                {key: item for key, item in baseline["local"].items() if key not in ("data", "flags")},
                name + ": original ownership/local containers changed")
        identities = local["identities"]
        require(len(identities["roles_retained"]) == 19 and all(identities["roles_retained"]) and
                all(identities[key] is True for key in ("roles_pairwise_distinct", "input_retained", "world_retained")),
                "original identity control failure")
        require({key: item for key, item in local["flags"].items() if key != "o"} ==
                {key: item for key, item in baseline["local"]["flags"].items() if key != "o"},
                "unrelated flags changed")
        for array in ("metadata", "block_light"):
            require(local["data"][array] == baseline["local"]["data"][array], "unrelated local array changed")
        expected_blocks = case["initial"] if name in ("before", "after") else case["repeat"]
        require(old.blob(local["data"]["blocks"], 32768, name) == expected_blocks, "borrowed block bytes changed")
    require(value["after"]["local"]["flags"]["o"] is True and value["repeat_after"]["local"]["flags"]["o"] is True,
            "final dirty effect missing")
    require(value["after"]["world"] == value["repeat_before"]["world"], "raw block edit changed world")
    require(trace["schema"] == 2 and trace["side"] == side and trace["coordinates"] == case["coordinates"] and
            trace["jar_sha256"] == JAR_HASHES[side] and trace["listen_address"] == "127.0.0.1" and
            trace["can_get_method_return_values"] is True and trace["can_get_bytecodes"] is True and
            trace["child_exit"] == 0, "trace provenance/capability/exit")
    require(len(trace["passes"]) == 2, "two complete traced passes")
    if side == "server":
        setup = trace["setup_trace"]
        require(setup["spawn_entries"] == 1 and setup["random_walk_iterations"] == 0,
                "fixed server setup did not follow immediate origin route")
    for number, stage_name in enumerate(("after", "repeat_after")):
        measured = trace["passes"][number]
        require(measured["pass"] == number and measured["direct_call_count"] == 1024 and
                measured["ignored_entries"] == 0 and len(measured["records"]) == 1024 and
                measured["begin_marker"] is True and measured["end_marker"] is True,
                "complete ordered trace required")
        for marker in ("begin", "end"):
            inventory = measured[marker + "_threads"]
            require(inventory["marker"] == marker and inventory["pass"] == number and
                    inventory["suspend_policy"] == 2 and
                    all(inventory[key] is True for key in ("measured_main_thread", "all_threads_suspended",
                        "all_other_threads_daemon", "no_other_original_frames")) and
                    inventory["measured_thread_id"] == measured["identity"]["thread_id"] and
                    inventory["original_loader_id"] == measured["identity"]["world_loader_id"] and
                    1 <= inventory["thread_count"] == len(inventory["threads"]) <= inventory["thread_limit"] <= 128,
                    "finite suspended marker thread inventory")
            for thread in inventory["threads"]:
                require(thread["suspended"] is True and
                        thread["frame_count"] == len(thread["frames"]) <= inventory["frames_per_thread_limit"] <= 64 and
                        (thread["measured"] is True and thread["daemon"] is False or
                         thread["measured"] is False and thread["daemon"] is True and thread["original_frame"] is False),
                        "marker thread role/frame bounds")
        heights = bytes.fromhex(value[stage_name]["local"]["data"]["heightmap"]["values"])
        cx, cz = case["coordinates"]
        for ordinal, item in enumerate(measured["records"]):
            x, z, direction = ordinal // 64, ordinal // 4 % 16, ordinal % 4
            qx = i32(i32(cx * 16) + x + (-1, 1, 0, 0)[direction])
            qz = i32(i32(cz * 16) + z + (0, 0, -1, 1)[direction])
            require(item == {"ordinal": ordinal, "x": qx, "z": qz,
                             "local_height": heights[z * 16 + x],
                             "result": world_height(baseline["world"], qx, qz)},
                    "traced order/coordinates/local height/actual cache result mismatch")
    return value


def replay_input(value, case):
    before = value["before"]
    world, local = before["world"], before["local"]
    chunks = world["chunks"]
    require(len(chunks) <= 512 and len(queue_regions(world)) <= 32768 - 1024, "C replay storage bound")
    ids = {chunk["object"]["ref"]: index for index, chunk in enumerate(chunks)}
    require(len(ids) == len(chunks), "unique cached identities")
    mappings, insertions = world["map"]["entries"], world["insertion_list"]["entries"]
    require(all(entry["value"] is not None for entry in mappings + insertions), "nonnull observed cache required")
    data = bytearray(b"MCF1")
    data += struct.pack(">ii?iIIII?", *case["coordinates"], world["no_sky"], world["enqueue_depth"],
                        len(chunks), len(mappings), len(insertions), len(queue_regions(world)), local["flags"]["o"])
    data += case["initial"] + case["repeat"]
    for name in ("metadata", "skylight", "block_light", "heightmap"):
        data += bytes.fromhex(local["data"][name]["values"])
    data += struct.pack(">i", local["data"]["minimum_height"])
    data += bytes.fromhex(before["opacity"]["values"])
    for chunk in chunks:
        data += struct.pack(">ii???", *chunk["coordinates"], chunk["empty"], chunk["flags"]["o"], chunk["measured"])
        data += bytes.fromhex(chunk["heightmap"]["values"])
    for entry in mappings:
        data += struct.pack(">iI", entry["key"], ids[entry["value"]])
    for entry in insertions:
        data += struct.pack(">I", ids[entry["value"]])
    for region in queue_regions(world):
        data += struct.pack(">Iiiiiii", *region)
    return data


def c_projection(value, trace):
    result = {"schema": 1, "context_retained": True}
    for name in STAGES:
        stage = value[name]
        queries = []
        if name in ("after", "repeat_after"):
            measured = trace["passes"][0 if name == "after" else 1]
            queries = [[item[key] for key in ("x", "z", "local_height", "result")] for item in measured["records"]]
        result[name] = {"data": stage["local"]["data"], "dirty": stage["local"]["flags"]["o"],
                        "depth": stage["world"]["enqueue_depth"], "queue": queue_regions(stage["world"]), "trace": queries}
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--capture-only", action="store_true")
    parser.add_argument("--tested-commit", default="")
    parser.add_argument("--case", type=int, action="append", help="Selected fixed corpus indices, 0 through 5")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    report, common, output, inputs = None, None, None, {}
    status = 1
    try:
        common, old, executed = helpers()
        require(args.capture_only or args.executable is not None, "C executable required")
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "full lowercase source ID")
        require(not any(os.environ.get(name) for name in JAVA_ENVIRONMENT), "injected Java environment")
        tools = common.runtime_tools(args)
        release = tools["jdk_release"].read_text(encoding="utf-8")
        version = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if version is None or int(version.group(1)) < 17:
            raise common.Unavailable("height tracing requires JDK17 or newer with jdk.jdi")
        if not args.capture_only and not args.executable.is_file():
            raise common.Unavailable("missing C finishing executable")
        jars = {side: getattr(args, side).resolve() for side in JAR_HASHES}
        for side, jar in jars.items():
            if not jar.is_file():
                raise common.Unavailable("missing supplied " + side + " JAR")
            require(common.file_record(jar, byte_limit=2 * 1024 * 1024)["sha256"] == JAR_HASHES[side], "original JAR hash")
        selected = args.case if args.case is not None else list(range(6))
        require(selected and len(set(selected)) == len(selected) and all(0 <= n < 6 for n in selected), "fixed case selection")
        sources = [ROOT / "tests/reference/chunk" / name for name in
                   ("ChunkObservation.java", "HeightTraceObservation.java", "HeightTraceController.java")]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT / "CMakeLists.txt"] + sources
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [ROOT / "tests/world/chunk/finish_probe.c"] + [Path(item["path"]) for item in executed]
        attributed += list(jars.values()) + list(tools.values())
        if not args.capture_only:
            attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024) for path in attributed}
        for item in executed:
            require(inputs[str(Path(item["path"]).resolve())]["sha256"] == item["sha256"], "executed-source attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve()
        report = {"schema": 1, "status": "running", "tested_commit": args.tested_commit or None,
                  "mode": "original-only" if args.capture_only else "three-way", "selected_cases": selected,
                  "source_label": "Caller label; actual source, helper and executable fingerprints retained.",
                  "started_utc": common.utc_now(), "artifact_root": str(output), "python": sys.version,
                  "platform": platform.platform(), "inputs_before": inputs, "executed_source_modules": executed,
                  "cases": [], "errors": [], "limits": {"controller_seconds": 120, "debuggee_seconds": 90,
                  "heap_mib": 512, "stream_bytes": MAX_BYTES, "trace_per_pass": 1024,
                  "policy": "Bounded children and observed inventories, not an OS process-tree or filesystem quota."}}
        common.write_json(output / "manifest.json", report)
        for name in ("source", "classes", "empty", "compile-work"):
            (output / name).mkdir()
        for source in sources:
            (output / "source" / source.name).write_bytes(source.read_bytes())
        common.tool_versions(tools, output, report)

        def run(name, command, directory, seconds=120, world=None, unavailable=False):
            process = common.run_live(command, directory / (name + "-out.txt"), directory / (name + "-err.txt"),
                                      directory, seconds=seconds, stdout_limit=MAX_BYTES, stderr_limit=MAX_BYTES, world_root=world)
            if unavailable and not process.stop_reason and process.returncode == 77:
                report["unavailable_process"] = {"name": name, **asdict(process)}
                raise common.Unavailable(name + ": genuine original setup unavailable; see retained process logs")
            require(not process.stop_reason and process.returncode == 0,
                    name + " failed: " + str(asdict(process)))
            return asdict(process)

        report["jdk_module_process"] = run("jdk-modules", [str(tools["java"]), "--list-modules"], output, 15)
        modules = {line.split("@", 1)[0] for line in (output / "jdk-modules-out.txt").read_text(encoding="utf-8").splitlines()}
        if not {"jdk.jdi", "jdk.jdwp.agent"}.issubset(modules):
            raise common.Unavailable("JDK must contain jdk.jdi and jdk.jdwp.agent")
        report["compile"] = run("compile", [str(tools["javac"]), "-J-Xmx512m", "--release", "17", "--add-modules", "jdk.jdi",
            "-proc:none", "-implicit:none", "-encoding", "UTF-8", "-classpath", str(output / "empty"),
            "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"),
            "-d", str(output / "classes")] + [str(output / "source" / path.name) for path in sources], output / "compile-work", 30)
        classes = sorted((output / "classes").glob("*.class"))
        names = ("ChunkObservation", "HeightTraceObservation", "HeightTraceController")
        require(3 <= len(classes) <= 16 and all(any(path.stem == n or path.stem.startswith(n + "$") for n in names) for path in classes),
                "compiled helper inventory")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024) for path in classes}
        corpus = cases()
        for number in selected:
            case = corpus[number]
            directory = output / f"case-{number}"
            directory.mkdir()
            initial, repeated = directory / "initial.bin", directory / "repeat.bin"
            initial.write_bytes(case["initial"])
            repeated.write_bytes(case["repeat"])
            row = {"index": number, "coordinates": case["coordinates"], "sides": {}, "comparisons": {}}
            report["cases"].append(row)
            observed = {}
            for side, jar in jars.items():
                snapshots, traced = directory / (side + "-snapshots.json"), directory / (side + "-trace.json")
                arguments = [str(jar), side, str(directory / (side + "-world")), str(snapshots),
                             str(initial), str(repeated)] + list(map(str, case["coordinates"]))
                command = [str(tools["java"]), "-Xmx512m", "--add-modules", "jdk.jdi", "-cp", str(output / "classes"),
                           "HeightTraceController"] + arguments + [str(traced)]
                side_row = {"trace_process": run(side + "-trace", command, directory, world=Path(arguments[2]), unavailable=True)}
                row["sides"][side] = side_row
                require(snapshots.stat().st_size <= MAX_BYTES and traced.stat().st_size <= MAX_BYTES, "complete record bound")
                value, trace = common.strict_json(snapshots.read_bytes()), common.strict_json(traced.read_bytes())
                validate_original(value, trace, case, side, old)
                side_row["snapshot_record"] = common.file_record(snapshots, byte_limit=MAX_BYTES)
                side_row["trace_record"] = common.file_record(traced, byte_limit=MAX_BYTES)
                control = directory / (side + "-control.json")
                control_args = arguments.copy()
                control_args[2:4] = [str(directory / (side + "-control-world")), str(control)]
                side_row["control_process"] = run(side + "-control", [str(tools["java"]), "-Xmx512m", "-cp", str(output / "classes"),
                                                                      "HeightTraceObservation"] + control_args, directory,
                                                               world=Path(control_args[2]), unavailable=True)
                require(control.stat().st_size <= MAX_BYTES and control.read_bytes() == snapshots.read_bytes(),
                        side + ": untouched control differs from traced complete snapshots")
                side_row["control_equal"] = True
                for kind, root in (("traced_world", arguments[2]), ("control_world", control_args[2])):
                    inventory = common.world_inventory(Path(root), hashes=True)
                    require(inventory["complete"], "complete world inventory required")
                    side_row[kind] = inventory
                expected = c_projection(value, trace)
                observed[side] = expected
                if not args.capture_only:
                    replay = directory / (side + "-input.bin")
                    replay.write_bytes(replay_input(value, case))
                    side_row["c_process"] = run(side + "-c", [str(args.executable.resolve()), str(replay)], directory, 30)
                    actual = common.strict_json((directory / (side + "-c-out.txt")).read_bytes())
                    difference = common.first_difference(expected, actual)
                    require(difference is None, f"C/{side} case {number}: {difference}")
                    row["comparisons"]["C/" + side] = "pass"
            require(observed["client"] == observed["server"], "client/server complete finishing results differ")
            row["comparisons"]["client/server"] = "pass"
            common.write_json(output / "manifest.json", report)
        report["status"], status = "pass", 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable)
        status = 77 if unavailable else 1
        if report is not None:
            report["status"] = "unavailable" if unavailable else "fail"
            report["errors"].append(str(error))
        print(("UNAVAILABLE: " if unavailable else "FAILURE: ") + str(error), file=sys.stderr)
    finally:
        if report is not None:
            report["inputs_after"] = {}
            for name, before in inputs.items():
                try:
                    after = common.file_record(Path(name), byte_limit=before["bytes"])
                    report["inputs_after"][name] = after
                    require(after == before, "input changed: " + name)
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            if "classes_before" in report:
                try:
                    after = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                             for path in sorted((output / "classes").glob("*.class"))}
                    report["classes_after"] = after
                    require(after == report["classes_before"], "compiled helpers changed")
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            report["finished_utc"] = common.utc_now()
            common.write_json(output / "manifest.json", report)
            print(report["status"].upper() + ": " + str(output / "manifest.json"))
    return status


if __name__ == "__main__":
    sys.exit(main())
