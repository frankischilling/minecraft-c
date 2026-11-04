"""Replay ordinary Torch methods and audit each process's actual drop entropy."""

import argparse
import copy
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
PACKET = ROOT / "tests/reference/world/torch_placement"
SEPARATOR = b"\r\n" if os.name == "nt" else b"\n"
BOOTSTRAP = SEPARATOR.join((b"16 achievements", b"151 recipes")) + SEPARATOR
TORCH_KEYS = {"index", "name", "values", "result", "exception", "center_before",
              "center_after", "changes", "queues_before", "queues_after", "events",
              "drops", "world_rng_unchanged"}
ENTROPY_KEYS = {"index", "name", "world_before", "world_after", "math_before",
                "math_after", "items"}


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


def guarded_inputs(executable, resources):
    selected = [Path(__file__).resolve(),
                ROOT / "tests/world/runtime/torch_placement_reference.c",
                ROOT / "tests/entity/player/test_construction.c",
                ROOT / "tests/reference/client/player_statistics/support.py",
                ROOT / "src/world/runtime/placement.c",
                ROOT / "src/world/runtime/torch_support.c",
                ROOT / "src/world/runtime/torch_support_internal.h",
                ROOT / "src/world/runtime/mutation/torch.c",
                ROOT / "src/world/runtime/mutation/hooks.c",
                ROOT / "src/world/runtime/mutation/ticks.c",
                ROOT / "src/world/runtime/mutation/drops.c",
                ROOT / "src/world/runtime/mutation/mutation_internal.h",
                ROOT / "include/mc/world/runtime/placement.h",
                ROOT / "include/mc/world/runtime/mutations.h",
                ROOT / "src/core/java/random_profile_internal.h",
                ROOT / "src/entity/entity.c"]
    return {"executable": digest(executable),
            "selected_sources": {p.relative_to(ROOT).as_posix(): digest(p) for p in selected},
            "reference_packet": tree_manifest(PACKET), "resources": tree_manifest(resources)}


def canonical(value):
    # Preserve JSON scalar types: Python's True == 1 must not accept a receipt.
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


def compare(actual, expected):
    if len(actual) != len(expected):
        raise ValueError(f"Torch record count {len(actual)} != {len(expected)}")
    for index, (left, right) in enumerate(zip(actual, expected)):
        if canonical(left) != canonical(right):
            raise ValueError(f"Torch record {index}: {canonical(left)} != {canonical(right)}")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def decode_record(text):
    def invalid_constant(value):
        raise ValueError(f"Nonfinite JSON constant: {value}")
    return json.loads(text, object_pairs_hook=unique_object, parse_constant=invalid_constant)


def identities(rows, entropy):
    if len(rows) != 193 or len(entropy) != 193:
        raise ValueError("Incomplete Torch/entropy cohort")
    for index, (row, own) in enumerate(zip(rows, entropy)):
        if (type(row) is not dict or set(row) != TORCH_KEYS or
                type(own) is not dict or set(own) != ENTROPY_KEYS or
                type(row["index"]) is not int or row["index"] != index or
                type(own["index"]) is not int or own["index"] != index or
                type(row["name"]) is not str or type(own["name"]) is not str or
                own["name"] != row["name"] or
                type(row["values"]) is not list or len(row["values"]) != 12 or
                any(type(v) is not int or not -(1 << 31) <= v < (1 << 31)
                    for v in row["values"]) or type(row["world_rng_unchanged"]) is not bool):
            raise ValueError(f"Torch/entropy typed identity {index}")
        if type(own["items"]) is not list or any(
                type(item) is not dict or type(item.get("ordinal")) is not int or
                item["ordinal"] != ordinal for ordinal, item in enumerate(own["items"])):
            raise ValueError(f"Torch Item entropy typed ordinal {index}")


