#!/usr/bin/env python3
"""Compare complete unpopulated normal generation with both untouched originals."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import os
from pathlib import Path
import platform
import re
import struct
import sys
import types
import uuid


ROOT = Path(__file__).resolve().parents[3]
MAX_BYTES = 32 * 1024 * 1024
SEEDS = (23, 24, 26, 51)
COORDINATES = ((1, 0), (0, 0), (1, 0), (-1, -1), (32768, 0), (2000000, 0),
               (-2000000, 0), (-2147483640, 0), (2147483638, 0))
SCRATCH = {"q": 425, "r": 256, "s": 256, "t": 256, "d": 425, "e": 425,
           "f": 425, "g": 25, "h": 25}
STAGES = (("generator", 0), ("generator", 20), ("constructor", 52), ("generator", 41),
          ("generator", 71), ("generator", 83), ("base", 0), ("base", 637),
          ("generator", 96), ("surface", 0), ("surface", 604), ("generator", 107),
          ("generator", 122), ("finish", 0), ("finish", 221), ("generator", 127),
          ("generator", 129))
CONTROL_KEYS = {"noise82_unchanged", "manager_d_unchanged", "prior_returns_unchanged",
                "fresh_unpublished_returns", "opacity_unchanged"}


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def helpers():
    path = ROOT / "scripts/reference/chunk/compare_finish.py"
    data = path.read_bytes()
    require(len(data) <= 2 * 1024 * 1024, "finishing helper source bound")
    module = types.ModuleType("_normal_finish")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    common, old, executed = module.helpers()
    executed.append({"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return common, old, module, executed


def raw_descriptor(value, kind="double", length=None):
    if value is None:
        require(length is None, "required descriptor missing")
        return
    count = value["length"]
    require(type(count) is int and 0 <= count <= 4096 and (length is None or count == length),
            "complete descriptor length")
    pattern = r"[0-9a-f]{" + str(count * 16) + r"}" if kind == "double" else r"[a-m]{" + str(count) + r"}"
    require(type(value["values"]) is str and re.fullmatch(pattern, value["values"]) is not None,
            "complete raw descriptor values")


def next_longs(seed):
    require(type(seed) is int and 0 <= seed < 1 << 48, "actual 48-bit Random state")
    values = []
    for _ in range(8):
        seed = (seed * 25214903917 + 11) & ((1 << 48) - 1)
        high = seed >> 16
        seed = (seed * 25214903917 + 11) & ((1 << 48) - 1)
        low = seed >> 16
        high = high if high < 1 << 31 else high - (1 << 32)
        low = low if low < 1 << 31 else low - (1 << 32)
        values.append(f"{((high << 32) + low) & ((1 << 64) - 1):016x}")
    return values


def random_projection(value, state):
    require(type(value["serialized_hex"]) is str and
            re.fullmatch(r"(?:[0-9a-f]{2}){1,4096}", value["serialized_hex"]) is not None and
            value["serialized_hex"].endswith(f"{state:016x}78"), "serialized original Random state")
    require(value["next_longs"] == next_longs(state), "actual JDI seed/serialized clone continuation")
    return {"state": f"{state:012x}", "next_longs": value["next_longs"]}


def manager_projection(value):
    for name in ("a", "b", "c"):
        raw_descriptor(value[name])
    raw_descriptor(value["d"], "biome")
    return {name: value[name] for name in ("a", "b", "c", "d")}


def validate_noise(value):
    lengths = dict(zip(("k", "l", "m", "n", "o", "a", "b", "c"), (16, 16, 8, 4, 4, 10, 16, 8)))
    require(type(value) is dict and set(value) == set(lengths), "complete noise82 channels")
    for name, count in lengths.items():
        require(type(value[name]) is list and len(value[name]) == count, "complete noise82 layers")
        for layer in value[name]:
            require(set(layer) == {"offsets", "permutation"} and len(layer["offsets"]) == 3 and
                    all(type(item) is str and re.fullmatch(r"[0-9a-f]{16}", item) for item in layer["offsets"]) and
                    len(layer["permutation"]) == 512 and
                    all(type(item) is int and 0 <= item <= 255 for item in layer["permutation"]),
                    "complete raw noise layer")


def world_projection(world, queue=True):
    return {key: value for key, value in world.items() if key != "presence" and (queue or key != "queue")}


def validate_presence(world, coordinates, finish):
    entries = {entry["key"]: entry["value"] for entry in world["map"]["entries"]}
    for item, offset in zip(world["presence"], ((0, 0), (-1, 0), (1, 0), (0, -1), (0, 1))):
        x, z = (finish.i32(coordinates[n] + offset[n]) for n in (0, 1))
        key = finish.cache_key(x, z)
        require(item == {"coordinates": [x, z], "key": key, "present": key in entries,
                         "contains": key in entries, "value": entries.get(key)}, "actual current presence queries")


def validate_original(value, trace, seed, side, old, finish):
    seed_hex = f"{seed:016x}"
    require(value["schema"] == 1 and value["side"] == side and value["seed_hex"] == seed_hex and
            value["jar_sha256"] == finish.JAR_HASHES[side] and value["platform_parent"] is True,
            "original schema/input/loader provenance")
    required = ({"fd", "lm", "ok", "xa", "rh", "wt", "wi", "eb", "st", "ei", "cl", "bf",
                 "uu", "yy", "wy", "yf", "xv", "kd", "uf", "am", "fv", "sq"} if side == "client" else
                {"dj", "hi", "jb", "os", "kp", "om", "ob", "co", "lv", "ct", "bl", "an",
                 "na", "qa", "dq", "pk", "ph", "gs", "mp", "ab", "dy", "lt"})
    require(required.issubset(value["class_origins"]) and 29 <= len(value["class_origins"]) <= 40 and
            len(set(value["class_origins"].values())) == 1, "required selected original class origins")
    bindings = {"world": "fd", "chunk": "lm", "generator": "yf", "generate": "b", "finish": "c"} if side == "client" else {
        "world": "dj", "chunk": "hi", "generator": "pk", "generate": "b", "finish": "b"}
    require(value["bindings"] == bindings, "exact original method bindings")
    require(set(value["controls"]) == CONTROL_KEYS and all(value["controls"].values()),
            "complete preservation controls")
    require(trace["schema"] == 1 and trace["side"] == side and trace["seed_hex"] == seed_hex and
            trace["jar_sha256"] == finish.JAR_HASHES[side] and trace["listen_address"] == "127.0.0.1" and
            trace["can_get_method_return_values"] is True and trace["can_get_bytecodes"] is True and
            trace["child_exit"] == 0, "trace provenance/capabilities/exit")
    if side == "server":
        require(trace["setup_trace"]["spawn_entries"] == 1 and trace["setup_trace"]["random_walk_iterations"] == 0,
                "qualifying genuine server spawn route")
    require(len(value["calls"]) == len(trace["calls"]) == len(COORDINATES), "all nine generation calls")
    old.validate_world(value["setup_world"], 32768, "setup")
    validate_presence(value["setup_world"], (0, 0), finish)
    cache = world_projection(value["setup_world"], queue=False)
    initial = value["constructor"]
    validate_noise(initial["generator"]["noise"])
    fresh = initial["generator"]
    require(set(fresh["scratch"]) == set(SCRATCH) and fresh["biomes"] is None and fresh["materials"] is None and
            fresh["cave_range"] == 8 and fresh["original_only"] == {"i": [[0] * 32 for _ in range(32)], "w": None},
            "fresh complete original generator constructor")
    for name, length in SCRATCH.items():
        require(fresh["scratch"][name] == ({"length": length, "values": "0" * (length * 16)} if name in "rst" else None),
                "fresh original scratch constructor")
    manager_projection(initial["manager"])
    random_projection(initial["generator"]["generator_random"], trace["constructor_random"]["generator_seed"])
    require(initial["generator"]["cave_random"] == {"omitted": "unconsumed default-seeded state before public cave reseed"},
            "explicit unconsumed initial cave Random scope")
    materials = initial["materials"]
    require(len(materials) == 13 and [item["field"] for item in materials] == list("abcdefghijklm") and
            all(type(item[key]) is int and 0 <= item[key] <= 255 for item in materials for key in ("top", "filler")),
            "complete current material registry")
    previous_world, previous_manager = value["setup_world"], initial["manager"]
    previous_locals = []
    returned_ids = set()
    previous_seeds = trace["constructor_random"]
    stable_identity = None
    previous_generator = fresh
    for number, coordinates in enumerate(COORDINATES):
        row, measured = value["calls"][number], trace["calls"][number]
        require(row["index"] == measured["index"] == number and row["coordinates"] == measured["coordinates"] == list(coordinates),
                "fixed ordered generation corpus")
        require(world_projection(row["before_world"]) == world_projection(previous_world), "inter-call world changed")
        old.validate_world(row["before_world"], 32768, "before")
        validate_presence(row["before_world"], coordinates, finish)
        after, generator = row["after"], row["after"]["generator"]
        old.validate_world(after["world"], 32768, "after")
        old.validate_data(after["local"]["data"], "returned chunk")
        validate_presence(after["world"], coordinates, finish)
        require(world_projection(after["world"], queue=False) == cache and after["materials"] == materials and
                after["opacity_identity_retained"] is True, "cache/material/opacity preservation")
        opacity = old.blob(after["opacity"], 1024, "opacity")
        require(hashlib.sha256(opacity).hexdigest() == old.OPACITY_SHA, "initialized original opacity")
        local = after["local"]
        require(local["coordinates"] == list(coordinates) and
                local["flags"] == {"c": False, "n": False, "o": True, "p": False, "q": False},
                "fresh normal flags/coordinates")
        require(local["time"] == "0000000000000000" and local["map"] == {"class": "java.util.HashMap", "size": 0} and
                local["entity_slices"] == [{"class": "java.util.ArrayList", "size": 0}] * 8,
                "fresh empty local collections/time")
        identity = local["identities"]
        require(len(identity["roles_retained"]) == 19 and all(identity["roles_retained"]) and
                all(identity[key] is True for key in ("roles_pairwise_distinct", "input_retained", "world_retained")),
                "complete returned ownership controls")
        require(row["prior_retained"] == [{"index": n, "local": item} for n, item in enumerate(previous_locals)] and
                after["retained"] == row["prior_retained"] + [{"index": number, "local": local}],
                "all prior complete returned chunks retained")
        previous_locals.append(local)
        require(generator["noise"] == initial["generator"]["noise"] and generator["cave_range"] == 8 and
                generator["original_only"] == initial["generator"]["original_only"], "immutable original generator state")
        require(set(generator["scratch"]) == set(SCRATCH), "complete mutable scratch roles")
        for name, length in SCRATCH.items():
            raw_descriptor(generator["scratch"][name], length=length)
        raw_descriptor(generator["biomes"], "biome", 256)
        require(generator["materials"] == "".join(f"{materials[ord(biome)-97]['top']:02x}{materials[ord(biome)-97]['filler']:02x}"
                                                   for biome in generator["biomes"]["values"]), "actual material handoff")
        manager_projection(after["manager"])
        require(after["manager"]["d"] == initial["manager"]["d"] and
                after["manager"]["identities"]["d"] == initial["manager"]["identities"]["d"], "manager d retained")
        identities = generator["identities"]
        for name in ("generator", "world", "random", "cave", "cave_random", "i", "i_rows", "w", "r", "s", "t"):
            require(identities[name] == fresh["identities"][name], "retained original generator ownership")
        for name in ("q", "d", "e", "f", "g", "h", "v"):
            old.validate_ref(identities[name], "generator " + name)
            require(identities[name]["ref"] is not None and
                    (number == 0 or identities[name] == previous_generator["identities"][name]), "retained original generator buffers")
        require(len({identities[name]["ref"] for name in (*SCRATCH, "v")}) == 10 and
                after["manager"]["identities"]["manager"] == initial["manager"]["identities"]["manager"], "separate generator/manager ownership")
        require(measured["begin_marker"] is True and measured["end_marker"] is True and
                measured["direct_call_count"] == len(measured["records"]) == 1024 and measured["ignored_entries"] == 0 and
                [(item["kind"], item["code_index"]) for item in measured["stages"]] == list(STAGES),
                "complete actual stages and query pairs")
        native_id = measured["identity"]["chunk_object_id"]
        identity = measured["identity"]
        require(type(native_id) is int and native_id > 0 and native_id not in returned_ids and
                all(stage["chunk_object_id"] == (None if n < 2 else native_id) for n, stage in enumerate(measured["stages"])),
                "fresh constructor/finish/return identity")
        current_identity = {key: item for key, item in identity.items() if key != "chunk_object_id"}
        require(identity["thread_name"] == "main" and identity["world_class"] == bindings["world"] and
                identity["generator_class"] == bindings["generator"] and identity["chunk_class"] == bindings["chunk"] and
                identity["world_loader_id"] == identity["generator_loader_id"] and
                (stable_identity is None or current_identity == stable_identity), "stable actual generation context")
        stable_identity = current_identity
        reseeded = ((coordinates[0] * 341873128712 + coordinates[1] * 132897987541) ^ 25214903917) & ((1 << 48) - 1)
        stages = measured["stages"]
        final = stages[-1]
        signatures = {"generator": (bindings["generator"], "b", "(II)L" + bindings["chunk"] + ";"),
                      "constructor": (bindings["chunk"], "<init>", "(L" + bindings["world"] + ";[BII)V"),
                      "base": (bindings["generator"], "a", "(II[B[L" + ("kd" if side == "client" else "gs") + ";[D)V"),
                      "surface": (bindings["generator"], "a", "(II[B[L" + ("kd" if side == "client" else "gs") + ";)V"),
                      "finish": (bindings["chunk"], bindings["finish"], "()V")}
        for n, stage in enumerate(stages):
            require(stage["ordinal"] == n and tuple(stage[key] for key in ("class", "method", "signature")) == signatures[stage["kind"]],
                    "exact observed stage bindings")
            for key in ("generator_seed", "cave_seed"):
                require(type(stage[key]) is int and 0 <= stage[key] < 1 << 48, "actual stage Random state")
            require(stage["generator_seed"] == (previous_seeds["generator_seed"] if n == 0 else reseeded if n < 10 else final["generator_seed"]) and
                    stage["cave_seed"] == (previous_seeds["cave_seed"] if n < 12 else final["cave_seed"]),
                    "complete observed reseed/surface/cave Random transitions")
        returned_ids.add(native_id)
        for marker in ("begin", "end"):
            inventory = measured[marker + "_threads"]
            require(inventory["marker"] == marker and inventory["index"] == number and inventory["suspend_policy"] == 2 and
                    all(inventory[key] is True for key in ("measured_main_thread", "all_threads_suspended", "all_other_threads_daemon", "no_other_original_frames")) and
                    inventory["measured_thread_id"] == measured["identity"]["thread_id"] and
                    inventory["original_loader_id"] == measured["identity"]["world_loader_id"] and
                    1 <= inventory["thread_count"] == len(inventory["threads"]) <= inventory["thread_limit"] <= 64,
                    "finite suspended generation marker inventory")
            measured_threads = []
            thread_ids = set()
            for ordinal, thread in enumerate(inventory["threads"]):
                require(thread["ordinal"] == ordinal and type(thread["id"]) is int and thread["id"] not in thread_ids and
                        thread["suspended"] is True and type(thread["measured"]) is bool and
                        thread["frame_count"] == len(thread["frames"]) <= inventory["frames_per_thread_limit"] <= 64 and
                        thread["original_frame"] is False and
                        (thread["measured"] is True and thread["daemon"] is False and thread["id"] == identity["thread_id"] and thread["name"] == "main" or
                         thread["measured"] is False and thread["daemon"] is True), "complete suspended thread roles")
                thread_ids.add(thread["id"])
                if thread["measured"]:
                    measured_threads.append(thread)
                for frame_number, frame in enumerate(thread["frames"]):
                    require(frame["ordinal"] == frame_number and frame["original_frame"] is False and
                            frame["class_loader_id"] != identity["world_loader_id"], "complete marker frame inventory")
            require(len(measured_threads) == 1, "one measured main thread")
        heights = bytes.fromhex(local["data"]["heightmap"]["values"])
        cx, cz = coordinates
        for ordinal, item in enumerate(measured["records"]):
            x, z, direction = ordinal // 64, ordinal // 4 % 16, ordinal % 4
            qx = finish.i32(cx * 16 + x + (-1, 1, 0, 0)[direction])
            qz = finish.i32(cz * 16 + z + (0, 0, -1, 1)[direction])
            require(item == {"ordinal": ordinal, "x": qx, "z": qz, "local_height": heights[z * 16 + x],
                             "result": finish.world_height(row["before_world"], qx, qz)}, "actual query order/value/cache handoff")
        random_projection(generator["generator_random"], final["generator_seed"])
        random_projection(generator["cave_random"], final["cave_seed"])
        previous_seeds = final
        previous_world, previous_manager = after["world"], after["manager"]
        previous_generator = generator
    return value


def c_projection(value, trace, finish):
    constructor = value["constructor"]
    result = {"schema": 1, "seed_hex": value["seed_hex"], "constructor": {
        "noise": constructor["generator"]["noise"], "manager": manager_projection(constructor["manager"]),
        "generator_random": random_projection(constructor["generator"]["generator_random"], trace["constructor_random"]["generator_seed"])}, "calls": []}
    previous_manager, previous_biomes = constructor["manager"], constructor["generator"]["identities"]["v"]
    for row, measured in zip(value["calls"], trace["calls"]):
        after, stage = row["after"], measured["stages"][-1]
        generator = after["generator"]
        result["calls"].append({"index": row["index"], "coordinates": row["coordinates"], "data": after["local"]["data"],
            "dirty": after["local"]["flags"]["o"], "depth": after["world"]["enqueue_depth"], "queue": finish.queue_regions(after["world"]),
            "trace": [[item[key] for key in ("x", "z", "local_height", "result")] for item in measured["records"]],
            "generator": {"scratch": generator["scratch"], "biomes": generator["biomes"], "materials": generator["materials"],
                "generator_random": random_projection(generator["generator_random"], stage["generator_seed"]),
                "cave_random": random_projection(generator["cave_random"], stage["cave_seed"])},
            "manager": manager_projection(after["manager"]),
            "manager_reused": [after["manager"]["identities"][key] == previous_manager["identities"][key] for key in ("a", "b", "c")],
            "biomes_reused": generator["identities"]["v"] == previous_biomes,
            "context_retained": True, "prior_retained": True, "immutable_retained": True, "suffix_retained": True})
        previous_manager, previous_biomes = after["manager"], generator["identities"]["v"]
    return result


def replay_input(value, finish):
    world = value["setup_world"]
    chunks, mappings, insertions = world["chunks"], world["map"]["entries"], world["insertion_list"]["entries"]
    ids = {chunk["object"]["ref"]: number for number, chunk in enumerate(chunks)}
    regions = finish.queue_regions(world)
    require(len(ids) == len(chunks) <= 512 and len(mappings) <= 512 and len(insertions) <= 512 and
            len(regions) <= 32768 - len(COORDINATES) * 1024, "native replay storage bounds")
    require(all(entry["value"] is not None for entry in mappings + insertions), "nonnull imported cache")
    data = bytearray(b"MCG1")
    data += struct.pack(">q?iIIII", int(value["seed_hex"], 16), world["no_sky"], world["enqueue_depth"],
                        len(chunks), len(mappings), len(insertions), len(regions))
    data += bytes.fromhex(value["calls"][0]["after"]["opacity"]["values"])
    for material in value["constructor"]["materials"]:
        data += bytes((material["top"], material["filler"]))
    for name in ("a", "b", "c", "d"):
        descriptor = value["constructor"]["manager"][name]
        data += struct.pack(">i", -1 if descriptor is None else descriptor["length"])
        if descriptor is not None:
            data += descriptor["values"].encode("ascii") if name == "d" else bytes.fromhex(descriptor["values"])
    for chunk in chunks:
        data += struct.pack(">ii??", *chunk["coordinates"], chunk["empty"], chunk["flags"]["o"])
        data += bytes.fromhex(chunk["heightmap"]["values"])
    for entry in mappings:
        data += struct.pack(">iI", entry["key"], ids[entry["value"]])
    for entry in insertions:
        data += struct.pack(">I", ids[entry["value"]])
    for region in regions:
        data += struct.pack(">Iiiiiii", *region)
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--capture-only", action="store_true")
    parser.add_argument("--tested-commit", default="")
    parser.add_argument("--seed", type=int, action="append", help="Selected qualifying world seeds: 23, 24, 26, 51")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    report, common, output, inputs = None, None, None, {}
    status = 1
    try:
        common, old, finish, executed = helpers()
        require(args.capture_only or args.executable is not None, "C executable required")
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "full lowercase source ID")
        require(not any(os.environ.get(name) for name in finish.JAVA_ENVIRONMENT), "injected Java environment")
        tools = common.runtime_tools(args)
        release = tools["jdk_release"].read_text(encoding="utf-8")
        version = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if version is None or int(version.group(1)) < 17:
            raise common.Unavailable("height tracing requires JDK17 or newer with jdk.jdi")
        if not args.capture_only and not args.executable.is_file():
            raise common.Unavailable("missing C normal generation executable")
        jars = {side: getattr(args, side).resolve() for side in finish.JAR_HASHES}
        for side, jar in jars.items():
            if not jar.is_file():
                raise common.Unavailable("missing supplied " + side + " JAR")
            require(common.file_record(jar, byte_limit=2 * 1024 * 1024)["sha256"] == finish.JAR_HASHES[side], "original JAR hash")
        selected = args.seed if args.seed is not None else list(SEEDS)
        require(selected and len(set(selected)) == len(selected) and all(n in SEEDS for n in selected), "qualifying fixed seed selection")
        sources = [ROOT / "tests/reference/chunk/ChunkObservation.java"] + [
            ROOT / "tests/reference/generation" / name for name in
            ("NormalGenerationObservation.java", "NormalGenerationController.java")]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT / "CMakeLists.txt"] + sources
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [ROOT / "tests/world/generation/normal_probe.c"] + [Path(item["path"]) for item in executed]
        attributed += list(jars.values()) + list(tools.values())
        if not args.capture_only:
            attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024) for path in attributed}
        for item in executed:
            require(inputs[str(Path(item["path"]).resolve())]["sha256"] == item["sha256"], "executed-source attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve()
        report = {"schema": 1, "status": "running", "tested_commit": args.tested_commit or None,
                  "mode": "original-only" if args.capture_only else "three-way", "selected_seeds": selected,
                  "source_label": "Caller label; actual source, helper and executable fingerprints retained.",
                  "started_utc": common.utc_now(), "artifact_root": str(output), "python": sys.version,
                  "platform": platform.platform(), "inputs_before": inputs, "executed_source_modules": executed,
                  "cases": [], "errors": [], "limits": {"controller_seconds": 220, "debuggee_seconds": 180,
                  "heap_mib": 512, "stream_bytes": MAX_BYTES, "trace_per_call": 1024, "calls_per_seed": 9,
                  "policy": "Bounded children and observed inventories, not an OS process-tree or filesystem quota."}}
        common.write_json(output / "manifest.json", report)
        for name in ("source", "classes", "empty", "compile-work"):
            (output / name).mkdir()
        for source in sources:
            (output / "source" / source.name).write_bytes(source.read_bytes())
        common.tool_versions(tools, output, report)

        def run(name, command, directory, seconds=220, world=None, unavailable=False):
            process = common.run_live(command, directory / (name + "-out.txt"), directory / (name + "-err.txt"),
                                      directory, seconds=seconds, stdout_limit=MAX_BYTES, stderr_limit=MAX_BYTES, world_root=world)
            if unavailable and not process.stop_reason and process.returncode == 77:
                report["unavailable_process"] = {"name": name, **asdict(process)}
                raise common.Unavailable(name + ": genuine original setup unavailable; see retained process logs")
            require(not process.stop_reason and process.returncode == 0,
                    name + " failed: " + str(asdict(process)))
            return asdict(process)

        report["jdk_module_process"] = run("jdk-modules", [str(tools["java"]), "--list-modules"], output, 15)
        modules = {line.split("@", 1)[0] for line in (output / "jdk-modules-out.txt").read_text(encoding="utf-8").splitlines()}
        if not {"jdk.jdi", "jdk.jdwp.agent"}.issubset(modules):
            raise common.Unavailable("JDK must contain jdk.jdi and jdk.jdwp.agent")
        report["compile"] = run("compile", [str(tools["javac"]), "-J-Xmx512m", "--release", "17", "--add-modules", "jdk.jdi",
            "-proc:none", "-implicit:none", "-encoding", "UTF-8", "-classpath", str(output / "empty"),
            "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"),
            "-d", str(output / "classes")] + [str(output / "source" / path.name) for path in sources], output / "compile-work", 30)
        classes = sorted((output / "classes").glob("*.class"))
        names = ("ChunkObservation", "NormalGenerationObservation", "NormalGenerationController")
        require(3 <= len(classes) <= 16 and all(any(path.stem == n or path.stem.startswith(n + "$") for n in names) for path in classes),
                "compiled helper inventory")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024) for path in classes}
        for seed in selected:
            directory = output / f"seed-{seed}"
            directory.mkdir()
            row = {"seed": seed, "coordinates": [list(pair) for pair in COORDINATES], "sides": {}, "comparisons": {}}
            report["cases"].append(row)
            observed = {}
            for side, jar in jars.items():
                snapshots, traced = directory / (side + "-snapshots.json"), directory / (side + "-trace.json")
                arguments = [str(jar), side, f"{seed:016x}", str(directory / (side + "-world")), str(snapshots)]
                command = [str(tools["java"]), "-Xmx512m", "--add-modules", "jdk.jdi", "-cp", str(output / "classes"),
                           "NormalGenerationController"] + arguments + [str(traced)]
                side_row = {"trace_process": run(side + "-trace", command, directory, world=Path(arguments[3]), unavailable=True)}
                row["sides"][side] = side_row
                require(snapshots.stat().st_size <= MAX_BYTES and traced.stat().st_size <= MAX_BYTES, "complete record bound")
                value, trace = common.strict_json(snapshots.read_bytes()), common.strict_json(traced.read_bytes())
                validate_original(value, trace, seed, side, old, finish)
                side_row["snapshot_record"] = common.file_record(snapshots, byte_limit=MAX_BYTES)
                side_row["trace_record"] = common.file_record(traced, byte_limit=MAX_BYTES)
                control = directory / (side + "-control.json")
                control_args = arguments.copy()
                control_args[3:5] = [str(directory / (side + "-control-world")), str(control)]
                side_row["control_process"] = run(side + "-control", [str(tools["java"]), "-Xmx512m", "-cp", str(output / "classes"),
                                                                      "NormalGenerationObservation"] + control_args, directory,
                                                               world=Path(control_args[3]), unavailable=True)
                require(control.stat().st_size <= MAX_BYTES and control.read_bytes() == snapshots.read_bytes(),
                        side + ": untouched control differs from traced complete snapshots")
                side_row["control_equal"] = True
                side_row["control_record"] = common.file_record(control, byte_limit=MAX_BYTES)
                for kind, root in (("traced_world", arguments[3]), ("control_world", control_args[3])):
                    inventory = common.world_inventory(Path(root), hashes=True)
                    require(inventory["complete"], "complete world inventory required")
                    side_row[kind] = inventory
                expected = c_projection(value, trace, finish)
                observed[side] = expected
                if not args.capture_only:
                    replay = directory / (side + "-input.bin")
                    replay.write_bytes(replay_input(value, finish))
                    side_row["input_record"] = common.file_record(replay, byte_limit=MAX_BYTES)
                    side_row["c_process"] = run(side + "-c", [str(args.executable.resolve()), str(replay)], directory, 45)
                    actual = common.strict_json((directory / (side + "-c-out.txt")).read_bytes())
                    difference = common.first_difference(expected, actual)
                    require(difference is None, f"C/{side} seed {seed}: {difference}")
                    row["comparisons"]["C/" + side] = "pass"
            require(observed["client"] == observed["server"], "client/server complete generation results differ")
            row["comparisons"]["client/server"] = "pass"
            common.write_json(output / "manifest.json", report)
        report["status"], status = "pass", 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable)
        status = 77 if unavailable else 1
        if report is not None:
            report["status"] = "unavailable" if unavailable else "fail"
            report["errors"].append(str(error))
        print(("UNAVAILABLE: " if unavailable else "FAILURE: ") + str(error), file=sys.stderr)
    finally:
        if report is not None:
            report["inputs_after"] = {}
            for name, before in inputs.items():
                try:
                    after = common.file_record(Path(name), byte_limit=before["bytes"])
                    report["inputs_after"][name] = after
                    require(after == before, "input changed: " + name)
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            if "classes_before" in report:
                try:
                    after = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                             for path in sorted((output / "classes").glob("*.class"))}
                    report["classes_after"] = after
                    require(after == report["classes_before"], "compiled helpers changed")
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            report["finished_utc"] = common.utc_now()
            common.write_json(output / "manifest.json", report)
            print(report["status"].upper() + ": " + str(output / "manifest.json"))
    return status


if __name__ == "__main__":
    sys.exit(main())
