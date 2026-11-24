"""Reproduce unchanged original Player construction and compare concrete native owners."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys

from classfile import parse_class, validate_jar
from comparison import compare
from projection import validate
from support import (check_initialization, extract_resources, fingerprint, fresh_directory,
                     require, run_bounded, validate_tape, verify_binding)

HERE = Path(__file__).resolve().parent
JAVA_OPTIONS = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS")
HELPERS = ("ConstructorJson", "PlayerConstructionTarget", "PlayerConstructionJdi")
TARGET_LINES = ([b"16 achievements", b"151 recipes"] +
                [f"CASE {i} {'success' if i < 6 else 'NullPointerException'}".encode()
                 for i in range(8)])


def tool(value):
    resolved = shutil.which(value)
    require(resolved is not None, "missing executable: " + value)
    return Path(resolved).resolve()


def bounded_json(path, lines=False):
    require(path.is_file() and path.stat().st_size <= 2097152, "observation file extent")
    text = path.read_text(encoding="ascii")
    return [json.loads(line) for line in text.splitlines()] if lines else json.loads(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--native", required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--javac", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    require(sys.flags.optimize == 0, "Python assertions must remain enabled")
    require(not any(os.environ.get(name) for name in JAVA_OPTIONS),
            "ambient Java options must be unset")
    env = os.environ.copy()
    for name in (*JAVA_OPTIONS, "CLASSPATH"):
        env.pop(name, None)
    provenance = bounded_json(HERE / "provenance.json")
    require(provenance["schema"] == "PLAYER_CONSTRUCTION_COMPARISON_V1", "provenance schema")
    for name, expected in provenance["reference_files"].items():
        require(fingerprint(HERE / name) == expected, "reference source pin: " + name)
    source_root = args.source_root.resolve() if args.source_root else next(
        (p for p in HERE.parents if (p / "src/entity/player/construction.c").is_file()), None)
    require(source_root is not None, "native source root; use --source-root")
    for name, expected in provenance["native_sources"].items():
        require(fingerprint(source_root / name) == expected, "native source pin: " + name)
    java, javac, native = tool(args.java), tool(args.javac), tool(args.native)
    require(java.parent == javac.parent, "java/javac must belong to the same explicit JDK17")
    run = fresh_directory(args.work_dir)
    report = {"run_directory": str(run), "native": {"path": str(native), **fingerprint(native)},
              "tools": {}, "source_root": str(source_root)}
    for name, executable in (("java", java), ("javac", javac)):
        result = run_bounded([executable, "-version"], run, name + "-version", env=env)
        version = (result.stdout + result.stderr).decode("utf-8")
        require(result.returncode == 0 and re.search(r'\b(?:version\s+")?17\.', version),
                name + " JDK17 version")
        report["tools"][name] = {"path": str(executable), **fingerprint(executable),
                                  "version": version}
    jar = args.client_jar.resolve()
    pins = bounded_json(HERE / "source-pins.json")
    report["jar"] = validate_jar(jar, "client", pins)
    classes = pins["jars"]["client"]["classes"]
    # Compile only authored helpers into an empty directory. No source game
    # class, resource, patch or replacement class can precede the original JAR.
    compiled = run / "helpers"
    compiled.mkdir()
    result = run_bounded([javac, "-source", "17", "-target", "17", "-g:none",
                          "-encoding", "UTF-8", "--add-modules", "jdk.jdi", "-classpath", jar,
                          "-d", compiled, *(HERE / (name + ".java") for name in HELPERS)],
                         run, "compile", env=env)
    require(result.returncode == 0 and not result.stdout and not result.stderr,
            "authored helper compilation")
    require({p.name for p in compiled.iterdir()} == {name + ".class" for name in HELPERS},
            "helper-only compiled directory")
    for name in HELPERS:
        require(parse_class((compiled / (name + ".class")).read_bytes()) ==
                provenance["helper_classes"][name], "authored helper class/Code pin: " + name)
    class_names = "".join(c["name"].replace("/", ".") + "\n" for c in classes).encode("ascii")
    normal = run / "normal"
    normal.mkdir()
    (normal / "classes.txt").write_bytes(class_names)
    normal_command = [java, "-Xmx256m", "-Xverify:all",
                      "--add-opens=java.base/java.lang=ALL-UNNAMED",
                      "--add-opens=java.base/java.util=ALL-UNNAMED",
                      "-Dplayer.probe.classes=" + str(normal / "classes.txt"),
                      "-Dplayer.probe.binding=" + str(normal / "binding.tsv"),
                      "-Xlog:class+init=info:file=original-class-init.log", "-classpath",
                      str(compiled) + os.pathsep + str(jar), "PlayerConstructionTarget",
                      normal / "observations.ndjson"]
    result = run_bounded(normal_command, normal, "original", env=env)
    require(result.returncode == 0 and not result.stderr and
            result.stdout.splitlines() == TARGET_LINES, "original target exit/diagnostics/banners")
    normal_rows = bounded_json(normal / "observations.ndjson", lines=True)
    require(len(normal_rows) == 9 and normal_rows[0]["event"] == "setup" and
            [row["case"] for row in normal_rows[1:]] == list(range(8)), "normal case transport")
    setup = normal_rows[0]
    require((setup["world_class"], setup["save_handler_class"], setup["dimension_class"]) ==
            ("fd", "wy", "rh"), "genuine concrete World inputs")
    require(len(setup["creative"]) == 28 and all(row["same_static"] for row in setup["creative"]),
            "actual creative-list aliases")
    original_classes = list(provenance["original_initialized_classes"])
    report["normal_initialization"] = check_initialization(
        normal / "original-class-init.log", jar, original_classes)
    report["normal_runtime_bindings"] = verify_binding(normal / "binding.tsv", jar, classes)
    debugger = run / "debugger"
    debugger.mkdir()
    (debugger / "classes.txt").write_bytes(class_names)
    result = run_bounded([java, "-Xmx256m", "--add-modules", "jdk.jdi", "-classpath", compiled,
                          "PlayerConstructionJdi", jar, compiled, debugger, java.parent.parent],
                         debugger, "controller", timeout=180, env=env)
    require(result.returncode == 0 and not result.stdout and not result.stderr,
            "JDI controller exit/diagnostics")
    require((debugger / "target.stdout").read_bytes().splitlines() == TARGET_LINES and
            not (debugger / "target.stderr").read_bytes(), "JDI target diagnostics/banners")
    report["debugger_initialization"] = check_initialization(
        debugger / "target-class-init.log", jar, original_classes)
    report["debugger_runtime_bindings"] = verify_binding(debugger / "binding.tsv", jar, classes)
    bounded_json(debugger / "checkpoints.ndjson", lines=True)
    bounded_json(debugger / "target-observations.ndjson", lines=True)
    projection_pins = {"classes": {c["name"].replace("/", "."): c for c in classes}}
    prefix, report["original_validation"] = validate(
        normal / "observations.ndjson", debugger, projection_pins)
    prefix_path = run / "constructor-prefix-projection.json"
    prefix_path.write_bytes((json.dumps(prefix, indent=2) + "\n").encode("ascii"))
    tape = "".join(bits + "\n" for row in normal_rows[1:] for bits in row["draws"]).encode("ascii")
    validate_tape(tape)
    tape_path = run / "scalar-formula-draws.txt"
    tape_path.write_bytes(tape)
    resources = run / "private-resources"
    extract_resources(jar, resources, provenance["private_resources"])
    native_output = run / "native.ndjson"
    result = run_bounded([native, resources, run / "native-world", tape_path, native_output],
                         run, "native", env=env)
    require(result.returncode == 0 and not result.stderr and
            result.stdout.splitlines() == [b"151 recipes", b"16 achievements"],
            "native exit/diagnostics/banners")
    bounded_json(native_output, lines=True)
    report["comparison"] = compare(normal / "observations.ndjson", native_output, prefix_path)
    report["jar_after"] = validate_jar(jar, "client", pins)
    for name, expected in provenance["native_sources"].items():
        require(fingerprint(source_root / name) == expected, "native source changed during run")
    report["scope"] = provenance["scope"]
    report["status"] = "passed"
    (run / "report.json").write_bytes((json.dumps(report, indent=2) + "\n").encode("ascii"))
    print(json.dumps(report))


if __name__ == "__main__":
    main()
