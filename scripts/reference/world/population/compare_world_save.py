#!/usr/bin/env python3
"""Compare genuine seed23 held-World saves with complete original observations.

Both implementations populate their own World and perform two real full saves.
Metadata clocks are checked against each save's measured interval; complete typed
NBT, cache/Info/numerical state and within-run file rotation remain observable.
This finite successful profile does not qualify source I/O-failure behavior,
unloading, restart, nonempty Players or named SavedData.
"""

from __future__ import annotations

import argparse
import copy
from functools import lru_cache
import hashlib
import importlib.util
import itertools
import json
import os
from pathlib import Path
import re
import stat
import sys
import zlib


ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location(
    "_population_world_save_persistence", ROOT / "scripts/reference/world/population/compare_persistence.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing population save/readback comparison support")
P = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = P
SPEC.loader.exec_module(P)
C, require, equal = P.C, P.require, P.equal
load, keys, integer = P.load, P.keys, P.integer
disk, disk_files, session, epoch = P.disk, P.disk_files, P.session, P.epoch
byte_array, copied_random = P.byte_array, P.copied_random
HASHES, SEED, PROFILE = P.HASHES, P.SEED, "full-world-save"
SOURCES = (*P.SOURCES, "tests/reference/world/population/PopulationWorldSaveObservation.java")
TRANSPORT = P.load("_population_world_save_transport", "tests/tools/world/replay_population_persistence.py")
LEVEL = "world/level.dat"
OLD_LEVEL = "world/level.dat_old"


@lru_cache(maxsize=1)
def public_origins():
    _, rows = C.PUBLIC.load_fixture()
    return {row["record"]["side"]: row["expected"] for row in rows
            if row["record"]["scenario"] == "population"}


def all_boundaries(value):
    require(type(value.get("saves")) is list and len(value["saves"]) == 2,
            "complete two-save full World schedule")
    return [value["initial"], value["time_api"]["before"], value["time_api"]["after"],
            *[row[name] for row in value["saves"] for name in ("before", "after")]]


def reference(value, expected_class):
    P.keys(value, ("ref", "class"), "complete actual object reference")
    require(type(value["ref"]) is str and re.fullmatch(r"r[0-9]+", value["ref"]) is not None,
            "nonnull actual object identity")
    equal(value["class"], expected_class, "actual object class")
    return value["ref"]


def original_bindings(value, side, jar):
    client = side == "client"
    world, info, compound, progress, saved, storage, base, handler = (
        ("fd", "ei", "nu", "yb", "hc", "wt", "fm", "mx") if client else
        ("dj", "ct", "iq", "pj", "es", "om", "dq", "ie"))
    methods = {}
    for role, owner, name, descriptor, modifiers in (
            ("world_time_get", world, "t" if client else "m", "()J", 1),
            ("world_time_set", world, "a", "(J)V", 1),
            ("world_save", world, "a", "(ZL" + progress + ";)V", 1),
            ("world_metadata", world, "D" if client else "w", "()V", 2),
            ("world_session", world, "r" if client else "k", "()V", 1),
            ("handler_list_save", handler, "a", "(L" + info + ";Ljava/util/List;)V", 1),
            ("handler_base_list_save", base, "a", "(L" + info + ";Ljava/util/List;)V", 1),
            ("info_list", info, "a", "(Ljava/util/List;)L" + compound + ";", 1),
            ("info_common_writer", info, "a", "(L" + compound + ";L" + compound + ";)V", 2),
            ("saved_data_save", saved, "a", "()V", 1),
            ("info_version_set", info, "d" if client else "a", "(I)V", 1),
            ("region_close", "hi" if client else "ie", "c" if client else "e", "()V", 1)):
        methods[role] = {"class": owner, "method": name, "descriptor": descriptor, "modifiers": modifiers}
    equal(value["bindings"], methods, "exact actual original method selections")
    fields = {}
    for role, owner, name, descriptor, modifiers in (
            ("world_players", world, "d", "Ljava/util/List;", 1),
            ("world_saved_data", world, "z", "L" + saved + ";", 1),
            ("world_info", world, "x", "L" + info + ";", 4),
            ("world_storage", world, "w", "L" + storage + ";", 20),
            ("saved_storage", saved, "a", "L" + storage + ";", 2),
            ("saved_named_cache", saved, "b", "Ljava/util/Map;", 2),
            ("saved_ordered_list", saved, "c", "Ljava/util/List;", 2),
            ("saved_id_map", saved, "d", "Ljava/util/Map;", 2)):
        fields[role] = {"class": owner, "field": name, "descriptor": descriptor, "modifiers": modifiers}
    equal(value["field_bindings"], fields, "exact actual original field selections")
    names = {world, info, compound, progress, saved, storage, base, handler, "hi" if client else "ie"}
    equal(value["class_origins"], {name: jar.resolve().as_uri().replace("file:///", "file:/", 1)
                                   for name in names}, "all selected members from pinned original")


