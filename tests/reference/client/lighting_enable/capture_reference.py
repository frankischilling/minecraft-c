"""Capture unchanged U.b state and real light parameters on a Windows device."""

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
from classfile import inspect_class
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import JAR, LIBRARIES

NATIVE_ARCHIVE = "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
NATIVE_SHA256 = "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43"


def words(text, count, width=8):
    return re.fullmatch(r"[0-9a-f]{" + str(width) + r"}(?:,[0-9a-f]{" +
                        str(width) + r"}){" + str(count - 1) + "}", text) is not None


def rows(text, case):
    """Require the complete fixed observation inventory, including vector identities."""
    name = case["name"]
    found = [line for line in text.splitlines() if line.startswith((
        "LIGHT_FAIL ", "LIGHT_STATE ", "LIGHT_BUFFER ", "LIGHT_POOL ", "LIGHT_VECTOR ",
        "LIGHT_PARAMETER ", "LIGHT_MODEL ", "LIGHT_MATRICES "))]
    if not case["device"]:
        ensure(len(found) == 1, "one complete failed-first-GL observation")
        value = found[0].split()
        ensure(len(value) == 7 and value[:6] == ["LIGHT_FAIL", name, "1", "0", "16", "16"] and
               words(value[6], 16), "same initialized U buffer survives both failures")
        ensure(value[6] == ",".join(["00000000"] * 16), "failed GL child did not reach buffer fills")
        return found
    ensure(len(found) == 17 * case["calls"], "all 17 rows per whole U.b invocation")
    for call in range(case["calls"]):
        group = found[call * 17:(call + 1) * 17]
        tag = [name, str(call)]
        state = group[0].split()
        ensure(state == ["LIGHT_STATE", *tag, "1", "1", "1", "1", "1032", "5634", "7424",
                         "5889" if case["transformed"] else "5888"],
               "actual enabled lights/material/flat shade and unchanged selected matrix")
        buffer = group[1].split()
        same = "1" if case["seeded"] or call != 0 else "-1"
        ensure(len(buffer) == 8 and buffer[:7] == ["LIGHT_BUFFER", *tag, same, "0", "4", "16"] and
               words(buffer[7], 16), "actual same U buffer/cursors/full capacity")
        cursor = (2 if case["seeded"] else 0) + 4 * call
        ensure(group[2].split() == ["LIGHT_POOL", *tag, str(cursor), str(cursor + 4),
                                    str(cursor), str(cursor + 4), "1", "1"],
               "four real pooled vectors with stable List and earlier objects")
        for index, line in enumerate(group[3:7]):
            vector = line.split()
            ensure(len(vector) == 7 and vector[:4] == ["LIGHT_VECTOR", *tag, str(index)] and
                   all(re.fullmatch(r"[0-9a-f]{16}", value) for value in vector[4:]),
                   "actual pooled vector coordinates")
        for index, line in enumerate(group[7:15]):
            parameter = line.split()
            ensure(len(parameter) == 6 and parameter[:5] == ["LIGHT_PARAMETER", *tag,
                   str(index // 4), str((4611, 4609, 4608, 4610)[index % 4])] and words(parameter[5], 4),
                   "both actual lights and all four parameter vectors")
        model = group[15].split()
        ensure(len(model) == 4 and model[:3] == ["LIGHT_MODEL", *tag] and words(model[3], 4),
               "actual global light model ambient")
        matrices = group[16].split()
        ensure(len(matrices) == 7 and matrices[:3] == ["LIGHT_MATRICES", *tag] and
               all(words(value, 16) for value in matrices[3:]), "actual before/after matrix state")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "declared pinned LWJGL device profile is Windows")
    ensure(args.java is not None and args.javac is not None, "Java and Javac are required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True)
                   for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "matching JDK17 tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == expected
           for path, expected in zip(libraries, LIBRARIES.values())), "unchanged original archive and SDK")
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == JAR, "selected archive pin")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "complete class and selected Code authority: " + name)
    native_archive = (args.library_root / NATIVE_ARCHIVE).resolve(strict=True)
    ensure(digest(native_archive) == NATIVE_SHA256, "pinned Windows native archive")
    cases = json.loads((HERE / "inputs.json").read_text(encoding="utf-8"))["cases"]
    ensure([case["name"] for case in cases] == ["cold_no_context", "identity_cold", "seeded_projection",
           "seeded_replaced_modelview", "seeded_warm"], "declared finite profile inventory")
    args.output.mkdir(parents=True, exist_ok=False)
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "lwjgl64.dll").write_bytes(archive.read("lwjgl64.dll"))
    tsv = args.output / "inputs.tsv"
    tsv.write_text("\n".join("\t".join([case["name"], str(int(case["device"])),
                   str(int(case["seeded"])), str(int(case["transformed"])),
                   str(int(case["replace_modelview"])), str(case["calls"])]) for case in cases) + "\n",
                   encoding="utf-8", newline="\n")
    inputs = [java, javac, jar, *libraries, native_archive, natives / "lwjgl64.dll", tsv,
              HERE / "LightingObservation.java", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "capture_reference.py", SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Five fresh unchanged U.b profiles: two first-child failures with no context, "
                       "and five normal whole calls on actual GL. Exact Java buffer/pool state and "
                       "driver-exposed light/matrix parameter bits only; no geometry, pixels or lit draw."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "LightingObservation.java")], args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(helper) == {"LightingObservation.class", "LightingObservation$1.class"}, "authored helper inventory")
        report["helper_before_sha256"] = helper
        observed = []
        for index, case in enumerate(cases):
            receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                               "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-cp", cp,
                               "LightingObservation", str(jar), str(natives.resolve()), str(tsv.resolve()), str(index)],
                              args.output, "original" + str(index), 60, environment, args.output.resolve())
            profile = {"name": case["name"], "capture": receipt}
            report["profiles"].append(profile)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text()
            ensure("Original lighting: " + case["name"] + "; network=0" in text.splitlines(),
                   "normal helper return after actual context cleanup/no networking")
            links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
            ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(),
                   "only declared genuine native link")
            profile["driver"] = [line for line in text.splitlines() if line.startswith("LIGHT_DRIVER ")]
            profile["format"] = [line for line in text.splitlines() if line.startswith("LIGHT_FORMAT ")]
            profile["exceptions"] = [line for line in text.splitlines() if line.startswith("LIGHT_EXCEPTION ")]
            ensure(len(profile["driver"]) == int(case["device"]) and
                   len(profile["format"]) == int(case["device"]) and
                   len(profile["exceptions"]) == int(not case["device"]), "actual device/failure receipt inventory")
            actual = rows(text, case)
            profile.update(status="PASS", rows=len(actual))
            observed.extend(actual)
        ensure(len(observed) == 86, "one failure row plus five whole 17-row device calls")
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=86, fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "archive/native/runtime/helper/input bytes unchanged")
    print("Original U.b lighting: 86 state/parameter rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
