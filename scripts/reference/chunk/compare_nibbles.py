#!/usr/bin/env python3
"""Capture original packed nibbles, then compare complete C/client/server observations."""

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
import zipfile


SCRIPT = Path(__file__).absolute()
ROOT = SCRIPT.parents[3]
SOURCE = ROOT / "tests/reference/chunk/NibblesObservation.java"
SOURCE_SHA = "0dfe9f1dfb1449d9a864e3924fbe62321addf1890ae652e8c676715f574ddf2a"
MAX_SOURCE = 2 * 1024 * 1024
MAX_STREAM = 4 * 1024 * 1024
MAX_MANIFEST = 4 * 1024 * 1024
MAX_INVENTORY_BYTES = 32 * 1024 * 1024
MAX_JAR = 2 * 1024 * 1024
MAX_TOOL = 512 * 1024 * 1024
CHILD_SECONDS = 30
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS",
                    "JDK_JAVAC_OPTIONS", "CLASSPATH")
OWNED_DIRECTORIES = ("classes", "source", "empty-classpath", "empty-sourcepath", "empty-modulepath",
                     "compile-work", "client-work", "server-work", "c-work")
OWNED_FILES = {
    "source/NibblesObservation.java", "classes/NibblesObservation.class",
    "compile-stdout.txt", "compile-stderr.txt",
    "client-stdout.jsonl", "client-stderr.txt", "client-record.json",
    "server-stdout.jsonl", "server-stderr.txt", "server-record.json",
    "c-stdout.jsonl", "c-stderr.txt", "c-record.json",
    "reference-vectors.json",
}


class Failure(Exception):
    pass


