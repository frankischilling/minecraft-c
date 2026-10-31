"""Capture JDK math vectors for a bytecode-pinned original h(F) expression."""
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


def rows(text):
    observed = [row for row in text.splitlines() if row.startswith("BLEND ")]
    ensure(len(observed) == 8, "fog blend row count differs")
    distances = [-2147483648, 0, 1, 2, 3, 4, 5, 2147483647]
    for distance, row in zip(distances, observed):
        ensure(re.fullmatch(r"BLEND -?[0-9]+ (NAN|[0-9a-f]{8})", row) and
               int(row.split()[1]) == distance, "fog blend row shape/order differs")
    return observed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(tool) or "").resolve(strict=True)
                   for tool in ("java", "javac")]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(digest(args.client_jar) == pins["client_jar_sha256"], "original JAR differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            methods = {(m["name"], m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in methods]
            ensure(actual == expected, "original World-frame method differs")
    inputs = [java, javac, args.client_jar, HERE / "source-pins.json", HERE / "capture_math.py",
              HERE / "FogBlendObservation.java", HERE.parent / "controller/capture.py",
              HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "JDK Math.pow and Java narrowing for the bytecode-derived h(F) distance "
                       "expression; the original px method is pinned but not executed"}
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS"):
        env.pop(name, None)
    try:
        for label, executable in (("java", java), ("javac", javac)):
            version = capture([str(executable), "-version"], args.output, label, 10, env)
            require_success(version)
            text = ((args.output / version["stdout_file"]).read_text() +
                    (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        classes = args.output / "classes"
        classes.mkdir()
        compiled = capture([str(javac), "-encoding", "UTF-8", "-d", str(classes),
                            str(HERE / "FogBlendObservation.java")], args.output, "compile", 30, env)
        require_success(compiled, empty_stderr=True)
        helper = classes / "FogBlendObservation.class"
        report["helper_before_sha256"] = digest(helper)
        result = capture([str(java), "-Xint", "-Xverify:all", "-Xmx64m", "-classpath",
                          str(classes.resolve()), "FogBlendObservation"], args.output, "jdk", 15,
                         env, args.output.resolve())
        require_success(result, empty_stderr=True)
        vectors = rows((args.output / result["stdout_file"]).read_text())
        (args.output / "math-vectors.json").write_text(json.dumps({"rows": vectors}, indent=2) + "\n",
                                                      encoding="utf-8", newline="\n")
        report["helper_after_sha256"] = digest(helper)
        ensure(report["helper_before_sha256"] == report["helper_after_sha256"], "helper changed")
        report.update(status="PASS", rows=8, jdk=result)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
        ensure(same, "math capture inputs changed")
    print("Fog blend: 8 JDK vectors; original PX methods pinned, not executed")


if __name__ == "__main__":
    main()
