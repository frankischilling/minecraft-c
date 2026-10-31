"""Compare native AABB helper values/identity with the genuine paired jars.

No native compilation, source regeneration, reference rewriting, World creation,
or game launch occurs. javac compiles only the reflection observer. Each output
directory is new and is left in place, including failed or partial attempts.
Raw binary64 comparison includes a complete two-NaN operand pair matrix. The
native build's separate capture must establish its executable-to-source join.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import time


JARS = {
    "CLIENT": (
        "b1.7.3.jar",
        1465375,
        "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    ),
    "SERVER": (
        "b1.7.3_server.jar",
        503100,
        "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
    ),
}
OPERATIONS = (
    "get", "sweep", "expand", "shift", "contract", "copy", "translate",
    "assign", "clip_x", "clip_y", "clip_z", "intersects", "reset",
)
SPECIAL_BITS = (
    "0000000000000000", "8000000000000000", "0000000000000001",
    "8000000000000001", "0010000000000000", "8010000000000000",
    "3fefffffffffffff", "3ff0000000000000", "3ff0000000000001",
    "bff0000000000000", "7fefffffffffffff", "ffefffffffffffff",
    "7ff0000000000000", "fff0000000000000", "7ff8000000000000",
    "7ff8000000000042", "fff8000000000017", "7ff0000000000001",
)
NAN_MATRIX_BITS = (
    "7ff8000000000001", "7ff8000000000042",  # Positive quiet, two payloads.
    "fff8000000000001", "fff8000000000042",  # Negative quiet, two payloads.
    "7ff0000000000001", "7ff0000000000042",  # Positive signaling, two payloads.
    "fff0000000000001", "fff0000000000042",  # Negative signaling, two payloads.
)


def pin(path: Path) -> dict:
    data = path.read_bytes()
    return {"path": str(path.resolve()), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest()}


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def bits(value: float | int) -> str:
    return struct.pack(">d", float(value)).hex()


def scenarios() -> list[dict]:
    unit = [bits(v) for v in (0, 0, 0, 1, 1, 1)]
    result = []

    def add(label, first, second, delta):
        result.append({"label": label, "a": first, "b": second, "delta": delta})

    add("unit-self-positive", unit, unit, list(map(bits, (3, 4, 5))))
    add("unit-self-negative", unit, unit, list(map(bits, (-3, -4, -5))))
    add("negative-zero-delta", unit, unit, ["8000000000000000"] * 3)
    add("positive-zero-delta", unit, unit, ["0000000000000000"] * 3)
    add("all-negative-zero", ["8000000000000000"] * 6, unit,
        ["8000000000000000"] * 3)
    add("inverted", list(map(bits, (3, 4, 5, -3, -4, -5))), unit,
        list(map(bits, (2, -2, 0.25))))
    add("collapsed", [bits(0)] * 6, unit, list(map(bits, (2, -2, 1))))
    add("all-nan", ["7ff8000000000042"] * 6,
        ["fff8000000000017"] * 6, ["7ff8000000000042"] * 3)
    add("all-infinite", ["fff0000000000000"] * 3 + ["7ff0000000000000"] * 3,
        unit, ["7ff0000000000000", "fff0000000000000", bits(0)])
    for axis in range(3):
        for direction in (-1, 1):
            moving = [0, 0, 0, 1, 1, 1]
            moving[axis] = -2 if direction == 1 else 2
            moving[axis + 3] = -1 if direction == 1 else 3
            add(f"axis-{axis}-direction-{direction}-gap", unit,
                list(map(bits, moving)), [bits(direction * 5)] * 3)
            moving[axis] = -1 if direction == 1 else 1
            moving[axis + 3] = 0 if direction == 1 else 2
            add(f"axis-{axis}-direction-{direction}-touch", unit,
                list(map(bits, moving)), [bits(direction * 5)] * 3)
            # A touching cross-axis must reject even for a motion-axis gap.
            cross = (axis + 1) % 3
            moving[cross], moving[cross + 3] = 1, 2
            add(f"axis-{axis}-direction-{direction}-cross-touch", unit,
                list(map(bits, moving)), [bits(direction * 5)] * 3)
    for index, special in enumerate(SPECIAL_BITS):
        first = unit.copy()
        second = unit.copy()
        first[index % 6] = special
        second[(index + 2) % 6] = special
        delta = [bits(0.25), bits(-0.75), bits(2)]
        delta[index % 3] = special
        add(f"special-{index}", first, second, delta)
    generator = random.Random(0xB173)
    pool = [bits(v) for v in (-100, -2, -1, -0.5, 0, 0.5, 1, 2, 100)]
    pool.extend(SPECIAL_BITS)
    for index in range(160):
        values = [generator.choice(pool) for _ in range(15)]
        add(f"seeded-special-{index}", values[:6], values[6:12], values[12:])
    for index in range(32):
        values = [f"{generator.getrandbits(64):016x}" for _ in range(15)]
        add(f"seeded-fullwidth-{index}", values[:6], values[6:12], values[12:])
    # The 8x8 matrix uses each coordinate slot separately and the corresponding
    # delta axis. All operations and alias modes exercise addition/subtraction,
    # passive assignments and NaN-rejected sweep branches against every
    # quiet/signaling/sign/payload operand combination.
    for left_index, coordinate_nan in enumerate(NAN_MATRIX_BITS):
        for right_index, delta_nan in enumerate(NAN_MATRIX_BITS):
            for axis in range(3):
                for endpoint in range(2):
                    first = unit.copy()
                    first[axis + endpoint * 3] = coordinate_nan
                    delta = list(map(bits, (0.25, -0.75, 2)))
                    delta[axis] = delta_nan
                    add(f"nan-matrix-left-{left_index}-right-{right_index}-axis-{axis}-end-{endpoint}",
                        first, unit.copy(), delta)
    return result


def cases(side: str) -> list[dict]:
    rows = []
    for scenario in scenarios():
        for operation in OPERATIONS + (("clear",) if side == "CLIENT" else ()):
            for mode in (0, 1):
                case = {"id": f"c{len(rows):06d}", "operation": operation,
                        "mode": mode, "scenario": scenario["label"],
                        "input_bits": scenario["a"] + scenario["b"] + scenario["delta"]}
                case["line"] = "\t".join(
                    [case["id"], operation, str(mode)] + case["input_bits"])
                rows.append(case)
    return rows


def valid_output(line: str | None, case: dict) -> bool:
    if line is None:
        return False
    fields = line.split("\t")
    return len(fields) == 16 and fields[:2] == [case["id"], case["operation"]] and \
        all(len(field) == 16 and all(digit in "0123456789abcdef" for digit in field)
            for field in fields[2:15]) and fields[15] in ("0", "1")


def execute(output: Path, label: str, command: list[str], cwd: Path,
            stdin_path: Path | None = None) -> dict:
    start = time.time_ns()
    record = {"label": label, "command": command, "cwd": str(cwd.resolve()),
              "stdin": pin(stdin_path) if stdin_path else None, "start_ns": start}
    write_json(output / f"{label}.command.json", record)
    source = stdin_path.open("rb") if stdin_path else subprocess.DEVNULL
    try:
        with (output / f"{label}.stdout").open("wb") as stdout, \
             (output / f"{label}.stderr").open("wb") as stderr:
            completed = subprocess.run(command, cwd=cwd, stdin=source,
                                       stdout=stdout, stderr=stderr, check=False)
        record["returncode"] = completed.returncode
    except OSError as error:
        record["launch_error"] = str(error)
        record["returncode"] = None
    finally:
        if stdin_path:
            source.close()
    record["end_ns"] = time.time_ns()
    record["stdout"] = pin(output / f"{label}.stdout")
    record["stderr"] = pin(output / f"{label}.stderr")
    write_json(output / f"{label}.result.json", record)
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[4],
                        help="repository root containing the pinned original jars")
    parser.add_argument("--native", type=Path, required=True,
                        help="built mc_entity_bounds_values_probe executable")
    parser.add_argument("--output", type=Path, required=True,
                        help="new directory for retained commands, streams and complete cases")
    parser.add_argument("--java", default=shutil.which("java"),
                        help="Java launcher path or command (default: java on PATH)")
    parser.add_argument("--javac", default=shutil.which("javac"),
                        help="Java compiler path or command (default: javac on PATH)")
    args = parser.parse_args()
    repo = args.repo.resolve()
    native = args.native.resolve()
    output = args.output.resolve()
    helper = Path(__file__).with_name("BoundsObservation.java").resolve()
    if not repo.is_dir() or not native.is_file() or not helper.is_file():
        parser.error("repository, native executable and adjacent Java observer must exist")
    if output.exists():
        parser.error("output directory must be new; existing evidence is never overwritten")
    for name in ("java", "javac"):
        selected = shutil.which(getattr(args, name)) if getattr(args, name) else None
        if selected is None:
            parser.error(f"{name} executable is required")
        setattr(args, name, str(Path(selected).resolve()))
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "FAIL", "corpus": "edge values plus 8x8 NaN pair matrix, all axes/endpoints",
              "scope": "paired original AABB movement helper values/identity",
              "limitations": ["helper observations only; no Entity/World movement",
                              "native allocation failures are not original JVM failure equivalence",
                              "original bodies run with this pinned host Java interpreter",
                              "no ray/Vec3/HitResult/String observation"],
              "producer": pin(Path(__file__)), "helper_before": pin(helper),
              "native_before": pin(native), "host": {"os": os.name, "python": sys.version},
              "sides": {}, "processes": [], "errors": []}
    try:
        if not args.java or not args.javac:
            raise RuntimeError("java and javac are required")
        report["java"] = pin(Path(args.java))
        report["javac"] = pin(Path(args.javac))
        report["processes"].append(execute(output, "java-version", [args.java, "-version"], repo))
        report["processes"].append(execute(output, "javac-version", [args.javac, "-version"], repo))
        source_paths = [repo / "tests/entity/bounds/values_probe.c",
                        repo / "include/mc/entity/bounds/bounds.h",
                        repo / "docs/reference/bounds.md"]
        report["observation_sources_before"] = [pin(path) for path in source_paths]
        for side, (filename, size, digest) in JARS.items():
            actual = pin(repo / filename)
            if actual["bytes"] != size or actual["sha256"] != digest:
                raise RuntimeError(f"original {side} jar pin mismatch")
            report.setdefault("original_jars_before", {})[side] = actual
        classes = output / "classes"
        classes.mkdir()
        process = execute(output, "javac", [args.javac, "-encoding", "UTF-8", "-d",
                          str(classes), str(helper)], repo)
        report["processes"].append(process)
        if process["returncode"] != 0:
            raise RuntimeError("reflection helper compilation failed")
        report["helper_class_before"] = pin(classes / "BoundsObservation.class")
        for side in JARS:
            rows = cases(side)
            input_path = output / f"{side.lower()}.input.tsv"
            input_path.write_bytes(("\n".join(row["line"] for row in rows) + "\n").encode("ascii"))
            original = execute(output, f"{side.lower()}-original",
                [args.java, "-Xint", "-Dfile.encoding=US-ASCII", "-cp", str(classes),
                 "BoundsObservation", str(repo / JARS[side][0]), side], repo, input_path)
            converted = execute(output, f"{side.lower()}-native", [str(native), side],
                                repo, input_path)
            report["processes"].extend((original, converted))
            reference_lines = (output / f"{side.lower()}-original.stdout").read_bytes().splitlines()
            native_lines = (output / f"{side.lower()}-native.stdout").read_bytes().splitlines()
            mismatches = []
            with (output / f"{side.lower()}.cases.jsonl").open("w", encoding="utf-8", newline="\n") as stream:
                for index, row in enumerate(rows):
                    expected = reference_lines[index].decode("ascii") if index < len(reference_lines) else None
                    actual = native_lines[index].decode("ascii") if index < len(native_lines) else None
                    row["original"] = expected
                    row["native"] = actual
                    row["original_format_valid"] = valid_output(expected, row)
                    row["native_format_valid"] = valid_output(actual, row)
                    row["match"] = row["original_format_valid"] and row["native_format_valid"] \
                        and expected == actual
                    if not row["match"]:
                        mismatches.append(row["id"])
                    stream.write(json.dumps(row, sort_keys=True) + "\n")
            side_result = {"input": pin(input_path), "cases": pin(output / f"{side.lower()}.cases.jsonl"),
                           "expected_rows": len(rows), "original_rows": len(reference_lines),
                           "native_rows": len(native_lines), "mismatch_ids": mismatches,
                           "operations": list(OPERATIONS) + (["clear"] if side == "CLIENT" else []),
                           "status": "PASS" if not mismatches and len(reference_lines) == len(rows)
                               and len(native_lines) == len(rows) and original["returncode"] == 0
                               and converted["returncode"] == 0 else "FAIL"}
            report["sides"][side] = side_result
        report["native_after"] = pin(native)
        report["helper_after"] = pin(helper)
        report["helper_class_after"] = pin(classes / "BoundsObservation.class")
        report["java_after"] = pin(Path(args.java))
        report["javac_after"] = pin(Path(args.javac))
        report["original_jars_after"] = {side: pin(repo / spec[0]) for side, spec in JARS.items()}
        report["observation_sources_after"] = [pin(path) for path in source_paths]
        stable = report["native_before"] == report["native_after"] and \
            report["helper_before"] == report["helper_after"] and \
            report["helper_class_before"] == report["helper_class_after"] and \
            report["original_jars_before"] == report["original_jars_after"] and \
            report["observation_sources_before"] == report["observation_sources_after"] and \
            report["java"] == report["java_after"] and report["javac"] == report["javac_after"]
        report["inputs_stable"] = stable
        if stable and all(value["status"] == "PASS" for value in report["sides"].values()):
            report["status"] = "PASS"
    except Exception as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write_json(output / "report.json", report)
    print(json.dumps({"status": report["status"], "report": str(output / "report.json"),
                      "rows": {side: value["expected_rows"] for side, value in report["sides"].items()},
                      "mismatches": {side: len(value["mismatch_ids"]) for side, value in report["sides"].items()},
                      "errors": report["errors"]}, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
