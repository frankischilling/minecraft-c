#!/usr/bin/env python3
"""Compare fixed normal density and base blocks with isolated original methods."""

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
MAX_INPUT_BYTES = 1024 * 1024
MAX_OUTPUT_BYTES = 64 * 1024 * 1024
MAX_WORLD_ENTRIES = 8192
MAX_WORLD_BYTES = 128 * 1024 * 1024
LABELS = ("c-reference", "client-reference", "server-reference")
OUTPUT_NAMES = ("manifest.json", "compile-stdout.txt", "compile-stderr.txt",
                "DensityProbe.class", "fixtures.json")
OUTPUT_NAMES += tuple(label + "-" + suffix for label in LABELS
                      for suffix in ("requests.txt", "stdout.txt", "stderr.txt"))
SPANS = ("d", "e", "f", "g", "h", "density", "blocks")
CHANNELS = dict(zip("klmnoabc", (16, 16, 8, 4, 4, 10, 16, 8)))
RESPONSE_KEYS = {"state", "continuation", "climate", *SPANS, "retained"}
BASE_CONTROL = "CONTROL_BASE_RECOMPUTE q425_exact scratch_exact"
ZERO_BITS = "0000000000000000"


def sha256(path: Path, maximum: int | None = None) -> str:
    digest = hashlib.sha256()
    count = 0
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            count += len(chunk)
            if maximum is not None and count > maximum:
                raise RuntimeError("world file exceeds inventory byte budget: " + str(path))
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


def run(command: list[str], data: str | None = None) -> ProcessResult:
    encoded = None if data is None else data.encode("utf-8")
    if encoded is not None and len(encoded) > MAX_INPUT_BYTES:
        raise RuntimeError("probe request batch exceeds input limit")
    # Temporary files avoid pipe deadlocks. Retain bounded output, including
    # partial protocol/setup diagnostics when subprocess.run kills a timeout.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        timed_out = False
        returncode = None
        try:
            process = subprocess.run(command, input=encoded, stdout=stdout, stderr=stderr,
                                     timeout=60, check=False)
            returncode = process.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read(MAX_OUTPUT_BYTES + 1)
        errors = stderr.read(MAX_OUTPUT_BYTES + 1)
        limited = len(output) > MAX_OUTPUT_BYTES or len(errors) > MAX_OUTPUT_BYTES
        return ProcessResult(returncode, output[:MAX_OUTPUT_BYTES], errors[:MAX_OUTPUT_BYTES],
                             timed_out, limited)


def checked_text(result: ProcessResult, purpose: str) -> tuple[str, str]:
    if result.output_limited:
        raise RuntimeError(purpose + ": process output exceeds limit")
    if result.timed_out:
        raise RuntimeError(purpose + ": process timed out")
    if result.returncode:
        raise RuntimeError(purpose + f": process exited {result.returncode}")
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
    descriptor, name = tempfile.mkstemp(prefix=".density-manifest-", suffix=".tmp", dir=work)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, indent=2, ensure_ascii=True)
            stream.write("\n")
        os.replace(temporary, work / "manifest.json")
    finally:
        temporary.unlink(missing_ok=True)