def empty_collection(value, expected_class):
    P.keys(value, ("object", "size", "entries"), "complete actual collection")
    identity = reference(value["object"], expected_class)
    equal(value["size"], 0, "actual empty collection size")
    equal(value["entries"], [], "complete actual empty collection entries")
    return identity


def collections(value, side, raw):
    P.keys(value, ("world", "players", "saved_data"), "complete held collection graph")
    reference(value["world"], "fd" if side == "client" else "dj")
    equal(value["world"], raw["world"]["world"], "same actual cache/collection World")
    saved = value["saved_data"]
    P.keys(saved, ("object", "storage", "named_cache", "ordered_list", "id_map"), "actual SavedData fields")
    manager = reference(saved["object"], "hc" if side == "client" else "es")
    reference(saved["storage"], "mx" if side == "client" else "ie")
    equal(saved["storage"], raw["world"]["storage"], "actual manager borrows actual World storage")
    owners = [empty_collection(value["players"], "java.util.ArrayList"),
              empty_collection(saved["named_cache"], "java.util.HashMap"),
              empty_collection(saved["ordered_list"], "java.util.ArrayList"),
              empty_collection(saved["id_map"], "java.util.HashMap")]
    require(len(set(owners + [manager, value["world"]["ref"], saved["storage"]["ref"]])) == 7,
            "distinct actual World/storage/manager and four collection owners")


def signed(hex_value):
    return int.from_bytes(P.raw_hex(hex_value, 8, "actual long bits"), "big", signed=True)


