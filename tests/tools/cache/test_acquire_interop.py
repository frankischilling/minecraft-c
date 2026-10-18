"""Validate public original acquisition fixtures and replay each continuous MCA1 input."""

from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/cache/acquire_fixtures.json"
MAX_INPUT = 4 * 1024 * 1024
MAX_ORIGINAL = 32 * 1024 * 1024
MAX_EXPECTED = 64 * 1024 * 1024
MAX_FIXTURE = 8 * 1024 * 1024
SEED = "0000000000000017"
CASES = (("client", "persistent-client", "shared"),
         ("server", "dedicated-constructor", "shared"),
         ("server", "dedicated-constructor", "dedicated-gates"))
PLANS = {"shared": (("c", 1, 0), ("c", 0, 0), ("b", 0, 0),
                    ("c", 32768, 0), ("c", -1, -1), ("c", 1, 0)),
         "dedicated-gates": (("b", 9, 0), ("d", 9, 0), ("b", 9, 0), ("c", 9, 0),
                             ("d", 9, 0), ("b", 9, 0), ("c", 9, 0), ("c", 0, 0))}
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
HELPERS = ("ChunkObservation.java", "CacheAcquisitionObservation.java", "CacheAcquisitionController.java")
MODULES = {"scripts/reference/caves/compare_caves.py", "scripts/reference/caves/fixtures.py",
           "scripts/reference/chunk/compare_chunk.py", "scripts/reference/chunk/compare_finish.py",
           "scripts/reference/generation/compare_normal.py"}
SCRATCH = {"q": 425, "r": 256, "s": 256, "t": 256, "d": 425, "e": 425, "f": 425, "g": 25, "h": 25}
LENGTHS = {"blocks": 32768, "metadata": 16384, "skylight": 16384, "block_light": 16384, "heightmap": 256}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def keys(value, expected, message):
    require(type(value) is dict and set(value) == set(expected), message + ": complete keys")


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    require(type(value) is int and low <= value <= high, "bounded integer")


def pairs(values):
    result = {}
    for key, value in values:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def strict_json(data):
    def finite(value):
        result = float(value)
        require(math.isfinite(result), "non-finite JSON number")
        return result
    return json.loads(data, object_pairs_hook=pairs, parse_float=finite,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError("non-finite JSON: " + value)))


def hex_string(value, count):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]{" + str(count) + "}", value) is not None,
            "complete lowercase hexadecimal bytes")


def fingerprint(value, limit=512 * 1024 * 1024, empty=False):
    keys(value, ("bytes", "sha256"), "fingerprint")
    integer(value["bytes"], 0 if empty else 1, limit)
    hex_string(value["sha256"], 64)


def unpack(value, limit):
    keys(value, ("bytes", "sha256", "zlib_base64"), "packed fixture")
    fingerprint({name: value[name] for name in ("bytes", "sha256")}, limit)
    require(type(value["zlib_base64"]) is str and len(value["zlib_base64"]) <= MAX_FIXTURE, "packed base64 byte bound")
    encoded = base64.b64decode(value["zlib_base64"], validate=True)
    require(base64.b64encode(encoded).decode("ascii") == value["zlib_base64"], "canonical fixture base64")
    decoder = zlib.decompressobj()
    data = decoder.decompress(encoded, value["bytes"] + 1)
    require(len(data) == value["bytes"] and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and
            hashlib.sha256(data).hexdigest() == value["sha256"], "complete packed bytes/digest")
    return data


def packed(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "zlib_base64": base64.b64encode(zlib.compress(data)).decode("ascii")}


def relative_name(name):
    return (type(name) is str and bool(name) and ":" not in name and "\\" not in name and
            not PurePosixPath(name).is_absolute() and all(part not in ("", ".", "..") for part in name.split("/")))


def validate_inventory(value):
    keys(value, ("exists", "entries", "files", "directories", "bytes", "complete", "error"), "world inventory")
    require(type(value["exists"]) is bool and value["complete"] is True and value["error"] is None and
            type(value["entries"]) is list and len(value["entries"]) <= 4096, "complete bounded world inventory")
    seen, files, directories, total = set(), 0, 0, 0
    for entry in value["entries"]:
        name = entry["path"]
        require(relative_name(name) and name not in seen, "unique relative inventory path")
        seen.add(name)
        if entry["kind"] == "file":
            keys(entry, ("path", "kind", "bytes", "sha256"), "inventory file")
            fingerprint({key: entry[key] for key in ("bytes", "sha256")}, 64 * 1024 * 1024, empty=True)
            files += 1
            total += entry["bytes"]
        else:
            require(entry == {"path": name, "kind": "directory"}, "known inventory directory")
            directories += 1
    require(all(type(value[name]) is int for name in ("files", "directories", "bytes")) and
            (value["files"], value["directories"], value["bytes"]) == (files, directories, total), "complete inventory totals")


