#!/usr/bin/env python3
"""Retain native falling Block and Entity publication contracts.

Saved regions are the constructed input records, not final-state observations
or original-game comparisons. Entity movement and later ticks are unqualified.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "tests/tools/world/test_tile_entities.py"
SPEC = importlib.util.spec_from_file_location("falling_capture_support", HELPER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("falling capture support is missing")
TILES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TILES)
CAPTURE = TILES.CAPTURE


def inspect_inputs(root):
    expected = {f"world{i}/region/r.0.0.mcr" for i in range(4)}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*.mcr")}
    CAPTURE.require(actual == expected, "exact four retained input regions")
    records = []
    for side in range(4):
        path = root / f"world{side}/region/r.0.0.mcr"
        raw = path.read_bytes()
        CAPTURE.require(len(raw) >= 8192 and len(raw) % 4096 == 0,
                        "complete sector-aligned region")
        offsets = struct.unpack_from(">1024I", raw)
        stamps = struct.unpack_from(">1024I", raw, 4096)
        slots = {0, 1, 32, 33}
        CAPTURE.require({i for i, value in enumerate(offsets) if value} == slots,
                        "exact four serialized input Chunk slots")
        CAPTURE.require(all(value == (0xFEDCBA98 if i in slots else 0)
                            for i, value in enumerate(stamps)),
                        "all actual input timestamps retained")
        occupied = {0, 1}
        for slot in sorted(slots):
            first, sectors = offsets[slot] >> 8, offsets[slot] & 255
            span = set(range(first, first + sectors))
            CAPTURE.require(first >= 2 and sectors > 0 and not occupied & span and
                            (first + sectors) * 4096 <= len(raw),
                            "complete independent Chunk sector span")
            occupied.update(span)
            length, = struct.unpack_from(">I", raw, first * 4096)
            CAPTURE.require(1 < length <= sectors * 4096 - 4 and
                            raw[first * 4096 + 4] == 2,
                            "complete zlib input record")
            records.append({"world": side, "slot": slot})
        world = root / f"world{side}"
        CAPTURE.require((world / "level.dat").is_file() and
                        (world / "session.lock").read_bytes() == struct.pack(">q", 127),
                        "actual metadata and session owner")
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "constructed native falling contracts",
              "tested_commit": None, "passed": False, "original_comparison": False,
              "later_entity_ticks": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "native falling executable")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "fixture_driver": HELPER,
                      "test": ROOT / "tests/world/runtime/test_falling.c",
                      "fixture_source": ROOT / "tests/world/runtime/test_fluids.c",
                      "random_test_support": ROOT / "tests/support/java_random.h"})
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "ordinary selected source inputs")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        inputs = work / "inputs"
        inputs.mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (inputs / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh owned Unicode root")
        stdout, stderr = CAPTURE.run(
            [str(args.executable.resolve()), "world-falling", str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"], deadline=time.monotonic() + 80)
        CAPTURE.require(not stderr, "native falling stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(bool(lines) and lines[:-1] == ["16 achievements\n", "151 recipes\n"] * 2,
                        "complete source bootstrap console")
        matched = re.fullmatch(r"PASS: ([1-9][0-9]*) World falling checks \(world-falling\)\n",
                               lines[-1])
        CAPTURE.require(matched is not None, "complete genuine native result")
        report["checks"] = int(matched.group(1))
        report["input_records"] = inspect_inputs(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "source input changed during run: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                                 for path in sorted(root.rglob("*")) if path.is_file()}
        report["passed"] = True
        print("".join(lines), end="")
        return 0
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
