#!/usr/bin/env python3
"""Run native runtime/owner contracts on fresh, retained owned worlds."""

import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("int_set_capture", ROOT / "scripts/reference/java/compare_int_set.py")
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)
SCENARIOS = ("ownership-and-runtime", "generation", "generation-copy-limit", "profile-and-entity-bindings",
             "world-info-bindings", "saved-data-ids", "typed-world-info", "game-world", "world-registry",
             "world-biome-runtime")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "constructed native runtime/owner contracts", "tested_commit": None,
              "scenario": args.scenario, "passed": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "native contract executable is missing")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = {"executable": args.executable.resolve(), "driver": Path(__file__),
                 "capture_support": Path(SPEC.origin), "test": ROOT / "tests/world/runtime/test_runtime.c",
                 "runtime": ROOT / "src/world/runtime/runtime.c", "runtime_header": ROOT / "include/mc/world/runtime/runtime.h",
                 "entity": ROOT / "src/entity/entity.c", "entity_header": ROOT / "include/mc/entity/entity.h",
                 "entity_internal": ROOT / "src/entity/entity_internal.h",
                 "store": ROOT / "src/world/storage/store.c", "store_header": ROOT / "include/mc/world/storage/store.h",
                 "binding_test": ROOT / "tests/world/runtime/test_info_binding.c",
                 "info": ROOT / "src/world/info/info.c", "info_header": ROOT / "include/mc/world/info/info.h",
                 "level": ROOT / "src/storage/level/level.c", "level_header": ROOT / "include/mc/storage/level/level.h",
                 "saved_data_test": ROOT / "tests/world/saved_data/test_storage.c",
                 "saved_data": ROOT / "src/world/saved_data/storage.c",
                 "saved_data_header": ROOT / "include/mc/world/saved_data/storage.h",
                 "files": ROOT / "src/world/storage/files.c", "files_header": ROOT / "src/world/storage/files.h",
                 "typed_info_test": ROOT / "tests/world/storage/test_typed_info.c",
                 "construction": ROOT / "src/world/runtime/construction.c",
                 "construction_header": ROOT / "include/mc/world/runtime/construction.h",
                 "runtime_internal": ROOT / "src/world/runtime/runtime_internal.h",
                 "dimension": ROOT / "src/world/dimension/dimension.c",
                 "dimension_header": ROOT / "include/mc/world/dimension/dimension.h",
                 "biome": ROOT / "src/world/biome/biome.c",
                 "biome_internal": ROOT / "src/world/biome/biome_internal.h",
                 "provider": ROOT / "src/world/runtime/provider.c",
                 "provider_internal": ROOT / "src/world/runtime/provider_internal.h",
                 "environment": ROOT / "src/world/runtime/environment.c",
                 "environment_header": ROOT / "include/mc/world/runtime/environment.h",
                 "blocks": ROOT / "src/world/runtime/blocks.c",
                 "blocks_header": ROOT / "include/mc/world/runtime/blocks.h"}
        if args.scenario == "game-world":
            paths["game_test"] = ROOT / "tests/world/runtime/test_game_world.c"
        if args.scenario == "world-registry":
            paths.update({
                "registry_test": ROOT / "tests/world/runtime/test_world_registry.c",
                "registry_runtime": ROOT / "src/block/registry/runtime.c",
                "registry_runtime_header": ROOT / "include/mc/block/registry/runtime.h",
                "registry": ROOT / "src/block/registry/registry.c",
                "registry_internal": ROOT / "src/block/registry/registry_internal.h",
                "world_registry": ROOT / "src/world/runtime/registry.c",
            })
        if args.scenario == "world-biome-runtime":
            paths.update({
                "biome_runtime_test": ROOT / "tests/world/dimension/test_biome_runtime.c",
                "biome_runtime_header": ROOT / "include/mc/world/dimension/biome_runtime.h",
                "biome_sampling": ROOT / "src/world/biome/sampling.c",
                "biome_sampling_header": ROOT / "include/mc/world/biome/sampling.h",
                "biome_sampling_internal": ROOT / "src/world/biome/sampling_internal.h",
                "fixed_sampling": ROOT / "src/world/biome/fixed_sampling.c",
                "biome_registry": ROOT / "src/world/biome/registry.c",
                "biome_registry_header": ROOT / "include/mc/world/biome/registry.h",
                "biome_registry_internal": ROOT / "src/world/biome/registry_internal.h",
                "registry_runtime": ROOT / "src/block/registry/runtime.c",
                "registry_runtime_header": ROOT / "include/mc/block/registry/runtime.h",
                "registry_runtime_internal": ROOT / "src/block/registry/runtime_internal.h",
                "registry_internal": ROOT / "src/block/registry/registry_internal.h",
                "world_registry": ROOT / "src/world/runtime/registry.c",
            })
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        for name, path in paths.items():
            if name != "executable":
                (work / (name + path.suffix)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "world root must be fresh")
        command = [str(args.executable.resolve()), args.scenario, str(root).encode("utf-8").hex()]
        stdout, stderr = CAPTURE.run(command, work=work, name="native", records=report["processes"],
                                     deadline=time.monotonic() + 70)
        CAPTURE.require(not stderr, "native contract stderr must be empty")
        text = stdout.decode("ascii").replace("\r\n", "\n")
        if args.scenario in ("world-registry", "world-biome-runtime"):
            lines = text.splitlines(keepends=True)
            CAPTURE.require(bool(lines), "native registry contract result is missing")
            CAPTURE.require(all(line in ("16 achievements\n", "151 recipes\n") for line in lines[:-1]),
                            "unexpected native bootstrap console output")
            report["bootstrap_console"] = lines[:-1]
            text = lines[-1]
        CAPTURE.require(text.startswith("PASS: ") and text.endswith("(" + args.scenario + ")\n") and
                        text.count("\n") == 1, "complete native contract result")
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name], "input changed: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                                 for path in sorted(root.rglob("*")) if path.is_file()}
        timestamps = []
        for path in sorted(root.rglob("*.mcr")):
            with path.open("rb") as stream:
                header = stream.read(8192)
            CAPTURE.require(len(header) == 8192, "complete region header")
            for slot in range(1024):
                if struct.unpack_from(">I", header, slot * 4)[0] != 0:
                    timestamp = struct.unpack_from(">I", header, 4096 + slot * 4)[0]
                    CAPTURE.require(timestamp == 0xfedcba98, "explicit native region clock in live header cell")
                    timestamps.append({"file": path.relative_to(root).as_posix(), "slot": slot, "timestamp": timestamp})
        CAPTURE.require(bool(timestamps) == (args.scenario in ("ownership-and-runtime", "generation", "world-info-bindings")),
                        "expected committed region slots")
        report["region_timestamps_checked"] = timestamps
        report["passed"] = True
        print(text, end="")
        return 0
    except (CAPTURE.Failure, OSError, ValueError) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print("Capture: " + str(work))


if __name__ == "__main__":
    sys.exit(main())
