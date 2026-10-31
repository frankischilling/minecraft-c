"""Capture original LL frame arithmetic on a genuinely constructed wh(NULL)."""
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
spec = importlib.util.spec_from_file_location("tile_frame_frozen_support", HERE / "capture_reference.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
ensure, digest, capture, require_success = base.ensure, base.digest, base.capture, base.require_success


def encode(cases):
    return [" ".join([str(case["id"]), case["name"], case["partial"], *case["float_pose"], *case["double_pose"]]) for case in cases]


def rows(text, cases):
    lines = text.splitlines()
    tags = {"TILE_FRAME_CTOR", "TILE_FRAME_DECLARED", "TILE_FRAME_INPUT", "TILE_FRAME_RESULT", "TILE_FRAME_SIDE"}
    result = [line for line in lines if line.split(" ", 1)[0] in tags]
    ensure(len(result) == 1 + 4 * len(cases) and len(lines) == len(result) + 1,
           "complete actual constructor and all arithmetic/side-effect rows; no ignored output or exceptions")
    ctor = result[0].split()
    ensure(len(ctor) == 8 and ctor[:4] == ["TILE_FRAME_CTOR", "wh", "1", "1"] and
           all(re.fullmatch(r"-?[0-9]+", value) for value in ctor[4:7]) and ctor[7] in ("0", "1"),
           "actual normal untouched Pig constructor, null World, Entity ID/counter/Watcher observations")
    for index, declared in enumerate(encode(cases)):
        declared_row, input_row, output_row, side_row = result[1 + 4 * index:5 + 4 * index]
        ensure(declared_row == "TILE_FRAME_DECLARED " + declared, "complete bound authored operand inventory")
        cells = input_row.split()
        ensure(len(cells) == 14 and cells[:3] == ["TILE_FRAME_INPUT", str(index), cases[index]["name"]] and
               all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[3:8]) and
               all(re.fullmatch(r"[0-9a-f]{16}", value) for value in cells[8:]), "actual stored source pose and partial raw words")
        # Stored signaling-NaN representations are independently observed;
        # never replace them with nominal input bits or normalized NaNs.
        cells = output_row.split()
        ensure(len(cells) == 15 and cells[:3] == ["TILE_FRAME_RESULT", str(index), "1"] and
               all(value in ("0", "1") for value in cells[3:10]) and
               all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[10:12]) and
               all(re.fullmatch(r"[0-9a-f]{16}", value) for value in cells[12:]), "normal whole LL frame return and exact raw outputs")
        ensure(side_row.split() == ["TILE_FRAME_SIDE", str(index)] + ["1"] * 7,
               "actual unchanged uniquifier/private Random/Math Random/Entity ID/pose/unrelated LL/Watcher state")
    ensure(lines[-1] == f"Original TileEntityRenderer frame: cases={len(cases)}; rows={len(result)}; network=0; native=0; writes=0",
           "complete normal zero-external-work helper return")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(args.java and args.javac, "matching JDK17 tools required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "same JDK17 tools directory")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in base.LIBRARIES]
    ensure(digest(jar) == base.JAR and all(digest(path) == pin for path, pin in zip(libraries, base.LIBRARIES.values())),
           "unchanged original client and SDK type archive pins")
    pins = json.loads((HERE / "frame-source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == base.JAR, "same original source authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = base.classfile.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"] if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "whole class/selected original Code and attribute pins: " + name)
    inputs_spec = json.loads((HERE / "frame-inputs.json").read_text(encoding="utf-8"))
    cases = inputs_spec["cases"]
    ensure(len(cases) == 24 and [case["id"] for case in cases] == list(range(24)) and
           len({case["name"] for case in cases}) == 24 and
           inputs_spec["float_pose_fields"] == ["aU", "aS", "aV", "aT"] and
           inputs_spec["double_pose_fields"] == ["bl", "bm", "bn", "aM", "aN", "aO"], "complete authored case and field inventory")
    for case in cases:
        ensure(re.fullmatch(r"[a-z0-9_]+", case["name"]) and len(case["float_pose"]) == 4 and len(case["double_pose"]) == 6 and
               all(re.fullmatch(r"[0-9a-f]{8}", value) for value in [case["partial"], *case["float_pose"]]) and
               all(re.fullmatch(r"[0-9a-f]{16}", value) for value in case["double_pose"]), "authored raw operand encodings")
    args.output.mkdir(parents=True, exist_ok=False)
    tsv = args.output / "frame-inputs.tsv"
    tsv.write_text("\n".join(encode(cases)) + "\n", encoding="ascii", newline="\n")
    inputs = [java, javac, jar, *libraries, tsv, HERE / "TileEntityRendererFrameObservation.java", HERE / "capture_frame.py",
              HERE / "frame-source-pins.json", HERE / "frame-inputs.json", HERE / "FRAME_README.md", HERE / "capture_reference.py",
              HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Unchanged LL frame arithmetic with genuine original wh(fd=NULL) receiver. Constructor is an original "
                       "operand prerequisite only, with no native Pig-construction parity claim. No World/Engine/Font instance, "
                       "asset, native link, graphics or draw. Seed only10 read Entity pose fields and5 LL scalar output sentinels.",
              "declared_ll_scalar_inputs": {"h_i": ["3f012345", "bf054321"],
                                            "j_k_l": ["3ff123456789abcd", "3ff123456789abce", "3ff123456789abcf"]},
              "inventory": cases}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            versions.append((args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text())
        runtime, compiler = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0]), re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 versions")
        report["jdk"] = versions
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "TileEntityRendererFrameObservation.java")], args.output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"TileEntityRendererFrameObservation.class", "TileEntityRendererFrameObservation$1.class"},
               "only authored helper classes, no game class replacement")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED", "--add-opens", "java.base/java.lang=ALL-UNNAMED",
                           "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                           "-Xlog:class+init=info:file=class-init.log::filesize=2m,filecount=1", "-cp", cp,
                           "TileEntityRendererFrameObservation", str(jar), str(tsv.resolve())],
                          args.output, "original", 60, environment, args.output.resolve())
        report["original"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text(encoding="utf-8")
        actual = rows(text, cases)
        log = args.output / "class-init.log"
        ensure(log.is_file() and log.stat().st_size < 2 * 1024 * 1024 and len(list(args.output.glob("class-init.log*"))) == 1,
               "complete bounded initialization log")
        initialized = re.findall(r"Initializing '([^']+)'", log.read_text(encoding="utf-8"))
        ensure(all(name in initialized for name in ("ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt",
                   "wh", "bg", "ii", "ls", "sn", "eq", "ud", "ma", "java/lang/Math$RandomNumberGeneratorHolder")),
               "actual constructor chain and reached real Math Random holder")
        ensure(all(name not in initialized for name in (*base.TILES, "fd", "ji", "sj", "cv")) and
               not any(name.startswith(("org/lwjgl/", "org.lwjgl.")) for name in initialized),
               "no World/Engine/Font/CV/TileEntity/SDK active use")
        fixture = args.output / "frame-finite.json"
        fixture.write_text(json.dumps({"rows": actual}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", cases=len(cases), rows=len(actual), initialization_log_sha256=digest(log), fixture_sha256=digest(fixture))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
               report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "archive/tool/source/helper/input bytes unchanged")
    print("Original TileEntityRenderer frame: " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
