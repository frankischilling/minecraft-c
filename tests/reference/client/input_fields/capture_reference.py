"""Capture finite observations from the pinned, unchanged InventoryPlayer body."""
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
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
SELECTED = (-28, -9, -1, 0, 1, 7, 8, 9, 28)
WHEELS = (-2147483648, -120, -1, 0, 1, 120, 2147483647)


def rows(text):
    result = [line for line in text.splitlines() if line.startswith("SCROLL ")]
    ensure(len(result) == 63, "selection row count differs")
    for row, identity in zip(result, [(s, w) for s in SELECTED for w in WHEELS]):
        ensure(re.fullmatch(r"SCROLL -?[0-9]+ -?[0-9]+ [0-8]", row),
               "selection row encoding differs")
        ensure(tuple(map(int, row.split()[1:3])) == identity, "selection row order differs")
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
            actual = inspect_class(archive.read(name.replace(".", "/") + ".class"))
            methods = {(m["name"], m["descriptor"]) for m in pin["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in methods]
            ensure(actual == pin, "original class/method differs: " + name)
    inputs = [java, javac, args.client_jar, HERE / "SelectionObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "63 original ix.b(I)V calls on genuine InventoryPlayer with null Player. "
                       "No Minecraft.k execution, settings arithmetic observation or playable-client claim."}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for name, tool in (("java", java), ("javac", javac)):
            version = capture([str(tool), "-version"], args.output, name, 10, environment)
            require_success(version)
            text = ((args.output / version["stdout_file"]).read_text() +
                    (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
            report[name + "_version"] = text
        classpath = os.pathsep.join(str(p.resolve()) for p in (classes, args.client_jar))
        compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                            "-d", str(classes), str(HERE / "SelectionObservation.java")],
                           args.output, "compile", 30, environment)
        require_success(compiled, empty_stderr=True)
        helper = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helper) == {"SelectionObservation.class", "SelectionObservation$1.class"},
               "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx128m",
                           "-Djava.awt.headless=true", "-Djava.security.manager=allow", "-classpath",
                           classpath, "SelectionObservation", str(args.client_jar.resolve())],
                          args.output, "original", 30, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original selection: 63 rows; network=0; native=0",
               "original completion/guard receipt missing")
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=63, original=receipt,
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
    print("Original selection: 63 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
