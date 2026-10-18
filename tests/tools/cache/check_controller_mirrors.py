#!/usr/bin/env python3
"""Check controller mirror identity retention without original game assets."""
from __future__ import annotations

import sys
sys.dont_write_bytecode = True

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import time
import uuid

ROOT = Path(__file__).resolve().parents[3]
HARNESS = ROOT / "tests/reference/cache/ControllerMirrorIdentityRegression.java"
EXPORTER = ROOT / "scripts/reference/cache/export_lifecycle.py"


def need(value, message):
    if not value:
        raise ValueError(message)


def read(path, maximum=2 * 1024 * 1024):
    need(path.is_file() and not path.is_symlink() and path.stat().st_size <= maximum,
         "bounded ordinary input: " + str(path))
    value = path.read_bytes()
    need(len(value) <= maximum, "bounded input changed: " + str(path))
    return value


def fingerprint(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    args = parser.parse_args(argv)
    java_command, javac_command = shutil.which(args.java), shutil.which(args.javac)
    need(java_command and javac_command, "JDK java/javac must resolve through PATH or explicit executable paths")
    java, javac = Path(java_command).resolve(), Path(javac_command).resolve()
    controller = ROOT / "tests/reference/cache/ChunkLifecycleController.java"
    observation = ROOT / "tests/reference/cache/ChunkLifecycleObservation.java"
    plan = {"scope": "asset-independent actual Session serialization/identity regression",
            "controller": str(controller), "harness": str(HARNESS),
            "java": str(java), "javac": str(javac), "scenarios": ["marker", "delayed"],
            "process_controller": str(EXPORTER), "per_JVM_seconds_limit": 120,
            "debugger_heap": "96MiB", "target_heap": "64MiB",
            "target_roots": 128, "discarded_controls": 128,
            "target_GC_or_disableCollection": False,
            "required": "both scenarios preserve every recorded ID and pass disposal/capacity/duplicate controls"}

    need(java.parent == javac.parent, "same native JDK runtime/compiler")
    need(all(not os.environ.get(name) for name in
             ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")),
         "clean Java option/classpath environment")
    inputs = {str(path): fingerprint(read(path)) for path in
              (Path(__file__).resolve(), HARNESS, EXPORTER, controller, observation)}
    # Reuse the existing process group/job owner without calling its producer.
    spec = importlib.util.spec_from_file_location("_mirror_process_controller", EXPORTER)
    owned = importlib.util.module_from_spec(spec)
    exporter_raw = read(EXPORTER)
    need(fingerprint(exporter_raw) == inputs[str(EXPORTER)], "executed process controller matches measured source")
    exec(compile(exporter_raw, str(EXPORTER), "exec"), owned.__dict__)
    need(owned.NATIVE_BYTES == read(owned.NATIVE_SOURCE), "executed process support matches measured source")
    inputs[str(owned.NATIVE_SOURCE)] = fingerprint(read(owned.NATIVE_SOURCE))
    tools = (java, javac, java.parent.parent / "lib/modules", Path(sys.executable).resolve())
    tools_before = {str(path): owned.file_fingerprint(path) for path in tools}
    work = args.work_dir.resolve() / ("capture-" + uuid.uuid4().hex)
    need(all(not path.is_relative_to(work) for path in map(Path, inputs)), "work cannot contain measured source inputs")
    work.mkdir(parents=True, exist_ok=False)
    sources, classes = work / "source", work / "classes"
    sources.mkdir(); classes.mkdir()
    for original, name in ((controller, "ChunkLifecycleController.java"),
                           (observation, "ChunkLifecycleObservation.java"),
                           (HARNESS, HARNESS.name)):
        (sources / name).write_bytes(read(original))
        need(fingerprint(read(sources / name)) == inputs[str(original)], "exact isolated helper source")
    report = {"schema": 1, "scope": plan["scope"], "status": "fail", "plan": plan,
              "processes": [], "errors": [], "inputs_before": inputs, "scenarios": {},
              "tools_before": tools_before,
              "tool_scope": "java/javac binaries, JDK runtime modules containing JDI, Python executable; excludes OS libraries and Python library closure"}
    started = time.monotonic()
    try:
        owned.run("java-version", [java, "-Xmx64m", "-version"], work, report, 30)
        owned.run("compile", [javac, "-J-Xmx512m", "-proc:none", "-implicit:none", "-g",
                  "-Xlint:all", "-Werror", "--release", "17", "--add-modules", "jdk.jdi",
                  "-encoding", "UTF-8", "-classpath", classes, "-sourcepath", sources,
                  "-d", classes, sources / "ChunkLifecycleObservation.java",
                  sources / "ChunkLifecycleController.java", sources / HARNESS.name], work, report, 60)
        for mode in ("marker", "delayed"):
            output = work / mode
            output.mkdir()
            command = [java, "--add-modules", "jdk.jdi", "-Xmx96m", "-Xint",
                       "-XX:SoftRefLRUPolicyMSPerMB=0", "-cp", classes,
                       "ControllerMirrorIdentityRegression", mode, classes, output]
            try:
                owned.run(mode, command, work, report, 120)
            except owned.NATIVE.Failure:
                # Keep a proved stale-identity failure distinct from process or
                # inconclusive failures. Every such result still fails this CLI.
                row = report["processes"][-1]
                need(row["returncode"] == 10 and row["stopped"] is None and row["reaped"] is True
                     and not row.get("cleanup_errors") and
                     not row["process_tree"].get("remaining_group_terminated", False) and
                     not row["process_tree"].get("remaining_processes_terminated", 0),
                     "unexpected JVM failure or incomplete process cleanup")
            value = json.loads(read(output / "report.json"))
            report["scenarios"][mode] = value
            need(value.get("mode") == mode and value.get("target_reaped") is True
                 and value.get("target_exit") == 0 and not value.get("cleanup_errors"),
                 "complete synthetic target teardown")
            evidence = value.get("evidence", {})
            need(evidence.get("control_mirrors_cleared") == 128 and
                 evidence.get("control_disposals_completed") == 128 and
                 evidence.get("control_ids_changed") == 128 and
                 evidence.get("actual_identity_markers") == 128 and
                 evidence.get("duplicate_identity_controls_rejected") == 2,
                 "positive actual-JDI disposal and marker controls")
            if value.get("status") != "pass" or value.get("exit_code") != 0 or evidence.get("stable") is not True:
                report["errors"].append(mode + ": stale serialized identity after proved mirror disposal")
            else:
                need(evidence.get("anchor_ids_changed") == 0 and evidence.get("anchor_mirrors_cleared") == 0
                     and evidence.get("anchor_disposals_completed") == 0 and evidence.get("capacity_contract") is True,
                     "all first-observation and marker IDs retained under exercised disposal pressure")
        if not report["errors"]:
            report["status"] = "pass"
    except Exception as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        report["inputs_after"] = {path: fingerprint(read(Path(path))) for path in inputs}
        report["tools_after"] = {str(path): owned.file_fingerprint(path) for path in tools}
        if report["tools_after"] != tools_before:
            report["errors"].append("measured runtime/compiler/JDI/Python tool bytes changed")
        if report["inputs_after"] != inputs:
            report["errors"].append("measured helper/process support changed")
        if report["errors"]:
            report["status"] = "fail"
        report["seconds"] = round(time.monotonic() - started, 6)
        report["JVM_executed"] = True
        write_json(work / "report.json", report)
    print(json.dumps({"status": report["status"], "report": str(work / "report.json"), "errors": report["errors"]}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
