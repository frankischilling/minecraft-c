"""Capture original position arithmetic with explicit raw-bit inputs."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

from capture import capture, digest, ensure, require_success, validate_native_result
from classfile import inspect_class

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
NATIVE = "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"


def rows(text):
    result = [line for line in text.splitlines() if line.startswith("POSITION ")]
    ensure(len(result) == 36, "position row count differs")
    for index, line in enumerate(result):
        cells = line.split()
        ensure(cells[:2] == ["POSITION", str(index)] and len(cells) == 11,
               "position row inventory differs")
        ensure(all(re.fullmatch("[0-9a-f]{16}", value) for value in cells[2:]), "position bits malformed")
    return result


def main(arguments=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--natives", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-stdout", type=Path)
    parser.add_argument("--native-report", type=Path)
    options = parser.parse_args(arguments)
    ensure((options.native_stdout is None) == (options.native_report is None),
           "--native-stdout and --native-report must be supplied together")
    native_completion = (validate_native_result(options.native_stdout, options.native_report, "position-probe")
                         if options.native_stdout is not None else None)
    ensure(os.name == "nt", "Original application profile requires qualified Windows x64")
    options.output.mkdir(parents=True, exist_ok=False)
    java = Path(shutil.which(options.java) or "").resolve(strict=True)
    javac = Path(shutil.which(options.javac) or "").resolve(strict=True)
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "matching JDK required")
    libraries = [options.libraries / name for name in LIBRARIES]
    ensure(digest(options.client_jar) == CLIENT, "original JAR differs")
    ensure(all(digest(path) == LIBRARIES[path.name] for path in libraries), "library JAR differs")
    ensure(digest(options.natives / "lwjgl64.dll") == NATIVE, "Windows native differs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    ensure(pins["client_jar_sha256"] == CLIENT, "pin authority differs")
    with zipfile.ZipFile(options.client_jar) as archive:
        for name, expected in pins["classes"].items():
            ensure(inspect_class(archive.read(name.replace(".", "/") + ".class")) == expected,
                   "class or method Code differs: " + name)
    for name in ["dc", "gs", "ls"]:
        ensure(not any(method["name"] == "e" and method["descriptor"] == "(DDD)V"
                       for method in pins["classes"][name]["methods"]), "foreign position override")
    specification = json.loads((HERE / "vectors.json").read_text())
    vector_text = "".join(" ".join([str(value["id"]), *[value[name] for name in
                         ["x", "y", "z", "width", "height", "offset", "y_size"]]]) + "\n"
                          for value in specification["vectors"])
    ensure(len(specification["vectors"]) == 36 and (HERE / "vectors.txt").read_text() == vector_text,
           "source vector representations disagree")
    inputs = [java, javac, options.client_jar, *libraries, options.natives / "lwjgl64.dll",
              *[HERE / name for name in ["PositionObservation.java", "vectors.json", "vectors.txt",
                                        "source-pins.json", "capture.py", "classfile.py", "capture_position.py"]]]
    if native_completion is not None:
        inputs.extend(Path(name) for name in native_completion["inputs"])
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Actual inherited sn.e(DDD) arithmetic on genuine constructed dc; controlled source fields; no general gameplay or constructor-entropy claim"}
    report["native_completion"] = native_completion
    classes = options.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ["JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"]:
        environment.pop(name, None)
    try:
        versions = []
        for label, tool in [("java", java), ("javac", javac)]:
            result = capture([str(tool), "-version"], options.output, label + "-version", 10, environment)
            require_success(result)
            versions.append(((options.output / result["stdout_file"]).read_bytes() +
                             (options.output / result["stderr_file"]).read_bytes()).decode())
        runtime = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 tools required")
        report["jdk"] = versions
        classpath = os.pathsep.join(str(path.resolve()) for path in [classes, options.client_jar, *libraries])
        result = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath, "-d", str(classes),
                          str(HERE / "PositionObservation.java")], options.output, "compile", 30, environment)
        require_success(result, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"PositionObservation.class"}, "unexpected helper class")
        report["helper_before_sha256"] = helper
        result = capture([str(java), "-Xint", "-Xverify:all", "-Xmx1g",
                          "-Xlog:class+init=info:file=class-init.log::filesize=1m,filecount=1",
                          "-Dorg.lwjgl.librarypath=" + str(options.natives.resolve()), "-classpath", classpath,
                          "PositionObservation", str((options.output / "worlds").resolve()),
                          str(options.client_jar.resolve()), str((HERE / "vectors.txt").resolve())],
                         options.output, "original", 120, environment, options.output.resolve())
        require_success(result, empty_stderr=True)
        report["original"] = result
        log = options.output / "class-init.log"
        ensure(log.is_file() and log.stat().st_size < 1024 * 1024 and
               len(list(options.output.glob("class-init.log*"))) == 1, "initialization log exceeded bound")
        ensure(not re.search(r"Initializing 'org/lwjgl/(?:opengl/Display|openal/AL)'", log.read_text()),
               "unexpected Display/OpenAL initialization")
        report["class_initialization_log_sha256"] = digest(log)
        text = (options.output / "original.stdout").read_text()
        ensure(text.splitlines()[-1] == "Original Entity position: 36 states", "original terminal marker missing")
        origins = [line[7:] for line in text.splitlines() if line.startswith("ORIGIN ")]
        ensure(len(origins) == 12 and len(set(origins)) == 12 and set(origins) == set(pins["classes"]),
               "runtime class origins differ")
        observed = rows(text)
        (options.output / "rows.txt").write_text("\n".join(observed) + "\n", encoding="utf-8", newline="\n")
        if options.native_stdout:
            native = rows(options.native_stdout.read_text())
            ensure(observed == native, {"first_difference": next(
                ((index, left, right) for index, (left, right) in enumerate(zip(observed, native))
                 if left != right), None)})
        report.update(status="PASS", records=36, native_compared=options.native_stdout is not None,
                      original_origins=origins, classes=len(pins["classes"]),
                      methods=sum(len(value["methods"]) for value in pins["classes"].values()),
                      code_bodies=sum(sum("code_sha256" in method for method in value["methods"])
                                      for value in pins["classes"].values()))
    finally:
        after = {str(path.resolve()): digest(path) for path in inputs}
        report["input_after_sha256"] = after
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path)
                                         for path in classes.rglob("*.class")}
        same = before == after and ("helper_before_sha256" not in report or
                                    report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report.update(status="FAIL", input_changed=True)
        (options.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "source/runtime/helper inputs changed")
    print("Original position arithmetic: 36 exact records; native_compared=" +
          str(report["native_compared"]) + "; report=" + str((options.output / "capture.json").resolve()))


if __name__ == "__main__":
    main()