def metadata(raw, before, interval):
    require(type(raw) is bytes and 0 < len(raw) <= 65536, "bounded complete metadata file")
    stream = zlib.decompressobj(31)
    plain = stream.decompress(raw, 65537)
    require(stream.eof and not stream.unused_data and not stream.unconsumed_tail and
            0 < len(plain) <= 65536, "one complete bounded metadata gzip member")
    tree = P.canonical_nbt(P.STORAGE.nbt(plain))
    equal((tree["type"], tree["name_utf16"], len(tree["value"])), (10, "", 1), "exact metadata root")
    data = P.STORAGE.level.child(tree, "Data")
    require(data is not None, "actual Data compound")
    equal((data["type"], data["name_utf16"], len(data["value"])),
          (10, "Data".encode("utf-16-be").hex(), 13), "all thirteen metadata Data tags")
    children = {row["name_utf16"]: row for row in data["value"]}
    stamp = P.STORAGE.level.child(data, "LastPlayed")
    require(stamp is not None and stamp["type"] == 4, "actual typed LastPlayed")
    low, high = P.epoch(interval)
    P.integer(stamp["value"], low, high, "actual metadata serializer clock sample")
    expected = {
        "RandomSeed": (4, signed(before["seed"])),
        "SpawnX": (3, before["spawn_x"]), "SpawnY": (3, before["spawn_y"]),
        "SpawnZ": (3, before["spawn_z"]), "Time": (4, signed(before["time"])),
        "SizeOnDisk": (4, signed(before["size_on_disk"])), "LastPlayed": (4, stamp["value"]),
        "LevelName": (8, before["level_name_utf16"]), "version": (3, 19132),
        "raining": (1, int(before["raining"])), "rainTime": (3, before["rain_time"]),
        "thundering": (1, int(before["thundering"])), "thunderTime": (3, before["thunder_time"])}
    equal(set(children), {name.encode("utf-16-be").hex() for name in expected},
          "complete exact metadata names without Player")
    for name, (kind, value) in expected.items():
        equal(children[name.encode("utf-16-be").hex()],
              {"type": kind, "name_utf16": name.encode("utf-16-be").hex(), "value": value},
              "complete typed metadata " + name)
    return {"tree": tree, "gzip": {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
            "last_played": stamp["value"], "size_on_disk": signed(before["size_on_disk"]),
            "tags": len(expected), "player_present": False}


def filtered_region_disk(value):
    entries = [row for row in value["entries"]
               if row["kind"] == "directory" or row["path"] in (P.REGION, P.SESSION)]
    result = {"complete": True, "entries": entries,
              "files": sum(row["kind"] == "file" for row in entries),
              "directories": sum(row["kind"] == "directory" for row in entries),
              "bytes": sum(row.get("bytes", 0) for row in entries)}
    return P.disk(result, original=True)


def project_original(document, side, jar):
    """Validate a complete original-only full save and derive its expected state."""
    P.keys(document, ("schema", "scope", "profile", "side", "seed_hex", "jar_sha256",
                      "construction_epoch_ms", "population", "world_save"),
           "complete original full World save wrapper")
    require(type(side) is str and side in HASHES, "known original full-save side")
    equal({name: document[name] for name in ("schema", "scope", "profile", "side", "seed_hex", "jar_sha256")},
          {"schema": 1, "scope": "original-game-population-full-world-save", "profile": PROFILE,
           "side": side, "seed_hex": SEED, "jar_sha256": HASHES[side]}, "fixed original full-save profile")
    public = public_origins()[side]
    P.epoch(document["construction_epoch_ms"])
    prefix = C.validate_original_observation(document["population"], side, "origin", jar)
    require(type(document["population"]["states"]) is list and len(document["population"]["states"]) == 5,
            "all five unchanged origin numerical boundaries")
    equal(prefix["initial"], public["initial"], "unchanged public constructor projection")
    for row, expected in zip(prefix["operations"], public["operations"]):
        equal({name: row[name] for name in ("index", "method", "coordinates", "result", "status", "snapshot")},
              {name: expected[name] for name in ("index", "method", "coordinates", "result", "status", "snapshot")},
              "unchanged complete public origin acquisition")
    value = document["world_save"]
    P.keys(value, ("schema", "scope", "full_world_save", "unload", "world_restart", "bindings", "field_bindings",
                   "class_origins", "initial", "time_api", "saves", "limits"), "complete original full-save record")
    equal({name: value[name] for name in ("schema", "scope", "full_world_save", "unload", "world_restart")},
          {"schema": 1, "scope": "original-game-held-world-full-save", "full_world_save": True,
           "unload": False, "world_restart": False}, "held original full-save scope")
    original_bindings(value, side, jar)
    require(type(value["saves"]) is list and len(value["saves"]) == 2, "two actual full World saves")
    time = value["time_api"]
    P.keys(time, ("binding", "receiver", "value", "previous", "epoch_ms", "before", "after"), "complete time API event")
    equal(time["binding"], value["bindings"]["world_time_set"], "actual time method binding")
    equal((time["value"], time["previous"]), (1234, "0000000000000000"), "actual time argument and prior value")
    P.epoch(time["epoch_ms"])
    boundaries = [value["initial"], time["before"], time["after"],
                  *[row[name] for row in value["saves"] for name in ("before", "after")]]
    equal(len(boundaries), 7, "all seven actual full-save boundaries")
    _, _, finish, _, acquire, _ = C.PUBLIC.driver.helpers()
    callbacks = document["population"]["callbacks"]
    ids = acquire.identities(callbacks)
    for row in callbacks["operations"]:
        for entry in row["after"]["world"]["insertion_list"]["entries"]:
            if entry["value"] not in ids:
                ids[entry["value"]] = len(ids)
    raw_last = callbacks["operations"][-1]["after"]
    projected, states, retained = [], [], 0
    for boundary in boundaries:
        P.keys(boundary, ("state", "collections"), "complete measured boundary")
        state = boundary["state"]
        P.keys(state, ("cache", "numerical", "info", "disk"), "complete unchanged base boundary")
        C.PUBLIC.driver.retained_snapshot_owners(state["cache"], raw_last, acquire)
        collections(boundary["collections"], side, state["cache"])
        equal(boundary["collections"], boundaries[0]["collections"], "all actual collection owners and contents retained")
        equal(state["numerical"], document["population"]["states"][-1], "all original numerical values and aliases unchanged")
        provider, aux = state["numerical"]["provider"], state["numerical"]["aux"]
        for random in (provider["generator_random"], provider["cave_random"], aux["world_random"]):
            P.copied_random(random, original=True)
        equal(state["info"]["owner"], boundaries[0]["state"]["info"]["owner"], "actual same Info owner")
        disk = P.disk(state["disk"], original=True)
        P.session(disk, document["construction_epoch_ms"])
        retained += disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "complete retained raw-file budget")
        projected.append({"cache": C.PUBLIC.driver.snapshot_projection(state["cache"], ids, acquire, finish),
                          "info": P.info(state["info"], side, original=True), "disk": disk, "numerical": state["numerical"]})
        states.append(state["numerical"])
    numerical = document["population"]["states"] + states
    for state in numerical:
        for random in (state["provider"]["generator_random"], state["provider"]["cave_random"], state["aux"]["world_random"]):
            P.copied_random(random, original=True)
        equal(state["aux"]["world_random_attribution"], "per-run-constructor-input; never replaced or reseeded",
              "actual World.r input attribution")
        reference(state["aux"]["world_random_identity"], "java.util.Random")
        reference(state["aux"]["world_random_seed_identity"], "java.util.concurrent.atomic.AtomicLong")
        for name in ("world_random", "world_random_identity", "world_random_seed_identity"):
            equal(state["aux"][name], numerical[0]["aux"][name], "actual World.r state and seed owners retained")
    equal(projected[0]["cache"], prefix["operations"][-1]["snapshot"], "complete population/full-save join")
    equal(projected[1], projected[0], "continuous time entry")
    initial_info = projected[0]["info"]
    equal(initial_info, {"seed": SEED, "spawn_x": 0, "spawn_y": 64, "spawn_z": 0,
                         "time": "0000000000000000", "last_played": "0000000000000000",
                         "size_on_disk": "0000000000000000", "player": None, "player_dimension": 0,
                         "level_name_utf16": "cache-callback-reference".encode("utf-16-be").hex(),
                         "version": 0, "raining": False, "rain_time": 0,
                         "thundering": False, "thunder_time": 0}, "all fifteen genuine initial Info fields")
    expected = copy.deepcopy(projected[1])
    expected["cache"]["context"]["world_time"] = P.TIME
    expected["info"]["time"] = P.TIME
    equal(projected[2], expected, "time-only complete state transition")
    equal(time["receiver"], boundaries[0]["collections"]["world"], "actual public time receiver")
    equal(projected[3], projected[2], "continuous first full-save entry")
    expected = copy.deepcopy(projected[3])
    for chunk in expected["cache"]["world"]["chunks"]:
        if not chunk["blank"]:
            chunk["flags"].update(o=False, q=False)
            chunk["time"] = P.TIME
    expected["info"].update(version=19132, size_on_disk="0000000000006000")
    expected["disk"] = projected[4]["disk"]
    equal(projected[4], expected, "complete first full-save live effects")
    equal(projected[5], projected[4], "continuous second full-save entry")
    equal({name: projected[6][name] for name in ("cache", "info")},
          {name: projected[4][name] for name in ("cache", "info")}, "second full-save preserves live cache and Info")
    disks = [P.disk_files(row["disk"]) for row in projected]
    equal(set(disks[0]), {P.REGION, P.SESSION}, "genuine no-metadata initial file set")
    P.region_records(projected[0]["disk"], time["epoch_ms"], saved=False)
    equal(set(disks[4]), {P.REGION, P.SESSION, "world/level.dat"}, "complete first full-save files")
    equal(set(disks[6]), {P.REGION, P.SESSION, "world/level.dat", "world/level.dat_old"}, "complete second full-save files")
    directory_names = [entry for entry in projected[0]["disk"]["entries"] if entry["kind"] == "directory"]
    equal(directory_names, [{"path": path, "kind": "directory"} for path in
                            (".", "world", "world/data", "world/region")],
          "complete genuine constructor directory inventory")
    for row in projected:
        equal([entry for entry in row["disk"]["entries"] if entry["kind"] == "directory"], directory_names,
              "full-save preserves complete directory inventory including empty data directory")
    equal(disks[4][P.SESSION], disks[0][P.SESSION], "full save preserves actual session owner bytes")
    for index in (5, 6):
        for path in (P.SESSION, P.REGION):
            equal(disks[index][path], disks[4][path], "second save preserves entire actual " + path)
    equal(disks[6]["world/level.dat_old"], disks[4]["world/level.dat"], "exact first metadata becomes level.dat_old")
    metadata_rows = []
    for index, save in enumerate(value["saves"]):
        P.keys(save, ("index", "binding", "receiver", "force", "progress", "returned_void", "epoch_ms", "before", "after"),
               "complete actual void World-save result")
        equal({name: save[name] for name in ("index", "binding", "receiver", "force", "progress", "returned_void")},
              {"index": index, "binding": value["bindings"]["world_save"], "receiver": time["receiver"],
               "force": True, "progress": None, "returned_void": True}, "actual full World a(true,null) call")
        P.epoch(save["epoch_ms"])
        metadata_rows.append(metadata(disks[4 if index == 0 else 6]["world/level.dat"],
                                      projected[3 if index == 0 else 5]["info"], save["epoch_ms"]))
    equal([row["size_on_disk"] for row in metadata_rows], [0, 24576], "metadata precedes Chunk growth")
    records = P.region_records(filtered_region_disk(projected[4]["disk"]), value["saves"][0]["epoch_ms"], saved=True)
    cached = {tuple(chunk["coordinates"]): chunk for chunk in projected[3]["cache"]["world"]["chunks"] if not chunk["blank"]}
    equal(set(cached), set(P.COORDINATES), "four genuine retained presave normal owners")
    for record in records:
        tree = record["tree"]
        equal((tree["type"], tree["name_utf16"], len(tree["value"])), (10, "", 1), "exact saved Chunk root")
        level = P.STORAGE.level.child(tree, "Level")
        require(level is not None, "actual saved Level")
        equal((level["type"], len(level["value"])), (10, 11), "all eleven saved Level tags")
        coordinates = (record["slot"] & 31, record["slot"] >> 5)
        owner = cached[coordinates]
        for name, kind, expected_value in (("xPos", 3, coordinates[0]), ("zPos", 3, coordinates[1]),
                                           ("LastUpdate", 4, 1234), ("TerrainPopulated", 1, int(owner["flags"]["n"]))):
            child = P.STORAGE.level.child(level, name)
            require(child is not None, "actual saved typed field " + name)
            equal((child["type"], child["value"]), (kind, expected_value), "actual saved " + name)
        for name, role in (("Blocks", "blocks"), ("Data", "metadata"), ("SkyLight", "skylight"),
                           ("BlockLight", "block_light"), ("HeightMap", "heightmap")):
            child = P.STORAGE.level.child(level, name)
            require(child is not None, "actual saved array " + name)
            equal((child["type"], child["value"]), (7, owner["data"][role]["values"]), "full saved presave-owner array " + name)
        for name in ("Entities", "TileEntities"):
            child = P.STORAGE.level.child(level, name)
            require(child is not None, "actual saved empty list " + name)
            equal((child["type"], child["value"]), (9, {"element_type": 1, "items": []}), "exact typed empty list " + name)
    equal(value["limits"], {"output_bytes": C.MAX_BYTES, "collection_entries": 256, "inventory_entries": 256,
                            "file_bytes": 4 * 1024 * 1024, "snapshot_file_bytes": 8 * 1024 * 1024,
                            "retained_file_bytes": 8 * 1024 * 1024, "actual_retained_file_bytes": retained},
          "complete original observation limits and actual retained bytes")
    return {"population": prefix, "boundaries": projected, "records": records, "metadata": metadata_rows}


