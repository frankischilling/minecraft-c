#!/usr/bin/env python3
"""Compare synchronous cache acquisition with genuine retained original providers."""

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
MAX_C_BYTES = 64 * 1024 * 1024
SEED = "0000000000000017"
CASES = (("client", "persistent-client", "shared"),
         ("server", "dedicated-constructor", "shared"),
         ("server", "dedicated-constructor", "dedicated-gates"))
BT_RUN = "140005b8000ba7fffa4ca7fff6"


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def same_json(actual, expected):
    """Compare JSON values without Python's bool/int/float coercion."""
    if type(actual) is not type(expected):
        return False
    if type(actual) is dict:
        return actual.keys() == expected.keys() and all(
            type(key) is str and same_json(value, expected[key]) for key, value in actual.items())
    if type(actual) is list:
        return len(actual) == len(expected) and all(
            same_json(left, right) for left, right in zip(actual, expected))
    return type(actual) in (str, int, float, bool, type(None)) and actual == expected


def keys(value, expected, context):
    require(type(value) is dict and set(value) == set(expected), context + ": complete keys")


def integer(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ": integer bounds")


def raw_hex(value, count, context):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]{" + str(count * 2) + "}", value),
            context + ": complete raw bytes")
    return bytes.fromhex(value)


def descriptor(value, count, context):
    keys(value, ("length", "values"), context)
    require(type(value["length"]) is int and value["length"] == count, context + ": exact length")
    return raw_hex(value["values"], count, context)