def inventory(root: Path) -> dict:
    """Read bounded metadata/hashes without following links or deleting worlds."""
    result = {"root": str(root), "exists": False, "entries": [], "file_bytes": 0}
    if root.is_symlink():
        raise RuntimeError("owned world root became a symlink: " + str(root))
    if not root.exists():
        return result
    if not root.is_dir():
        raise RuntimeError("owned world root is not a directory: " + str(root))
    result["exists"] = True
    pending = [root]
    while pending:
        directory = pending.pop()
        with os.scandir(directory) as children:
            for child in children:
                if len(result["entries"]) >= MAX_WORLD_ENTRIES:
                    raise RuntimeError("world inventory exceeds entry budget")
                path = Path(child.path)
                relative = path.relative_to(root).as_posix()
                if child.is_symlink():
                    raise RuntimeError("unexpected world symlink: " + str(path))
                if child.is_dir(follow_symlinks=False):
                    result["entries"].append({"path": relative, "kind": "directory"})
                    pending.append(path)
                elif child.is_file(follow_symlinks=False):
                    size = child.stat(follow_symlinks=False).st_size
                    remaining = MAX_WORLD_BYTES - result["file_bytes"]
                    if size > remaining:
                        raise RuntimeError("world inventory exceeds byte budget")
                    digest = sha256(path, remaining)
                    result["entries"].append({"path": relative, "kind": "file", "bytes": size,
                                              "sha256": digest})
                    result["file_bytes"] += size
                else:
                    raise RuntimeError("unexpected world filesystem entry: " + str(path))
    result["entries"].sort(key=lambda entry: entry["path"])
    return result


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def bit_values(values: object, length: int) -> bool:
    return isinstance(values, list) and len(values) == length and all(
        isinstance(value, str) and re.fullmatch("[0-9a-f]{16}", value) for value in values)


def parse_response(line: str, case: Fixture) -> dict:
    if len(line) > 1024 * 1024 or not line.startswith("OK\t"):
        raise ValueError(f"{case.label}: invalid response prefix/size")
    record = json.loads(line[3:], object_pairs_hook=unique_object)
    if not isinstance(record, dict) or set(record) != RESPONSE_KEYS:
        raise ValueError(f"{case.label}: snapshot keys differ")
    state = record["state"]
    if case.operation in {"init", "state"}:
        if not isinstance(state, dict) or set(state) != set(CHANNELS):
            raise ValueError(f"{case.label}: constructor channel keys differ")
        for channel, count in CHANNELS.items():
            layers = state[channel]
            if not isinstance(layers, list) or len(layers) != count:
                raise ValueError(f"{case.label}: {channel} layer count differs")
            for layer in layers:
                if not isinstance(layer, dict) or set(layer) != {"offsets", "permutation"}:
                    raise ValueError(f"{case.label}: invalid layer keys")
                if not bit_values(layer["offsets"], 3):
                    raise ValueError(f"{case.label}: invalid constructor offsets")
                permutation = layer["permutation"]
                if not isinstance(permutation, list) or len(permutation) != 512 or any(
                        type(value) is not int or not 0 <= value <= 255 for value in permutation):
                    raise ValueError(f"{case.label}: invalid permutation entries")
                if sorted(permutation[:256]) != list(range(256)) or permutation[:256] != permutation[256:]:
                    raise ValueError(f"{case.label}: permutation duplication/integrity differs")
    elif state is not None:
        raise ValueError(f"{case.label}: unexpected constructor snapshot")
    if not bit_values(record["continuation"], 4):
        raise ValueError(f"{case.label}: invalid Random continuation")
    climate = record["climate"]
    if climate is not None:
        if not isinstance(climate, dict) or set(climate) != {"temperature", "rainfall", "auxiliary", "biomes"}:
            raise ValueError(f"{case.label}: invalid climate keys")
        if any(not bit_values(climate[key], 256) for key in ("temperature", "rainfall", "auxiliary")):
            raise ValueError(f"{case.label}: invalid climate binary64 values")
        if not isinstance(climate["biomes"], str) or not re.fullmatch("[a-m]{256}", climate["biomes"]):
            raise ValueError(f"{case.label}: invalid biome identities")
    if case.operation == "init" and climate is not None:
        raise ValueError(f"{case.label}: init climate must be unobserved")
    if case.operation in {"density", "base", "generate"} and climate is None:
        raise ValueError(f"{case.label}: missing observed climate")
    for name in SPANS:
        array = record[name]
        if array is None:
            if case.operation in {"buffers", "density", "base", "generate"}:
                raise ValueError(f"{case.label}: missing {name} span")
            continue
        if case.operation == "init":
            raise ValueError(f"{case.label}: init spans must be null")
        if not isinstance(array, dict) or set(array) != {"length", "values"}:
            raise ValueError(f"{case.label}: invalid {name} descriptor")
        minimum, maximum = (32768, 32832) if name == "blocks" else (25, 64) if name in {"g", "h"} else (425, 512)
        length, values = array["length"], array["values"]
        if type(length) is not int or not minimum <= length <= maximum:
            raise ValueError(f"{case.label}: invalid {name} length")
        if name == "blocks":
            if not isinstance(values, str) or not re.fullmatch("[0-9a-f]{" + str(length * 2) + "}", values):
                raise ValueError(f"{case.label}: invalid block bytes")
        elif not bit_values(values, length):
            raise ValueError(f"{case.label}: invalid {name} binary64 values")
    mask = record["retained"]
    if not isinstance(mask, str) or not re.fullmatch("[01]{7}", mask):
        raise ValueError(f"{case.label}: invalid retained mask")
    if case.operation in {"init", "buffers"} and mask != "0000000":
        raise ValueError(f"{case.label}: fixture retention must be fixed zero")
    if case.operation in {"density", "base", "generate"} and mask != "1111111":
        raise ValueError(f"{case.label}: valid sampling replaced a supplied span")
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


