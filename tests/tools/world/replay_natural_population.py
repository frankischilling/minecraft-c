#!/usr/bin/env python3
"""Run fresh native seed23 population against the public cache-only oracle.

No JAR, Java process, saved Chunk input, or imported population result is used.
The file-backed resources are project-authored minimal test inputs, rather than
the exact original resource profile. Numerical provider records receive native
transport and repeatability checks; they have no original numerical oracle here.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
COMPARISON = ROOT / "scripts/reference/world/population/compare_natural.py"
SPEC = importlib.util.spec_from_file_location("native_population_replay_comparison", COMPARISON)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing natural population comparison support")
COMPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPARE)
CAPTURE, PUBLIC = COMPARE.CAPTURE, COMPARE.PUBLIC

# The same four project-authored format strings as population/test_normal.c.
LANGUAGE = (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
            b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")
RESOURCES = {"lang/en_US.lang": LANGUAGE, "lang/stats_US.lang": b"",
             "achievement/map.txt": b""}
GENERATION = ("q", "d", "e", "f", "g", "h", "r", "s", "t")
RANDOMS = ("generator_random", "cave_random", "world_random")


def keys(value, names, label):
    COMPARE.require(type(value) is dict and set(value) == set(names), label + ": complete keys")


def copied_longs(seed):
    """Check the encoded continuation against its captured Java 48-bit state."""
    state, result = int(seed, 16), []
    for _ in range(8):
        state = (state * 0x5DEECE66D + 11) & ((1 << 48) - 1)
        high = state >> 16
        state = (state * 0x5DEECE66D + 11) & ((1 << 48) - 1)
        low = state >> 16
        if low >= 1 << 31:
            low -= 1 << 32
        result.append(f"{((high << 32) + low) & ((1 << 64) - 1):016x}")
    return result


def validate_native(native, side, expected):
    keys(native, ("schema", "scope", "side", "seed_hex", "initial", "operations", "failure"),
         "native observation")
    COMPARE.equal(native["side"], side, "native side")
    COMPARE.cache_projection(native, expected)
    snapshots = [native["initial"]]
    for row in native["operations"]:
        keys(row, ("index", "method", "coordinates", "result", "status", "snapshot"),
             "native operation")
        snapshots.append(row["snapshot"])
    shapes, world_random = {}, None
    counts = {"boundaries": len(snapshots), "random_records": 0,
              "array_records": 0, "nonnull_array_records": 0}
    for index, snapshot in enumerate(snapshots):
        keys(snapshot, ("context", "world", "provider", "shared"), "native snapshot")
        provider, shared = snapshot["provider"], snapshot["shared"]
        keys(provider, (*RANDOMS, "generation", "v", "manager", "w"), "native provider")
        keys(provider["generation"], GENERATION, "native generation fields")
        keys(provider["manager"], ("a", "b", "c", "d"), "native manager fields")
        keys(shared, ("drain_depth", "chunk_positive_sky", "falling"), "native shared fields")
        COMPARE.require(type(shared["drain_depth"]) is int and
                        -(1 << 31) <= shared["drain_depth"] < 1 << 31 and
                        type(shared["chunk_positive_sky"]) is bool and
                        type(shared["falling"]) is bool, "native shared field types")
        for name in RANDOMS:
            value = COMPARE.random_value(provider[name])
            COMPARE.equal(value["next_longs"], copied_longs(value["seed48"]),
                          name + " copied continuation boundary " + str(index))
            counts["random_records"] += 1
            if name == "world_random":
                if index == 0:
                    world_random = value
                COMPARE.equal(value, world_random,
                              "per-run constructor World.r unchanged boundary " + str(index))
        arrays = [(name, provider["generation"][name], False) for name in GENERATION]
        arrays.extend((("w", provider["w"], False), ("v", provider["v"], True)))
        arrays.extend(("manager." + name, provider["manager"][name], name == "d")
                      for name in ("a", "b", "c", "d"))
        aliases = {}
        for name, array, biome in arrays:
            value = COMPARE.array_value(array, biome=biome)
            counts["array_records"] += 1
            if array is None:
                continue
            counts["nonnull_array_records"] += 1
            identity, shape = array["identity"], (biome, value["length"])
            if identity in shapes:
                COMPARE.equal(list(shape), list(shapes[identity]),
                              "retained source array type/length " + name)
            else:
                shapes[identity] = shape
            if identity in aliases:
                COMPARE.equal(value, aliases[identity], "same-boundary array alias " + name)
            else:
                aliases[identity] = value
    for name in ("provider", "shared"):
        COMPARE.equal(snapshots[-1][name], snapshots[-2][name],
                      "repeated cache hit preserves complete " + name + " state and identities")
    return counts


def input_paths(executable):
    paths = {"executable": executable, "python": Path(sys.executable).resolve(),
             "driver": Path(__file__).resolve(), "comparison": COMPARISON,
             "public_fixture": PUBLIC.FIXTURE, "public_validator": Path(PUBLIC.__file__).resolve(),
             "capture_support": Path(CAPTURE.__file__).resolve(), "cmake": ROOT / "CMakeLists.txt",
             "probe": ROOT / "tests/world/runtime/population/natural_probe.c",
             "resource_example": ROOT / "tests/world/runtime/population/test_normal.c"}
    for base, suffixes in (("src", (".c", ".h")), ("include", (".h",)),
                           ("scripts/reference", (".py",))):
        for path in sorted((ROOT / base).rglob("*")):
            if path.is_file() and path.suffix in suffixes:
                paths[path.relative_to(ROOT).as_posix()] = path
    manifest = ROOT / ".source-manifest.json"
    if manifest.exists():
        paths["source_export_manifest"] = manifest
    COMPARE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                    "ordinary complete selected native replay inputs")
    if "source_export_manifest" in paths:
        exported = COMPARE.read_json(manifest)
        COMPARE.require(type(exported) is dict and type(exported.get("schema")) is int and
                        exported["schema"] == 1 and type(exported.get("files")) is dict,
                        "selected source export manifest")
        for name, path in paths.items():
            if name not in ("executable", "python", "source_export_manifest") and path.is_relative_to(ROOT):
                relative = path.relative_to(ROOT).as_posix()
                COMPARE.require(relative in exported["files"], "export includes replay input: " + relative)
                COMPARE.equal(CAPTURE.file_fingerprint(path), exported["files"][relative],
                              "selected exported replay input: " + relative)
    return paths


def write_resources(root):
    COMPARE.require(not root.exists(), "fresh owned minimal resource root")
    root.mkdir()
    result = {}
    for name, raw in RESOURCES.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(raw)
        result[name] = CAPTURE.file_fingerprint(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed23 fresh native GAME population; public cache oracle only",
              "passed": False, "tested_commit": None, "jar_free": True,
              "original_cache_comparison": True, "original_numerical_comparison": False,
              "whole_population_equivalence": False, "imports_original_effects": False,
              "resource_binding": "project-authored minimal translation files; not exact original resources",
              "source_attribution_limit": "Selected tool/source fingerprints; no build-to-source or transitive system-library attestation.",
              "processes": [], "sides": {}}
    try:
        executable = args.executable.resolve()
        COMPARE.require(executable.is_file() and not executable.is_symlink(), "actual native population probe")
        work = CAPTURE.capture_root(args.work_dir, [Path(sys.executable).resolve()])
        paths = input_paths(executable)
        report["inputs"] = {name: {"path": str(path), **CAPTURE.file_fingerprint(path)}
                            for name, path in paths.items()}
        report["python_version"] = sys.version
        _, decoded = PUBLIC.load_fixture()
        expected = {row["record"]["side"]: row["expected"] for row in decoded
                    if row["record"]["scenario"] == "population"}
        COMPARE.equal(sorted(expected), ["client", "server"], "both public population cases")
        for side in ("client", "server"):
            directory = work / side
            directory.mkdir()
            resources, world = directory / "resources", directory / "native-world"
            output = directory / "native.json"
            row = {"passed": False, "resources": write_resources(resources)}
            report["sides"][side] = row
            COMPARE.require(not world.exists() and not world.is_symlink() and not output.exists(),
                            "fresh GAME World and observation output")
            COMPARE.run([str(executable), side, str(resources).encode("utf-8").hex(),
                         str(world).encode("utf-8").hex(), str(output)],
                        work, side + "-native", report["processes"])
            COMPARE.equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            COMPARE.equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                          b"16 achievements\n151 recipes\n", "complete native bootstrap console")
            native = COMPARE.read_json(output)
            row["native_output"] = CAPTURE.file_fingerprint(output)
            row["numerical_transport"] = validate_native(native, side, expected[side])
            row["public_cache_match"] = True
            row["repeated_provider_and_shared_state_retained"] = True
            for name, raw in RESOURCES.items():
                COMPARE.equal((resources / name).read_bytes(), raw, "minimal resource preserved: " + name)
                COMPARE.equal(CAPTURE.file_fingerprint(resources / name), row["resources"][name],
                              "resource fingerprint preserved: " + name)
            row["passed"] = True
            print("PASS: " + side + " seed23 native population; public cache oracle, minimal resources")
        for name, path in paths.items():
            COMPARE.equal({"path": str(path), **CAPTURE.file_fingerprint(path)}, report["inputs"][name],
                          "replay input preserved: " + name)
        report["inputs_unchanged"] = True
        report["files"] = COMPARE.file_inventory(work)
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
                    report["files"] = COMPARE.file_inventory(work)
                except Exception as error:
                    report["inventory_failure"] = str(error)
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
