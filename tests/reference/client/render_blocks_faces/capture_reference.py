"""Observe unchanged original CV faces and NW float color on prepared fields."""
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
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def rows(text):
    result = [line for line in text.splitlines() if line.startswith(("FACE ", "FCOLOR "))]
    ensure(len(result) == 1680, "CV face/color row count differs")
    for index, row in enumerate(result[:1656]):
        cells = row.split()
        ensure(len(cells) == 16 and cells[:2] == ["FACE", str(index)], "CV face row identity differs")
        ensure(cells[14] in ("NONE", "NPE", "AIOOBE"), "CV face exception differs")
    for index, row in enumerate(result[1656:]):
        cells = row.split()
        ensure(len(cells) == 7 and cells[:2] == ["FCOLOR", str(index)] and cells[-1] == "NONE",
               "NW float-color row identity differs")
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
    java, javac = [Path(shutil.which(p) or "").resolve(strict=True)
                   for p in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    ensure(digest(args.client_jar) == JAR, "original client JAR differs")
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "SDK metadata differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == JAR, "source pin authority differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in pin["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in selected]
            ensure(actual == pin, "original class/method differs: " + name)
    inputs = [java, javac, args.client_jar, *libraries, HERE / "FaceObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Unchanged six CV face bodies and NW float-color leaf on actual source Block/CV/NW "
                       "identities with prepared bounds, rotations, flip, AO corners, texture overrides, "
                       "NULL/short-array failure prefixes and IEEE color inputs. No DK/World/frame or GPU claim."}


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
        classpath = os.pathsep.join(str(p.resolve()) for p in (classes, args.client_jar, *libraries))
        compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                            "-d", str(classes), str(HERE / "FaceObservation.java")],
                           args.output, "compile", 30, environment)
        require_success(compiled, empty_stderr=True)
        helper = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helper) == {"FaceObservation.class", "FaceObservation$1.class"},
               "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.security.manager=allow", "-Djava.awt.headless=true", "-classpath",
                           classpath, "FaceObservation", str(args.client_jar.resolve())],
                          args.output, "original", 30, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original CV faces: 1656 rows; 24 colors; network=0; native=0" in text.splitlines(),
               "original completion/guard receipt missing")
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=1680, original=receipt,
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
    print("Original CV faces: 1680 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
