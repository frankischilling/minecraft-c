"""Reproduce unchanged original GL matrices and compare selected native camera math."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile
from urllib.parse import unquote, urlparse
import zipfile
from support import native_rows, callback_rows, require, run

HERE = Path(__file__).resolve().parent


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def number(bits):
    return struct.unpack(">f", bytes.fromhex(bits.decode("ascii")))[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client-jar", "native", "callbacks", "java", "javac", "libraries", "native-dir", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    require(os.name == "nt", "this actual original LWJGL2 context profile is Windows x64")
    require(args.java.resolve().parent == args.javac.resolve().parent, "matching explicit JDK17")
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        require(not os.environ.get(key), "ambient JVM options: " + key)
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"):
        env.pop(key, None)
    test_tree = HERE.parents[2]
    require(not args.work_dir.resolve().is_relative_to(test_tree), "private outputs outside source tests")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="camera-original-", dir=args.work_dir.resolve()))
    pins = json.loads((HERE / "source-pins.json").read_text())
    proof = json.loads((HERE / "provenance.json").read_text())
    reference_before = {name: fingerprint(HERE / name) for name in proof["reference_files"]}
    require(reference_before == proof["reference_files"], "reference source pins")
    repo = HERE.parents[3]
    native_before = {name: fingerprint(repo / name) for name in proof["native_sources"]}
    require(native_before == proof["native_sources"], "complete selected native source/header pins")
    controller_before = {name: fingerprint(repo / name) for name in proof["controller_files"]}
    require(controller_before == proof["controller_files"], "authored controller pins")
    require(fingerprint(args.client_jar) == pins["client_jar"], "whole original JAR")
    libraries = [args.libraries / name for name in pins["libraries"]]
    for path in libraries:
        require(fingerprint(path) == pins["libraries"][path.name], "original library " + path.name)
    for name, pin in pins["native"].items():
        require(fingerprint(args.native_dir / name) == pin, "original native " + name)
    spec = importlib.util.spec_from_file_location("camera_classfile", HERE.parent / "notice/classfile.py")
    require(spec is not None and spec.loader is not None, "class parser")
    classfile = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(classfile)
    with zipfile.ZipFile(args.client_jar) as jar:
        for name, pin in pins["classes"].items():
            require(classfile.inspect_class(jar.read(name.replace(".", "/") + ".class")) == pin, "class/Code " + name)
        for name, pin in pins["resources"].items():
            require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"), "private resource allowlist")
            data = jar.read(name)
            require({"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} == pin, "private resource pin")
            path = work / "resources" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    before = {str(p.resolve()): fingerprint(p) for p in (args.native, args.callbacks, args.java, args.javac, args.client_jar, *libraries,
              *(args.native_dir / name for name in pins["native"]))}
    for label, tool in (("java", args.java), ("javac", args.javac)):
        result = run([tool.resolve(), "-version"], work, label + "-version", env=env)
        require(result.returncode == 0 and re.search(rb'(?:version "|javac )17\.', result.stdout + result.stderr), "actual JDK17")
    helpers = work / "helpers"
    helpers.mkdir()
    classpath = os.pathsep.join(str(p.resolve()) for p in (helpers, args.client_jar, *libraries))
    compiled = run([args.javac.resolve(), "--release", "17", "-encoding", "UTF-8", "-classpath", classpath,
                    "-d", helpers, HERE / "CameraGLObservation.java"], work, "compile", env=env)
    require(compiled.returncode == 0 and compiled.stderr == b"", "authored helper compile")
    helper_before = {p.relative_to(helpers).as_posix(): fingerprint(p) for p in helpers.rglob("*") if p.is_file()}
    require(set(helper_before) == {"CameraGLObservation.class", "CameraGLObservation$Added.class"}, "helper-only exact classpath")
    original = run([args.java.resolve(), "-Xint", "-Xverify:all", "-Xmx512m",
        "-Dorg.lwjgl.librarypath=" + str(args.native_dir.resolve()),
        "-Xlog:class+init=info:file=class-init.log", "-classpath", classpath, "CameraGLObservation", work / "original-worlds"], work, "original", env=env)
    require(original.returncode == 0 and original.stderr == b"", "actual GL original exit/diagnostics")
    lines = original.stdout.splitlines()
    require(lines[-1:] == [b"CAMERA_GL_ORIGINAL completed"], "actual original completion")
    require(lines[3:6] == [b"16 achievements", b"151 recipes", b"OWNERS dc fd ji n px"], "actual original banners and owners")
    require(re.fullmatch(rb"FALLBACK_FONT .+ [0-3] [1-9][0-9]*", lines[6]) is not None and
            lines[7] == b"MAP_OWNER actual-ra-iy-ji image128x128 int16384 rgba65536-zero retained1 transfer1048576-limit65536", "actual constructor observations")
    cursor = 8
    for name in pins["classes"]:
        require(cursor < len(lines) and lines[cursor].startswith(b"BIND\t" + name.encode() + b"\t"), "fixed runtime binding order")
        cursor += 1
    for which in range(29):
        if which in (19, 20):
            continue
        if which in (17, 18, 21, 28):
            require(lines[cursor] == ("NULL %d java.lang.NullPointerException" % which).encode(), "actual ordered null row")
            cursor += 1
        else:
            require(re.fullmatch(("CASE %d FOV " % which).encode() + rb"[0-9a-f]{8} CLOUD false", lines[cursor]) is not None, "actual ordered completed case")
            cursor += 1
            for label, digits, count in (("ORIGIN", 16, 4), ("PROJECTION", 8, 16), ("MODELVIEW", 8, 16)):
                require(re.fullmatch(("%s %d" % (label, which)).encode() +
                    (rb"(?: [0-9a-f]{%d}){%d}" % (digits, count)), lines[cursor]) is not None, "actual ordered matrix/origin row")
                cursor += 1
    for which in (0, 1, 2, 3, 4, 8, 9):
        expected = "null" if which in (3, 4, 9) else "return"
        require(lines[cursor] == ("CALLBACK_ORIGINAL %d %s" % (which, expected)).encode(), "actual ordered callback row")
        cursor += 1
        if which <= 4:
            for label in ("CBPROJECTION", "CBMODELVIEW"):
                require(re.fullmatch(("%s %d" % (label, which)).encode() + rb"(?: [0-9a-f]{8}){16}", lines[cursor]) is not None, "actual ordered callback prefix matrix")
                cursor += 1
    require(cursor + 1 == len(lines), "complete original stdout inventory")
    bindings = {}
    for line in lines:
        if line.startswith(b"BIND\t"):
            _, name, origin, digest = line.decode().split("\t")
            parsed = urlparse(origin)
            path = unquote(parsed.path)
            if re.match(r"^/[A-Za-z]:/", path):
                path = path[1:]
            require(parsed.scheme == "file" and Path(path).resolve() == args.client_jar.resolve(), "actual runtime JAR origin")
            require(name not in bindings and digest == pins["classes"][name]["class_sha256"], "runtime class bytes")
            bindings[name] = digest
    require(set(bindings) == set(pins["classes"]), "complete runtime bindings")
    expected_callbacks = [(0, "return"), (1, "return"), (2, "return"), (3, "null"), (4, "null"), (8, "return"), (9, "null")]
    callback_outcomes = [line for line in lines if line.startswith(b"CALLBACK_ORIGINAL ")]
    require(callback_outcomes == [("CALLBACK_ORIGINAL %d %s" % pair).encode() for pair in expected_callbacks], "exact ordered original callback inventory")
    native = run([args.native.resolve(), work / "resources", work / "world"], work, "native", env=env)
    observed = native_rows(native)
    aggregate = {"matrices": 0, "coefficients": 0, "exact_bits": 0, "maximum_absolute_difference": 0.0}
    by_label = {tuple(line.split()[:2]): line for line in lines if line.startswith((b"PROJECTION ", b"MODELVIEW ", b"ORIGIN "))}
    fovs = {int(line.split()[1]): line.split()[3] for line in lines if line.startswith(b"CASE ")}
    for which, cells in observed["cases"].items():
        if cells[2] != b"0":
            continue
        require(cells[6] == fovs[which], "actual source FOV bits " + str(which))
        require(observed["origins"][which] == by_label[b"ORIGIN", str(which).encode()], "authored scalar origin projection " + str(which))
        for kind in ("PROJECTION", "MODELVIEW"):
            actual = by_label[kind.encode(), str(which).encode()].split()[2:]
            selected = observed["matrices"][kind, which]
            require(len(actual) == len(selected) == 16, "matrix extent")
            aggregate["matrices"] += 1
            for a, b in zip(actual, selected):
                left, right = number(a), number(b)
                require(math.isfinite(left) and math.isfinite(right), "finite actual/native matrix profile")
                delta = abs(left-right)
                require(delta <= 0.000002, "actual driver/native float matrix tolerance " + str((kind, which, a, b)))
                aggregate["maximum_absolute_difference"] = max(aggregate["maximum_absolute_difference"], delta)
                aggregate["coefficients"] += 1
                aggregate["exact_bits"] += a == b
    callbacks = run([args.callbacks.resolve(), work / "resources", work / "callback-world"], work, "native-callbacks", env=env)
    callbacks_observed = callback_rows(callbacks)
    callback_comparison = {"matrices": 0, "coefficients": 0, "exact_bits": 0, "maximum_absolute_difference": 0.0}
    callback_labels = {}
    for line in lines:
        if line.startswith((b"CBPROJECTION ", b"CBMODELVIEW ")):
            cells = line.split()
            key = (cells[0].decode(), int(cells[1]))
            require(key not in callback_labels and len(cells) == 18 and all(re.fullmatch(rb"[0-9a-f]{8}", cell) for cell in cells[2:]), "original callback matrix extent/identity")
            callback_labels[key] = cells[2:]
    require(set(callback_labels) == set(callbacks_observed["matrices"]), "all ten callback prefix matrices")
    for key, actual in callback_labels.items():
        selected = callbacks_observed["matrices"][key]
        callback_comparison["matrices"] += 1
        for a, b in zip(actual, selected):
            left, right = number(a), number(b)
            require(math.isfinite(left) and math.isfinite(right), "finite callback matrix profile")
            delta = abs(left-right)
            require(delta <= 0.000002, "actual callback driver/native matrix tolerance " + str((key, a, b)))
            callback_comparison["coefficients"] += 1
            callback_comparison["exact_bits"] += a == b
            callback_comparison["maximum_absolute_difference"] = max(callback_comparison["maximum_absolute_difference"], delta)
    require(helper_before == {p.relative_to(helpers).as_posix(): fingerprint(p) for p in helpers.rglob("*") if p.is_file()}, "helper products unchanged")
    require(before == {p: fingerprint(Path(p)) for p in before}, "input bytes unchanged")
    require(reference_before == {name: fingerprint(HERE / name) for name in reference_before}, "reference bytes unchanged")
    require(native_before == {name: fingerprint(repo / name) for name in native_before}, "native source/header bytes unchanged")
    require(controller_before == {name: fingerprint(repo / name) for name in controller_before}, "controller bytes unchanged")
    report = {"status": "PASS", "scope": "selected CPU operations vs actual original compatibility GL matrices; not visual or complete px equality",
        "inputs": before, "runtime_bindings": bindings, "reference_sources": reference_before,
        "native_sources": native_before, "controllers": controller_before,
        "driver": [x.decode() for x in lines[:3]],
        "native_checks": observed["checks"], "matrix_comparison": aggregate,
        "callback_checks": callbacks_observed["checks"], "callback_comparison": callback_comparison,
        "callback_original_outcomes": [line.decode() for line in callback_outcomes], "helper_products": helper_before,
        "callback_capture": callbacks.capture_report,
        "original_capture": original.capture_report, "native_capture": native.capture_report,
        "work_directory": str(work)}
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
