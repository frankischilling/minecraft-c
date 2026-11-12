"""Observe unchanged OS bodies; the prepared App constructor is outside scope."""
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
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def rows(text):
    result = [line for line in text.splitlines() if line.startswith(("CTICK ", "CRESET "))]
    expected = [(kind, str(mode), str(n)) for mode in range(3) for n in range(10)
                for kind in ("CTICK", "CRESET")]
    ensure(len(result) == 60, "tick/reset row count differs")
    for row, identity in zip(result, expected):
        cells = row.split()
        ensure(len(cells) == 7 and tuple(cells[:3]) == identity, "tick/reset row order differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[3:6]),
               "tick/reset float encoding differs")
        ensure(int(cells[6]) == (0 if identity[0] == "CRESET" else -2147483641),
               "tick/reset integer encoding differs")
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
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(digest(args.client_jar) == JAR, "original client JAR differs")
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "SDK metadata JAR differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == JAR, "source pin authority differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            ensure(inspect_class(archive.read(name.replace(".", "/") + ".class")) == pin,
                   "original class/method body differs: " + name)
    inputs = [java, javac, args.client_jar, *libraries, HERE / "ControllerTickObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "scope": "Unchanged original OS tick/reset on genuine OS/YO; App uses Object-only prepared kq and actual B operand. No App constructor, setup, live audio, device or whole-client qualification.",
              "input_before_sha256": before}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            versions.append(((args.output / receipt["stdout_file"]).read_bytes() +
                             (args.output / receipt["stderr_file"]).read_bytes()).decode())
        runtime = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 required")
        report["jdk"] = versions
        classpath = os.pathsep.join(str(p.resolve()) for p in [classes, args.client_jar, *libraries])
        compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath, "-d",
                            str(classes), str(HERE / "ControllerTickObservation.java")],
                           args.output, "compile", 30, environment)
        require_success(compiled)
        helper = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helper) == {"ControllerTickObservation.class", "ControllerTickObservation$1.class"},
               "unexpected helper class inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.awt.headless=true", "-Djava.security.manager=allow", "-classpath",
                           classpath, "ControllerTickObservation", str(args.client_jar.resolve())],
                          args.output, "original", 30, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure(text.splitlines()[-1] ==
               "Original controller tick/reset: 60 rows; network=0; native=0",
               "original completion/guard receipt missing")
        result = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": result}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=60, completed_ticks=10, null_prefixes=20, resets=30,
                      original=receipt, fixture_sha256=digest(args.output / "finite.json"))
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
        ensure(same, "source/runtime/helper inputs changed")
    print("Original controller tick/reset: 60 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