class Unavailable(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def source_module(path, name):
    """Use the same exact-source loading convention as the existing cave drivers."""
    path = Path(path).resolve(strict=True)
    require(path.stat().st_size <= MAX_SOURCE, "source exceeds execution size bound")
    with path.open("rb") as stream:
        data = stream.read(MAX_SOURCE + 1)
    require(len(data) <= MAX_SOURCE, "source grew beyond execution size bound")
    attributed = {"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    module = types.ModuleType(name)
    module.__file__ = str(path)
    old = sys.modules.get(name)
    sys.modules[name] = module
    try:
        exec(compile(data, str(path), "exec"), module.__dict__)
    finally:
        if old is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = old
    return module, attributed


def load_modules(root):
    directory = root / "scripts/reference/caves"
    fixture, fixture_record = source_module(directory / "fixtures.py", "_nibbles_cave_fixtures")
    old = sys.modules.get("fixtures")
    sys.modules["fixtures"] = fixture
    try:
        common, common_record = source_module(directory / "compare_caves.py", "_nibbles_common")
    finally:
        if old is None:
            sys.modules.pop("fixtures", None)
        else:
            sys.modules["fixtures"] = old
    protocol, protocol_record = source_module(root / "scripts/reference/chunk/nibbles_protocol.py",
                                              "_nibbles_protocol")
    return common, protocol, [fixture_record, common_record, protocol_record]


def java_environment():
    return {name: os.environ.get(name) for name in JAVA_ENVIRONMENT}


def release_version(path):
    with Path(path).open("rb") as stream:
        data = stream.read(65537)
    require(len(data) <= 65536, "JDK release metadata exceeds bound")
    match = re.search(rb'(?m)^JAVA_VERSION="([^"]{1,128})"\r?$', data)
    require(match is not None, "JDK release lacks JAVA_VERSION")
    value = match.group(1).decode("ascii", errors="strict")
    major = re.match(r"([0-9]+)(?:\.([0-9]+))?", value)
    require(major is not None, "JDK release version is not numeric")
    number = int(major.group(2) if major.group(1) == "1" else major.group(1))
    if number < 11:
        raise Unavailable("Java 11 or newer is required; JDK release reports " + value)
    return value


def archive_members(jar):
    """Hash every bounded original ZIP member without extracting or initializing classes."""
    require(Path(jar).is_file() and Path(jar).stat().st_size <= MAX_JAR, "original archive file bound")
    result, total = {}, 0
    with zipfile.ZipFile(jar) as archive:
        members = archive.infolist()
        require(len(members) <= 4096 and len({entry.filename for entry in members}) == len(members),
                "original archive member cardinality/duplicates")
        require("META-INF/INDEX.LIST" not in {entry.filename for entry in members}, "archive index not allowed")
        for entry in sorted(members, key=lambda item: item.filename):
            name = entry.filename
            require(0 < len(name) <= 256 and name.isascii() and not name.startswith("/") and
                    "\\" not in name and ".." not in name.split("/"), "original member name bound")
            require(0 <= entry.file_size <= 1024 * 1024, "original member byte bound")
            require(total + entry.file_size <= MAX_INVENTORY_BYTES, "original expanded archive byte bound")
            with archive.open(entry) as stream:
                data = stream.read(1024 * 1024 + 1)
                require(len(data) == entry.file_size and not stream.read(1), "original member length")
            total += len(data)
            item = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "kind": "directory" if entry.is_dir() else "file"}
            if name.endswith(".class"):
                require(len(data) >= 8 and data[:4] == b"\xca\xfe\xba\xbe", "original classfile magic")
                item["classfile_major"] = int.from_bytes(data[6:8], "big")
            result[name] = item
    require(result, "original member inventory empty")
    return result


def source_paths(root):
    return [root / name for name in (
        "scripts/reference/chunk/compare_nibbles.py", "scripts/reference/chunk/nibbles_protocol.py",
        "scripts/reference/caves/compare_caves.py", "scripts/reference/caves/fixtures.py",
        "tests/reference/chunk/NibblesObservation.java", "tests/world/chunk/nibbles_probe.c",
        "src/world/chunk/data.c", "src/world/chunk/nibbles.c",
        "include/mc/world/chunk/data.h", "include/mc/world/chunk/nibbles.h",
    )]


def preflight(args, common, protocol, executed):
    require(sys.version_info >= (3, 11), "Python 3.11 or newer required")
    require(SCRIPT.resolve() == (ROOT / "scripts/reference/chunk/compare_nibbles.py").resolve(),
            "executing driver must be the attributed canonical source")
    active = [name for name, value in java_environment().items() if value]
    if active:
        raise Unavailable("Java option/classpath environment must be empty: " + ", ".join(active))
    inputs = {}

    def add(path, maximum):
        path = Path(path).resolve()
        if not path.is_file():
            raise Unavailable("missing prerequisite: " + str(path))
        item = common.file_record(path, byte_limit=maximum)
        inputs[item["path"]] = item
        return Path(item["path"])

    for path in source_paths(ROOT):
        add(path, MAX_SOURCE)
    require(inputs[str(SOURCE.resolve())]["sha256"] == SOURCE_SHA, "reviewed original observer source differs")
    for item in executed:
        require(inputs.get(item["path"]) == item, "executed utility/protocol differs from attributed source")
    add(Path(sys.executable), MAX_TOOL)
    jars, members = {}, {}
    for side in protocol.JARS:
        jars[side] = add(getattr(args, side), MAX_JAR)
        require(inputs[str(jars[side])]["sha256"] == protocol.JARS[side][0],
                side + " original JAR SHA-256 differs")
        members[side] = archive_members(jars[side])
        require(protocol.JARS[side][1] + ".class" in members[side], "original owner class absent")
    tools = {name: common.find_tool(getattr(args, name), name) for name in ("java", "javac")}
    homes = {path.parent.parent.resolve() for path in tools.values()}
    require(len(homes) == 1, "java and javac must belong to one JDK")
    home = next(iter(homes))
    tools.update(jdk_release=home / "release", jdk_modules=home / "lib/modules", jdk_ct_sym=home / "lib/ct.sym")
    tools = {name: add(path, MAX_TOOL) for name, path in tools.items()}
    release_version(tools["jdk_release"])
    executable = None if args.capture_only else add(args.executable, MAX_TOOL)
    work = common.no_link_components(args.work_dir)
    common.samefile_or_ancestor(work, [Path(path) for path in inputs])
    require(not work.exists() or work.is_dir(), "work directory is not a directory")
    return inputs, jars, tools, members, executable, work


def classes_inventory(common, directory):
    common.no_link_components(directory)
    require({item.name for item in directory.iterdir()} == {"NibblesObservation.class"},
            "unexpected helper class inventory")
    path = directory / "NibblesObservation.class"
    require(not common.is_link(path.lstat()) and path.is_file() and path.stat().st_nlink == 1,
            "helper class is not fresh regular storage")
    with path.open("rb") as stream:
        require(stream.read(8) == bytes.fromhex("cafebabe00000037"), "helper Java11 class header")
    return {"NibblesObservation.class": common.file_record(path, byte_limit=MAX_SOURCE)}


def empty_inventory(common, path):
    value = common.world_inventory(path, hashes=True, max_entries=128, max_bytes=MAX_INVENTORY_BYTES)
    require(value["exists"] and value["complete"] and not value["error"] and not value["entries"],
            "working/search directory was not empty: " + str(path))
    return value


def owned_inventory(common, output):
    value = common.world_inventory(output, hashes=True, max_entries=128, max_bytes=MAX_INVENTORY_BYTES)
    entry = next((item for item in value["entries"] if item["path"] == "manifest.json"), None)
    if entry is not None:
        value["entries"].remove(entry)
        value["files"] -= 1
        value["bytes"] -= entry["bytes"]
    value["excluded_self_metadata"] = ["manifest.json"]
    for item in value["entries"]:
        allowed = OWNED_FILES if item["kind"] == "file" else OWNED_DIRECTORIES
        if item["path"] not in allowed:
            value["complete"] = False
            value["error"] = "unexpected owned artifact: " + item["path"]
            break
    return value


def after_inputs(common, inputs):
    after, errors = {}, []
    for name, before in inputs.items():
        try:
            current = common.file_record(Path(name), byte_limit=before["bytes"])
            after[name] = current
            require(current == before, "attributed input changed: " + name)
        except Exception as error:
            errors.append("input preservation: " + name + ": " + str(error))
    return after, errors


def read_stream(path):
    require(path.stat().st_size <= MAX_STREAM, "raw stream exceeds bound")
    with path.open("rb") as stream:
        data = stream.read(MAX_STREAM + 1)
    require(len(data) <= MAX_STREAM, "raw stream grew beyond bound")
    return data


def capture(args, common, protocol, executed, inputs, jars, tools, members_before, executable, base):
    common.no_link_components(base)
    common.samefile_or_ancestor(base, [Path(name) for name in inputs])
    base.mkdir(parents=True, exist_ok=True)
    output = base / ("capture-" + uuid.uuid4().hex)
    common.no_link_components(output)
    require(not output.exists(), "fresh capture root already exists")
    output.mkdir()
    require(output.resolve().parent == base.resolve(), "fresh capture root redirected")
    manifest = output / "manifest.json"
    report = {
        "schema": 1, "status": "running", "completed": False,
        "mode": "original-only" if args.capture_only else "three-way",
        "scope": "Normal packed-nibble constructor/get/set observations; no worlds or chunk finishing.",
        "started_utc": common.utc_now(), "artifact_root": str(output), "source_sha256": SOURCE_SHA,
        "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
        "commit_record": "Caller-supplied label; actual source and executable hashes recorded separately.",
        "executed_source_modules": executed, "java_environment_before": java_environment(),
        "python": {"version": sys.version, "executable": sys.executable,
                   "implementation": platform.python_implementation()},
        "jdk_release_version": release_version(tools["jdk_release"]),
        "inputs_before": inputs, "original_members_before": members_before,
        "limits": {"child_seconds": CHILD_SECONDS, "observer_heap_mib": 256,
                   "observer_processes": 2, "stdout_live_bytes": MAX_STREAM,
                   "stderr_live_bytes": MAX_STREAM, "record_bytes": protocol.MAX_RECORD_BYTES,
                   "inventory_entries": 128, "inventory_bytes": MAX_INVENTORY_BYTES,
                   "process_policy": "Bounded direct child; not an OS process-tree quota.",
                   "filesystem_policy": "Observed inventories; not a filesystem write quota."},
        "sides": {}, "errors": [], "comparisons": [],
    }
    classes, sealed, records = None, {}, {}
    unavailable = None

    def save():
        require(len(json.dumps(report, indent=2, ensure_ascii=True, allow_nan=False).encode("utf-8")) + 1
                <= MAX_MANIFEST, "report bound")
        common.write_json(manifest, report)

    def check_inputs():
        require(java_environment() == report["java_environment_before"], "Java option environment changed")
        _, errors = after_inputs(common, inputs)
        require(not errors, "; ".join(errors))
        for name, item in sealed.items():
            require(common.file_record(Path(name), byte_limit=item["bytes"]) == item,
                    "retained artifact changed: " + name)

    def seal():
        check_inputs()
        inventory = owned_inventory(common, output)
        require(inventory["complete"] and not inventory["error"], "owned artifact inventory failed")
        for item in inventory["entries"]:
            if item["kind"] == "file":
                require(item["bytes"] <= MAX_STREAM, "retained artifact size bound")
                value = common.file_record(output / item["path"], byte_limit=MAX_STREAM)
                sealed.setdefault(value["path"], value)

    save()
    try:
        for name in OWNED_DIRECTORIES:
            (output / name).mkdir()
            empty_inventory(common, output / name)
        snapshot = output / "source/NibblesObservation.java"
        with SOURCE.open("rb") as stream:
            source_bytes = stream.read(MAX_SOURCE + 1)
        require(len(source_bytes) <= MAX_SOURCE and hashlib.sha256(source_bytes).hexdigest() == SOURCE_SHA,
                "reviewed source changed before snapshot")
        with snapshot.open("xb") as stream:
            stream.write(source_bytes)
        source_record = common.file_record(snapshot, byte_limit=MAX_SOURCE)
        require(source_record["sha256"] == SOURCE_SHA, "reviewed source snapshot differs")
        sealed[source_record["path"]] = source_record
        check_inputs()
        command = [str(tools["javac"]), "-J-Xmx256m", "--release", "11", "-encoding", "UTF-8",
                   "-Xlint:all", "-Werror", "-proc:none", "-implicit:none", "-g:none",
                   "--class-path", str(output / "empty-classpath"), "--source-path", str(output / "empty-sourcepath"),
                   "--module-path", str(output / "empty-modulepath"), "-d", str(output / "classes"), str(snapshot)]
        process = common.run_live(command, output / "compile-stdout.txt", output / "compile-stderr.txt",
                                  output / "compile-work", seconds=CHILD_SECONDS,
                                  stdout_limit=MAX_STREAM, stderr_limit=MAX_STREAM)
        report["compile_process"] = asdict(process)
        require(process.returncode == 0 and not process.stop_reason and not process.timed_out,
                "strict fresh helper compilation failed")
        classes = classes_inventory(common, output / "classes")
        report["classes_before"] = classes
        for name in OWNED_DIRECTORIES[2:]:
            empty_inventory(common, output / name)
        seal()
        save()

        sides = ("client", "server") if args.capture_only else ("client", "server", "c")
        for side in sides:
            check_inputs()
            empty_inventory(common, output / (side + "-work"))
            stdout, stderr = output / (side + "-stdout.jsonl"), output / (side + "-stderr.txt")
            state = {"status": "running", "records": 0}
            report["sides"][side] = state
            save()
            command = ([str(executable)] if side == "c" else
                       [str(tools["java"]), "-Xmx256m", "-cp", str(output / "classes"),
                        "NibblesObservation", side, str(jars[side])])
            process = common.run_live(command, stdout, stderr, output / (side + "-work"),
                                      seconds=CHILD_SECONDS, stdout_limit=MAX_STREAM, stderr_limit=MAX_STREAM)
            state["process"] = asdict(process)
            # Compare complete observations before interpreting any later process status.
            value = protocol.parse_record(read_stream(stdout), side)
            if value is not None:
                records[side] = value
                saved = output / (side + "-record.json")
                common.write_json(saved, value)
                state["record_artifact"] = common.file_record(saved, byte_limit=protocol.MAX_RECORD_BYTES)
                state["records"] = 1
                if side in jars:
                    member = protocol.JARS[side][1] + ".class"
                    state["observed_original_members"] = {member: members_before[side][member]}
                if side == "server":
                    report["comparisons"].append(protocol.compare_records(records["client"], value))
                elif side == "c":
                    for original in ("client", "server"):
                        report["comparisons"].append(protocol.compare_records(value, records[original]))
            state["working_inventory"] = empty_inventory(common, output / (side + "-work"))
            if side == "c":
                require(process.returncode == 0 and not process.stop_reason and not process.timed_out and
                        value is not None and not read_stream(stderr), "C observation did not cleanly complete")
            else:
                markers = protocol.lifecycle(read_stream(stderr), side)
                state["lifecycle"] = markers
                status = protocol.classify_original(value, markers, process.returncode,
                                                    process.stop_reason, process.timed_out)
                if status == "unavailable":
                    state["status"] = status
                    raise Unavailable(side + " explicit original setup unavailable")
            seal()
            state["status"] = "pass"
            if side == "server":
                vectors = protocol.reference_vectors(records["client"], records["server"])
                vectors["observer_source_sha256"] = SOURCE_SHA
                vectors["original_record_artifacts"] = {
                    original: report["sides"][original]["record_artifact"] for original in jars}
                vector_path = output / "reference-vectors.json"
                require(len(json.dumps(vectors, indent=2, ensure_ascii=True, allow_nan=False).encode("utf-8")) + 1
                        <= MAX_SOURCE, "reference vector report exceeds bound")
                common.write_json(vector_path, vectors)
                report["reference_vectors_artifact"] = common.file_record(vector_path, byte_limit=MAX_SOURCE)
                seal()
            save()
        expected = 2 if args.capture_only else 3
        require(len(records) == expected and len(report["comparisons"]) == (1 if args.capture_only else 3) and
                all(item["status"] == "pass" for item in report["sides"].values()), "incomplete comparison")
        report["completed"] = True
    except Unavailable as error:
        unavailable = str(error)
    except KeyboardInterrupt:
        report["errors"].append("capture interrupted")
    except Exception as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        report["java_environment_after"] = java_environment()
        report["java_environment_preserved"] = (report["java_environment_after"] == report["java_environment_before"] and
                                                 not any(report["java_environment_after"].values()))
        if not report["java_environment_preserved"]:
            report["errors"].append("Java option environment preservation failed")
        after, errors = after_inputs(common, inputs)
        report["inputs_after"], report["inputs_preserved"] = after, not errors
        report["errors"].extend(errors)
        try:
            check_inputs()
            report["sealed_artifacts_preserved"] = True
        except Exception as error:
            report["sealed_artifacts_preserved"] = False
            report["errors"].append("retained artifact preservation: " + str(error))
        report["sealed_artifacts"] = sorted(sealed.values(), key=lambda item: item["path"])
        try:
            for side, jar in jars.items():
                require(common.file_record(jar, byte_limit=MAX_JAR)["sha256"] == protocol.JARS[side][0],
                        "original archive changed before final member inspection")
            after_members = {side: archive_members(jar) for side, jar in jars.items()}
            report["original_members_after"] = after_members
            require(after_members == members_before, "original class members changed")
            report["original_members_preserved"] = True
        except Exception as error:
            report["original_members_preserved"] = False
            report["errors"].append("original member preservation: " + str(error))
        if classes is not None:
            try:
                current = classes_inventory(common, output / "classes")
                report["classes_after"], report["classes_preserved"] = current, current == classes
                require(current == classes, "fresh helper classes changed")
            except Exception as error:
                report["classes_preserved"] = False
                report["errors"].append("helper class preservation: " + str(error))
        report["working_inventories"] = {}
        for name in OWNED_DIRECTORIES[2:]:
            try:
                report["working_inventories"][name] = empty_inventory(common, output / name)
            except Exception as error:
                report["errors"].append("empty directory preservation: " + str(error))
        for item in report["sides"].values():
            if item["status"] == "running":
                item["status"] = "fail"
        report["unavailable_reason"] = unavailable
        if not report["completed"] and unavailable is None and not report["errors"]:
            report["errors"].append("capture incomplete")
        report["status"] = "fail" if report["errors"] else ("unavailable" if unavailable else "pass")
        report["finished_utc"] = common.utc_now()
        inventory = owned_inventory(common, output)
        report["output_inventory"] = inventory
        if not inventory["complete"] or inventory["error"]:
            report["errors"].append("final owned artifact inventory failed")
            report["status"] = "fail"
        if report["status"] != "pass":
            report["completed"] = False
        save()
    print(json.dumps({"status": report["status"], "manifest": str(manifest),
                      "manifest_sha256": common.file_record(manifest, byte_limit=MAX_MANIFEST)["sha256"],
                      "completed": report["completed"], "comparisons": report["comparisons"],
                      "errors": report["errors"], "unavailable_reason": unavailable}))
    return 1 if report["status"] == "fail" else (77 if unavailable else 0)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--client", type=Path, default=ROOT / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=ROOT / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=ROOT / ".local/reference/chunk-nibbles")
    parser.add_argument("--tested-commit")
    parser.add_argument("--capture-only", action="store_true", help="observe both originals without running C")
    parser.add_argument("--preflight", action="store_true", help="validate prerequisites without running subprocesses or writing files")
    args = parser.parse_args(argv)
    if args.tested_commit and not re.fullmatch("[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires 40 hexadecimal characters")
    if not args.capture_only and args.executable is None:
        parser.error("--executable is required unless --capture-only is selected")
    if args.capture_only and args.executable is not None:
        parser.error("--capture-only does not accept a C executable")
    common = None
    try:
        common, protocol, executed = load_modules(ROOT)
        prepared = preflight(args, common, protocol, executed)
        if args.preflight:
            print(json.dumps({"status": "validated", "execution": False,
                              "inputs": common.input_fingerprint(prepared[0])}))
            return 0
        return capture(args, common, protocol, executed, *prepared)
    except KeyboardInterrupt:
        print("FAIL: nibbles preflight interrupted", file=sys.stderr)
        return 1
    except Exception as error:
        unavailable = isinstance(error, Unavailable) or (common is not None and isinstance(error, common.Unavailable))
        print(("UNAVAILABLE: " if unavailable else "FAIL: ") + str(error), file=sys.stderr)
        return 77 if unavailable else 1


if __name__ == "__main__":
    sys.exit(main())