def bindings(side):
    return dict(zip(("world", "cache", "storage", "loader", "generator"),
                    ("fd", "ok", "mx", "ld", "yf") if side == "client" else ("dp", "he", "ie", "hb", "pk")))


def validate_fixture(value):
    keys(value, ("schema", "encoding", "projection", "provenance", "cases"), "acquisition fixture")
    require(type(value["schema"]) is int and value["schema"] == 1 and type(value["encoding"]) is str and
            "MCA1" in value["encoding"] and type(value["projection"]) is str and value["projection"], "acquisition fixture schema")
    origin = value["provenance"]
    keys(origin, ("mode", "jar_sha256", "source_fingerprints", "helper_sources", "helper_classes", "helper_class_version",
                  "executed_source_modules", "cases", "operations", "generation_calls", "height_query_pairs",
                  "marker_inventories", "manifest_sha256", "stage_events"), "original provenance")
    require(origin["mode"] == "original-only" and origin["jar_sha256"] == JAR_HASHES and
            type(origin["helper_class_version"]) is int and origin["helper_class_version"] == 61,
            "pinned original-only archive/helper provenance")
    hex_string(origin["manifest_sha256"], 64)
    for name, expected in (("cases", 3), ("operations", 20), ("generation_calls", 5),
                            ("height_query_pairs", 5120), ("marker_inventories", 40)):
        require(type(origin[name]) is int and origin[name] == expected, "complete original corpus counts")
    sources = origin["source_fingerprints"]
    required = MODULES | {"scripts/reference/cache/compare_acquire.py", "tests/world/cache/acquire_probe.c",
                          "src/world/cache/acquire.c", "include/mc/world/cache/acquire.h", "CMakeLists.txt"}
    required |= {"tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name for name in HELPERS}
    require(type(sources) is dict and required <= set(sources) and len(sources) <= 4096, "complete source attribution")
    for name, record in sources.items():
        require(relative_name(name) and (name == "CMakeLists.txt" or name.startswith(("scripts/", "tests/", "src/", "include/"))),
                "relative project source attribution")
        fingerprint(record)
    keys(origin["helper_sources"], HELPERS, "all three original helper sources")
    for name, record in origin["helper_sources"].items():
        relative = "tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name
        require(record == sources[relative], "copied helper source attribution")
    classes, owners = origin["helper_classes"], tuple(name[:-5] for name in HELPERS)
    require(type(classes) is dict and 3 <= len(classes) <= 16 and all(owner + ".class" in classes for owner in owners),
            "complete compiled helper class inventory")
    for name, record in classes.items():
        require(type(name) is str and re.fullmatch(r"[A-Za-z0-9_$]+\.class", name) and
                any(name == owner + ".class" or name.startswith(owner + "$") for owner in owners), "known helper class family")
        fingerprint(record, 2 * 1024 * 1024)
    keys(origin["executed_source_modules"], MODULES, "complete executed validation modules")
    for name, record in origin["executed_source_modules"].items():
        require(record == sources[name], "executed module source attribution")
    rows = value["cases"]
    require(type(rows) is list and len(rows) == 3, "all three acquisition cases required")
    total_events = 0
    for index, (row, context) in enumerate(zip(rows, CASES)):
        keys(row, ("index", "side", "mode", "scenario", "seed_hex", "schedule", "operations", "generation_calls",
                   "height_query_pairs", "stage_events", "marker_inventories", "control_equal", "jar_sha256", "bindings",
                   "class_origins", "input", "expected", "protocol", "snapshot_record", "trace_record", "control_record",
                   "replay_input", "projection_record", "world", "control_world"), "complete original fixture case")
        require(type(row["index"]) is int and row["index"] == index and
                (row["side"], row["mode"], row["scenario"]) == context and row["seed_hex"] == SEED and
                row["schedule"] == [list(item) for item in PLANS[context[2]]], "exact fixed acquisition corpus/order")
        for method, x, z in row["schedule"]:
            integer(x)
            integer(z)
        operations, generations = (6, 2) if index < 2 else (8, 1)
        require(row["control_equal"] is True and row["jar_sha256"] == JAR_HASHES[row["side"]] and
                row["bindings"] == bindings(row["side"]), "genuine constructor bindings and untouched control")
        for name, expected in (("operations", operations), ("generation_calls", generations),
                                ("height_query_pairs", generations * 1024), ("marker_inventories", operations * 2)):
            require(type(row[name]) is int and row[name] == expected, "complete original per-case counts")
        integer(row["stage_events"], operations, operations * 512)
        total_events += row["stage_events"]
        origins = row["class_origins"]
        require(type(origins) is dict and 20 <= len(origins) <= 128 and set(row["bindings"].values()) <= set(origins) and
                all(type(name) is str and re.fullmatch(r"[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*", name) and
                    url == JAR_HASHES[row["side"]]
                    for name, url in origins.items()), "isolated supplied archive class origins")
        for name in ("snapshot_record", "trace_record", "control_record", "replay_input", "projection_record"):
            fingerprint(row[name], MAX_EXPECTED if name == "projection_record" else MAX_INPUT if name == "replay_input" else MAX_ORIGINAL)
        require(row["snapshot_record"] == row["control_record"], "entire untouched control fingerprint equality")
        for name, record in (("input", "replay_input"), ("expected", "projection_record")):
            keys(row[name], ("bytes", "sha256", "zlib_base64"), "packed input/projection")
            require({key: row[name].get(key) for key in ("bytes", "sha256")} == row[record],
                    "retained replay/projection fingerprint")
        keys(row["protocol"], ("snapshot", "trace"), "complete public original protocol")
        for item in row["protocol"].values():
            keys(item, ("bytes", "sha256", "zlib_base64"), "packed original protocol")
            fingerprint({name: item[name] for name in ("bytes", "sha256")}, MAX_ORIGINAL)
        require(sum(item["bytes"] for item in row["protocol"].values()) <= MAX_ORIGINAL, "bounded protocol pair")
        for name in ("world", "control_world"):
            validate_inventory(row[name])
    require(type(origin["stage_events"]) is int and total_events == origin["stage_events"], "complete lifecycle event count")


def descriptor(value, length=None, kind="bytes", nullable=False):
    if value is None:
        require(nullable, "required descriptor")
        return
    keys(value, ("length", "values"), "raw descriptor")
    integer(value["length"], 0, 4096 if length is None else length)
    require(length is None or value["length"] == length, "complete descriptor length")
    if kind == "biomes":
        require(type(value["values"]) is str and re.fullmatch(r"[a-m]{" + str(value["length"]) + "}", value["values"]) is not None,
                "actual biome descriptors")
    else:
        hex_string(value["values"], value["length"] * (16 if kind == "doubles" else 2))


def next_longs(state):
    values = []
    for _ in range(8):
        state = (state * 25214903917 + 11) & ((1 << 48) - 1)
        high = state >> 16
        state = (state * 25214903917 + 11) & ((1 << 48) - 1)
        low = state >> 16
        high = high if high < 1 << 31 else high - (1 << 32)
        low = low if low < 1 << 31 else low - (1 << 32)
        values.append(f"{((high << 32) + low) & ((1 << 64) - 1):016x}")
    return values


def validate_snapshot(value):
    keys(value, ("world", "generator", "manager"), "complete composed snapshot")
    world = value["world"]
    keys(world, ("map", "insertions", "pending", "chunks", "queue", "depth"), "complete world state")
    integer(world["depth"])
    chunks = world["chunks"]
    require(type(chunks) is list and 1 <= len(chunks) <= 32, "retained chunk inventory")
    for index, chunk in enumerate(chunks):
        keys(chunk, ("id", "coordinates", "blank", "flags", "time", "data"), "retained chunk")
        require(type(chunk["id"]) is int and chunk["id"] == index and type(chunk["blank"]) is bool and
                type(chunk["coordinates"]) is list and len(chunk["coordinates"]) == 2, "stable chunk identity/coordinate")
        for number in chunk["coordinates"]:
            integer(number)
        keys(chunk["flags"], ("c", "n", "o", "p", "q"), "retained chunk flags")
        require(all(type(flag) is bool for flag in chunk["flags"].values()), "actual chunk booleans")
        hex_string(chunk["time"], 16)
        if chunk["blank"]:
            require(chunk["data"] is None, "blank virtual dispatch has no projected backing data")
        else:
            keys(chunk["data"], (*LENGTHS, "minimum_height"), "all normal chunk arrays")
            integer(chunk["data"]["minimum_height"])
            for name, length in LENGTHS.items():
                descriptor(chunk["data"][name], length)
    require(chunks[0]["blank"] is True, "real blank fallback identity zero")
    require(type(world["map"]) is list and len(world["map"]) <= 64, "bounded map membership")
    seen = set()
    for entry in world["map"]:
        require(type(entry) is list and len(entry) == 2, "packed map pair")
        integer(entry[0])
        integer(entry[1], 0, len(chunks) - 1)
        require(entry[0] not in seen, "unique packed map keys")
        seen.add(entry[0])
    require(world["map"] == sorted(world["map"]), "signed-key map projection")
    require(type(world["insertions"]) is list and len(world["insertions"]) <= 64 and
            type(world["pending"]) is list and len(world["pending"]) <= 64, "bounded list/pending")
    for number in world["insertions"]:
        integer(number, 0, len(chunks) - 1)
    for number in world["pending"]:
        integer(number)
    require(world["pending"] == sorted(set(world["pending"])), "unique signed-key pending membership")
    require(type(world["queue"]) is list and len(world["queue"]) <= 32768, "full ordered queue")
    for entry in world["queue"]:
        require(type(entry) is list and len(entry) == 7, "queued light bounds")
        integer(entry[0], 0, 1)
        for number in entry[1:]:
            integer(number)
    manager = value["manager"]
    keys(manager, ("a", "b", "c", "d"), "complete manager")
    for name in ("a", "b", "c", "d"):
        descriptor(manager[name], kind="biomes" if name == "d" else "doubles", nullable=True)
    generator = value["generator"]
    keys(generator, ("noise", "scratch", "biomes", "materials", "generator_random", "cave_random"), "complete generator")
    sizes = dict(zip(("k", "l", "m", "n", "o", "a", "b", "c"), (16, 16, 8, 4, 4, 10, 16, 8)))
    keys(generator["noise"], sizes, "all82 noise layers")
    for name, count in sizes.items():
        require(type(generator["noise"][name]) is list and len(generator["noise"][name]) == count, "complete noise channel")
        for layer in generator["noise"][name]:
            keys(layer, ("offsets", "permutation"), "noise layer")
            require(type(layer["offsets"]) is list and len(layer["offsets"]) == 3 and
                    type(layer["permutation"]) is list and len(layer["permutation"]) == 512, "raw noise layer size")
            for number in layer["offsets"]:
                hex_string(number, 16)
            for number in layer["permutation"]:
                integer(number, 0, 255)
    keys(generator["scratch"], SCRATCH, "all retained scratch")
    for name, length in SCRATCH.items():
        descriptor(generator["scratch"][name], length, "doubles")
    descriptor(generator["biomes"], 256, "biomes")
    hex_string(generator["materials"], 1024)
    for name in ("generator_random", "cave_random"):
        random = generator[name]
        keys(random, ("state", "next_longs"), "raw Random and copied continuations")
        hex_string(random["state"], 12)
        require(random["next_longs"] == next_longs(int(random["state"], 16)), "independent Java Random continuation")


def validate_projection(value, row):
    keys(value, ("schema", "seed_hex", "initial", "operations"), "full acquisition projection")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["seed_hex"] == SEED and
            type(value["operations"]) is list and len(value["operations"]) == row["operations"], "continuous fixed acquisition projection")
    validate_snapshot(value["initial"])
    prior = value["initial"]
    total = 0
    finish_indices = (0, 4) if row["scenario"] == "shared" else (3,)
    for index, (operation, schedule) in enumerate(zip(value["operations"], row["schedule"])):
        keys(operation, ("index", "method", "coordinates", "result", "status", "snapshot", "callbacks", "finishes"), "full operation")
        require(type(operation["index"]) is int and operation["index"] == index and operation["method"] == schedule[0] and
                type(operation["coordinates"]) is list and operation["coordinates"] == schedule[1:] and
                operation["status"] == "ok", "exact operation order/result status")
        for number in operation["coordinates"]:
            integer(number)
        validate_snapshot(operation["snapshot"])
        chunks = operation["snapshot"]["world"]["chunks"]
        if operation["result"] is not None:
            integer(operation["result"], 0, len(chunks) - 1)
        require(type(operation["finishes"]) is list and len(operation["finishes"]) == int(index in finish_indices),
                "fixed genuine provider generation calls")
        require(type(operation["callbacks"]) is list, "complete lifecycle callback records")
        expected_kinds = ["load", "generate_begin", "generate_end", "tiles"] + ["entities"] * 8 if index in finish_indices else []
        require([callback["kind"] for callback in operation["callbacks"]] == expected_kinds, "exact load/generate/activation ordering")
        for number, callback in enumerate(operation["callbacks"]):
            keys(callback, ("kind", "x", "z", "id", "slice", "map_count", "list_count", "pending_count", "active", "dirty",
                            "populated", "last_save_time"), "complete callback state")
            require([callback["x"], callback["z"]] == schedule[1:] and
                    (type(callback["slice"]) is int and callback["slice"] == number - 4 if number >= 4 else callback["slice"] is None),
                    "callback coordinate and slice order")
            integer(callback["x"])
            integer(callback["z"])
            for name in ("map_count", "list_count", "pending_count"):
                integer(callback[name], 0, 64)
            if number < 2:
                require(all(callback[name] is None for name in ("id", "active", "dirty", "populated", "last_save_time")),
                        "unpublished callback context")
            else:
                integer(callback["id"], 0, len(chunks) - 1)
                require(all(type(callback[name]) is bool for name in ("active", "dirty", "populated")), "callback chunk flags")
                hex_string(callback["last_save_time"], 16)
        for finish in operation["finishes"]:
            keys(finish, ("id", "queries"), "complete finish trace")
            integer(finish["id"], 0, len(chunks) - 1)
            require(finish["id"] == operation["result"] and type(finish["queries"]) is list and len(finish["queries"]) == 1024,
                    "all1024 finish entry/return pairs")
            for query in finish["queries"]:
                require(type(query) is list and len(query) == 4, "complete height record")
                for number in query:
                    integer(number)
                integer(query[2], 0, 255)
                integer(query[3], 0, 255)
            total += len(finish["queries"])
        if index not in finish_indices:
            require(operation["snapshot"]["generator"] == prior["generator"] and
                    operation["snapshot"]["manager"] == prior["manager"], "hit/gate/drop preserves provider state")
        require(operation["snapshot"]["generator"]["noise"] == value["initial"]["generator"]["noise"], "all82 noise layers retained")
        prior = operation["snapshot"]
    require(total == row["height_query_pairs"], "complete height trace count")


