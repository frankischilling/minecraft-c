"""Capture unchanged NW normal packing without a Display or native game link."""

import argparse
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


def rows(text, cases):
    found = [line for line in text.splitlines()
             if line.startswith(("NW_STATE ", "NW_VERTEX "))]
    tags = ["cold", "outside0", "outside1", "null_out", "begin",
            *(case[0] for case in cases), "begin_failure", "quad"]
    ensure(len(found) == len(tags) + 6, "complete normal/quad observation inventory")
    for tag, line in zip(tags, found):
        cells = line.split()
        ensure(len(cells) == 11 and cells[:2] == ["NW_STATE", tag] and
               all(value in ("0", "1") for value in cells[2:4] + cells[8:]) and
               re.fullmatch(r"[0-9a-f]{8}", cells[4]) and
               all(re.fullmatch(r"[0-9]+", value) for value in cells[5:8]),
               "actual NW normal state row " + tag)
    for index, line in enumerate(found[len(tags):]):
        cells = line.split()
        ensure(len(cells) == 4 and cells[:3] == ["NW_VERTEX", "quad", str(index)] and
               re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){7}", cells[3]),
               "all eight real words of each expanded vertex")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(args.java is not None and args.javac is not None, "matching JDK17 tools required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True)
                   for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tool directory")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == expected
           for path, expected in zip(libraries, LIBRARIES.values())), "archive and SDK pins")
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == JAR, "source archive authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "complete unchanged class and Code: " + name)
    cases = json.loads((HERE / "inputs.json").read_text(encoding="utf-8"))["cases"]
    ensure(len(cases) == 17 and len({case[0] for case in cases}) == len(cases),
           "complete distinct authored normal cases")
    args.output.mkdir(parents=True, exist_ok=False)
    tsv = args.output / "inputs.tsv"
    tsv.write_text("\n".join("\t".join(case) for case in cases) + "\n",
                   encoding="utf-8", newline="\n")
    inputs = [java, javac, jar, *libraries, tsv, HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "NormalObservation.java", HERE / "capture_reference.py",
              SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Whole unchanged NW.b(FFF), including two genuine stdout diagnostics, "
                       "null System.out failure, real begin/repeat failure and converted-quad "
                       "vertex bytes. Real NW/ge constructors; two declared stale normal words. "
                       "No Display, draw, original class replacement or native game link."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = ((args.output / receipt["stdout_file"]).read_text() +
                    (args.output / receipt["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        classpath = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath, "-d",
                           str(classes), str(HERE / "NormalObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(helper) == {"NormalObservation.class", "NormalObservation$1.class"},
               "authored helper inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx128m",
                           "-Djava.security.manager=allow", "-classpath", classpath,
                           "NormalObservation", str(jar), str(tsv.resolve())],
                          args.output, "original", 60, environment, args.output.resolve())
        report["capture"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        marker = "Original NW normals: complete; network=0; links=0"
        ensure(text.splitlines()[-1] == marker and text.splitlines().count("But..") == 2,
               "normal original completion and both exact source diagnostics")
        observed = rows(text, cases)
        ensure(all(line in observed or line in ("But..", marker) for line in text.splitlines()),
               "no undeclared original output")
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=len(observed), diagnostics=2,
                      fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        ensure(same, "original, SDK, tools, authored inputs and helper bytes unchanged")
    print("Original NW normals: 30 state/vertex rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
