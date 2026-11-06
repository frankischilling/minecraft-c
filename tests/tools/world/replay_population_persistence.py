#!/usr/bin/env python3
"""Run fresh native populated Chunk saves/readbacks against public originals.

No JAR, Java process, saved Chunk input, or imported population effect is used.
The three file-backed resources are the same project-authored minimal test
inputs as replay_natural_population.py, rather than exact original resources.
The fixed seed23 profile compares the origin population, two forced cache
saves and four direct loads. Full World save, unload and restart are outside
this finite comparison.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
COMPARISON = ROOT / "scripts/reference/world/population/compare_persistence.py"
FIXTURE_MODULE = ROOT / "scripts/reference/world/population/persistence_fixtures.py"
FIXTURE = ROOT / "tests/reference/world/population/persistence_fixtures.json"
SPEC = importlib.util.spec_from_file_location("public_population_persistence_comparison", COMPARISON)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing population save/readback comparison support")
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)
C, CAPTURE, PUBLIC = P.C, P.C.CAPTURE, P.C.PUBLIC
REPLAY = P.load("public_population_persistence_resources", "tests/tools/world/replay_natural_population.py")


def input_paths(executable):
    paths = REPLAY.input_paths(executable)
    paths.update(driver=Path(__file__).resolve(), comparison=COMPARISON,
                 natural_comparison=ROOT / "scripts/reference/world/population/compare_natural.py",
                 resource_replay=Path(REPLAY.__file__).resolve(),
                 persistence_fixture_module=FIXTURE_MODULE, persistence_fixture=FIXTURE,
                 storage_transport_validator=ROOT / "tests/tools/storage/test_store.py")
    paths.update({source: ROOT / source for source in P.SOURCES})
    C.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
              "ordinary complete selected persistence replay inputs")
    manifest = paths.get("source_export_manifest")
    if manifest is not None:
        exported = C.read_json(manifest)
        C.require(type(exported) is dict and type(exported.get("schema")) is int and
                  exported["schema"] == 1 and type(exported.get("files")) is dict,
                  "selected source export manifest")
        for name, path in paths.items():
            if name not in ("executable", "python", "source_export_manifest") and path.is_relative_to(ROOT):
                relative = path.relative_to(ROOT).as_posix()
                C.require(relative in exported["files"], "export includes replay input: " + relative)
                C.equal(CAPTURE.file_fingerprint(path), exported["files"][relative],
                        "selected exported replay input: " + relative)
    return paths


def validate_transport(snapshots):
    """Check every transported provider descriptor, including copied RNG tails."""
    shapes, world_random = {}, None
    counts = {"boundaries": len(snapshots), "random_records": 0,
              "array_records": 0, "nonnull_array_records": 0}
    for index, snapshot in enumerate(snapshots):
        P.keys(snapshot, ("context", "world", "provider", "shared"), "native snapshot")
        provider, shared = snapshot["provider"], snapshot["shared"]
        P.keys(provider, (*REPLAY.RANDOMS, "generation", "v", "manager", "w"), "native provider")
        P.keys(provider["generation"], REPLAY.GENERATION, "native generation fields")
        P.keys(provider["manager"], ("a", "b", "c", "d"), "native manager fields")
        P.keys(shared, ("drain_depth", "chunk_positive_sky", "falling"), "native shared fields")
        P.integer(shared["drain_depth"], -(1 << 31), (1 << 31) - 1, "native drain depth")
        C.require(type(shared["chunk_positive_sky"]) is bool and type(shared["falling"]) is bool,
                  "native shared Boolean fields")
        for name in REPLAY.RANDOMS:
            value = C.random_value(provider[name])
            C.equal(value["next_longs"], REPLAY.copied_longs(value["seed48"]),
                    name + " copied continuation boundary " + str(index))
            counts["random_records"] += 1
            if name == "world_random":
                if index == 0:
                    world_random = value
                C.equal(value, world_random, "per-run constructor World.r retained boundary " + str(index))
        arrays = [(name, provider["generation"][name], False) for name in REPLAY.GENERATION]
        arrays.extend((("w", provider["w"], False), ("v", provider["v"], True)))
        arrays.extend(("manager." + name, provider["manager"][name], name == "d")
                      for name in ("a", "b", "c", "d"))
        aliases = {}
        for name, array, biome in arrays:
            value = C.array_value(array, biome=biome)
            counts["array_records"] += 1
            if array is None:
                continue
            counts["nonnull_array_records"] += 1
            identity, shape = array["identity"], (biome, value["length"])
            if identity in shapes:
                C.equal(list(shape), list(shapes[identity]), "retained array type/length " + name)
            else:
                shapes[identity] = shape
            if identity in aliases:
                C.equal(value, aliases[identity], "same-boundary array alias " + name)
            else:
                aliases[identity] = value
    return counts


def validate_native(native, side, case, origin_expected):
    C.equal(native.get("side"), side, "requested native persistence side")
    P.compare_original(native, case["original"], origin_expected, case["jar"])
    # Retain the older immutable cache control and strict origin event schema.
    REPLAY.validate_native(native["population"], side, origin_expected)
    snapshots = [native["population"]["initial"],
                 *[row["snapshot"] for row in native["population"]["operations"]],
                 *[row["snapshot"] for row in P.all_boundaries(native["persistence"])]]
    C.equal(len(snapshots), 20, "five origin and fifteen persistence boundaries")
    return validate_transport(snapshots)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed23 fresh native GAME origin population and Chunk save/direct-readback; original oracle with minimal resources",
              "profile": "save-readback", "passed": False, "tested_commit": None, "jar_free": True,
              "original_cache_comparison": True, "original_numerical_comparison": True,
              "original_saved_nbt_comparison": True, "original_direct_load_comparison": True,
              "whole_population_equivalence": False, "imports_original_effects": False,
              "full_world_save": False, "unload": False, "world_restart": False,
              "resource_binding": "project-authored minimal translation files; not exact original resources",
              "source_attribution_limit": "Selected tool/source fingerprints; no build-to-source or transitive system-library attestation.",
              "processes": [], "sides": {}}
    try:
        executable = args.executable.resolve()
        C.require(executable.is_file() and not executable.is_symlink(), "actual native population probe")
        work = CAPTURE.capture_root(args.work_dir, [Path(sys.executable).resolve()])
        C.require(not executable.is_relative_to(work), "capture excludes selected executable")
        paths = input_paths(executable)
        report["inputs"] = {name: {"path": str(path), **CAPTURE.file_fingerprint(path)}
                            for name, path in paths.items()}
        report["python_version"] = sys.version
        _, decoded = PUBLIC.load_fixture()
        origin_expected = {row["record"]["side"]: row["expected"] for row in decoded
                           if row["record"]["scenario"] == "population"}
        P.keys(origin_expected, ("client", "server"), "both public origin controls")
        fixtures = P.load("public_population_persistence_fixtures",
                          FIXTURE_MODULE.relative_to(ROOT).as_posix())
        cases = fixtures.load_package(FIXTURE, P)
        P.keys(cases, ("client", "server"), "both original persistence cases")
        for side in ("client", "server"):
            directory = work / side
            directory.mkdir()
            resources, world = directory / "resources", directory / "native-world"
            output = directory / "native.json"
            row = {"passed": False, "resources": REPLAY.write_resources(resources)}
            report["sides"][side] = row
            C.require(not world.exists() and not world.is_symlink() and
                      not output.exists() and not output.is_symlink(),
                      "fresh GAME World and persistence output")
            command = [str(executable), side, str(resources).encode("utf-8").hex(),
                       str(world).encode("utf-8").hex(), str(output), "save-readback"]
            C.run(command, work, side + "-native", report["processes"])
            C.equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            C.equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                    b"16 achievements\n151 recipes\n", "complete native bootstrap console")
            native = C.read_json(output)
            row["native_output"] = CAPTURE.file_fingerprint(output)
            row["numerical_transport"] = validate_native(native, side, cases[side], origin_expected[side])
            row.update(public_cache_match=True, original_numerical_match=True,
                       forced_saves=2, direct_loads=4, complete_typed_nbt=True,
                       complete_disk_preservation=True)
            for name, raw in REPLAY.RESOURCES.items():
                C.equal((resources / name).read_bytes(), raw, "minimal resource preserved: " + name)
                C.equal(CAPTURE.file_fingerprint(resources / name), row["resources"][name],
                        "resource fingerprint preserved: " + name)
            row["passed"] = True
            print("PASS: " + side + " seed23 native populated Chunk save/direct-readback; original oracle, minimal resources")
        for name, path in paths.items():
            C.equal({"path": str(path), **CAPTURE.file_fingerprint(path)}, report["inputs"][name],
                    "replay input preserved: " + name)
        report["inputs_unchanged"] = True
        report["files"] = C.file_inventory(work)
        report["passed"] = True
        return 0
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            if "files" not in report:
                try:
                    report["files"] = C.file_inventory(work)
                except Exception as error:
                    report["inventory_failure"] = str(error)
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
