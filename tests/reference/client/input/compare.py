"""Compare the native input owner with unchanged original CPU-only methods."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid
import zipfile

sys.dont_write_bytecode = True
from classfile import inspect_class

HERE = Path(__file__).resolve().parent


def sha(data):
    return hashlib.sha256(data).hexdigest()


def execute(command, directory, label, env):
    result = subprocess.run(command, cwd=directory, env=env, capture_output=True,
                            timeout=60, check=False)
    (directory / (label + ".stdout")).write_bytes(result.stdout)
    (directory / (label + ".stderr")).write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}: " +
                           result.stderr.decode("utf-8", errors="replace")[-3000:])
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    provenance = json.loads((HERE / "provenance.json").read_text())
    jar, native = args.jar.resolve(), args.native.resolve()
    if sha(jar.read_bytes()) != provenance["client_jar_sha256"]:
        raise ValueError("reference JAR SHA-256 differs")
    with zipfile.ZipFile(jar) as archive:
        original_names = {name[:-6].replace("/", ".") for name in archive.namelist()
                          if name.endswith(".class")}
        for name, expected in provenance["classes"].items():
            if inspect_class(archive.read(name + ".class")) != expected:
                raise ValueError("original class/method Code pin differs: " + name)
    raw_inputs = (HERE / "inputs.json").read_bytes()
    if sha(raw_inputs) != provenance["inputs_sha256"]:
        raise ValueError("authored input hash differs")
    inputs = json.loads(raw_inputs)
    commands = [command for scenario in inputs["scenarios"] for command in scenario["commands"]]
    if not commands or len(commands) > 10000:
        raise ValueError("authored command count")
    output = args.work_dir.resolve() / ("run-" + uuid.uuid4().hex)
    output.mkdir(parents=True)
    classes = output / "probe-classes"
    classes.mkdir()
    plan = output / "inputs.plan"
    plan.write_bytes(("\n".join(commands) + "\n").encode("ascii"))
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS"):
        env.pop(key, None)
    execute([args.javac, "-classpath", str(jar), "-d", str(classes),
             str(HERE / "MovementInputProbe.java")], output, "javac", env)
    original = execute([args.java, "-Xverify:all",
                        "-Xlog:class+init=info:file=original-class-init.log",
                        "-classpath", str(classes) + os.pathsep + str(jar),
                        "MovementInputProbe", str(plan)], output, "original", env)
    initialized = set()
    for line in (output / "original-class-init.log").read_text().splitlines():
        match = re.search(r"Initializing '([^']+)'", line)
        if match and match.group(1).replace("/", ".") in original_names:
            initialized.add(match.group(1).replace("/", "."))
    if initialized != {"uo", "lr", "kv", "qb"}:
        raise ValueError("unexpected original class initialization: " + repr(initialized))
    actual = execute([str(native), str(plan)], output, "native", env)
    # Newline spelling is outside the scalar/state observation projection.
    original_lines, actual_lines = original.splitlines(), actual.splitlines()
    if len(original_lines) != len(commands) or len(actual_lines) != len(commands):
        raise ValueError("comparison record count")
    row = re.compile(rb"[0-9]+ (OK|NPE) (base|options) [0-9a-f]{8} [0-9a-f]{8}"
                     rb" [01] [01] [01] [01] (0|10) [0-9]+")
    for index, (expected, observed) in enumerate(zip(original_lines, actual_lines)):
        if not row.fullmatch(expected) or not row.fullmatch(observed) or expected != observed:
            raise AssertionError(f"row {index}: {expected!r} != {observed!r}")
    report = {"result": "exact match", "scenarios": len(inputs["scenarios"]),
              "records": len(commands), "projected_fields_per_record": 10,
              "projected_fields": len(commands) * 10,
              "client_jar_sha256": provenance["client_jar_sha256"],
              "inputs_sha256": sha(raw_inputs), "plan_sha256": sha(plan.read_bytes()),
              "native_executable_sha256": sha(native.read_bytes()),
              "original_initialized_classes": sorted(initialized),
              "original_game_classes_modified": False, "player_constructed": False,
              "reference_helpers": {name:sha((HERE / name).read_bytes()) for name in
                                    ("classfile.py", "compare.py", "inputs.json",
                                     "MovementInputProbe.java", "provenance.json")},
              "original_stdout_sha256":sha(original), "native_stdout_sha256":sha(actual)}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"result":report["result"], "scenarios":report["scenarios"],
                      "records":report["records"], "projected_fields":report["projected_fields"],
                      "output":str(output)}))


if __name__ == "__main__":
    main()