def validate_input(data, row, initial=None):
    position = 0

    def take(count):
        nonlocal position
        require(type(count) is int and 0 <= count <= len(data) - position, "complete MCA1 input")
        result = data[position:position + count]
        position += count
        return result

    def read(format):
        return struct.unpack(format, take(struct.calcsize(format)))

    require(0 < len(data) <= MAX_INPUT and take(4) == b"MCA1", "bounded MCA1 input magic")
    seed, dedicated, world_force, provider_force, spawn_x, spawn_z, time, depth, no_sky, nc, ne, ni, np, nq, no = read(">qBBBiiqiBIIIIII")
    require(seed == int(SEED, 16) and dedicated == int(row["side"] == "server") and
            world_force in (0, 1) and provider_force in (0, 1) and (spawn_x, spawn_z) == (0, 0) and no_sky in (0, 1) and
            1 <= nc <= 16 and max(ne, ni, np) <= 64 and nq <= 32768 and no == row["operations"], "fixed bounded MCA1 header")
    require(all(value >= 0 for value in read(">256i")), "nonnegative actual opacity")
    take(26)
    manager = {}
    for name in ("a", "b", "c", "d"):
        length, = read(">i")
        require(-1 <= length <= 4096, "bounded MCA1 manager descriptors")
        raw = take(length if name == "d" else length * 8) if length >= 0 else None
        if name == "d" and raw is not None:
            require(all(ord("a") <= value <= ord("m") for value in raw), "actual MCA1 manager biomes")
        manager[name] = None if raw is None else {"length": length, "values": raw.decode("ascii") if name == "d" else raw.hex()}
    chunks = []
    for index in range(nc):
        x, z, blank, dirty, active, populated, never_save, entities, saved, minimum = read(">iiBBBBBBqi")
        require(all(value in (0, 1) for value in (blank, dirty, active, populated, never_save, entities)), "canonical MCA1 chunk booleans")
        heights = take(256)
        data_value = None
        if not blank:
            data_value = {name: {"length": length, "values": take(length).hex()}
                          for name, length in LENGTHS.items() if name != "heightmap"}
            data_value.update(heightmap={"length": 256, "values": heights.hex()}, minimum_height=minimum)
        chunks.append({"id": index, "coordinates": [x, z], "blank": bool(blank),
                       "flags": dict(zip(("c", "n", "o", "p", "q"), map(bool, (active, populated, dirty, never_save, entities)))),
                       "time": f"{saved & ((1 << 64) - 1):016x}", "data": data_value})
    require(chunks[0]["blank"], "MCA1 blank fallback identity zero")
    mappings, seen = [], set()
    for _ in range(ne):
        key, number = read(">iI")
        require(key not in seen and number < nc, "unique real MCA1 cache key/identity")
        seen.add(key)
        mappings.append([key, number])
    insertions = [read(">I")[0] for _ in range(ni)]
    require(all(number < nc for number in insertions), "actual ordered insertion identities")
    pending = [read(">i")[0] for _ in range(np)]
    require(len(set(pending)) == len(pending), "unique pending packed keys")
    queue = [list(read(">Iiiiiii")) for _ in range(nq)]
    require(all(entry[0] in (0, 1) for entry in queue), "actual queued light kinds")
    schedule = []
    for _ in range(no):
        method, x, z = read(">Iii")
        require(method <= 2, "fixed acquisition MCA1 methods")
        schedule.append([("b", "c", "d")[method], x, z])
    require(schedule == row["schedule"] and position == len(data), "exact fixed schedule and complete MCA1 consumption")
    if initial is not None:
        require(initial["manager"] == manager and initial["world"] == {
            "map": sorted(mappings), "insertions": insertions, "pending": sorted(pending), "chunks": chunks, "queue": queue,
            "depth": depth}, "MCA1 exactly imports recorded initial world/manager")
    return {"dedicated": dedicated, "world_force": world_force, "provider_force": provider_force,
            "time": time, "no_sky": no_sky, "chunks": nc, "operations_offset": len(data) - no * 12}