def validate_cases(cases: list[Fixture]) -> None:
    labels = {case.label for case in cases}
    if not 0 < len(cases) <= 1024 or len(labels) != len(cases):
        raise ValueError("invalid fixture count/labels")
    initialized, buffered, matching = False, False, None
    init_count = 0
    for case in cases:
        if len(case.request) > 256 or any(character in case.request for character in "\r\n"):
            raise ValueError("request line exceeds protocol bounds")
        fields = case.request.split("\t")
        expected = {"init": 2, "state": 1, "buffers": 8, "density": 3, "base": 3, "generate": 3}.get(fields[0])
        if len(fields) != expected:
            raise ValueError("invalid fixture operation/field count")
        if any(not re.fullmatch(r"-?(?:0|[1-9][0-9]*)", value) for value in fields[1:]):
            raise ValueError("invalid decimal request argument")
        values = [int(value) for value in fields[1:]]
        if case.operation == "init":
            if not -(1 << 63) <= values[0] < 1 << 63:
                raise ValueError("seed exceeds Java long")
            initialized, buffered, matching = True, False, None
            init_count += 1
        elif not initialized:
            raise ValueError("fixture operation precedes init")
        elif case.operation == "buffers":
            for name, value in zip(SPANS, values):
                minimum, maximum = (32768, 32832) if name == "blocks" else (25, 64) if name in {"g", "h"} else (425, 512)
                if not minimum <= value <= maximum:
                    raise ValueError("fixture span exceeds valid bounded capacities")
            buffered, matching = True, None
        elif case.operation in {"density", "base", "generate"}:
            if not buffered or any(not -(1 << 31) <= value < 1 << 31 for value in values):
                raise ValueError("invalid sampling fixture coordinates/buffers")
            if case.operation == "base" and matching != tuple(values):
                raise ValueError("base fixture lacks matching density")
            matching = tuple(values)
        if case.equivalent_to is not None and case.equivalent_to not in labels:
            raise ValueError("fixture equivalence target missing")
        if any(field not in RESPONSE_KEYS for field in (*case.equal_fields, *case.preserve_fields)):
            raise ValueError("unknown fixture control field")
    if init_count > 32:
        raise ValueError("fixture world setup count exceeds limit")
    if len(("\n".join(case.request for case in cases) + "\n").encode("utf-8")) > MAX_INPUT_BYTES:
        raise ValueError("fixture request batch exceeds byte budget")


class SetupUnavailable(RuntimeError):
    def __init__(self, message: str, label: str, records: list[dict]):
        super().__init__(message)
        self.label, self.records = label, records


