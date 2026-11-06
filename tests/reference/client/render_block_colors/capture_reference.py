"""Observe unchanged Minecraft.v and Block color virtuals with real EW/climate."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import JAR, LIBRARIES


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def rows(text, mode):
    prefix, count = ("AO ", 7) if mode == "ao" else ("COLOR ", 25)
    result = [line for line in text.splitlines() if line.startswith(prefix)]
    ensure(len(result) == count and len({line.split()[1] for line in result}) == count,
           "complete original " + mode + " row inventory")
    for line in result:
        cells = line.split()
        ensure(len(cells) == (5 if mode == "ao" else 11), "original row cell count")
        ensure(cells[2] in ("OK", "NULL", "INDEX"), "original result category")
        if mode == "color":
            ensure(re.fullmatch(r"[0-9a-f]{8}", cells[3]) is not None, "original color bits")
            ensure(all(v in ("0", "1") for v in cells[4:6] + cells[8:11]), "original identity bits")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(p) or "").resolve(strict=True) for p in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(p) == h for p, h in zip(libraries, LIBRARIES.values())),
           "unchanged original archive/SDK metadata")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
            ensure(actual == expected, "unchanged class/method: " + name)
    picking_path = HERE.parent / "picking/observe.py"
    authored_path = HERE.parents[2] / "tools/client/test_picking.py"
    source = HERE / "ColorObservation.java"
    inputs = [java, javac, jar, *libraries, source, HERE / "capture_reference.py", HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py", picking_path, authored_path,
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    world = args.output / "world"
    module("colors_authored_world", picking_path).world(world)
    world_before = inventory(world)
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before, "world_before_sha256": world_before,
              "scope": "7 static AO calls and25 Block color virtual calls; genuine saved World/EW and source climate sampling. Explicit priming and prepared actual three-cell climate arrays; palette setters. No game replacement, renderer or constructor qualification. AO PCs are source-derived sites, not JDI events."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt["stderr_file"]).read_text() + (args.output / receipt["stdout_file"]).read_text()
            ensure(re.search(r"(?:version \"|javac )17\.", version), "JDK17 required")
        classpath = os.pathsep.join(str(p.resolve()) for p in [classes, jar, *libraries])
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", classpath, "-d", str(classes), str(source)],
                          args.output, "compile", 30, environment)
        require_success(receipt)
        helper = inventory(classes)
        ensure(set(helper) == {"ColorObservation.class", "ColorObservation$1.class"}, "authored helper inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                           "-Djava.security.manager=allow", "-cp", classpath, "ColorObservation",
                           str(jar), str(world.resolve())], args.output, "original", 100, environment,
                          args.output.resolve())
        report["original"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original colors: 7 AO; 25 Block; network=0; native=0", "normal original completion")
        observed = {mode: rows(text, mode) for mode in ("ao", "color")}
        (args.output / "finite.json").write_text(json.dumps(observed, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=32, fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["world_after_sha256"] = inventory(world)
        report["helper_after_sha256"] = inventory(classes)
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or helper == report["helper_after_sha256"])
        same = same and {p: h for p, h in world_before.items() if p != "session.lock"} == {
            p: h for p, h in report["world_after_sha256"].items() if p != "session.lock"}
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "original/helper/runtime/World source inputs changed")
    print("Original colors: 32 rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
