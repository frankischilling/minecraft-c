"""Observe unchanged original rain tick on explicitly prepared source operands."""
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
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

spec = importlib.util.spec_from_file_location(
    "rain_original_sdk_pins", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
JAR, LIBRARIES = sdk.JAR, sdk.LIBRARIES


def rows(text):
    result = [row for row in text.splitlines() if row.startswith("RAIN ")]
    ensure(len(result) == 20, "rain observation row count differs")
    for row, identity in zip(result, [(f, n) for f in range(2) for n in range(10)]):
        ensure(re.fullmatch(r"RAIN [01] [0-9] (OK|FAIL) [01] [0-9a-f]{16} 17", row),
               "rain observation encoding differs")
        ensure(tuple(map(int, row.split()[1:3])) == identity, "rain row order differs")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java = Path(shutil.which("java") or "").resolve(strict=True)
    javac = Path(shutil.which("javac") or "").resolve(strict=True)
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 required")
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(digest(args.client_jar) == JAR, "original client JAR differs")
    ensure(all(digest(p) == pin for p, pin in zip(libraries, LIBRARIES.values())),
           "SDK metadata JAR differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            methods = {(m["name"], m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in methods]
            ensure(actual == expected, "original rain class/method differs: " + name)
    inputs = [java, javac, args.client_jar, *libraries, HERE / "RainObservation.java",
              HERE / "source-pins.json", HERE / "capture_reference.py",
              HERE.parent / "controller_tick/capture_reference.py",
              HERE.parent / "controller/classfile.py", HERE.parent / "controller/capture.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "20 original private PX.c calls on Object-only prepared operands; "
                       "no constructor, real Chunk, particle, playback or whole-tick qualification"}
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, env)
            require_success(receipt)
            version = ((args.output / receipt["stdout_file"]).read_text() +
                       (args.output / receipt["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
        classpath = os.pathsep.join(str(p.resolve()) for p in [classes, args.client_jar, *libraries])
        receipt = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                           "-d", str(classes), str(HERE / "RainObservation.java")],
                          args.output, "compile", 30, env)
        require_success(receipt)
        helpers = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helpers) == {"RainObservation.class", "RainObservation$1.class"},
               "unexpected observer class inventory")
        report["helper_before_sha256"] = helpers
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                           "-classpath", classpath, "RainObservation",
                           str(args.client_jar.resolve())], args.output, "original", 30, env,
                          args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] == "Original rain body: 20 rows; network=0; native=0",
               "original completion/guard receipt missing")
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=20, original=receipt)
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
    print("Original rain body: 20 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
