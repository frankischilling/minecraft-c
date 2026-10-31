"""Capture actual unsigned-byte color arrays, compiled lists and queried current state."""

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
spec = importlib.util.spec_from_file_location("color_conversion_sdk", HERE / "capture_driver_precision.py")
if spec is None or spec.loader is None:
    raise RuntimeError("local driver-capture support unavailable")
SDK = importlib.util.module_from_spec(spec)
spec.loader.exec_module(SDK)
capture, digest, ensure, require_success = SDK.capture, SDK.digest, SDK.ensure, SDK.require_success
DRIVERS = {
    "windows": "NVIDIA Corporation | NVIDIA GeForce RTX 3060/PCIe/SSE2 | 4.6.0 NVIDIA 581.42",
    "linux": "Mesa | llvmpipe (LLVM 20.1.2, 256 bits) | 4.5 (Compatibility Profile) Mesa 25.2.8-0ubuntu0.24.04.4",
}
LABELS = ("before_direct", "after_direct", "before_compile", "after_compile", "before_call", "after_call",
          "before_repeat", "after_repeat")


def words(value, count):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){" + str(count - 1) + r"}", value)


def observations(text):
    found = [line for line in text.splitlines()
             if line.startswith(("COLOR_STATE ", "COLOR_STORAGE ", "COLOR_DECODE "))]
    ensure(len(found) == 2560, "256 complete storage/feedback and eight-state inventories")
    for value in range(256):
        group = found[value*10:(value+1)*10]
        for label, line in zip(LABELS, group[:8]):
            cells = line.split()
            ensure(len(cells) == 12 and cells[:3] == ["COLOR_STATE", str(value), label] and
                   words(cells[3], 3) and all(words(item, 4) for item in cells[4:9]) and
                   cells[9:] == ["1", "1032", "5634"], "complete queried current attributes/materials/selectors")
        storage, decoded = (line.split() for line in group[8:])
        ensure(len(storage) == 5 and storage[:2] == ["COLOR_STORAGE", str(value)] and
               all(re.fullmatch(r"[0-9a-f]{64}", item) for item in storage[2:]) and
               len(set(storage[2:])) == 3, "three distinct actual source/poisoned-storage hashes")
        ensure(len(decoded) == 5 and decoded[:2] == ["COLOR_DECODE", str(value)] and
               all(words(item, 12) for item in decoded[2:]), "all direct/list/repeated-list actual RGBA vectors")
    return found


