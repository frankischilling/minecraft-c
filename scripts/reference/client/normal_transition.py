#!/usr/bin/env python3
"""Capture the authored V3 original-NW normal transition on a supplied desktop.

Supply explicit JDK17 tools, the original client, all four pinned SDK archives,
an extracted native directory, and a new --work-dir. On Linux run this wrapper
under an existing display or xvfb-run; it does not start a display server.

The Java source is byte-identical to the historical V3 observer. This wrapper
is a new producer: its reports do not replace or requalify earlier captures.
PASS means a complete, stable observation, not native parity or admission of a
new driver to the frozen normal-transition corpus. Missing prerequisites write
UNAVAILABLE and exit77. Invalid pins or executed failures write FAIL and exit1.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import urlsplit
from urllib.request import url2pathname
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "tests/reference/client/lighting_raster"
SUPPORT = ROOT / "tests/reference/client/controller"
SOURCE = PACKET / "NormalTransition.java"
SOURCE_PIN = {"bytes": 7939,
              "sha256": "a6622f734a3d1a2d8ebcd16057589bce327081e9e807503beb6dea223b9a0049"}
NATIVES = {
    "windows": ("lwjgl64.dll", "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"),
    "linux": ("liblwjgl64.so", "60a9a67061a3dc6b9d055946c58f1f64792abb2127ba0156eaf735dbee5bc729"),
}
AWT = ("awt", "awt_xawt", "awt_headless", "jawt", "fontmanager")
MAX_INPUT = 512 * 1024 * 1024


class Unavailable(Exception):
    pass


def need(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    need(path.is_file() and path.stat().st_size <= MAX_INPUT, "bounded regular input: " + str(path))
    digest, count = hashlib.sha256(), 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            count += len(block)
            need(count <= MAX_INPUT, "input grew beyond byte bound")
            digest.update(block)
    return {"bytes": count, "sha256": digest.hexdigest()}


def snapshot(inputs):
    return {role: {"path": str(path), **fingerprint(path)} for role, path in inputs.items()}


def existing(value, role, *, directory=False, tool=False):
    selected = shutil.which(str(value)) if tool else None
    path = Path(selected or value).resolve()
    if not (path.is_dir() if directory else path.is_file()):
        raise Unavailable("missing " + role + ": " + str(path))
    need("|" not in str(path) and "\n" not in str(path) and "\r" not in str(path),
         "unambiguous observer path: " + role)
    return path


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def native_inventory(directory):
    paths = sorted(directory.iterdir())
    need(len(paths) <= 64, "bounded flat native directory")
    result = {}
    for path in paths:
        need(path.is_file() and not path.is_symlink(), "ordinary flat native input: " + str(path))
        result[path.name] = fingerprint(path)
    return result


def helper_inventory(directory):
    paths = sorted(directory.iterdir())
    need(len(paths) <= 16 and all(p.is_file() and not p.is_symlink() for p in paths),
         "bounded ordinary helper output")
    return {p.name: fingerprint(p) for p in paths}


def file_origin(text):
    url = urlsplit(text)
    need(url.scheme == "file" and url.netloc in ("", "localhost"), "local class CodeSource")
    return Path(url2pathname(url.path)).resolve(strict=True)


def observations(text, jar, lwjgl, native, jdk_natives):
    lines = text.splitlines()
    need(lines and all(line.startswith("NORMAL_") for line in lines), "declared observer stdout")
    drivers = [line for line in lines if line.startswith("NORMAL_DRIVER ")]
    origins = [line for line in lines if line.startswith("NORMAL_ORIGIN ")]
    finishes = [line for line in lines if line.startswith("NORMAL_FINISH ")]
    need(len(drivers) == len(origins) == len(finishes) == 1, "single driver/origin/completion receipt")
    origin = origins[0].removeprefix("NORMAL_ORIGIN ").split(" | ")
    need(len(origin) == 2 and file_origin(origin[0]) == jar and file_origin(origin[1]) == lwjgl,
         "actual original NW and unchanged LWJGL class origins")
    need(lines[-1] == finishes[0] and finishes[0].startswith("NORMAL_FINISH network=0 links="),
         "terminal completion with zero network attempts")
    links = finishes[0].removeprefix("NORMAL_FINISH network=0 links=").split("|")
    linked_native = False
    for link in links:
        if link in AWT:
            continue
        path = Path(link)
        need(path.is_absolute(), "declared absolute native link")
        path = path.resolve(strict=True)
        need(path == native or path in jdk_natives, "only pinned LWJGL/declared JDK native links")
        linked_native |= path == native
    need(linked_native, "actual pinned LWJGL native loaded")
    expected, actual = [], []
    word3 = r"[0-9a-f]{8}(?:,[0-9a-f]{8}){2}"
    for feedback in (False, True):
        for lighting in (0, 1):
            for queried in (0, 1):
                tag = f"{'feedback' if feedback else 'fixed'}-lighting{lighting}-queries{queried}"
                if queried:
                    for label in ("initial", *[f"face{i}" for i in range(6)]):
                        expected.append((f"NORMAL_CURRENT {tag} {label} ", word3))
                expected.append((f"NORMAL_CONSUMED {tag} " if feedback else f"NORMAL_PIXEL {tag} ",
                                 r"[0-9a-f]{8}(?:,[0-9a-f]{8}){23}" if feedback else r"[0-9a-f]{8}"))
                expected.append((f"NORMAL_AFTER {tag} ", word3 + r" normal-array=0 error=0"))
    actual = [line for line in lines if line.startswith(("NORMAL_CURRENT ", "NORMAL_CONSUMED ",
                                                        "NORMAL_PIXEL ", "NORMAL_AFTER "))]
    need(len(actual) == len(expected) == 44 and len(lines) == 47, "complete ordered V3 observation inventory")
    for line, (prefix, pattern) in zip(actual, expected):
        need(line.startswith(prefix) and re.fullmatch(pattern, line[len(prefix):]),
             "ordered actual state/pixel/feedback row: " + prefix)
    return {"driver": drivers[0], "origins": origin, "native_links": links, "rows": actual}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ("java", "javac", "client", "lwjgl", "lwjgl-util", "jinput", "jutils", "natives"):
        parser.add_argument("--" + role, type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True, help="new private capture directory")
    args = parser.parse_args()
    output = args.work_dir.resolve()
    try:
        output.mkdir(parents=True, exist_ok=False)
    except OSError as error:
        print("Capture directory unavailable: " + str(error), file=sys.stderr)
        return 1
    host = "windows" if os.name == "nt" else "linux" if sys.platform.startswith("linux") else None
    inputs = {"wrapper": Path(__file__).resolve(), "observer_source": SOURCE,
              "classfile_validator": SUPPORT / "classfile.py", "process_capture": SUPPORT / "capture.py",
              "archive_pins": ROOT / "tests/reference/client/render_global_sky/capture_prefix.py",
              "source_pins": ROOT / "tests/reference/client/nw_normals/source-pins.json",
              "python": Path(sys.executable).resolve()}
    report = {"schema": 1, "status": "FAIL", "host": host, "processes": [],
              "scope": "Fresh finite V3 unchanged-original NW direct normal-array/no-normal observations. "
                       "No game classes replaced or source fields assigned. Native pixel parity, whole BB/World "
                       "frames and new-driver admission are outside this capture.",
              "historical_captures_requalified": False, "input_freeze_complete": False}
    classes = output / "classes"
    exit_code = 1
    try:
        report["input_before"] = snapshot(inputs)
        source_before = dict(report["input_before"])
        if host is None:
            raise Unavailable("only native Windows and Linux capture hosts are supported")
        java = existing(args.java, "Java", tool=True)
        javac = existing(args.javac, "Javac", tool=True)
        need(java.parent.samefile(javac.parent), "matching JDK tool directory")
        jar = existing(args.client, "original client")
        libraries = [existing(getattr(args, name), name) for name in ("lwjgl", "lwjgl_util", "jinput", "jutils")]
        natives = existing(args.natives, "native directory", directory=True)
        need(not output.is_relative_to(natives), "capture output outside supplied native directory")
        native = existing(natives / NATIVES[host][0], "pinned LWJGL native")
        inputs.update(java=java, javac=javac, client=jar, native=native)
        inputs.update(zip(("lwjgl", "lwjgl_util", "jinput", "jutils"), libraries))
        jdk_natives = set()
        jdk = java.parent.parent
        for folder in ("lib", "bin"):
            for name in AWT:
                filename = name + ".dll" if host == "windows" else "lib" + name + ".so"
                need(not (natives / filename).exists(), "JDK native must not be shadowed in LWJGL directory")
                path = jdk / folder / filename
                if path.is_file():
                    path = path.resolve(strict=True)
                    inputs[f"jdk_{folder}_{name}"] = path
                    jdk_natives.add(path)
        for relative in ("release", "lib/modules", "bin/java.dll", "bin/server/jvm.dll",
                         "lib/libjava.so", "lib/server/libjvm.so"):
            path = jdk / relative
            if path.is_file():
                inputs["jdk_" + relative.replace("/", "_")] = path.resolve(strict=True)
        report["natives_before"] = native_inventory(natives)
        report["native_directory"] = str(natives)
        report["input_before"] = snapshot(inputs)
        need(all(report["input_before"][name] == value for name, value in source_before.items()),
             "wrapper/validator/source changed during setup")
        report["input_freeze_complete"] = True
        sys.path.insert(0, str(SUPPORT))
        process = load("normal_transition_capture", inputs["process_capture"])
        classfile = load("normal_transition_classfile", inputs["classfile_validator"])
        pins = load("normal_transition_archive_pins", inputs["archive_pins"])
        need(fingerprint(SOURCE) == SOURCE_PIN, "byte-identical historical V3 authored helper")
        need(fingerprint(jar)["sha256"] == pins.JAR, "unchanged original client archive")
        need(len(pins.LIBRARIES) == 4 and all(fingerprint(path)["sha256"] == expected
             for path, expected in zip(libraries, pins.LIBRARIES.values())), "unchanged role-specific SDK archives")
        need(fingerprint(native)["sha256"] == NATIVES[host][1], "pinned native implementation")
        authority = json.loads(inputs["source_pins"].read_text(encoding="utf-8"))
        need(authority["client_jar_sha256"] == pins.JAR, "selected class/Code archive authority")
        with zipfile.ZipFile(jar) as archive:
            for name, expected in authority["classes"].items():
                actual = classfile.inspect_class(archive.read(name + ".class"))
                selected = {(m["name"], m["descriptor"]) for m in expected["methods"]}
                actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
                need(actual == expected, "unchanged complete class and selected Code: " + name)
        report["verified_source_classes"] = sorted(authority["classes"])
        environment = os.environ.copy()
        removed = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS",
                   "CLASSPATH", "LD_PRELOAD", "LD_AUDIT", "DYLD_INSERT_LIBRARIES")
        for name in removed:
            environment.pop(name, None)
        report["environment"] = {name: environment.get(name) for name in ("DISPLAY", "LIBGL_ALWAYS_SOFTWARE")}
        report["removed_option_variables"] = list(removed)
        if host == "linux" and not environment.get("DISPLAY"):
            raise Unavailable("Linux capture requires an existing DISPLAY; run under xvfb-run if needed")

        def run(command, tag, timeout=90):
            receipt = process.capture(list(map(str, command)), output, tag, timeout, environment, output)
            report["processes"].append({"tag": tag, **receipt})
            process.require_success(receipt)
            return ((output / receipt["stdout_file"]).read_text(encoding="utf-8"),
                    (output / receipt["stderr_file"]).read_text(encoding="utf-8"))

        versions = {}
        for name, tool in (("java", java), ("javac", javac)):
            stdout, stderr = run([tool, "-version"], name + "-version", 10)
            versions[name] = stdout + stderr
        runtime = re.search(r'version "(17(?:\.[0-9]+){0,3})', versions["java"])
        compiler = re.search(r'javac (17(?:\.[0-9]+){0,3})', versions["javac"])
        if not runtime or not compiler or runtime[1] != compiler[1]:
            raise Unavailable("matching JDK17 runtime/compiler versions required")
        report["tools"] = versions
        classes.mkdir()
        compile_cp = os.pathsep.join(map(str, (jar, *libraries)))
        run([javac, "-encoding", "UTF-8", "-cp", compile_cp, "-d", classes, SOURCE], "compile")
        report["helpers_before"] = helper_inventory(classes)
        need(set(report["helpers_before"]) == {"NormalTransition.class", "NormalTransition$1.class"},
             "only the two authored helper classes compiled")
        report["helper_Code"] = {path.name: classfile.inspect_class(path.read_bytes()) for path in classes.iterdir()}
        classpath = os.pathsep.join(map(str, (classes, jar, *libraries)))
        stdout, _ = run([java, "-Djava.security.manager=allow",
                         "-Dorg.lwjgl.opengl.Display.allowSoftwareOpenGL=true",
                         "-Dorg.lwjgl.librarypath=" + str(natives), "-Djava.library.path=" + str(natives),
                         "-cp", classpath, "NormalTransition", natives], "observe")
        report["observations"] = observations(stdout, jar, libraries[0], native, jdk_natives)
        report["status"] = "PASS"
        exit_code = 0
    except Unavailable as error:
        report.update(status="UNAVAILABLE", error={"type": type(error).__name__, "message": str(error)})
        exit_code = 77
    except BaseException as error:
        report.update(status="FAIL", error={"type": type(error).__name__, "message": str(error)})
        exit_code = 130 if isinstance(error, KeyboardInterrupt) else 1
    finally:
        report["input_after"], final_errors = {}, {}
        for role, path in inputs.items():
            try:
                report["input_after"][role] = {"path": str(path), **fingerprint(path)}
            except Exception as error:
                final_errors[role] = {"type": type(error).__name__, "message": str(error)}
        if report["input_freeze_complete"] and report["input_before"] != report["input_after"]:
            final_errors["input_continuity"] = "Input bytes or paths changed"
        if "natives_before" in report:
            try:
                report["natives_after"] = native_inventory(Path(report["native_directory"]))
                if report["natives_before"] != report["natives_after"]:
                    final_errors["native_continuity"] = "Native directory changed"
            except Exception as error:
                final_errors["native_continuity"] = str(error)
        if classes.is_dir():
            try:
                report["helpers_after"] = helper_inventory(classes)
                if "helpers_before" in report and report["helpers_before"] != report["helpers_after"]:
                    final_errors["helper_continuity"] = "Compiled helper changed"
            except Exception as error:
                final_errors["helper_continuity"] = str(error)
        if final_errors:
            report.update(status="FAIL", finalization_errors=final_errors)
            exit_code = 1
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Normal transition {report['status']}; {output / 'report.json'}")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
