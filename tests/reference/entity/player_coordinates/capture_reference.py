"""Bounded unchanged-original coordinate/getter/copy/placement observations."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

from packet import SOURCES, rows, verify_source_pins

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "client/controller"))
from capture import capture, digest, ensure, require_success

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar":
        "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar":
        "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar":
        "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar":
        "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-stdout", type=Path)
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ("java", "javac")]
    jar = args.client_jar.resolve(strict=True)
    ensure(digest(jar) == JAR, "unchanged original client archive")
    verify_source_pins(jar)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           "exact existing reference SDK libraries")
    helper = HERE / "PlayerCoordinatesObservation.java"
    inputs = [java, javac, jar, *libraries, *[HERE / name for name in SOURCES]]
    if args.native_stdout is not None:
        inputs.append(args.native_stdout.resolve(strict=True))
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "scope": "unchanged original GS.Q/GS.a(BR)/SN.c(DDDFF) finite calls",
              "input_before_sha256": before}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    try:
        for name, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, name, 10, environment)
            report[name] = receipt
            require_success(receipt)
            version = "".join((output / receipt[key]).read_text() for key in
                              ("stdout_file", "stderr_file"))
            ensure(re.search(r'(?:version "|javac )17\.', version), "reference JDK17")
        classes = output / "classes"
        classes.mkdir()
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                           "-d", str(classes), str(helper)], output, "compile", 60, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper_before = {path.name: digest(path) for path in sorted(classes.glob("*.class"))}
        ensure(set(helper_before) == {"PlayerCoordinatesObservation.class",
                                     "PlayerCoordinatesObservation$Guard.class"},
               "only authored observer/guard classes compiled")
        report["helper_before_sha256"] = helper_before
        with zipfile.ZipFile(jar) as archive:
            import hashlib
            report["original_class_sha256"] = {
                name: hashlib.sha256(archive.read(name + ".class")).hexdigest()
                for name in ("dc", "gs", "ls", "sn", "br", "eq")}
        observed = []
        for index in range(2):
            work = output / ("work-" + str(index))
            work.mkdir()
            receipt = capture([str(java), "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                               "-classpath", classpath, "PlayerCoordinatesObservation", str(jar),
                               str(work)], output, "original-" + str(index), 90,
                              environment, work)
            report.setdefault("original", []).append(receipt)
            require_success(receipt)
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            ensure(text.splitlines()[-1] == "Original Player coordinates: 5 rows; "
                   "network=0; native=0; rejected_writes=0", "complete guarded original marker")
            observed.append(rows(text))
        ensure(observed[0] == observed[1], "independent VM finite rows agree")
        report["original_rows"] = observed[0]
        selected = output / "selected.txt"
        selected.write_text("\n".join(observed[0]) + "\n", encoding="ascii", newline="\n")
        report["selected_sha256"] = digest(selected)
        report["helper_after_sha256"] = {path.name: digest(path) for path in sorted(classes.glob("*.class"))}
        ensure(report["helper_after_sha256"] == helper_before, "compiled helper unchanged")
        if args.native_stdout is not None:
            ensure(rows(args.native_stdout.read_text(encoding="ascii")) == observed[0],
                   "native exact method/pose/history/bounds rows match both original VMs")
            report["native_rows_compared"] = 5
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in inputs}
        if report["input_after_sha256"] != before:
            report["status"] = "FAIL"
            report["input_changed"] = True
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    ensure(report["status"] == "PASS", "coordinate reference input integrity")
    print("Player coordinates: 5 rows in two original VMs; " + str(output / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
