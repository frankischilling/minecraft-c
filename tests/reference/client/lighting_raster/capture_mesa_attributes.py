"""Capture frozen array, compiled-list and material diagnostics on actual Linux/Mesa."""

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
spec = importlib.util.spec_from_file_location("mesa_attribute_support", HERE / "capture_mesa.py")
if spec is None or spec.loader is None:
    raise RuntimeError("local Mesa capture support unavailable")
MESA = importlib.util.module_from_spec(spec)
spec.loader.exec_module(MESA)
capture, digest, ensure, require_success = MESA.capture, MESA.digest, MESA.ensure, MESA.require_success

FROZEN = {
    "RasterObservation.java": MESA.FROZEN["RasterObservation.java"],
    "NormalConversionReceiptObservation.java": "85a90d50e98f90bc71fe1051420ea63bef02a34463acebd5373ab4b7db40e59c",
    "NormalListConversionReceiptObservation.java": "f2e97431cdb3175d09187914493027d7df9017c18b7364a98891e81e7af7f848",
    "MaterialObservation.java": "543fb3e37a2bc43a53fe74844fd2e72d36027be75fb5e178854185bad76c9360",
}
PROBES = (
    ("array", "NormalConversionReceiptObservation", "capture_conversion_receipts.py", 264,
     "Original normal conversion receipts: 256 decode and 8 precision rows; cleanup-complete; network=0",
     "normal-conversion-receipts.json"),
    ("list", "NormalListConversionReceiptObservation", "capture_list_conversion_receipts.py", 518,
     "Original normal list conversion: 256 values; 6 state receipts; cleanup-complete; network=0",
     "normal-list-conversion-receipts.json"),
    ("material", "MaterialObservation", "capture_material.py", 16,
     "Original material controls: 16 rows; cleanup-complete; network=0", "material.json"),
)


