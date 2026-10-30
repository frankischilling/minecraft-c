"""Capture unchanged LL null-World callbacks and null-Living frame prefixes only."""
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
spec = importlib.util.spec_from_file_location("tile_world_frozen_capture_support", HERE / "capture_reference.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
ensure, digest, capture, require_success = base.ensure, base.digest, base.capture, base.require_success
PROFILES = ("callbacks", "frame_null_cold")
FLOAT_INPUTS = ["3f800001", "bf400002"]
DOUBLE_INPUTS = ["3ff4000000000001", "c004000000000002", "400e000000000003"]
FIELDS = (["c", "d", "e", "f", "a", "b", "g", "h", "i", "j", "k", "l", "m"] +
          list("nopqrstuvwxyzABCDEFGHIJKLMNO") + list("PQRSTUVWXYZ") + ["aa"] +
          ["a" + c for c in "bcdefghijklm"])
TAGS = {"TILE_WORLD_CASE", "TILE_WORLD_LL", "TILE_WORLD_JE", "TILE_WORLD_CHILDREN", "TILE_WORLD_CV_ID",
        "TILE_WORLD_FIELD", "TILE_WORLD_FRAME_NULL", "TILE_WORLD_FRAME_ID", "TILE_WORLD_DONE"}


def rows(text, profile):
    lines = text.splitlines()
    result = [line for line in lines if line.split(" ", 1)[0] in TAGS]
    ensure(len(FIELDS) == len(set(FIELDS)) == 65, "complete declared field schema")
    expected_count = 232 if profile == "callbacks" else 19
    ensure(len(result) == expected_count and result[0] == "TILE_WORLD_CASE " + profile and
           result[-1] == f"TILE_WORLD_DONE {profile} {3 if profile == 'callbacks' else 0} 1",
           "complete bounded profile inventory")
    ensure(lines[-1] == f"Original TileEntityRenderer World: {profile}; rows={len(result)}; network=0; native=0; writes=0",
           "normal return and zero external-work guard receipt")
    stages = ["initial"] + (["world0", "world1", "world2"] if profile == "callbacks" else []) + ["before_frame", "after_frame"]
    ll = [line.split() for line in result if line.startswith("TILE_WORLD_LL ")]
    ensure([row[1] for row in ll] == stages and all(len(row) == 17 for row in ll), "all exact LL source-field snapshots")
    for row in ll:
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", v) for v in row[6:8]) and
               all(re.fullmatch(r"[0-9a-f]{16}", v) for v in row[8:14]), "raw LL float/double encodings")
    ensure([(row.split()[1], row.split()[2]) for row in result if row.startswith("TILE_WORLD_JE ")] ==
           [(stage, key) for stage in stages for key in ("yk", "cy", "uk")], "all unchanged actual JE identities/backpointers")
    ensure([row.split()[1] for row in result if row.startswith("TILE_WORLD_CHILDREN ")] == stages,
           "all actual PO/AG/HY child snapshots")
    fields = [line.split() for line in result if line.startswith("TILE_WORLD_FIELD ")]
    callbacks = 3 if profile == "callbacks" else 0
    ensure([(row[1], row[2]) for row in fields] == [(str(step), name) for step in range(callbacks) for name in FIELDS],
           "every declared CV field after every callback, no omitted defaults")
    for row in fields:
        ensure(len(row) == 6 and row[3] in ("S", "I"), "actual CV field/declaring-static schema")
        ensure((row[4] == "F" and re.fullmatch(r"[0-9a-f]{8}", row[5])) or
               (row[4] == "I" and re.fullmatch(r"-?[0-9]+", row[5])) or
               (row[4] == "Z" and row[5] in ("0", "1")) or
               (row[4] == "Lxp;" and row[5] == "null"), "actual CV field descriptor/value encoding")
    ensure([row.split()[1] for row in result if row.startswith("TILE_WORLD_CV_ID ")] == [str(i) for i in range(callbacks)],
           "all immediate-previous/all-prior CV identity controls")
    failures = [line.split() for line in result if line.startswith("TILE_WORLD_FRAME_NULL ")]
    ensure(len(failures) == 1 and len(failures[0]) == 5 and
           failures[0][1:4] == ["java.lang.NullPointerException", "ll", "a"] and
           re.fullmatch(r"-?[0-9]+", failures[0][4]), "unmodified actual frame exception/stack line")
    ensure(sum(line.startswith("TILE_WORLD_FRAME_ID ") for line in result) == 1 and
           sum(line.startswith("TILE_WORLD_EXCEPTION ") for line in lines) == 1, "same-World skip and raw exception diagnostics")
    orders = [line.split() for line in lines if line.startswith("TILE_WORLD_ORDER ")]
    ensure(len(orders) == callbacks * 3 and [(row[1], row[2]) for row in orders] ==
           [(str(step), str(index)) for step in range(callbacks) for index in range(3)], "complete actual unsorted Map values order")
    ensure(all(not line.startswith("TILE_WORLD_") or line.split(" ", 1)[0] in
               TAGS | {"TILE_WORLD_EXCEPTION", "TILE_WORLD_ORDER"} for line in lines), "no ignored probe row kind")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(args.java and args.javac, "matching JDK17 Java/Javac required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "same JDK17 bin directory")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in base.LIBRARIES]
    ensure(digest(jar) == base.JAR and all(digest(path) == pin for path, pin in zip(libraries, base.LIBRARIES.values())),
           "untouched client JAR and pinned SDK type archives")
    pins = json.loads((HERE / "world-source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == base.JAR, "same original bytecode authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = base.classfile.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"] if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "whole class and selected Code/attribute authority: " + name)
    args.output.mkdir(parents=True, exist_ok=False)
    inputs = [java, javac, jar, *libraries, HERE / "TileEntityRendererWorldObservation.java", HERE / "capture_world.py",
              HERE / "world-source-pins.json", HERE / "WORLD_README.md", HERE / "capture_reference.py",
              HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Unchanged LL.a(fd) null-World callbacks (three actual HY CV(XP=null) constructors), "
                       "and all-null-reference LL frame-bind failure after field stores, same-World skip. "
                       "No World/TileEntity/Entity/Engine/Font instances, interpolation, resources, graphics or draw credit.",
              "declared_scalar_inputs": {"h_i_float_bits": FLOAT_INPUTS, "j_k_l_double_bits": DOUBLE_INPUTS,
                                         "partial_float_bits": "3f000000", "reference_operands": "all null"},
              "frame_failure_source_site": {"class": "ll", "method": "a", "descriptor": "(Lfd;Lji;Lsj;Lls;F)V",
                                            "pc": 32, "authority": "pinned original Code, separate from raw observed Java stack line"}}
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
                           str(HERE / "TileEntityRendererWorldObservation.java")], args.output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"TileEntityRendererWorldObservation.class", "TileEntityRendererWorldObservation$1.class"},
               "only the two authored helper classes; original game bodies unchanged")
        report["helper_before_sha256"] = helper
        observed = []
        for profile in PROFILES:
            work = args.output / profile
            work.mkdir()
            receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                               "-Djava.security.manager=allow", "-Xlog:class+init=info:file=class-init.log::filesize=2m,filecount=1",
                               "-cp", cp, "TileEntityRendererWorldObservation", str(jar), profile],
                              args.output, profile, 60, environment, work.resolve())
            item = {"name": profile, "capture": receipt}
            report["profiles"].append(item)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text(encoding="utf-8")
            actual = rows(text, profile)
            log = work / "class-init.log"
            ensure(log.is_file() and log.stat().st_size < 2 * 1024 * 1024 and len(list(work.glob("class-init.log*"))) == 1,
                   "complete bounded class initialization log")
            initialized = re.findall(r"Initializing '([^']+)'", log.read_text(encoding="utf-8"))
            ensure(all(name not in initialized for name in (*base.TILES, "fd", "ls", "ji", "sj")) and
                   not any(name.startswith(("org/lwjgl/", "org.lwjgl.")) for name in initialized),
                   "no instance-side class, OW/Piston or SDK active initialization")
            ensure(("cv" in initialized) == (profile == "callbacks") and all(name in initialized for name in
                   ("ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt")),
                   "actual constructor graph, and CV active use only when World callback is reached")
            item.update(status="PASS", rows=len(actual), initialization_log_sha256=digest(log),
                        diagnostics=[line for line in text.splitlines() if line.startswith(("TILE_WORLD_EXCEPTION ", "TILE_WORLD_ORDER "))])
            observed.extend(actual)
        fixture = args.output / "world-finite.json"
        fixture.write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=len(observed), fixture_sha256=digest(fixture))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
               report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "all archive/tool/source/helper bytes unchanged")
    print("Original TileEntityRenderer World null scope: " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
