"""Capture unchanged frozen scene32 pixels before querying its full material state."""

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


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("local capture support unavailable: " + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CAPTURE = load("material_sequence_capture", SUPPORT / "capture.py")
CLASSFILE = load("material_sequence_classfile", SUPPORT / "classfile.py")
capture, digest, ensure, require_success = (
    CAPTURE.capture, CAPTURE.digest, CAPTURE.ensure, CAPTURE.require_success)

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
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
JDK_NAMES = ("awt", "awt_xawt", "awt_headless", "jawt", "fontmanager")
FROZEN = {
    "RasterObservation.java": "511f4f8f0c51100e6fffb9e7b70ac4bf40b0f087546730e7f97c00fcb2593d85",
    "MaterialObservation.java": "543fb3e37a2bc43a53fe74844fd2e72d36027be75fb5e178854185bad76c9360",
}
MARKER = "Material sequence: frozen scene32 draw then material query; cleanup-complete; network=0"
PREFIX_CALLS = (
    "CM(1032,4608)+Enable(2903)",
    "Color(3e800000,3f000000,3f400000,3f000000)",
    "CM(1032,4609)",
    "Color(3f400000,3e800000,3f000000,3f400000)",
    "CM(1032,5632)",
    "Color(3e000000,3e800000,3e000000,3f800000)",
    "Disable(2903)",
    "Color(3f600000,3f000000,3e800000,3e000000)",
)


def prefix_input(prefix):
    calls = ";".join(PREFIX_CALLS[:prefix]) if prefix else "none"
    return "MATERIAL_SEQUENCE_PREFIX " + str(prefix) + " setup=RasterObservation.setup(false) calls=" + calls


def marker(prefix):
    return MARKER if prefix is None else (
        "Material sequence: prefix" + str(prefix) + " end material query; cleanup-complete; network=0")


def observations(text, prefix=None):
    lines = text.splitlines()
    found = [line for line in lines if line.startswith(("RASTER ", "RASTER_STATE ", "MATERIAL "))]
    ensure(len(found) == (3 if prefix is None else 1), "complete declared observation inventory")
    if prefix is None:
        pixel, state = [line.split() for line in found[:2]]
        ensure(len(pixel) == 4 and pixel[:3] == ["RASTER", "material_switch_freeze", "0"] and
               re.fullmatch(r"[0-9a-f]{32768}", pixel[3]), "complete frozen64x64 RGBA8 frame")
        ensure(len(state) == 8 and state[:3] == ["RASTER_STATE", "material_switch_freeze", "0"] and
               re.fullmatch(r"[01]{9}", state[3]) and all(value.isdecimal() for value in state[4:]),
               "actual enabled flags, material selectors, shade and fog mode")
    material = found[-1].split()
    name = "after_contiguous_draw" if prefix is None else "after_prefix_" + str(prefix)
    ensure(len(material) == 16 and material[:2] == ["MATERIAL", name], "one full material query at end")
    ensure(all(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){3}", value)
               for value in [*material[2:10], material[12]]) and
           all(re.fullmatch(r"[0-9a-f]{8}", value) for value in material[10:12]) and
           material[13] in ("0", "1") and all(value.isdecimal() for value in material[14:]),
           "all front/back RGBA materials, shininess, current RGBA and tracking selectors")
    terminal = marker(prefix)
    ensure(lines.count(terminal) == 1 and lines.index(found[-1]) < lines.index(terminal),
           "one normal cleanup marker after all observations")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--native-profile", choices=NATIVES, default="windows")
    parser.add_argument("--native-archive", type=Path)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--prefixes", nargs="*", type=int,
                        help="fresh JVM/context for each declared prefix0..8; no values selects all nine; "
                             "omitted runs unchanged complete scene32")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prefixes = None if args.prefixes is None else (args.prefixes or list(range(9)))
    ensure(prefixes is None or (1 <= len(prefixes) <= 9 and len(prefixes) == len(set(prefixes)) and
           all(0 <= value <= 8 for value in prefixes)), "one to nine distinct declared prefixes0..8")
    ensure((args.native_profile == "windows" and os.name == "nt") or
           (args.native_profile == "linux" and sys.platform.startswith("linux")),
           "matching declared native host")
    ensure(args.java is not None and args.javac is not None, "local matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jdk_natives = {}
    if args.native_profile == "linux":
        jdk_lib = (java.parent.parent / "lib").resolve(strict=True)
        jdk_natives = {name: (jdk_lib / ("lib" + name + ".so")).resolve(strict=True) for name in JDK_NAMES}
        ensure(all(path.parent == jdk_lib for path in jdk_natives.values()), "exact JDK native containment")
    for name, pin in FROZEN.items():
        ensure(hashlib.sha256((HERE / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest() == pin,
               "unchanged complete frozen helper source: " + name)
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "unchanged original type-verification dependency and genuine SDK archives")
    metadata = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(metadata["client_jar_sha256"] == JAR, "original class-pin archive authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in metadata["classes"].items():
            actual = CLASSFILE.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "unchanged complete original class and selected Code: " + name)
    library_name, archive_pin, native_pin = NATIVES[args.native_profile]
    archive = (args.native_archive or args.library_root /
               ("org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-" +
                args.native_profile + ".jar")).resolve(strict=True)
    ensure(digest(archive) == archive_pin, "exact declared genuine native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    natives, classes = output / "natives", output / "classes"
    natives.mkdir()
    classes.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / library_name).write_bytes(source.read(library_name))
    ensure(digest(natives / library_name) == native_pin, "exact declared genuine native bytes")
    sources = [HERE / name for name in [*FROZEN, "MaterialSequenceObservation.java"]]
    inputs = [java, javac, *jdk_natives.values(), jar, *libraries, archive, natives / library_name,
              *sources, HERE / "source-pins.json", HERE / "capture_material_sequence.py",
              SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "native_profile": args.native_profile, "input_before_sha256": before,
              "prefixes": prefixes, "profiles": [],
              "frozen_source_lf_sha256": FROZEN,
              "jdk_native_inputs": {name: {"path": str(path), "sha256": digest(path)}
                                    for name, path in jdk_natives.items()},
              "scope": "Standalone authored diagnostic calls unchanged complete RasterObservation.scene(32): "
                       "contiguous setup, ColorMaterial/color calls, actual draw, fullRGBA8 pixels and RASTER_STATE; "
                       "only then unchanged MaterialObservation.state queries full front/back materials/current color. "
                       "Both frozen helpers compile unmodified, their mains/native guards never execute. "
                       "OriginalJar is a pinned type-verification dependency, no original game body or assets execute. "
                       "Optional authored prefixes0..8 instead use frozen setup(false), the declared firstN "
                       "contiguous scene32 call groups and one full material query before any draw, with a fresh "
                       "JVM/context for each prefix. No predicted pixel/material outputs, native renderer "
                       "qualification or tolerance."}
    try:
        report["tools"] = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, label, 10, environment)
            report["tools"].append(receipt)
            require_success(receipt)
            version = (output / receipt["stdout_file"]).read_text() + (output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
        cp = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), *map(str, sources)],
                          output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
        expected_helpers = {name + suffix for name in ("RasterObservation", "MaterialObservation",
                            "MaterialSequenceObservation") for suffix in (".class", "$1.class")}
        ensure(set(report["helper_before_sha256"]) == expected_helpers and
               all(path.parent == classes for path in classes.rglob("*.class")),
               "only six authored helper classes, no original class replacement")
        profiles = []
        for prefix in ([None] if prefixes is None else prefixes):
            arguments = [str(natives), library_name] + ([] if prefix is None else [str(prefix)])
            receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                               "-Dorg.lwjgl.librarypath=" + str(natives), "-cp", cp,
                               "MaterialSequenceObservation", *arguments],
                              output, "driver" if prefix is None else "prefix" + str(prefix), 60,
                              environment, output)
            profile = {"prefix": prefix, "capture": receipt, "status": "FAIL"}
            report["profiles"].append(profile)
            if prefix is None:
                report["capture"] = receipt
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            lines = text.splitlines()
            attempts = [line.removeprefix("NATIVE_LINK_ATTEMPT ") for line in lines
                        if line.startswith("NATIVE_LINK_ATTEMPT ")]
            jdk_links = [line for line in lines if line.startswith("JDK_NATIVE_LINK ")]
            links = [line.removeprefix("NATIVE_LINK ") for line in lines if line.startswith("NATIVE_LINK ")]
            profile.update(native_link_attempts=attempts, jdk_native_links=jdk_links, native_links=links)
            ensure(1 <= len(attempts) <= 32 and len(jdk_links) <= 16, "bounded actual native-link inventory")
            known = {str(natives / library_name), *jdk_natives, *(str(path) for path in jdk_natives.values())}
            ensure(all(argument in known for argument in attempts), "all native-link attempts explicitly allowed")
            for line in jdk_links:
                cells = line.split(" ", 2)
                ensure(len(cells) == 3 and cells[1] in jdk_natives and
                       Path(cells[2]).resolve(strict=True) == jdk_natives[cells[1]], "exact pinned JDK native link")
            ensure(links == [str(natives / library_name)], "one actual exact declared LWJGL native link")
            require_success(receipt)
            driver = [line for line in lines if line.startswith("MATERIAL_SEQUENCE_DRIVER ")]
            formats = [line for line in lines if line.startswith("MATERIAL_SEQUENCE_FORMAT ")]
            ensure(len(driver) == 1 and re.fullmatch(r"MATERIAL_SEQUENCE_DRIVER [^\r\n]{1,726}", driver[0]) and
                   len(formats) == 1 and re.fullmatch(r"MATERIAL_SEQUENCE_FORMAT 8 8 8 8 [0-9]+ [0-9]+", formats[0]),
                   "actual bounded driver and complete requested RGBA8/depth/stencil receipts")
            declared = [line for line in lines if line.startswith("MATERIAL_SEQUENCE_PREFIX ")]
            ensure(declared == ([] if prefix is None else [prefix_input(prefix)]),
                   "complete exact authored prefix input description")
            observed = observations(text, prefix)
            expected_lines = [*observed, *driver, *formats, *declared, marker(prefix),
                              *("NATIVE_LINK_ATTEMPT " + value for value in attempts),
                              *jdk_links, *("NATIVE_LINK " + value for value in links)]
            ensure(sorted(lines) == sorted(expected_lines), "exact output inventory without additional rows")
            if prefix is not None:
                ensure(lines.index(declared[0]) < lines.index(observed[0]), "declared input precedes end query")
            profile.update(status="PASS", frames=1 if prefix is None else 0, rows=len(observed),
                           driver=driver, format=formats, authored_input=declared)
            profiles.append({"prefix": prefix, "headers": [*driver, *formats, *declared], "rows": observed})
        fixture = output / "material-sequence.json"
        payload = ({"headers": profiles[0]["headers"], "rows": profiles[0]["rows"]}
                   if prefixes is None else {"prefixes": prefixes, "profiles": profiles})
        fixture.write_text(json.dumps(payload, indent=2) + "\n",
                           encoding="utf-8", newline="\n")
        report.update(status="PASS", frames=sum(profile["frames"] for profile in report["profiles"]),
                      rows=sum(profile["rows"] for profile in report["profiles"]), contexts=len(profiles),
                      fixture_sha256=digest(fixture))
        if prefixes is None:
            report.update(driver=driver, format=formats, native_link_attempts=attempts,
                          jdk_native_links=jdk_links, native_links=links)
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
        ensure(same, "JDK/native/SDK/original archive/frozen source/helper/input bytes unchanged")
    print("Material sequence: " + ("one frozen full frame, source state and full material row" if prefixes is None else
          str(len(prefixes)) + " fresh prefix contexts and end material rows") + "; " + str(output / "capture.json"))


if __name__ == "__main__":
    main()
