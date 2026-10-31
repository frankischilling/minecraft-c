"""Capture closed CPU prefixes of untouched private n.a(II)V."""
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


def rows(text):
    found = [line for line in text.splitlines() if line.startswith("POLL ")]
    cases = json.loads((HERE / "inputs.json").read_text())["cases"]
    ensure(len(found) == len(cases) == 14, "all14 closed poll rows")
    for line, case in zip(found, cases):
        cells = line.split()
        ensure(len(cells) == 6 and cells[1] == case["name"] and cells[2] in ("OK", "NULL", "INDEX"),
               "poll row identity/result")
        ensure(cells[3] == "00,11,10", "no source query/flag store reached")
        ensure(cells[4:] == (["1", "1"] if case["buffer"] else ["-1", "-1"]), "unreached buffer unchanged")
    return found


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
    libraries = [args.library_root / name for name in LIBRARIES]
    jar = args.client_jar.resolve(strict=True)
    ensure(digest(jar) == JAR and all(digest(p) == h for p, h in zip(libraries, LIBRARIES.values())),
           "unchanged archive and SDK metadata")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
            ensure(actual == expected, "unchanged class/method " + name)
    inputs = [java, javac, jar, *libraries, HERE / "PollObservation.java", HERE / "source-pins.json",
              HERE / "inputs.json", HERE / "capture_reference.py", SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    cases = json.loads((HERE / "inputs.json").read_text())["cases"]
    tsv = args.output / "inputs.tsv"
    tsv.write_text("\n".join("\t".join([c["name"], str(c["start"]), str(c["end"]), str(int(c["buffer"])),
                   "N" if c["array"] is None else ",".join(map(str, c["array"]))]) for c in cases) + "\n", encoding="utf-8", newline="\n")
    before[str(tsv.resolve())] = digest(tsv)
    inputs.append(tsv)
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "14 untouched private N query-poll CPU prefixes. Genuine Object-only N/DK and actual direct buffers. No GL method reached, game replacement or source constructor qualification."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(p.resolve()) for p in [classes, jar, *libraries])
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), str(HERE / "PollObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"PollObservation.class", "PollObservation$1.class"}, "authored helper inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow", "-cp", cp,
                           "PollObservation", str(jar), str(tsv.resolve())], args.output, "original", 40, environment,
                          args.output.resolve())
        report["original"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original poll: 14 closed CPU rows; network=0; native=0", "normal original completion")
        fixture = {"rows": rows(text)}
        (args.output / "finite.json").write_text(json.dumps(fixture, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=14, fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.name: digest(p) for p in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or helper == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "original/helper/runtime/source input hashes unchanged")
    print("Original poll: 14 rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