def native_collections(value):
    """Validate stable logical owner tags; the eighth binding is native-only."""
    equal(value, {"world": 0, "players": {"owner": 2, "size": 0, "entries": []},
                  "saved_data": {"owner": 3, "storage": 1, "native_storage_binding": 7,
                                 "named_cache": {"owner": 4, "size": 0, "entries": []},
                                 "ordered_list": {"owner": 5, "size": 0, "entries": []},
                                 "id_map": {"owner": 6, "size": 0, "entries": []}}},
          "complete actual empty native collection graph and retained owner tags")


def metadata_payload(value):
    """Retain all typed metadata except its separately measured clock input."""
    tree = copy.deepcopy(value["tree"])
    data = P.STORAGE.level.child(tree, "Data")
    name = "LastPlayed".encode("utf-16-be").hex()
    data["value"] = [row for row in data["value"] if row["name_utf16"] != name]
    return tree


def compare_original(native, original, public, jar):
    """Compare complete fresh native effects against validated original saves."""
    P.keys(native, ("schema", "scope", "profile", "side", "seed_hex", "construction_epoch_ms",
                    "population", "world_save"), "complete native full World save wrapper")
    side = native["side"]
    require(type(side) is str and side in HASHES, "known native full-save side")
    equal({name: native[name] for name in ("schema", "scope", "profile", "side", "seed_hex")},
          {"schema": 1, "scope": "native-game-population-full-world-save", "profile": PROFILE,
           "side": original["side"], "seed_hex": SEED}, "requested native full-save profile")
    expected = project_original(original, side, jar)
    P.epoch(native["construction_epoch_ms"])
    P.keys(native["population"], ("schema", "scope", "side", "seed_hex", "initial", "operations", "failure"),
           "complete native origin prefix")
    for row in native["population"]["operations"]:
        P.keys(row, ("index", "method", "coordinates", "result", "status", "snapshot"),
               "complete native origin operation")
    C.compare_original(native["population"], original["population"], public, jar, profile="origin")
    TRANSPORT.REPLAY.validate_native(native["population"], side, public)
    value = native["world_save"]
    P.keys(value, ("session_token", "block_map", "initial", "time_api", "saves", "failure"),
           "complete native held full-save record")
    require(value["failure"] is None, "native full-save producer failure")
    # No loader is called in this profile. Retain its supplied map's complete
    # transport without claiming an original post-load map comparison.
    P.byte_array(value["block_map"], 256, "complete native unused post-load map")
    boundaries = all_boundaries(value)
    equal(len(boundaries), len(expected["boundaries"]), "all seven native full-save boundaries")
    snapshots = [native["population"]["initial"],
                 *[row["snapshot"] for row in native["population"]["operations"]]]
    states = list(original["population"]["states"])
    native_disks, native_infos = [], []
    retained = 0
    for index, (boundary, observed) in enumerate(zip(boundaries, expected["boundaries"])):
        P.keys(boundary, ("state", "collections"), "complete native full-save boundary")
        state = boundary["state"]
        P.keys(state, ("snapshot", "info", "disk"), "complete native base boundary")
        snapshot = state["snapshot"]
        equal({name: snapshot[name] for name in ("context", "world")}, observed["cache"],
              "complete native full-save cache boundary " + str(index))
        actual_info = P.info(state["info"], side, original=False)
        equal(actual_info, observed["info"], "all fifteen native Info fields boundary " + str(index))
        equal(state["info"]["owner"], boundaries[0]["state"]["info"]["owner"], "actual native Info owner retained")
        native_collections(boundary["collections"])
        equal(boundary["collections"], boundaries[0]["collections"], "native actual collection graph retained")
        actual_disk = P.disk(state["disk"], original=False)
        P.session(actual_disk, native["construction_epoch_ms"], value["session_token"])
        retained += actual_disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "native complete cumulative raw-file budget")
        equal([{name: entry[name] for name in ("path", "kind")} for entry in actual_disk["entries"]],
              [{name: entry[name] for name in ("path", "kind")} for entry in observed["disk"]["entries"]],
              "complete native/source files and directory inventory")
        equal({name: actual_disk[name] for name in ("files", "directories")},
              {name: observed["disk"][name] for name in ("files", "directories")},
              "complete native file and directory counts")
        # Gzip byte counts are checked against each actual byte inventory, not
        # equated across Java/C encoders or their different LastPlayed inputs.
        native_disks.append(actual_disk)
        native_infos.append(actual_info)
        snapshots.append(snapshot)
        states.append(observed["numerical"])
    equal(len(snapshots), 12, "five origin and seven full-save numerical boundaries")
    TRANSPORT.validate_transport(snapshots)
    C.compare_numerical_snapshots(snapshots, states)
    for snapshot in snapshots:
        for name in ("generator_random", "cave_random", "world_random"):
            P.copied_random(snapshot["provider"][name], original=False)
    time = value["time_api"]
    P.keys(time, ("value", "previous", "epoch_ms", "before", "after", "status", "native_status"),
           "complete native time event")
    equal({name: time[name] for name in ("value", "previous", "status", "native_status")},
          {"value": 1234, "previous": "0000000000000000", "status": "ok", "native_status": 0},
          "actual native public time result")
    P.epoch(time["epoch_ms"])
    for index in (1, 2, 3):
        equal(native_disks[index], native_disks[0], "native time setter preserves all actual disk bytes")
    equal(native_disks[5], native_disks[4], "continuous native second-save entry")
    files = [P.disk_files(row) for row in native_disks]
    for index in (4, 5, 6):
        equal(files[index][P.SESSION], files[0][P.SESSION], "native full saves preserve session bytes")
    equal(files[6][P.REGION], files[4][P.REGION], "native second save preserves complete region bytes")
    equal(files[6][OLD_LEVEL], files[4][LEVEL], "native exact first metadata becomes level.dat_old")
    P.region_records(native_disks[0], time["epoch_ms"], saved=False)
    equal(P.region_records(filtered_region_disk(native_disks[4]), value["saves"][0]["epoch_ms"], saved=True),
          expected["records"], "complete native/source sector layout and typed saved Chunk NBT")
    for index, row in enumerate(value["saves"]):
        P.keys(row, ("index", "force", "progress", "returned_void", "epoch_ms", "before", "after",
                     "status", "native_status", "error"), "complete native full-save result")
        equal({name: row[name] for name in ("index", "force", "progress", "returned_void", "status", "native_status", "error")},
              {"index": index, "force": True, "progress": None, "returned_void": True,
               "status": "ok", "native_status": 0, "error": None}, "actual successful native full World save")
        P.epoch(row["epoch_ms"])
        actual = metadata(files[4 if index == 0 else 6][LEVEL], native_infos[3 if index == 0 else 5], row["epoch_ms"])
        equal(metadata_payload(actual), metadata_payload(expected["metadata"][index]),
              "all thirteen typed source/native metadata tags with separately measured LastPlayed")
    return expected