def load_fixture(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= MAX_FIXTURE, "public fixture file size/type")
    value = strict_json(path.read_bytes())
    validate_fixture(value)
    decoded = []
    for row in value["cases"]:
        expected = strict_json(unpack(row["expected"], MAX_EXPECTED))
        validate_projection(expected, row)
        data = unpack(row["input"], MAX_INPUT)
        validate_input(data, row, expected["initial"])
        snapshot = strict_json(unpack(row["protocol"]["snapshot"], MAX_ORIGINAL))
        trace = strict_json(unpack(row["protocol"]["trace"], MAX_ORIGINAL))
        require(snapshot["class_origins"] == {name: "supplied:" + row["side"] for name in row["class_origins"]} and
                all(snapshot[name] == row[name] for name in ("side", "mode", "scenario", "seed_hex")) and
                all(trace[name] == row[name] for name in ("side", "mode", "scenario", "seed_hex")), "complete redacted original protocol provenance")
        command = trace["child_command"]
        require(type(command) is list and len(command) == 14 and
                [command[index] for index in (0, 5, 7, 11, 12)] ==
                ["tool:java", "helper:classes", "supplied:" + row["side"], "case:world", "case:snapshots.json"],
                "logical original command provenance")
        decoded.append((row, data, expected))
    require(decoded[0][2] == decoded[1][2], "complete shared client/server original equivalence")
    return value, decoded


