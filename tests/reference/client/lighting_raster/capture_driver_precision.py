"""Capture standalone fixed-function, shader, interpolation and framebuffer precision controls."""

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
SUPPORT = HERE.parent / "controller/capture.py"
spec = importlib.util.spec_from_file_location("controlled_precision_capture", SUPPORT)
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)
capture, digest, ensure, require_success = (
    support.capture, support.digest, support.ensure, support.require_success)

# These are local SDK inputs, not game classes. No client archive is loaded.
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
NATIVES = {
    "windows": ("lwjgl64.dll", "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43",
                "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"),
    "linux": ("liblwjgl64.so", "f89797962cd00024588b9972b7b2fb9d1c1c81817db0e47499824df2f8a310fd",
              "60a9a67061a3dc6b9d055946c58f1f64792abb2127ba0156eaf735dbee5bc729"),
}
JDK_NATIVE_NAMES = ("awt", "awt_xawt", "awt_headless", "jawt", "fontmanager")
POINTS = ((8, 8), (16, 16), (24, 24))
CLAMPS = ("initial", "vertex_clamped", "unclamped")
PIPES = ("clear", "fragment120", "fragment330", "fixed_current_smooth", "fixed_current_flat",
         "fixed_array_smooth", "fixed_array_flat", "generic120_smooth", "generic120_flat",
         "generic330_smooth", "generic330_flat", "lit_smooth", "lit_flat")
CONSTANTS = (["base" + str(index) for index in range(7)] +
             ["t" + str(value) + "u" + str(step) for value in (0, 1, 63, 127, 146, 159, 191, 254)
              for step in range(-4, 5)])
GRADIENTS = ("gradient_rgba", "gradient_alpha", "byte_float_constant_divide",
             "byte_float_constant_multiply", "byte_float_gradient_divide", "byte_float_gradient_multiply")
BYTE_PIPES = ("fixed_byte_smooth", "fixed_byte_flat", "generic120_byte_smooth", "generic120_byte_flat",
              "generic330_byte_smooth", "generic330_byte_flat")


def words(value, count):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){" + str(count - 1) + r"}", value)


def inventory():
    expected = []
    for target in ("rgba8", "rgba32f"):
        for clamp in CLAMPS:
            for scene in [*CONSTANTS, *GRADIENTS]:
                selected = PIPES if scene in CONSTANTS else PIPES[5:11]
                for pipe in selected:
                    expected.extend((target, clamp, pipe, scene, str(x), str(y)) for x, y in POINTS)
            for scene in ("byte_constant", "byte_gradient"):
                for pipe in BYTE_PIPES:
                    expected.extend((target, clamp, pipe, scene, str(x), str(y)) for x, y in POINTS)
            for scene in ("normalized_A18", "normalized_A20"):
                expected.extend((target, clamp, "fixed_lit_smooth", scene, str(x), str(y)) for x, y in POINTS)
    for scene in ("mrt" + str(index) for index in range(8)):
        for pipe in ("mrt_uniform", "mrt_smooth", "mrt_flat"):
            for target in ("mrt8", "mrt32f"):
                expected.extend((target, "initial", pipe, scene, str(x), str(y)) for x, y in POINTS)
    return expected


