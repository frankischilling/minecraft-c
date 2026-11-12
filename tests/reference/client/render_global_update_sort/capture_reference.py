"""Bounded authored JDK17 object-sort regression; no game JAR or assets."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success


def validate_rows(rows, manifest):
    ensure(len(rows) == len(manifest), "sort fixture row count differs")
    statuses = {0: 0, 1: 0, 2: 0}
    for row, case in zip(rows, manifest):
        cells = row.split()
        status, calls = map(int, cells[:2])
        count = case["count"]
        ensure(status in statuses and 0 <= calls <= count * 64 + 1 and
               len(cells) == 3 + count + calls * 3 and
               re.fullmatch(r"[0-9a-f]{16}", cells[2]), "sort row shape differs")
        identities = list(map(int, cells[3:3 + count]))
        ensure(all(0 <= identity < count for identity in identities), "sort array identity differs")
        if status == 0:
            ensure(sorted(identities) == list(range(count)), "successful sort lost an identity")
        trace = list(map(int, cells[3 + count:]))
        accumulator = 0xcbf29ce484222325
        for index in range(calls):
            left, right, order = trace[index * 3:index * 3 + 3]
            ensure(0 <= left < count and 0 <= right < count and order in (-1, 0, 1, 91),
                   "comparator trace value differs")
            if order == 91:
                ensure(status == 1 and index == calls - 1 and
                       calls == case["stop_at_comparison"], "comparator stop differs")
            for value in (left, right, order):
                accumulator = ((accumulator ^ (value & 0xffffffffffffffff)) *
                               1099511628211) & 0xffffffffffffffff
        ensure(f"{accumulator:016x}" == cells[2], "comparison trace digest differs")
        statuses[status] += 1
    ensure(all(statuses.values()), "success/stop/contract coverage missing")
    return statuses


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--native", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(value) or "").resolve(strict=True)
                   for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    pins = json.loads((HERE / "runtime-pins.json").read_text())
    for name in ("cases.txt", "cases.json", "expected.txt"):
        ensure(digest(HERE / name) == pins[name.replace(".", "_") + "_sha256"],
               "checked-in sort fixture changed: " + name)
    manifest = json.loads((HERE / "cases.json").read_text())["cases"]
    ensure(len(manifest) == pins["cases"], "case manifest extent differs")
    expected = (HERE / "expected.txt").read_text().splitlines()
    statuses = validate_rows(expected, manifest)
    inputs = [java, javac, HERE / "SortKernelObservation.java", HERE / "capture_reference.py",
              HERE / "runtime-pins.json", HERE / "cases.txt", HERE / "cases.json",
              HERE / "expected.txt", SUPPORT / "capture.py"]
    if args.native is not None:
        inputs.append(args.native.resolve(strict=True))
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Authored integer comparator controls on unchanged pinned JDK17 Arrays.sort. "
                       "Full comparison traces and array contents on success, callback interruption "
                       "and detected contract failure. No original game JAR, MD/World invocation, "
                       "Display, source compiler or generic JVM allocator parity claimed."}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for label, tool in (("java", java), ("javac", javac)):
            version = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(version)
            text = ((args.output / version["stdout_file"]).read_text() +
                    (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
            report[label + "_version"] = text
        compilation = capture([str(javac), "-encoding", "UTF-8", "-d", str(classes),
                               str(HERE / "SortKernelObservation.java")], args.output,
                              "compile", 30, environment)
        require_success(compilation, empty_stderr=True)
        helpers = {path.relative_to(classes).as_posix(): digest(path)
                   for path in classes.rglob("*.class")}
        ensure(set(helpers) == {"SortKernelObservation.class", "SortKernelObservation$Value.class",
                                "SortKernelObservation$Stop.class", "SortKernelObservation$Integers.class"},
               "unexpected sort observer classes")
        report["helper_before_sha256"] = helpers
        observed = capture([str(java), "-Xint", "-Xverify:all",
                            "-Djava.util.Arrays.useLegacyMergeSort=false", "-cp", str(classes.resolve()),
                            "SortKernelObservation", str((HERE / "cases.txt").resolve())],
                           args.output, "reference", 30, environment)
        require_success(observed, empty_stderr=True)
        lines = (args.output / observed["stdout_file"]).read_text().splitlines()
        header = [pins["profile"]] + ["CLASS " + name + " " + value
                                     for name, value in pins["bootstrap_class_sha256"].items()]
        ensure(lines[:3] == header, "bootstrap sort class/runtime profile differs")
        ensure(lines[-1] == f"Sort kernel fixture: {pins['cases']} cases",
               "reference completion receipt missing")
        actual = lines[3:-1]
        ensure(actual == expected, "JDK17 sort trace/state baseline differs")
        validate_rows(actual, manifest)
        if args.native is not None:
            native = capture([str(args.native.resolve()), "--observe", str((HERE / "cases.txt").resolve())],
                             args.output, "native", 30, environment)
            require_success(native, empty_stderr=True)
            native_rows = (args.output / native["stdout_file"]).read_text().splitlines()
            ensure(native_rows == actual + [lines[-1]], "native sort trace/state differs")
            report["native"] = native
        report.update(status="PASS", cases=pins["cases"], status_counts=statuses,
                      reference=observed, fixture_sha256=digest(HERE / "expected.txt"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path)
                                          for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or
            report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
        ensure(same, "sort observer inputs changed")
    print(f"JDK17 sort fixture: {pins['cases']} cases PASS; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
