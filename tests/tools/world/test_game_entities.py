#!/usr/bin/env python3
"""Retain constructed native GAME Entity disk and failure contracts.

The native executable exercises actual owners, source methods and cleanup.
This driver independently reads every final region and checks the finite saved
record policy. Original execution and natural game routes remain separate.
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
SPEC = importlib.util.spec_from_file_location("game_entity_disk_capture", SAVING_DRIVER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("actual disk capture support is missing")
SAVING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SAVING)
CAPTURE, TILES = SAVING.CAPTURE, SAVING.TILES


def region_document(path):
    raw = path.read_bytes()
    CAPTURE.require(8192 <= len(raw) <= 4 * 1024 * 1024 and len(raw) % 4096 == 0,
                    "complete bounded sector-aligned region")
    offsets = struct.unpack_from(">1024I", raw)
    CAPTURE.require(offsets[0] != 0 and all(value == 0 for value in offsets[1:]),
                    "only the selected real Chunk slot is populated")
    offset, sectors = offsets[0] >> 8, offsets[0] & 255
    CAPTURE.require(offset >= 2 and sectors > 0 and (offset + sectors) * 4096 <= len(raw),
                    "complete real Chunk allocation")
    start = offset * 4096
    length = struct.unpack_from(">I", raw, start)[0]
    CAPTURE.require(1 < length <= sectors * 4096 - 4 and raw[start + 4] == 2,
                    "complete zlib Chunk payload")
    decoder = zlib.decompressobj()
    payload = decoder.decompress(raw[start + 5:start + 4 + length], 1024 * 1024 + 1)
    CAPTURE.require(len(payload) <= 1024 * 1024 and decoder.eof and
                    not decoder.unused_data and not decoder.unconsumed_tail,
                    "one complete bounded decompressed record")
    return SAVING.decode_nbt(payload)


def check_saved_records(value, raw_stack):
    level = value["Level"]
    CAPTURE.require(value["RootEntityExtra"] == 901 and level["LevelEntityExtra"] == 902,
                    "independent root and Level unknown fields survive")
    records = level["Entities"]
    expected = [10, 202, 20, 101, 30, 101] if raw_stack else [10, 202, 20, 101, 30]
    CAPTURE.require([row.get("UnknownExtra", row.get("EntityExtra")) for row in records] == expected,
                    "current occurrence stream and unknown record order")
    for marker, row in zip(expected, records):
        if marker < 100:
            CAPTURE.require(row["id"] == "NativeUnknown", "unknown remains an opaque record")
            continue
        CAPTURE.require(row["id"] == "Item", "actual known Item serializer identity")
        item = row["Item"]
        if marker == 101 and raw_stack:
            CAPTURE.require((item["id"], item["Count"], item["Damage"]) == (0, 0, 0) and
                            "ItemExtra" not in item,
                            "RAW current Stack fields replace old Item provenance")
        else:
            fields = (262, 5, 7) if marker == 101 else (263, 6, 8)
            CAPTURE.require((item["id"], item["Count"], item["Damage"]) == fields and
                            item["ItemExtra"] == marker + 1000,
                            "actual current Stack fields and attached provenance")
    CAPTURE.require(level["TileEntities"] == [], "complete later empty Tile traversal")


def validate_worlds(root):
    expected = {f"epoch{epoch}/world{slot}/region/r.0.0.mcr"
                for epoch in range(3) for slot in range(4)}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*.mcr")}
    CAPTURE.require(actual == expected, "exact twelve actual selected region files")
    rows = []
    for epoch in range(3):
        for slot in range(4):
            base = root / f"epoch{epoch}/world{slot}"
            CAPTURE.require((base / "level.dat").is_file() and
                            (base / "session.lock").read_bytes()[:8] == struct.pack(">q", 137),
                            "real World metadata and restored session ownership")
            path = base / "region/r.0.0.mcr"
            value = region_document(path)
            level = value["Level"]
            CAPTURE.require((level["xPos"], level["zPos"]) == (0, 0),
                            "actual fixed selected Chunk coordinates")
            for name, length in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                                 ("BlockLight", 16384), ("HeightMap", 256)):
                CAPTURE.require(type(level[name]) is bytes and len(level[name]) == length,
                                "complete real saved array: " + name)
            if epoch == 0:
                check_saved_records(value, True)
            elif epoch == 1:
                CAPTURE.require(level["Blocks"][0] == 200 and level["TileEntities"] == 1,
                                "failed imports preserve disk input before normalization")
            elif slot < 2:
                check_saved_records(value, False)
            rows.append({"path": path.relative_to(root).as_posix(),
                         "fingerprint": CAPTURE.file_fingerprint(path),
                         "entity_records": len(level["Entities"])})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = root = None
    report = {"schema": 1, "scope": "constructed native GAME Entity disk integration",
              "tested_commit": None, "passed": False, "original_comparison": False,
              "natural_population_equivalence": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "actual native Entity disk executable")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "saving_capture": SAVING_DRIVER,
                      "test": ROOT / "tests/world/runtime/entities/test_game_entities.c",
                      "tile_fixture": ROOT / "tests/world/runtime/tiles/test_tiles.c"})
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "ordinary actual selected inputs")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        (work / "inputs").mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (work / "inputs" / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh owned Unicode root")
        stdout, stderr = CAPTURE.run(
            [str(args.executable.resolve()), "world-game-entities", str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"],
            deadline=time.monotonic() + 100)
        CAPTURE.require(not stderr, "native Entity disk contract stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(bool(lines), "complete native stdout")
        expected_console = {"16 achievements\n", "151 recipes\n",
                            "Skipping Entity with id NativeUnknown\n"}
        CAPTURE.require(all(line in expected_console for line in lines[:-1]),
                        "only actual Registry and unknown-Entity diagnostic output")
        result = re.fullmatch(
            r"PASS: ([1-9][0-9]*) GAME Entity disk integration checks \(world-game-entities\)\n",
            lines[-1])
        CAPTURE.require(result is not None, "one complete successful native result")
        report["checks"] = int(result.group(1))
        report["source_console"] = lines[:-1]
        report["saved_regions"] = validate_worlds(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "capture input changed: " + name)
        report["inputs_unchanged"] = True
        report["passed"] = True
        print("".join(lines), end="")
        return 0
    except (CAPTURE.Failure, OSError, ValueError, UnicodeError, zlib.error, struct.error,
            KeyError, TypeError) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            if root is not None and root.is_dir():
                report["world_files"] = {
                    path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                    for path in sorted(root.rglob("*")) if path.is_file()}
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
            print("Capture: " + str(work))


if __name__ == "__main__":
    sys.exit(main())
