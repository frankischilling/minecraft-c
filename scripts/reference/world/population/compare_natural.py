#!/usr/bin/env python3
"""Compare a fresh production GAME World with genuine original population.

The finite seed-23 schedule compares complete retained chunk arrays, cache
state and ordered light regions. Original observations supply every expected
voxel. Provider state extends the older, explicitly opaque callback corpus.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[4]
MAX_BYTES = 64 * 1024 * 1024
SEED = "0000000000000017"
ORIGIN_SCHEDULE = ((1, 0), (0, 1), (1, 1), (1, 1))
RING_SCHEDULE = (*ORIGIN_SCHEDULE, (-1, 0), (-1, 1), (0, -1), (1, -1), (-1, -1), (-1, -1))
SCHEDULES = {"origin": ORIGIN_SCHEDULE, "ring": RING_SCHEDULE}
RESOURCES = ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt")
SOURCES = ("tests/reference/chunk/ChunkObservation.java",
           "tests/reference/cache/CacheCallbacksObservation.java",
           "tests/reference/world/population/NaturalPopulationObservation.java")


def load_module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    if spec is None or spec.loader is None:
        raise RuntimeError("missing comparison support: " + relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PUBLIC = load_module("natural_population_public", "tests/tools/cache/test_callbacks_interop.py")
CAPTURE = load_module("natural_population_capture", "scripts/reference/java/compare_int_set.py")
HASHES = PUBLIC.JAR_HASHES


def require(value, message):
    if not value:
        raise ValueError(message)


def read_json(path):
    require(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= MAX_BYTES,
            "bounded complete observation: " + str(path))
    return PUBLIC.strict_json(path.read_bytes())


def first_difference(actual, expected, path="$", depth=0):
    """Report a location without copying full terrain payloads into logs."""
    if type(actual) is not type(expected):
        return path + ": JSON type"
    if type(actual) is dict:
        if actual.keys() != expected.keys():
            return path + ": keys " + repr(sorted(actual.keys() ^ expected.keys()))
        for key in expected:
            result = first_difference(actual[key], expected[key], path + "." + key, depth + 1)
            if result:
                return result
    elif type(actual) is list:
        if len(actual) != len(expected):
            return path + ": length " + str(len(actual)) + " != " + str(len(expected))
        for index, (left, right) in enumerate(zip(actual, expected)):
            result = first_difference(left, right, path + "[" + str(index) + "]", depth + 1)
            if result:
                return result
    elif actual != expected:
        if type(actual) is str and max(len(actual), len(expected)) > 128:
            different = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                             min(len(actual), len(expected)))
            return path + ": character " + str(different) + " (lengths " + str(len(actual)) + "," + str(len(expected)) + ")"
        return path + ": " + repr(actual) + " != " + repr(expected)
    return None


def equal(actual, expected, label):
    difference = first_difference(actual, expected)
    require(difference is None, label + ": " + str(difference))


def cache_projection(native, expected):
    profile = native.get("profile", "origin") if type(native) is dict else None
    require(type(native) is dict and native.get("schema") == 1 and
            type(native["schema"]) is int and type(profile) is str and profile in SCHEDULES and
            native.get("scope") == "native-game-normal-population" + ("-ring" if profile == "ring" else "") and
            native.get("side") in HASHES and native.get("seed_hex") == SEED,
            "native GAME profile")
    require(type(native.get("operations")) is list and
            len(native["operations"]) == len(expected["operations"]) == len(SCHEDULES[profile]),
            "complete native acquisition schedule")
    require(native.get("failure") is None, "native producer reported a failure")
    equal({name: native["initial"][name] for name in ("context", "world")},
          expected["initial"], "constructor state")
    for index, (row, reference) in enumerate(zip(native["operations"], expected["operations"])):
        equal({name: row[name] for name in ("index", "method", "coordinates", "result", "status")},
              {name: reference[name] for name in ("index", "method", "coordinates", "result", "status")},
              "operation " + str(index) + " result")
        equal({name: row["snapshot"][name] for name in ("context", "world")},
              reference["snapshot"], "operation " + str(index) + " complete state")


def packed_key(x, z):
    """Actual yy/qa key: independent sign bits and fifteen-bit coordinate halves."""
    raw = (0x80000000 if x < 0 else 0) | ((x & 32767) << 16) | (32768 if z < 0 else 0) | (z & 32767)
    return raw if raw < 1 << 31 else raw - (1 << 32)


def validate_original_observation(value, side, profile, jar):
    require(type(profile) is str and profile in SCHEDULES, "known original population profile")
    required = {"schema", "scope", "side", "seed_hex", "jar_sha256", "callbacks", "states"}
    if profile == "ring":
        required.add("profile")
    require(type(value) is dict and set(value) == required and
            type(value["schema"]) is int and value["schema"] == 1 and
            value["scope"] == "original-game-normal-population" + ("-ring" if profile == "ring" else "") and
            value.get("profile", "origin") == profile and value["side"] == side and
            value["seed_hex"] == SEED and value["jar_sha256"] == HASHES[side],
            "fresh original profile")
    _, _, finish, _, acquire, _ = PUBLIC.driver.helpers()
    callbacks = value["callbacks"]
    if profile == "origin":
        expected, _ = PUBLIC.driver.validate_observation(callbacks, side, "population", jar, acquire, finish)
        return expected
    require(type(callbacks) is dict and type(callbacks.get("operations")) is list and
            len(callbacks["operations"]) == len(RING_SCHEDULE), "complete original ring schedule")
    # Preserve the original validator unchanged: its exact header, constructor
    # and four-call acceptance apply to the genuine prefix of this larger run.
    prefix = dict(callbacks, operations=callbacks["operations"][:len(ORIGIN_SCHEDULE)])
    PUBLIC.driver.validate_observation(prefix, side, "population", jar, acquire, finish)
    ids = acquire.identities(callbacks)
    previous_raw = callbacks["initial"]
    previous = PUBLIC.driver.snapshot_projection(previous_raw, ids, acquire, finish)
    expected = {"schema": 1, "scope": "natural-population-ring", "seed_hex": SEED,
                "initial": previous, "operations": []}
    for index, (row, coordinates) in enumerate(zip(callbacks["operations"], RING_SCHEDULE)):
        require(type(row) is dict and set(row) == {"index", "method", "coordinates", "result", "before", "after"},
                "complete original ring operation")
        equal({name: row[name] for name in ("index", "method", "coordinates")},
              {"index": index, "method": "c", "coordinates": list(coordinates)}, "original ring schedule")
        for snapshot in (row["before"], row["after"]):
            PUBLIC.driver.retained_snapshot_owners(snapshot, callbacks["initial"], acquire)
            PUBLIC.driver.retained_snapshot_owners(snapshot, previous_raw, acquire)
        before = PUBLIC.driver.snapshot_projection(row["before"], ids, acquire, finish)
        equal(before, previous, "continuous original ring state")
        inventory = acquire.chunk_map(row["after"])
        for entry in row["after"]["world"]["insertion_list"]["entries"]:
            identity = entry["value"]
            require(identity in inventory, "actual chronologically inserted original Chunk")
            if identity not in ids:
                ids[identity] = len(ids)
        require(set(ids) == set(inventory), "all original ring identities observed in insertion order")
        result = acquire.ref(row["result"])
        require(result in ids, "returned original ring identity retained")
        after = PUBLIC.driver.snapshot_projection(row["after"], ids, acquire, finish)
        equal(after["context"], expected["initial"]["context"], "original ring context retained")
        mappings = dict(after["world"]["map"])
        for key, number in after["world"]["map"]:
            chunk = after["world"]["chunks"][number]
            require(not chunk["blank"] and key == packed_key(*chunk["coordinates"]),
                    "source-packed original ring cache key")
        require(mappings.get(packed_key(*coordinates)) == ids[result] and
                after["world"]["chunks"][ids[result]]["coordinates"] == list(coordinates),
                "actual ring result published at requested key")
        if packed_key(*coordinates) in dict(before["world"]["map"]):
            equal(after, before, "repeated original ring cache hit")
        expected["operations"].append({"index": index, "method": "c", "coordinates": list(coordinates),
                                       "result": ids[result], "status": "ok", "snapshot": after})
        previous, previous_raw = after, row["after"]
    return expected


def array_value(value, *, biome=False):
    if value is None:
        return None
    require(type(value) is dict and set(value) == {"identity", "length", "values"} and
            type(value["identity"]) is int and value["identity"] >= 0 and
            type(value["length"]) is int and 0 <= value["length"] <= 4096,
            "native source array descriptor")
    values = value["values"]
    if biome:
        require(type(values) is list and len(values) == value["length"] and
                all(item is None or type(item) is str and len(item) == 1 and "a" <= item <= "m"
                    for item in values), "actual registered native biome identities")
        values = "".join("-" if item is None else item for item in values)
    else:
        require(type(values) is str and len(values) == value["length"] * 16 and
                re.fullmatch("[0-9a-f]*", values) is not None, "complete native binary64 array")
    return {"length": value["length"], "values": values}


def random_value(value):
    require(type(value) is dict and set(value) == {
        "state", "have_next_next_gaussian", "next_next_gaussian_bits", "next_longs"},
        "complete native Random state and copied continuation")
    require(type(value["state"]) is str and re.fullmatch("[0-9a-f]{12}", value["state"]) and
            type(value["have_next_next_gaussian"]) is bool and
            type(value["next_next_gaussian_bits"]) is str and
            re.fullmatch("[0-9a-f]{16}", value["next_next_gaussian_bits"]) and
            type(value["next_longs"]) is list and len(value["next_longs"]) == 8 and
            all(type(item) is str and re.fullmatch("[0-9a-f]{16}", item)
                for item in value["next_longs"]), "native Random values")
    return {"seed48": value["state"], **{name: value[name] for name in
            ("have_next_next_gaussian", "next_next_gaussian_bits", "next_longs")}}


def original_random_value(value):
    require(type(value) is dict and set(value) == {
        "seed48", "have_next_next_gaussian", "next_next_gaussian_bits", "next_longs"},
        "complete original Random state and copied continuation")
    return random_value({"state": value["seed48"], **{name: value[name] for name in
                        ("have_next_next_gaussian", "next_next_gaussian_bits", "next_longs")}})


def array_id(value):
    return None if value is None else value["identity"]


def normalize_identities(sequence):
    """Compare cross-field aliases and replacements, never host addresses/IDs."""
    names = {}
    result = []
    for row in sequence:
        projected = {}
        for field, identity in row.items():
            if identity is not None and identity not in names:
                names[identity] = len(names)
            projected[field] = None if identity is None else names[identity]
        result.append(projected)
    return result


def compare_original(native, original, public, jar, *, profile):
    require(type(profile) is str and profile in SCHEDULES, "known requested population profile")
    equal(native.get("profile", "origin"), profile, "requested native population profile")
    equal(original.get("profile", "origin"), profile, "requested original population profile")
    _, _, finish, _, acquire, _ = PUBLIC.driver.helpers()
    expected = validate_original_observation(original, native["side"], profile, jar)
    cache_projection(native, expected)
    # The older immutable corpus had JDI instrumentation. A fresh untraced
    # control must reproduce its complete projected source state as well.
    for field in ("initial",):
        equal(expected[field], public[field], "fresh/public original " + field)
    for index, (fresh, previous) in enumerate(zip(expected["operations"], public["operations"])):
        equal({key: fresh[key] for key in ("index", "method", "coordinates", "result", "status", "snapshot")},
              {key: previous[key] for key in ("index", "method", "coordinates", "result", "status", "snapshot")},
              "fresh/public original operation " + str(index))
    snapshots = [native["initial"], *[row["snapshot"] for row in native["operations"]]]
    states = original["states"]
    require(type(states) is list and len(states) == len(snapshots) == len(SCHEDULES[profile]) + 1,
            "complete numerical observation boundaries")
    native_ids, original_ids = [], []
    native_world_random, original_world_random = None, None
    for index, (snapshot, state) in enumerate(zip(snapshots, states)):
        provider, observed = snapshot["provider"], state["provider"]
        names = ("q", "d", "e", "f", "g", "h", "r", "s", "t")
        equal({**{name: array_value(provider["generation"][name]) for name in names},
               "w": array_value(provider["w"])}, observed["scratch"],
              "provider scratch boundary " + str(index))
        equal(array_value(provider["v"], biome=True), observed["biomes"],
              "provider biome identities boundary " + str(index))
        for name in ("generator_random", "cave_random"):
            equal(random_value(provider[name]), original_random_value(observed[name]), name + " boundary " + str(index))
        equal({name: array_value(provider["manager"][name], biome=name == "d")
               for name in ("a", "b", "c", "d")},
              {name: state["aux"]["manager"][name] for name in ("a", "b", "c", "d")},
              "manager buffers boundary " + str(index))
        native_identity = {name: array_id(provider["generation"][name]) for name in names}
        native_identity.update(w=array_id(provider["w"]), v=array_id(provider["v"]))
        original_identity = {name: acquire.ref(observed["identities"][name]) for name in (*names, "w", "v")}
        for name in ("a", "b", "c", "d"):
            native_identity["manager." + name] = array_id(provider["manager"][name])
            original_identity["manager." + name] = acquire.ref(state["aux"]["manager"]["identities"][name])
        native_ids.append(native_identity)
        original_ids.append(original_identity)
        shared = snapshot["shared"]
        equal({"drain_depth": shared["drain_depth"], "chunk_static_a": shared["chunk_positive_sky"],
               "falling": shared["falling"]},
              {name: state["aux"][name] for name in ("drain_depth", "chunk_static_a", "falling")},
              "shared state boundary " + str(index))
        actual_random = random_value(provider["world_random"])
        actual_original = original_random_value(state["aux"]["world_random"])
        if index == 0:
            native_world_random, original_world_random = actual_random, actual_original
        equal(actual_random, native_world_random, "native clock-seeded World.r unchanged boundary " + str(index))
        equal(actual_original, original_world_random, "original clock-seeded World.r unchanged boundary " + str(index))
        equal(state["aux"]["world_random_attribution"],
              "per-run-constructor-input; never replaced or reseeded", "World.r input attribution")
        for name in ("world_random_identity", "world_random_seed_identity"):
            require(acquire.ref(state["aux"][name]) is not None, "actual original World.r owner")
            equal(state["aux"][name], states[0]["aux"][name], "original World.r owner retained")
    equal(normalize_identities(native_ids), normalize_identities(original_ids),
          "actual array alias/replacement graph")


def extract_resources(jar, destination):
    require(not destination.exists(), "fresh resource directory")
    destination.mkdir()
    result = {}
    with zipfile.ZipFile(jar) as archive:
        for name in RESOURCES:
            entries = [entry for entry in archive.infolist() if entry.filename == name]
            require(len(entries) == 1 and not entries[0].is_dir() and
                    0 < entries[0].file_size <= 128 * 1024,
                    "unique bounded original resource: " + name)
            raw = archive.read(entries[0])
            require(len(raw) == entries[0].file_size, "complete resource extraction")
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            result[name] = CAPTURE.file_fingerprint(path)
    return result


def run(command, work, name, records, seconds=120):
    stdout, stderr = work / (name + ".stdout"), work / (name + ".stderr")
    record = {"name": name, "command": command, "exit": None, "reaped": False,
              "timed_out": False, "output_limited": False}
    records.append(record)
    with stdout.open("wb") as outgoing, stderr.open("wb") as errors:
        child = subprocess.Popen(command, cwd=work, stdin=subprocess.DEVNULL,
                                 stdout=outgoing, stderr=errors)
        deadline = time.monotonic() + seconds
        try:
            while child.poll() is None:
                if time.monotonic() >= deadline:
                    record["timed_out"] = True
                    break
                if max(stdout.stat().st_size, stderr.stat().st_size) > MAX_BYTES:
                    record["output_limited"] = True
                    break
                time.sleep(0.02)
        finally:
            if child.poll() is None:
                child.kill()
            child.wait()
            record["reaped"], record["exit"] = True, child.returncode
    for label, path in (("stdout", stdout), ("stderr", stderr)):
        record[label] = CAPTURE.file_fingerprint(path)
        record["output_limited"] |= record[label]["bytes"] > MAX_BYTES
    require(not record["timed_out"], "child timeout: " + name)
    require(not record["output_limited"], "child output limit: " + name)
    require(child.returncode == 0,
            "child failed: " + name + " exit " + str(child.returncode) + "; see retained stderr")


def source_inputs(executable, java, javac, jars):
    paths = {"executable": executable, "driver": Path(__file__).resolve(),
             "python": Path(sys.executable).resolve(),
             "cmake": ROOT / "CMakeLists.txt",
             "public_fixture": PUBLIC.FIXTURE,
             "public_validator": Path(PUBLIC.__file__).resolve(),
             "capture_support": Path(CAPTURE.__file__).resolve(),
             **CAPTURE.jdk_inputs(java, javac)}
    paths.update({side + "_jar": path for side, path in jars.items()})
    for base, suffixes in (("src", (".c", ".h")), ("include", (".h",)),
                           ("tests/reference", (".java",)), ("scripts/reference", (".py",))):
        for path in sorted((ROOT / base).rglob("*")):
            if path.is_file() and path.suffix in suffixes:
                paths[path.relative_to(ROOT).as_posix()] = path
    paths["probe"] = ROOT / "tests/world/runtime/population/natural_probe.c"
    if (ROOT / ".source-manifest.json").is_file():
        paths["source_export_manifest"] = ROOT / ".source-manifest.json"
    require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
            "ordinary complete selected comparison inputs")
    if "source_export_manifest" in paths:
        exported = read_json(paths["source_export_manifest"])["files"]
        for name, path in paths.items():
            if path.is_relative_to(ROOT) and name != "source_export_manifest" and not name == "executable":
                relative = path.relative_to(ROOT).as_posix()
                require(relative in exported, "export includes selected source input: " + relative)
                equal(CAPTURE.file_fingerprint(path), exported[relative], "selected exported source: " + relative)
    return paths


def file_inventory(root):
    result = {}
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), "ordinary owned capture entry")
        if path.is_file():
            result[path.relative_to(root).as_posix()] = CAPTURE.file_fingerprint(path)
    return result


def source_context(work, records):
    if (ROOT / ".git").exists():
        for role, command in (("head", ["git", "-C", str(ROOT), "rev-parse", "HEAD"]),
                              ("status", ["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=all"])):
            run(command, work, "git-" + role, records)
        return {"source_head": (work / "git-head.stdout").read_text(encoding="ascii").strip(),
                "source_dirty": bool((work / "git-status.stdout").read_bytes().strip()),
                "source_export": False}
    manifest = read_json(ROOT / ".source-manifest.json")
    require(type(manifest) is dict and type(manifest.get("schema")) is int and manifest["schema"] == 1 and
            type(manifest.get("source_head")) is str and
            re.fullmatch("[0-9a-f]{40}", manifest["source_head"]) and
            type(manifest.get("source_dirty")) is bool and type(manifest.get("files")) is dict and
            1 <= len(manifest["files"]) <= 10000, "complete exported source attribution")
    for name, fingerprint in manifest["files"].items():
        require(type(name) is str and "\\" not in name and ":" not in name and
                all(part not in ("", ".", "..") for part in name.split("/")),
                "relative ordinary exported source path")
        path = ROOT / name
        require(path.is_file() and not path.is_symlink(), "complete exported source file")
        equal(CAPTURE.file_fingerprint(path), fingerprint, "exported source fingerprint: " + name)
    return {"source_head": manifest["source_head"], "source_dirty": manifest["source_dirty"],
            "source_export": True, "source_export_manifest": CAPTURE.file_fingerprint(ROOT / ".source-manifest.json")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    parser.add_argument("--profile", choices=tuple(SCHEDULES), default="origin")
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed-23 fresh GAME World normal population", "profile": args.profile,
              "passed": False, "tested_commit": args.tested_commit or None,
              "whole_population_equivalence": False, "processes": [], "sides": {}}
    try:
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit),
                "complete tested commit")
        require(not any(os.environ.get(name) for name in
                        ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")),
                "uninjected original Java environment")
        tools = {name: getattr(args, name).resolve() for name in ("executable", "java", "javac")}
        jars = {side: getattr(args, side).resolve() for side in HASHES}
        require(all(path.is_file() for path in (*tools.values(), *jars.values())), "selected tools/JARs")
        for side, jar in jars.items():
            require(CAPTURE.file_fingerprint(jar)["sha256"] == HASHES[side], "pinned original " + side)
        work = CAPTURE.capture_root(args.work_dir, [sys.executable, tools["java"], tools["javac"]])
        require(not tools["executable"].is_relative_to(work), "capture excludes selected executable")
        report.update(source_context(work, report["processes"]))
        require(not args.tested_commit or args.tested_commit == report["source_head"] and
                report["source_dirty"] is False, "tested commit requires matching clean source")
        paths = source_inputs(**tools, jars=jars)
        report["inputs"] = {name: {"path": str(path), **CAPTURE.file_fingerprint(path)}
                            for name, path in paths.items()}
        classes = work / "classes"
        classes.mkdir()
        run([str(tools["javac"]), "--release", "17", "-d", str(classes),
             *[str(ROOT / source) for source in SOURCES]], work, "compile", report["processes"])
        _, decoded = PUBLIC.load_fixture()
        expected = {row["record"]["side"]: row["expected"] for row in decoded
                    if row["record"]["scenario"] == "population"}
        for side, jar in jars.items():
            directory = work / side
            directory.mkdir()
            resources = directory / "resources"
            row = {"resources": extract_resources(jar, resources), "passed": False}
            report["sides"][side] = row
            original_path = directory / "original.json"
            run([str(tools["java"]), "-Xint", "-Xmx512m", "--add-opens",
                 "java.base/java.util=ALL-UNNAMED", "-cp", str(classes),
                 "NaturalPopulationObservation", str(jar), side, SEED,
                 str(directory / "original-world"), str(original_path), args.profile],
                work, side + "-original", report["processes"])
            original = read_json(original_path)
            output = directory / "native.json"
            run([str(tools["executable"]), side, str(resources).encode("utf-8").hex(),
                 str(directory / "native-world").encode("utf-8").hex(), str(output), args.profile],
                work, side + "-native", report["processes"])
            native = read_json(output)
            equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                  (work / (side + "-original.stderr")).read_bytes().replace(b"\r\n", b"\n"),
                  "complete original/native bootstrap console")
            equal(native.get("side"), side, "native side")
            if args.profile == "origin":
                cache_projection(native, expected[side])
            compare_original(native, original, expected[side], jar, profile=args.profile)
            row["public_original_cache_match"] = True
            row["passed"] = True
            print("PASS: " + side + " seed-23 natural GAME population (" + args.profile + ")")
        for name, path in paths.items():
            equal({"path": str(path), **CAPTURE.file_fingerprint(path)}, report["inputs"][name],
                  "comparison input preserved: " + name)
        report["inputs_unchanged"] = True
        report["files"] = file_inventory(work)
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
                    report["files"] = file_inventory(work)
                except Exception as error:
                    report["inventory_failure"] = str(error)
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
