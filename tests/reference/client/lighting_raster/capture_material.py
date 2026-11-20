"""Observe actual legacy GL material tracking before qualifying its native bridge."""

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
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import LIBRARIES

NAMES = ["initial", "ambient_enabled", "tracked_ambient_direct_write",
         "ambient_tracked_ambient_and_diffuse_write", "next_color", "diffuse_selected",
         "back_emission_selected", "tracked_emission_direct_write", "both_specular_selected",
         "both_ambient_diffuse_selected", "tracking_disabled", "disabled_next_color",
         "disabled_direct_write", "tracking_reenabled", "untracked_shininess_write",
         "untracked_out_of_range_emission"]


def rows(text):
    found = [line for line in text.splitlines() if line.startswith("MATERIAL ")]
    ensure(len(found) == len(NAMES), "all actual material tracking observations")
    for line, name in zip(found, NAMES):
        cells = line.split()
        ensure(len(cells) == 16 and cells[:2] == ["MATERIAL", name], "material state row shape")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){3}", value)
                   for value in [*cells[2:10], cells[12]]), "all front/back material and current-color components")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[10:12]) and
               cells[13] in ("0", "1") and all(value.isdecimal() for value in cells[14:]),
               "shininess and actual tracking selectors")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "pinned native profile is Windows")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())), "SDK archive pins")
    archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43", "native archive pin")
    args.output.mkdir(parents=True, exist_ok=False)
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / "lwjgl64.dll").write_bytes(source.read("lwjgl64.dll"))
    classes = args.output / "classes"
    classes.mkdir()
    inputs = [java, javac, *libraries, archive, natives / "lwjgl64.dll", HERE / "MaterialObservation.java",
              HERE / "capture_material.py", SUPPORT / "capture.py", HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Authored legacy GL material controls using unchanged SDK wrappers. No Minecraft body, assets, geometry or pixels."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "MaterialObservation.java")], args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(report["helper_before_sha256"]) == {"MaterialObservation.class", "MaterialObservation$1.class"}, "helper inventory")
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                           "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-cp", cp,
                           "MaterialObservation", str(natives.resolve())], args.output, "original", 60,
                          environment, args.output.resolve())
        report["capture"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original material controls: 16 rows; cleanup-complete; network=0" in text.splitlines(), "checked normal completion")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(), "only pinned native link")
        report["driver"] = [line for line in text.splitlines() if line.startswith("MATERIAL_DRIVER ")]
        ensure(len(report["driver"]) == 1, "actual driver receipt")
        observed = rows(text)
        (args.output / "material.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=16, fixture_sha256=digest(args.output / "material.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "runtime/native/SDK/helper/input bytes unchanged")
    print("Original material controls: 16 rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