def helpers():
    path = ROOT / "scripts/reference/generation/compare_normal.py"
    require(Path(__file__).resolve() == (ROOT / "scripts/reference/cache/compare_acquire.py").resolve(),
            "canonical executing source identity")
    data = path.read_bytes()
    require(len(data) <= 2 * 1024 * 1024, "generation helper source bound")
    module = types.ModuleType("_acquire_normal")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    common, old, finish, executed = module.helpers()
    executed.append({"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return common, old, finish, module, executed


def plan(scenario):
    if scenario == "shared":
        return (("c", 1, 0), ("c", 0, 0), ("b", 0, 0),
                ("c", 32768, 0), ("c", -1, -1), ("c", 1, 0))
    require(scenario == "dedicated-gates", "fixed acquisition scenario")
    return (("b", 9, 0), ("d", 9, 0), ("b", 9, 0), ("c", 9, 0),
            ("d", 9, 0), ("b", 9, 0), ("c", 9, 0), ("c", 0, 0))


def ref(value):
    if value is None:
        return None
    keys(value, ("ref", "class"), "original identity")
    if value == {"ref": None, "class": None}:
        return None
    require(type(value["ref"]) is str and re.fullmatch(r"r[0-9]{1,8}", value["ref"]) and
            type(value["class"]) is str and 1 <= len(value["class"]) <= 256, "typed original identity")
    return value["ref"]


def flags(value):
    keys(value, ("c", "n", "o", "p", "q"), "chunk flags")
    require(all(type(item) is bool for item in value.values()), "actual boolean chunk flags")
    return value


def local_data(value):
    lengths = {"blocks": 32768, "metadata": 16384, "skylight": 16384,
               "block_light": 16384, "heightmap": 256}
    keys(value, (*lengths, "minimum_height"), "complete chunk data")
    for name, length in lengths.items():
        descriptor(value[name], length, name)
    integer(value["minimum_height"], -(1 << 31), (1 << 31) - 1, "minimum height")
    return value


def original_random(value, normal):
    keys(value, ("serialized_hex", "next_longs"), "original Random")
    encoded = value["serialized_hex"]
    require(type(encoded) is str and re.fullmatch(r"(?:[0-9a-f]{2}){9,4096}", encoded) and
            encoded.endswith("78"), "bounded serialized original Random")
    state = int(encoded[-18:-2], 16)
    integer(state, 0, (1 << 48) - 1, "serialized 48-bit seed")
    return normal.random_projection(value, state)


def generator_projection(value, normal):
    keys(value, ("noise", "scratch", "biomes", "materials", "generator_random", "cave_random",
                 "cave_range", "original_only", "identities"), "complete retained generator")
    normal.validate_noise(value["noise"])
    keys(value["scratch"], normal.SCRATCH, "complete retained scratch")
    for name, length in normal.SCRATCH.items():
        keys(value["scratch"][name], ("length", "values"), "scratch descriptor")
        normal.raw_descriptor(value["scratch"][name], length=length)
    keys(value["biomes"], ("length", "values"), "retained biomes")
    normal.raw_descriptor(value["biomes"], "biome", 256)
    raw_hex(value["materials"], 512, "selected current biome materials")
    require(type(value["cave_range"]) is int and value["cave_range"] == 8, "ordinary cave range")
    keys(value["original_only"], ("i", "w"), "unused original state")
    require(value["original_only"] == {"i": [[0] * 32 for _ in range(32)], "w": None},
            "unused matrix and nullable field retained")
    identity = value["identities"]
    keys(identity, (*normal.SCRATCH, "v", "i", "i_rows", "w", "generator", "world", "random", "cave", "cave_random"),
         "complete retained generator identities")
    require(type(identity["i_rows"]) is list and len(identity["i_rows"]) == 32 and
            len({ref(row) for row in identity["i_rows"]}) == 32 and
            all(row["class"] == "[I" for row in identity["i_rows"]), "distinct retained matrix rows")
    for name, item in identity.items():
        if name != "i_rows":
            require(ref(item) is not None or name == "w", "nonnull live generator identity")
    return {key: value[key] for key in ("noise", "scratch", "biomes", "materials")} | {
        name: original_random(value[name], normal) for name in ("generator_random", "cave_random")}


def chunk_map(snapshot):
    world = snapshot["world"]
    require(type(world["chunks"]) is list and 1 <= len(world["chunks"]) <= 32, "complete bounded chunk inventory")
    result = {ref(chunk["object"]): chunk for chunk in world["chunks"]}
    require(len(result) == len(world["chunks"]), "unique complete chunk identities")
    return result


def world_projection(snapshot, ids, finish):
    world = snapshot["world"]
    chunks = chunk_map(snapshot)
    require(set(chunks) == set(ids), "all imported and generated identities retained")
    full = {ref(chunk["object"]): chunk for chunk in snapshot["cached_locals"]}
    require(len(full) == len(snapshot["cached_locals"]), "unique complete ordinary local state")
    projected = []
    for identity, number in sorted(ids.items(), key=lambda item: item[1]):
        chunk = chunks[identity]
        require(type(chunk["empty"]) is bool, "actual blank dispatch")
        flags(chunk["flags"])
        raw_hex(chunk["time"], 8, "last-save time")
        descriptor(chunk["heightmap"], 256, "cached heightmap")
        coordinates = chunk["coordinates"]
        require(type(coordinates) is list and len(coordinates) == 2, "chunk coordinate pair")
        for coordinate in coordinates:
            integer(coordinate, -(1 << 31), (1 << 31) - 1, "chunk coordinate")
        data = None
        if not chunk["empty"]:
            require(identity in full, "ordinary cached object has all five arrays")
            local = full.pop(identity)
            require(local["coordinates"] == coordinates and local["flags"] == chunk["flags"] and
                    local["time"] == chunk["time"] and ref(local["world"]) == ref(world["world"]),
                    "ordinary local/cache identity and flags agree")
            require(local["array_references"] == chunk["array_references"] and len(local["array_references"]) == 8 and
                    len({ref(item) for item in local["array_references"]}) == 8, "complete distinct ordinary array identities")
            data = local_data(local["data"])
            require(data["heightmap"] == chunk["heightmap"] and data["minimum_height"] == chunk["minimum_height"],
                    "cached and complete local heights agree")
            for name in ("blocks", "metadata", "skylight", "block_light"):
                require(chunk[name] == {"length": data[name]["length"],
                    "sha256": hashlib.sha256(bytes.fromhex(data[name]["values"])).hexdigest()},
                    "cached and complete local fingerprint agree")
            require(local["tile_map"]["size"] == 0 and len(local["entity_slices"]) == 8 and
                    all(slice_["size"] == 0 for slice_ in local["entity_slices"]), "empty registration collections")
        projected.append({"id": number, "coordinates": coordinates, "blank": chunk["empty"],
                          "flags": chunk["flags"], "time": chunk["time"], "data": data})
    require(not full, "no omitted full local objects")
    mappings = world["map"]["entries"]
    require(type(mappings) is list and len(mappings) <= 64 and
            len({entry["key"] for entry in mappings}) == len(mappings), "unique bounded map entries")
    for entry in mappings:
        integer(entry["key"], -(1 << 31), (1 << 31) - 1, "signed packed key")
        require(entry["value"] in ids, "nonnull known mapped identity")
    pending = [entry["key"] for entry in world["pending_set"]["entries"]]
    require(len(pending) <= 64 and len(set(pending)) == len(pending), "unique bounded pending membership")
    for key in pending:
        integer(key, -(1 << 31), (1 << 31) - 1, "signed pending key")
    insertions = world["insertion_list"]["entries"]
    require(len(insertions) <= 64 and all(entry["value"] in ids for entry in insertions), "known ordered insertions")
    queue = finish.queue_regions(world)
    require(len(queue) <= 32768, "complete queue bound")
    for row in queue:
        require(type(row) is list and len(row) == 7 and type(row[0]) is int and row[0] in (0, 1), "queued kind/bounds")
        for coordinate in row[1:]:
            integer(coordinate, -(1 << 31), (1 << 31) - 1, "queued bound")
    integer(world["enqueue_depth"], -(1 << 31), (1 << 31) - 1, "shared enqueue depth")
    return {"map": sorted([[entry["key"], ids[entry["value"]]] for entry in mappings]),
            "insertions": [ids[entry["value"]] for entry in insertions], "pending": sorted(pending),
            "chunks": projected, "queue": queue, "depth": world["enqueue_depth"]}


def snapshot_projection(value, ids, finish, normal):
    keys(value, ("world", "cached_locals", "generator", "manager", "materials", "world_time",
                 "world_force_load", "provider_force_load", "opacity", "opacity_identity_retained",
                 "opacity_values_retained"), "complete acquisition snapshot")
    descriptor(value["opacity"], 1024, "actual opacity registry")
    require(value["opacity_identity_retained"] is True and value["opacity_values_retained"] is True,
            "original opacity identity and values retained")
    raw_hex(value["world_time"], 8, "world time")
    require(value["world_force_load"] is False and
            value["provider_force_load"] is (None if value["world"]["world"]["class"] == "fd" else False),
            "actual default force flags in the fixed constructor corpus")
    require(type(value["materials"]) is list and len(value["materials"]) == 13, "all13 actual material pairs")
    for number, material in enumerate(value["materials"]):
        keys(material, ("field", "top", "filler"), "current registered biome material")
        require(material["field"] == chr(ord("a") + number), "ordered current biome identities")
        integer(material["top"], 0, 255, "current top byte")
        integer(material["filler"], 0, 255, "current filler byte")
    keys(value["manager"], ("a", "b", "c", "d", "identities"), "complete manager snapshot")
    keys(value["manager"]["identities"], ("a", "b", "c", "d", "manager"), "complete manager identities")
    for name, identity in value["manager"]["identities"].items():
        require(ref(identity) is not None or name == "d" and value["manager"]["d"] is None,
                "live manager buffer/owner identity")
    return {"world": world_projection(value, ids, finish),
            "generator": generator_projection(value["generator"], normal),
            "manager": normal.manager_projection(value["manager"])}


def identities(value):
    world = value["initial"]["world"]
    chunks = chunk_map(value["initial"])
    fallback = world["fallback"]
    require(fallback in chunks and chunks[fallback]["empty"] is True, "retained blank fallback")
    result = {fallback: 0}
    for identity in chunks:
        if identity not in result:
            result[identity] = len(result)
    require(len(result) <= 16, "imported chunk bound")
    return result


def signed_time(value):
    raw = int.from_bytes(raw_hex(value, 8, "time bits"), "big")
    return raw if raw < 1 << 63 else raw - (1 << 64)


def replay_input(value, finish):
    snapshot = value["initial"]
    ids = identities(value)
    world = world_projection(snapshot, ids, finish)
    dedicated = value["side"] == "server"
    spawn = value["setup_metadata"]["spawn"]
    require(spawn == [0, 64, 0], "qualifying original spawn")
    data = bytearray(b"MCA1")
    data += struct.pack(">q???iiqi?IIIIII", int(value["seed_hex"], 16), dedicated,
        snapshot["world_force_load"] if dedicated else False,
        snapshot["provider_force_load"] if dedicated else False, spawn[0], spawn[2],
        signed_time(snapshot["world_time"]), world["depth"], snapshot["world"]["no_sky"],
        len(world["chunks"]), len(world["map"]), len(world["insertions"]), len(world["pending"]),
        len(world["queue"]), len(value["operations"]))
    data += descriptor(snapshot["opacity"], 1024, "actual opacity registry")
    require(len(snapshot["materials"]) == 13, "all13 actual biome materials")
    for number, material in enumerate(snapshot["materials"]):
        require(material["field"] == chr(ord("a") + number), "ordered registered biome identities")
        for name in ("top", "filler"):
            integer(material[name], 0, 255, "actual material byte")
        data += bytes((material["top"], material["filler"]))
    for name in ("a", "b", "c", "d"):
        buffer = snapshot["manager"][name]
        data += struct.pack(">i", -1 if buffer is None else buffer["length"])
        if buffer is not None:
            data += buffer["values"].encode("ascii") if name == "d" else bytes.fromhex(buffer["values"])
    originals = chunk_map(snapshot)
    for chunk in world["chunks"]:
        flags_ = chunk["flags"]
        original = originals[next(identity for identity, number in ids.items() if number == chunk["id"])]
        data += struct.pack(">ii??????qi", *chunk["coordinates"], chunk["blank"], flags_["o"], flags_["c"],
                            flags_["n"], flags_["p"], flags_["q"], signed_time(chunk["time"]), original["minimum_height"])
        data += bytes.fromhex(original["heightmap"]["values"])
        if not chunk["blank"]:
            for name in ("blocks", "metadata", "skylight", "block_light"):
                data += bytes.fromhex(chunk["data"][name]["values"])
    for key, identity in world["map"]:
        data += struct.pack(">iI", key, identity)
    for identity in world["insertions"]:
        data += struct.pack(">I", identity)
    for key in world["pending"]:
        data += struct.pack(">i", key)
    for region in world["queue"]:
        data += struct.pack(">Iiiiiii", *region)
    for operation in value["operations"]:
        data += struct.pack(">Iii", {"b": 0, "c": 1, "d": 2}[operation["method"]], *operation["coordinates"])
    require(len(data) <= 4 * 1024 * 1024, "complete replay input bound")
    return data


def validate_inventory(value, index, marker, side, operation):
    require(value["marker"] == marker and type(value["index"]) is int and value["index"] == index and
            value["suspend_policy"] == 2 and value["measured_thread_id"] == operation["thread_id"] and
            all(value[name] is True for name in ("measured_main_thread", "all_threads_suspended",
                "all_other_threads_daemon", "no_unallowed_original_frames")), "complete suspended main inventory")
    integer(value["thread_count"], 1, 64, "finite inventoried threads")
    require(value["thread_count"] == len(value["threads"]) and value["thread_limit"] == 64 and
            value["frames_per_thread_limit"] == 64, "complete finite inventory bounds")
    require(type(value["exact_inert_bt_count"]) is int and value["exact_inert_bt_count"] == (side == "server") and
            value["bt_run_hex"] == (BT_RUN if side == "server" else None), "scope-specific inert daemon policy")
    main_count = inert_count = 0
    for ordinal, thread in enumerate(value["threads"]):
        require(type(thread["ordinal"]) is int and thread["ordinal"] == ordinal and thread["suspended"] is True and
                type(thread["daemon"]) is bool and type(thread["measured"]) is bool and
                type(thread["exact_inert_bt"]) is bool and type(thread["original_frame"]) is bool,
                "typed inventoried thread")
        integer(thread["frame_count"], 0, 64, "finite complete stack")
        require(thread["frame_count"] == len(thread["frames"]), "no omitted stack frames")
        actual_original = False
        for frame_number, frame in enumerate(thread["frames"]):
            require(type(frame["ordinal"]) is int and frame["ordinal"] == frame_number and
                    type(frame["original_frame"]) is bool and
                    frame["original_frame"] == (frame["class_loader_id"] == value["original_loader_id"]),
                    "actual frame origin classification")
            actual_original |= frame["original_frame"]
        require(thread["original_frame"] == actual_original, "complete thread origin classification")
        if thread["measured"]:
            main_count += 1
            require(thread["id"] == operation["thread_id"] and thread["name"] == "main" and
                    thread["daemon"] is False and thread["exact_inert_bt"] is False, "actual measured main")
        elif thread["exact_inert_bt"]:
            inert_count += 1
            frames = thread["frames"]
            require(side == "server" and thread["daemon"] is True and thread["object_class"] == "bt" and
                    2 <= len(frames) <= 3 and frames[-1]["class"] == "bt" and frames[-1]["method"] == "run" and
                    frames[-1]["signature"] == "()V" and frames[-1]["code_index"] == 3 and frames[-1]["original_frame"] is True,
                    "exact dedicated sleeping daemon")
            require(all(frame["class"] == "java.lang.Thread" and frame["method"] in ("sleep", "sleep0") and
                        frame["signature"] == "(J)V" and frame["class_loader_id"] is None and
                        frame["original_frame"] is False for frame in frames[:-1]), "exact bootstrap sleep chain")
        else:
            require(thread["daemon"] is True and actual_original is False, "all other threads daemon and unrelated")
    require(main_count == 1 and inert_count == (side == "server") and
            value["no_other_original_frames"] is (side == "client"), "complete scope-specific inventory roles")


def trace_links(trace, snapshots):
    rows = trace["object_references"]
    require(type(rows) is list and 1 <= len(rows) <= 1024, "bounded explicit object links")
    links = {}
    for row in rows:
        keys(row, ("reference", "object_id", "class", "loader_id"), "object link")
        require(type(row["reference"]) is str and re.fullmatch(r"r[0-9]{1,8}", row["reference"]), "helper reference label")
        integer(row["object_id"], 1, (1 << 63) - 1, "JDI object identity")
        require(row["reference"] not in links and row["object_id"] not in [item["object_id"] for item in links.values()],
                "one-to-one complete object link")
        links[row["reference"]] = row
    for snapshot in snapshots:
        for chunk in snapshot["world"]["chunks"]:
            identity = ref(chunk["object"])
            require(identity in links and links[identity]["class"] == chunk["object"]["class"], "explicit chunk helper/JDI link")
    return links


def trace_cache(value, links, ids):
    by_jdi = {row["object_id"]: ids[name] for name, row in links.items() if name in ids}
    def chunk(value_):
        require(type(value_) is dict and value_["id"] in by_jdi, "known actual trace chunk")
        flags(value_["flags"])
        raw_hex(value_["time"], 8, "traced chunk time")
        return by_jdi[value_["id"]]
    mappings = [[row["key"], chunk(row["value"])] for row in value["entries"]]
    require(len(mappings) <= 64 and len({row[0] for row in mappings}) == len(mappings), "complete unique traced map")
    return {"map": sorted(mappings), "insertions": [chunk(row) for row in value["insertions"]],
            "pending": sorted(value["pending"]), "queue": [row["region"] for row in value["queue"]]}


def observed_callbacks(operation, ids, links, coordinates):
    result = []
    number = 0
    by_jdi = {row["object_id"]: ids[name] for name, row in links.items() if name in ids}
    selected = {"load_wrapper": "load", "generate": "generate_begin", "generate_return": "generate_end",
                "register_tiles": "tiles", "register_entities": "entities"}
    for event in operation["events"]:
        if event["kind"] not in selected:
            continue
        kind = selected[event["kind"]]
        chunk = event["result"] if kind == "generate_end" else event["chunk"] if kind in ("tiles", "entities") else None
        require(chunk is None or chunk["id"] in by_jdi, "actual callback chunk link")
        cache = trace_cache(event["cache"], links, ids)
        x, z = coordinates if chunk is None else chunk["coordinates"]
        result.append({"kind": kind, "x": x, "z": z, "id": None if chunk is None else by_jdi[chunk["id"]],
            "slice": number if kind == "entities" else None, "map_count": len(cache["map"]),
            "list_count": len(cache["insertions"]), "pending_count": len(cache["pending"]),
            "active": None if chunk is None else chunk["flags"]["c"],
            "dirty": None if chunk is None else chunk["flags"]["o"],
            "populated": None if chunk is None else chunk["flags"]["n"],
            "last_save_time": None if chunk is None else chunk["time"]})
        number += kind == "entities"
    require(number in (0, 8), "all eight registration slices")
    return result


def retained_snapshot(value, initial):
    for name in ("materials", "world_time", "world_force_load", "provider_force_load", "opacity"):
        require(value[name] == initial[name], "stable original world policy/resources: " + name)
    for name in ("generator", "manager"):
        require(value[name]["identities"] == initial[name]["identities"], "stable warm original " + name + " ownership")
    for name in ("world", "provider", "storage", "cache", "provider_world", "manager", "cache_world", "generator", "loader",
                 "no_sky", "enqueue_depth", "drain_depth", "chunk_static_a"):
        require(value["world"][name] == initial["world"][name], "stable original world relationship: " + name)
    for name in ("map", "insertion_list", "pending_set", "queue"):
        require(value["world"][name]["object"] == initial["world"][name]["object"], "stable collection owner: " + name)


def lifecycle(measured, row, before, after, ids, links, side, scenario, index):
    events, method = measured["events"], row["method"]
    x, z = row["coordinates"]
    generated = len(measured["finishes"]) == 1
    get_return = 33 if side == "client" else 55
    if method == "d":
        schedule = [("pending", 0), ("pending", 71), ("pending", 89)]
    elif method == "b":
        denied = ref(row["result"]) == row["before"]["world"]["fallback"]
        schedule = [("get", 0), ("get_return", 53 if denied else get_return)]
    elif not generated:
        schedule = [("acquire", 0), ("acquire", 20), ("acquire", 391), ("acquire_return", 393)]
    else:
        schedule = [("acquire", 0), ("acquire", 20), ("acquire", 46), ("load_wrapper", 0), ("loader", 0),
            ("loader_return", 27), ("load_wrapper_return", 41), ("acquire", 78), ("generate", 0),
            ("finish", 0), ("finish", 221), ("generator_ready", 129), ("generate_return", 129),
            ("acquire", 85), ("acquire", 100), ("acquire", 112), ("activate", 0), ("activate", 5),
            ("register_tiles", 0), ("register_tiles_return", 32)]
        schedule += [(kind, offset) for _ in range(8) for kind, offset in (("register_entities", 0), ("register_entities_return", 43))]
        schedule += [("activate", 51), ("acquire", 128)]
        if scenario == "shared" and index == 0:
            schedule += [("get", 0), ("get_return", get_return)]
        schedule += [("acquire", 391), ("acquire_return", 393)]
    require([(event["kind"], event["code_index"]) for event in events] == schedule,
            "complete exact finite lifecycle entry/return schedule")
    chunk_class, cache_class, world_class, generator_class, loader_class = (
        ("lm", "ok", "fd", "yf", "ld") if side == "client" else ("hi", "he", "dj", "pk", "hb"))
    chunk_signature = "(II)L" + chunk_class + ";"
    world_id, cache_id, generator_id = measured["world_id"], measured["cache_id"], measured["generator_id"]
    loader_id = links[ref(row["before"]["world"]["loader"])]["object_id"]
    returned = ref(row["result"])
    returned_id = None if returned is None else links[returned]["object_id"]
    def frame(owner, name, signature, offset, receiver):
        return {"class": owner, "method": name, "signature": signature, "code_index": offset, "receiver": receiver}
    cache_frame = lambda offset: frame(cache_class, "c", chunk_signature, offset, cache_id)
    wrapper_frame = lambda offset: frame(cache_class, "d" if side == "client" else "e", chunk_signature, offset, cache_id)
    generator_frame = lambda offset: frame(generator_class, "b", chunk_signature, offset, generator_id)
    activation_frame = lambda offset: frame(chunk_class, "e" if side == "client" else "d", "()V", offset, returned_id)
    before_graph = {name: before["world"][name] for name in ("map", "insertions", "pending", "queue")}
    after_graph = {name: after["world"][name] for name in before_graph}
    phase, collection_arguments, slice_number, active = dict(before_graph), {}, 0, False
    by_id = {number: name for name, number in ids.items()}
    by_jdi = {item["object_id"]: ids[name] for name, item in links.items() if name in ids}
    owners = {"cache_id": "cache", "map_id": "map", "list_id": "insertion_list",
              "pending_id": "pending_set", "queue_id": "queue"}
    def traced_chunk(value):
        keys(value, ("id", "class", "coordinates", "flags", "time"), "complete traced chunk")
        require(value["id"] in by_jdi, "known traced state")
        number = by_jdi[value["id"]]
        expected = after["world"]["chunks"][number]
        expected_flags = dict(expected["flags"])
        if generated and value["id"] == returned_id:
            expected_flags["c"] = active
        require(value["class"] == links[by_id[number]]["class"] and value["coordinates"] == expected["coordinates"] and
                value["flags"] == expected_flags and value["time"] == expected["time"], "traced cached state and virtual identity")
    for ordinal, event in enumerate(events):
        keys(event, ("ordinal", "kind", "class", "method", "signature", "code_index", "thread_id", "receiver", "arguments",
                     "result", "generated_local", "chunk", "cache", "original_frames"), "complete lifecycle event")
        kind, offset = event["kind"], event["code_index"]
        require(type(event["ordinal"]) is int and event["ordinal"] == ordinal and event["thread_id"] == measured["thread_id"],
                "ordered same-thread lifecycle event")
        if kind in ("acquire", "acquire_return"):
            expected_frame, callers, arguments = cache_frame(offset), [], [x, z]
        elif kind in ("get", "get_return"):
            expected_frame = frame(cache_class, "b", chunk_signature, offset, cache_id)
            callers, arguments = ([cache_frame(194)], [0, 0]) if generated else ([], [x, z])
        elif kind == "pending":
            expected_frame, callers, arguments = frame(cache_class, "d", "(II)V", offset, cache_id), [], [x, z]
        elif kind.startswith("load_wrapper"):
            expected_frame, callers, arguments = wrapper_frame(offset), [cache_frame(46)], [x, z]
        elif kind in ("loader", "loader_return"):
            expected_frame = frame(loader_class, "a", "(L" + world_class + ";II)L" + chunk_class + ";", offset, loader_id)
            callers, arguments = [wrapper_frame(19), cache_frame(46)], [{"id": world_id, "class": "fd" if side == "client" else "dp"}, x, z]
        elif kind in ("generate", "generator_ready", "generate_return"):
            expected_frame, callers, arguments = generator_frame(offset), [cache_frame(78)], [x, z]
        elif kind == "finish":
            expected_frame = frame(chunk_class, "c" if side == "client" else "b", "()V", offset, returned_id)
            callers, arguments = [generator_frame(124), cache_frame(78)], []
        elif kind == "activate":
            expected_frame, callers, arguments = activation_frame(offset), [cache_frame(125)], []
        else:
            entities = kind.startswith("register_entities")
            require(entities or kind.startswith("register_tiles"), "known registration event")
            expected_frame = frame(world_class, "a", "(Ljava/util/List;)V" if entities else "(Ljava/util/Collection;)V", offset, world_id)
            callers = [activation_frame(42 if entities else 18), cache_frame(125)]
            if entities:
                local = next(chunk for chunk in row["after"]["cached_locals"] if ref(chunk["object"]) == returned)
                identity = ref(local["entity_slices"][slice_number]["object"])
                arguments = [{"id": links[identity]["object_id"], "class": "java.util.ArrayList"}]
                slice_number += kind.endswith("_return")
            else:
                require(len(event["arguments"]) == 1 and type(event["arguments"][0]) is dict and
                        event["arguments"][0]["class"] == "java.util.HashMap$Values", "actual empty tile values collection")
                integer(event["arguments"][0]["id"], 1, (1 << 63) - 1, "actual tile values object")
                collection_arguments.setdefault("tiles", event["arguments"])
                arguments = collection_arguments["tiles"]
        require({key: event[key] for key in expected_frame} == expected_frame and event["arguments"] == arguments and
                event["original_frames"] == [expected_frame, *callers], "exact original event binding, receiver, arguments and caller frames")
        if kind == "acquire" and offset == 20:
            phase["pending"] = after_graph["pending"]
        if generated and kind == "finish" and offset == 221:
            phase["queue"] = after_graph["queue"]
        if generated and kind == "acquire" and offset == 100:
            phase["map"] = after_graph["map"]
        if generated and kind == "acquire" and offset == 112:
            phase["insertions"] = after_graph["insertions"]
        if kind == "pending" and offset == 89:
            phase["pending"] = after_graph["pending"]
        if kind == "activate" and offset == 5:
            active = True
        require(trace_cache(event["cache"], links, ids) == phase, "complete exact intermediate map/list/pending/queue contents")
        require(event["cache"]["cache_id"] == cache_id and event["cache"]["world_force_load"] == row["before"]["world_force_load"] and
                event["cache"]["provider_force_load"] == row["before"]["provider_force_load"], "live traced cache and policy identity")
        for key, role in owners.items():
            identity = row["before"]["world"][role]
            if role != "cache":
                identity = identity["object"]
            require(event["cache"][key] == links[ref(identity)]["object_id"], "exact traced collection owner identity")
        for item in [value["value"] for value in event["cache"]["entries"]] + event["cache"]["insertions"] + [event["cache"]["fallback"]]:
            traced_chunk(item)
        if kind in ("loader_return", "load_wrapper_return"):
            require(event["result"] is None, "actual load wrapper and disk loader returned null")
        elif kind in ("acquire_return", "generate_return", "get_return"):
            expected_result = returned_id
            if kind == "get_return" and generated:
                expected_result = links[next(name for name, number in ids.items() if number == before["world"]["map"][0][1])]["object_id"]
            require(event["result"] is not None and event["result"]["id"] == expected_result, "paired actual original return value")
        else:
            require(event["result"] is None, "no invented value at non-return event")
        require(event["generated_local"] is not None if kind == "generate_return" else event["generated_local"] is None,
                "complete generated-return local data only at measured return")


def validate_original(value, trace, side, mode, scenario, jar, normal, finish):
    keys(value, ("schema", "side", "mode", "scenario", "seed_hex", "jar_sha256", "platform_parent",
                 "class_origins", "bindings", "setup_metadata", "initial", "operations"), "original acquisition document")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["side"] == side and value["mode"] == mode and
            value["scenario"] == scenario and value["seed_hex"] == SEED and value["jar_sha256"] == finish.JAR_HASHES[side] and
            value["platform_parent"] is True, "actual original acquisition provenance")
    bindings = dict(zip(("world", "cache", "storage", "loader", "generator"),
                       ("fd", "ok", "mx", "ld", "yf") if side == "client" else ("dp", "he", "ie", "hb", "pk")))
    require(value["bindings"] == bindings, "actual persistent/dedicated constructor route")
    origins = value["class_origins"]
    require(type(origins) is dict and 20 <= len(origins) <= 128 and set(bindings.values()) <= set(origins) and
            all(type(name) is str and type(url) is str and url == jar.resolve().as_uri().replace("file:///", "file:/", 1)
                for name, url in origins.items()), "all original classes from exact selected archive")
    setup = value["setup_metadata"]
    require(setup["spawn"] == [0, 64, 0] and setup["constructor_map_count"] == 1 and
            setup["constructor_insertion_count"] == 1 and setup["constructor_pending_count"] == 0 and
            setup["constructor_queue_count"] == 0 and setup["constructor_origin_retained"] is True and
            setup["client_warmup"] is False and setup["constructor_origin"] == setup["baseline_origin"],
            "genuine qualifying constructor-cached origin")
    keys(trace, ("schema", "side", "mode", "scenario", "seed_hex", "jar_sha256", "can_get_method_return_values", "can_get_bytecodes",
                 "vm_name", "vm_version", "child_command", "debuggee_mode", "listen_address", "port", "setup_proof", "object_references",
                 "operations", "jdi_event_count", "child_exit", "child_stdout", "child_stderr", "elapsed_ms"), "complete acquisition trace")
    require(type(trace["schema"]) is int and trace["schema"] == 1 and
            all(trace[name] == value[name] for name in ("side", "mode", "scenario", "seed_hex", "jar_sha256")) and
            trace["can_get_method_return_values"] is True and trace["can_get_bytecodes"] is True and
            type(trace["child_exit"]) is int and trace["child_exit"] == 0 and trace["listen_address"] == "127.0.0.1" and
            trace["debuggee_mode"] == "interpreted", "actual bounded loopback JDI run")
    integer(trace["port"], 1, 65535, "ephemeral loopback port")
    integer(trace["elapsed_ms"], 1, 90000, "bounded controller elapsed time")
    integer(trace["jdi_event_count"], 1, 65536, "bounded total JDI event delivery")
    proof = trace["setup_proof"]
    require(proof == {"spawn_entries": 0, "random_steps": 0, "same_world": None, "same_thread": None, "bt_run_hex": None}
            if side == "client" else proof == {"spawn_entries": 1, "random_steps": 0, "same_world": True,
                                               "same_thread": True, "bt_run_hex": BT_RUN}, "actual controlled constructor route")
    schedule = plan(scenario)
    require(type(value["operations"]) is list and type(trace["operations"]) is list and
            len(value["operations"]) == len(trace["operations"]) == len(schedule), "all ordered acquisition calls")
    snapshots = [value["initial"]] + [stage for row in value["operations"] for stage in (row["before"], row["after"])]
    links = trace_links(trace, snapshots)
    ids = identities(value)
    projected_initial = snapshot_projection(value["initial"], ids, finish, normal)
    initial = value["initial"]
    expected_context = {"world_id": links[ref(initial["world"]["world"])]["object_id"],
                        "cache_id": links[ref(initial["world"]["cache"])]["object_id"],
                        "generator_id": links[ref(initial["world"]["generator"])]["object_id"]}
    thread_id = trace["operations"][0]["thread_id"]
    integer(thread_id, 1, (1 << 63) - 1, "actual measured main ID")
    projected = {"schema": 1, "seed_hex": SEED, "initial": projected_initial, "operations": []}
    previous = value["initial"]
    initial_noise = projected_initial["generator"]["noise"]
    for index, (row, measured, (method, x, z)) in enumerate(zip(value["operations"], trace["operations"], schedule)):
        keys(row, ("index", "method", "coordinates", "result", "before", "after"), "complete original operation")
        keys(measured, ("index", "thread_id", "world_id", "cache_id", "generator_id", "result", "begin_threads", "end_threads",
                        "begin_random", "end_random", "events", "finishes"), "complete measured operation")
        require(type(row["index"]) is int and row["index"] == index and row["method"] == method and row["coordinates"] == [x, z]
                and type(measured["index"]) is int and measured["index"] == index, "exact fixed operation order")
        require(all(type(measured[name]) is int and measured[name] == number for name, number in expected_context.items()) and
                type(measured["thread_id"]) is int and measured["thread_id"] == thread_id, "one actual retained thread/world/cache/generator context")
        for snapshot in (row["before"], row["after"]):
            retained_snapshot(snapshot, initial)
        previous_locals = {ref(chunk["object"]): chunk for chunk in row["before"]["cached_locals"]}
        after_locals = {ref(chunk["object"]): chunk for chunk in row["after"]["cached_locals"]}
        require(all(after_locals.get(name) == local for name, local in previous_locals.items()), "all prior complete local values and owners retained")
        for name in ("generator", "manager", "materials", "world_time", "world_force_load", "provider_force_load", "opacity"):
            require(row["before"][name] == previous[name], "continuous retained original " + name)
        require(normal.world_projection(row["before"]["world"]) == normal.world_projection(previous["world"]),
                "continuous complete original world")
        before = snapshot_projection(row["before"], ids, finish, normal)
        result = ref(row["result"])
        after_chunks = chunk_map(row["after"])
        new = set(after_chunks) - set(ids)
        require(len(new) <= 1 and (not new or result in new), "at most one genuine nonrecursive generated identity")
        if new:
            ids[result] = len(ids)
        after = snapshot_projection(row["after"], ids, finish, normal)
        require(after["generator"]["noise"] == initial_noise, "all82 immutable retained noise layers")
        for marker in ("begin", "end"):
            validate_inventory(measured[marker + "_threads"], index, marker, side, measured)
            random_state = before if marker == "begin" else after
            require(measured[marker + "_random"] == {"generator_seed": random_state["generator"]["generator_random"]["state"],
                    "cave_seed": random_state["generator"]["cave_random"]["state"]}, "independent JDI/serialized Random state")
        require(result is None and measured["result"] is None or result in ids and measured["result"]["id"] == links[result]["object_id"],
                "actual returned original identity")
        events = measured["events"]
        require(type(events) is list and 1 <= len(events) <= 512, "complete bounded lifecycle events")
        for ordinal, event in enumerate(events):
            require(type(event["ordinal"]) is int and event["ordinal"] == ordinal and event["thread_id"] == measured["thread_id"],
                    "ordered same-thread lifecycle events")
            trace_cache(event["cache"], links, ids)
        lifecycle(measured, row, before, after, ids, links, side, scenario, index)
        callbacks = observed_callbacks(measured, ids, links, [x, z])
        finishes = measured["finishes"]
        require(len(finishes) == len(new), "one complete finish per actual new provider result")
        expected_finishes = []
        if new:
            require([item["kind"] for item in callbacks] == ["load", "generate_begin", "generate_end", "tiles"] + ["entities"] * 8,
                    "genuine load-miss/generation/activation callback order")
            require([event["code_index"] for event in events if event["kind"] == "acquire"] == [0,20,46,78,85,100,112,128,391],
                    "all original acquisition handoff offsets")
            generated = [event for event in events if event["kind"] == "generate_return"]
            require(len(generated) == 1 and generated[0]["generated_local"]["chunk"]["id"] == links[result]["object_id"] and
                    generated[0]["generated_local"]["data"] == after["world"]["chunks"][ids[result]]["data"],
                    "complete finished provider return before publication")
            for kind, offset, count, list_count in (("generate_return", None, len(before["world"]["map"]), len(before["world"]["insertions"])),
                    ("acquire", 100, len(after["world"]["map"]), len(before["world"]["insertions"])),
                    ("acquire", 112, len(after["world"]["map"]), len(after["world"]["insertions"]))):
                stages = [event for event in events if event["kind"] == kind and (offset is None or event["code_index"] == offset)]
                require(len(stages) == 1 and len(stages[0]["cache"]["entries"]) == count and
                        len(stages[0]["cache"]["insertions"]) == list_count, "actual generation/map/list publication boundary")
            activations = [event for event in events if event["kind"] == "activate"]
            require([event["code_index"] for event in activations] == [0,5,51] and
                    [event["chunk"]["flags"]["c"] for event in activations] == [False,True,True], "active flag precedes registration")
            measured_finish = finishes[0]
            require(measured_finish["chunk"]["id"] == links[result]["object_id"] and measured_finish["direct_call_count"] == 1024 and
                    len(measured_finish["records"]) == 1024, "all1024 actual entry/return pairs")
            heights = bytes.fromhex(after["world"]["chunks"][ids[result]]["data"]["heightmap"]["values"])
            for ordinal, query in enumerate(measured_finish["records"]):
                local_x, local_z, direction = ordinal // 64, ordinal // 4 % 16, ordinal % 4
                qx = finish.i32(x * 16 + local_x + (-1,1,0,0)[direction])
                qz = finish.i32(z * 16 + local_z + (0,0,-1,1)[direction])
                require(query == [qx,qz,heights[local_z * 16 + local_x],finish.world_height(row["before"]["world"],qx,qz)],
                        "exact query order, local value and unpublished cache result")
            expected_finishes.append({"id": ids[result], "queries": measured_finish["records"]})
        else:
            require(not callbacks and not finishes and after["generator"] == before["generator"] and
                    after["manager"] == before["manager"], "hit/gate/pending calls do not invoke provider or activation")
        projected["operations"].append({"index": index, "method": method, "coordinates": [x,z],
            "result": None if result is None else ids[result], "status": "ok", "snapshot": after,
            "callbacks": callbacks, "finishes": expected_finishes})
        previous = row["after"]
    minimum_events = len(schedule) * 2 + len(links) + sum(len(operation["events"]) +
        2 * sum(len(finish_["records"]) for finish_ in operation["finishes"]) for operation in trace["operations"])
    require(trace["jdi_event_count"] >= minimum_events, "recorded event count includes complete queries, stages, markers and identity links")
    return projected


def setup_unavailable(process, trace_path, snapshot_path, side, mode, scenario, common, jar_hash):
    require(process.returncode == 77 and not process.stop_reason and not trace_path.exists() and
            not snapshot_path.exists(), "setup77 only without completed measurements or snapshots")
    path = Path(str(trace_path) + ".unavailable.json")
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 16384, "bounded setup-only unavailable proof")
    value = common.strict_json(path.read_bytes())
    keys(value, ("schema", "status", "stage", "child_exit", "side", "mode", "scenario", "jar_sha256",
                 "begin_markers", "end_markers", "completed_operations", "snapshot_exists", "child_command",
                 "debuggee_mode", "elapsed_ms"), "complete setup-only unavailable proof")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["status"] == "unavailable" and
            value["stage"] == "original_setup" and type(value["child_exit"]) is int and value["child_exit"] == 77 and
            value["side"] == side and value["mode"] == mode and value["scenario"] == scenario and value["jar_sha256"] == jar_hash and
            all(type(value[name]) is int and value[name] == 0 for name in ("begin_markers", "end_markers", "completed_operations")) and
            value["snapshot_exists"] is False and value["debuggee_mode"] == "interpreted", "zero-measurement original setup failure")
    return value


