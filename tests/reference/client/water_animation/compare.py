"""Exact unchanged-original water arrays/RGBA/Math continuation comparison."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from urllib.parse import unquote, urlparse
import zipfile
sys.dont_write_bytecode = True
from support import CASES, MAX_TRACE_BYTES, inspect_trace, native_success, require, run

HERE = Path(__file__).resolve().parent
LIBRARY_CLASSES = ("java.lang.Math", "java.lang.Math$RandomNumberGeneratorHolder", "java.util.Random")


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client-jar", "native", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    test_tree = HERE.parents[2]
    require(args.work_dir.resolve() != test_tree and test_tree not in args.work_dir.resolve().parents,
            "private outputs must stay outside the source test tree")
    require(args.java.resolve().parent == args.javac.resolve().parent, "explicit matching JDK17 tools")
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        require(not os.environ.get(key), "ambient Java option rejected: " + key)
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"):
        env.pop(key, None)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="water-reference-", dir=args.work_dir.resolve()))
    report = {"status": "RUNNING", "work_directory": str(work)}
    try:
        pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
        proof = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
        sources = {path.name: fingerprint(path) for path in HERE.iterdir() if path.is_file()}
        for name, pin in proof["reference_files"].items():
            require(fingerprint(HERE / name) == pin, "reference source pin " + name)
        repo = HERE.parents[3]
        native_sources = {name: fingerprint(repo / name) for name in proof["native_sources"]}
        require(native_sources == proof["native_sources"], "native source cohort pins")
        inputs = {str(path.resolve(strict=True)): fingerprint(path) for path in (args.client_jar, args.java, args.javac, args.native)}
        report.update(inputs=inputs, sources=sources, native_sources=native_sources)
        require(fingerprint(args.client_jar) == pins["client_jar"], "whole supplied JAR pin")
        spec = importlib.util.spec_from_file_location("water_classfile", HERE.parent / "notice/classfile.py")
        require(spec is not None and spec.loader is not None, "structural class parser")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        resources = work / "resources"
        with zipfile.ZipFile(args.client_jar) as archive:
            for name, pin in pins["classes"].items():
                require(module.inspect_class(archive.read(name.replace(".", "/") + ".class")) == pin, "original class/Code " + name)
            for name, pin in pins["resources"].items():
                require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"), "private resource allowlist")
                data = archive.read(name)
                require({"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} == pin, "private resource pin")
                path = resources / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        report["tools"] = {}
        for name, tool in (("java", args.java), ("javac", args.javac)):
            result = run([tool.resolve(), "-version"], work, name + "-version", env=env)
            require(result.returncode == 0 and re.search(rb'(?:version "|javac )17\.', result.stdout + result.stderr), "JDK17 version")
            report["tools"][name] = {"version": (result.stdout + result.stderr).decode(), "capture": result.capture_report}
        classes = work / "helpers"
        classes.mkdir()
        classpath = os.pathsep.join((str(classes), str(args.client_jar.resolve())))
        result = run([args.javac.resolve(), "--release", "17", "-encoding", "UTF-8", "-classpath", classpath,
            "-d", classes, HERE / "WaterAnimationObservation.java"], work, "compile", env=env)
        require(result.returncode == 0 and not result.stderr, "helper compilation")
        helpers = {path.name: fingerprint(path) for path in classes.rglob("*.class")}
        require(set(helpers) == {"WaterAnimationObservation.class"}, "sole authored helper class")
        names = work / "classes.txt"
        names.write_text("\n".join((*pins["classes"], *LIBRARY_CLASSES)) + "\n", encoding="ascii", newline="\n")
        original_trace, native_trace = work / "original.trace", work / "native.trace"
        original = run([args.java.resolve(), "-Xint", "-Xverify:all", "-Xmx256m",
            "--add-opens=java.base/java.lang=ALL-UNNAMED", "--add-opens=java.base/java.util=ALL-UNNAMED",
            "-Xlog:class+init=info:file=class-init.log", "-classpath", classpath,
            "WaterAnimationObservation", original_trace, names, work / "binding.tsv"], work, "original", env=env)
        require(original.returncode == 0 and original.stderr == b"", "original process/diagnostics")
        require(original.stdout.splitlines() == [b"16 achievements", b"151 recipes",
            ("Water animation original: " + str(len(CASES)) + " cases").encode()], "actual original stdout")
        binding = {}
        for line in (work / "binding.tsv").read_text(encoding="utf-8").splitlines():
            name, size, digest, origin = line.split("\t")
            require(name not in binding, "duplicate runtime origin")
            if name in LIBRARY_CLASSES:
                require(origin == "jrt:/java.base", "actual bootstrap library class")
            else:
                require(name in pins["classes"], "runtime original class inventory")
                pin = pins["classes"][name]
                require(int(size) == pin["class_bytes"] and digest == pin["class_sha256"], "runtime class bytes")
                uri = urlparse(origin)
                path = unquote(uri.path)
                if re.match(r"^/[A-Za-z]:", path):
                    path = path[1:]
                require(uri.scheme == "file" and Path(path).resolve() == args.client_jar.resolve(), "runtime supplied JAR origin")
            binding[name] = {"bytes": int(size), "sha256": digest, "origin": origin}
        require(set(binding) == set(pins["classes"]) | set(LIBRARY_CLASSES), "complete runtime binding")
        native = run([args.native.resolve(), resources, native_trace], work, "native", env=env)
        checks = native_success(native)
        observations = []
        for path in (original_trace, native_trace):
            require(path.stat().st_size <= MAX_TRACE_BYTES, "trace byte bound")
            observations.append(inspect_trace(path.read_bytes()))
        require(observations[0]["lines"] == observations[1]["lines"], "raw array/RGBA/counter/Math traces differ")
        observations[0].pop("lines")
        init = (work / "class-init.log").read_text(encoding="utf-8")
        for excluded in ("org/lwjgl/", "net/minecraft/client/Minecraft", "xi", "cj"):
            require("Initializing '" + excluded not in init, "unexpected class initialization " + excluded)
        require(sources == {path.name: fingerprint(path) for path in HERE.iterdir() if path.is_file()}, "reference sources changed")
        require(native_sources == {name: fingerprint(repo / name) for name in native_sources}, "native source changed")
        require(inputs == {str(path.resolve()): fingerprint(path) for path in (args.client_jar, args.java, args.javac, args.native)}, "binary inputs changed")
        require(helpers == {path.name: fingerprint(path) for path in classes.rglob("*.class")}, "helper classes changed")
        report.update(status="PASS", observation=observations[0], native_checks=checks, runtime_binding=binding,
            helper_classes=helpers, original_capture=original.capture_report, native_capture=native.capture_report,
            traces={path.name: fingerprint(path) for path in (original_trace, native_trace)}, class_init=fingerprint(work / "class-init.log"))
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({name: report[name] for name in ("status", "work_directory", "observation", "native_checks", "error") if name in report}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
