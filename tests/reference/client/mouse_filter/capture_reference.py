"""Observe finite histories through unchanged original mouse-filter arithmetic."""
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

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def rows(text):
    result = [line for line in text.splitlines() if line.startswith("FILTER ")]
    ensure(len(result) == 96, "mouse-filter row count differs")
    for row, identity in zip(result, [(m, n) for m in range(8) for n in range(12)]):
        ensure(re.fullmatch(r"FILTER [0-7] (?:[0-9]|1[01])(?: [0-9a-f]{8}){4}", row),
               "mouse-filter row encoding differs")
        ensure(tuple(map(int, row.split()[1:3])) == identity, "mouse-filter row order differs")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(p) or "").resolve(strict=True)
                   for p in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    ensure(digest(args.client_jar) == JAR, "original client JAR differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == JAR, "source pin authority differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in pin["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in selected]
            ensure(actual == pin, "original class/method differs: " + name)
    inputs = [java, javac, args.client_jar, HERE / "MouseFilterObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "96 unchanged cu.a(FF)F observations across eight authored field histories "
                       "on genuine constructed cu. Selected finite/special arithmetic and stored "
                       "bits only; no settings, SDL input, renderer body or whole-frame claim."}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for label, tool in (("java", java), ("javac", javac)):
            version = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(version)
            version_text = ((args.output / version["stdout_file"]).read_text() +
                            (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', version_text), "JDK17 required")
            report[label + "_version"] = version_text
        classpath = os.pathsep.join(str(p.resolve()) for p in (classes, args.client_jar))
        compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                            "-d", str(classes), str(HERE / "MouseFilterObservation.java")],
                           args.output, "compile", 30, environment)
        require_success(compiled, empty_stderr=True)
        helper = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helper) == {"MouseFilterObservation.class", "MouseFilterObservation$1.class"},
               "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx128m",
                           "-Djava.awt.headless=true", "-Djava.security.manager=allow", "-classpath",
                           classpath, "MouseFilterObservation", str(args.client_jar.resolve())],
                          args.output, "original", 30, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original mouse filter: 96 rows; network=0; native=0",
               "original completion/guard receipt missing")
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=96, original=receipt,
                      fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.relative_to(classes).as_posix(): digest(p)
                                          for p in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or
            report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
        ensure(same, "observer inputs changed")
    print("Original mouse filter: 96 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
