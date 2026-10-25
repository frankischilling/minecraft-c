"""Compile and compare current native CPU rendering with the pinned client JAR."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid
import zipfile

sys.dont_write_bytecode = True
from classfile import inspect_class, sha256


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
RELATIVE = Path("tests/reference/client/rendering")
FAMILY_SOURCES = [
    "src/client/world/mesher_faces.c", "src/client/world/mesher_lighting.c",
    "src/block/registry/rendering.c", "src/block/registry/texture.c",
    "src/block/registry/opacity.c", "src/block/registry/runtime.c",
    "src/block/registry/registry.c", "src/block/material/material.c",
    "src/world/chunk/nibbles.c", "src/core/math/math.c", "src/core/java/java.c",
]


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path, value):
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(value, indent=2) + "\n")


def execute(command, cwd, output, label, env):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=60, check=False)
    (output / f"{label}.stdout").write_bytes(result.stdout)
    (output / f"{label}.stderr").write_bytes(result.stderr)
    if result.returncode:
        message = result.stderr.decode("utf-8", errors="replace")[-4000:]
        raise RuntimeError(f"{label} exited {result.returncode}: {message}")
    return result


def verify_archives(client, libraries, provenance):
    actual = sha256(client.read_bytes())
    if actual != provenance["client_jar_sha256"]:
        raise ValueError(f"client JAR SHA-256 differs: {actual}")
    paths = []
    for name, expected in provenance["libraries"].items():
        path = libraries / name
        actual = sha256(path.read_bytes())
        if actual != expected:
            raise ValueError(f"{name} SHA-256 differs: {actual}")
        paths.append(path.resolve())
    with zipfile.ZipFile(client) as archive:
        for name, expected in provenance["classes"].items():
            actual = inspect_class(archive.read(name.replace(".", "/") + ".class"))
            if actual != expected:
                raise ValueError(f"class/descriptor/Code pin differs: {name}")
    return paths


def freeze_sources(destination):
    paths = {Path(path) for path in FAMILY_SOURCES}
    paths.add(Path("src/client/world/mesher.c"))
    for base in (REPO / "src", REPO / "include"):
        for path in base.rglob("*"):
            if path.is_file() and path.suffix in (".h", ".inc"):
                paths.add(path.relative_to(REPO))
    for path in HERE.iterdir():
        if path.is_file():
            paths.add(path.relative_to(REPO))
    hashes = {}
    for path in sorted(paths):
        content = (REPO / path).read_bytes()
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        hashes[path.as_posix()] = sha256(content)
    for path, expected in hashes.items():
        if sha256((REPO / path).read_bytes()) != expected:
            raise RuntimeError(f"source changed while freezing: {path}; rerun")
    return hashes


def records(stdout, kind, original, inputs):
    lines = stdout.decode("utf-8").splitlines()
    if original:
        lines = [line for line in lines if line not in ("151 recipes", "16 achievements")]
    hexword = r"[0-9a-f]{8}"
    pattern = re.compile(r"[0-9]+ [0-9]+ [0-9]+ " +
                         (" ".join([hexword] * 6) if kind == "faces" else hexword))
    expected = []
    if kind in ("faces", "ao"):
        for case in range(3):
            for side in range(6):
                for vertex in range(6):
                    expected.append(f"{case} {side} {vertex} ")
        if len(lines) != len(expected):
            raise ValueError(f"{kind}: {len(lines)} records, expected {len(expected)}")
        for line, prefix in zip(lines, expected):
            if not line.startswith(prefix) or pattern.fullmatch(line) is None:
                raise ValueError(f"{kind}: invalid or unordered record: {line!r}")
    else:
        vertex_pattern = re.compile(r"[0-9]+ [0-9]+ " + " ".join([hexword] * 6))
        index = 0
        for case in inputs["families"]["cases"]:
            number, count = case["case"], case["vertices"]
            if index >= len(lines) or lines[index] != f"case {number} {count}":
                raise ValueError(f"families: missing/different count for case {number}")
            index += 1
            for vertex in range(count):
                if (index >= len(lines) or not lines[index].startswith(f"{number} {vertex} ") or
                        vertex_pattern.fullmatch(lines[index]) is None):
                    raise ValueError(f"families: invalid/missing case {number} vertex {vertex}")
                index += 1
        if index != len(lines):
            raise ValueError("families: trailing records")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=REPO / ".local/reference-client-rendering")
    parser.add_argument("--cc", default="gcc", help="GCC executable with LTO support")
    parser.add_argument("--faces-native", type=Path)
    parser.add_argument("--ao-native", type=Path)
    parser.add_argument("--families-native", type=Path)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    args = parser.parse_args()
    selected = (args.faces_native, args.ao_native, args.families_native)
    if any(path is not None for path in selected) and not all(path is not None for path in selected):
        parser.error("provide all three native executable paths or none")
    prebuilt = all(path is not None for path in selected)
    provenance = read_json(HERE / "provenance.json")
    inputs = read_json(HERE / "inputs.json")
    if sha256((HERE / "inputs.json").read_bytes()) != provenance["inputs_sha256"]:
        raise ValueError("authored inputs hash differs")
    for name, expected in provenance["probe_sha256"].items():
        if sha256((HERE / name).read_bytes()) != expected:
            raise ValueError(f"authored probe hash differs: {name}")
    libraries = verify_archives(args.client_jar, args.library_dir, provenance)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = args.output_dir.resolve() / f"{stamp}-{uuid.uuid4().hex[:8]}"
    output.mkdir(parents=True)
    snapshot = output / "snapshot"
    hashes = freeze_sources(snapshot)
    write_json(output / "frozen-sources.json", hashes)
    env = os.environ.copy()
    removed = [name for name in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS") if name in env]
    for name in removed:
        del env[name]
    report = {"schema": 1, "status": "running", "client_jar_sha256": provenance["client_jar_sha256"],
              "inputs_sha256": provenance["inputs_sha256"], "cleared_java_option_variables": removed,
              "source_sha256": hashes, "commands": [], "comparisons": {},
              "native_mode": "prebuilt" if prebuilt else "frozen-source GCC",
              "native_executables": {},
              "prebuilt_source_join": "caller build provenance; snapshot is not a link proof" if prebuilt else None}
    write_json(output / "report.json", report)
    try:
        versions = [(args.java, "-version", "java-version"),
                    (args.javac, "-version", "javac-version")]
        if not prebuilt:
            versions.insert(0, (args.cc, "--version", "compiler-version"))
        for executable, option, label in versions:
            execute([executable, option], snapshot, output, label, env)
        java_classes = output / "java-classes"
        java_classes.mkdir()
        command = [args.javac, "-encoding", "UTF-8", "-d", str(java_classes)] + [
            str(snapshot / RELATIVE / name) for name in ("FaceProbe.java", "AoProbe.java", "FamilyProbe.java")]
        report["commands"].append(command)
        execute(command, snapshot, output, "java-compile", env)
        classpath = os.pathsep.join(map(str, [java_classes, args.client_jar.resolve(), *libraries]))
        common = [args.cc, "-std=c17", "-O2", "-flto", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                  "-ffp-contract=off", "-fno-fast-math", "-Iinclude", "-Isrc/client/world"]
        profiles = (
            ("faces", "FaceProbe", "faces.c", ["src/client/world/mesher_faces.c"]),
            ("ao", "AoProbe", "ambient_occlusion.c", ["src/client/world/mesher_lighting.c"]),
            ("families", "FamilyProbe", "families.c", FAMILY_SOURCES),
        )
        for profile_index, (kind, main_class, native_file, sources) in enumerate(profiles):
            if prebuilt:
                executable = selected[profile_index].resolve()
                if not executable.is_file():
                    raise ValueError(f"native executable is absent: {executable}")
            else:
                executable = output / (kind + (".exe" if os.name == "nt" else ""))
                command = common + [str(RELATIVE / native_file), *sources, "-lm", "-o", str(executable)]
                report["commands"].append(command)
                execute(command, snapshot, output, kind + ".compile", env)
            executable_hash = sha256(executable.read_bytes())
            report["native_executables"][kind] = {"path": str(executable), "sha256": executable_hash}
            command = [args.java, "-Djava.awt.headless=true", "-cp", classpath, main_class]
            report["commands"].append(command)
            original = execute(command, snapshot, output, kind + ".original", env)
            command = [str(executable)]
            report["commands"].append(command)
            native = execute(command, snapshot, output, kind + ".native", env)
            if sha256(executable.read_bytes()) != executable_hash:
                raise ValueError(f"native executable changed during comparison: {kind}")
            a = records(original.stdout, kind, True, inputs)
            b = records(native.stdout, kind, False, inputs)
            canonical_a = ("\n".join(a) + "\n").encode("ascii")
            canonical_b = ("\n".join(b) + "\n").encode("ascii")
            (output / f"{kind}.original.records").write_bytes(canonical_a)
            (output / f"{kind}.native.records").write_bytes(canonical_b)
            expected = provenance["observations"][kind]
            result = {"equal": a == b, "records": len(a), "original_sha256": sha256(canonical_a),
                      "native_sha256": sha256(canonical_b)}
            report["comparisons"][kind] = result
            if sha256(canonical_a) != expected["records_sha256"]:
                raise ValueError(f"{kind}: original observation differs from pinned record hash")
            if a != b:
                index = next(i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1])
                result["first_difference"] = {"record": index, "original": a[index], "native": b[index]}
                raise ValueError(f"{kind}: vertex record {index} differs")
        verify_archives(args.client_jar, args.library_dir, provenance)
        report["status"] = "passed"
        print("PASS: 108 face records, 108 AO colors, 534 family vertices and 16 counts match exactly")
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        write_json(output / "report.json", report)
        print(f"Artifacts: {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
