"""Execute authored CPU observers on the supplied unchanged verified client."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from capture import capture, ensure, require_success

HERE = Path(__file__).resolve().parent
JAR_SHA256 = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def executable(value):
    found = shutil.which(value) if value else None
    ensure(found, "JDK executable missing: " + str(value))
    return Path(found).resolve(strict=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--work-dir", type=Path)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    args = parser.parse_args()
    ensure(sha(args.client_jar) == JAR_SHA256, "client JAR pin differs")
    libraries = [args.libraries / name for name in LIBRARIES]
    ensure(all(sha(path) == LIBRARIES[path.name] for path in libraries), "library pin differs")
    if args.work_dir is not None:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        args.output = Path(tempfile.mkdtemp(prefix="original-material-movement-", dir=args.work_dir))
    else:
        args.output.mkdir(parents=True, exist_ok=False)
    classes = args.output / "observer-classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ["JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"]:
        environment.pop(name, None)
    java = executable(args.java)
    javac = executable(args.javac)
    ensure(java.parent.samefile(javac.parent), "java and javac must resolve to the same JDK bin directory")
    version = capture([str(java), "-version"], args.output, "java-version", 10, environment)
    compiler_version = capture([str(javac), "-version"], args.output, "javac-version", 10, environment)
    require_success(version)
    require_success(compiler_version)
    version_text = ((args.output / version["stdout_file"]).read_bytes() +
                    (args.output / version["stderr_file"]).read_bytes()).decode(errors="replace")
    compiler_text = ((args.output / compiler_version["stdout_file"]).read_bytes() +
                     (args.output / compiler_version["stderr_file"]).read_bytes()).decode(errors="replace")
    vm_match = re.search(r'version "(17\.[0-9]+\.[0-9]+)', version_text)
    compiler_match = re.search(r'javac (17\.[0-9]+\.[0-9]+)', compiler_text)
    ensure(vm_match and compiler_match and vm_match.group(1) == compiler_match.group(1), "matching JDK17 runtime/compiler required")
    sources = [HERE / "MaterialObservation.java"]
    classpath = os.pathsep.join(str(p.resolve()) for p in [classes, args.client_jar] + libraries)
    compiled = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath, "-d", str(classes)] +
                       [str(p) for p in sources], args.output, "compile", 30, environment)
    require_success(compiled, empty_stderr=True)
    helpers = {str(p.relative_to(classes)): sha(p) for p in classes.rglob("*.class")}
    ensure(set(helpers) == {"MaterialObservation.class"}, helpers)
    command = [str(java), "-Xint", "-Xverify:all", "-Xmx1g",
               "-Xlog:class+init=info:file=" + str((args.output / "class-init.log").resolve()),
               "-classpath", classpath, "MaterialObservation", str((args.output / "worlds").resolve())]
    run = capture(command, args.output, "original", 120, environment)
    require_success(run, empty_stderr=True)
    ensure(helpers == {str(p.relative_to(classes)): sha(p) for p in classes.rglob("*.class")}, "helper class bytes changed")
    lines = (args.output / run["stdout_file"]).read_text().splitlines()
    match = re.fullmatch(r"Original material movement: ([1-9][0-9]*) rows", lines[-1]) if lines else None
    ensure(match is not None, "original positive final marker missing")
    observations = [line for line in lines if re.match(r"^(FLOW|SCAN|EYE|VEC|PLAYER) ", line)]
    ensure(len(observations) == int(match.group(1)), "original record count differs")
    (args.output / "observations.json").write_text(json.dumps(observations, indent=2) + "\n")
    report = {**run, "command": command, "java_version": version_text,
              "javac_version": compiler_text, "matching_jdk17_version": vm_match.group(1),
              "java_executable_sha256": sha(java), "javac_executable_sha256": sha(javac),
              "helper_class_sha256": helpers,
              "helper_pin_scope": "Exact compiled authored class bytes, including all Code attributes, retained and hashed before/after execution; individual helper Code arrays not separately pinned.",
              "jar_sha256": JAR_SHA256,
              "libraries": LIBRARIES, "sources": {p.name: sha(p) for p in sources},
              "game_class_replacements": False,
              "controls": ["actual acquired Chunk block/metadata bytes prepared identically",
                           "actual Vec3 cursor reset before declared vector cases",
                           "actual Falling constructor, source position and velocity fields"]}
    (args.output / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    ensure(len(observations) > 0, report)
    print("Observed actual original material movement; report=" + str((args.output / "run.json").resolve()) +
          "; observations=" + str((args.output / "observations.json").resolve()))


if __name__ == "__main__":
    main()