def setup_progress(stderr: str, cases: list[Fixture], root: Path) -> tuple[int, int | None]:
    initializations = [(index, int(case.request.split("\t")[1]))
                       for index, case in enumerate(cases) if case.operation == "init"]
    begun, finished, active = 0, 0, None
    for line in stderr.splitlines():
        if line.startswith("SETUP_BEGIN"):
            match = re.fullmatch(r"SETUP_BEGIN seed=(-?[0-9]+) root=(.+)", line)
            if match is None or active is not None or begun >= len(initializations):
                raise RuntimeError("invalid original setup-begin marker")
            index, seed = initializations[begun]
            expected_root = root / f"seed-{begun + 1:03d}"
            if int(match.group(1)) != seed or Path(match.group(2)) != expected_root:
                raise RuntimeError("original setup marker seed/root differs from its request")
            active = index
            begun += 1
        elif line.startswith("SETUP_DONE"):
            match = re.fullmatch(r"SETUP_DONE seed=(-?[0-9]+)", line)
            if match is None or active is None or int(match.group(1)) != initializations[finished][1]:
                raise RuntimeError("invalid original setup-done marker")
            active = None
            finished += 1
    return finished, active


def cardinalities(records: list[dict]) -> dict:
    counts = Counter(binary64_values=0, permutation_entries=0, continuation_longs=0,
                     block_bytes=0, biome_identities=0)
    for record in records:
        counts["continuation_longs"] += len(record["continuation"])
        if record["state"] is not None:
            for layers in record["state"].values():
                for layer in layers:
                    counts["binary64_values"] += len(layer["offsets"])
                    counts["permutation_entries"] += len(layer["permutation"])
        if record["climate"] is not None:
            for name in ("temperature", "rainfall", "auxiliary"):
                counts["binary64_values"] += len(record["climate"][name])
            counts["biome_identities"] += len(record["climate"]["biomes"])
        for name in SPANS:
            if record[name] is not None:
                counts["block_bytes" if name == "blocks" else "binary64_values"] += record[name]["length"]
    return dict(counts)


