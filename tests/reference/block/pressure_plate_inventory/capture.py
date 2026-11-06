"""Observe genuine original pressure plate scalar leaves; no display or GL."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module("pressure_plate_capture_process", ROOT / "tests/reference/client/controller/capture.py")
I = module("pressure_plate_classfile", ROOT / "tests/reference/client/controller/classfile.py")


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def successful(report):
    C.ensure(report["exit"] == 0 and not report["timed_out"] and
             report["pipe_capture_complete"] and not report["reader_errors"] and
             not report["output_overflow"], "complete successful original process")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jar", type=Path, default=ROOT / "b1.7.3.jar")
    parser.add_argument("--jdk", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / "classes"
    classes.mkdir()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, "verified untouched original client JAR")
    jdk = args.jdk.resolve(strict=True) if args.jdk else Path(shutil.which("java")).resolve().parents[1]
    suffix = ".exe" if os.name == "nt" else ""
    java, javac = [jdk / "bin" / (name + suffix) for name in ("java", "javac")]
    helpers = [HERE / name for name in ("PressurePlateInventoryObservation.java", "capture.py")]
    files = [jar, java, javac, jdk / "lib/modules", *helpers,
             ROOT / "tests/reference/client/controller/capture.py",
             ROOT / "tests/reference/client/controller/classfile.py"]
    files += list((jdk / "bin").glob("*.dll")) + list((jdk / "bin/server").glob("*.dll"))
    before = {str(path.resolve()): C.digest(path) for path in files}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    compile_report = C.capture([javac, "-proc:none", "-encoding", "UTF-8", "-d", classes,
                                helpers[0]], output, "compile", 60, environment)
    successful(compile_report)
    class_inputs = {str(path.resolve()): C.digest(path) for path in classes.glob("*.class")}
    command = [java, "-Xint", "-Xverify:all", "-Djava.awt.headless=true", "-cp", classes,
               "PressurePlateInventoryObservation", jar]
    runs = []
    for stem in ("original-a", "original-b"):
        report = C.capture(command, output, stem, 60, environment)
        successful(report)
        C.ensure(report["stderr_bytes"] == 0, "no original stderr or caught source exception")
        runs.append(report)
    a = (output / "original-a.stdout").read_bytes()
    b = (output / "original-b.stdout").read_bytes()
    C.ensure(a == b, "two fresh ordinary original VMs agree byte-for-byte")
    after = {str(path.resolve()): C.digest(path) for path in files}
    after_classes = {str(path.resolve()): C.digest(path) for path in classes.glob("*.class")}
    C.ensure(before == after and class_inputs == after_classes, "selected original inputs unchanged")
    lines = a.decode("utf-8").splitlines()
    C.ensure(lines[:2] == ["16 achievements", "151 recipes"], "ordinary class-init banners")
    C.ensure(all(line.startswith("PRESSURE_PLATE_INVENTORY ") for line in lines[2:]),
             "only complete original receiver records after banners")
    rows = [json.loads(line.removeprefix("PRESSURE_PLATE_INVENTORY ")) for line in lines[2:]]
    C.ensure(len(rows) == 2, "exact two original receiver observations")
    pins = {"schema": 1, "client_jar_sha256": JAR, "classes": {}}
    with zipfile.ZipFile(jar) as archive:
        for name in ("uu", "bv"):
            pins["classes"][name] = I.inspect_class(archive.read(name + ".class"))
    for row in rows:
        C.ensure(row["class_sha256"] == {name: pin["class_sha256"]
                                        for name, pin in pins["classes"].items()},
                 "live original class resources match exact pinned archive entries")
    write(output / "source-pins.json", pins)
    write(output / "finite.json", {"schema": 1, "client_jar_sha256": JAR, "rows": rows})
    write(output / "observations.json", {"schema": 1,
          "ordinary_a_stdout": a.decode("utf-8"), "ordinary_b_stdout": b.decode("utf-8"),
          "ordinary_a_stderr": "", "ordinary_b_stderr": ""})
    write(output / "capture.json", {"schema": 1, "status": "PASS", "compile": compile_report,
          "original": runs, "inputs_before": before, "inputs_after": after,
          "compiled_observer_before": class_inputs, "compiled_observer_after": after_classes,
          "source_fields_written_by_observer": False, "display_created": False,
          "source_mutations": "Only the two actual virtual BV.g invocations per receiver."})
    print("PASS: two genuine registered Blocks, two ordinary VMs; no display or GL")


if __name__ == "__main__":
    main()
