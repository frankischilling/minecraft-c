#!/usr/bin/env python3
"""Compare actual native WorldInfo graphs with immutable original-derived rows."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[3]
RECORDER = ROOT / "scripts/reference/world/capture_world_info.py"
RECORDER_SHA256 = "9ddd373e45c8ad968cf7ac0164563b29d44552b1d1036338f5793565883d9812"
PROCESS = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
FIXTURES = ROOT / "tests/reference/world"
EXPECTED = {
    "client": "e192d46047daff38652634b29246a3cf46c388b3c1527fc8678745dc30931a36",
    "server": "6eff4edea0833b4e7a815cffeaea6477f4cf36638af52ed895957585e2df237d",
    "provenance": "77f6162aab786245839806035da256967cd723b39bd1afbf157385e2454f1f69",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load(name, path, digest):
    require(hashlib.sha256(path.read_bytes()).hexdigest() == digest, "reviewed helper changed: " + name)
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def compare(actual, expected, side):
    if actual == expected:
        return
    left, right = actual.splitlines(), expected.splitlines()
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else b"<missing>"
        b = right[index] if index < len(right) else b"<missing>"
        if a != b:
            raise ValueError(f"{side} stable row {index + 1}: native={a!r}, original={b!r}")
    raise ValueError(side + " stable stream ending differs")


def corruption_guards(raw, expected, side, recorder):
    """Construct verifier controls from actual output; never alter a game clock or fixture."""
    rows = [line.split("\t") for line in raw.decode("ascii").splitlines()]

    def index(prefix):
        found = [i for i, row in enumerate(rows) if row[:len(prefix)] == prefix]
        require(len(found) == 1, "unique corruption-guard premise")
        return found[0]

    controls = {}
    for name, prefix, column, replacement in (
        ("copied_Player_alias", ["FIELD", "3", "h", "C"], 4, "9999"),
        ("cached_dimension", ["FIELD", "4", "i", "I"], 4, "00000009"),
        ("loaded_LastPlayed", ["FIELD", "9", "f", "J"], 4, "0000000000000000"),
        ("shared_Player_rename", ["PLAYER", "9"], 5, "004200650066006f00720065"),
        ("null_name_returned_Data", ["EXCEPTION", "5"], 7, "0"),
    ):
        changed = [row[:] for row in rows]
        target = index(prefix)
        require(changed[target][column] != replacement, "nontrivial corruption control")
        changed[target][column] = replacement
        controls[name] = changed
    changed = [row[:] for row in rows]
    first, second = index(["FIELD", "2", "b", "I"]), index(["FIELD", "2", "c", "I"])
    changed[first], changed[second] = changed[second], changed[first]
    controls["field_order"] = changed
    changed = [row[:] for row in rows]
    del changed[index(["FIELD", "3", "a", "J"])]
    controls["dropped_field"] = changed
    outcomes = {}
    for name, changed in controls.items():
        candidate = ("\n".join("\t".join(row) for row in changed) + "\n").encode("ascii")
        try:
            _, stable = recorder.parse(candidate, side)
            compare(stable, expected, side)
        except ValueError as error:
            outcomes[name] = {"status": "rejected", "reason": str(error)}
        else:
            raise ValueError("corrupted native graph accepted: " + name)
    changed = [row[:] for row in rows]
    clock_rows = 0
    for row in changed:
        if row[0] == "CLOCK_OUTPUT":
            row[2] = f"{int(row[2], 16) ^ 1:016x}"
            clock_rows += 1
    require(clock_rows == (5 if side == "client" else 3), "complete clock-only control")
    candidate = ("\n".join("\t".join(row) for row in changed) + "\n").encode("ascii")
    _, stable = recorder.parse(candidate, side)
    compare(stable, expected, side)
    return {"corruptions": outcomes, "clock_only_text_control": "accepted as excluded rows",
            "rejected": len(outcomes), "changed_clock_rows": clock_rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    recorder = load("world_info_replay_parser", RECORDER, RECORDER_SHA256)
    process = load("world_info_replay_process", PROCESS, PROCESS_SHA256)
    inputs = {"probe": options.probe.resolve(strict=True), "checker": Path(__file__).resolve(),
              "recorder": RECORDER, "process": PROCESS,
              "client": FIXTURES / "world_info_client.tsv",
              "server": FIXTURES / "world_info_server.tsv",
              "provenance": FIXTURES / "world_info_provenance.json"}
    before = {key: process.fingerprint(path) for key, path in inputs.items()}
    require(all(before[key]["sha256"] == digest for key, digest in EXPECTED.items()),
            "immutable original-derived fixture/provenance changed")
    provenance = json.loads(inputs["provenance"].read_text(encoding="ascii"))
    require(provenance["schema"] == "WORLD_INFO_ORIGINAL_GRAPH_V1" and
            provenance["inputs"]["helper"]["sha256"] == recorder.HELPER_SHA256,
            "original observer provenance")
    require(all(provenance["inputs"][side]["sha256"] == digest
                for side, digest in recorder.JARS.items()), "original archive binding")
    directory = options.work_dir.resolve() / ("capture-" + uuid.uuid4().hex)
    directory.mkdir(parents=True, exist_ok=False)
    receipts = []
    report = {"status": "failed", "scope": "Finite original-derived WorldInfo graph replay",
              "inputs_before": before, "children": receipts,
              "environment": {key: os.environ.get(key) for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
              "transformation": provenance["transformation"],
              "limitations": provenance["limitations"]}
    try:
        comparisons, guards = {}, {}
        for side in ("client", "server"):
            raw = process.run([inputs["probe"], side], directory, side, receipts)
            counts, stable = recorder.parse(raw, side)
            target = directory / (side + ".stable.tsv")
            target.write_bytes(stable)
            compare(stable, inputs[side].read_bytes(), side)
            require(counts == provenance["observations"][side]["counts"], "complete native inventory")
            guards[side] = corruption_guards(raw, inputs[side].read_bytes(), side, recorder)
            comparisons[side] = {"counts": counts, "stable": process.fingerprint(target)}
        require(len(receipts) == 2 and all(child["exit_code"] == 0 and child["reaped"] is True
                and child["stderr"]["bytes"] == 0 for child in receipts), "successful native children")
        report.update(status="passed", comparisons=comparisons, guards=guards)
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            report["inputs_after"] = {key: process.fingerprint(path) for key, path in inputs.items()}
            require(report["inputs_after"] == before, "native comparison inputs changed")
        except BaseException as error:
            report["status"] = "failed"
            report["input_error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            with (directory / "comparison.json").open("x", encoding="ascii", newline="\n") as stream:
                json.dump(report, stream, indent=2, sort_keys=True)
                stream.write("\n")
    print("PASS: 750 original-derived stable WorldInfo rows match; 14 corruptions rejected; only 5/3 real clock rows excluded.")


if __name__ == "__main__":
    main()