def select_rows(output):
    if not output.startswith(BOOTSTRAP):
        raise ValueError("Native Torch Registry bootstrap output differs")
    data = output[len(BOOTSTRAP):]
    lines = data.decode("ascii").splitlines()
    if data != ("\n".join(lines) + "\n").encode("ascii"):
        raise ValueError("Native Torch data must retain exact LF separators")
    if len(lines) != 387 or lines[-1] != "COMPLETE\t193":
        raise ValueError("Native Torch completion or row count differs")
    rows, entropy = [], []
    for index in range(193):
        first, second = lines[2 * index:2 * index + 2]
        if not first.startswith("TORCH\t") or not second.startswith("ENTROPY\t"):
            raise ValueError(f"Torch/entropy source order differs at {index}")
        rows.append(decode_record(first[6:]))
        entropy.append(decode_record(second[8:]))
    identities(rows, entropy)
    return rows, entropy


def encoded(rows, entropy):
    lines = []
    for row, own in zip(rows, entropy):
        lines.extend(("TORCH\t" + canonical(row), "ENTROPY\t" + canonical(own)))
    return BOOTSTRAP + ("\n".join(lines) + "\nCOMPLETE\t193\n").encode("ascii")


def controls(expected, entropy, own_auditor):
    """Corrupt retained observations; never manufacture a gameplay result."""
    total = 0

    def rejected(operation):
        nonlocal total
        try:
            operation()
        except (ValueError, TypeError, KeyError, IndexError):
            total += 1
            return
        raise ValueError("Torch comparator/auditor accepted a changed receipt")

    compare(expected, expected)
    identities(expected, entropy)
    own_auditor.audit_cohort(expected, entropy)
    for changed in (expected[1:], expected + [expected[-1]],
                    [expected[1], expected[0], *expected[2:]]):
        rejected(lambda changed=changed: compare(changed, expected))
    # Literal source operands, outcomes, exception prefixes and complete effects.
    for key, replacement in (("index", True), ("name", "changed"),
                             ("result", "UNRETURNED"), ("exception", "changed"),
                             ("world_rng_unchanged", not expected[0]["world_rng_unchanged"])):
        changed = copy.deepcopy(expected)
        changed[0][key] = replacement
        rejected(lambda changed=changed: compare(changed, expected))
    for column in range(12):
        changed = copy.deepcopy(expected)
        changed[0]["values"][column] ^= 1
        rejected(lambda changed=changed: compare(changed, expected))
    for key in ("center_before", "center_after", "changes", "queues_before", "queues_after",
                "events", "drops"):
        index = next(i for i, row in enumerate(expected) if row[key])
        changed = copy.deepcopy(expected)
        changed[index][key].append(["extra"])
        rejected(lambda changed=changed: compare(changed, expected))
    exceptional = next(i for i, row in enumerate(expected) if row["exception"] != "NONE")
    changed = copy.deepcopy(expected)
    changed[exceptional]["exception"] = "NONE"
    rejected(lambda: compare(changed, expected))
    data = encoded(expected, entropy)
    select_rows(data)
    for malformed in (data[len(BOOTSTRAP):], data[:-1], data + b"unknown\n",
                      data.replace(b"COMPLETE\t193", b"COMPLETE\t192"),
                      BOOTSTRAP + data[len(BOOTSTRAP):].replace(b"\n", b"\r\n"),
                      data.replace(b'"index":0', b'"index":0,"index":0', 1),
                      data.replace(b'"index":0', b'"index":true', 1),
                      data.replace(b'"index":0', b'"index":NaN', 1)):
        rejected(lambda malformed=malformed: select_rows(malformed))
    drop_index = next(i for i, own in enumerate(entropy) if own["items"])
    for key in ("position", "motion", "yaw", "bob", "math_post", "entity_random"):
        changed = copy.deepcopy(entropy)
        item = changed[drop_index]["items"][0]
        if key in ("position", "motion"):
            item[key][0] = f"{int(item[key][0], 16) ^ 1:016x}"
        elif key in ("yaw", "bob"):
            item[key] = f"{int(item[key], 16) ^ 1:08x}"
        elif key == "math_post":
            item[key]["seed"] = f"{int(item[key]['seed'], 16) ^ 1:012x}"
        else:
            # Entity constructor seed is observed, not predicted. Its untouched
            # fresh Gaussian cache is independently constrained by the body.
            item[key]["have"] = True
        rejected(lambda changed=changed: own_auditor.audit_cohort(expected, changed))
    for key in ("world_before", "world_after", "math_after"):
        changed = copy.deepcopy(entropy)
        state = changed[drop_index][key]
        state["seed"] = f"{int(state['seed'], 16) ^ 1:012x}"
        rejected(lambda changed=changed: own_auditor.audit_cohort(expected, changed))
    for field, replacement in (("gaussian", "0000000000000001"), ("have", True)):
        changed = copy.deepcopy(entropy)
        changed[drop_index]["world_after"][field] = replacement
        rejected(lambda changed=changed: own_auditor.audit_cohort(expected, changed))
    changed = copy.deepcopy(entropy)
    changed[drop_index]["items"] = []
    rejected(lambda: own_auditor.audit_cohort(expected, changed))
    changed = copy.deepcopy(entropy)
    changed[drop_index]["items"][0]["extra"] = 0
    rejected(lambda: own_auditor.audit_cohort(expected, changed))
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="torch-placement-", dir=args.work_dir)).resolve()
    resources, world = output / "resources", output / "world"
    world.mkdir()
    support = load_module("torch_placement_resources",
                          ROOT / "tests/reference/client/player_statistics/support.py")
    support.resources_at(resources)
    before = guarded_inputs(executable, resources)
    report = {"status": "FAIL", "inputs_before": before, "output": str(output),
              "entropy_scope": "own-process continuation/formula audit; no cross-process equality"}
    try:
        packet = load_module("torch_placement_packet", PACKET / "packet.py")
        own_auditor = load_module("torch_placement_entropy", PACKET / "entropy.py")
        report["reference"] = packet.audit()
        expected = packet.reference_rows()
        if type(expected) is not list or len(expected) != 193:
            raise ValueError("Original Torch packet must expose all 193 literal records")
        command = [str(executable), str(resources), str(world), str(PACKET / "profiles.tsv")]
        report["command"] = command
        result = subprocess.run(command, capture_output=True, timeout=120)
        (output / "stdout.txt").write_bytes(result.stdout)
        (output / "stderr.txt").write_bytes(result.stderr)
        report.update(returncode=result.returncode,
                      stdout_sha256=hashlib.sha256(result.stdout).hexdigest(),
                      stderr_sha256=hashlib.sha256(result.stderr).hexdigest())
        if result.returncode != 0 or result.stderr:
            raise ValueError("Native Torch process failed: " +
                             result.stderr.decode("utf-8", errors="backslashreplace"))
        rows, entropy = select_rows(result.stdout)
        compare(rows, expected)
        report["own_entropy"] = own_auditor.audit_cohort(rows, entropy)
        report["comparator_controls"] = controls(expected, entropy, own_auditor)
        if packet.audit() != report["reference"]:
            raise ValueError("Original Torch packet changed during replay")
        report.update(status="PASS", profiles=193, compared_records=len(rows),
                      actual_entropy_records=len(entropy))
    except BaseException as error:
        report["error"] = repr(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (output / "stdout.txt").write_bytes(error.stdout or b"")
            (output / "stderr.txt").write_bytes(error.stderr or b"")
        raise
    finally:
        report["inputs_after"] = guarded_inputs(executable, resources)
        report["inputs_unchanged"] = report["inputs_after"] == before
        continuity_failed = report["status"] == "PASS" and not report["inputs_unchanged"]
        if continuity_failed:
            report.update(status="FAIL", error="Selected Torch replay inputs changed")
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if continuity_failed:
            raise ValueError(report["error"])
    print(f"PASS: 193 original Torch records; {report['own_entropy']['drops']} own-audited drops; "
          f"{report['comparator_controls']} corruption controls; {output}")


if __name__ == "__main__":
    main()
