#!/usr/bin/env python3
"""Compare the original float lookup table and pure lookup/floor methods."""

import argparse
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

from fixtures import Fixture, fixtures


JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
HARNESS_SHA256 = "f5af8190d96b33c7dfa93acaf5a13a25adb5d54cd6cf12ae459f034c87be6f06"
MAX_REQUESTS = 768
MAX_INPUT_BYTES = 32768
MAX_OUTPUT_BYTES = 4 * 1024 * 1024
CHILD_SECONDS = 15
TABLE_VALUES = 65536
LABELS = ("c-reference", "client-reference", "server-reference")
OUTPUT_NAMES = ("manifest.json", "fixtures.json", "TrigMathObservation.class",
                "compile-stdout.txt", "compile-stderr.txt")
OUTPUT_NAMES += tuple(label + "-" + suffix for label in LABELS
                      for suffix in ("requests.txt", "stdout.txt", "stderr.txt"))
HEADERS = ["TABLE_DESCRIPTOR\ta\t[F\t65536", "METHOD_DESCRIPTOR\tsin\ta\t(F)F",
           "METHOD_DESCRIPTOR\tcos\tb\t(F)F", "METHOD_DESCRIPTOR\tfloor\tb\t(D)I"]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_tool(override: str | None, name: str) -> Path | None:
    found = shutil.which(override or name)
    if found:
        return Path(found).resolve()
    if not override and os.environ.get("JAVA_HOME"):
        candidate = Path(os.environ["JAVA_HOME"]) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return candidate.resolve()
    return None


@dataclass
class ProcessResult:
    returncode: int | None
    stdout: bytes
    stderr: bytes
    timed_out: bool = False
    output_limited: bool = False
    stdout_bytes: int = 0
    stderr_bytes: int = 0


def run(command: list[str], data: str | None = None) -> ProcessResult:
    encoded = None if data is None else data.encode("ascii")
    if encoded is not None and len(encoded) > MAX_INPUT_BYTES:
        raise RuntimeError("request batch exceeds byte limit")
    # These are post-child size checks and retained-stream limits, not hard
    # limits on temporary disk writes while the bounded child is running.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        timed_out, returncode = False, None
        try:
            process = subprocess.run(command, input=encoded, stdout=stdout, stderr=stderr,
                                     timeout=CHILD_SECONDS, check=False)
            returncode = process.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
        out_size, err_size = os.fstat(stdout.fileno()).st_size, os.fstat(stderr.fileno()).st_size
        limited = out_size > MAX_OUTPUT_BYTES or err_size > MAX_OUTPUT_BYTES
        stdout.seek(0)
        stderr.seek(0)
        return ProcessResult(returncode, stdout.read(min(out_size, MAX_OUTPUT_BYTES)),
                             stderr.read(min(err_size, MAX_OUTPUT_BYTES)), timed_out, limited,
                             out_size, err_size)


def checked_text(result: ProcessResult, purpose: str) -> tuple[str, str]:
    if result.output_limited:
        raise RuntimeError(purpose + ": output exceeds retained-stream limit")
    if result.timed_out:
        raise RuntimeError(purpose + ": child timed out")
    if result.returncode:
        raise RuntimeError(f"{purpose}: child exited {result.returncode}")
    return result.stdout.decode("utf-8"), result.stderr.decode("utf-8")


def java_major(output: str, tool: str) -> int:
    if tool == "java":
        match = re.search(r"(?m)^\s*java\.specification\.version\s*=\s*(\d+(?:\.\d+)?)", output)
        if not match:
            match = re.search(r'(?m)^\s*(?:openjdk|java) version "(\d+(?:\.\d+)?)', output)
    else:
        match = re.search(r"(?m)^\s*javac\s+(\d+(?:\.\d+)?)", output)
    if not match:
        raise RuntimeError(f"could not determine {tool} major version")
    parts = match.group(1).split(".")
    return int(parts[1] if parts[0] == "1" and len(parts) > 1 else parts[0])


