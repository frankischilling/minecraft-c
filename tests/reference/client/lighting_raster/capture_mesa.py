"""Capture all frozen lighting scenes on the separately pinned Linux/Mesa profile."""

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


CAPTURE = load("mesa_lighting_process_capture", SUPPORT / "capture.py")
CLASSFILE = load("mesa_lighting_classfile", SUPPORT / "classfile.py")
capture, digest, ensure, require_success = (
    CAPTURE.capture, CAPTURE.digest, CAPTURE.ensure, CAPTURE.require_success)

NATIVE_ARCHIVE = "f89797962cd00024588b9972b7b2fb9d1c1c81817db0e47499824df2f8a310fd"
NATIVE = "60a9a67061a3dc6b9d055946c58f1f64792abb2127ba0156eaf735dbee5bc729"
JDK_NAMES = ("awt", "awt_xawt", "awt_headless", "jawt", "fontmanager")
DEFAULT_DRIVER = ("Mesa | llvmpipe (LLVM 20.1.2, 256 bits) | "
                  "4.5 (Compatibility Profile) Mesa 25.2.8-0ubuntu0.24.04.4")
# Freeze scene/input contents while allowing checkout line-ending conventions.
FROZEN = {
    "RasterObservation.java": "511f4f8f0c51100e6fffb9e7b70ac4bf40b0f087546730e7f97c00fcb2593d85",
    "RasterExtendedObservation.java": "07739d3441c32a5d412409b1052fb2236bc2f8d01303df69c6e7a9bbfd76977a",
    "ConvertedRasterObservation.java": "759c6ba3a9fb8a344216ee2a99486bfa7b01f3155043ff188110b8f39fb5224f",
    "inputs.json": "d3066de5866083bd9079ea3e209e56d851bea73040b9d72f2b32de9abdf69fdd",
    "extended-inputs.json": "880a7af400f30d468c1441d54d72eb124b28a686d9adf77e23b28d3d682c79fd",
    "converted-inputs.json": "63b8f9ceb8d22cd7b1a47bbfc710a59a94e61fc55385b5d3ce26d3c38cf14720",
}
OLD_GUARD = '''            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }'''
LINUX_GUARD = r'''            private int nativeAttempts;
            @Override public void checkLink(String library) {
                String argument = library.replace('\n',' ').replace('\r',' ');
                System.out.println("NATIVE_LINK_ATTEMPT " + argument.substring(0,Math.min(argument.length(),240)));
                require(++nativeAttempts <= 32,"bounded native-link attempt inventory");
                Path requested = Path.of(library);
                if (requested.isAbsolute() && requested.normalize().equals(natives.resolve("liblwjgl64.so"))) {
                    links.add(library); return;
                }
                try {
                    Path jdkLib = Path.of(System.getProperty("java.home"),"lib").toRealPath();
                    for (String name : new String[]{"awt","awt_xawt","awt_headless","jawt","fontmanager"}) {
                        Path exact = jdkLib.resolve(System.mapLibraryName(name)).toRealPath();
                        require(exact.getParent().equals(jdkLib),"exact JDK native stays in java.home/lib");
                        if (library.equals(name) || requested.isAbsolute() && requested.normalize().equals(exact)) {
                            System.out.println("JDK_NATIVE_LINK " + name + " " + exact); return;
                        }
                    }
                } catch (java.io.IOException error) {
                    throw new SecurityException("cannot resolve exact JDK native allowlist",error);
                }
                throw new SecurityException("native outside exact LWJGL/JDK allowlist: " + argument);
            }'''
