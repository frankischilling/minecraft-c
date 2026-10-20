#!/usr/bin/env python3
"""Capture genuine native GAME save integration on fresh owned worlds.

Successful saves change selected region bytes. This harness validates current
saved NBT and complete captures; it does not apply the Tile fixture's unchanged
region validator or claim original/native parity.
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
FIXTURE_DRIVER = ROOT / "tests/tools/world/test_tiles.py"
SPEC = importlib.util.spec_from_file_location("saving_fixture_capture", FIXTURE_DRIVER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("real Tile capture support is missing")
TILES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TILES)
CAPTURE = TILES.CAPTURE


def decode_nbt(raw):
    """Read the complete bounded fixture NBT; names/strings are fixture ASCII.

    This is a harness reader, never a producer of native state or expected
    voxel arrays. Native tests separately compare every saved byte to the real
    current source arrays and traverse actual Map/Stack owners.
    """
    CAPTURE.require(len(raw) <= 1024 * 1024, "bounded owned fixture NBT record")
    position = 0
    tags = 0

    def take(length):
        nonlocal position
        CAPTURE.require(0 <= length <= len(raw) - position, "complete NBT payload")
        result = raw[position:position + length]
        position += length
        return result

    def number(code):
        return struct.unpack(code, take(struct.calcsize(code)))[0]

    def text():
        return take(number(">H")).decode("ascii")

    def payload(kind, depth):
        nonlocal tags
        tags += 1
        CAPTURE.require(tags <= 100000 and 1 <= depth <= 128, "bounded complete NBT traversal")
        if kind in (1, 2, 3, 4):
            return number({1: ">b", 2: ">h", 3: ">i", 4: ">q"}[kind])
        if kind in (5, 6):
            return take(4 if kind == 5 else 8)
        if kind == 7:
            return take(number(">i"))
        if kind == 8:
            return text()
        if kind == 9:
            child, length = number(">B"), number(">i")
            CAPTURE.require(0 <= length <= 100000 and (child != 0 or length == 0),
                            "valid bounded NBT list")
            return [payload(child, depth + 1) for _ in range(length)]
        CAPTURE.require(kind == 10, "known fixture NBT tag type")
        result = {}
        while True:
            child = number(">B")
            if child == 0:
                return result
            name = text()
            CAPTURE.require(name not in result, "unique retained fixture compound field")
            result[name] = payload(child, depth + 1)

    CAPTURE.require(number(">B") == 10, "actual saved Compound root")
    root_name = text()
    value = payload(10, 1)
    CAPTURE.require(root_name == "" and position == len(raw), "complete one saved NBT record")
    return value


def validate_saved(root):
    expected = {f"world{i}/region/r.0.0.mcr" for i in range(4)}
    CAPTURE.require({path.relative_to(root).as_posix() for path in root.rglob("*.mcr")} == expected,
                    "complete exact four-world save region inventory")
    rows = []
    for slot in range(4):
        region = root / f"world{slot}/region/r.0.0.mcr"
        baseline = root / f"seed-world{slot}.region.bin"
        original, saved = baseline.read_bytes(), region.read_bytes()
        CAPTURE.require(saved != original and len(saved) >= len(original) and len(saved) % 4096 == 0,
                        "successful saves change complete selected region bytes")
        CAPTURE.require(len(saved) >= 8192, "complete native region header")
        offsets = struct.unpack_from(">1024I", saved)
        stamps = struct.unpack_from(">1024I", saved, 4096)
        CAPTURE.require(offsets[0] != 0 and all(value == 0 for value in offsets[1:]) and
                        stamps[0] == 0x53000000 + slot and all(value == 0 for value in stamps[1:]),
                        "exact selected slot and final direct-save timestamp")
        offset, sectors = offsets[0] >> 8, offsets[0] & 255
        CAPTURE.require(offset >= 2 and sectors > 0 and (offset + sectors) * 4096 <= len(saved),
                        "complete committed native extent")
        length = struct.unpack_from(">I", saved, offset * 4096)[0]
        CAPTURE.require(1 < length <= sectors * 4096 - 4 and saved[offset * 4096 + 4] == 2,
                        "real native zlib region record")
        compressed = saved[offset * 4096 + 5:offset * 4096 + 4 + length]
        inflater = zlib.decompressobj()
        raw = inflater.decompress(compressed, 1024 * 1024 + 1)
        CAPTURE.require(len(raw) <= 1024 * 1024 and inflater.eof and not inflater.unused_data and
                        not inflater.unconsumed_tail, "complete bounded one zlib payload")
        document = decode_nbt(raw)
        CAPTURE.require(list(document) == ["RootExtra", "Level"] and document["RootExtra"] == 901 + slot,
                        "retained unknown root and original relative field positions")
        level = document["Level"]
        CAPTURE.require(list(level)[0] == "LevelExtra" and level["LevelExtra"] == 902 + slot and
                        level["xPos"] == level["zPos"] == 0 and level["LastUpdate"] == 20000 + slot and
                        level["TerrainPopulated"] == 1 and level["Entities"] == [] and
                        level["TileEntities"] == [], "actual Info time and deleted known records stay deleted")
        for name, size in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                           ("BlockLight", 16384), ("HeightMap", 256)):
            CAPTURE.require(type(level[name]) is bytes and len(level[name]) == size,
                            "complete current saved native array: " + name)
        CAPTURE.require((root / f"world{slot}/session.lock").read_bytes() == struct.pack(">q", 137) and
                        (root / f"world{slot}/level.dat").is_file(), "real restored session and metadata owners")
        rows.append({"file": region.relative_to(root).as_posix(),
                     "baseline": baseline.relative_to(root).as_posix(),
                     "before": CAPTURE.file_fingerprint(baseline), "after": CAPTURE.file_fingerprint(region),
                     "timestamp": stamps[0], "nbt": CAPTURE.fingerprint(raw),
                     "last_update": level["LastUpdate"], "tile_records": 0})
    CAPTURE.require({path.name for path in root.glob("seed-*.region.bin")} ==
                    {f"seed-world{i}.region.bin" for i in range(4)}, "all four retained before-save baselines")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    root = None
    report = {"schema": 1, "scope": "constructed native GAME save integration contracts",
              "scenario": "world-saving", "tested_commit": None, "passed": False,
              "original_comparison": False, "natural_population_equivalence": False,
              "entity_serializer_implemented": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "actual native save executable is missing")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "fixture_capture": FIXTURE_DRIVER,
                      "test": ROOT / "tests/world/runtime/saving/test_saving.c",
                      "tile_fixture": ROOT / "tests/world/runtime/tiles/test_tiles.c"})
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "ordinary actual selected inputs")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        receipts = work / "inputs"
        receipts.mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (receipts / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh owned Unicode world root")
        report["clock_window"] = [int(time.time())]
        stdout, stderr = CAPTURE.run([str(args.executable.resolve()), "world-saving",
                                     str(root).encode("utf-8").hex()], work=work, name="native",
                                     records=report["processes"], deadline=time.monotonic() + 100)
        report["clock_window"].append(int(time.time()))
        CAPTURE.require(not stderr, "native save contract stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(len(lines) == 5 and lines[:-1] == ["16 achievements\n", "151 recipes\n"] * 2,
                        "complete exact two-owner Registry bootstrap output")
        result = re.fullmatch(r"PASS: ([1-9][0-9]*) live Tile save integration checks \(world-saving\)\n", lines[-1])
        CAPTURE.require(result is not None, "one complete genuine native save result")
        report["checks"] = int(result.group(1))
        report["bootstrap_console"] = lines[:-1]
        report["saved_regions"] = validate_saved(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name], "input changed: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                                  for path in sorted(root.rglob("*")) if path.is_file()}
        report["passed"] = True
        print("".join(lines), end="")
        return 0
    except (CAPTURE.Failure, OSError, ValueError, UnicodeError, zlib.error, struct.error, KeyError, TypeError) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            if root is not None and root.is_dir():
                report["world_files"] = {path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                                          for path in sorted(root.rglob("*")) if path.is_file()}
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print("Capture: " + str(work))


if __name__ == "__main__":
    sys.exit(main())