def headers(text, kind, driver):
    prefixes = {
        "array": ("CONVERSION_DRIVER", "CONVERSION_FORMAT", "NORMAL_ARRAY_LAYOUT"),
        "list": ("NORMAL_LIST_DRIVER", "NORMAL_LIST_FORMAT", "NORMAL_LIST_LAYOUT", "NORMAL_LIST_CAPTURE",
                 "NORMAL_LIST_STATE_LAYOUT", "NORMAL_LIST_STATE_STORAGE"),
        "material": ("MATERIAL_DRIVER",),
    }[kind]
    result = {}
    for prefix in prefixes:
        found = [line for line in text.splitlines() if line.startswith(prefix + " ")]
        ensure(len(found) == 1, "single actual " + prefix + " receipt")
        result[prefix] = found[0]
    ensure(result[prefixes[0]] == prefixes[0] + " " + driver, "actual complete expected Mesa driver")
    if kind == "array":
        ensure(result["CONVERSION_FORMAT"] == "CONVERSION_FORMAT RGBA32F vertex_clamp=1 fragment_clamp=0 read_clamp=0" and
               result["NORMAL_ARRAY_LAYOUT"] == "NORMAL_ARRAY_LAYOUT type=5120 stride=32 buffer_binding=0",
               "actual frozen floating-lighting and direct-byte-array profile")
    elif kind == "list":
        layout = ("normal_type=5120 normal_stride=32 normal_buffer=0 vertex_type=5126 "
                  "vertex_size=3 vertex_stride=32 vertex_buffer=0")
        ensure(result["NORMAL_LIST_LAYOUT"] == "NORMAL_LIST_LAYOUT " + layout and
               result["NORMAL_LIST_STATE_LAYOUT"] == "NORMAL_LIST_STATE_LAYOUT color_type=5121 "
               "color_size=4 color_stride=32 color_buffer=0 " + layout, "actual captured list/client layouts")
        ensure(result["NORMAL_LIST_CAPTURE"] == "NORMAL_LIST_CAPTURE mode=4864 compile_program=0 draw_mode=4 "
               "vertices=3 feedback_components=9 sentinel_components=7 lighting=1 normalize=0 rescale=0",
               "actual frozen list/feedback state")
        ensure(re.fullmatch(r"NORMAL_LIST_FORMAT 8 8 8 8 [1-9][0-9]* [0-9]+", result["NORMAL_LIST_FORMAT"]),
               "actual RGBA8/depth/stencil context")
        storage = result["NORMAL_LIST_STATE_STORAGE"].split()
        ensure(len(storage) == 3 and storage[1] != storage[2] and
               all(re.fullmatch(r"[0-9a-f]{64}", value) for value in storage[1:]),
               "actual distinct state-control source and poisoned storage")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True,
                        help="pinned unused NW-type dependency of the unchanged array support helper")
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--native-archive", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--expected-driver", default=MESA.DEFAULT_DRIVER)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(sys.platform.startswith("linux") and os.pathsep == ":", "declared Linux native host")
    ensure(args.java is not None and args.javac is not None, "local matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jdk_lib = (java.parent.parent / "lib").resolve(strict=True)
    jdk_natives = {name: (jdk_lib / ("lib" + name + ".so")).resolve(strict=True) for name in MESA.JDK_NAMES}
    ensure(all(path.parent == jdk_lib for path in jdk_natives.values()), "exact JDK native containment")
    for name, pin in FROZEN.items():
        ensure(MESA.sha((HERE / name).read_bytes().replace(b"\r\n", b"\n")) == pin,
               "frozen complete diagnostic/support source: " + name)
    parsers = {kind: MESA.load("mesa_attribute_" + kind, HERE / path) for kind, _, path, _, _, _ in PROBES}
    # The frozen array parser imports its precision-row parser by module name.
    # Bind that exact sibling explicitly, avoiding ambient same-name modules.
    precision = MESA.load("mesa_attribute_precision_rows", HERE / "capture_conversion.py")
    existing = sys.modules.get("capture_conversion")
    ensure(existing is None or Path(existing.__file__).resolve() == (HERE / "capture_conversion.py").resolve(),
           "no unrelated precision-row parser binding")
    sys.modules["capture_conversion"] = precision
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in parsers["array"].LIBRARIES]
    ensure(digest(jar) == parsers["array"].JAR and
           all(digest(path) == pin for path, pin in zip(libraries, parsers["array"].LIBRARIES.values())),
           "unchanged original type dependency and exact SDK archives")
    metadata = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(metadata["client_jar_sha256"] == digest(jar), "actual original class-pin archive")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in metadata["classes"].items():
            actual = MESA.CLASSFILE.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "unchanged complete original class and selected Code: " + name)
    archive = args.native_archive.resolve(strict=True)
    ensure(digest(archive) == MESA.NATIVE_ARCHIVE, "exact pinned Linux native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    natives, sources = output / "natives", output / "sources"
    natives.mkdir(); sources.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / "liblwjgl64.so").write_bytes(source.read("liblwjgl64.so"))
    ensure(digest(natives / "liblwjgl64.so") == MESA.NATIVE, "exact pinned Linux native bytes")
    generated = {name: sources / name for name in FROZEN}
    audit = {"recipe": "Reuse capture_mesa.generated_source: one exact native guard replacement; "
                       "all other bytes protected and reversal byte-identical before compilation.",
             "original_guard_lf": MESA.OLD_GUARD, "linux_guard_lf": MESA.LINUX_GUARD,
             "sources": [MESA.generated_source(HERE / name, path) for name, path in generated.items()]}
    audit_path = output / "source-transformations.json"
    audit_path.write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8", newline="\n")
    inputs = [java, javac, *jdk_natives.values(), jar, *libraries, archive, natives / "liblwjgl64.so",
              *(HERE / name for name in FROZEN), *generated.values(), audit_path,
              HERE / "capture_mesa_attributes.py", HERE / "capture_mesa.py", HERE / "source-pins.json",
              *(HERE / probe[2] for probe in PROBES), HERE / "capture_conversion.py",
              MESA.SUPPORT / "capture.py", MESA.SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before, "probes": [], "complete_inventory": False,
              "expected_driver": args.expected_driver, "source_transformations": audit,
              "jdk_native_inputs": {name: {"path": str(path), "sha256": digest(path)}
                                    for name, path in jdk_natives.items()},
              "scope": "Independent actual Mesa observations, never native expected words or a fitted decoder. "
                       "Frozen array256 TF/current observations plus eight existing precision controls; list256 "
                       "TF/current/source/poison/replay observations plus six queried states; material16 queried states. "
                       "OriginalJar is only the unchanged array support helper's unused NW-type compile/runtime "
                       "dependency; list/material have SDK-only classpaths. No game-body invocation qualification. "
                       "The frozen array helper's labelled formula comparison column remains authored metadata; "
                       "it supplies no expected result or new conversion model."}
    try:
        report["tools"] = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, label, 10, environment)
            report["tools"].append(receipt); require_success(receipt)
            version = (output / receipt["stdout_file"]).read_text() + (output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "JDK17 required")
        for kind, main_class, _, count, marker, filename in PROBES:
            directory = output / kind
            directory.mkdir(); classes = directory / "classes"; classes.mkdir()
            source_names = [main_class + ".java"]
            if kind == "array":
                source_names.insert(0, "RasterObservation.java")
            cp = os.pathsep.join(str(path) for path in
                                 (classes, *([jar] if kind == "array" else []), *libraries))
            result = {"name": kind, "status": "FAIL", "classpath": cp}
            report["probes"].append(result)
            try:
                receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                                   *(str(generated[name]) for name in source_names)], directory, "compile", 30, environment)
                result["compile"] = receipt; require_success(receipt, empty_stderr=True)
                helper = {path.name: digest(path) for path in classes.rglob("*.class")}
                allowed = {Path(name).stem + suffix for name in source_names for suffix in (".class", "$1.class")}
                ensure(set(helper) == allowed and all(path.parent == classes for path in classes.rglob("*.class")),
                       "only exact authored helper classes, no original class replacement")
                result["helper_before_sha256"] = helper
                receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                                   "-Dorg.lwjgl.librarypath=" + str(natives), "-cp", cp, main_class, str(natives)],
                                  directory, "driver", 60, environment, directory)
                result["capture"] = receipt
                text = (directory / receipt["stdout_file"]).read_text(encoding="utf-8")
                links = MESA.native_receipts(text, result, natives, jdk_natives)
                require_success(receipt)
                ensure(links == [str(natives / "liblwjgl64.so")] and text.splitlines().count(marker) == 1,
                       "single exact LWJGL link and complete unchanged controls/cleanup/network0")
                observed_headers = headers(text, kind, args.expected_driver)
                observed = parsers[kind].rows(text)
                ensure(len(observed) == count, "complete frozen diagnostic inventory")
                fixture = output / filename
                fixture.write_text(json.dumps({"headers": observed_headers, "rows": observed}, indent=2) + "\n",
                                   encoding="utf-8", newline="\n")
                result.update(status="PASS", rows=count, headers=observed_headers,
                              fixture_file=filename, fixture_sha256=digest(fixture))
            except BaseException as error:
                result["error"] = {"type": type(error).__name__, "message": str(error)}
                raise
            finally:
                result["helper_after_sha256"] = {path.name: digest(path) for path in classes.rglob("*.class")}
                if "helper_before_sha256" in result and result["helper_before_sha256"] != result["helper_after_sha256"]:
                    result["status"] = "FAIL"
                (directory / "capture.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
                ensure("helper_before_sha256" not in result or
                       result["helper_before_sha256"] == result["helper_after_sha256"], "compiled helper bytes unchanged")
        ensure(sum(probe["rows"] for probe in report["probes"]) == 798, "all three complete diagnostic inventories")
        report.update(status="PASS", complete_inventory=True, rows=798, probes_completed=3)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"; report["complete_inventory"] = False
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "original/generated source/helper support/JDK/SDK/native/Jar inputs unchanged")
    print("Mesa attribute diagnostics: array256+precision8, list256+storage256+state6, material16; " +
          str(output / "capture.json"))


if __name__ == "__main__":
    main()
