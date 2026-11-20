#!/usr/bin/env python3
"""Reproduce unchanged-original client/server crafting CPU comparisons."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import urllib.parse
import urllib.request

from classfile import validate_jar, parse_class
from support import (require, fingerprint, fresh_directory, run_bounded,
                     validate_plan, extract_resources, check_initialization)

REFERENCE = Path(__file__).resolve().parent


def executable(value):
    candidate = Path(value)
    resolved = candidate.resolve() if candidate.is_file() else Path(shutil.which(value) or "")
    require(resolved.is_file(), "missing executable: " + value)
    return resolved.resolve()


def source_root(explicit):
    candidates = [explicit] if explicit else list(REFERENCE.parents)
    for candidate in candidates:
        if candidate and (candidate / "src/item/recipes/matching.c").is_file() and \
                (candidate / "tests/inventory/crafting/trace_crafting.c").is_file():
            return candidate.resolve()
    raise ValueError("crafting native source root not found")


def runtime_binding(path, jar, classes):
    require(path.is_file() and path.stat().st_size <= 65536, "runtime class binding extent")
    lines = path.read_text(encoding="utf-8").splitlines()
    require(lines[:1] == ["CRAFTING_BINDING_V1"] and len(lines) == len(classes) + 1,
            "runtime class binding shape")
    for row, cls in zip(lines[1:], classes):
        parts = row.split("\t")
        require(len(parts) == 4 and parts[:3] == [cls["name"], str(cls["class_bytes"]),
                                                  cls["class_sha256"]], "runtime class bytes")
        location = urllib.parse.urlparse(parts[3])
        require(location.scheme == "file" and not location.netloc, "runtime class origin scheme")
        origin = Path(urllib.request.url2pathname(location.path)).resolve()
        require(origin == jar, "runtime class origin is not the validated JAR")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--native", required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--javac", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    for variable in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        require(not os.environ.get(variable), "uncontrolled JVM option environment: " + variable)
    provenance = json.loads((REFERENCE / "provenance.json").read_text(encoding="utf-8"))
    require(provenance["schema"] == "CRAFTING_CPU_COMPARISON_V1", "provenance schema")
    for name, pin in provenance["reference_files"].items():
        require(fingerprint(REFERENCE / name) == pin, "reference source/input pin: " + name)
    root = source_root(args.source_root)
    for name, pin in provenance["native_sources"].items():
        require(fingerprint(root / name) == pin, "native source pin: " + name)
    java, javac, native = (executable(value) for value in (args.java, args.javac, args.native))
    run = fresh_directory(args.work_dir)
    tools = {}
    for name, tool in (("java", java), ("javac", javac)):
        result = run_bounded([tool, "-version"], run, name + "-version", 15)
        text = (result.stdout + result.stderr).decode("utf-8")
        require(result.returncode == 0 and re.search(r'(?:version "|javac )17\.', text),
                "measured JDK17 profile required: " + name)
        tools[name] = {"path": str(tool), **fingerprint(tool), "version": text}
    classes = run / "helper-classes"
    classes.mkdir()
    command = [javac, "--release", "8", "-g:none", "-encoding", "UTF-8", "-d", classes,
               REFERENCE / "CraftingOriginalProbe.java"]
    compiled = run_bounded(command, run, "compile-helper", 30)
    require(compiled.returncode == 0 and not compiled.stdout and not compiled.stderr,
            "helper compiler exit/output")
    class_files = list(classes.rglob("*"))
    require(class_files == [classes / "CraftingOriginalProbe.class"], "helper-only classpath")
    helper_class = class_files[0]
    require(parse_class(helper_class.read_bytes()) == provenance["helper_class"],
            "compiled helper class/Code pin")
    pins = json.loads((REFERENCE / "class-code-pins.json").read_text(encoding="utf-8"))
    results = {}
    for side, argument in (("client", args.client_jar), ("server", args.server_jar)):
        jar = argument.resolve(strict=True)
        binding = validate_jar(jar, side, pins)
        plan = REFERENCE / (side + "-cases.tsv")
        case_count = validate_plan(plan.read_bytes())
        require(case_count == provenance["cases_per_side"], "authored case count")
        work = run / side
        work.mkdir()
        resources = work / "private-resources"
        extract_resources(jar, resources, provenance["private_resources"][side])
        selected = pins["jars"][side]["classes"]
        (work / "classes.txt").write_text("\n".join(c["name"] for c in selected) + "\n",
                                           encoding="ascii")
        command = [java, "-Xmx256m", "-Djava.awt.headless=true",
                   "-Dcrafting.probe.classes=classes.txt", "-Dcrafting.probe.binding=binding.tsv",
                   "-Xlog:class+init=info:file=class-init.log:uptime,level,tags", "-cp",
                   os.pathsep.join((str(classes), str(jar))), "CraftingOriginalProbe", side, plan]
        original = run_bounded(command, work, "original", 45)
        require(original.returncode == 0 and not original.stderr, side + " original exit/stderr")
        runtime_binding(work / "binding.tsv", jar, selected)
        initialization = check_initialization(work / "class-init.log", side)
        actual = run_bounded([native, side, resources, plan], work, "native", 120)
        require(actual.returncode == 0 and not actual.stderr, side + " native exit/stderr")
        expected_rows, native_rows = original.stdout.splitlines(), actual.stdout.splitlines()
        require(expected_rows[:3] == [b"151 recipes", b"16 achievements",
                                       b"CRAFTING_TRACE_V1\t" + side.encode()] and
                expected_rows[-1:] == [b"END"], "actual original console/protocol banners")
        require(len(expected_rows) == provenance["projected_rows_per_side"] and
                sum(row.startswith(b"C\t") for row in expected_rows) == case_count,
                "original projection extent")
        require(expected_rows == native_rows, side + " exact original/native projection differs")
        require(validate_jar(jar, side, pins) == binding, "original JAR changed during execution")
        results[side] = {"jar": str(jar), "binding": binding, "case_count": case_count,
                         "rows": len(expected_rows), "class_initialization": initialization,
                         "original_stdout": fingerprint(work / "original.stdout"),
                         "native_stdout": fingerprint(work / "native.stdout"), "stderr_bytes": 0}
    report = {"status": "passed", "run_directory": str(run), "tools": tools,
              "native": {"path": str(native), **fingerprint(native)}, "results": results,
              "source_root": str(root), "native_sources": provenance["native_sources"],
              "scope": provenance["scope"], "normalization": "splitlines only"}
    (run / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
