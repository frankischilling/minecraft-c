"""Exact finite unchanged-original insertion/pickup state and alias comparison."""
import argparse
import json
import os
import re
import shutil
import tempfile
import zipfile
from pathlib import Path
from support import digest, module, require, run, REFERENCE
from run_native import native
from audit_dispatch import audit

HERE = Path(__file__).resolve().parent
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5"}


def records(lines):
    selected = [x for x in lines if x.startswith((b"INSERT ", b"PICKUP "))]
    require([int(x.split()[1]) for x in selected if x.startswith(b"INSERT ")] ==
            list(range(23)) + list(range(100, 107)), "insertion/alias profile changed")
    require([int(x.split()[1]) for x in selected if x.startswith(b"PICKUP ")] ==
            list(range(7)), "pickup profile changed")
    require(len(selected) == 37, "incomplete original profile")
    return selected


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--executable", type=Path, required=True)
    p.add_argument("--client-jar", type=Path, required=True)
    p.add_argument("--libraries", type=Path, required=True)
    p.add_argument("--java", default=shutil.which("java"))
    p.add_argument("--javac", default=shutil.which("javac"))
    p.add_argument("--work-dir", type=Path, required=True)
    a = p.parse_args()
    a.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="item-pickup-reference-", dir=a.work_dir.resolve()))
    java = Path(shutil.which(a.java) or "").resolve(strict=True)
    javac = Path(shutil.which(a.javac) or "").resolve(strict=True)
    require(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "one matching JDK bin required")
    require(digest(a.client_jar) == JAR, "actual client archive differs")
    audit(a.client_jar)
    libraries = [a.libraries / name for name in LIBRARIES]
    require(all(digest(x) == LIBRARIES[x.name] for x in libraries), "dependency archives differ")
    pins = json.loads((HERE / "source-pins.json").read_text())
    parser_path = REFERENCE / "client/rendering/classfile.py"
    parser = module("pickup_source_metadata", parser_path)
    with zipfile.ZipFile(a.client_jar) as jar:
        for name, pin in pins["classes"].items():
            require(parser.inspect_class(jar.read(name + ".class")) == pin, "actual class/Code differs: " + name)
    inputs = [java, javac, a.client_jar, a.executable, *libraries, parser_path,
              *[x for x in HERE.iterdir() if x.is_file()]]
    before = {str(x.resolve()): digest(x) for x in inputs}
    report = {"status": "FAIL", "input_before": before}
    classes = out / "classes"
    classes.mkdir()
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    try:
        # Version tools intentionally use stderr; retain bounded capture but do
        # not apply the empty-stderr requirement used for compile/game/native.
        capture = module("pickup_version_capture", REFERENCE / "client/statistics/support.py")
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            result = capture.run_bounded([tool, "-version"], out, label + "-version", timeout=10, env=env)
            require(result.returncode == 0, "version executable failed")
            versions.append((result.stdout + result.stderr).decode("utf-8"))
        vm = re.search(r'version "(17\.[0-9]+\.[0-9]+(?:\.[0-9]+)?)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+(?:\.[0-9]+)?)', versions[1])
        require(vm and compiler and vm[1] == compiler[1], "matching JDK17 required")
        cp = os.pathsep.join(str(x.resolve()) for x in [classes, a.client_jar, *libraries])
        run([javac, "-encoding", "UTF-8", "-classpath", cp, "-d", classes,
             HERE / "PickupObservation.java"], out, "compile", timeout=30, env=env)
        helpers = {x.relative_to(classes).as_posix(): digest(x) for x in classes.rglob("*.class")}
        require(set(helpers) == {"PickupObservation.class"}, "unexpected helper class")
        report["helper_before"] = helpers
        original = run([java, "--add-opens", "java.base/java.util=ALL-UNNAMED", "-Xint", "-Xverify:all", "-Xmx1g",
                        "-classpath", cp, "PickupObservation", (out / "original-worlds").resolve(),
                        a.client_jar.resolve()], out, "original", env=env)
        lines = original.stdout.splitlines()
        require(lines[-1] == b"Original Item pickup: 37 states", "original final marker missing")
        require(len([x for x in lines if x.startswith(b"ORIGIN ")]) == 9, "actual class origins missing")
        native_dir = out / "native"
        native_dir.mkdir()
        native_lines, native_report = native(a.executable, native_dir, reference=True)
        expected, actual = records(lines), records(native_lines)
        require(expected == actual, {"first_difference": next(((i, x.decode(), y.decode()) for i, (x, y) in
                enumerate(zip(expected, actual)) if x != y), None)})
        report.update(status="PASS", records=37, insertion_cases=23, pickup_cases=7,
            jdk=versions, original_capture=original.capture_report, native=native_report,
            original_stdout_sha256=digest(out / "original.stdout"),
            scope="Actual constructed dc/World/Inventory/Stack/Item.23 insertion rows plus7 pickup rows and7 resulting Inventory rows.2 insertion source failures,4 completed pickup early returns,2 actual achievement null-App prefixes and1 actual dc notification null-App prefix; raw arrays/counts/metadata/animation/same-owner aliases/Item Random states compared. No successful particle notification/death, unsafe setup, constructor entropy or full tick claim.")
    finally:
        report["input_after"] = {str(x.resolve()): digest(x) for x in inputs}
        report["helper_after"] = {x.relative_to(classes).as_posix(): digest(x) for x in classes.rglob("*.class")}
        if report["input_after"] != before or ("helper_before" in report and report["helper_before"] != report["helper_after"]):
            report["status"] = "FAIL"
        (out / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        require(report["input_after"] == before, "comparison inputs changed")
        if "helper_before" in report:
            require(report["helper_before"] == report["helper_after"], "compiled helper changed")
    print("Item pickup original/native:37 exact records; report=" + str(out / "comparison.json"))


if __name__ == "__main__":
    main()
