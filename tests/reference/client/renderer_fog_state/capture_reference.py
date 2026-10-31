"""Observe unchanged PX fog-state calls and separate authored native fog pixels."""
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

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def rows(text):
    observed = [row for row in text.splitlines() if row.startswith("FOG_STATE ")]
    identities = [(d,a,p,b) for d in range(2) for a in range(2)
                  for p in (-1,0,1) for b in range(4)]
    ensure(len(observed) == 48, "fog state row count differs")
    for row, identity in zip(observed,identities):
        cells=row.split()
        ensure(len(cells)==28 and tuple(map(int,cells[1:5]))==identity,
               "fog state row identity differs")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}",cell) for cell in cells[6:20]),
               "fog state float encoding differs")
        ensure(cells[-3:]==["0","4","16"], "source fog buffer cursors differ")
    return observed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "this native reference profile requires Windows")
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(p) or "").resolve(strict=True)
                   for p in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    ensure(digest(args.client_jar) == JAR, "original client JAR differs")
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "SDK metadata differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == JAR, "source pin authority differs")
    with zipfile.ZipFile(args.client_jar) as archive:
        for name, pin in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in pin["methods"]}
            actual["methods"] = [m for m in actual["methods"]
                                 if (m["name"], m["descriptor"]) in selected]
            ensure(actual == pin, "original class/method differs: " + name)
    native_archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(native_archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43",
           "SDK native archive differs")
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        payload = archive.read("lwjgl64.dll")
    (natives / "lwjgl64.dll").write_bytes(payload)
    inputs = [java, javac, args.client_jar, *libraries, native_archive, natives / "lwjgl64.dll", HERE / "FogStateObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "48 unchanged original private PX.a(IF) calls with real native GL state; "
                       "declared prepared PX/App/Settings/LocalPlayer/World fields; genuine ge.e16 "
                       "buffer and Dimension binds. Only World block/metadata transport authored "
                       "(air, water9, lava11, metadata0); original immersion and method unchanged. "
                       "Separate authored fog triangle pixel discriminant, not source sky/frame parity."}

    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        for label, tool in (("java", java), ("javac", javac)):
            version = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(version)
            version_text = ((args.output / version["stdout_file"]).read_text() +
                            (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', version_text), "JDK17 required")
            report[label + "_version"] = version_text
        classpath = os.pathsep.join(str(p.resolve()) for p in (classes, args.client_jar, *libraries))
        compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                            "-d", str(classes), str(HERE / "FogStateObservation.java")],
                           args.output, "compile", 30, environment)
        require_success(compiled, empty_stderr=True)
        helper = {p.relative_to(classes).as_posix(): digest(p) for p in classes.rglob("*.class")}
        ensure(set(helper) == {"FogStateObservation.class", "FogStateObservation$1.class", "FogStateObservation$TransportWorld.class"},
               "unexpected observer classes")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m",
                           "-Djava.security.manager=allow", "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-classpath",
                           classpath, "FogStateObservation", str(args.client_jar.resolve()), str(natives.resolve())],
                          args.output, "original", 30, environment, args.output.resolve())
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        ensure("Original fog state: 48 rows; network=0; display=1; buffer=1" in text.splitlines(),
               "original completion/guard receipt missing")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines()
                 if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(),
               "unexpected original native links")
        initial = [line for line in text.splitlines() if line.startswith("FOG_INITIAL ")]
        pixels = [line for line in text.splitlines() if line.startswith(("FOG_PIXEL ", "FOG_PIXEL_EXP ", "FOG_PIXEL_EXP2 "))]
        drivers = [line for line in text.splitlines() if line.startswith("FOG_DRIVER ")]
        profiles = [line for line in text.splitlines() if line.startswith("FOG_PIXEL_PROFILE ")]
        ensure(len(initial) == 1 and len(pixels) in (15,30) and len(drivers) == 1 and len(profiles) == 1,
               "initial NV/pixel/driver receipt missing")
        report.update(initial_nv=initial[0], pixel_rows=pixels, driver=drivers[0], pixel_profile=profiles[0])
        observed = rows(text)
        (args.output / "finite.json").write_text(json.dumps({"rows": observed, "initial_nv":initial[0],
            "pixel_rows":pixels, "driver":drivers[0], "pixel_profile":profiles[0]}, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=48, original=receipt, native_links=links,
                      fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.relative_to(classes).as_posix(): digest(p)
                                          for p in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or
            report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
        ensure(same, "observer inputs changed")
    print("Original fog state: 48 exact rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
