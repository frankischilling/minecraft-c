#!/usr/bin/env python3
"""Compare C numerical noise with untouched pinned client/server Perlin/octaves."""

import argparse
from collections import Counter
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
MAX_INPUT_BYTES = 1024 * 1024
MAX_OUTPUT_BYTES = 16 * 1024 * 1024
LABELS = ("c-reference", "client-reference", "server-reference")
OUTPUT_NAMES = ("manifest.json", "compile-stdout.txt", "compile-stderr.txt", "NoiseProbe.class", "fixtures.json")
OUTPUT_NAMES += tuple(label + "-" + suffix for label in LABELS
                      for suffix in ("requests.txt", "stdout.txt", "stderr.txt"))


def sha256(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def find_tool(override: str | None, name: str) -> Path | None:
    found = shutil.which(override or name)
    if found:
        return Path(found).resolve()
    if not override and os.environ.get("JAVA_HOME"):
        candidate = Path(os.environ["JAVA_HOME"]) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return candidate.resolve()
    return None


def run(command: list[str], data: str | None = None) -> subprocess.CompletedProcess[str]:
    if data is not None and len(data.encode("utf-8")) > MAX_INPUT_BYTES:
        raise RuntimeError("probe request batch exceeds limit")
    # Bound retained output and wall time, including failures in supplied tools.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(command, input=data, text=True, encoding="utf-8", errors="strict",
                                stdout=stdout, stderr=stderr, timeout=60, check=False)
        stdout.seek(0)
        stderr.seek(0)
        output, errors = stdout.read(MAX_OUTPUT_BYTES + 1), stderr.read(MAX_OUTPUT_BYTES + 1)
        if len(output) > MAX_OUTPUT_BYTES or len(errors) > MAX_OUTPUT_BYTES:
            raise RuntimeError("probe process output exceeds limit")
        return subprocess.CompletedProcess(command, result.returncode, output.decode("utf-8"), errors.decode("utf-8"))


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
    if output.exists():
        return any(path.exists() and output.samefile(path) for path in protected)
    return False


def save_manifest(work: Path, manifest: dict) -> None:
    # Replace, rather than truncate, so a report is never half old/half new and
    # a stale report cannot remain a pass after a new unavailable/failed run.
    descriptor, name = tempfile.mkstemp(prefix=".noise-manifest-", suffix=".tmp", dir=work)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, indent=2, ensure_ascii=True)
            stream.write("\n")
        os.replace(temporary, work / "manifest.json")
    finally:
        temporary.unlink(missing_ok=True)


def parse_response(line: str, case: Fixture) -> dict:
    fields = line.split("\t")
    if len(fields) != 4 or fields[0] != "OK":
        raise ValueError(f"{case.label}: invalid response prefix/field count: {line[:100]!r}")
    values = [] if fields[1] == "-" else fields[1].split(",")
    continuation = fields[2].split(",")
    permutations = [] if fields[3] == "-" else fields[3].split(",")
    if len(values) != case.output_count() or any(not re.fullmatch("[0-9a-f]{16}", value) for value in values):
        raise ValueError(f"{case.label}: invalid value count or double hex")
    if len(continuation) != 4 or any(not re.fullmatch("[0-9a-f]{16}", value) for value in continuation):
        raise ValueError(f"{case.label}: invalid Random continuation")
    if len(permutations) != case.permutation_count() or any(
            not re.fullmatch(r"0|[1-9][0-9]{0,2}", value) or int(value) > 255 for value in permutations):
        raise ValueError(f"{case.label}: invalid permutation count or entry")
    return {"values": values, "random_continuation": continuation, "permutation": permutations}


