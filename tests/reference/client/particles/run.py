"""Bounded native CTest driver and unchanged-original JDK17 comparison."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
REFERENCE = HERE.parent
sys.path.insert(0, str(REFERENCE / "statistics"))
from support import run_bounded

JAR_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
GAME_CLASSES = {"em", "xw", "sn", "in", "dc", "dn", "ju", "eq"}
HELPERS = {"ParticleObservation.class", "ParticleInputs.class", "ParticleInputs$Drain.class"}
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def need(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def controlled_environment() -> dict[str, str]:
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    return env


def run(command: list[str], root: Path, name: str, timeout: int = 120):
    result = run_bounded(command, cwd=root, prefix=name, timeout=timeout,
                         limit=2 * 1024 * 1024, env=controlled_environment())
    need(result.returncode == 0, f"{name}: exit {result.returncode}")
    need(result.stderr == b"", f"{name}: nonempty stderr")
    return result.stdout.decode("ascii").replace("\r\n", "\n")


def native_count(text: str) -> int:
    lines = text.splitlines()
    marker = re.fullmatch(r"Pickup particles: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    need(marker is not None, "missing positive native completion marker")
    return int(marker[1])


def expected_labels() -> list[tuple[str, ...]]:
    labels = [("manager", "constructed"), ("manager-next",), ("null-cold",)]
    for case in range(8):
        labels += [("particle", f"ctor{case}"), ("next", str(case))]
        labels += [("particle", f"tick{case}-{tick}") for tick in range(1, 5)]
        labels += [("layer", str(case))]
        if case == 0:
            labels += [("particle", "wrap")]
    return labels + [("null",), ("manager", "one"), ("manager", "tick0"),
                     ("manager", "tick1"), ("manager", "tick2"), ("manager", "cap"),
                     ("manager", "duplicates1"), ("duplicate-state",),
                     ("manager", "duplicates2"), ("manager", "clear"), ("final",)]


def profile_records(text: str, original: bool) -> list[str]:
    lines = text.splitlines()
    if original:
        need(bool(lines) and lines[-1] == "Original particles: 71 records",
             "missing exact original completion marker")
    else:
        native_count(text)
    records = lines[:-1]
    labels = expected_labels()
    need(len(labels) == len(records) == 71, "particle profile must contain all71 records")
    for index, (record, expected) in enumerate(zip(records, labels)):
        need(tuple(record.split("\t")[:len(expected)]) == expected,
             f"particle profile record {index} has wrong case/order")
    return records


def class_info(data: bytes):
    spec = importlib.util.spec_from_file_location("particle_classfile", REFERENCE / "notice/classfile.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.inspect_class(data)


def jdk_info(java: Path):
    with zipfile.ZipFile(java.parent.parent / "jmods/java.base.jmod") as archive:
        return {name: class_info(archive.read("classes/" + name + ".class")) for name in
                ("java/util/Random", "java/lang/Math", "java/lang/Math$RandomNumberGeneratorHolder", "java/lang/System")}


def helper_inventory(classes: Path) -> dict[str, str]:
    return {path.relative_to(classes).as_posix(): sha(path) for path in sorted(classes.rglob("*.class"))}


def read_origins(path: Path) -> list[list[str]]:
    # Files.writeString in the genuine observation helper writes UTF8.
    return [line.split("\t") for line in path.read_text(encoding="utf-8").splitlines()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path)
    parser.add_argument("--library-dir", type=Path)
    parser.add_argument("--java", type=Path)
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="pickup-", dir=args.work_dir.resolve()))
    native = args.native.resolve(strict=True)
    inputs = {native, *[path for path in HERE.iterdir() if path.is_file()],
              REFERENCE / "statistics/support.py", REFERENCE / "notice/classfile.py"}
    before = {str(path.resolve()): sha(path) for path in inputs}
    report = {"status": "FAIL", "tested_commit": args.tested_commit, "inputs_before": before}
    classes = None
    helpers = None
    try:
        if args.client_jar is None:
            text = run([str(native)], root, "native")
            profile_records(text, original=False)
            report.update(native_checks=native_count(text), reference=False, records=71)
        else:
            need(args.library_dir is not None, "--library-dir is required with --client-jar")
            jar = args.client_jar.resolve(strict=True)
            need(sha(jar) == JAR_SHA, "unverified original JAR")
            libraries = [args.library_dir / name for name in LIBRARIES]
            for path in libraries:
                need(sha(path) == LIBRARIES[path.name], "unverified library: " + path.name)
            need({path.name for path in args.library_dir.glob("*.jar")} == set(LIBRARIES),
                 "unexpected library JAR")
            java = (args.java or Path(shutil.which("java") or "")).resolve(strict=True)
            javac = java.with_name("javac.exe" if os.name == "nt" else "javac")
            inputs.update({java, javac, jar, *libraries, java.parent.parent / "jmods/java.base.jmod"})
            before = {str(path.resolve()): sha(path) for path in inputs}
            report["inputs_before"] = before
            versions = []
            for name, exe in (("java", java), ("javac", javac)):
                result = run_bounded([str(exe), "-version"], cwd=root, prefix=name, timeout=10,
                                     limit=65536, env=controlled_environment())
                need(result.returncode == 0, name + " version failed")
                match = re.search(r'(?:version\s+"|javac\s+)(17\.[0-9]+\.[0-9]+)',
                                  (result.stdout + result.stderr).decode("utf-8", "strict"))
                need(match is not None, name + " must be JDK17")
                versions.append(match[1])
            need(versions[0] == versions[1], "java/javac versions differ")
            pins = json.loads((HERE / "source-pins.json").read_text())
            need(set(pins["classes"]) == GAME_CLASSES, "original class inventory differs")
            with zipfile.ZipFile(jar) as archive:
                for name, expected in pins["classes"].items():
                    need(class_info(archive.read(name + ".class")) == expected,
                         "original whole class/Code metadata differs: " + name)
            classes = root / "classes"
            classes.mkdir()
            run([str(javac), "--add-modules", "jdk.jdi", "-cp", str(jar), "-d", str(classes),
                 str(HERE / "ParticleObservation.java"), str(HERE / "ParticleInputs.java")], root, "compile")
            helpers = helper_inventory(classes)
            need(set(helpers) == HELPERS, "unexpected compiled helper class inventory")
            report["helpers_before"] = helpers
            cp = os.pathsep.join((str(classes), str(jar), str(args.library_dir.resolve() / "*")))
            observed = run([str(java), "--add-modules", "jdk.jdi", "-cp", str(classes), "ParticleInputs", cp,
                            str(root / "original.stdout"), str(root / "original.stderr"), str(jar),
                            str(root / "origins.tsv"), str(root / "target-process.tsv")], root, "observer")
            need(re.fullmatch(r"(?:input\t[0-9a-f]+\t[0-9a-f]+\n){14}Draws: 48 floats, 58 doubles, 1 longs\nInputs: 14 constructors\n", observed) is not None,
                 "incomplete observed constructor inputs")
            need((root / "original.stderr").read_bytes() == b"", "target stderr is nonempty")
            origins = read_origins(root / "origins.tsv")
            need(len(origins) == 8 and {row[0] for row in origins} == GAME_CLASSES and
                 all(len(row) == 2 and Path(row[1]).samefile(jar) for row in origins),
                 "original runtime class origins differ")
            original = profile_records((root / "original.stdout").read_text(encoding="ascii"), original=True)
            native_text = run([str(native), str(root / "observer.stdout")], root, "native")
            actual = profile_records(native_text, original=False)
            if actual != original:
                import difflib
                (root / "difference.txt").write_text("\n".join(difflib.unified_diff(
                    original, actual, fromfile="original", tofile="native")), encoding="ascii")
                raise RuntimeError("original/native rows differ; see difference.txt")
            report.update(native_checks=native_count(native_text), reference=True, records=71,
                          constructor_inputs=14, jar_sha256=sha(jar), jdk=versions[0],
                          compiled_helpers={path.name: class_info(path.read_bytes()) for path in sorted(classes.glob("*.class"))},
                          runtime_origins=origins, jdk_classes=jdk_info(java),
                          target_process=(root / "target-process.tsv").read_text(encoding="utf-8").splitlines(),
                          draw_calls={"nextFloat": 48, "nextDouble": 58, "nextLong": 1})
        report["status"] = "PASS"
    finally:
        report["inputs_after"] = {str(path.resolve()): sha(path) for path in inputs}
        changed = report["inputs_after"] != before
        if helpers is not None:
            report["helpers_after"] = helper_inventory(classes)
            changed = changed or report["helpers_after"] != helpers
        if changed:
            report["status"] = "FAIL"
            report["changed_inputs"] = True
        (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        need(not changed, "qualification inputs changed")
    print(f"Pickup qualification: {report['native_checks']} checks; report {root / 'report.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
