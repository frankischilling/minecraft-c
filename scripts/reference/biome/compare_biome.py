#!/usr/bin/env python3
"""Compare bounded climate/classifier fixtures with untouched original numerical methods."""

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

from fixtures import Fixture, client_fixtures, fixtures


JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
MAX_INPUT_BYTES = 1024 * 1024
MAX_OUTPUT_BYTES = 16 * 1024 * 1024
LABELS = ("c-reference", "client-reference", "server-reference", "c-client-only", "client-only-reference")
OUTPUT_NAMES = ("manifest.json", "compile-stdout.txt", "compile-stderr.txt", "BiomeProbe.class", "fixtures.json")
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
    descriptor, name = tempfile.mkstemp(prefix=".biome-manifest-", suffix=".tmp", dir=work)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, indent=2, ensure_ascii=True)
            stream.write("\n")
        os.replace(temporary, work / "manifest.json")
    finally:
        temporary.unlink(missing_ok=True)


SLOTS = ("a", "b", "c", "d", "t", "o")
RESPONSE_KEYS = {"catalog", "table", "biome", "number", *SLOTS, "retained", "returned", "aliases"}


class SetupUnavailable(RuntimeError):
    pass


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def parse_response(line: str, case: Fixture) -> dict:
    if len(line) > 1024 * 1024 or not line.startswith("OK\t"):
        raise ValueError(f"{case.label}: invalid response prefix/size")
    record = json.loads(line[3:], object_pairs_hook=unique_object)
    if not isinstance(record, dict) or set(record) != RESPONSE_KEYS:
        raise ValueError(f"{case.label}: snapshot keys differ")
    catalog, table = record["catalog"], record["table"]
    if case.operation == "catalog":
        if not isinstance(catalog, list) or len(catalog) != 13 or any(type(name) is not str for name in catalog):
            raise ValueError(f"{case.label}: invalid original catalogue")
    elif catalog is not None:
        raise ValueError(f"{case.label}: unexpected catalogue")
    if case.operation == "table":
        if not isinstance(table, str) or not re.fullmatch("[a-m]{4096}", table):
            raise ValueError(f"{case.label}: invalid original table")
    elif table is not None:
        raise ValueError(f"{case.label}: unexpected table")
    if case.operation in {"float", "double", "point", "chunk"}:
        if not isinstance(record["biome"], str) or not re.fullmatch("[a-m]", record["biome"]):
            raise ValueError(f"{case.label}: invalid biome identity")
    elif record["biome"] is not None:
        raise ValueError(f"{case.label}: unexpected biome result")
    if case.operation == "raw":
        if not isinstance(record["number"], str) or not re.fullmatch("[0-9a-f]{16}", record["number"]):
            raise ValueError(f"{case.label}: invalid raw binary64 result")
    elif record["number"] is not None:
        raise ValueError(f"{case.label}: unexpected scalar number")
    for slot in SLOTS:
        array = record[slot]
        if array is None:
            continue
        if not isinstance(array, dict) or set(array) != {"length", "values"}:
            raise ValueError(f"{case.label}: invalid {slot} descriptor")
        length, values = array["length"], array["values"]
        if type(length) is not int or not 0 <= length <= 4096:
            raise ValueError(f"{case.label}: invalid {slot} length")
        if slot in {"d", "o"}:
            if not isinstance(values, str) or len(values) != length or not re.fullmatch("[-a-m]*", values):
                raise ValueError(f"{case.label}: invalid {slot} identities")
        elif not isinstance(values, list) or len(values) != length or any(
                not isinstance(value, str) or not re.fullmatch("[0-9a-f]{16}", value) for value in values):
            raise ValueError(f"{case.label}: invalid {slot} binary64 values")
    for field, length in (("retained", 6), ("returned", 6), ("aliases", 15)):
        if not isinstance(record[field], str) or not re.fullmatch(f"[01]{{{length}}}", record[field]):
            raise ValueError(f"{case.label}: invalid {field} mask")
    if case.operation in {"init", "buffers"} and record["retained"] != "000000":
        raise ValueError(f"{case.label}: fixture retained mask must be fixed zero")
    expected_return = {"temperature": "000010", "biomes": "000001", "cached": "000100"}.get(case.operation, "000000")
    if record["returned"] != expected_return:
        raise ValueError(f"{case.label}: returned-array mask differs from the operation")
    return record