def same_input(output: Path, protected: set[Path]) -> bool:
    if output.resolve() in protected:
        return True
    return output.exists() and any(path.exists() and output.samefile(path) for path in protected)


def save_manifest(work: Path, manifest: dict) -> None:
    descriptor, name = tempfile.mkstemp(prefix=".math-manifest-", suffix=".tmp", dir=work)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, indent=2, ensure_ascii=True)
            stream.write("\n")
        os.replace(temporary, work / "manifest.json")
    finally:
        temporary.unlink(missing_ok=True)


def validate_cases(cases: list[Fixture]) -> str:
    if not 0 < len(cases) <= MAX_REQUESTS or len({case.label for case in cases}) != len(cases):
        raise ValueError("invalid request count or duplicate fixture labels")
    for case in cases:
        width = 16 if case.operation == "floor" else 8
        if case.operation not in {"sin", "cos", "floor"} or not re.fullmatch("[0-9a-f]{" + str(width) + "}", case.input_bits):
            raise ValueError("invalid method-specific request bits")
        if len(case.request) > 32:
            raise ValueError("request line exceeds character limit")
    data = "\n".join(case.request for case in cases) + "\n"
    if len(data.encode("ascii")) > MAX_INPUT_BYTES:
        raise ValueError("request bytes exceed limit")
    return data


def parse_output(data: bytes, cases: list[Fixture], original: bool) -> dict:
    if len(data) > MAX_OUTPUT_BYTES:
        raise ValueError("numerical stream exceeds byte limit")
    lines = data.decode("ascii").splitlines()
    expected = 4 + TABLE_VALUES + len(cases) + 2 + int(original)
    if len(lines) != expected or lines[:4] != HEADERS:
        raise ValueError("stream cardinality or method/table descriptors differ")
    start = 5 if original else 4
    table = []
    for index, line in enumerate(lines[start:start + TABLE_VALUES]):
        fields = line.split("\t")
        if len(fields) != 3 or fields[:2] != ["TABLE", str(index)] or not re.fullmatch("[0-9a-f]{8}", fields[2]):
            raise ValueError(f"invalid table index/bits at entry {index}")
        table.append(fields[2])
    digest = hashlib.sha256(bytes.fromhex("".join(table))).hexdigest()
    if original:
        if lines[4] != "TABLE_SHA256_BEFORE\t" + digest or lines[-2] != "TABLE_SHA256_AFTER\t" + digest:
            raise ValueError("original table before/after controls differ from all observed bits")
    elif lines[-2] != "TABLE_UNCHANGED\t1":
        raise ValueError("C table immutability control failed or is missing")
    if lines[-1] != "CALL_COUNT\t" + str(len(cases)):
        raise ValueError("call-count control differs")
    calls = []
    for case, line in zip(cases, lines[start + TABLE_VALUES:-2]):
        fields = line.split("\t")
        if len(fields) != 6 or fields[:4] != ["CALL", case.operation, case.descriptor, case.input_bits]:
            raise ValueError(case.label + ": call order, descriptor or raw input echo differs")
        if not re.fullmatch("[0-9a-f]{8}", fields[4]):
            raise ValueError(case.label + ": invalid result bits")
        if case.operation == "floor":
            if not re.fullmatch(r"-?(?:0|[1-9][0-9]*)", fields[5]) or len(fields[5]) > 11:
                raise ValueError(case.label + ": invalid signed integer result")
            integer = int(fields[5])
            if not -(1 << 31) <= integer < 1 << 31 or str(integer) != fields[5] or f"{integer & 0xffffffff:08x}" != fields[4]:
                raise ValueError(case.label + ": signed integer/result bits disagree")
        elif fields[5] != "-":
            raise ValueError(case.label + ": unexpected integer rendering")
        calls.append(tuple(fields[1:]))
    # Normalize only validated per-transport controls; all numerical entries,
    # inputs, descriptors, result bits and signed renderings remain exact.
    return {"table": table, "calls": calls, "table_sha256_f32be": digest}


class SetupUnavailable(RuntimeError):
    pass


