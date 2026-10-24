#!/usr/bin/env python3
"""Retain constructed native Dungeon contracts and inspect the saved regions.

These are prepared GAME World scenarios, not original/natural population
equivalence. Native contracts check full state and continuations; this driver
independently reads the two genuinely saved/reloaded Dungeon records.
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
ENTITY_DRIVER = ROOT / "tests/tools/world/test_game_entities.py"
SPEC = importlib.util.spec_from_file_location("dungeon_region_capture", ENTITY_DRIVER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("actual region capture support is missing")
REGION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REGION)
CAPTURE, TILES = REGION.CAPTURE, REGION.TILES


def validate_worlds(root):
    expected = {f"epoch{epoch}/world{side}/region/r.0.0.mcr"
                for epoch in range(18) for side in range(2)}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*.mcr")}
    CAPTURE.require(actual == expected, "exact thirty-six real region files")
    saved = []
    for epoch in range(18):
        for slot in range(4):
            world = root / f"epoch{epoch}/world{slot}"
            CAPTURE.require((world / "level.dat").is_file() and
                            (world / "session.lock").read_bytes()[:8] == struct.pack(">q", 137),
                            "real metadata/session owners for all seventy-two Worlds")
        for side in range(2):
            path = root / f"epoch{epoch}/world{side}/region/r.0.0.mcr"
            level = REGION.region_document(path)["Level"]
            CAPTURE.require((level["xPos"], level["zPos"]) == (0, 0), "selected actual Chunk")
            for name, length in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                                 ("BlockLight", 16384), ("HeightMap", 256)):
                CAPTURE.require(type(level[name]) is bytes and len(level[name]) == length,
                                "complete real array: " + name)
            CAPTURE.require(level["Entities"] == [], "no invented Dungeon Entities")
            tile_entities = level["TileEntities"]
            if epoch == 0:
                CAPTURE.require(len(tile_entities) == 3, "two real Chests and one Spawner saved")
                by_position = {(tile_entity["x"], tile_entity["y"], tile_entity["z"]): tile_entity
                               for tile_entity in tile_entities}
                CAPTURE.require(set(by_position) == {(10, 64, 8), (8, 64, 10), (8, 64, 8)},
                                "complete actual saved TileEntity coordinates")
                for position in ((10, 64, 8), (8, 64, 10)):
                    chest = by_position[position]
                    CAPTURE.require(chest["id"] == "Chest" and 0 < len(chest["Items"]) <= 8,
                                    "actual nonempty Chest serializer")
                    slots = set()
                    for item in chest["Items"]:
                        CAPTURE.require(0 <= item["Slot"] < 27 and item["Slot"] not in slots and
                                        1 <= item["Count"] <= 4 and item["Damage"] in (0, 3),
                                        "actual logical slots and bounded source loot fields")
                        slots.add(item["Slot"])
                spawner = by_position[(8, 64, 8)]
                CAPTURE.require(spawner["id"] == "MobSpawner" and spawner["Delay"] == 20 and
                                spawner["EntityId"] in ("Skeleton", "Zombie", "Spider"),
                                "real final Spawner name and unchanged delay")
                saved.append({"path": path.relative_to(root).as_posix(),
                              "fingerprint": CAPTURE.file_fingerprint(path),
                              "tile_count": len(tile_entities)})
            else:
                CAPTURE.require(tile_entities == [],
                                "unsaved prepared inputs remain without live TileEntities")
    return saved


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "constructed native Dungeon integration",
              "tested_commit": None, "passed": False, "original_comparison": False,
              "natural_population_equivalence": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "actual native Dungeon executable")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "region_capture": ENTITY_DRIVER,
                      "saving_capture": REGION.SAVING_DRIVER,
                      "test": ROOT / "tests/world/runtime/population/test_dungeon.c",
                      "tile_fixture": ROOT / "tests/world/runtime/tile_entities/test_tile_entities.c"})
        for relative in ("src/item/registry", "include/mc/item/registry"):
            for path in sorted((ROOT / relative).rglob("*")):
                if path.is_file() and path.suffix in (".c", ".h"):
                    paths[path.relative_to(ROOT).as_posix()] = path
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
            [str(args.executable.resolve()), "world-dungeon", str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"],
            deadline=time.monotonic() + 140)
        CAPTURE.require(not stderr, "native Dungeon stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(bool(lines) and lines[:-1] ==
                        ["16 achievements\n", "151 recipes\n"] * 36,
                        "complete source bootstrap console for thirty-six class owners")
        matched = re.fullmatch(r"PASS: ([1-9][0-9]*) Dungeon native integration checks "
                               r"\(world-dungeon\)\n", lines[-1])
        CAPTURE.require(matched is not None, "one complete genuine native result")
        report["checks"] = int(matched.group(1))
        report["saved_records"] = validate_worlds(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "input changed during run: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {
            path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
            for path in sorted(root.rglob("*")) if path.is_file()}
        report["passed"] = True
        print("".join(lines), end="")
        return 0
    except (CAPTURE.Failure, OSError, ValueError, UnicodeError) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n",
                                             encoding="utf-8", newline="\n")
            print("Capture: " + str(work))


if __name__ == "__main__":
    sys.exit(main())
