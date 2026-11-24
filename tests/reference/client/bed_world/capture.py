"""Capture unchanged original type14 rendering in two bounded CPU-only VMs."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success
from packet import SOURCES, rows, verify_source_pins


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-stdout", type=Path)
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    support = module("bed_original_sdk", HERE.parent / "picking/observe.py")
    inputs = module("bed_original_inputs", ROOT / "tests/tools/client/test_picking.py")
    jar = ROOT / "b1.7.3.jar"
    ensure(digest(jar) == support.JAR_SHA, "unchanged original archive")
    verify_source_pins(jar)
    libraries = [Path(os.environ["APPDATA"]) / ".minecraft/libraries" / name
                 for name in support.LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, support.LIBRARIES.values())),
           "exact reference SDK hashes")
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ("java", "javac")]
    helper = HERE / "BedObservation.java"
    paths = [jar, *libraries, java, javac, *[HERE / name for name in SOURCES]]
    if args.native_stdout is not None:
        paths.append(args.native_stdout.resolve(strict=True))
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "scope": "unchanged CV.b/q,VE textures,JJ arrays,EW,NW CPU words",
              "input_before_sha256": before}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    try:
        classes = output / "classes"
        classes.mkdir()
        classpath = os.pathsep.join(map(str, (classes, jar, *libraries)))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-classpath", classpath,
                           "-d", str(classes), str(helper)], output, "compile", 60, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        compiled = {path.name: digest(path) for path in sorted(classes.glob("*.class"))}
        ensure(set(compiled) == {"BedObservation.class", "BedObservation$1.class"},
               "only authored observer classes compiled")
        report["helper_before_sha256"] = compiled
        observed = []
        for index in range(2):
            work = output / ("world-" + str(index))
            inputs.authored_world(work)
            receipt = capture([str(java), "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                               "-classpath", classpath, "BedObservation", str(jar), str(work)],
                              output, "original-" + str(index), 90, environment, work)
            report.setdefault("original", []).append(receipt)
            require_success(receipt)
            text = (output / receipt["stdout_file"]).read_text(encoding="ascii")
            ensure(text.splitlines()[-1] == "BED_COMPLETE calls=192 network=0 native=0 exec=0",
                   "actual original completion and effect guards")
            observed.append(rows(text))
        ensure(observed[0] == observed[1], "both original VMs agree")
        selected = output / "selected.txt"
        selected.write_text("\n".join(observed[0]) + "\n", encoding="ascii", newline="\n")
        report["selected_sha256"] = digest(selected)
        report["calls_per_vm"] = 192
        report["helper_after_sha256"] = {path.name: digest(path) for path in sorted(classes.glob("*.class"))}
        ensure(report["helper_after_sha256"] == compiled, "compiled observer unchanged")
        if args.native_stdout is not None:
            ensure(rows(args.native_stdout.read_text(encoding="ascii")) == observed[0],
                   "all native NW rows match both original VMs")
            report["native_rows_compared"] = 192
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        if report["input_after_sha256"] != before:
            report["status"] = "FAIL"
            report["input_changed"] = True
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    ensure(report["status"] == "PASS", "Bed capture input integrity")
    print("Bed:192 exact rows in two original VMs; " + str(output / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
