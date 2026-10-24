#!/usr/bin/env python3
"""Capture real native live TileEntity contracts on fresh owned GAME worlds.

This runs a distinct executable with actual Entity bindings. It is neither an
original-game comparison nor natural-population/all-joins qualification.
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
SUPPORT = ROOT / "scripts/reference/java/compare_int_set.py"
SPEC = importlib.util.spec_from_file_location("tile_entities_capture_support", SUPPORT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("capture support cannot be loaded")
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)


def input_paths(executable):
    paths = {
        "executable": executable.resolve(),
        "driver": Path(__file__).resolve(),
        "capture_support": SUPPORT,
        "test": ROOT / "tests/world/runtime/tile_entities/test_tile_entities.c",
        "cmake": ROOT / "CMakeLists.txt",
        "fixture_source": ROOT / "tests/world/runtime/test_entity_publication.c",
        "fixture_capture": ROOT / "tests/tools/world/test_runtime.py",
    }
    # Retain the selected implementation/header subsystems, beyond this test
    # and the Tile inputs. This is not the complete link dependency closure.
    # No executable or expected output is made
    # from these source files by this harness.
    for relative in (
        "src/world", "src/entity", "src/item/stack", "src/block/registry",
        "src/storage/nbt", "include/mc/world", "include/mc/entity",
        "include/mc/item/stack", "include/mc/block/registry", "include/mc/storage/nbt",
    ):
        for path in sorted((ROOT / relative).rglob("*")):
            if path.is_file() and path.suffix in (".c", ".h"):
                paths[path.relative_to(ROOT).as_posix()] = path
    return paths


def validate_worlds(root, mode):
    worlds = range(2) if mode == "core" else range(4)
    expected = {f"world{i}/region/r.0.0.mcr" for i in worlds}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*.mcr")}
    CAPTURE.require(actual == expected, "complete exact selected-mode region inventory")
    baselines = []
    for i in worlds:
        region = root / f"world{i}/region/r.0.0.mcr"
        baseline = root / f"seed-world{i}.region.bin"
        raw = region.read_bytes()
        CAPTURE.require(baseline.read_bytes() == raw,
                        "source operations/native retirement preserve every region byte")
        CAPTURE.require(len(raw) >= 8192 and len(raw) % 4096 == 0,
                        "complete sector-aligned region")
        offsets = struct.unpack_from(">1024I", raw, 0)
        times = struct.unpack_from(">1024I", raw, 4096)
        CAPTURE.require(offsets[0] != 0 and all(value == 0 for value in offsets[1:]),
                        "exact seeded populated Chunk slot, without fallback generation")
        CAPTURE.require(times[0] == 0xFEDCBA98 and all(value == 0 for value in times[1:]),
                        "actual explicit seed timestamp remains unchanged")
        offset, sectors = offsets[0] >> 8, offsets[0] & 255
        CAPTURE.require(offset >= 2 and sectors != 0 and (offset + sectors) * 4096 <= len(raw),
                        "complete live region extent")
        length = struct.unpack_from(">I", raw, offset * 4096)[0]
        CAPTURE.require(1 < length <= sectors * 4096 - 4 and raw[offset * 4096 + 4] == 2,
                        "real compressed zlib Chunk record")
        CAPTURE.require((root / f"world{i}/level.dat").is_file() and
                        (root / f"world{i}/session.lock").read_bytes()[:8] ==
                        struct.pack(">q", 137), "actual metadata and native session ownership")
        baselines.append({"region": region.relative_to(root).as_posix(),
                          "baseline": baseline.relative_to(root).as_posix(),
                          "fingerprint": CAPTURE.file_fingerprint(region)})
    CAPTURE.require({path.name for path in root.glob("seed-*.region.bin")} ==
                    {f"seed-world{i}.region.bin" for i in worlds},
                    "all independently retained native seed baselines for selected mode")
    for i in range(4):
        CAPTURE.require((root / f"world{i}/level.dat").is_file() and
                        (root / f"world{i}/session.lock").read_bytes()[:8] == struct.pack(">q", 137),
                        "all four real constructed World metadata/session owners")
    return baselines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--mode", choices=("core", "all"), default="all")
    args = parser.parse_args()
    scenario = "world-tile-entities-core" if args.mode == "core" else "world-tile-entities"
    work = None
    report = {"schema": 1, "scope": "constructed native live Tile integration contracts",
              "scenario": scenario, "mode": args.mode, "tested_commit": None, "passed": False,
              "original_comparison": False, "natural_population_equivalence": False,
              "full_tile_store_qualification": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "actual native Tile executable is missing")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = input_paths(args.executable)
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "all actual capture inputs are ordinary live files")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        inputs = work / "inputs"
        inputs.mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (inputs / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh Unicode world root")
        stdout, stderr = CAPTURE.run(
            [str(args.executable.resolve()), scenario, str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"],
            deadline=time.monotonic() + 70,
        )
        CAPTURE.require(not stderr, "native contract stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(len(lines) == 9 and
                        lines[:-1] == ["16 achievements\n", "151 recipes\n"] * 4,
                        "complete exact main/callback Registry bootstrap output")
        matched = re.fullmatch(r"PASS: ([1-9][0-9]*) live TileEntity integration checks \(" +
                               re.escape(scenario) + r"\)\n",
                               lines[-1])
        CAPTURE.require(matched is not None, "single complete genuine native result")
        report["checks"] = int(matched.group(1))
        report["bootstrap_console"] = lines[:-1]
        report["unchanged_regions"] = validate_worlds(root, args.mode)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "actual input changed during run: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {
            path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
            for path in sorted(root.rglob("*")) if path.is_file()
        }
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