def actual_disk(root):
    """Read a complete bounded ordinary world directory after its process exits."""
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), "ordinary retained World root")
    root = root.resolve()
    stack, paths = [root], []
    while stack:
        path = stack.pop()
        require(path.is_relative_to(root) and len(paths) < 256, "bounded owned retained inventory")
        attributes = path.lstat()
        require(not stat.S_ISLNK(attributes.st_mode) and
                not (getattr(attributes, "st_file_attributes", 0) & 0x400),
                "ordinary retained path without links or reparse points")
        paths.append(path)
        if stat.S_ISDIR(attributes.st_mode):
            with os.scandir(path) as children:
                entries = list(itertools.islice(children, 257))
            require(len(entries) <= 256, "bounded complete retained directory")
            stack.extend(Path(entry.path) for entry in entries)
        else:
            require(stat.S_ISREG(attributes.st_mode), "ordinary retained file")
    paths.sort(key=lambda path: "." if path == root else path.relative_to(root).as_posix())
    entries, files, directories, total = [], 0, 0, 0
    for path in paths:
        name = "." if path == root else path.relative_to(root).as_posix()
        before = path.lstat()
        if stat.S_ISDIR(before.st_mode):
            entries.append({"path": name, "kind": "directory"})
            directories += 1
            continue
        require(stat.S_ISREG(before.st_mode) and 0 <= before.st_size <= 4 * 1024 * 1024 and
                total <= 8 * 1024 * 1024 - before.st_size, "bounded complete retained file bytes")
        with path.open("rb") as stream:
            raw = stream.read(4 * 1024 * 1024 + 1)
        after = path.lstat()
        require(stat.S_ISREG(after.st_mode) and len(raw) == before.st_size == after.st_size and
                not (getattr(after, "st_file_attributes", 0) & 0x400) and
                (before.st_mtime_ns, before.st_dev, before.st_ino) ==
                (after.st_mtime_ns, after.st_dev, after.st_ino), "stable complete retained file read")
        entries.append({"path": name, "kind": "file", "bytes": len(raw),
                        "sha256": hashlib.sha256(raw).hexdigest(), "hex": raw.hex()})
        total += len(raw)
        files += 1
    return P.disk({"complete": True, "entries": entries, "files": files,
                   "directories": directories, "bytes": total}, original=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed23 genuine held GAME World population and two full saves",
              "profile": PROFILE, "passed": False, "tested_commit": args.tested_commit or None,
              "full_world_save": True, "world_restart": False, "unload": False,
              "nonempty_players": False, "nonempty_named_data": False,
              "source_io_failure_policy_qualified": False, "whole_population_equivalence": False,
              "processes": [], "sides": {}}
    try:
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "complete tested commit")
        require(not any(os.environ.get(name) for name in
                        ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")),
                "uninjected original Java environment")
        tools = {name: getattr(args, name).resolve() for name in ("executable", "java", "javac")}
        jars = {side: getattr(args, side).resolve() for side in HASHES}
        require(all(path.is_file() and not path.is_symlink() for path in (*tools.values(), *jars.values())),
                "ordinary selected tools and original JARs")
        for side, jar in jars.items():
            equal(C.CAPTURE.file_fingerprint(jar)["sha256"], HASHES[side], "pinned supplied original " + side)
        work = C.CAPTURE.capture_root(args.work_dir, [sys.executable, tools["java"], tools["javac"]])
        require(not tools["executable"].is_relative_to(work), "capture excludes selected executable")
        report.update(C.source_context(work, report["processes"]))
        require(not args.tested_commit or args.tested_commit == report["source_head"] and report["source_dirty"] is False,
                "tested commit requires matching clean source")
        paths = C.source_inputs(**tools, jars=jars)
        paths.update(world_save_driver=Path(__file__).resolve(),
                     persistence_driver=Path(P.__file__).resolve(),
                     strict_transport=Path(TRANSPORT.__file__).resolve(),
                     numerical_replay=Path(TRANSPORT.REPLAY.__file__).resolve(),
                     storage_transport_validator=ROOT / "tests/tools/storage/test_store.py")
        report["inputs"] = {name: {"path": str(path), **C.CAPTURE.file_fingerprint(path)} for name, path in paths.items()}
        report["python_version"] = sys.version
        classes = work / "classes"
        classes.mkdir()
        C.run([str(tools["javac"]), "--release", "17", "-d", str(classes),
               *[str(ROOT / source) for source in SOURCES]], work, "compile", report["processes"])
        report["helper_classes"] = C.file_inventory(classes)
        public = public_origins()
        for side, jar in jars.items():
            directory = work / side
            directory.mkdir()
            resources = directory / "resources"
            row = {"resources": C.extract_resources(jar, resources), "passed": False}
            report["sides"][side] = row
            original_path, output = directory / "original.json", directory / "native.json"
            C.run([str(tools["java"]), "-Xint", "-Xmx512m", "--add-opens", "java.base/java.util=ALL-UNNAMED",
                   "-cp", str(classes), "PopulationWorldSaveObservation", str(jar), side, SEED,
                   str(directory / "original-world"), str(original_path), PROFILE],
                  work, side + "-original", report["processes"])
            original = C.read_json(original_path)
            expected = project_original(original, side, jar)
            C.run([str(tools["executable"]), side, str(resources).encode("utf-8").hex(),
                   str(directory / "native-world").encode("utf-8").hex(), str(output), PROFILE],
                  work, side + "-native", report["processes"])
            native = C.read_json(output)
            equal((work / (side + "-original.stdout")).read_bytes(), b"", "original stdout")
            equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                  (work / (side + "-original.stderr")).read_bytes().replace(b"\r\n", b"\n"),
                  "complete original/native bootstrap console")
            compare_original(native, original, public[side], jar)
            equal(actual_disk(directory / "original-world"), expected["boundaries"][-1]["disk"],
                  "complete actual final original disk equals its captured bytes")
            final_native = all_boundaries(native["world_save"])[-1]["state"]["disk"]
            equal(actual_disk(directory / "native-world"), P.disk(final_native, original=False),
                  "complete actual final native disk equals its captured bytes")
            for name, fingerprint in row["resources"].items():
                equal(C.CAPTURE.file_fingerprint(resources / name), fingerprint, "original resource preserved: " + name)
            row.update(passed=True, full_world_saves=2, boundaries=12, save_boundaries=7,
                       complete_typed_nbt=True, complete_metadata=True, exact_within_run_rotation=True,
                       actual_final_disks_verified=True,
                       original_output=C.CAPTURE.file_fingerprint(original_path),
                       native_output=C.CAPTURE.file_fingerprint(output))
            print("PASS: " + side + " populated held GAME World full saves", flush=True)
        equal(C.file_inventory(classes), report["helper_classes"], "compiled helper classes preserved")
        for name, path in paths.items():
            equal({"path": str(path), **C.CAPTURE.file_fingerprint(path)}, report["inputs"][name],
                  "comparison input preserved: " + name)
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