def observations(text):
    found = [line for line in text.splitlines() if line.startswith("PRECISION_SAMPLE ")]
    expected = inventory()
    ensure(len(found) == len(expected) == 19530, "complete precision sample inventory")
    for line, key in zip(found, expected):
        cells = line.split()
        ensure(len(cells) == 9 and tuple(cells[1:7]) == key and
               re.fullmatch(r"[0-9a-f]{8}", cells[7]) and words(cells[8], 4),
               "ordered separate byte/float observations, without predicted output values")
    inputs = [line for line in text.splitlines() if line.startswith("PRECISION_INPUT ")]
    names = [*CONSTANTS, *GRADIENTS, *("mrt" + str(index) for index in range(8))]
    ensure(len(inputs) == len(names) == 93, "all authored input float bits")
    for line, name in zip(inputs, names):
        cells = line.split()
        ensure(len(cells) == 3 and cells[:2] == ["PRECISION_INPUT", name] and words(cells[2], 12),
               "three complete authored RGBA vertex inputs")
    return found, inputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--native-profile", choices=NATIVES, default="windows")
    parser.add_argument("--context-profile", choices=("legacy", "compat33"), default="legacy")
    parser.add_argument("--native-archive", type=Path)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure((args.native_profile == "windows" and os.name == "nt") or
           (args.native_profile == "linux" and sys.platform.startswith("linux")),
           "matching declared native host profile")
    ensure(args.java is not None and args.javac is not None, "local matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jdk_natives = {}
    if args.native_profile == "linux":
        jdk_lib = (java.parent.parent / "lib").resolve(strict=True)
        for name in JDK_NATIVE_NAMES:
            library = (jdk_lib / ("lib" + name + ".so")).resolve(strict=True)
            ensure(library.parent == jdk_lib, "declared JDK native stays within actual JDK lib directory")
            jdk_natives[name] = library
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), "SDK archive pins")
    library_name, archive_pin, native_pin = NATIVES[args.native_profile]
    archive = (args.native_archive or args.library_root /
               ("org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-" +
                args.native_profile + ".jar")).resolve(strict=True)
    ensure(digest(archive) == archive_pin, "declared exact genuine native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    natives = output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / library_name).write_bytes(source.read(library_name))
    ensure(digest(natives / library_name) == native_pin, "exact native bytes")
    classes = output / "classes"
    classes.mkdir()
    source = HERE / "ControlledPrecisionObservation.java"
    inputs = [java, javac, *jdk_natives.values(), *libraries, archive, natives / library_name, source,
              HERE / "capture_driver_precision.py", SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "native_profile": args.native_profile,
              "context_profile": args.context_profile, "input_before_sha256": before,
              "jdk_native_inputs": {name: {"path": str(path), "sha256": digest(path)}
                                    for name, path in jdk_natives.items()},
              "scope": "Standalone authored compatibility-GL driver precision diagnostic; constant current/float/byte "
                       "colors, thresholds, gradients, fixed lighting, GLSL120/330 smooth/flat/uniform controls; actual "
                       "RGBA8, RGBA32F and simultaneous mixed targets with separate float/UBYTE reads. Exact failing "
                       "normalized GL_BYTE16/64/64 cases with Mdiag2/3/4 and currentnormal0.5. No game classes/assets "
                       "or original game bodies, expected pixel outputs, renderer qualification or tolerances."}
    try:
        report["tools"] = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, label, 10, environment)
            report["tools"].append(receipt)
            require_success(receipt)
            version = (output / receipt["stdout_file"]).read_text() + (output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
        cp = os.pathsep.join(str(path) for path in (classes, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), str(source)],
                          output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(report["helper_before_sha256"]) == {"ControlledPrecisionObservation.class",
               "ControlledPrecisionObservation$1.class"}, "only standalone authored helpers")
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                           "-Dorg.lwjgl.librarypath=" + str(natives), "-cp", cp,
                           "ControlledPrecisionObservation", str(natives), library_name, args.context_profile],
                          output, "driver", 180, environment, output)
        report["capture"] = receipt
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        report["native_link_attempts"] = [line.removeprefix("NATIVE_LINK_ATTEMPT ")
                                          for line in text.splitlines() if line.startswith("NATIVE_LINK_ATTEMPT ")]
        jdk_links = [line for line in text.splitlines() if line.startswith("JDK_NATIVE_LINK ")]
        report["jdk_native_links"] = jdk_links
        ensure(len(report["native_link_attempts"]) <= 32 and len(jdk_links) <= 16,
               "bounded native-link attempt and JDK-native receipts")
        for line in jdk_links:
            cells = line.split(" ", 2)
            ensure(len(cells) == 3 and cells[1] in jdk_natives and
                   Path(cells[2]).resolve(strict=True) == jdk_natives[cells[1]],
                   "accepted JDK native is one of the exact before/after pinned local JDK files")
        require_success(receipt)
        ensure("Controlled driver precision: 6486 draws; 19530 samples; cleanup-complete; network=0" in text.splitlines(),
               "complete native controls and checked cleanup")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve() == natives / library_name, "only pinned native link")
        rows, authored = observations(text)
        prefixes = ("PRECISION_DRIVER ", "PRECISION_CONTEXT ", "PRECISION_FIXED_SETUP ",
                    "PRECISION_INITIAL_CLAMP ", "PRECISION_PROGRAM ",
                    "PRECISION_FORMAT ", "PRECISION_CLAMP ", "PRECISION_BYTE_INPUT ",
                    "PRECISION_NORMAL_INPUT ", "PRECISION_NORMAL_STATE ", "PRECISION_MRT_FORMAT ")
        headers = [line for line in text.splitlines() if line.startswith(prefixes)]
        counts = {prefix: sum(line.startswith(prefix) for line in headers) for prefix in prefixes}
        ensure(list(counts.values()) == [1, 1, 1, 1, 9, 2, 7, 2, 2, 12, 1],
               "complete actual driver, format, clamp, program and normalized-state receipts")
        fixture = output / "driver-precision.json"
        fixture.write_text(json.dumps({"headers": headers, "inputs": authored, "rows": rows}, indent=2) + "\n",
                           encoding="utf-8", newline="\n")
        report.update(status="PASS", draws=6486, samples=19530, authored_inputs=93,
                      headers=headers, fixture_sha256=digest(fixture))
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "runtime/native/SDK/helper/input bytes unchanged")
    print("Controlled driver precision: 6486 actual draws, 19530 separate byte/float samples; " + str(output / "capture.json"))


if __name__ == "__main__":
    main()