class Comparison:
    def __init__(self, stage: Path, work: Path):
        self.stage = stage
        self.work = work
        self.errors: list[str] = []
        self.artifacts: list[dict] = []
        self.records: dict[str, list[dict]] = {}

    def publish(self, name: str, content: str | bytes) -> None:
        if name not in OUTPUT_NAMES or name == "manifest.json":
            raise ValueError("unexpected artifact name")
        path = self.stage / name
        path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        digest = sha256(path)
        os.replace(path, self.work / name)
        self.artifacts.append({"file": name, "sha256": digest, "bytes": (self.work / name).stat().st_size})

    def batch(self, label: str, command: list[str], cases: list[Fixture]) -> list[dict]:
        if label not in LABELS or not cases or len(cases) > 1024:
            raise ValueError("invalid batch label/count")
        requests = "\n".join(case.request() for case in cases) + "\n"
        self.publish(label + "-requests.txt", requests)
        result = run(command, requests)
        self.publish(label + "-stdout.txt", result.stdout)
        self.publish(label + "-stderr.txt", result.stderr)
        if result.returncode:
            raise RuntimeError(f"{label}: process exited {result.returncode}: {result.stderr[:1000]}")
        lines = result.stdout.splitlines()
        if len(lines) != len(cases):
            raise RuntimeError(f"{label}: {len(lines)} responses for {len(cases)} requests")
        records = [parse_response(line, case) for line, case in zip(lines, cases)]
        self.records[label] = [{"label": case.label,
                                "response_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
                                "value_count": len(record["values"]),
                                "permutation_count": len(record["permutation"])}
                               for case, line, record in zip(cases, lines, records)]
        return records

    def compare(self, label: str, cases: list[Fixture], left: list[dict], right: list[dict]) -> int:
        matches = 0
        for case, expected, actual in zip(cases, left, right):
            if expected == actual:
                matches += 1
                continue
            for key in ("values", "random_continuation", "permutation"):
                if expected[key] == actual[key]:
                    continue
                index = next(i for i, (a, b) in enumerate(zip(expected[key], actual[key])) if a != b)
                self.errors.append(f"{label}/{case.label}: {key}[{index}] differs: "
                                   f"{expected[key][index]} != {actual[key][index]}")
        return matches


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/noise")
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable, work = args.executable.resolve(), args.work_dir.resolve()
    jars = {"client": args.client.resolve(), "server": args.server.resolve()}
    source = root / "tests/reference/noise/NoiseProbe.java"
    sources = [source, Path(__file__).resolve(), Path(__file__).resolve().with_name("fixtures.py")]
    sources += [path for path in (root / "src/world/noise/noise.c", root / "include/mc/world/noise/noise.h",
                                 root / "tests/world/noise/noise_probe.c", root / "docs/reference/noise.md") if path.is_file()]
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
    manifest = {"schema": 1, "run_id": uuid.uuid4().hex, "status": "running",
                "started_utc": datetime.now(timezone.utc).isoformat(),
                "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
                "commit_record": "supplied by caller" if args.tested_commit else "unrecorded",
                "scope": "Pure procedural Perlin/octave numerical methods; no chunks, worlds, files or game-state claims",
                "harness_java_release": 11, "python": sys.version, "host": platform.platform(),
                "java": {"path": str(java) if java else None}, "javac": {"path": str(javac) if javac else None},
                "executable": {"path": str(executable)}, "jars": {side: {"path": str(path)} for side, path in jars.items()},
                "symbols": {"client": {"perlin": "am", "octaves": "uf"},
                            "server": {"perlin": "ab", "octaves": "mp"}},
                "comparison_rules": ["Every returned double bit, including signed zero, compares exactly",
                                     "All 512 permutation entries and XYZ offset bits compare per initialized layer",
                                     "Four nextLong continuation values compare exactly after every operation",
                                     "Client-to-C, server-to-C and client-to-server results compare separately",
                                     "No finite-bit tolerance, coordinate wrapping or scalar/bulk substitution"],
                "errors": [], "artifacts": [], "records": {}}
    comparison: Comparison | None = None

    def unavailable(reason: str) -> int:
        manifest.update({"status": "unavailable", "reason": reason,
                         "finished_utc": datetime.now(timezone.utc).isoformat()})
        save_manifest(work, manifest)
        print("UNAVAILABLE: " + reason, file=sys.stderr)
        return 77

    try:
        work.mkdir(parents=True, exist_ok=True)
        save_manifest(work, manifest)
        missing = [name for name, tool in (("java", java), ("javac", javac)) if tool is None]
        if not executable.is_file():
            missing.append(f"C probe {executable}")
        for side, jar in jars.items():
            if not jar.is_file():
                missing.append(f"{side} JAR {jar}")
            else:
                manifest["jars"][side]["sha256"] = sha256(jar)
                if manifest["jars"][side]["sha256"] != JAR_HASHES[side]:
                    missing.append(f"{side} JAR SHA-256 mismatch ({jar})")
        if missing:
            return unavailable("; ".join(missing))
        assert java is not None and javac is not None
        for key, path in (("executable", executable), ("java", java), ("javac", javac)):
            manifest[key]["sha256"] = sha256(path)
        manifest["sources"] = [{"file": path.relative_to(root).as_posix(), "sha256": sha256(path)} for path in sources]
        runtime = run([str(java), "-XshowSettings:properties", "-version"])
        compiler = run([str(javac), "-version"])
        if runtime.returncode or compiler.returncode:
            raise RuntimeError("Java runtime/compiler version command failed")
        manifest["runtime_info"], manifest["compiler_info"] = runtime.stdout + runtime.stderr, compiler.stdout + compiler.stderr
        runtime_major, compiler_major = java_major(manifest["runtime_info"], "java"), java_major(manifest["compiler_info"], "javac")
        manifest["java"]["major"], manifest["javac"]["major"] = runtime_major, compiler_major
        if runtime_major < 11 or compiler_major < 11:
            return unavailable("Java and javac 11 or newer required")
        for path in (java.parent.parent / "release", java.parent.parent / "lib/modules"):
            if path.is_file():
                manifest["java"].setdefault("runtime_files", []).append({"path": str(path), "sha256": sha256(path)})
        # The class directory is fresh for every run. Never execute a stale class
        # left by an earlier successful compile or from the publication directory.
        with tempfile.TemporaryDirectory(prefix=".noise-run-", dir=work) as temporary:
            stage = Path(temporary)
            comparison = Comparison(stage, work)
            classes = stage / "classes"
            classes.mkdir()
            compiled = run([str(javac), "--release", "11", "-encoding", "UTF-8", "-d", str(classes), str(source)])
            comparison.publish("compile-stdout.txt", compiled.stdout)
            comparison.publish("compile-stderr.txt", compiled.stderr)
            if compiled.returncode:
                raise RuntimeError("Java harness compilation failed: " + compiled.stderr[:1000])
            class_file = classes / "NoiseProbe.class"
            manifest["compiled_harness_sha256"] = sha256(class_file)
            comparison.publish("NoiseProbe.class", class_file.read_bytes())
            cases = fixtures()
            comparison.publish("fixtures.json", json.dumps([
                {"label": case.label, "request": case.request(), "coverage": case.coverage,
                 "value_count": case.output_count(), "permutation_count": case.permutation_count()}
                for case in cases], indent=2) + "\n")
            manifest["fixture_counts"] = dict(Counter(case.operation for case in cases))
            manifest["fixture_count"] = len(cases)
            manifest["coverage_counts"] = dict(Counter(coverage for case in cases for coverage in case.coverage))
            manifest["values_per_side"] = sum(case.output_count() for case in cases)
            manifest["permutation_entries_per_side"] = sum(case.permutation_count() for case in cases)
            manifest["random_continuation_longs_per_side"] = 4 * len(cases)
            c_records = comparison.batch("c-reference", [str(executable)], cases)
            original_records = {}
            for side, jar in jars.items():
                command = [str(java), "-cp", str(classes), "NoiseProbe", side, str(jar)]
                original_records[side] = comparison.batch(side + "-reference", command, cases)
            manifest["matched_records"] = {
                "client_to_c": comparison.compare("client-to-C", cases, original_records["client"], c_records),
                "server_to_c": comparison.compare("server-to-C", cases, original_records["server"], c_records),
                "client_to_server": comparison.compare("client-to-server", cases, original_records["client"], original_records["server"]),
            }
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError) as error:
        manifest["errors"].append(str(error))
    if comparison is not None:
        manifest["errors"] += comparison.errors
        manifest["artifacts"], manifest["records"] = comparison.artifacts, comparison.records
    manifest.update({"status": "fail" if manifest["errors"] else "pass",
                     "finished_utc": datetime.now(timezone.utc).isoformat()})
    try:
        work.mkdir(parents=True, exist_ok=True)
        save_manifest(work, manifest)
    except OSError as error:
        print(f"FAIL: cannot write manifest: {error}", file=sys.stderr)
        return 1
    if manifest["errors"]:
        for error in manifest["errors"][:30]:
            print("FAIL: " + error, file=sys.stderr)
        print(f"{len(manifest['errors'])} failures; full report: {work}", file=sys.stderr)
        return 1
    print(f"PASS: {manifest['fixture_count']} requests each for C, original client and original server; "
          f"{manifest['values_per_side']} exact double values, {manifest['permutation_entries_per_side']} permutation entries "
          f"and {manifest['random_continuation_longs_per_side']} continuation longs per side")
    print(f"Pure numerical methods only; full report: {work / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
