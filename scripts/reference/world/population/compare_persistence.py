#!/usr/bin/env python3
"""Compare actual populated GAME Chunk saves and direct region readbacks.

Each implementation constructs, populates and saves its own World. Complete
source observations supply the expected arrays, typed NBT and cache boundaries.
Per-run session and timestamp inputs remain measured, unmodified clock inputs.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[4]


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    if spec is None or spec.loader is None:
        raise RuntimeError("missing persistence comparison support: " + relative)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


C = load("_population_persistence_natural", "scripts/reference/world/population/compare_natural.py")
STORAGE = load("_population_persistence_storage", "scripts/reference/storage/compare_world_storage.py")
HASHES, SEED = C.HASHES, C.SEED
PROFILE = "save-readback"
SOURCES = (*C.SOURCES, "tests/reference/world/population/PopulationPersistenceObservation.java")
COORDINATES = ((0, 0), (1, 0), (0, 1), (1, 1))
TIME = "00000000000004d2"
REGION = "world/region/r.0.0.mcr"
SESSION = "world/session.lock"
INFO_ROLES = ("seed", "spawn_x", "spawn_y", "spawn_z", "time", "last_played", "size_on_disk",
              "player", "player_dimension", "level_name", "version", "raining", "rain_time",
              "thundering", "thunder_time")
LONGS = ("seed", "time", "last_played", "size_on_disk")
ARRAYS = {"blocks": 32768, "metadata": 16384, "skylight": 16384, "block_light": 16384, "heightmap": 256}
require, equal = C.require, C.equal


def keys(value, names, label):
    require(type(value) is dict and set(value) == set(names), label + ": complete keys")


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high, label + ": integer bounds")
    return value


def raw_hex(value, size, label):
    require(type(value) is str and len(value) == size * 2 and re.fullmatch(r"[0-9a-f]*", value) is not None,
            label + ": complete canonical hex bytes")
    return bytes.fromhex(value)


def epoch(value):
    require(type(value) is list and len(value) == 2, "measured two-endpoint epoch interval")
    low, high = [integer(item, 0, (1 << 63) - 1, "epoch milliseconds") for item in value]
    require(low <= high, "nondecreasing measured epoch interval")
    return low, high


def byte_array(value, count, label):
    keys(value, ("length", "values"), label)
    equal(integer(value["length"], 0, 32768, label), count, label + " length")
    return raw_hex(value["values"], count, label)


def copied_random(value, *, original):
    """Bind the eight copied nextLong bits to this actual captured 48-bit seed."""
    result = C.original_random_value(value) if original else C.random_value(value)
    seed, continuation = int(result["seed48"], 16), []
    for _ in range(8):
        seed = (seed * 0x5DEECE66D + 11) & ((1 << 48) - 1)
        high = seed >> 16
        seed = (seed * 0x5DEECE66D + 11) & ((1 << 48) - 1)
        low = seed >> 16
        if low >= 1 << 31:
            low -= 1 << 32
        continuation.append(f"{((high << 32) + low) & ((1 << 64) - 1):016x}")
    equal(result["next_longs"], continuation, "copied nextLong continuation derives from actual captured seed")
    return result


def info(value, side, *, original):
    if original:
        keys(value, ("owner", "class", "fields"), "original Info")
        owner = "ei" if side == "client" else "ct"
        compound = "nu" if side == "client" else "iq"
        equal(value["class"], owner, "actual original Info class")
        require(type(value["fields"]) is list and len(value["fields"]) == 15, "all fifteen Info fields")
        result = {}
        descriptors = ("J", "I", "I", "I", "J", "J", "J", "L" + compound + ";", "I",
                       "Ljava/lang/String;", "I", "Z", "I", "Z", "I")
        for index, (row, role, descriptor) in enumerate(zip(value["fields"], INFO_ROLES, descriptors)):
            keys(row, ("field", "role", "descriptor", "modifiers", "value"), "original Info field")
            equal({name: row[name] for name in ("field", "role", "descriptor", "modifiers")},
                  {"field": chr(ord("a") + index), "role": role, "descriptor": descriptor, "modifiers": 2},
                  "exact original Info field binding")
            result[role] = row["value"]
        require(type(result["level_name"]) is str, "original level-name String")
        result["level_name_utf16"] = result.pop("level_name").encode("utf-16-be", errors="surrogatepass").hex()
        equal(result["player"], {"ref": None, "class": None}, "empty-player original Info domain")
        result["player"] = None
        require(value["owner"] == {"ref": value["owner"].get("ref"), "class": owner} and
                type(value["owner"]["ref"]) is str and re.fullmatch(r"r[0-9]+", value["owner"]["ref"]),
                "actual original Info owner")
    else:
        keys(value, ("owner", *[name if name != "level_name" else "level_name_utf16" for name in INFO_ROLES]),
             "native Info")
        integer(value["owner"], 0, (1 << 63) - 1, "actual native Info owner tag")
        result = {name: item for name, item in value.items() if name != "owner"}
    for name in LONGS:
        raw_hex(result[name], 8, "Info " + name)
    for name in ("spawn_x", "spawn_y", "spawn_z", "player_dimension", "version", "rain_time", "thunder_time"):
        integer(result[name], -(1 << 31), (1 << 31) - 1, "Info " + name)
    for name in ("raining", "thundering"):
        require(type(result[name]) is bool, "Info actual Boolean " + name)
    require(result["player"] is None, "empty-player native Info domain")
    require(type(result["level_name_utf16"]) is str and len(result["level_name_utf16"]) <= 65536 and
            len(result["level_name_utf16"]) % 4 == 0, "complete bounded UTF16 level name")
    raw_hex(result["level_name_utf16"], len(result["level_name_utf16"]) // 2, "UTF16 level name")
    return result


def disk(value, *, original):
    keys(value, ("complete", "entries", "files", "directories", "bytes"), "complete disk inventory")
    require(value["complete"] is True and type(value["entries"]) is list and 1 <= len(value["entries"]) <= 256,
            "complete bounded disk entries")
    result, files, directories, total = [], 0, 0, 0
    paths = {}
    for entry in value["entries"]:
        require(type(entry) is dict and entry.get("kind") in ("file", "directory"), "ordinary disk entry kind")
        names = {"path", "kind"} | ({"bytes", "hex"} | ({"sha256"} if original else set())
                                          if entry["kind"] == "file" else set())
        keys(entry, names, "complete disk entry")
        path = entry["path"]
        require(type(path) is str and path and path not in paths and len(path) <= 1024 and
                (path == "." or all(re.fullmatch(r"[A-Za-z0-9_.-]+", part) is not None and
                                    part not in (".", "..") for part in path.split("/"))),
                "unique ordinary relative disk path")
        require(path == "." or paths.get(path.rpartition("/")[0] or ".") == "directory",
                "disk path has inventoried parent")
        paths[path] = entry["kind"]
        projected = dict(entry)
        if entry["kind"] == "file":
            size = integer(entry["bytes"], 0, 4 * 1024 * 1024, "file byte budget")
            raw = raw_hex(entry["hex"], size, "complete raw file")
            digest = hashlib.sha256(raw).hexdigest()
            if original:
                equal(entry["sha256"], digest, "actual source file SHA256")
            projected["sha256"] = digest
            total += size
            files += 1
        else:
            directories += 1
        result.append(projected)
    equal([entry["path"] for entry in result], sorted(paths), "complete sorted inventory")
    equal(result[0], {"path": ".", "kind": "directory"}, "root directory inventoried")
    for name, expected in (("files", files), ("directories", directories), ("bytes", total)):
        integer(value[name], 0, 8 * 1024 * 1024, "inventory " + name)
        equal(value[name], expected, "actual inventory " + name)
    require(total <= 8 * 1024 * 1024, "complete snapshot file-byte budget")
    return {"complete": True, "entries": result, "files": files, "directories": directories, "bytes": total}


def disk_files(value):
    return {entry["path"]: bytes.fromhex(entry["hex"]) for entry in value["entries"] if entry["kind"] == "file"}


def session(value, interval, token=None):
    files = disk_files(value)
    require(SESSION in files and len(files[SESSION]) == 8, "complete actual session.lock long")
    actual = struct.unpack(">q", files[SESSION])[0]
    low, high = epoch(interval)
    require(low <= actual <= high, "session token is an actual per-run constructor clock input")
    if token is not None:
        equal(files[SESSION], raw_hex(token, 8, "held native session token"), "actual Store session token")


def canonical_nbt(value):
    """Compound entry order is not semantic; retain every type, name and value."""
    keys(value, ("type", "name_utf16", "value"), "complete typed NBT tag")
    kind = integer(value["type"], 1, 10, "NBT tag type")
    result = dict(value)
    if kind == 10:
        require(type(value["value"]) is list, "actual NBT Compound children")
        children = [canonical_nbt(item) for item in value["value"]]
        names = [item["name_utf16"] for item in children]
        require(len(set(names)) == len(names), "distinct NBT Compound names")
        result["value"] = sorted(children, key=lambda item: item["name_utf16"])
    return result


def region_records(value, interval, *, saved):
    files = disk_files(value)
    equal(set(files), {REGION, SESSION}, "complete constructor/save file set")
    raw = files[REGION]
    equal(len(raw), 24576 if saved else 8192, "actual region allocation length")
    if not saved:
        equal(raw, bytes(8192), "fresh complete empty region header")
        return []
    low, high = epoch(interval)
    locations = [struct.unpack_from(">I", raw, index * 4)[0] for index in range(1024)]
    equal({index for index, location in enumerate(locations) if location}, {0, 1, 32, 33}, "all occupied region slots")
    allocations, result = set(), []
    for index, location in enumerate(locations):
        timestamp = struct.unpack_from(">I", raw, 4096 + index * 4)[0]
        if not location:
            equal(timestamp, 0, "untouched empty-slot timestamp")
            continue
        require(low // 1000 <= timestamp <= high // 1000, "region timestamp in actual save interval")
        start, count = (location >> 8) * 4096, location & 255
        require(start >= 8192 and count and start + count * 4096 <= len(raw), "complete region sector allocation")
        sectors = set(range(start // 4096, start // 4096 + count))
        require(not allocations & sectors, "distinct occupied region sectors")
        allocations.update(sectors)
        length = struct.unpack_from(">I", raw, start)[0]
        require(1 < length and length + 4 <= count * 4096 and raw[start + 4] in (1, 2),
                "complete compressed region record")
        stream = zlib.decompressobj(31 if raw[start + 4] == 1 else 15)
        plain = stream.decompress(raw[start + 5:start + 4 + length], 2 * 1024 * 1024 + 1)
        require(stream.eof and not stream.unused_data and not stream.unconsumed_tail and
                0 < len(plain) <= 2 * 1024 * 1024, "complete bounded compressed NBT stream")
        equal(raw[start + 4 + length:start + count * 4096], bytes(count * 4096 - 4 - length),
              "complete initial sector padding")
        result.append({"slot": index, "sector": start // 4096, "sectors": count,
                       "compression": raw[start + 4], "tree": canonical_nbt(STORAGE.nbt(plain))})
    equal(allocations, set(range(2, len(raw) // 4096)), "complete allocated-sector inventory")
    return result


def original_bindings(value, side, jar):
    client = side == "client"
    world, cache, loader, normalizer = ("fd", "ok", "ld", "vi") if client else ("dj", "jb", "hb", "nn")
    expected = {}
    for role, owner, name, descriptor, modifiers in (
            ("world_time_set", world, "a", "(J)V", 1),
            ("world_time_get", world, "t" if client else "m", "()J", 1),
            ("cache_save", cache, "a", "(ZL" + ("yb" if client else "pj") + ";)Z", 1),
            ("region_load", loader, "a", "(L" + world + ";II)L" + ("lm" if client else "hi") + ";", 1),
            ("block_normalizer", normalizer, "a", "([B)V", 9)):
        expected[role] = {"class": owner, "method": name, "descriptor": descriptor, "modifiers": modifiers}
    equal(value["bindings"], expected, "exact original save/readback method bindings")
    origins = value["class_origins"]
    names = {world, cache, loader, normalizer, "ei" if client else "ct", "nu" if client else "iq", "yb" if client else "pj"}
    keys(origins, names, "complete original persistence class origins")
    equal(set(origins.values()), {jar.resolve().as_uri().replace("file:///", "file:/", 1)}, "all persistence classes from supplied archive")


def all_boundaries(value):
    return [value["initial"], value["time_api"]["before"], value["time_api"]["after"],
            *[row[name] for row in value["cache_saves"] for name in ("before", "after")],
            *[row[name] for row in value["direct_loads"] for name in ("before", "after")]]


def loaded_chunk(value, number, side, world, acquire):
    keys(value, ("object", "world", "coordinates", "flags", "time", "data", "array_references", "tile_map", "entity_slices"),
         "complete original direct-load Chunk")
    equal(value["object"]["class"], "lm" if side == "client" else "hi", "ordinary loaded Chunk class")
    require(acquire.ref(value["object"]) is not None, "actual loaded Chunk owner")
    equal(value["world"], world, "loaded Chunk belongs to actual held World")
    require(type(value["coordinates"]) is list and len(value["coordinates"]) == 2 and
            all(type(item) is int for item in value["coordinates"]), "actual loaded Chunk coordinates")
    keys(value["flags"], ("c", "n", "o", "p", "q"), "all direct-load Chunk flags")
    require(all(type(item) is bool for item in value["flags"].values()), "actual direct-load Boolean flags")
    raw_hex(value["time"], 8, "loaded Chunk time")
    keys(value["data"], (*ARRAYS, "minimum_height"), "complete loaded Chunk data")
    for name, size in ARRAYS.items():
        byte_array(value["data"][name], size, "loaded " + name)
    integer(value["data"]["minimum_height"], -(1 << 31), (1 << 31) - 1, "loaded minimum height")
    require(type(value["array_references"]) is list and len(value["array_references"]) == 8 and
            len({acquire.ref(item) for item in value["array_references"]}) == 8 and
            all(acquire.ref(item) is not None for item in value["array_references"]), "all fresh loaded backing/wrapper owners")
    keys(value["tile_map"], ("object", "size"), "loaded Tile map")
    equal(value["tile_map"]["size"], 0, "empty loaded Tile map")
    equal(value["tile_map"]["object"]["class"], "java.util.HashMap", "actual loaded Tile HashMap")
    require(type(value["entity_slices"]) is list and len(value["entity_slices"]) == 8, "eight loaded Entity slices")
    owners = [acquire.ref(value["tile_map"]["object"])]
    for row in value["entity_slices"]:
        keys(row, ("object", "size"), "loaded Entity slice")
        equal(row["size"], 0, "empty loaded Entity slice")
        equal(row["object"]["class"], "java.util.ArrayList", "actual loaded Entity ArrayList")
        owners.append(acquire.ref(row["object"]))
    require(None not in owners and len(set(owners)) == 9, "distinct actual loaded collection owners")
    return {"id": number, "coordinates": value["coordinates"], "blank": False,
            "flags": value["flags"], "time": value["time"], "data": value["data"]}


def project_original(value, side, jar):
    """Validate a complete original-only record and derive its expected state."""
    keys(value, ("schema", "scope", "profile", "side", "seed_hex", "jar_sha256", "construction_epoch_ms", "population", "persistence"),
         "complete original save/readback wrapper")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["scope"] == "original-game-population-save-readback"
            and value["profile"] == PROFILE and value["side"] == side and side in HASHES and
            value["seed_hex"] == SEED and value["jar_sha256"] == HASHES[side], "fixed original save/readback identity")
    construction = epoch(value["construction_epoch_ms"])
    population = C.validate_original_observation(value["population"], side, "origin", jar)
    p = value["persistence"]
    keys(p, ("schema", "scope", "full_world_save", "unload", "world_restart", "bindings", "class_origins", "initial",
             "time_api", "cache_saves", "direct_loads", "block_map", "limits"), "complete original persistence record")
    require(type(p["schema"]) is int and p["schema"] == 1 and p["scope"] == "original-game-population-chunk-save-readback" and
            all(p[name] is False for name in ("full_world_save", "unload", "world_restart")), "finite cache-save/direct-readback scope")
    original_bindings(p, side, jar)
    require(type(p["cache_saves"]) is list and len(p["cache_saves"]) == 2 and
            type(p["direct_loads"]) is list and len(p["direct_loads"]) == 4, "complete two-save/four-read schedule")
    _, _, finish, _, acquire, _ = C.PUBLIC.driver.helpers()
    callbacks = value["population"]["callbacks"]
    ids = acquire.identities(callbacks)
    for operation in callbacks["operations"]:
        for row in operation["after"]["world"]["insertion_list"]["entries"]:
            if row["value"] not in ids:
                ids[row["value"]] = len(ids)
    last_raw = callbacks["operations"][-1]["after"]
    projected, retained = [], 0
    boundaries = all_boundaries(p)
    require(len(boundaries) == 15, "all fifteen persistence boundaries")
    for index, boundary in enumerate(boundaries):
        keys(boundary, ("cache", "numerical", "info", "disk"), "complete original persistence boundary")
        C.PUBLIC.driver.retained_snapshot_owners(boundary["cache"], last_raw, acquire)
        cache = C.PUBLIC.driver.snapshot_projection(boundary["cache"], ids, acquire, finish)
        equal(boundary["numerical"], value["population"]["states"][-1], "original numerical state retained boundary " + str(index))
        if index:
            equal(boundary["info"]["owner"], boundaries[0]["info"]["owner"], "actual original Info owner retained")
        actual_info = info(boundary["info"], side, original=True)
        actual_disk = disk(boundary["disk"], original=True)
        session(actual_disk, list(construction))
        retained += actual_disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "cumulative retained raw file budget")
        projected.append({"cache": cache, "info": actual_info, "disk": actual_disk, "numerical": boundary["numerical"]})
    equal(projected[0]["cache"], population["operations"][-1]["snapshot"], "unmodified population/persistence join")
    initial_info = projected[0]["info"]
    equal(initial_info, {"seed": SEED, "spawn_x": 0, "spawn_y": 64, "spawn_z": 0,
                        "time": "0000000000000000", "last_played": "0000000000000000",
                        "size_on_disk": "0000000000000000", "player": None, "player_dimension": 0,
                        "level_name_utf16": "cache-callback-reference".encode("utf-16-be").hex(), "version": 0,
                        "raining": False, "rain_time": 0, "thundering": False, "thunder_time": 0}, "actual seed23 initial Info")
    time_api = p["time_api"]
    keys(time_api, ("binding", "receiver", "value", "previous", "epoch_ms", "before", "after"), "actual time API event")
    equal(time_api["binding"], p["bindings"]["world_time_set"], "actual time setter binding")
    equal(time_api["receiver"], last_raw["world"]["world"], "actual held World time receiver")
    equal(time_api["value"], 1234, "time setter argument")
    require(type(time_api["value"]) is int, "actual time argument integer")
    equal(time_api["previous"], initial_info["time"], "actual prior World time")
    epoch(time_api["epoch_ms"])
    equal(projected[1], projected[0], "continuous original time entry")
    after_time = copy.deepcopy(projected[0])
    after_time["cache"]["context"]["world_time"] = TIME
    after_time["info"]["time"] = TIME
    equal(projected[2], after_time, "complete actual time API effects")
    region_records(projected[0]["disk"], time_api["epoch_ms"], saved=False)
    for index, row in enumerate(p["cache_saves"]):
        keys(row, ("index", "binding", "receiver", "force", "progress", "completed", "epoch_ms", "before", "after"), "actual forced cache-save event")
        equal(row["index"], index, "save index")
        require(type(row["index"]) is int and row["force"] is True and row["progress"] is None and row["completed"] is True,
                "actual completed forced cache save")
        equal(row["binding"], p["bindings"]["cache_save"], "actual cache-save binding")
        equal(row["receiver"], last_raw["world"]["cache"], "actual cache save receiver")
        epoch(row["epoch_ms"])
    equal(projected[3], projected[2], "continuous first save entry")
    after_save = copy.deepcopy(projected[3])
    for chunk in after_save["cache"]["world"]["chunks"]:
        if not chunk["blank"]:
            chunk["flags"].update(o=False, q=False)
            chunk["time"] = TIME
    after_save["info"]["size_on_disk"] = "0000000000006000"
    after_save["disk"] = projected[4]["disk"]
    equal(projected[4], after_save, "complete first-save effects")
    equal(disk_files(projected[3]["disk"])[SESSION], disk_files(projected[4]["disk"])[SESSION], "session bytes retained by saves")
    equal(projected[5], projected[4], "continuous second save entry")
    equal(projected[6], projected[4], "second forced save preserves all state and bytes")
    records = region_records(projected[4]["disk"], p["cache_saves"][0]["epoch_ms"], saved=True)
    block_map = p["block_map"]
    keys(block_map, ("class", "field", "descriptor", "modifiers", "owner", "length", "bytes", "sha256", "values", "first_observation"), "actual original normalization map")
    equal({name: block_map[name] for name in ("class", "field", "descriptor", "modifiers", "length", "bytes", "first_observation")},
          {"class": "vi" if side == "client" else "nn", "field": "a", "descriptor": "[B", "modifiers": 10,
           "length": 256, "bytes": 256, "first_observation": "after-first-genuine-loader-return; no earlier observer active use"},
          "late observed source normalization field")
    mapping = raw_hex(block_map["values"], 256, "all actual normalization-map bytes")
    equal(hashlib.sha256(mapping).hexdigest(), block_map["sha256"], "actual normalization-map SHA256")
    require(acquire.ref(block_map["owner"]) is not None and block_map["owner"]["class"] == "[B" and
            mapping[0] == 0 and all(value in (0, index) for index, value in enumerate(mapping)), "actual source block-ID normalizer")
    cached = {tuple(chunk["coordinates"]): chunk for chunk in projected[3]["cache"]["world"]["chunks"] if not chunk["blank"]}
    arrays = {acquire.ref(item) for chunk in last_raw["cached_locals"] for item in chunk["array_references"]}
    loaded = []
    for index, (row, coordinates) in enumerate(zip(p["direct_loads"], COORDINATES)):
        keys(row, ("index", "binding", "receiver", "world", "coordinates", "epoch_ms", "result", "chunk", "normalization_map", "before", "after"), "actual direct-load event")
        equal(row["index"], index, "direct-load index")
        require(type(row["index"]) is int, "actual direct-load index integer")
        equal(row["coordinates"], list(coordinates), "direct-load requested coordinates")
        equal(row["binding"], p["bindings"]["region_load"], "actual direct-load binding")
        equal(row["receiver"], last_raw["world"]["loader"], "actual held region loader")
        equal(row["world"], last_raw["world"]["world"], "actual held World loader argument")
        epoch(row["epoch_ms"])
        equal(row["normalization_map"], block_map, "same actual retained normalization map")
        result = acquire.ref(row["result"])
        require(result not in ids and result is not None, "fresh unpublished actual direct-load result")
        ids[result] = len(ids)
        equal(row["chunk"]["object"], row["result"], "complete actual returned Chunk")
        chunk = loaded_chunk(row["chunk"], ids[result], side, row["world"], acquire)
        backing = {acquire.ref(item) for item in row["chunk"]["array_references"]}
        require(not backing & arrays, "fresh loaded backing and wrapper owners")
        arrays.update(backing)
        expected = copy.deepcopy(cached[coordinates])
        expected.update(id=ids[result], time="0000000000000000")
        expected["flags"] = {"c": False, "n": expected["flags"]["n"], "o": False, "p": False, "q": False}
        expected["data"]["blocks"]["values"] = bytes(mapping[byte] for byte in bytes.fromhex(expected["data"]["blocks"]["values"])).hex()
        expected["data"]["minimum_height"] = 0
        equal(chunk, expected, "actual normalized direct-load Chunk " + str(index))
        for boundary in projected[7 + index * 2:9 + index * 2]:
            equal(boundary, projected[4], "direct read preserves cache/Info/numerical state and all disk bytes")
        loaded.append(chunk)
    for record in records:
        tree = record["tree"]
        equal((tree["type"], tree["name_utf16"], len(tree["value"])), (10, "", 1), "exact root Compound")
        level = STORAGE.level.child(tree, "Level")
        equal((level["type"], len(level["value"])), (10, 11), "all eleven Level tags")
        coordinates = (record["slot"] & 31, record["slot"] >> 5)
        saved = cached[coordinates]
        for name, kind, expected in (("xPos", 3, coordinates[0]), ("zPos", 3, coordinates[1]),
                                     ("LastUpdate", 4, 1234), ("TerrainPopulated", 1, int(saved["flags"]["n"]))):
            child = STORAGE.level.child(level, name)
            equal((child["type"], child["value"]), (kind, expected), "actual saved typed " + name)
            require(type(child["value"]) is int, "actual NBT integer " + name)
        for name, role in (("Blocks", "blocks"), ("Data", "metadata"), ("SkyLight", "skylight"),
                           ("BlockLight", "block_light"), ("HeightMap", "heightmap")):
            child = STORAGE.level.child(level, name)
            equal((child["type"], child["value"]), (7, saved["data"][role]["values"]), "complete saved " + name)
        for name in ("Entities", "TileEntities"):
            child = STORAGE.level.child(level, name)
            equal((child["type"], child["value"]), (9, {"element_type": 1, "items": []}), "actual empty on-disk " + name)
    equal(p["limits"], {"output_bytes": C.MAX_BYTES, "inventory_entries": 256, "file_bytes": 4 * 1024 * 1024,
                        "snapshot_file_bytes": 8 * 1024 * 1024, "retained_file_bytes": 8 * 1024 * 1024,
                        "actual_retained_file_bytes": retained}, "complete source observer budgets")
    world_random = None
    for state in [*value["population"]["states"], *[boundary["numerical"] for boundary in projected]]:
        for name in ("generator_random", "cave_random"):
            copied_random(state["provider"][name], original=True)
        actual = copied_random(state["aux"]["world_random"], original=True)
        if world_random is None:
            world_random = actual
        equal(actual, world_random, "actual original clock-seeded World.r stays unchanged")
    return {"population": population, "boundaries": projected, "direct_loads": loaded,
            "block_map": {"length": 256, "values": block_map["values"]}, "records": records}


def compare_original(native, original, public, jar):
    expected = project_original(original, native.get("side"), jar)
    keys(native, ("schema", "scope", "profile", "side", "seed_hex", "construction_epoch_ms", "population", "persistence"),
         "complete native save/readback wrapper")
    require(type(native["schema"]) is int and native["schema"] == 1 and native["scope"] == "native-game-population-save-readback"
            and native["profile"] == PROFILE and native["side"] == original["side"] and native["seed_hex"] == SEED,
            "requested native save/readback profile")
    epoch(native["construction_epoch_ms"])
    keys(native["population"], ("schema", "scope", "side", "seed_hex", "initial", "operations", "failure"),
         "complete native origin prefix")
    for row in native["population"]["operations"]:
        keys(row, ("index", "method", "coordinates", "result", "status", "snapshot"), "complete native prefix operation")
    C.compare_original(native["population"], original["population"], public, jar, profile="origin")
    p = native["persistence"]
    keys(p, ("initial", "time_api", "cache_saves", "direct_loads", "block_map", "session_token", "failure"), "complete native persistence")
    require(p["failure"] is None, "native persistence producer failure")
    require(type(p["cache_saves"]) is list and len(p["cache_saves"]) == 2 and
            type(p["direct_loads"]) is list and len(p["direct_loads"]) == 4, "complete native persistence schedule")
    byte_array(p["block_map"], 256, "actual Registry-derived native block map")
    equal(p["block_map"], expected["block_map"], "actual source/Registry normalization map")
    boundaries = all_boundaries(p)
    snapshots = [native["population"]["initial"], *[row["snapshot"] for row in native["population"]["operations"]]]
    states = list(original["population"]["states"])
    native_disks = []
    retained = 0
    for index, (boundary, reference) in enumerate(zip(boundaries, expected["boundaries"])):
        keys(boundary, ("snapshot", "info", "disk"), "complete native persistence boundary")
        snapshot = boundary["snapshot"]
        equal({name: snapshot[name] for name in ("context", "world")}, reference["cache"], "complete native cache boundary " + str(index))
        equal(info(boundary["info"], native["side"], original=False), reference["info"], "all native Info fields boundary " + str(index))
        equal(boundary["info"]["owner"], boundaries[0]["info"]["owner"], "actual native Info owner retained")
        actual_disk = disk(boundary["disk"], original=False)
        session(actual_disk, native["construction_epoch_ms"], p["session_token"])
        retained += actual_disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "native cumulative raw-file budget")
        equal([{name: item[name] for name in ("path", "kind")} for item in actual_disk["entries"]],
              [{name: item[name] for name in ("path", "kind")} for item in reference["disk"]["entries"]],
              "complete source/native file and directory inventory")
        equal({name: actual_disk[name] for name in ("files", "directories", "bytes")},
              {name: reference["disk"][name] for name in ("files", "directories", "bytes")}, "complete native disk counts")
        native_disks.append(actual_disk)
        snapshots.append(snapshot)
        states.append(reference["numerical"])
    for snapshot in snapshots:
        keys(snapshot, ("context", "world", "provider", "shared"), "complete native full snapshot")
        provider = snapshot["provider"]
        keys(provider, ("generator_random", "cave_random", "world_random", "generation", "v", "manager", "w"),
             "complete native numerical provider")
        keys(provider["generation"], ("q", "d", "e", "f", "g", "h", "r", "s", "t"), "all native generation arrays")
        keys(provider["manager"], ("a", "b", "c", "d"), "all native manager arrays")
        keys(snapshot["shared"], ("drain_depth", "chunk_positive_sky", "falling"), "complete native shared state")
        for name in ("generator_random", "cave_random", "world_random"):
            copied_random(provider[name], original=False)
    C.compare_numerical_snapshots(snapshots, states)
    for index in (1, 2, 3):
        equal(native_disks[index], native_disks[0], "time setter preserves actual native disk bytes")
    for index in range(5, len(native_disks)):
        equal(native_disks[index], native_disks[4], "second save and direct reads preserve all actual native disk bytes")
    equal(disk_files(native_disks[0])[SESSION], disk_files(native_disks[4])[SESSION], "native saves preserve session bytes")
    region_records(native_disks[0], p["time_api"]["epoch_ms"], saved=False)
    equal(region_records(native_disks[4], p["cache_saves"][0]["epoch_ms"], saved=True), expected["records"],
          "complete typed source/native saved NBT and sector allocation")
    time_api = p["time_api"]
    keys(time_api, ("value", "previous", "epoch_ms", "before", "after", "status", "native_status"), "complete native time API event")
    equal({name: time_api[name] for name in ("value", "previous", "status", "native_status")},
          {"value": 1234, "previous": "0000000000000000", "status": "ok", "native_status": 0}, "actual native time setter result")
    epoch(time_api["epoch_ms"])
    for index, row in enumerate(p["cache_saves"]):
        keys(row, ("index", "force", "progress", "completed", "epoch_ms", "before", "after", "status", "native_status", "error"), "complete native cache-save result")
        equal({name: row[name] for name in ("index", "force", "progress", "completed", "status", "native_status", "error")},
              {"index": index, "force": True, "progress": None, "completed": True, "status": "ok", "native_status": 0, "error": None},
              "actual completed native cache-save result")
        epoch(row["epoch_ms"])
    for index, (row, chunk) in enumerate(zip(p["direct_loads"], expected["direct_loads"])):
        keys(row, ("index", "coordinates", "epoch_ms", "result", "chunk", "normalization_map", "before", "after", "status", "native_status", "error"), "complete native direct-load result")
        equal({name: row[name] for name in ("index", "coordinates", "result", "chunk", "normalization_map", "status", "native_status", "error")},
              {"index": index, "coordinates": list(COORDINATES[index]), "result": chunk["id"], "chunk": chunk,
               "normalization_map": expected["block_map"], "status": "ok", "native_status": 0, "error": None},
              "complete fresh native direct-load result " + str(index))
        epoch(row["epoch_ms"])
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed23 actual populated GAME Chunk save/direct-readback", "profile": PROFILE,
              "passed": False, "tested_commit": args.tested_commit or None, "full_world_save": False,
              "world_restart": False, "unload": False, "whole_population_equivalence": False,
              "processes": [], "sides": {}}
    try:
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "complete tested commit")
        require(not any(os.environ.get(name) for name in
                        ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")), "uninjected original Java environment")
        tools = {name: getattr(args, name).resolve() for name in ("executable", "java", "javac")}
        jars = {side: getattr(args, side).resolve() for side in HASHES}
        require(all(path.is_file() and not path.is_symlink() for path in (*tools.values(), *jars.values())), "ordinary selected tools/JARs")
        for side, jar in jars.items():
            equal(C.CAPTURE.file_fingerprint(jar)["sha256"], HASHES[side], "pinned supplied original " + side)
        work = C.CAPTURE.capture_root(args.work_dir, [sys.executable, tools["java"], tools["javac"]])
        require(not tools["executable"].is_relative_to(work), "capture excludes selected executable")
        report.update(C.source_context(work, report["processes"]))
        require(not args.tested_commit or args.tested_commit == report["source_head"] and report["source_dirty"] is False,
                "tested commit requires matching clean source")
        paths = C.source_inputs(**tools, jars=jars)
        paths["persistence_driver"] = Path(__file__).resolve()
        paths["storage_transport_validator"] = ROOT / "tests/tools/storage/test_store.py"
        report["inputs"] = {name: {"path": str(path), **C.CAPTURE.file_fingerprint(path)} for name, path in paths.items()}
        report["python_version"] = sys.version
        classes = work / "classes"
        classes.mkdir()
        C.run([str(tools["javac"]), "--release", "17", "-d", str(classes), *[str(ROOT / source) for source in SOURCES]], work, "compile", report["processes"])
        report["helper_classes"] = C.file_inventory(classes)
        _, decoded = C.PUBLIC.load_fixture()
        public = {row["record"]["side"]: row["expected"] for row in decoded if row["record"]["scenario"] == "population"}
        for side, jar in jars.items():
            directory = work / side
            directory.mkdir()
            resources = directory / "resources"
            row = {"resources": C.extract_resources(jar, resources), "passed": False}
            report["sides"][side] = row
            original_path, output = directory / "original.json", directory / "native.json"
            C.run([str(tools["java"]), "-Xint", "-Xmx512m", "--add-opens", "java.base/java.util=ALL-UNNAMED", "-cp", str(classes),
                   "PopulationPersistenceObservation", str(jar), side, SEED, str(directory / "original-world"), str(original_path), PROFILE],
                  work, side + "-original", report["processes"])
            original = C.read_json(original_path)
            project_original(original, side, jar)
            C.run([str(tools["executable"]), side, str(resources).encode("utf-8").hex(), str(directory / "native-world").encode("utf-8").hex(),
                   str(output), PROFILE], work, side + "-native", report["processes"])
            native = C.read_json(output)
            equal((work / (side + "-original.stdout")).read_bytes(), b"", "original stdout")
            equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                  (work / (side + "-original.stderr")).read_bytes().replace(b"\r\n", b"\n"), "complete original/native bootstrap console")
            compare_original(native, original, public[side], jar)
            for name, fingerprint in row["resources"].items():
                equal(C.CAPTURE.file_fingerprint(resources / name), fingerprint, "original resource preserved: " + name)
            row.update(passed=True, forced_saves=2, direct_loads=4, complete_typed_nbt=True,
                       complete_disk_preservation=True, original_output=C.CAPTURE.file_fingerprint(original_path),
                       native_output=C.CAPTURE.file_fingerprint(output))
            print("PASS: " + side + " populated GAME Chunk save/direct-readback", flush=True)
        equal(C.file_inventory(classes), report["helper_classes"], "compiled helper classes preserved")
        for name, path in paths.items():
            equal({"path": str(path), **C.CAPTURE.file_fingerprint(path)}, report["inputs"][name], "comparison input preserved: " + name)
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