class AcquisitionFixtureTests(unittest.TestCase):
    @staticmethod
    def minimal_fixture():
        record = {"bytes": 1, "sha256": hashlib.sha256(b"x").hexdigest()}
        names = MODULES | {"scripts/reference/cache/compare_acquire.py", "tests/world/cache/acquire_probe.c",
                           "src/world/cache/acquire.c", "include/mc/world/cache/acquire.h", "CMakeLists.txt"}
        names |= {"tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name for name in HELPERS}
        sources = {name: record for name in names}
        inventory = {"exists": True, "entries": [], "files": 0, "directories": 0, "bytes": 0, "complete": True, "error": None}
        cases = []
        for index, (side, mode, scenario) in enumerate(CASES):
            operations, generations = (6, 2) if index < 2 else (8, 1)
            classes = set(bindings(side).values()) | {"class" + str(number) for number in range(20)}
            cases.append({"index": index, "side": side, "mode": mode, "scenario": scenario, "seed_hex": SEED,
                "schedule": [list(item) for item in PLANS[scenario]], "operations": operations, "generation_calls": generations,
                "height_query_pairs": generations * 1024, "stage_events": operations, "marker_inventories": operations * 2,
                "control_equal": True, "jar_sha256": JAR_HASHES[side], "bindings": bindings(side),
                "class_origins": {name: JAR_HASHES[side] for name in classes}, "input": packed(b"x"), "expected": packed(b"x"),
                "protocol": {"snapshot": packed(b"x"), "trace": packed(b"x")}, "snapshot_record": record, "trace_record": record,
                "control_record": record, "replay_input": record, "projection_record": record, "world": inventory, "control_world": inventory})
        return {"schema": 1, "encoding": "MCA1", "projection": "membership; exact list/queue",
            "provenance": {"mode": "original-only", "jar_sha256": JAR_HASHES, "source_fingerprints": sources,
                "helper_sources": {name: record for name in HELPERS},
                "helper_classes": {name[:-5] + ".class": record for name in HELPERS}, "helper_class_version": 61,
                "executed_source_modules": {name: record for name in MODULES}, "cases": 3, "operations": 20,
                "generation_calls": 5, "height_query_pairs": 5120, "marker_inventories": 40,
                "manifest_sha256": "2" * 64, "stage_events": 20}, "cases": cases}

    def test_public_corpus_is_complete(self):
        _, rows = load_fixture(FIXTURE)
        self.assertEqual(sum(len(expected["operations"]) for _, _, expected in rows), 20)

    def test_corpus_and_provenance_guards(self):
        fixture = self.minimal_fixture()
        validate_fixture(fixture)
        for mode in ("missing", "duplicate", "order", "side", "scenario", "seed", "schedule", "control", "control_digest",
                     "counts", "source", "source_path", "source_hash", "class", "class_version", "module", "mode", "origins", "protocol"):
            value = copy.deepcopy(fixture)
            row, origin = value["cases"][0], value["provenance"]
            if mode == "missing": value["cases"].pop()
            elif mode == "duplicate": value["cases"][2] = row
            elif mode == "order": value["cases"].reverse()
            elif mode == "side": row["side"] = "other"
            elif mode == "scenario": row["scenario"] = "other"
            elif mode == "seed": row["seed_hex"] = "0000000000000018"
            elif mode == "schedule": row["schedule"][0][1] = 2
            elif mode == "control": row["control_equal"] = False
            elif mode == "control_digest": row["control_record"] = {"bytes": 1, "sha256": "3" * 64}
            elif mode == "counts": row["height_query_pairs"] -= 1
            elif mode == "source": del origin["source_fingerprints"]["scripts/reference/cache/compare_acquire.py"]
            elif mode == "source_path": origin["source_fingerprints"]["C:/private/source.c"] = {"bytes": 1, "sha256": "3" * 64}
            elif mode == "source_hash": origin["source_fingerprints"]["scripts/reference/cache/compare_acquire.py"] = {"bytes": 1, "sha256": "bad"}
            elif mode == "class": origin["helper_classes"].clear()
            elif mode == "class_version": origin["helper_class_version"] = 65
            elif mode == "module": origin["executed_source_modules"].clear()
            elif mode == "mode": origin["mode"] = "live-original-three-way"
            elif mode == "origins": row["class_origins"]["fd"] = "file:/private.jar"
            else: del row["protocol"]["trace"]
            with self.subTest(mode=mode), self.assertRaises((ValueError, KeyError)):
                validate_fixture(value)

    def test_packed_streams_and_strict_json(self):
        raw = b"MCA1" + bytes(range(256))
        value = packed(raw)
        self.assertEqual(unpack(value, MAX_INPUT), raw)
        for mode in ("digest", "length", "base64", "truncated", "trailing", "second", "corrupt", "bomb"):
            changed = dict(value)
            stream = base64.b64decode(value["zlib_base64"])
            if mode == "digest": changed["sha256"] = "0" * 64
            elif mode == "length": changed["bytes"] -= 1
            elif mode == "base64": changed["zlib_base64"] = "!"
            elif mode == "truncated": changed["zlib_base64"] = base64.b64encode(stream[:-1]).decode("ascii")
            elif mode == "trailing": changed["zlib_base64"] = base64.b64encode(stream + b"x").decode("ascii")
            elif mode == "second": changed["zlib_base64"] = base64.b64encode(stream + stream).decode("ascii")
            elif mode == "corrupt": changed["zlib_base64"] = base64.b64encode(stream[:-1] + bytes((stream[-1] ^ 1,))).decode("ascii")
            else: changed["zlib_base64"] = base64.b64encode(zlib.compress(b"x" * 1048576)).decode("ascii")
            with self.subTest(mode=mode), self.assertRaises((ValueError, zlib.error)):
                unpack(changed, MAX_INPUT)
        for raw in (b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":Infinity}', b'{"x":1e999}'):
            with self.assertRaises(ValueError):
                strict_json(raw)

    def test_input_complete_fixed_schedule_and_imports(self):
        _, rows = load_fixture(FIXTURE)
        row, data, expected = rows[0]
        header = validate_input(data, row, expected["initial"])
        mutations = [b"BAD1" + data[4:], data[:-1], data + b"x", data[:11] + b"\x18" + data[12:],
                     data[:12] + b"\x02" + data[13:]]
        offset = header["operations_offset"]
        mutations += [data[:offset] + struct.pack(">Iii", 1, 2, 0) + data[offset + 12:]]
        for changed in mutations:
            with self.subTest(size=len(changed)), self.assertRaises(ValueError):
                validate_input(changed, row, expected["initial"])
        altered = copy.deepcopy(expected["initial"])
        altered["world"]["insertions"].clear()
        with self.assertRaises(ValueError):
            validate_input(data, row, altered)

    def test_export_original_only_and_retained_bytes_guards(self):
        def load(name, path):
            spec = importlib.util.spec_from_file_location(name, path)
            result = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(result)
            return result
        acquire = load("acquire_export_driver_test", ROOT / "scripts/reference/cache/compare_acquire.py")
        with patch.dict(sys.modules, {"compare_acquire": acquire}):
            exporter = load("acquire_export_test", ROOT / "scripts/reference/cache/export_acquire.py")
        report = {"selected_cases": [0, 1, 2], "client/server": "pass", "cases": []}
        for index, (side, mode, scenario) in enumerate(CASES):
            report["cases"].append({"index": index, "side": side, "mode": mode, "scenario": scenario, "comparisons": {},
                "control_equal": True, "operations": len(PLANS[scenario]), "queries": 2048 if index < 2 else 1024,
                "controller": {"returncode": 0, "timed_out": False, "stop_reason": None},
                "control": {"returncode": 0, "timed_out": False, "stop_reason": None}})
        exporter.validate_capture_cases(report)
        for mode in ("selection", "duplicate", "schedule", "comparison", "failed_control", "c_output"):
            value = copy.deepcopy(report)
            if mode == "selection": value["selected_cases"].pop()
            elif mode == "duplicate": value["cases"][2] = value["cases"][0]
            elif mode == "schedule": value["cases"][0]["operations"] -= 1
            elif mode == "comparison": value["cases"][0]["comparisons"] = {"C/client": "pass"}
            elif mode == "failed_control": value["cases"][0]["control"]["returncode"] = 1
            else: value["cases"][0]["c_record"] = {}
            with self.subTest(mode=mode), self.assertRaises(acquire.Failure):
                exporter.validate_capture_cases(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "record"
            path.write_bytes(b"original record")
            record = {"bytes": 15, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            self.assertEqual(exporter.checked_bytes(path, record), b"original record")
            path.write_bytes(b"corrupt! record")
            with self.assertRaises(acquire.Failure):
                exporter.checked_bytes(path, record)
            manifest = Path(directory) / "manifest.json"
            for mode in ("failed", "three_way", "errors"):
                value = {"schema": 1, "status": "pass", "errors": [], "mode": "original-only"}
                if mode == "failed": value["status"] = "fail"
                elif mode == "three_way": value["mode"] = "live-original-three-way"
                else: value["errors"].append("original comparison failed")
                manifest.write_text(json.dumps(value), encoding="utf-8")
                with self.subTest(mode=mode), self.assertRaisesRegex(acquire.Failure, "successful original-only capture required"):
                    exporter.export(manifest)


def replay(executable, raw, *, invalid=False):
    # A single child retains provider, cache, queue, and all returned identities for the whole schedule.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        process = subprocess.run([str(executable)], input=raw, stdout=stdout, stderr=stderr, timeout=90, check=False)
        require(stdout.tell() <= MAX_EXPECTED and stderr.tell() <= MAX_ORIGINAL, "bounded C replay streams")
        stdout.seek(0)
        stderr.seek(0)
        output, errors = stdout.read(), stderr.read()
    if invalid:
        require(process.returncode == 2 and not output, "malformed MCA1 must return2 without stdout")
        return None
    require(process.returncode == 0 and not errors, "complete C acquisition replay failed")
    return strict_json(output)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, default=FIXTURE)
    args = parser.parse_args(argv)
    _, rows = load_fixture(args.fixtures)
    executable = args.executable.resolve(strict=True)
    for row, data, expected in rows:
        require(replay(executable, data) == expected, "complete original/C acquisition mismatch: case " + str(row["index"]))
    row, data, _ = rows[0]
    offset = validate_input(data, row)["operations_offset"]
    for changed in (b"BAD1" + data[4:], data[:-1], data + b"x", data[:12] + b"\x02" + data[13:],
                    data[:offset] + struct.pack(">I", 4) + data[offset + 4:]):
        replay(executable, changed, invalid=True)
    print("cache acquisition fixtures: 3 continuous original inputs, 20 operations, 5 generated chunks, 5120 exact height queries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