class Comparison:
    def __init__(self, stage: Path, work: Path):
        self.stage, self.work = stage, work
        self.errors: list[str] = []
        self.artifacts: list[dict] = []
        self.records: dict[str, dict] = {}
        self.processes: dict[str, dict] = {}

    def publish(self, name: str, content: str | bytes) -> Path:
        if name not in OUTPUT_NAMES or name == "manifest.json":
            raise ValueError("unexpected artifact name")
        path = self.stage / name
        path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        digest, size = sha256(path), path.stat().st_size
        destination = self.work / name
        os.replace(path, destination)
        self.artifacts.append({"file": name, "sha256": digest, "bytes": size})
        return destination

    def batch(self, label: str, command: list[str], cases: list[Fixture], original: bool = False) -> dict:
        if label not in LABELS:
            raise ValueError("invalid batch label")
        requests = validate_cases(cases)
        request_path = self.publish(label + "-requests.txt", requests)
        result = run([*command, str(request_path)] if original else command,
                     None if original else requests)
        self.publish(label + "-stdout.txt", result.stdout)
        self.publish(label + "-stderr.txt", result.stderr)
        self.processes[label] = {"command": [*command, str(request_path)] if original else command,
                                 "returncode": result.returncode, "timed_out": result.timed_out,
                                 "output_limited": result.output_limited,
                                 "stdout_bytes": result.stdout_bytes, "stderr_bytes": result.stderr_bytes}
        if original and result.returncode == 77 and not result.stdout and result.stderr.decode("utf-8").splitlines() == ["UNAVAILABLE: original JAR SHA-256 differs"]:
            raise SetupUnavailable(label + ": original JAR verification unavailable")
        checked_text(result, label)
        record = parse_output(result.stdout, cases, original)
        self.records[label] = {"table_values": len(record["table"]), "calls": len(record["calls"]),
                               "table_sha256_f32be": record["table_sha256_f32be"],
                               "table_control": "original before/after hashes" if original else "C TABLE_UNCHANGED=1",
                               "call_records": [{"label": case.label, "raw_record": call}
                                                for case, call in zip(cases, record["calls"])]}
        return record

    def compare(self, label: str, cases: list[Fixture], left: dict, right: dict) -> dict:
        if len(left["table"]) != TABLE_VALUES or len(right["table"]) != TABLE_VALUES or len(left["calls"]) != len(cases) or len(right["calls"]) != len(cases):
            raise ValueError("comparison cardinalities differ")
        table_differences = [index for index, (a, b) in enumerate(zip(left["table"], right["table"])) if a != b]
        call_differences = [index for index, (a, b) in enumerate(zip(left["calls"], right["calls"])) if a != b]
        if table_differences:
            index = table_differences[0]
            self.errors.append(f"{label}: {len(table_differences)} table values differ; first table[{index}]: {left['table'][index]} != {right['table'][index]}")
        if call_differences:
            index = call_differences[0]
            self.errors.append(f"{label}: {len(call_differences)} calls differ; first {cases[index].label}: {left['calls'][index]} != {right['calls'][index]}")
        return {"table_entries": TABLE_VALUES - len(table_differences), "calls": len(cases) - len(call_differences)}