def first_difference(left: object, right: object, path: str = "snapshot") -> str | None:
    if type(left) is not type(right):
        return path + ": type differs"
    if isinstance(left, dict):
        if left.keys() != right.keys():
            return path + ": keys differ"
        for key in left:
            difference = first_difference(left[key], right[key], path + "." + key)
            if difference:
                return difference
    elif isinstance(left, list):
        if len(left) != len(right):
            return path + ": length differs"
        for index, (a, b) in enumerate(zip(left, right)):
            difference = first_difference(a, b, path + f"[{index}]")
            if difference:
                return difference
    elif left != right:
        return f"{path}: {str(left)[:100]!r} != {str(right)[:100]!r}"
    return None


class Comparison:
    def __init__(self, stage: Path, work: Path):
        self.stage, self.work = stage, work
        self.errors: list[str] = []
        self.artifacts: list[dict] = []
        self.records: dict[str, list[dict]] = {}

    def publish(self, name: str, content: str | bytes) -> None:
        if name not in OUTPUT_NAMES or name == "manifest.json":
            raise ValueError("unexpected artifact name")
        path = self.stage / name
        path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        digest = sha256(path)
        size = path.stat().st_size
        os.replace(path, self.work / name)
        self.artifacts.append({"file": name, "sha256": digest, "bytes": size})

    def batch(self, label: str, command: list[str], cases: list[Fixture], original: bool = False) -> list[dict]:
        if label not in LABELS or not 0 < len(cases) <= 8192:
            raise ValueError("invalid batch label/count")
        data = "\n".join(case.request for case in cases) + "\n"
        self.publish(label + "-requests.txt", data)
        result = run(command, data)
        self.publish(label + "-stdout.txt", result.stdout)
        self.publish(label + "-stderr.txt", result.stderr)
        if original and result.returncode == 77 and "UNAVAILABLE:" in result.stderr and not result.stdout.strip():
            raise SetupUnavailable(f"{label}: {result.stderr[:2000]}")
        if result.returncode:
            raise RuntimeError(f"{label}: process exited {result.returncode}: {result.stderr[:1000]}")
        lines = result.stdout.splitlines()
        if len(lines) != len(cases):
            raise RuntimeError(f"{label}: {len(lines)} responses for {len(cases)} requests")
        records = [parse_response(line, case) for line, case in zip(lines, cases)]
        self.records[label] = [{"label": case.label,
                                "response_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
                                "array_lengths": {slot: None if record[slot] is None else record[slot]["length"] for slot in SLOTS}}
                               for case, line, record in zip(cases, lines, records)]
        return records

    def compare(self, label: str, cases: list[Fixture], left: list[dict], right: list[dict]) -> int:
        if len(cases) != len(left) or len(cases) != len(right):
            raise RuntimeError("comparison record counts differ")
        matches = 0
        for case, expected, actual in zip(cases, left, right):
            difference = first_difference(expected, actual)
            if difference:
                self.errors.append(f"{label}/{case.label}: {difference}")
            else:
                matches += 1
        return matches

    def controls(self, label: str, cases: list[Fixture], records: list[dict]) -> dict:
        by_label = {case.label: record for case, record in zip(cases, records)}
        counts = {"equivalent": 0, "selector_distinction": 0, "preserved_fields": 0,
                  "temperature_path_agreement": 0, "point_cache_agreement": 0}
        previous = None
        for case, record in zip(cases, records):
            if case.equivalent_to:
                other = by_label[case.equivalent_to]
                fields = case.equal_fields or tuple(record)
                if any(record[field] != other[field] for field in fields):
                    self.errors.append(f"{label}/{case.label}: expected equality with {case.equivalent_to}")
                else:
                    counts["equivalent"] += 1
            if case.different_from:
                if record["biome"] == by_label[case.different_from]["biome"]:
                    self.errors.append(f"{label}/{case.label}: float selector and table lookup unexpectedly agree")
                else:
                    counts["selector_distinction"] += 1
            for field in case.preserve_fields:
                if previous is None or record[field] != previous[field]:
                    self.errors.append(f"{label}/{case.label}: expected unchanged {field}")
                else:
                    counts["preserved_fields"] += 1
            if case.temperature_from:
                size = int(case.request.split("\t")[3])
                temperature = by_label[case.temperature_from]["t"]
                if temperature is None or record["a"] is None or record["a"]["values"][:size * size] != temperature["values"][:size * size]:
                    self.errors.append(f"{label}/{case.label}: processed temperature paths differ")
                else:
                    counts["temperature_path_agreement"] += 1
            if case.operation in {"point", "chunk"}:
                if record["d"] is None or record["d"]["length"] < 1 or record["biome"] != record["d"]["values"][0]:
                    self.errors.append(f"{label}/{case.label}: scalar biome differs from actual cache element0")
                else:
                    counts["point_cache_agreement"] += 1
            previous = record
        return counts


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/biome")
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable, work = args.executable.resolve(), args.work_dir.resolve()
    jars = {"client": args.client.resolve(), "server": args.server.resolve()}
    source = root / "tests/reference/biome/BiomeProbe.java"
    sources = [source, Path(__file__).resolve(), Path(__file__).resolve().with_name("fixtures.py")]
    sources += [path for path in (root / "src/world/biome/biome.c", root / "include/mc/world/biome/biome.h",
                                 root / "tests/world/biome/biome_probe.c", root / "docs/reference/biome.md") if path.is_file()]
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
                "scope": "Actual classifier and explicit-seed climate fixture, not world startup, biome population or terrain",
                "fixture_setup": "Actual protected xv()/ph() plus independent actual ug/mq channels seeded with wrapped products9871/39811/543321 and layers4/4/2; no world or storage instance",
                "initialization_output": "Original stdout is routed to captured stderr only during classifier and fixture setup; bundled classpath resource reads and registration banners are retained",
                "harness_java_release": 11, "python": sys.version, "host": platform.platform(),
                "java": {"path": str(java) if java else None}, "javac": {"path": str(javac) if javac else None},
                "executable": {"path": str(executable)}, "jars": {side: {"path": str(path)} for side, path in jars.items()},
                "symbols": {"client": {"classifier": "kd", "manager": "xv", "octaves": "ug", "chunk_pair": "yy"},
                            "server": {"classifier": "gs", "manager": "ph", "octaves": "mq", "chunk_pair": "qa"}},
                "comparison_rules": ["All six array descriptors, lengths, exact binary64 bits and actual identity labels compare",
                                     "All thirteen names and all4096 actual table entries compare",
                                     "Retained/returned/alias masks compare for actual methods; fixture init/buffers retention is fixed000000",
                                     "Common client-to-C, server-to-C and client-to-server comparisons are separate",
                                     "Client-only raw temperature has its own C/client phase and no server counterpart claim",
                                     "Only valid unit-interval selectors, square biomes and bounded temperature rectangles are sent to originals"],
                "errors": [], "artifacts": [], "records": {}}
    comparison: Comparison | None = None

    def save() -> None:
        if comparison is not None:
            manifest["artifacts"], manifest["records"] = comparison.artifacts, comparison.records
        save_manifest(work, manifest)

    def unavailable(reason: str) -> int:
        manifest.update({"status": "unavailable", "reason": reason,
                         "finished_utc": datetime.now(timezone.utc).isoformat()})
        save()
        print("UNAVAILABLE: " + reason, file=sys.stderr)
        return 77

    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
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
        runtime, compiler = run([str(java), "-XshowSettings:properties", "-version"]), run([str(javac), "-version"])
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
        with tempfile.TemporaryDirectory(prefix=".biome-run-", dir=work) as temporary:
            stage = Path(temporary)
            comparison = Comparison(stage, work)
            classes = stage / "classes"
            classes.mkdir()
            compiled = run([str(javac), "--release", "11", "-encoding", "UTF-8", "-d", str(classes), str(source)])
            comparison.publish("compile-stdout.txt", compiled.stdout)
            comparison.publish("compile-stderr.txt", compiled.stderr)
            if compiled.returncode:
                raise RuntimeError("Java harness compilation failed: " + compiled.stderr[:1000])
            class_file = classes / "BiomeProbe.class"
            manifest["compiled_harness_sha256"] = sha256(class_file)
            comparison.publish("BiomeProbe.class", class_file.read_bytes())
            cases, raw_cases = fixtures(), client_fixtures()
            comparison.publish("fixtures.json", json.dumps([
                {"phase": phase, "label": case.label, "request": case.request, "coverage": case.coverage,
                 "equivalent_to": case.equivalent_to, "equal_fields": case.equal_fields,
                 "different_from": case.different_from, "preserve_fields": case.preserve_fields,
                 "temperature_from": case.temperature_from}
                for phase, items in (("common", cases), ("client-only", raw_cases)) for case in items], indent=2) + "\n")
            manifest["fixture_count"], manifest["client_only_fixture_count"] = len(cases), len(raw_cases)
            manifest["fixture_counts"] = dict(Counter(case.operation for case in cases))
            c_records = comparison.batch("c-reference", [str(executable)], cases)
            originals = {}
            for side, jar in jars.items():
                originals[side] = comparison.batch(side + "-reference", [str(java), "-cp", str(classes), "BiomeProbe", side, str(jar)], cases, original=True)
            c_raw = comparison.batch("c-client-only", [str(executable)], raw_cases)
            original_raw = comparison.batch("client-only-reference", [str(java), "-cp", str(classes), "BiomeProbe", "client", str(jars["client"])], raw_cases, original=True)
            manifest["matched_records"] = {
                "client_to_c": comparison.compare("client-to-C", cases, originals["client"], c_records),
                "server_to_c": comparison.compare("server-to-C", cases, originals["server"], c_records),
                "client_to_server": comparison.compare("client-to-server", cases, originals["client"], originals["server"]),
                "client_only_to_c": comparison.compare("client-only-to-C", raw_cases, original_raw, c_raw),
            }
            manifest["control_results"] = {"C": comparison.controls("C", cases, c_records),
                                          "client": comparison.controls("client", cases, originals["client"]),
                                          "server": comparison.controls("server", cases, originals["server"]),
                                          "C-client-only": comparison.controls("C-client-only", raw_cases, c_raw),
                                          "client-only": comparison.controls("client-only", raw_cases, original_raw)}
            table_index = next(i for i, case in enumerate(cases) if case.operation == "table")
            manifest["actual_table"] = {side: {"sha256": hashlib.sha256(records[table_index]["table"].encode()).hexdigest(),
                                                "counts": dict(Counter(records[table_index]["table"]))} for side, records in originals.items()}
    except SetupUnavailable as error:
        return unavailable(str(error))
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError, RecursionError) as error:
        manifest["errors"].append(str(error))
    if comparison is not None:
        manifest["errors"] += comparison.errors
    manifest.update({"status": "fail" if manifest["errors"] else "pass", "finished_utc": datetime.now(timezone.utc).isoformat()})
    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
    except OSError as error:
        print(f"FAIL: cannot write manifest: {error}", file=sys.stderr)
        return 1
    if manifest["errors"]:
        for error in manifest["errors"][:30]:
            print("FAIL: " + error, file=sys.stderr)
        print(f"{len(manifest['errors'])} failures; full report: {work}", file=sys.stderr)
        return 1
    print(f"PASS: {len(cases)} common snapshots each for C/client/server and {len(raw_cases)} separate client-only snapshots; all state fields exact")
    print(f"Explicit-seed numerical fixture only; full report: {work / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