def validate_command(command, arguments, tools, classes):
    require(type(command) is list and len(command) == 14 and Path(command[0]).resolve() == tools["java"].resolve() and
            command[1:7] == ["-Xint", "-Xmx512m", "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                              "-cp", str(classes), "CacheAcquisitionObservation"] and command[7:] == arguments,
            "exact attributed original child command")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--capture-only", action="store_true")
    parser.add_argument("--case", type=int, action="append", help="Selected fixed contexts: 0,1 shared; 2 dedicated gates")
    parser.add_argument("--tested-commit", default="")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    report, common, output, inputs = None, None, None, {}
    status, executing, proved_unavailable = 1, False, False
    try:
        common, old, finish, normal, executed = helpers()
        require(args.capture_only or args.executable is not None, "C executable required outside original-only capture")
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "full lowercase source ID")
        require(not any(os.environ.get(name) for name in finish.JAVA_ENVIRONMENT), "injected Java environment")
        selected = args.case if args.case is not None else list(range(len(CASES)))
        require(selected and len(set(selected)) == len(selected) and all(type(n) is int and 0 <= n < len(CASES) for n in selected),
                "distinct fixed acquisition context selection")
        tools = common.runtime_tools(args)
        release = tools["jdk_release"].read_text(encoding="utf-8")
        version = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if version is None or int(version.group(1)) < 17:
            raise common.Unavailable("cache tracing requires JDK17 or newer with jdk.jdi")
        if not args.capture_only and not args.executable.is_file():
            raise common.Unavailable("missing C chunk acquisition executable")
        jars = {side: getattr(args, side).resolve() for side in finish.JAR_HASHES}
        for side, jar in jars.items():
            if not jar.is_file():
                raise common.Unavailable("missing supplied " + side + " original archive")
            require(common.file_record(jar, byte_limit=2 * 1024 * 1024)["sha256"] == finish.JAR_HASHES[side], "pinned original archive")
        sources = [ROOT / "tests/reference/chunk/ChunkObservation.java"] + [
            ROOT / "tests/reference/cache" / name for name in ("CacheAcquisitionObservation.java", "CacheAcquisitionController.java")]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT / "CMakeLists.txt"] + sources
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [ROOT / "tests/world/cache/acquire_probe.c"] + [Path(item["path"]) for item in executed]
        attributed += list(jars.values()) + list(tools.values())
        if not args.capture_only:
            attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024) for path in attributed}
        for item in executed:
            require(inputs[str(Path(item["path"]).resolve())]["sha256"] == item["sha256"], "executed source module attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve()
        report = {"schema": 1, "status": "running", "mode": "original-only" if args.capture_only else "live-original-three-way",
            "tested_commit": args.tested_commit or None, "source_label": "Caller label; actual source/helper/executable fingerprints retained.",
            "started_utc": common.utc_now(), "artifact_root": str(output), "python": sys.version, "platform": platform.platform(),
            "selected_cases": selected, "inputs_before": inputs, "executed_source_modules": executed, "cases": [], "errors": [],
            "scope": "Seed23 persistent-client and dedicated-owner constructor/API contexts, retained providers, no server setup/run/listener, no population/save/unload.",
            "projection": "Map and pending sorted by signed key for membership only; exact insertion-list/queue order; blank dispatch state without backing arrays.",
            "limits": {"controller_seconds": 90, "parent_child_seconds": 120, "heap_mib": 512, "original_record_bytes": MAX_BYTES,
                "c_output_bytes": MAX_C_BYTES, "threads": 64, "frames_per_thread": 64,
                "policy": "Bounded processes and observed inventories, not an OS process-tree or filesystem quota."}}
        common.write_json(output / "manifest.json", report)
        for name in ("source", "classes", "empty", "compile-work"):
            (output / name).mkdir()
        for source in sources:
            (output / "source" / source.name).write_bytes(source.read_bytes())
        common.tool_versions(tools, output, report)

        def run(name, command, directory, seconds=120, world=None, stdin=None, limit=MAX_BYTES):
            process = common.run_live(command, directory / (name + "-out.txt"), directory / (name + "-err.txt"),
                directory, seconds=seconds, stdout_limit=limit, stderr_limit=MAX_BYTES, world_root=world, stdin_path=stdin)
            return process

        def success(process, context):
            require(not process.stop_reason and process.returncode == 0, context + " failed: " + str(asdict(process)))

        modules = run("jdk-modules", [str(tools["java"]), "--list-modules"], output, 15)
        report["jdk_module_process"] = asdict(modules)
        success(modules, "JDK module preflight")
        available_modules = {line.split("@", 1)[0] for line in (output / "jdk-modules-out.txt").read_text(encoding="utf-8").splitlines()}
        if not {"jdk.jdi", "jdk.jdwp.agent"}.issubset(available_modules):
            raise common.Unavailable("JDK modules jdk.jdi and jdk.jdwp.agent are required")
        executing = True
        compiled = run("compile", [str(tools["javac"]), "-J-Xmx512m", "--release", "17", "--add-modules", "jdk.jdi",
            "-proc:none", "-implicit:none", "-encoding", "UTF-8", "-Xlint:all", "-Werror", "-classpath", str(output / "empty"),
            "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"), "-d", str(output / "classes")] +
            [str(output / "source" / path.name) for path in sources], output / "compile-work", 30)
        report["compile"] = asdict(compiled)
        success(compiled, "compilation")
        classes = sorted((output / "classes").glob("*.class"))
        families = tuple(path.stem for path in sources)
        require(3 <= len(classes) <= 16 and all(any(path.stem == name or path.stem.startswith(name + "$") for name in families) for path in classes),
                "complete known compiled helper families")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024) for path in classes}
        shared = {}
        for index in selected:
            side, mode, scenario = CASES[index]
            directory = output / (f"case-{index}-" + side + "-" + scenario)
            directory.mkdir()
            row = {"index": index, "side": side, "mode": mode, "scenario": scenario, "comparisons": {}}
            report["cases"].append(row)
            snapshot, trace = directory / "snapshots.json", directory / "trace.json"
            world = directory / "world"
            arguments = [str(jars[side]), side, mode, SEED, str(world), str(snapshot), scenario]
            process = run("controller", [str(tools["java"]), "--add-modules", "jdk.jdi", "-Xmx512m", "-cp", str(output / "classes"),
                "CacheAcquisitionController", *arguments, str(trace)], directory, world=world)
            row["controller"] = asdict(process)
            if process.returncode == 77 and not process.stop_reason:
                row["unavailable"] = setup_unavailable(process, trace, snapshot, side, mode, scenario, common, finish.JAR_HASHES[side])
                validate_command(row["unavailable"]["child_command"], arguments, tools, output / "classes")
                proved_unavailable = True
                raise common.Unavailable("genuine original setup unavailable before measurements: case " + str(index))
            success(process, "original traced acquisition")
            require(snapshot.stat().st_size <= MAX_BYTES and trace.stat().st_size <= MAX_BYTES, "complete bounded original artifacts")
            value, measured = common.strict_json(snapshot.read_bytes()), common.strict_json(trace.read_bytes())
            validate_command(measured["child_command"], arguments, tools, output / "classes")
            expected = validate_original(value, measured, side, mode, scenario, jars[side], normal, finish)
            control, control_world = directory / "control.json", directory / "control-world"
            control_arguments = [str(jars[side]), side, mode, SEED, str(control_world), str(control), scenario]
            process = run("control", [str(tools["java"]), "-Xint", "-Xmx512m", "-cp", str(output / "classes"),
                "CacheAcquisitionObservation", *control_arguments], directory, 90, world=control_world)
            row["control"] = asdict(process)
            if process.returncode == 77 and not process.stop_reason:
                require(not control.exists() and not control.is_symlink(), "control setup77 without completed snapshots")
                error_lines = (directory / "control-err.txt").read_text(encoding="utf-8").splitlines()
                require("UNAVAILABLE: genuine acquisition setup" in error_lines and
                        "FAILURE: acquisition observation" not in error_lines, "genuine premeasurement control setup boundary")
                proved_unavailable = True
                raise common.Unavailable("genuine untouched-control setup unavailable before its measurements: case " + str(index))
            success(process, "untouched control")
            require(control.stat().st_size <= MAX_BYTES and control.read_bytes() == snapshot.read_bytes(), "entire untouched control differs")
            row["control_equal"] = True
            row["snapshot_record"] = common.file_record(snapshot, byte_limit=MAX_BYTES)
            row["trace_record"] = common.file_record(trace, byte_limit=MAX_BYTES)
            row["control_record"] = common.file_record(control, byte_limit=MAX_BYTES)
            row["world"] = common.world_inventory(world, hashes=True)
            row["control_world"] = common.world_inventory(control_world, hashes=True)
            require(row["world"]["complete"] and row["control_world"]["complete"], "complete original world-file inventories")
            replay = directory / "input.mca"
            replay.write_bytes(replay_input(value, finish))
            row["replay_input"] = common.file_record(replay, byte_limit=4 * 1024 * 1024)
            common.write_json(directory / "expected.json", expected)
            row["projection_record"] = common.file_record(directory / "expected.json", byte_limit=MAX_C_BYTES)
            if not args.capture_only:
                process = run("c", [str(args.executable.resolve())], directory, 90, stdin=replay, limit=MAX_C_BYTES)
                row["c_process"] = asdict(process)
                success(process, "C composed acquisition")
                actual = common.strict_json((directory / "c-out.txt").read_bytes())
                require(same_json(actual, expected), "C/" + side + " complete composed acquisition differs")
                row["comparisons"]["C/" + side] = "pass"
                row["c_record"] = common.file_record(directory / "c-out.txt", byte_limit=MAX_C_BYTES)
            require(common.file_record(replay, byte_limit=4 * 1024 * 1024) == row["replay_input"], "immutable C replay input changed")
            row["queries"] = sum(len(finish_["queries"]) for operation in expected["operations"] for finish_ in operation["finishes"])
            row["operations"] = len(expected["operations"])
            row["stage_events"] = sum(len(operation["events"]) for operation in measured["operations"])
            if scenario == "shared":
                shared[side] = expected
            common.write_json(output / "manifest.json", report)
        if set(shared) == {"client", "server"}:
            require(same_json(shared["client"], shared["server"]), "complete common client/server acquisition differs")
            report["client/server"] = "pass"
        else:
            report["client/server"] = "not selected; no cross-context result claimed"
        report["status"], status = "pass", 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable) and (not executing or proved_unavailable)
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
                    require(after == before, "attributed input changed: " + name)
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            if "classes_before" in report:
                try:
                    after = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                             for path in sorted((output / "classes").glob("*.class"))}
                    report["classes_after"] = after
                    require(after == report["classes_before"], "compiled helper inventory or bytes changed")
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            report["finished_utc"] = common.utc_now()
            common.write_json(output / "manifest.json", report)
            print(report["status"].upper() + ": " + str(output / "manifest.json"))
    return status


if __name__ == "__main__":
    sys.exit(main())