def source_paths(root: Path) -> list[Path]:
    names = ("scripts/reference/math/compare_math.py", "scripts/reference/math/fixtures.py",
             "tests/reference/math/TrigMathObservation.java", "include/mc/core/math/math.h",
             "src/core/math/math.c", "tests/core/math/math_probe.c",
             "include/mc/core/java/java.h", "src/core/java/java.c", "docs/reference/math.md",
             "CMakeLists.txt", "tests/core/math/test_math.c", "tests/core/math/math_fixtures.h",
             "tests/tools/math/test_reference.py")
    return [(root / name).resolve() for name in names]


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/math")
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch("[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable, work = args.executable.resolve(), args.work_dir.resolve()
    jars = {side: path.resolve() for side, path in (("client", args.client), ("server", args.server))}
    source = root / "tests/reference/math/TrigMathObservation.java"
    sources = source_paths(root)
    protected = {path.resolve() for path in [executable, *jars.values(), *sources, java, javac] if path is not None}
    protected.update(Path(value).resolve() for value in (args.java, args.javac)
                     if value and ("/" in value or "\\" in value))
    try:
        if any(path == work or work in path.parents for path in protected):
            raise ValueError("work directory contains supplied inputs or source files")
        if any(same_input(work / name, protected) for name in OUTPUT_NAMES):
            raise ValueError("output paths overlap supplied inputs or source files")
    except (OSError, ValueError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    manifest = {"schema": 1, "protocol": 1, "run_id": uuid.uuid4().hex, "status": "running",
                "started_utc": datetime.now(timezone.utc).isoformat(),
                "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
                "commit_record": "supplied by caller" if args.tested_commit else "unrecorded",
                "scope": "Original 65536-entry float table and pure sine/cosine/double-floor helpers; no worlds or cave execution",
                "comparison_rules": ["All 65536 raw table bits and every ordered method descriptor/input/result/signed integer compare independently in three pairings",
                                     "Original before/after SHA-256 controls must match all observed table bytes; C TABLE_UNCHANGED must be1",
                                     "Only validated transport controls differ; no numerical normalization or tolerance"],
                "limits": {"requests": MAX_REQUESTS, "request_bytes": MAX_INPUT_BYTES,
                           "request_characters": 32, "child_seconds": CHILD_SECONDS, "java_heap_mib": 128,
                           "retained_bytes_per_stream": MAX_OUTPUT_BYTES,
                           "output_policy": "Full stream sizes checked after child completion before reads; retained limits are not hard temporary disk write caps"},
                "java": {"path": str(java) if java else None}, "javac": {"path": str(javac) if javac else None},
                "executable": {"path": str(executable)}, "python": sys.version, "host": platform.platform(),
                "jars": {side: {"path": str(path)} for side, path in jars.items()}, "errors": []}
    comparison = None
    input_hashes = {}
    unavailable_reason = None

    def save() -> None:
        if comparison is not None:
            manifest.update(artifacts=comparison.artifacts, records=comparison.records,
                            processes=comparison.processes)
        save_manifest(work, manifest)

    def remember(path: Path) -> str:
        value = sha256(path)
        input_hashes[path.resolve()] = value
        return value

    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
        missing = [name for name, path in (("java", java), ("javac", javac)) if path is None]
        if not executable.is_file():
            missing.append("C probe " + str(executable))
        for side, jar in jars.items():
            if not jar.is_file():
                missing.append(f"{side} JAR {jar}")
            else:
                manifest["jars"][side]["sha256"] = remember(jar)
                if manifest["jars"][side]["sha256"] != JAR_HASHES[side]:
                    missing.append(f"{side} JAR SHA-256 mismatch ({jar})")
        if missing:
            unavailable_reason = "; ".join(missing)
        else:
            assert java is not None and javac is not None
            for key, path in (("executable", executable), ("java", java), ("javac", javac)):
                manifest[key]["sha256"] = remember(path)
            manifest["sources"] = [{"file": path.relative_to(root).as_posix(), "sha256": remember(path)} for path in sources]
            if remember(source) != HARNESS_SHA256:
                raise ValueError("observation source differs from the frozen original-only harness")
            version = checked_text(run([str(java), "-Xms16m", "-Xmx128m", "-XshowSettings:properties", "-version"]), "Java version")
            compiler = checked_text(run([str(javac), "-J-Xms16m", "-J-Xmx128m", "-version"]), "javac version")
            manifest["runtime_info"], manifest["compiler_info"] = "".join(version), "".join(compiler)
            java_version = java_major(manifest["runtime_info"], "java")
            javac_version = java_major(manifest["compiler_info"], "javac")
            manifest["java"]["major"], manifest["javac"]["major"] = java_version, javac_version
            if min(java_version, javac_version) < 11:
                unavailable_reason = "Java and javac 11 or newer required"
            else:
                for path in (java.parent.parent / "release", java.parent.parent / "lib/modules"):
                    if path.is_file():
                        manifest["java"].setdefault("runtime_files", []).append({"path": str(path), "sha256": remember(path)})
                cases = fixtures()
                validate_cases(cases)
                manifest.update(fixture_count=len(cases), fixture_counts=dict(Counter(case.operation for case in cases)),
                                table_values_per_side=TABLE_VALUES,
                                coverage_counts=dict(Counter(reason for case in cases for reason in case.coverage)))
                with tempfile.TemporaryDirectory(prefix=".math-run-", dir=work) as temporary:
                    comparison = Comparison(Path(temporary), work)
                    classes = Path(temporary) / "classes"
                    classes.mkdir()
                    compiled = run([str(javac), "-J-Xms16m", "-J-Xmx128m", "--release", "11", "-encoding", "UTF-8", "-d", str(classes), str(source)])
                    comparison.publish("compile-stdout.txt", compiled.stdout)
                    comparison.publish("compile-stderr.txt", compiled.stderr)
                    checked_text(compiled, "Java harness compilation")
                    class_file = classes / "TrigMathObservation.class"
                    manifest["compiled_harness_sha256"] = sha256(class_file)
                    comparison.publish("TrigMathObservation.class", class_file.read_bytes())
                    comparison.publish("fixtures.json", json.dumps([dict(asdict(case), descriptor=case.descriptor, label=case.label) for case in cases], indent=2) + "\n")
                    try:
                        c_record = comparison.batch("c-reference", [str(executable)], cases)
                        originals = {}
                        for side, jar in jars.items():
                            originals[side] = comparison.batch(side + "-reference", [str(java), "-Xms16m", "-Xmx128m", "-Djava.awt.headless=true", "-cp", str(classes), "TrigMathObservation", side, str(jar)], cases, original=True)
                            manifest.setdefault("matched_records", {})[side + "_to_c"] = comparison.compare(side + "-to-C", cases, originals[side], c_record)
                            save()
                        manifest["matched_records"]["client_to_server"] = comparison.compare("client-to-server", cases, originals["client"], originals["server"])
                    finally:
                        manifest["compiled_harness_after_sha256"] = sha256(class_file)
                        if manifest["compiled_harness_after_sha256"] != manifest["compiled_harness_sha256"]:
                            comparison.errors.append("compiled harness changed during observation")
    except SetupUnavailable as error:
        unavailable_reason = str(error)
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError) as error:
        manifest["errors"].append(str(error))
    manifest["input_preservation"] = []
    for path, before in input_hashes.items():
        try:
            after = sha256(path)
            manifest["input_preservation"].append({"path": str(path), "before": before, "after": after})
            if before != after:
                manifest["errors"].append("input changed during observation: " + str(path))
        except OSError as error:
            manifest["errors"].append("cannot verify input preservation: " + str(error))
    if comparison is not None:
        manifest["errors"] += comparison.errors
        for artifact in comparison.artifacts:
            path = work / artifact["file"]
            try:
                if path.stat().st_size != artifact["bytes"] or sha256(path) != artifact["sha256"]:
                    manifest["errors"].append("artifact changed during observation: " + str(path))
            except OSError as error:
                manifest["errors"].append("cannot verify artifact: " + str(error))
    status = "fail" if manifest["errors"] else "unavailable" if unavailable_reason else "pass"
    manifest.update(status=status, finished_utc=datetime.now(timezone.utc).isoformat())
    if unavailable_reason:
        manifest["reason"] = unavailable_reason
    try:
        save()
    except OSError as error:
        print("FAIL: cannot write manifest: " + str(error), file=sys.stderr)
        return 1
    if status == "fail":
        for error in manifest["errors"][:30]:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    if status == "unavailable":
        print("UNAVAILABLE: " + str(unavailable_reason), file=sys.stderr)
        return 77
    print(f"PASS: {TABLE_VALUES} exact table entries and {manifest['fixture_count']} exact calls in three independent pairings")
    print("Full report: " + str(work / "manifest.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