def headers(text, driver):
    result = {}
    for prefix in ("COLOR_DRIVER", "COLOR_FORMAT", "COLOR_CLAMP", "COLOR_STATE_POLICY", "COLOR_LAYOUT", "COLOR_CAPTURE"):
        matches = [line for line in text.splitlines() if line.startswith(prefix + " ")]
        ensure(len(matches) == 1, "single actual " + prefix + " receipt")
        result[prefix] = matches[0]
    ensure(result["COLOR_DRIVER"] == "COLOR_DRIVER " + driver, "complete actual declared driver")
    ensure(re.fullmatch(r"COLOR_FORMAT 8 8 8 8 [1-9][0-9]* [0-9]+", result["COLOR_FORMAT"]),
           "actual RGBA8/depth/stencil context")
    ensure(re.fullmatch(r"COLOR_CLAMP vertex=[0-9]+ fragment=[0-9]+ read=[0-9]+", result["COLOR_CLAMP"]),
           "actual queried clamp settings")
    ensure(result["COLOR_STATE_POLICY"] ==
           "COLOR_STATE_POLICY after_direct=unspecified_observation no_state_TF_parity_assumed=1",
           "indeterminate direct-array current state is observed separately")
    ensure(result["COLOR_LAYOUT"] == "COLOR_LAYOUT color_type=5121 color_size=4 color_stride=32 color_buffer=0 "
           "vertex_type=5126 vertex_size=3 vertex_stride=32 vertex_buffer=0", "actual client-array layout")
    ensure(result["COLOR_CAPTURE"] == "COLOR_CAPTURE mode=4864 compile_program=0 draw_mode=4 vertices=3 "
           "feedback_components=12 sentinel_components=4 lighting=1 color_material=1 normalize=0 rescale=0",
           "actual fixed-function list compilation and feedback profile")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--native-profile", choices=SDK.NATIVES, default="windows")
    parser.add_argument("--native-archive", type=Path)
    parser.add_argument("--expected-driver")
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure((args.native_profile == "windows" and os.name == "nt") or
           (args.native_profile == "linux" and sys.platform.startswith("linux")), "matching declared host profile")
    ensure(args.java is not None and args.javac is not None, "local matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jdk_natives = {}
    if args.native_profile == "linux":
        root = (java.parent.parent / "lib").resolve(strict=True)
        jdk_natives = {name: (root / ("lib" + name + ".so")).resolve(strict=True) for name in SDK.JDK_NATIVE_NAMES}
        ensure(all(path.parent == root for path in jdk_natives.values()), "exact JDK native containment")
    libraries = [(args.library_root / name).resolve(strict=True) for name in SDK.LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, SDK.LIBRARIES.values())), "exact local SDK archives")
    library_name, archive_pin, native_pin = SDK.NATIVES[args.native_profile]
    archive = (args.native_archive or args.library_root /
               ("org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-" +
                args.native_profile + ".jar")).resolve(strict=True)
    ensure(digest(archive) == archive_pin, "exact genuine native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    natives, classes = output / "natives", output / "classes"
    natives.mkdir()
    classes.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / library_name).write_bytes(source.read(library_name))
    ensure(digest(natives / library_name) == native_pin, "exact genuine native bytes")
    source = HERE / "ColorConversionObservation.java"
    inputs = [java, javac, *jdk_natives.values(), *libraries, archive, natives / library_name, source,
              HERE / "capture_color_conversion.py", HERE / "capture_driver_precision.py", SDK.SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    driver = args.expected_driver or DRIVERS[args.native_profile]
    report = {"status": "FAIL", "native_profile": args.native_profile, "input_before_sha256": before,
              "expected_driver": driver,
              "jdk_native_inputs": {name: {"path": str(path), "sha256": digest(path)}
                                    for name, path in jdk_natives.items()},
              "scope": "Standalone SDK-only actual driver observations of all256 unsigned-byte RGBA inputs. "
                       "Direct gl_Color transform feedback and two poisoned-storage/current-attribute/client-array "
                       "replays of fixed-function compiled lists, plus eight separate current-normal/color/tracked-"
                       "material queries per value. Post-direct current state is an unspecified observation, not a "
                       "parity promise with transform feedback. No formula, native expected words, game classes, "
                       "raster pixels, renderer qualification or tolerance."}
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
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
        ensure(set(report["helper_before_sha256"]) == {"ColorConversionObservation.class", "ColorConversionObservation$1.class"}
               and all(path.parent == classes for path in classes.rglob("*.class")), "only two standalone authored helpers")
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                           "-Dorg.lwjgl.librarypath=" + str(natives), "-cp", cp,
                           "ColorConversionObservation", str(natives), library_name],
                          output, "driver", 90, environment, output)
        report["capture"] = receipt
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        attempts = [line.removeprefix("NATIVE_LINK_ATTEMPT ") for line in text.splitlines()
                    if line.startswith("NATIVE_LINK_ATTEMPT ")]
        jdk_links = [line for line in text.splitlines() if line.startswith("JDK_NATIVE_LINK ")]
        report.update(native_link_attempts=attempts, jdk_native_links=jdk_links)
        ensure(1 <= len(attempts) <= 32 and len(jdk_links) <= 16, "bounded native-link attempt receipts")
        allowed_paths = {natives / library_name, *jdk_natives.values()}
        ensure(all(argument in jdk_natives or Path(argument).is_absolute() and
                   Path(argument).resolve(strict=True) in allowed_paths for argument in attempts),
               "only explicitly declared native attempts")
        for line in jdk_links:
            cells = line.split(" ", 2)
            ensure(len(cells) == 3 and cells[1] in jdk_natives and Path(cells[2]).resolve(strict=True) == jdk_natives[cells[1]],
                   "actual accepted JDK native request matches a before/after pinned file")
        require_success(receipt)
        marker = "Actual color conversion: 256 decode; 256 storage; 2048 state rows; cleanup-complete; network=0"
        ensure(text.splitlines().count(marker) == 1, "complete actual color controls and checked cleanup")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve(strict=True) == natives / library_name,
               "single exact genuine LWJGL native link")
        observed_headers, observed = headers(text, driver), observations(text)
        decoded = [line.split() for line in observed if line.startswith("COLOR_DECODE ")]
        report["observed_equal_counts"] = {
            "direct_vs_compiled": sum(row[2] == row[3] for row in decoded),
            "compiled_vs_repeated": sum(row[3] == row[4] for row in decoded),
        }
        fixture = output / "color-conversion.json"
        fixture.write_text(json.dumps({"headers": observed_headers, "rows": observed}, indent=2) + "\n",
                           encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=2560, decode_rows=256, storage_rows=256, state_rows=2048,
                      headers=observed_headers, fixture_sha256=digest(fixture))
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "runtime/native/SDK/helper/source inputs unchanged")
    print("Actual color attribute observations: 256 conversions and 2048 separate current-state rows; " +
          str(output / "capture.json"))


if __name__ == "__main__":
    main()
