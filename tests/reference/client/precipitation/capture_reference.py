"""Capture finite unchanged-original precipitation prefixes without native loading."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

spec = importlib.util.spec_from_file_location("precipitation_sdk", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java = Path(shutil.which("java")).resolve(strict=True)
    javac = Path(shutil.which("javac")).resolve(strict=True)
    ensure(java.parent == javac.parent, "matching JDK tools required")
    libraries = [args.library_root / name for name in sdk.LIBRARIES]
    ensure(digest(args.client_jar) == sdk.JAR, "original client archive differs")
    ensure(all(digest(path) == pin for path, pin in zip(libraries, sdk.LIBRARIES.values())),
           "original SDK library differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            ensure(actual == pin, "original class differs: " + name)
    inputs = [java, javac, args.client_jar, *libraries, HERE / "PrecipitationObservation.java",
              HERE / "source-pins.json", Path(__file__), HERE.parent / "controller/classfile.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller_tick/capture_reference.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    classpath = os.pathsep.join(str(p.resolve()) for p in [classes, args.client_jar, *libraries])
    result = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath, "-d",
                      str(classes), str(HERE / "PrecipitationObservation.java")],
                     args.output, "compile", 30, env)
    require_success(result)
    helper_before = {p.name: digest(p) for p in classes.glob("*.class")}
    result = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                      "-Djava.security.manager=allow", "-classpath", classpath,
                      "PrecipitationObservation", str(args.client_jar.resolve())],
                     args.output, "original", 30, env, args.output.resolve())
    require_success(result)
    rows = [r for r in (args.output / result["stdout_file"]).read_text().splitlines()
            if r.startswith("PRECIP_PREFIX ")]
    ensure(len(rows) == 100, "original prefix row count differs")
    after = {str(p.resolve()): digest(p) for p in inputs}
    ensure(before == after, "original inputs changed")
    ensure(helper_before == {p.name: digest(p) for p in classes.glob("*.class")}, "helpers changed")
    report = {"status": "PASS", "jar_sha256": sdk.JAR, "rows": rows,
              "input_before_sha256": before, "input_after_sha256": after,
              "helper_sha256": helper_before, "original": result,
              "scope": "100 prepared dry/null-camera original c(F) prefixes; no rendered-weather comparison"}
    (args.output / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    print("100 original precipitation prefixes captured: " + str(args.output / "run.json"))


if __name__ == "__main__":
    main()
