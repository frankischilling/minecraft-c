"""Observe unchanged GuiIngame HUD with prepared cached textures and actual empty inventory."""
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

spec = importlib.util.spec_from_file_location("hud_sdk_pins", HERE.parent / "controller_tick/capture_reference.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def observations(text):
    rows = [row for row in text.splitlines() if row.startswith("HUD_STATE ")]
    words = [row for row in text.splitlines() if row.startswith("HUD_WORDS ")]
    pixels = [row for row in text.splitlines() if row.startswith("HUD_PIXELS ")]
    ensure(len(rows) == len(words) == len(pixels) == 26, "HUD row count differs")
    ids = [(size,health,partial) for fancy in range(2) for size in range(2) for health in range(3) for partial in range(2)] + [(0,0,0),(1,0,0)]
    geometry = []
    for index, (row, payload, pixel, identity) in enumerate(zip(rows, words, pixels, ids)):
        cells = row.split()
        ensure(len(cells) == 63 and tuple(map(int,cells[1:5])) == (index,*identity), "HUD state row identity differs")
        expected_c = "3f800000" if index < 12 else "3f40a3d7" if index < 24 else "3f404f30"
        ensure(cells[5:7] == ["c2b40000",expected_c] and cells[8:12] == ["4","1","1","5888"], "HUD source scalar/stack differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[12:48]), "HUD float encoding differs")
        tick = 17 if identity[0] == 0 else -2147483647
        product = ((tick*312871+2**31) % 2**32) - 2**31
        seed = (product ^ 0x5deece66d) & ((1<<48)-1)
        if identity[1] == 1:
            for _ in range(10): seed = (seed * 0x5deece66d + 11) & ((1<<48)-1)
        ensure(int(cells[7]) == seed, "actual HUD Random cursor differs")
        ensure(cells[48:] == ["1","1","1","0","0","0","0","0","0","0","0","1","770","771","48"], "HUD final source GL/NW state differs")
        parts = payload.split()
        ensure(len(parts) == 5 and parts[:4] == ["HUD_WORDS",str(index),"5","6"] and re.fullmatch(r"[0-9a-f]{240}", parts[4]), "HUD final NW payload differs")
        geometry.append(f"HUD_GEOMETRY {index} 5 6 {hashlib.sha256(bytes.fromhex(parts[4])).hexdigest()}")
        parts = pixel.split()
        ensure(len(parts) == 6 and parts[:4] == ["HUD_PIXELS",str(index),"320","240"] and 0 < int(parts[4]) <= 76800 and re.fullmatch(r"[0-9a-f]{64}",parts[5]), "HUD pixel receipt differs")
    inputs = [row for row in text.splitlines() if row.startswith("HUD_INPUTS ")]
    ensure(len(inputs) == 26, "HUD input row count differs")
    for index,row in enumerate(inputs):
        ensure(row.split() == ["HUD_INPUTS",str(index),str(min(1,index//12)),
                               "00000000" if index < 24 else "3e044444",
                               str(0 if index < 12 else 11)], "HUD actual brightness inputs differ")
    lights = [row for row in text.splitlines() if row.startswith("HUD_LIGHT ")]
    ensure(lights == [f"HUD_LIGHT {index} 8 120 8 1 3e044444" for index in (24,25)],
           "HUD actual in-height light rows differ")
    return rows,geometry,pixels,inputs,lights


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
              natives / "lwjgl64.dll", HERE / "HudObservation.java", Path(__file__),
              HERE / "source-pins.json", HERE / "operation-map.json", SUPPORT / "capture.py",
              SUPPORT / "classfile.py", HERE.parent / "controller_tick/capture_reference.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Twenty-six unchanged UQ.a(FZII) complete draws. Genuine original UQ/controller/in-memory World/Player constructors and all reached GUI helpers; actual unchanged PX.b() projection; prepared App/PX/Engine cache-hit fields, authored gui/icons/vignette textures, no FontRenderer calls for empty chat/debug-off/record-off. Source GUI scalar sizes differ from fixed actual Display/viewport320x240. Exact final source Random seed/state and final NW geometry, plus reference-only actual authored-texture pixel digests. Two appended in-height brightness paths use real bounds/light queries. No arbitrary Item rendering, optional pumpkin/portal/chat/debug/sleep overlay, constructor parity for prepared owners or original gameplay asset/pixel qualification."}
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
                           "-d", str(classes), str(HERE / "HudObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"HudObservation.class", "HudObservation$1.class"}, "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "--add-opens=java.base/java.util=ALL-UNNAMED", "-Djava.security.manager=allow", "-Dorg.lwjgl.librarypath=" + str(natives.resolve()),
                           "-classpath", classpath, "HudObservation", str(args.client_jar.resolve()),
                           str(natives.resolve())], args.output, "original", 120, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original HUD: 26 rows; network=0; display=1" in text.splitlines(),
               "original completion receipt missing")
        rows, geometry, pixels, inputs_observed, lights = observations(text)
        profiles = [line for line in text.splitlines() if line.startswith(("GL_VENDOR ", "GL_RENDERER ", "GL_VERSION "))]
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(profiles) == 3 and len(links) == 1 and Path(links[0]).resolve() ==
               (natives / "lwjgl64.dll").resolve(), "device/native profile differs")
        fixture = {"client_jar_sha256": sdk.JAR, "state_rows": rows, "geometry_rows": geometry, "pixel_rows": pixels, "input_rows": inputs_observed, "light_rows": lights, "pixel_scope": "Reference-only actual authored-texture output; no native/original asset pixel parity claim."}
        (args.output / "draw.json").write_text(json.dumps(fixture, indent=2) + "\n", newline="\n")
        report.update(status="PASS", state_rows=26, geometry_rows=26, pixel_rows=26, original=receipt,
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
    print("Original HUD: 26 rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
