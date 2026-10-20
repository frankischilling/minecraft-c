#!/usr/bin/env python3
"""Retain actual controller/cache contracts on prepared all-air GAME Worlds.

These are native ownership and source-order contracts, not original whole-call
or natural-terrain parity. Every saved Chunk slot is independently inspected.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[3]
SAVING_DRIVER = ROOT / "tests/tools/world/test_saving.py"
SPEC = importlib.util.spec_from_file_location("normal_population_capture", SAVING_DRIVER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("actual saved-World capture support is missing")
SAVING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SAVING)
CAPTURE, TILES = SAVING.CAPTURE, SAVING.TILES


def inspect_regions(root):
    expected = {f"world{side}/region/r.0.0.mcr" for side in range(2)}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*.mcr")}
    CAPTURE.require(actual == expected, "exact two saved four-slot regions")
    records = []
    for index in range(4):
        world = root / f"world{index}"
        CAPTURE.require((world / "level.dat").is_file() and
                        (world / "session.lock").read_bytes() == struct.pack(">q", 197),
                        "actual metadata/session owners for all four Worlds")
    for side in range(2):
        path = root / f"world{side}/region/r.0.0.mcr"
        raw = path.read_bytes()
        CAPTURE.require(8192 <= len(raw) <= 4 * 1024 * 1024 and len(raw) % 4096 == 0,
                        "bounded sector-aligned region")
        offsets = struct.unpack_from(">1024I", raw)
        slots = {0, 1, 32, 33}
        CAPTURE.require({i for i, value in enumerate(offsets) if value} == slots,
                        "exact four actual Chunk slots")
        allocated = set()
        for slot in sorted(slots):
            offset, sectors = offsets[slot] >> 8, offsets[slot] & 255
            extent = set(range(offset, offset + sectors))
            CAPTURE.require(offset >= 2 and sectors > 0 and
                            (offset + sectors) * 4096 <= len(raw) and not allocated & extent,
                            "complete nonoverlapping Chunk allocation")
            allocated.update(extent)
            start = offset * 4096
            length = struct.unpack_from(">I", raw, start)[0]
            CAPTURE.require(1 < length <= sectors * 4096 - 4 and raw[start + 4] == 2,
                            "complete zlib Chunk payload")
            decoder = zlib.decompressobj()
            payload = decoder.decompress(raw[start + 5:start + 4 + length], 1024 * 1024 + 1)
            CAPTURE.require(len(payload) <= 1024 * 1024 and decoder.eof and
                            not decoder.unused_data and not decoder.unconsumed_tail,
                            "one complete bounded decompressed record")
            level = SAVING.decode_nbt(payload)["Level"]
            CAPTURE.require((level["xPos"], level["zPos"]) == (slot % 32, slot // 32) and
                            level["TerrainPopulated"] == int(slot == 0) and
                            level["LastUpdate"] == 0 and level["Entities"] == [] and
                            level["TileEntities"] == [], "actual saved flags and empty children")
            for name, size, value in (("Blocks", 32768, 0), ("Data", 16384, 0),
                                      ("SkyLight", 16384, 255), ("BlockLight", 16384, 0),
                                      ("HeightMap", 256, 0)):
                CAPTURE.require(bytes(level[name]) == bytes([value]) * size,
                                "complete unchanged all-air " + name)
            records.append({"side": side, "slot": slot, "populated": slot == 0})
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "prepared native whole-controller/cache contracts",
              "tested_commit": None, "passed": False, "original_comparison": False,
              "natural_population_equivalence": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "actual native controller executable")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "saving_capture": SAVING_DRIVER,
                      "test": ROOT / "tests/world/runtime/population/test_normal.c",
                      "random_test_support": ROOT / "tests/support/java_random.h"})
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "ordinary selected inputs")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        inputs = work / "inputs"
        inputs.mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (inputs / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh owned Unicode root")
        stdout, stderr = CAPTURE.run(
            [str(args.executable.resolve()), "normal-population", str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"], deadline=time.monotonic() + 100)
        CAPTURE.require(not stderr, "native controller stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(bool(lines) and lines[:-1] == ["16 achievements\n", "151 recipes\n"] * 2,
                        "complete source bootstrap console for both warm class owners")
        matched = re.fullmatch(r"PASS: ([1-9][0-9]*) native controller/cache-gate checks "
                               r"\(normal-population\)\n", lines[-1])
        CAPTURE.require(matched is not None, "one complete genuine native result")
        report["checks"] = int(matched.group(1))
        report["saved_records"] = inspect_regions(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "input changed during run: " + name)
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
