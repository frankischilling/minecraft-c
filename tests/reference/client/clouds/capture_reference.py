"""Observe unchanged cloud drawing with actual original GL and Tessellator storage."""
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

spec = importlib.util.spec_from_file_location("clouds_sdk_pins", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def observations(text):
    states = [row for row in text.splitlines() if row.startswith("CLOUD_STATE ")]
    geometry = [row for row in text.splitlines() if row.startswith("CLOUD_GEOMETRY ")]
    words = [row for row in text.splitlines() if row.startswith("CLOUD_WORDS ")]
    ids = [(dim, fancy, height, ana, eye, sample)
           for sample in range(2) for dim in range(3) for fancy in range(2)
           for height in range(3) if sample == 0 or height == 1
           for ana, eye in ((0, 0), (1, 0), (1, 1))]
    ensure(len(states) == len(geometry) == len(words) == len(ids) == 72, "cloud row count differs")
    for index, (state, batch, raw, identity) in enumerate(zip(states, geometry, words, ids)):
        cells = state.split()
        ensure(len(cells) == 41 and tuple(map(int, cells[1:8])) == (index, *identity),
               "cloud state identity/shape differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", c) for c in cells[8:12] + cells[24:40]),
               "cloud float state encoding differs")
        ensure(all(c in ("0", "1") for c in cells[12:18] + cells[19:24]) and cells[18] == "7425",
               "cloud flags or pool cursor differ")
        count = int(cells[40])
        ensure(count >= 0 and count % 8 == 0, "cloud actual uploaded word count differs")
        batch_cells = batch.split()
        ensure(len(batch_cells) == 5 and list(map(int, batch_cells[1:4])) ==
               [index, 7 if identity[1] else 6, count // 8], "cloud geometry identity differs")
        raw_cells = raw.split()
        ensure(raw_cells[:2] == ["CLOUD_WORDS", str(index)] and len(raw_cells) in (2, 3),
               "cloud raw geometry shape differs")
        payload = bytes.fromhex(raw_cells[2]) if len(raw_cells) == 3 else b""
        ensure(len(payload) == (count // 8) * int(batch_cells[2]) * 4 and
               hashlib.sha256(payload).hexdigest() == batch_cells[4], "cloud geometry digest differs")
    return states, geometry


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
              natives / "lwjgl64.dll", HERE / "CloudObservation.java", Path(__file__),
              HERE / "source-pins.json", HERE / "operation-map.json", SUPPORT / "capture.py",
              SUPPORT / "classfile.py", HERE.parent / "controller_tick/capture_reference.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "72 unchanged n.b(F) calls including real n.c(F) fancy delegation and Nether returns; genuine N construction, WorldInfo and Dimensions/managers/noise, prepared App/World/Settings/camera/Engine cached-hit operands. Actual post-call GL state and final NW batch payload only. Source g zero and e.position zero are explicit prepared initial state. No game constructor, all-batch geometry, cache-miss upload, weather or matched-frame pixel qualification."}
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
                           "-d", str(classes), str(HERE / "CloudObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"CloudObservation.class", "CloudObservation$1.class"}, "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.security.manager=allow", "-Dorg.lwjgl.librarypath=" + str(natives.resolve()),
                           "-classpath", classpath, "CloudObservation", str(args.client_jar.resolve()),
                           str(natives.resolve())], args.output, "original", 120, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original clouds: 72 rows; network=0; display=1; unchanged-methods=2" in text.splitlines(),
               "original completion receipt missing")
        states, geometry = observations(text)
        profiles = [line for line in text.splitlines() if line.startswith(("GL_VENDOR ", "GL_RENDERER ", "GL_VERSION "))]
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(profiles) == 3 and len(links) == 1 and Path(links[0]).resolve() ==
               (natives / "lwjgl64.dll").resolve(), "device/native profile differs")
        fixture = {"client_jar_sha256": sdk.JAR, "state_rows": states, "geometry_rows": geometry}
        (args.output / "draw.json").write_text(json.dumps(fixture, indent=2) + "\n", newline="\n")
        report.update(status="PASS", state_rows=72, geometry_rows=72, original=receipt,
                      native_links=links, gl_profile=profiles, fixture_sha256=digest(args.output / "draw.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.name: digest(p) for p in classes.glob("*.class")}
        same = report["input_before_sha256"] == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")
        ensure(same, "observer inputs or helpers changed")
    print("Original clouds: 72 state and final-batch geometry rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
