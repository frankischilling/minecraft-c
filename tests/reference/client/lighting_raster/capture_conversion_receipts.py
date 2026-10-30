"""Exhaustive successor normal-array type/stride receipts, decode and color precision."""

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
from capture_prefix import JAR, LIBRARIES


def rows(text):
    from capture_conversion import rows as precision_rows
    precision = precision_rows(text)
    decoded = [line for line in text.splitlines() if line.startswith("NORMAL_DECODE ")]
    ensure(len(decoded) == 256, "all 256 signed-byte values observed")
    for line, value in zip(decoded, range(-128,128)):
        cells = line.split()
        ensure(len(cells) == 5 and cells[:2] == ["NORMAL_DECODE", str(value)] and
               all(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){2}", word) for word in cells[2:]),
               "actual current/array/formula normal components")
    selected = {int(line.split()[1]): line for line in precision}
    expected = []
    for value, line in zip(range(-128,128),decoded):
        expected.append(line)
        if value in selected: expected.append(selected[value])
    found = [line for line in text.splitlines() if line.startswith(("NORMAL_DECODE ", "NORMAL_CONVERSION "))]
    ensure(found == expected, "complete ordered decode and precision inventory")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt" and args.java is not None and args.javac is not None, "declared Windows/JDK profile")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           "unchanged dependency archives")
    archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43", "native archive pin")
    args.output.mkdir(parents=True, exist_ok=False)
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / "lwjgl64.dll").write_bytes(source.read("lwjgl64.dll"))
    classes = args.output / "classes"
    classes.mkdir()
    sources = [HERE / "RasterObservation.java", HERE / "NormalConversionReceiptObservation.java"]
    inputs = [java, javac, jar, *libraries, archive, natives / "lwjgl64.dll", *sources,
              HERE / "capture_conversion_receipts.py", HERE / "capture_conversion.py", SUPPORT / "capture.py", HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Authored driver diagnostic: all256 immediate byte-normal queries, actual GL_BYTE/stride32/client-buffer receipts and GLSL120 gl_Normal transform feedback, "
                       "fixed-function lighting on actual RGBA8 and RGBA32F with vertex clamp TRUE/fragment and read clamp FALSE. "
                       "No unchanged Minecraft/NW body is executed or qualified."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), *map(str,sources)],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(report["helper_before_sha256"]) == {"RasterObservation.class", "RasterObservation$1.class",
               "NormalConversionReceiptObservation.class", "NormalConversionReceiptObservation$1.class"}, "authored helper inventory")
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                           "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-cp", cp,
                           "NormalConversionReceiptObservation", str(natives.resolve())], args.output, "original", 60,
                          environment, args.output.resolve())
        report["capture"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original normal conversion receipts: 256 decode and 8 precision rows; cleanup-complete; network=0" in text.splitlines(), "checked completion")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(), "only pinned native link")
        report["driver"] = [line for line in text.splitlines() if line.startswith("CONVERSION_DRIVER ")]
        report["format"] = [line for line in text.splitlines() if line.startswith("CONVERSION_FORMAT ")]
        ensure(len(report["driver"]) == 1 and report["format"] == [
            "CONVERSION_FORMAT RGBA32F vertex_clamp=1 fragment_clamp=0 read_clamp=0"], "actual driver/profile receipt")
        report["array_layout"] = [line for line in text.splitlines() if line.startswith("NORMAL_ARRAY_LAYOUT ")]
        ensure(report["array_layout"] == ["NORMAL_ARRAY_LAYOUT type=5120 stride=32 buffer_binding=0"],
               "actual GL_BYTE normal array layout receipt")
        observed = rows(text)
        (args.output / "normal-conversion-receipts.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=264, decode_rows=256, precision_rows=8, fixture_sha256=digest(args.output / "normal-conversion-receipts.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "runtime/native/SDK/helper/input bytes unchanged")
    print("Legacy normal conversion receipts: 256 decode and 8 precision rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