COHORTS = (
    ("initial", "RasterObservation", "inputs.json", "capture_reference.py", 38, 2, "finite.json", "Original raster: "),
    ("extended", "RasterExtendedObservation", "extended-inputs.json", "capture_extended.py", 18, 3,
     "extended-finite.json", "Original extended raster: "),
    ("converted", "ConvertedRasterObservation", "converted-inputs.json", "capture_converted.py", 2, 3,
     "converted-finite.json", "Original converted raster: "),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def generated_source(source, destination):
    raw = source.read_bytes()
    text = raw.decode("utf-8")
    newline = "\r\n" if "\r\n" in text else "\n"
    old, new = [fragment.replace("\n", newline) for fragment in (OLD_GUARD, LINUX_GUARD)]
    ensure(text.count(old) == 1 and new not in text, "exactly one unchanged original native guard")
    prefix, suffix = text.split(old)
    generated = prefix + new + suffix
    ensure(generated.count(new) == 1 and generated.replace(new, old, 1).encode("utf-8") == raw,
           "reversing the only guard edit recovers the complete original source bytes")
    payload = generated.encode("utf-8")
    destination.write_bytes(payload)
    ensure(destination.read_bytes() == payload, "exact generated source bytes retained")
    return {"original": str(source), "generated": str(destination), "original_sha256": sha(raw),
            "generated_sha256": sha(payload), "unchanged_prefix_sha256": sha(prefix.encode("utf-8")),
            "unchanged_suffix_sha256": sha(suffix.encode("utf-8")),
            "line_endings": "CRLF" if newline == "\r\n" else "LF", "reversed_byte_identical": True,
            "replacement_count": 1}


def native_receipts(text, profile, natives, jdk_natives):
    lines = text.splitlines()
    attempts = [line.removeprefix("NATIVE_LINK_ATTEMPT ") for line in lines
                if line.startswith("NATIVE_LINK_ATTEMPT ")]
    jdk_links = [line for line in lines if line.startswith("JDK_NATIVE_LINK ")]
    links = [line.removeprefix("NATIVE_LINK ") for line in lines if line.startswith("NATIVE_LINK ")]
    profile.update(native_link_attempts=attempts, jdk_native_links=jdk_links, native_links=links)
    ensure(1 <= len(attempts) <= 32 and len(jdk_links) <= 16, "bounded native-link receipt inventory")
    known = {str(natives / "liblwjgl64.so"), *jdk_natives, *(str(path) for path in jdk_natives.values())}
    ensure(all(argument in known for argument in attempts), "every native-link attempt is explicitly allowed")
    for line in jdk_links:
        cells = line.split(" ", 2)
        ensure(len(cells) == 3 and cells[1] in jdk_natives and
               Path(cells[2]).resolve(strict=True) == jdk_natives[cells[1]], "exact pinned JDK native request")
    return links


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--native-archive", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--expected-driver", default=DEFAULT_DRIVER)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(sys.platform.startswith("linux") and os.pathsep == ":", "declared Linux native host")
    ensure(args.java is not None and args.javac is not None, "local matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jdk_lib = (java.parent.parent / "lib").resolve(strict=True)
    jdk_natives = {name: (jdk_lib / ("lib" + name + ".so")).resolve(strict=True) for name in JDK_NAMES}
    ensure(all(path.parent == jdk_lib for path in jdk_natives.values()), "exact JDK native containment")
    for name, expected in FROZEN.items():
        ensure(sha((HERE / name).read_bytes().replace(b"\r\n", b"\n")) == expected,
               "frozen complete scene/input content: " + name)
    parsers = {cohort[0]: load("mesa_frozen_" + cohort[0], HERE / cohort[3]) for cohort in COHORTS}
    ensure(all(Path(module.capture.__code__.co_filename).resolve() == (SUPPORT / "capture.py").resolve()
               for module in parsers.values()), "frozen parsers use the same bounded capture support")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in parsers["initial"].LIBRARIES]
    ensure(digest(jar) == parsers["initial"].JAR and
           all(digest(path) == pin for path, pin in zip(libraries, parsers["initial"].LIBRARIES.values())),
           "unchanged actual original and SDK archives")
    metadata = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(metadata["client_jar_sha256"] == digest(jar), "original class-pin archive authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in metadata["classes"].items():
            actual = CLASSFILE.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "unchanged complete original class and selected Code: " + name)
    native_archive = args.native_archive.resolve(strict=True)
    ensure(digest(native_archive) == NATIVE_ARCHIVE, "exact Linux LWJGL2.9 native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    natives, sources, classes = [output / name for name in ("natives", "sources", "classes")]
    for directory in (natives, sources, classes):
        directory.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "liblwjgl64.so").write_bytes(archive.read("liblwjgl64.so"))
    ensure(digest(natives / "liblwjgl64.so") == NATIVE, "exact genuine Linux native bytes")
    generated = [sources / (cohort[1] + ".java") for cohort in COHORTS]
    audit = {"recipe": "Replace exactly one existing SecurityManager.checkLink method per authored helper with "
                       "the Linux filename/attempt counter/exact named-JDK native guard. No other source edit; "
                       "reverse replacement must recover complete original bytes before compilation.",
             "original_guard_lf": OLD_GUARD, "linux_guard_lf": LINUX_GUARD,
             "sources": [generated_source(HERE / path.name, path) for path in generated]}
    audit_path = output / "source-transformations.json"
    audit_path.write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8", newline="\n")
    fixed_inputs = [HERE / name for name in FROZEN]
    inputs = [java, javac, *jdk_natives.values(), jar, *libraries, native_archive, natives / "liblwjgl64.so",
              *fixed_inputs, *generated, audit_path, HERE / "source-pins.json", HERE / "capture_mesa.py",
              *(HERE / cohort[3] for cohort in COHORTS), SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py",
              HERE / "finite.json", HERE / "extended-finite.json", HERE / "converted-finite.json"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before, "cohorts": [], "complete_inventory": False,
              "requested_complete_inventory": True, "expected_driver": args.expected_driver,
              "source_transformations": audit,
              "generated_source_before_sha256": {path.name: digest(path) for path in generated},
              "jdk_native_inputs": {name: {"path": str(path), "sha256": digest(path)}
                                    for name, path in jdk_natives.items()},
              "scope": "Separate Linux/Mesa capture of all frozen58 authored profiles/59 complete64x64RGBA8 frames "
                       "and state/auxiliary rows. Original scene/control/oracle/cleanup bodies are byte-protected; "
                       "only native guards differ. Actual originalJar and selected class/Code pins retained. "
                       "Initial36/37 remain genuine empty NW draws; only converted cohort qualifies actual NW "
                       "quad geometry. No Windows fixture replacement or complete original game frame."}
    try:
        report["tools"] = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, label, 10, environment)
            report["tools"].append(receipt)
            require_success(receipt)
            version = (output / receipt["stdout_file"]).read_text() + (output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
        cp = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), *map(str, generated)],
                          output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        report["helper_before_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
        allowed_helpers = {cohort[1] + suffix for cohort in COHORTS for suffix in (".class", "$1.class")}
        ensure(set(report["helper_before_sha256"]) == allowed_helpers and
               all(path.parent == classes for path in classes.rglob("*.class")),
               "only six authored helpers; no original class replacement")
        for cohort, main_class, input_name, parser_name, count, rows_per_frame, fixture_name, marker in COHORTS:
            directory = output / cohort
            directory.mkdir()
            data = json.loads((HERE / input_name).read_text(encoding="utf-8"))
            cases = data["cases"]
            ensure(data["width"] == data["height"] == 64 and len(cases) == count,
                   "complete fixed cohort dimensions and profile count")
            indices = list(range(count))
            result = {"name": cohort, "status": "FAIL", "profiles": [], "selected_indices": indices,
                      "complete_inventory": False, "fixture_file": fixture_name}
            report["cohorts"].append(result)
            observed = []
            try:
                for index, case in enumerate(cases):
                    arguments = [str(natives), str(index)]
                    if cohort != "extended":
                        arguments.insert(0, str(jar))
                    receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                                       "-Dorg.lwjgl.librarypath=" + str(natives), "-cp", cp, main_class, *arguments],
                                      directory, "original" + str(index), 60, environment, directory)
                    profile = {"name": case["name"], "index": index, "capture": receipt}
                    result["profiles"].append(profile)
                    text = (directory / receipt["stdout_file"]).read_text(encoding="utf-8")
                    links = native_receipts(text, profile, natives, jdk_natives)
                    require_success(receipt)
                    ensure(links == [str(natives / "liblwjgl64.so")], "one exact pinned LWJGL native link")
                    ensure(marker + case["name"] + "; cleanup-complete; network=0" in text.splitlines(),
                           "unchanged checked cleanup and no Java network attempts/writes")
                    profile["driver"] = [line for line in text.splitlines() if line.startswith("RASTER_DRIVER ")]
                    profile["format"] = [line for line in text.splitlines() if line.startswith("RASTER_FORMAT ")]
                    ensure(profile["driver"] == ["RASTER_DRIVER " + args.expected_driver],
                           "actual complete expected Mesa driver receipt")
                    ensure(len(profile["format"]) == 1 and re.fullmatch(r"RASTER_FORMAT 8 8 8 8 [0-9]+ [0-9]+",
                                                                                  profile["format"][0]),
                           "actual complete RGBA8/depth/stencil receipt")
                    actual = parsers[cohort].rows(text, case)
                    pixel = next(line for line in actual if line.startswith("RASTER "))
                    rgba = bytes.fromhex(pixel.split()[3])
                    profile.update(status="PASS", rows=len(actual), frames=case["frames"],
                                   probe_rgba=list(rgba[(47*64+16)*4:(47*64+16)*4+4]))
                    observed.extend(actual)
                frames = sum(case["frames"] for case in cases)
                ensure(len(observed) == frames * rows_per_frame, "every complete whole frame/state/auxiliary row")
                fixture = output / fixture_name
                fixture.write_text(json.dumps({"indices": indices, "rows": observed}, indent=2) + "\n",
                                   encoding="utf-8", newline="\n")
                result.update(status="PASS", rows=len(observed), frames=frames, profiles_completed=count,
                              complete_inventory=True, fixture_sha256=digest(fixture))
            except BaseException as error:
                result["error"] = {"type": type(error).__name__, "message": str(error)}
                raise
            finally:
                (directory / "capture.json").write_text(json.dumps(result, indent=2) + "\n",
                                                         encoding="utf-8", newline="\n")
        ensure(sum(cohort["profiles_completed"] for cohort in report["cohorts"]) == 58 and
               sum(cohort["frames"] for cohort in report["cohorts"]) == 59 and
               sum(cohort["rows"] for cohort in report["cohorts"]) == 138, "complete three-cohort inventory")
        report.update(status="PASS", complete_inventory=True, profiles_completed=58, frames=59, rows=138)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["generated_source_after_sha256"] = {path.name: digest(path) for path in generated}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
        same = (before == report["input_after_sha256"] and
                report["generated_source_before_sha256"] == report["generated_source_after_sha256"] and
                ("helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"]))
        if not same:
            report["status"] = "FAIL"
            report["complete_inventory"] = False
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "original/helper/generated source/JDK/native/SDK/Windows fixture bytes unchanged")
    print("Mesa original lighting capture: 58 profiles, 59 complete RGBA8 frames, 138 state/pixel/auxiliary rows; " +
          str(output / "capture.json"))


if __name__ == "__main__":
    main()