class Comparison:
    def __init__(self, stage: Path, work: Path):
        self.stage, self.work = stage, work
        self.errors: list[str] = []
        self.artifacts: list[dict] = []
        self.records: dict[str, list[dict]] = {}
        self.worlds: dict[str, dict] = {}
        self.processes: dict[str, dict] = {}

    def publish(self, name: str, content: str | bytes) -> None:
        if name not in OUTPUT_NAMES or name == "manifest.json":
            raise ValueError("unexpected artifact name")
        path = self.stage / name
        path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        digest, size = sha256(path), path.stat().st_size
        os.replace(path, self.work / name)
        self.artifacts.append({"file": name, "sha256": digest, "bytes": size})

    def batch(self, label: str, command: list[str], cases: list[Fixture], root: Path | None = None) -> list[dict]:
        if label not in LABELS:
            raise ValueError("invalid batch label")
        self.publish(label + "-requests.txt", "\n".join(case.request for case in cases) + "\n")
        try:
            result = run(command, "\n".join(case.request for case in cases) + "\n")
        finally:
            if root is not None:
                try:
                    self.worlds[label] = inventory(root)
                except (OSError, RuntimeError) as error:
                    self.worlds[label] = {"root": str(root), "inventory_error": str(error)}
                    self.errors.append(label + ": " + str(error))
        self.publish(label + "-stdout.txt", result.stdout)
        self.publish(label + "-stderr.txt", result.stderr)
        self.processes[label] = {"command": command, "returncode": result.returncode,
                                 "timed_out": result.timed_out, "output_limited": result.output_limited}
        if result.output_limited:
            raise RuntimeError(label + ": process output exceeds limit")
        stdout, stderr = result.stdout.decode("utf-8"), result.stderr.decode("utf-8")
        lines = stdout.splitlines()
        if len(lines) > len(cases):
            raise RuntimeError(f"{label}: too many protocol responses")
        records = []
        self.records[label] = []
        for case, line in zip(cases, lines):
            record = parse_response(line, case)
            records.append(record)
            self.records[label].append({"label": case.label,
                                        "response_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
                                        "span_lengths": {name: None if record[name] is None else record[name]["length"] for name in SPANS}})
        if root is not None:
            finished, active = setup_progress(stderr, cases, root)
            expected_controls = sum(case.operation in {"base", "generate"} for case in cases[:len(records)])
            actual_controls = stderr.splitlines().count(BASE_CONTROL)
            if actual_controls != expected_controls:
                raise RuntimeError(f"{label}: {actual_controls} base recomputation controls for {expected_controls} completed requests")
            self.processes[label].update(setup_done=finished, base_recompute_controls=actual_controls)
            next_init = len(records) < len(cases) and cases[len(records)].operation == "init"
            global_unavailable = not records and active is None and finished == 0 and any(
                line in {"UNAVAILABLE: original class setup failed", "UNAVAILABLE: original JAR missing or SHA-256 mismatch"}
                for line in stderr.splitlines())
            world_unavailable = next_init and active == len(records) and any(
                line == "UNAVAILABLE: genuine original world/generator setup failed for seed=" + cases[len(records)].request.split("\t")[1]
                for line in stderr.splitlines())
            if result.returncode == 77 and (global_unavailable or world_unavailable):
                raise SetupUnavailable(label + ": original setup unavailable; see captured stderr", label, records)
            if result.timed_out and next_init and active == len(records):
                raise SetupUnavailable(label + ": original world setup timed out before SETUP_DONE", label, records)
            if not result.returncode and not result.timed_out and (active is not None or finished != sum(case.operation == "init" for case in cases)):
                raise RuntimeError(label + ": original setup marker count differs")
        if result.timed_out:
            raise RuntimeError(label + ": process timed out during numerical execution")
        if result.returncode:
            raise RuntimeError(f"{label}: process exited {result.returncode}: {stderr[:1000]}")
        if len(records) != len(cases):
            raise RuntimeError(f"{label}: {len(records)} responses for {len(cases)} requests")
        return records

    def compare(self, label: str, cases: list[Fixture], left: list[dict], right: list[dict]) -> int:
        if len(cases) != len(left) or len(cases) != len(right):
            raise RuntimeError("comparison record counts differ")
        matched = 0
        for case, expected, actual in zip(cases, left, right):
            difference = first_difference(expected, actual)
            if difference:
                self.errors.append(f"{label}/{case.label}: {difference}")
            else:
                matched += 1
        return matched

    def controls(self, label: str, cases: list[Fixture], records: list[dict]) -> dict:
        by_label = {case.label: record for case, record in zip(cases, records)}
        counts = Counter()
        previous, initial = None, None

        def check(condition: bool, name: str, case: Fixture, detail: str) -> None:
            if condition:
                counts[name] += 1
            else:
                self.errors.append(f"{label}/{case.label}: {detail}")

        for case, record in zip(cases, records):
            if case.operation == "init":
                initial = record
            else:
                check(initial is not None and record["continuation"] == initial["continuation"],
                      "random_unchanged", case, "sampling consumed/reseeded constructor Random")
            if case.operation == "state":
                check(initial is not None and record["state"] == initial["state"],
                      "constructor_unchanged", case, "constructor state changed")
                if previous is not None:
                    check(all(record[name] == previous[name] for name in (*SPANS, "climate")),
                          "state_observation_preserved", case, "state observation changed buffers/climate")
                    check(record["retained"] == "".join("0" if previous[name] is None else "1" for name in SPANS),
                          "state_retention", case, "state observation retention differs")
            if case.equivalent_to is not None and case.equivalent_to in by_label:
                other = by_label[case.equivalent_to]
                fields = case.equal_fields or tuple(record)
                check(all(record[field] == other[field] for field in fields), "equivalent", case,
                      "expected equivalence with " + case.equivalent_to)
            for field in case.preserve_fields:
                check(previous is not None and record[field] == previous[field], "preserved_fields", case,
                      "expected unchanged " + field)
            if case.operation in {"density", "generate"} and previous is not None:
                for name in SPANS[:5]:
                    minimum = 25 if name in {"g", "h"} else 425
                    check(all(value == ZERO_BITS for value in record[name]["values"][minimum:]),
                          "scratch_zero_suffix", case, "scratch suffix not cleared: " + name)
                check(record["density"]["length"] == previous["density"]["length"] and
                      record["density"]["values"][425:] == previous["density"]["values"][425:],
                      "density_suffix_retained", case, "density suffix/length changed")
            if case.operation in {"base", "generate"} and previous is not None:
                check(record["blocks"]["length"] == previous["blocks"]["length"] and
                      record["blocks"]["values"][32768 * 2:] == previous["blocks"]["values"][32768 * 2:],
                      "block_suffix_retained", case, "block suffix/length changed")
                prefix = bytes.fromhex(record["blocks"]["values"][:32768 * 2])
                check(set(prefix) <= {0, 1, 9, 79}, "base_block_domain", case, "unexpected base block ID")
            if case.operation == "density" and previous is not None:
                check(record["blocks"] == previous["blocks"], "density_preserves_blocks", case,
                      "density modified blocks")
            if case.operation == "base" and previous is not None:
                check(all(record[name] == previous[name] for name in (*SPANS[:6], "climate")),
                      "base_preserves_inputs", case, "base changed density/scratch/climate")
            previous = record
        return dict(counts)


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/density")
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch("[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable, work = args.executable.resolve(), args.work_dir.resolve()
    jars = {side: path.resolve() for side, path in (("client", args.client), ("server", args.server))}
    source = root / "tests/reference/density/DensityProbe.java"
    sources = [source, Path(__file__).resolve(), Path(__file__).with_name("fixtures.py")]
    sources += [root / name for name in (
        "src/world/terrain/terrain.c", "include/mc/world/terrain/terrain.h",
        "src/world/noise/noise.c", "include/mc/world/noise/noise.h",
        "src/world/noise/simplex.c", "include/mc/world/noise/simplex.h",
        "src/world/biome/biome.c", "include/mc/world/biome/biome.h",
        "src/core/java/java.c", "include/mc/core/java/java.h",
        "tests/world/terrain/terrain_probe.c", "docs/reference/density.md") if (root / name).is_file()]
    protected = {path.resolve() for path in [executable, *jars.values(), *sources, java, javac] if path is not None}
    protected.update(Path(value).resolve() for value in (args.java, args.javac)
                     if value and ("/" in value or "\\" in value))
    world_base = (root / ".local/reference/density-setup").resolve()
    try:
        if root not in world_base.parents:
            raise ValueError("world setup directory resolves outside workspace")
        if any(path == work or work in path.parents for path in protected):
            raise ValueError("work directory contains supplied inputs or source files")
        if any(path == world_base or world_base in path.parents for path in protected):
            raise ValueError("world setup directory contains supplied inputs or source files")
        if work == world_base or work in world_base.parents or world_base in work.parents:
            raise ValueError("report work directory overlaps world setup directory")
        if any(same_input(work / name, protected) for name in OUTPUT_NAMES):
            raise ValueError("output paths overlap supplied inputs or source files")
    except (OSError, ValueError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        return 1

    manifest = {"schema": 1, "protocol": 1, "run_id": uuid.uuid4().hex, "status": "running",
                "started_utc": datetime.now(timezone.utc).isoformat(),
                "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
                "commit_record": "supplied by caller" if args.tested_commit else "unrecorded",
                "scope": "Fixed normal density and base interpolation; no surface, cave, population, lighting or save-compatibility claim",
                "fixture_setup": "Genuine original world/normal provider/manager plus a fresh actual seeded yf/pk generator; server spawn effects are not measured generator state",
                "world_policy": "New unique immediate children of workspace/.local/reference/density-setup; retained after success or failure; no world deletion/move/parsing",
                "world_setup_base": str(world_base), "workspace": str(root),
                "harness_java_release": 11, "python": sys.version, "host": platform.platform(),
                "limits": {"input_bytes": MAX_INPUT_BYTES, "retained_output_bytes_per_stream": MAX_OUTPUT_BYTES,
                           "child_seconds": 60, "requests": 1024, "request_characters": 256,
                           "world_inventory_entries": MAX_WORLD_ENTRIES, "world_inventory_bytes": MAX_WORLD_BYTES},
                "java": {"path": str(java) if java else None}, "javac": {"path": str(javac) if javac else None},
                "executable": {"path": str(executable)},
                "jars": {side: {"path": str(path)} for side, path in jars.items()},
                "comparison_rules": ["Exact response cardinality and every JSON field compare independently for client-to-C, server-to-C and client-to-server",
                                     "Raw binary64 bits, all 82 constructor layers, continuation longs, complete spans and climate identities compare without normalization",
                                     "Original base requests must prove internal q425 and full scratch recomputation equal the preceding observed density and scratch",
                                     "Scratch suffixes clear, final density/block suffixes retain, state and Random remain unchanged"],
                "errors": [], "artifacts": [], "records": {}, "owned_world_runs": {}}
    comparison = None
    snapshots = {}
    cases = []
    input_hashes: dict[Path, str] = {}
    unavailable_reason = None

    def save() -> None:
        if comparison is not None:
            manifest.update(artifacts=comparison.artifacts, records=comparison.records,
                            owned_world_runs=comparison.worlds, processes=comparison.processes)
        save_manifest(work, manifest)

    def remember(path: Path) -> str:
        value = sha256(path)
        input_hashes[path.resolve()] = value
        return value

    try:
        work.mkdir(parents=True, exist_ok=True)
        save()  # Persist running before any supplied process can execute.
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
            for key, path in (("java", java), ("javac", javac), ("executable", executable)):
                manifest[key]["sha256"] = remember(path)
            manifest["sources"] = [{"file": path.relative_to(root).as_posix(), "sha256": remember(path)} for path in sources]
            runtime = checked_text(run([str(java), "-XshowSettings:properties", "-version"]), "Java version")
            compiler = checked_text(run([str(javac), "-version"]), "javac version")
            manifest["runtime_info"], manifest["compiler_info"] = "".join(runtime), "".join(compiler)
            runtime_major = java_major(manifest["runtime_info"], "java")
            compiler_major = java_major(manifest["compiler_info"], "javac")
            manifest["java"]["major"], manifest["javac"]["major"] = runtime_major, compiler_major
            if runtime_major < 11 or compiler_major < 11:
                unavailable_reason = "Java and javac 11 or newer required"
            else:
                for path in (java.parent.parent / "release", java.parent.parent / "lib/modules"):
                    if path.is_file():
                        manifest["java"].setdefault("runtime_files", []).append({"path": str(path), "sha256": remember(path)})
                cases = fixtures()
                validate_cases(cases)
                manifest["fixture_count"] = len(cases)
                manifest["fixture_counts"] = dict(Counter(case.operation for case in cases))
                world_base.mkdir(parents=True, exist_ok=True)
                if world_base.resolve() != world_base or root not in world_base.parents:
                    raise RuntimeError("world setup directory changed canonical location")
                with tempfile.TemporaryDirectory(prefix=".density-run-", dir=work) as temporary:
                    stage = Path(temporary)
                    comparison = Comparison(stage, work)
                    classes = stage / "classes"
                    classes.mkdir()
                    compiled = run([str(javac), "--release", "11", "-encoding", "UTF-8", "-d", str(classes), str(source)])
                    comparison.publish("compile-stdout.txt", compiled.stdout)
                    comparison.publish("compile-stderr.txt", compiled.stderr)
                    checked_text(compiled, "Java harness compilation")
                    class_file = classes / "DensityProbe.class"
                    manifest["compiled_harness_sha256"] = sha256(class_file)
                    comparison.publish("DensityProbe.class", class_file.read_bytes())
                    comparison.publish("fixtures.json", json.dumps([asdict(case) for case in cases], indent=2) + "\n")
                    snapshots["c-reference"] = comparison.batch("c-reference", [str(executable)], cases)
                    manifest["control_results"] = {"C": comparison.controls("C", cases, snapshots["c-reference"])}
                    for side, jar in jars.items():
                        label = side + "-reference"
                        owned = world_base / (manifest["run_id"] + "-" + side)
                        if owned.exists() or owned.is_symlink() or owned.parent != world_base or owned.resolve() != owned:
                            raise RuntimeError("owned world root must be a fresh immediate child")
                        manifest.setdefault("reserved_world_roots", {})[label] = str(owned)
                        save()
                        command = [str(java), "-Xms32m", "-Xmx512m", "-Djava.awt.headless=true",
                                   "-cp", str(classes), "DensityProbe", side, str(jar), str(owned), str(root)]
                        snapshots[label] = comparison.batch(label, command, cases, owned)
                        manifest["control_results"][side] = comparison.controls(side, cases, snapshots[label])
                        manifest.setdefault("matched_records", {})[side + "_to_c"] = comparison.compare(
                            side + "-to-C", cases, snapshots[label], snapshots["c-reference"])
                        save()
                    manifest["matched_records"]["client_to_server"] = comparison.compare(
                        "client-to-server", cases, snapshots["client-reference"], snapshots["server-reference"])
                    manifest["cardinalities"] = {label: cardinalities(records) for label, records in snapshots.items()}
    except SetupUnavailable as error:
        unavailable_reason = str(error)
        if comparison is not None and error.records:
            prefix = cases[:len(error.records)]
            comparison.compare(error.label + "-completed-to-C", prefix, error.records,
                               snapshots["c-reference"][:len(prefix)])
            if error.label == "server-reference" and "client-reference" in snapshots:
                comparison.compare("completed-client-to-server", prefix,
                                   snapshots["client-reference"][:len(prefix)], error.records)
            manifest.setdefault("control_results", {})[error.label + "-completed"] = comparison.controls(error.label, prefix, error.records)
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError, RecursionError) as error:
        manifest["errors"].append(str(error))

    # Preserve the exact input/source attribution even for partial/unavailable
    # runs, and refuse a pass if any supplied input changed during execution.
    manifest["input_preservation"] = []
    for path, before in input_hashes.items():
        try:
            after = sha256(path)
            manifest["input_preservation"].append({"path": str(path), "before": before, "after": after})
            if after != before:
                manifest["errors"].append("input changed during comparison: " + str(path))
        except OSError as error:
            manifest["errors"].append("cannot verify input preservation: " + str(error))
    if comparison is not None:
        manifest["errors"] += comparison.errors
    status = "fail" if manifest["errors"] else "unavailable" if unavailable_reason else "pass"
    manifest.update(status=status, finished_utc=datetime.now(timezone.utc).isoformat())
    if unavailable_reason:
        manifest["reason"] = unavailable_reason
    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
    except OSError as error:
        print("FAIL: cannot write manifest: " + str(error), file=sys.stderr)
        return 1
    if status == "fail":
        for error in manifest["errors"][:30]:
            print("FAIL: " + error, file=sys.stderr)
        print(f"{len(manifest['errors'])} failures; full report: {work}", file=sys.stderr)
        return 1
    if status == "unavailable":
        print("UNAVAILABLE: " + str(unavailable_reason), file=sys.stderr)
        return 77
    print(f"PASS: {len(cases)} exact snapshots each for C/client/server; constructor, climate, density and base blocks")
    print("Full report: " + str(work / "manifest.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
