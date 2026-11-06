"""Observe unchanged outer first-person hand matrix and source failure prefixes."""
import argparse
import hashlib
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
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

spec = importlib.util.spec_from_file_location("hand_sdk_pins", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def observations(text):
    rows = [row for row in text.splitlines() if row.startswith("HAND_OUTER ")]
    ids = [(case, partial, bob, ana, eye) for case in range(4) for partial in range(2)
           for bob in range(2) for ana in range(2) for eye in range(2)]
    ensure(len(rows) == len(ids) == 64, "hand row count differs")
    for index, (row, identity) in enumerate(zip(rows, ids)):
        cells = row.split()
        ensure(len(cells) == 42 and tuple(map(int, cells[1:7])) == (index, *identity),
               "hand row identity/shape differs")
        ensure(cells[7] == ("OK" if identity[0] < 2 else "NULL") and
               cells[8] == ("2" if identity[0] == 3 else "1") and cells[9] == "5888",
               "hand return/failure or stack/mode differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", c) for c in cells[10:]), "hand matrix encoding differs")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "this reference device profile requires Windows")
    args.output.mkdir(parents=True, exist_ok=False)
    java = Path(shutil.which("java")).resolve(strict=True)
    javac = Path(shutil.which("javac")).resolve(strict=True)
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools required")
    ensure(digest(args.client_jar) == sdk.JAR, "original client archive differs")
    libraries = [args.library_root / name for name in sdk.LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, sdk.LIBRARIES.values())),
           "SDK metadata differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == sdk.JAR, "source pin authority differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in pin["methods"]}
            actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
            ensure(actual == pin, "original class/method differs: " + name)
    native_archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(native_archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43",
           "SDK native archive differs")
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "lwjgl64.dll").write_bytes(archive.read("lwjgl64.dll"))
    inputs = [java, javac, args.client_jar, *libraries, native_archive,
              natives / "lwjgl64.dll", HERE / "HandObservation.java", Path(__file__),
              HERE / "source-pins.json", HERE / "operation-map.json", SUPPORT / "capture.py",
              SUPPORT / "classfile.py", HERE.parent / "controller_tick/capture_reference.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "64 unchanged px.b(FI) calls with prepared actual PX/App/Settings/Player fields and actual hurt/bob/GL methods. Third-person/sleeping returns and explicit null RA failures; hide-GUI skips only the arm. No constructors, held-item body, overlays, model geometry or whole-frame qualification."}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
            report[label + "_version"] = version
        classpath = os.pathsep.join(str(p.resolve()) for p in [classes, args.client_jar, *libraries])
        receipt = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                           "-d", str(classes), str(HERE / "HandObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"HandObservation.class", "HandObservation$1.class"}, "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.security.manager=allow", "-Dorg.lwjgl.librarypath=" + str(natives.resolve()),
                           "-classpath", classpath, "HandObservation", str(args.client_jar.resolve()),
                           str(natives.resolve())], args.output, "original", 120, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original first-person hand: 64 outer rows; network=0; display=1" in text.splitlines(),
               "original completion receipt missing")
        rows = observations(text)
        profiles = [line for line in text.splitlines() if line.startswith(("GL_VENDOR ", "GL_RENDERER ", "GL_VERSION "))]
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(profiles) == 3 and len(links) == 1 and Path(links[0]).resolve() ==
               (natives / "lwjgl64.dll").resolve(), "device/native profile differs")
        fixture = {"client_jar_sha256": sdk.JAR, "rows": rows}
        (args.output / "outer.json").write_text(json.dumps(fixture, indent=2) + "\n", newline="\n")
        report.update(status="PASS", rows=64, original=receipt,
                      native_links=links, gl_profile=profiles, fixture_sha256=digest(args.output / "outer.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.name: digest(p) for p in classes.glob("*.class")}
        same = report["input_before_sha256"] == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")
        ensure(same, "observer inputs or helpers changed")
    print("Original first-person hand: 64 outer rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
