#!/usr/bin/env python3
"""Replay original SavedData memory observations through the real native owner."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/world"
SUPPORT = ROOT / "scripts/reference/java/compare_int_set.py"
EXPECTED = {"client.commands": "5267402bb8124ffba8a5f37871edcba46e5cfcf5df5e63065f2714d0c18943f5",
            "server.commands": "5267402bb8124ffba8a5f37871edcba46e5cfcf5df5e63065f2714d0c18943f5",
            "client.tsv": "386525ca0f6d27ff7e242b90543a498a692a25d954d21bb2822b2c53784d9cf1",
            "server.tsv": "386525ca0f6d27ff7e242b90543a498a692a25d954d21bb2822b2c53784d9cf1",
            "provenance.json": "a895b187d9b5890f802d71af29a8045bab01010881708f0b5551aa96d02a9420"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def compare(actual, expected):
    require(actual == expected, "complete native memory stream differs from original observations")


def corruption_guards(raw, expected):
    rows = [line.split("\t") for line in raw.decode("ascii").splitlines()]
    controls = {}
    def changed():
        return [row[:] for row in rows]
    copy = changed()
    wrap = next(i for i, row in enumerate(copy) if row[0] == "RETURN" and row[3] == "-32768")
    copy[wrap][3] = "32768"
    controls["short_wrap"] = copy
    copy = changed()
    null = next(i for i, row in enumerate(copy) if row[0] == "VALUE" and row[2] == "NULL")
    copy[null][2] = "EMPTY"
    controls["null_empty_key"] = copy
    copy = changed()
    unicode = next(i for i, row in enumerate(copy) if row[0] == "VALUE" and row[2] == "0000d800006edc00ffff")
    copy[unicode][2] = "d800006edc00ffff"
    controls["utf16_nul"] = copy
    copy = changed()
    repeat = next(i for i, row in enumerate(copy) if row[0] == "RETURN" and row[2] == "32765")
    copy[repeat][2] = "32764"
    controls["omitted_draw_count"] = copy
    copy = changed()
    order = next(i for i in range(len(copy) - 1) if copy[i][0] == copy[i + 1][0] == "VALUE")
    copy[order], copy[order + 1] = copy[order + 1], copy[order]
    controls["map_order"] = copy
    copy = changed()
    del copy[next(i for i, row in enumerate(copy) if row == ["MAP", "0", "0"])]
    controls["dropped_snapshot"] = copy
    controls["missing_footer"] = changed()[:-1]
    rejected = []
    for name, candidate in controls.items():
        candidate = ("\n".join("\t".join(row) for row in candidate) + "\n").encode("ascii")
        require(candidate != raw, "nontrivial corruption premise")
        try:
            compare(candidate, expected)
        except ValueError:
            rejected.append(name)
        else:
            raise ValueError("corrupted native output accepted: " + name)
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("saved_ids_native_process", SUPPORT)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    work = None
    report = {"schema": "SAVED_DATA_IDS_MEMORY_REPLAY_V1", "passed": False, "processes": [],
              "scope": "Original-derived logical memory state and returns; no disk or String argument identity comparison"}
    try:
        require(args.executable.is_file(), "native memory probe is missing")
        paths = {"executable": args.executable.resolve(), "checker": Path(__file__).resolve(), "support": SUPPORT,
                 "owner": ROOT / "src/world/saved_data/storage.c",
                 "owner_header": ROOT / "include/mc/world/saved_data/storage.h",
                 "probe": ROOT / "tests/world/saved_data/observation_probe.c",
                 "files": ROOT / "src/world/storage/files.c", "files_header": ROOT / "src/world/storage/files.h",
                 "observer": ROOT / "tests/reference/world/SavedDataIdObservation.java",
                 "recorder": ROOT / "scripts/reference/world/capture_saved_data_ids.py",
                 "exporter": ROOT / "scripts/reference/world/export_saved_data_ids.py"}
        for name in EXPECTED:
            paths[name] = FIXTURES / ("saved_data_ids_" + name)
        before = {name: capture.file_fingerprint(path) for name, path in paths.items()}
        require(all(before[name]["sha256"] == digest for name, digest in EXPECTED.items()),
                "immutable original-derived fixtures changed")
        provenance = json.loads(paths["provenance.json"].read_text(encoding="ascii"))
        require(provenance["schema"] == "SAVED_DATA_IDS_MEMORY_V1" and
                provenance["inputs"]["recorder"] == before["recorder"] and
                provenance["inputs"]["helper"] == before["observer"], "captured recorder/observer provenance")
        work = capture.capture_root(args.work_dir, [sys.executable])
        report.update(inputs=before, transformation=provenance["transformation"], limitations=provenance["limitations"])
        for name, path in paths.items():
            if name != "executable":
                (work / (name + path.suffix)).write_bytes(path.read_bytes())
        deadline = time.monotonic() + 50
        results = {}
        for side in ("client", "server"):
            request = paths[side + ".commands"].read_bytes()
            (work / (side + ".stdin")).write_bytes(request)
            raw, stderr = capture.run([str(args.executable.resolve())], request, work=work, name=side,
                                      records=report["processes"], deadline=deadline)
            require(not stderr, "native memory probe stderr")
            expected = paths[side + ".tsv"].read_bytes()
            compare(raw, expected)
            require(len(raw.splitlines()) == 39, "complete observed memory rows")
            results[side] = {"rows": 39, "actual_native_calls": 65543,
                             "guards": corruption_guards(raw, expected)}
        negatives = {"missing_end": b"NEW 0\n", "duplicate_owner": b"NEW 0\nNEW 0\nEND\n",
                     "repeat_bound": b"NEW 0\nNEXT 0 65539 NULL\nEND\n",
                     "trailing_ctrl_z": b"NEW 0\nEND\n\x1a"}
        rejected = []
        for name, request in negatives.items():
            (work / (name + ".stdin")).write_bytes(request)
            try:
                capture.run([str(args.executable.resolve())], request, work=work, name=name,
                            records=report["processes"], deadline=deadline)
            except capture.Failure:
                child = report["processes"][-1]
                require(child["exit"] == 1 and child["reaped"] and not child["timed_out"] and
                        not child["output_limited"], "natural bounded native protocol refusal")
                require(b"saved-data probe:" in (work / (name + ".stderr")).read_bytes() and
                        b"END\t" not in (work / (name + ".stdout")).read_bytes(),
                        "actual protocol diagnostic without success footer")
                rejected.append(name)
            else:
                raise ValueError("malformed native input accepted: " + name)
        report.update(passed=True, comparisons=results, protocol_refusals=rejected)
    except (capture.Failure, OSError, ValueError, StopIteration) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
    finally:
        if work is not None:
            report["inputs_after"] = {name: capture.file_fingerprint(path) for name, path in paths.items()}
            if report["inputs_after"] != before:
                report["passed"] = False
                report["failure"] = "input changed"
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print("Capture: " + str(work))
    if report["passed"]:
        rows = sum(value["rows"] for value in results.values())
        guards = sum(len(value["guards"]) for value in results.values())
        print(f"PASS: {rows} original-derived SavedData memory rows match; {guards} output corruptions and {len(rejected)} malformed inputs rejected")
        return 0
    if report.get("failure") == "input changed":
        print("FAIL: input changed", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
