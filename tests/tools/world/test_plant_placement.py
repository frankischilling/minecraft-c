"""Compare genuine native plant placement calls with the complete original packet."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "tests/reference/world/plant_placement"
PREFIXES = ("CLASS\t", "PLANT\t", "COMPLETE\t")
# The second genuinely initialized Registry belongs only to native ownership controls.
BOOTSTRAP = ["16 achievements", "151 recipes"] * 2
SEPARATOR = b"\r\n" if os.name == "nt" else b"\n"
BOOTSTRAP_BYTES = SEPARATOR.join(row.encode("ascii") for row in BOOTSTRAP) + SEPARATOR


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def tree_manifest(path):
    return {p.relative_to(path).as_posix(): digest(p)
            for p in sorted(path.rglob("*")) if p.is_file()}


def guarded_inputs(executable, profiles, resources):
    selected = [Path(__file__).resolve(),
                ROOT / "tests/world/runtime/plant_placement_reference.c",
                ROOT / "tests/entity/player/test_construction.c",
                ROOT / "tests/reference/client/player_statistics/support.py",
                ROOT / "src/world/runtime/placement.c",
                ROOT / "src/world/runtime/plant_substrate.c",
                ROOT / "src/world/runtime/plants.c",
                ROOT / "src/world/runtime/plant_substrate_internal.h",
                ROOT / "include/mc/world/runtime/placement.h"]
    return {"executable": digest(executable), "profiles": digest(profiles),
            "selected_sources": {p.relative_to(ROOT).as_posix(): digest(p) for p in selected},
            "reference_packet": tree_manifest(PACKET), "resources": tree_manifest(resources)}


def compare(actual, expected):
    if actual != expected:
        for index, (left, right) in enumerate(zip(actual, expected)):
            if left != right:
                raise ValueError(f"Plant placement row {index}: {left!r} != {right!r}")
        raise ValueError(f"Plant placement row count {len(actual)} != {len(expected)}")


def select_rows(output):
    if not output.startswith(BOOTSTRAP_BYTES):
        raise ValueError("Native plant Registry bootstrap output differs")
    data = output[len(BOOTSTRAP_BYTES):]
    rows = data.decode("ascii").splitlines()
    if data != ("\n".join(rows) + "\n").encode("ascii"):
        raise ValueError("Native plant data rows must retain exact LF separators")
    if any(not row.startswith(PREFIXES) for row in rows):
        raise ValueError("Unexpected native plant placement output")
    return rows


def controls(expected):
    data = ("\n".join(expected) + "\n").encode("ascii")
    compare(select_rows(BOOTSTRAP_BYTES + data), expected)
    changed_rows = [expected[1:], expected + [expected[-1]],
                    [expected[1], expected[0], *expected[2:]]]
    first = next(i for i, row in enumerate(expected) if row.startswith("PLANT\t"))
    for width in (1, 3):
        changed = expected.copy()
        changed[first:first + 2 * width] = (changed[first + width:first + 2 * width] +
                                          changed[first:first + width])
        changed_rows.append(changed)
    for prefix, columns in (("CLASS\t", range(1, 6)), ("PLANT\t", range(1, 14)),
                            ("COMPLETE\t", range(1, 4))):
        index = next(i for i, row in enumerate(expected) if row.startswith(prefix))
        for column in columns:
            changed = expected.copy()
            fields = changed[index].split("\t")
            try:
                fields[column] = str(int(fields[column]) + 1)
            except ValueError:
                fields[column] = "changed"
            changed[index] = "\t".join(fields)
            changed_rows.append(changed)
    # Exercise the actual exception rows as well as normal Boolean returns.
    exceptional = next(i for i, row in enumerate(expected)
                       if row.startswith("PLANT\t") and "\tUNRETURNED\t" in row)
    for column, value in ((11, "false"), (12, "NONE")):
        changed = expected.copy()
        fields = changed[exceptional].split("\t")
        fields[column] = value
        changed[exceptional] = "\t".join(fields)
        changed_rows.append(changed)
    for changed in changed_rows:
        try:
            compare(changed, expected)
        except ValueError:
            continue
        raise ValueError("Plant comparator accepted a changed transcript")
    other = b"\n" if SEPARATOR == b"\r\n" else b"\r\n"
    wrong_bootstrap = other.join(row.encode("ascii") for row in BOOTSTRAP) + other
    malformed = [data, wrong_bootstrap + data,
                 BOOTSTRAP_BYTES + data.replace(b"\n", b"\r\n", 1),
                 BOOTSTRAP_BYTES + data.replace(b"\n", b"\r\n"),
                 BOOTSTRAP_BYTES + data[:-1], BOOTSTRAP_BYTES + data + b"unknown\n"]
    for output in malformed:
        try:
            select_rows(output)
        except ValueError:
            continue
        raise ValueError("Plant selector accepted malformed output")
    return len(changed_rows) + len(malformed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="plant-placement-", dir=args.work_dir)).resolve()
    resources, world = output / "resources", output / "world"
    world.mkdir()
    support = load_module("plant_placement_resources",
                          ROOT / "tests/reference/client/player_statistics/support.py")
    support.resources_at(resources)
    profiles = PACKET / "profiles.tsv"
    before = guarded_inputs(executable, profiles, resources)
    report = {"status": "FAIL", "inputs_before": before, "output": str(output)}
    try:
        packet = load_module("plant_placement_packet", PACKET / "packet.py")
        report["reference"] = packet.audit()
        expected = (PACKET / "original.stdout").read_text(encoding="ascii").splitlines()
        report["comparator_controls"] = controls(expected)
        command = [str(executable), str(resources), str(world), str(profiles)]
        report["command"] = command
        result = subprocess.run(command, capture_output=True, timeout=120)
        (output / "stdout.txt").write_bytes(result.stdout)
        (output / "stderr.txt").write_bytes(result.stderr)
        report.update(returncode=result.returncode,
                      stdout_sha256=hashlib.sha256(result.stdout).hexdigest(),
                      stderr_sha256=hashlib.sha256(result.stderr).hexdigest())
        if result.returncode != 0 or result.stderr:
            raise ValueError("Native plant placement process failed: " +
                             result.stderr.decode("utf-8", errors="backslashreplace"))
        compare(select_rows(result.stdout), expected)
        if packet.audit() != report["reference"]:
            raise ValueError("Original plant placement packet changed during replay")
        report.update(status="PASS", receivers=8, profiles=288, calls=864,
                      compared_lines=len(expected))
    except BaseException as error:
        report["error"] = repr(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (output / "stdout.txt").write_bytes(error.stdout or b"")
            (output / "stderr.txt").write_bytes(error.stderr or b"")
        raise
    finally:
        report["inputs_after"] = guarded_inputs(executable, profiles, resources)
        report["inputs_unchanged"] = report["inputs_after"] == before
        continuity_failed = report["status"] == "PASS" and not report["inputs_unchanged"]
        if continuity_failed:
            report.update(status="FAIL", error="Selected plant replay inputs changed")
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if continuity_failed:
            raise ValueError(report["error"])
    print(f"PASS: 864 original plant placement calls, {report['comparator_controls']} "
          f"corruption controls; {output}")


if __name__ == "__main__":
    main()
