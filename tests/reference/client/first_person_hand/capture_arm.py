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

spec = importlib.util.spec_from_file_location("arm_sdk_pins", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def observations(text):
    parts = [row for row in text.splitlines() if row.startswith("ARM_PART ")]
    faces = [row for row in text.splitlines() if row.startswith("ARM_FACE ")]
    words = [row for row in text.splitlines() if row.startswith("ARM_WORDS ")]
    poses = [row for row in text.splitlines() if row.startswith("ARM_POSE ")]
    ensure(len(parts) == 2 and len(faces) == len(words) == 6, "arm row count differs")
    for index, row in enumerate(parts):
        cells = row.split()
        ensure(len(cells) == 30 and cells[1:6] == [str(index), "1", "1", str(index), "1"],
               "arm part identity or lazy-list state differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", c) for c in cells[7:]), "arm matrix/pose encoding differs")
    for index, (row, raw) in enumerate(zip(faces, words)):
        cells = row.split()
        ensure(len(cells) == 5 and cells[1:4] == [str(index), "6", str(4 * (index + 1))],
               "arm face identity/cursor differs")
        values = raw.split()
        ensure(values[:2] == ["ARM_WORDS", str(index)] and len(values) == 3, "arm face word shape differs")
        data = bytes.fromhex(values[2])
        ensure(len(data) == 6 * 7 * 4 and hashlib.sha256(data).hexdigest() == cells[4],
               "arm face payload hash differs")
    ensure(len(poses) == 16, "arm pose row count differs")
    for index, row in enumerate(poses):
        cells = row.split()
        ensure(len(cells) == 57 and cells[1:3] == [str(index), "0"] and
               all(re.fullmatch(r"[0-9a-f]{8}", c) for c in cells[3:]), "arm pose encoding differs")
    return parts, faces, poses


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
              natives / "lwjgl64.dll", HERE / "ArmObservation.java", Path(__file__),
              HERE / "source-pins.json", HERE / "operation-map.json", SUPPORT / "capture.py",
              SUPPORT / "classfile.py", HERE.parent / "controller_tick/capture_reference.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Genuine DS/FH/PS constructors, two unchanged DS.b calls including actual FH pose and PS.a lazy-list compilation/reuse, plus six independently invoked actual TZ.a final-batch payloads at scale .0625. Known source g zero/e.position zero and GL inputs. No full RA composition, skin lookup, all-Entity rendering or matched-frame pixels."}
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
                           "-d", str(classes), str(HERE / "ArmObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"ArmObservation.class", "ArmObservation$1.class"}, "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.security.manager=allow", "-Dorg.lwjgl.librarypath=" + str(natives.resolve()),
                           "-classpath", classpath, "ArmObservation", str(args.client_jar.resolve()),
                           str(natives.resolve())], args.output, "original", 120, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original arm: 2 part rows; 6 face rows; 16 pose rows; network=0; display=1" in text.splitlines(),
               "original completion receipt missing")
        parts, faces, poses = observations(text)
        profiles = [line for line in text.splitlines() if line.startswith(("GL_VENDOR ", "GL_RENDERER ", "GL_VERSION "))]
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(profiles) == 3 and len(links) == 1 and Path(links[0]).resolve() ==
               (natives / "lwjgl64.dll").resolve(), "device/native profile differs")
        fixture = {"client_jar_sha256": sdk.JAR, "part_rows": parts, "face_rows": faces, "pose_rows": poses}
        (args.output / "arm.json").write_text(json.dumps(fixture, indent=2) + "\n", newline="\n")
        report.update(status="PASS", part_rows=2, face_rows=6, pose_rows=16, original=receipt,
                      native_links=links, gl_profile=profiles, fixture_sha256=digest(args.output / "arm.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.name: digest(p) for p in classes.glob("*.class")}
        same = report["input_before_sha256"] == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")
        ensure(same, "observer inputs or helpers changed")
    print("Original arm: 2 part and 6 face rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
