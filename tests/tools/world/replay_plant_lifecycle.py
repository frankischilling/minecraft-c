#!/usr/bin/env python3
"""Replay original-derived plant projections without Java or game assets."""

import argparse
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "scripts/reference/world/plants/compare_lifecycle.py"
SPEC = importlib.util.spec_from_file_location("plant_lifecycle_comparison", DRIVER)
COMPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPARE)
FIXTURES = ROOT / "tests/reference/world/plants"


def guards(expected, native, names):
    count = 0
    for field in ("before", "after", "drops"):
        changed = copy.deepcopy(expected)
        if field == "drops":
            changed["cases"][0][field] = [{"stack": [1, 0, 6, 0], "pickup_delay": 10,
                                           "position_bits": ["0" * 16] * 3}]
        else:
            changed["cases"][0][field][0][4] ^= 1
        if not COMPARE.compare(changed, native, names):
            raise ValueError("missing comparison for " + field)
        count += 1
    for stream in ("supplied_random_tail", "world_random_tail"):
        for key in ("next_long_hex", "next_int31", "next_double_bits", "next_gaussian_bits"):
            changed = copy.deepcopy(expected)
            value = changed["cases"][0][stream][key]
            changed["cases"][0][stream][key] = value ^ 1 if type(value) is int else "0" * 16
            if not COMPARE.compare(changed, native, names):
                raise ValueError("missing Random comparison for " + stream + "." + key)
            count += 1
    changed = copy.deepcopy(expected)
    changed["cases"][0], changed["cases"][1] = changed["cases"][1], changed["cases"][0]
    try:
        COMPARE.compare(changed, native, names)
    except ValueError:
        count += 1
    else:
        raise ValueError("request order corruption accepted")
    return count


def malformed_guards(expected, native, names):
    dropped = next(index for index, row in enumerate(expected["cases"]) if row["drops"])
    missing = object()
    variants = [(("schema",), True),
                (("unexpected",), 0),
                (("sparse_default", 0), False),
                (("cases", 0, "outcome", "status"), True),
                (("cases", 0, "outcome", "unexpected"), 0)]
    for stream in ("supplied_random_tail", "world_random_tail"):
        prefix = ("cases", 0, stream)
        for value in (True, 0.0, -1, 31):
            variants.append((prefix + ("next_int31",), value))
        variants.extend(((prefix + ("next_long_hex",), missing),
                         (prefix + ("unexpected",), 0)))
        for field in ("next_long_hex", "next_double_bits", "next_gaussian_bits"):
            for value in (False, "0" * 15, "A" * 16, "g" * 16):
                variants.append((prefix + (field,), value))
    prefix = ("cases", dropped, "drops", 0)
    for field in range(4):
        for value in (False, 0.0, COMPARE.INT_MIN - 1, COMPARE.INT_MAX + 1):
            variants.append((prefix + ("stack", field), value))
    for value in (True, 0.0, COMPARE.INT_MIN - 1, COMPARE.INT_MAX + 1):
        variants.append((prefix + ("pickup_delay",), value))
    for field in range(3):
        for value in (True, "0" * 15, "A" * 16, "g" * 16):
            variants.append((prefix + ("position_bits", field), value))
    variants.extend(((prefix + ("stack",), [1, 0, 6]),
                     (prefix + ("position_bits",), ["0" * 16] * 2),
                     (prefix + ("pickup_delay",), missing),
                     (prefix + ("unexpected",), 0)))
    count = 0
    for path, value in variants:
        for original in (True, False):
            # Copy only containers along the changed path. The other cells and
            # cases remain read-only, keeping these schema trials inexpensive.
            source = expected if original else native
            changed = copy.copy(source)
            source_at, changed_at = source, changed
            for key in path[:-1]:
                child = copy.copy(source_at[key])
                changed_at[key] = child
                source_at, changed_at = source_at[key], child
            if value is missing:
                del changed_at[path[-1]]
            else:
                changed_at[path[-1]] = value
            try:
                COMPARE.compare(changed if original else expected,
                                native if original else changed, names)
            except ValueError:
                count += 1
            else:
                raise ValueError("malformed comparison input accepted: " + str(path))
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.work_dir = Path(tempfile.mkdtemp(prefix="capture-", dir=args.work_dir)).resolve()
    report = {"schema": 1, "passed": False, "sides": {}}
    try:
        provenance = COMPARE.read(FIXTURES / "provenance.json")
        if provenance["extent"] != [0, 31, 0, 127, 0, 31] or provenance["cases_per_side"] != 59:
            raise ValueError("declared fixture extent or inventory")
        if COMPARE.pin(FIXTURES / "requests.tsv") != provenance["requests"]:
            raise ValueError("fixture requests fingerprint")
        names = [line.split("\t")[0] for line in (FIXTURES / "requests.tsv").read_text().splitlines()
                 if line and not line.startswith("#")]
        with tempfile.TemporaryDirectory(prefix="plant-replay-", dir=args.work_dir) as temporary:
            for side in ("client", "server"):
                path = FIXTURES / (side + ".json")
                if COMPARE.pin(path) != provenance["sides"][side]["fixture"]:
                    raise ValueError(side + " fixture fingerprint")
                expected = COMPARE.read(path)
                if expected["jar_sha256"] != COMPARE.HASHES[side]:
                    raise ValueError(side + " original archive identity")
                result_path = args.work_dir.resolve() / (side + "-native.json")
                world = Path(temporary) / side
                child = subprocess.run([str(args.executable.resolve()), side,
                                        str(FIXTURES / "requests.tsv"), str(world).encode("utf-8").hex(),
                                        str(result_path)], capture_output=True, timeout=90)
                (args.work_dir / (side + ".stdout")).write_bytes(child.stdout)
                (args.work_dir / (side + ".stderr")).write_bytes(child.stderr)
                if child.returncode or child.stderr:
                    raise RuntimeError(side + " native probe failed: " + child.stderr.decode("utf-8", errors="replace"))
                native = COMPARE.read(result_path)
                mismatches = COMPARE.compare(expected, native, names)
                if mismatches:
                    report["sides"][side] = {"mismatches": mismatches}
                    raise ValueError(side + " replay mismatches")
                rejected = guards(expected, native, names)
                malformed = malformed_guards(expected, native, names)
                report["sides"][side] = {"cases": len(names), "corruption_guards": rejected,
                                          "malformed_guards": malformed,
                                          "native": COMPARE.pin(result_path)}
                print(f"{side}: {len(names)} plant cases, {rejected} corruption guards and "
                      f"{malformed} malformed guards passed")
        report["passed"] = True
        return 0
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error))
        return 1
    finally:
        (args.work_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
