"""Capture actual unchanged NW double-coordinate quad geometry and its lit pixels."""

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
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import JAR, LIBRARIES

BASE_SPEC = importlib.util.spec_from_file_location("lighting_raster_base_rows", HERE / "capture_reference.py")
ensure(BASE_SPEC is not None and BASE_SPEC.loader is not None, "sibling base row parser")
BASE_ROWS = importlib.util.module_from_spec(BASE_SPEC)
BASE_SPEC.loader.exec_module(BASE_ROWS)


def rows(text, case):
    ordinary = BASE_ROWS.rows(text, case)
    found = [line for line in text.splitlines() if line.startswith(("CONVERTED_WORDS ", "RASTER ", "RASTER_STATE "))]
    ensure(case["frames"] == 1 and len(found) == 3 and found[1:] == ordinary,
           "complete actual quad source-word/pixel/state rows")
    words = found[0].split()
    ensure(len(words) == 5 and words[:4] == ["CONVERTED_WORDS", case["name"], "6", "48"] and
           re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){47}", words[4]), "all 48 source vertex words")
    raw = words[4].split(",")
    ensure(raw[3*8+6] == "00000000" and raw[4*8+6] == "00000000", "actual untouched duplicate normal words")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--profiles", nargs="+", type=int,
                        help="capture only declared indices for diagnosis; omitted captures all profiles")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "declared pinned LWJGL native profile is Windows")
    ensure(args.java is not None and args.javac is not None, "Java and Javac required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           "unchanged original and SDK archive pins")
    metadata = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    with zipfile.ZipFile(jar) as archive:
        for name, expected in metadata["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"] if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "unchanged complete class and selected Code: " + name)
    native_archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(native_archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43", "native archive pin")
    cases = json.loads((HERE / "converted-inputs.json").read_text(encoding="utf-8"))["cases"]
    indices = list(range(len(cases))) if args.profiles is None else args.profiles
    ensure(indices and len(indices) == len(set(indices)) and all(0 <= index < len(cases) for index in indices),
           "unique declared profile indices")
    args.output.mkdir(parents=True, exist_ok=False)
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "lwjgl64.dll").write_bytes(archive.read("lwjgl64.dll"))
    classes = args.output / "classes"
    classes.mkdir()
    inputs = [java, javac, jar, *libraries, native_archive, natives / "lwjgl64.dll",
              HERE / "RasterObservation.java", HERE / "ConvertedRasterObservation.java", HERE / "converted-inputs.json", HERE / "source-pins.json",
              HERE / "capture_converted.py", HERE / "capture_reference.py", SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [], "selected_indices": indices,
              "complete_inventory": indices == list(range(len(cases))),
              "scope": "Actual unchanged original NW four double-coordinate vertices expand to six. All48 raw sourcewords, "
                       "full RGBA8 pixels and separate state. Authored lights and geometry; no whole game frame."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "RasterObservation.java"), str(HERE / "ConvertedRasterObservation.java")], args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(report["helper_before_sha256"]) == {"RasterObservation.class", "RasterObservation$1.class", "ConvertedRasterObservation.class", "ConvertedRasterObservation$1.class"}, "authored helper inventory")
        observed = []
        for index in indices:
            case = cases[index]
            receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                               "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-cp", cp,
                               "ConvertedRasterObservation", str(jar), str(natives.resolve()), str(index)],
                              args.output, "original" + str(index), 60, environment, args.output.resolve())
            profile = {"name": case["name"], "index": index, "capture": receipt}
            report["profiles"].append(profile)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text()
            ensure("Original converted raster: " + case["name"] + "; cleanup-complete; network=0" in text.splitlines(), "checked context cleanup and zero networking")
            links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
            ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(), "only pinned native link")
            profile["driver"] = [line for line in text.splitlines() if line.startswith("RASTER_DRIVER ")]
            profile["format"] = [line for line in text.splitlines() if line.startswith("RASTER_FORMAT ")]
            ensure(len(profile["driver"]) == 1 and len(profile["format"]) == 1, "actual driver/framebuffer receipts")
            actual = rows(text, case)
            probe = bytes.fromhex(actual[1].split()[3])
            profile.update(status="PASS", rows=len(actual),
                           probe_rgba=list(probe[(47 * 64 + 16) * 4:(47 * 64 + 16) * 4 + 4]))
            observed.extend(actual)
        ensure(len(observed) == sum(cases[index]["frames"] * 3 for index in indices), "all selected whole frames and state")
        (args.output / "finite.json").write_text(json.dumps({"indices": indices, "rows": observed}, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=len(observed), frames=len(observed) // 3,
                      fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "archive/native/runtime/helper/input bytes unchanged")
    print("Original converted lighting raster: " + str(len(indices)) + " profiles, " + str(report["frames"]) +
          " exact RGBA8 frames; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
