"""Capture whole unchanged flat using genuine World/EW/Block/NW CPU operands."""
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


def rows(text):
    result = [line for line in text.splitlines() if line.startswith("FLAT ")]
    profiles = [line.split() for line in (HERE / "profiles.txt").read_text().splitlines()]
    ensure(len(result) == len(profiles) == 25, "exact flat row inventory")
    for i,(row,profile) in enumerate(zip(result,profiles)):
        cells=row.split()
        ensure(cells[:13] == ["FLAT",str(i),*profile] and cells[13] in ("-1","0","1") and
               cells[14] in ("NONE","NULL","INDEX") and cells[15] == "0",
               "flat row identity/result differs")
        ensure((cells[13]=="-1") == (cells[14]!="NONE"), "flat failure cannot be ordinary false")
        words=int(cells[68])
        ensure(len(cells)==69+words and all(re.fullmatch(r"[0-9a-f]{8}",v) for v in
               [*cells[16:43],*cells[44:56],cells[65],*cells[69:]]) and
               all(re.fullmatch(r"[0-9a-f]{16}",v) for v in cells[56:62]), "flat fields/vertex shape differs")
        ensure(words % 48 == 0 and cells[43] == "2730", "whole source faces and unchanged flags")
        ensure(cells[14] == ("NULL" if i in (20,21,22) else "NONE"), "source null reachability differs")
        ensure(words == 0 if i in (2,3,20,21) else words > 0, "occluded/failure output differs")
        if i == 22: ensure(words == 48, "material failure preserves the completed bottom face")
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
    java = Path(shutil.which(args.java) or "").resolve(strict=True)
    javac = Path(shutil.which(args.javac) or "").resolve(strict=True)
    ensure(java.parent.samefile(javac.parent), "matching JDK tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(p) == h for p,h in zip(libraries,LIBRARIES.values())),
           "unchanged original archive/SDK metadata")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"],m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"] if (m["name"],m["descriptor"]) in selected]
            ensure(actual == expected, "unchanged class/method: " + name)
    picking_path = HERE.parent / "picking/observe.py"
    authored_path = HERE.parents[2] / "tools/client/test_picking.py"
    source = HERE / "FlatObservation.java"
    inputs = [java,javac,jar,*libraries,source,HERE / "capture_reference.py",HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py",HERE / "profiles.txt",picking_path,authored_path,
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()):digest(p) for p in inputs}
    world = args.output / "world"
    module("ew_authored_world",picking_path).world(world)
    world_before = inventory(world)
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"):
        environment.pop(name,None)
    report = {"status":"FAIL","input_before_sha256":before,"world_before_sha256":world_before,
              "scope":"whole unchanged cv.b flat, real Block/EW/World/Chunk/NW, authored light/block/CV operands; CPU source vertices only, no draw/GL/full-terrain frame claim"}
    try:
        for label,tool in (("java",java),("javac",javac)):
            receipt = capture([str(tool),"-version"],args.output,label,10,environment)
            require_success(receipt)
            version = (args.output / receipt["stderr_file"]).read_text() + (args.output / receipt["stdout_file"]).read_text()
            ensure(re.search(r"(?:version \"|javac )17\.",version),"JDK17 required")
        classpath = os.pathsep.join(str(p.resolve()) for p in [classes,jar,*libraries])
        receipt = capture([str(javac),"-encoding","UTF-8","-cp",classpath,"-d",str(classes),str(source)],
                          args.output,"compile",30,environment)
        require_success(receipt)
        helper = inventory(classes)
        ensure(set(helper) == {"FlatObservation.class","FlatObservation$1.class"},"authored helper inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java),"-Xint","-Xverify:all","-Xmx256m","-Djava.awt.headless=true",
                           "-Djava.security.manager=allow","-cp",classpath,"FlatObservation",
                           str(jar),str(world.resolve()),str((HERE / "profiles.txt").resolve())],args.output,"original",100,environment,
                          args.output.resolve())
        report["original"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original flat: 25 whole-body rows; network=0; native=0; draw=0", "normal original completion")
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows":observed},indent=2)+"\n",encoding="utf-8",newline="\n")
        report.update(status="PASS",rows=25,fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()):digest(p) for p in inputs}
        report["world_after_sha256"] = inventory(world)
        report["helper_after_sha256"] = inventory(classes)
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or helper == report["helper_after_sha256"])
        before_files = {p:h for p,h in world_before.items() if p != "session.lock"}
        after_files = {p:h for p,h in report["world_after_sha256"].items() if p != "session.lock"}
        same = same and before_files == after_files
        if not same: report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
        ensure(same,"original/helper/runtime/World source inputs changed")
    print("Original flat: 25 whole-body rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
