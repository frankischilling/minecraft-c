#!/usr/bin/env python3
"""Replay complete captured JDK Integer HashSet API traces against the C owner.

With --java and --javac, also run the project-owned helper twice on that JDK.
The fixed fixture remains the oracle; C output never creates expected values.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/java/int_set_fixtures.json"
HELPER = ROOT / "tests/reference/java/HashSetIntegerObservation.java"
TRACE_SHA256 = "b59da560a7a0d230976e43269349a71ebb9efb9bd57cbc6aea7cd1574eefecb5"
HELPER_SHA256 = "cb09229091962557a8c00b24fdc3b05440779a67c257455713ccd9582d29a351"
MAX_OUTPUT = 4 * 1024 * 1024


class Failure(Exception):
    pass


def require(value, message):
    if not value:
        raise Failure(message)


def encoded(value, *, sorted_keys=False):
    return (json.dumps(value, ensure_ascii=True, separators=(",", ":"),
                       sort_keys=sorted_keys) + "\n").encode("ascii")


def strict_json(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON field: " + key)
            result[key] = value
        return result
    def invalid(value):
        raise Failure("nonfinite JSON number: " + value)
    return json.loads(raw, object_pairs_hook=unique, parse_constant=invalid)


def fingerprint(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def file_fingerprint(path):
    digest = hashlib.sha256()
    size = 0
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(block)
            require(size <= 512 * 1024 * 1024, "bounded input fingerprint")
            digest.update(block)
    return {"bytes": size, "sha256": digest.hexdigest()}


def integer(value):
    return type(value) is int and -(2**31) <= value < 2**31


def snapshot(value):
    require(type(value) is dict and set(value) == {"size", "iteration", "fresh_first"},
            "snapshot fields")
    order = value["iteration"]
    require(type(order) is list and len(order) <= 512 and all(map(integer, order)),
            "bounded signed Integer iteration")
    require(type(value["size"]) is int and value["size"] == len(order) == len(set(order)),
            "snapshot unique size")
    require(value["fresh_first"] is None if not order else
            integer(value["fresh_first"]) and value["fresh_first"] == order[0],
            "snapshot fresh iterator first")


def validate_fixture(package):
    require(type(package["schema"]) is int and package["schema"] == 1 and package["scope"] == "jdk-integer-hash-set-public-api"
            and package["tested_commit"] is None, "fixture scope and historical attribution")
    trace = package["trace"]
    raw = encoded(trace)
    require(fingerprint(raw) == {"bytes": 276109, "sha256": TRACE_SHA256},
            "immutable complete JDK trace fingerprint")
    require(encoded(package["provenance"]["canonical_trace"], sorted_keys=True) ==
            encoded(fingerprint(raw), sorted_keys=True),
            "fixture trace provenance")
    require(hashlib.sha256(HELPER.read_bytes()).hexdigest() == HELPER_SHA256,
            "immutable JDK helper fingerprint")
    require(trace["internal_state_read"] is False and trace["original_game_classes_used"] is False,
            "JDK-only public API scope")
    require(len(trace["cases"]) == trace["case_count"] == 7 and trace["operation_count"] == 574,
            "complete seven-case 574-operation fixture")
    total = 0
    for case in trace["cases"]:
        previous = case["initial"]
        snapshot(previous)
        states = [encoded(previous)]
        maximum = previous["size"]
        for index, operation in enumerate(case["operations"]):
            require(operation["index"] == index and encoded(operation["before"]) == encoded(previous),
                    "operation index and complete before state")
            after = operation["after"]
            snapshot(after)
            action, argument = operation["action"], operation["argument"]
            members = set(previous["iteration"])
            if action in ("add", "remove"):
                require(integer(argument) and operation["selected"] is None and
                        operation["empty_before_action"] is None and
                        type(operation["result"]) is bool, "ordinary mutation fields")
                expected = argument not in members if action == "add" else argument in members
                require(operation["result"] is expected, "ordinary mutation return")
                if action == "add":
                    members.add(argument)
                else:
                    members.discard(argument)
            else:
                require(action == "fresh_first_set_remove" and argument is None,
                        "fresh iterator plus Set.remove action")
                empty = not members
                require(operation["empty_before_action"] is empty and
                        operation["selected"] == previous["fresh_first"] and
                        (operation["result"] is None if empty else operation["result"] is True),
                        "guarded fresh iterator removal")
                if not empty:
                    members.remove(operation["selected"])
            require(members == set(after["iteration"]), "mutation membership")
            maximum = max(maximum, after["size"])
            states.append(encoded(after))
            previous = after
            total += 1
        require(encoded(case["final"]) == encoded(previous) and case["maximum_size"] == maximum,
                "complete final state and maximum")
        for checkpoint in case["checkpoints"]:
            snapshot(checkpoint["state"])
            require(encoded(checkpoint["state"]) in states, "checkpoint is an observed complete state")
    require(total == 574, "complete operation count")
    return trace, raw


def replay_requests(trace):
    commands, expected, labels = [], [], []
    for case in trace["cases"]:
        commands.append("new")
        expected.append(case["initial"])
        labels.append(case["name"] + ":initial")
        for operation in case["operations"]:
            action = operation["action"]
            commands.append((action + " " + str(operation["argument"]))
                            if action in ("add", "remove") else "take")
            expected.append({key: operation[key] for key in
                             ("before", "empty_before_action", "selected", "result", "after")})
            labels.append(case["name"] + ":" + str(operation["index"]))
        commands.append("snapshot")
        expected.append(case["final"])
        labels.append(case["name"] + ":final")
    return ("\n".join(commands) + "\n").encode("ascii"), expected, labels


def compare(actual, expected, labels):
    require(len(actual) == len(expected), "C response count")
    for got, wanted, label in zip(actual, expected, labels):
        # Canonical JSON equality also distinguishes Boolean/integer aliases.
        require(encoded(got, sorted_keys=True) == encoded(wanted, sorted_keys=True),
                "C/JDK complete state mismatch at " + label)


def negative_controls(expected, labels):
    changed = []
    candidate = next(i for i, row in enumerate(expected) if "after" in row and row["after"]["size"] > 1)
    for kind in ("order", "first", "size", "result", "before"):
        rows = copy.deepcopy(expected)
        row = rows[candidate]
        if kind == "order":
            row["after"]["iteration"].reverse()
        elif kind == "first":
            row["after"]["fresh_first"] = None
        elif kind == "size":
            row["after"]["size"] = True
        elif kind == "result":
            row["result"] = int(row["result"])
        else:
            row["before"]["iteration"] = []
        try:
            compare(rows, expected, labels)
        except Failure:
            changed.append(kind)
        else:
            raise Failure("negative control accepted: " + kind)
    for raw in (b'{"size":999,"size":0,"iteration":[],"fresh_first":null}',
                b'{"size":NaN,"iteration":[],"fresh_first":null}'):
        try:
            strict_json(raw)
        except Failure:
            changed.append("strict-json")
        else:
            raise Failure("strict JSON rejection control accepted")
    return changed


def run(command, request=b"", *, work, name, records, deadline):
    require(len(request) <= 256 * 1024, "bounded child request")
    start = time.monotonic()
    stdout_path, stderr_path = work / (name + ".stdout"), work / (name + ".stderr")
    record = {"name": name, "command": command, "exit": None, "timed_out": False,
              "output_limited": False, "reaped": False}
    records.append(record)
    with tempfile.TemporaryFile() as incoming, stdout_path.open("w+b") as outgoing, stderr_path.open("w+b") as errors:
        incoming.write(request)
        incoming.seek(0)
        child = subprocess.Popen(command, stdin=incoming, stdout=outgoing, stderr=errors, cwd=work)
        try:
            while child.poll() is None:
                if time.monotonic() >= min(start + 30, deadline):
                    record["timed_out"] = True
                    break
                if os.fstat(outgoing.fileno()).st_size > MAX_OUTPUT or os.fstat(errors.fileno()).st_size > MAX_OUTPUT:
                    record["output_limited"] = True
                    break
                time.sleep(0.01)
        finally:
            if child.poll() is None:
                child.kill()
            child.wait()
            record["reaped"] = True
            record["exit"] = child.returncode
            for label, stream in (("stdout", outgoing), ("stderr", errors)):
                size = os.fstat(stream.fileno()).st_size
                record[label + "_total_bytes"] = size
                record["output_limited"] |= size > MAX_OUTPUT
                stream.truncate(min(size, MAX_OUTPUT))
                stream.seek(0)
                record[label] = fingerprint(stream.read(MAX_OUTPUT))
            outgoing.seek(0)
            errors.seek(0)
            stdout, stderr = outgoing.read(MAX_OUTPUT), errors.read(MAX_OUTPUT)
    require(not record["timed_out"], "child timeout: " + name)
    require(not record["output_limited"], "child output limit: " + name)
    require(child.returncode == 0,
            "child exit " + str(child.returncode) + ": " + stderr[:2048].decode("utf-8", "replace"))
    return stdout, stderr


def capture_root(base, tools):
    base = base.absolute()
    for path in (base, *base.parents):
        require(not path.is_symlink() and not (getattr(path.lstat(), "st_file_attributes", 0) & 0x400)
                if path.exists() else not path.is_symlink(), "ordinary output ancestors")
    base = base.resolve()
    if base.is_relative_to(ROOT):
        relative = base.relative_to(ROOT)
        require(relative.parts and (relative.parts[0] in (".local", "build") or relative.parts[0].startswith("build-")),
                "repository captures require .local or a build directory")
    else:
        require(not ROOT.is_relative_to(base), "output must not contain source root")
    for tool in tools:
        directory = Path(tool).resolve().parent
        require(not base.is_relative_to(directory) and not directory.is_relative_to(base),
                "output overlaps a tool directory")
        if Path(tool).stem in ("java", "javac"):
            require(not base.is_relative_to(directory.parent), "output overlaps JDK")
    base.mkdir(parents=True, exist_ok=True)
    work = base / ("capture-" + uuid.uuid4().hex)
    work.mkdir(exist_ok=False)
    return work


def jdk_inputs(java, javac):
    result = {"java": Path(java).resolve(), "javac": Path(javac).resolve()}
    for role, tool in (("runtime", java), ("compiler", javac)):
        root = Path(tool).resolve().parent.parent
        for name in ("release", "lib/modules", "lib/ct.sym", "bin/server/jvm.dll", "bin/java.dll", "bin/jli.dll", "bin/zip.dll",
                     "lib/server/libjvm.so", "lib/libjava.so", "lib/libjli.so", "lib/libzip.so", "lib/libverify.so"):
            path = root / name
            if path.is_file():
                result[role + "/" + name] = path
        require((root / "release").is_file() and (root / "lib/modules").is_file(), "modular JDK input closure")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--pending", action="store_true", help="Replay through mediated chunk-pending mutations")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=ROOT / ".local/reference/java-int-set")
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "JDK public Integer HashSet API only", "tested_commit": None,
              "passed": False, "processes": [], "inputs": {},
              "provenance_limit": "Archived project sources and selected modular JDK files; no transitive OS-library or historical-runtime reconstruction."}
    deadline = time.monotonic() + 140
    try:
        require(args.executable.is_file(), "C probe executable is missing")
        require(bool(args.java) == bool(args.javac), "supply both --java and --javac")
        java, javac = None, None
        if args.java:
            java, javac = shutil.which(args.java), shutil.which(args.javac)
            if not java or not javac:
                print("UNAVAILABLE: JDK tools are missing", file=sys.stderr)
                return 77
            for key in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH"):
                require(not os.environ.get(key), "untracked JDK option environment: " + key)
        work = capture_root(args.work_dir, [sys.executable] + ([java, javac] if java else []))
        sources = work / "sources"
        sources.mkdir()
        inputs = {"project/" + name: ROOT / name for name in (
            "CMakeLists.txt", ".clang-format", "src/core/java/int_set.c", "include/mc/core/java/int_set.h",
            "src/core/java/java.c", "include/mc/core/java/java.h", "src/core/math/math.c", "include/mc/core/math/math.h",
            "src/core/text/mutf8.c", "include/mc/core/text/mutf8.h", "tests/core/java/int_set_probe.c",
            "tests/core/java/test_int_set.c", "tests/reference/java/HashSetIntegerObservation.java",
            "tests/reference/java/int_set_fixtures.json", "scripts/reference/java/compare_int_set.py")}
        for directory in ("include/mc/core", "include/mc/world", "src/core", "src/world"):
            inputs.update({"project/" + path.relative_to(ROOT).as_posix(): path
                           for path in sorted((ROOT / directory).rglob("*"))
                           if path.is_file() and path.suffix in (".c", ".h")})
        inputs["c_probe"] = args.executable.resolve()
        inputs["python"] = Path(sys.executable).resolve()
        if java:
            inputs.update(jdk_inputs(java, javac))
        for name, path in inputs.items():
            report["inputs"][name] = file_fingerprint(path)
            if name.startswith("project/"):
                target = sources / name[8:]
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
                require(file_fingerprint(target) == report["inputs"][name], "archived input mismatch: " + name)
        require(FIXTURE.stat().st_size <= MAX_OUTPUT, "bounded fixture")
        package = strict_json(FIXTURE.read_bytes())
        trace, canonical = validate_fixture(package)
        request, expected, labels = replay_requests(trace)
        (work / "requests.txt").write_bytes(request)
        command = [str(args.executable.resolve())] + (["--pending"] if args.pending else [])
        stdout, stderr = run(command, request, work=work,
                             name="c", records=report["processes"], deadline=deadline)
        require(not stderr, "C probe stderr must be empty")
        actual = [strict_json(row) for row in stdout.splitlines()]
        compare(actual, expected, labels)
        controls = negative_controls(expected, labels)
        report.update({"c_responses": len(actual), "operations": 574, "c_output": fingerprint(stdout),
                       "negative_controls": controls})
        print(f"PASS: 7 JDK cases, 574 operations, {len(actual)} complete C responses; {len(controls)} rejection controls")
        print("Captured JDK trace SHA-256 " + TRACE_SHA256)
        print("C executable SHA-256 " + report["inputs"]["c_probe"]["sha256"])
        if java:
            classes = work / "classes"
            classes.mkdir()
            empty = work / "empty-lookups"
            empty.mkdir()
            class_inputs = None
            for name, command in (
                ("java-version", [java, "-version"]),
                ("javac-version", [javac, "-version"]),
                ("compile", [javac, "--release", "17", "-encoding", "UTF-8", "-proc:none", "-implicit:none",
                             "-classpath", str(empty), "-sourcepath", str(empty), "-d", str(classes),
                             str(sources / "tests/reference/java" / HELPER.name)]),
                ("observed", [java, "-cp", str(classes), "HashSetIntegerObservation", "observed"]),
                ("control", [java, "-cp", str(classes), "HashSetIntegerObservation", "control"]),
            ):
                out, err = run(command, work=work, name=name, records=report["processes"], deadline=deadline)
                if name == "compile":
                    class_inputs = {path.name: file_fingerprint(path) for path in sorted(classes.iterdir())}
                    require(len(class_inputs) == 2 and all(key.endswith(".class") for key in class_inputs), "two helper classes")
                    report["classes"] = class_inputs
                if name in ("observed", "control"):
                    require(not err and out == canonical, "live JDK complete trace differs from pinned fixture: " + name)
                    require({path.name: file_fingerprint(path) for path in sorted(classes.iterdir())} == class_inputs,
                            "helper classes changed")
            print("PASS: current JDK observed/control traces exactly match captured oracle")
        for name, path in inputs.items():
            require(file_fingerprint(path) == report["inputs"][name], "input changed: " + name)
            if name.startswith("project/"):
                require(file_fingerprint(sources / name[8:]) == report["inputs"][name], "archive changed: " + name)
        report["inputs_unchanged"] = True
        report["passed"] = True
        print("Scope: specified modern JDK public API histories; original game lifecycle and Java6 remain unverified.")
        return 0
    except (Failure, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        report["failure"] = str(error)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print("Capture: " + str(work))


if __name__ == "__main__":
    sys.exit(main())
