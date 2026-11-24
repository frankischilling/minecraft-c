"""Bounded unchanged LWJGL2.9 ARB query calls, real GL, no game JAR/assets."""
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

LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "this captured native profile requires Windows")
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(value) or "").resolve(strict=True)
                   for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),
           "matching JDK17 tools required")
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           "unchanged SDK metadata required")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(libraries[0]) as archive:
        actual = inspect_class(archive.read("org/lwjgl/opengl/ARBOcclusionQuery.class"))
        selected = {(m["name"], m["descriptor"]) for m in pins["class"]["methods"]}
        actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
        ensure(actual == pins["class"], "unchanged actual ARB wrapper Code identities")
    native_archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(native_archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43",
           "unchanged SDK native archive required")
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "lwjgl64.dll").write_bytes(archive.read("lwjgl64.dll"))
    inputs = [java, javac, *libraries, native_archive, natives / "lwjgl64.dll",
              HERE / "QueryObservation.java", HERE / "capture_reference.py", HERE / "source-pins.json",
              SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Unchanged pinned LWJGL2.9 ARB wrappers and actual native GL; declared empty "
                       "and authored triangle queries, real result availability/counts and direct "
                       "IntBuffer cursors/guards. No original game JAR/method, N loop or World parity. "
                       "Immediate availability is preserved as observed, not assumed repeatable."}
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    try:
        for label, tool in (("java", java), ("javac", javac)):
            version = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(version)
            text = ((args.output / version["stdout_file"]).read_text() +
                    (args.output / version["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
            report[label + "_version"] = text
        classpath = os.pathsep.join(str(path.resolve()) for path in (classes, *libraries))
        compilation = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                               "-d", str(classes), str(HERE / "QueryObservation.java")],
                              args.output, "compile", 30, environment)
        require_success(compilation, empty_stderr=True)
        helpers = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helpers) == {"QueryObservation.class", "QueryObservation$1.class"},
               "unexpected observer class inventory")
        report["helper_before_sha256"] = helpers
        observation = capture([str(java), "-Xint", "-Xverify:all", "-Xmx64m",
                               "-Djava.security.manager=allow",
                               "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-classpath", classpath,
                               "QueryObservation", str(natives.resolve())], args.output, "reference", 30,
                              environment, args.output.resolve())
        require_success(observation)
        lines = (args.output / observation["stdout_file"]).read_text().splitlines()
        ensure(lines[-1] == "Original ARB queries: network=0; source_wrappers=unchanged",
               "normal bounded original completion missing")
        driver = [line for line in lines if line.startswith("QUERY_DRIVER ")]
        capability = [line for line in lines if line.startswith("QUERY_CAPABILITY ")]
        rows = [line for line in lines if line.startswith(("QUERY_EMPTY ", "QUERY_VISIBLE "))]
        links = [line.removeprefix("NATIVE_LINK ") for line in lines if line.startswith("NATIVE_LINK ")]
        ensure(len(driver) == len(capability) == len(links) == 1 and
               Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(), "driver/link receipt differs")
        ensure(capability[0] in ("QUERY_CAPABILITY 0", "QUERY_CAPABILITY 1") and
               len(rows) == (2 if capability[0].endswith("1") else 0), "conditional query row inventory")
        if rows:
            empty, visible = [line.split() for line in rows]
            ensure(empty[1] in ("0", "1") and empty[2:] == ["0", "1", "2", "1"] and
                   visible[1] in ("0", "1") and visible[4:] == ["1", "2", "1"],
                   "buffer/result/cursor fields differ")
            samples, bits = map(int, visible[2:4])
            ensure(bits >= 0 and (samples == 0 if bits == 0 else samples > 0), "counter/result profile differs")
        (args.output / "finite.json").write_text(json.dumps({"driver": driver[0], "capability": capability[0],
            "rows": rows}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", observation=observation, native_links=links, rows=rows,
                      fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path)
                                          for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or
            report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
        ensure(same, "query observer inputs changed")
    print("Actual LWJGL ARB query capture PASS; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
