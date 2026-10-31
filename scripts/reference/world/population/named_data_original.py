"""Validate and project complete original-only held-World named MapData records.

The supplied observations retain genuine game object identities, complete raw
numerical state and complete file snapshots. Projection consumes no native
results and performs no capture-directory or game process operations. Capture
attribution and actual retained-file auditing belong to the capture driver.
"""

from __future__ import annotations

import copy
import hashlib
import importlib.util
from pathlib import Path
import re
import sys
import zlib


ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location(
    "_population_named_data_original_world_save",
    ROOT / "scripts/reference/world/population/compare_world_save.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing canonical full World save comparison support")
W = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = W
SPEC.loader.exec_module(W)
P, C = W.P, W.C
require, equal = W.require, W.equal
HASHES, SEED, PROFILE = C.HASHES, C.SEED, "named-map-data"
SOURCES = (*W.SOURCES, "tests/reference/world/population/PopulationNamedDataObservation.java")
NAME, LOOKUP = "named_map_value", "named_map_lookup"
NAMED_FILE = "world/data/" + NAME + ".dat"


def all_boundaries(value):
    """Return the nineteen chronological boundaries of the fixed real API schedule."""
    require(type(value) is dict, "complete named-data operation record")
    require(type(value.get("saves")) is list and len(value["saves"]) == 2,
            "complete two-save named-data schedule")
    operations = [value["time_api"], value["map_constructor"], value["dirty_mark"], value["put"],
                  *value["saves"], value["reader_constructor"], value["reader_load"], value["reader_cache_hit"]]
    return [value["initial"], *[row[which] for row in operations for which in ("before", "after")]]


def ref(value, expected_class, *, nullable=False):
    if nullable and value == {"ref": None, "class": None}:
        return None
    return W.reference(value, expected_class)


def reference_classes(document):
    pending, owners, count = [document], {}, 0
    while pending:
        value = pending.pop()
        count += 1
        require(count <= 2000000, "bounded complete observed JSON graph")
        if type(value) is dict:
            if set(value) == {"ref", "class"}:
                if value["ref"] is None:
                    equal(value, {"ref": None, "class": None}, "actual null reference pair")
                else:
                    require(type(value["class"]) is str and 1 <= len(value["class"]) <= 256,
                            "bounded actual reference class")
                    identity = ref(value, value["class"])
                    if identity in owners:
                        equal(value["class"], owners[identity], "one actual identity retains its actual class")
                    owners[identity] = value["class"]
            pending.extend(value.values())
        elif type(value) is list:
            pending.extend(value)
    return len(owners)


def string(value, expected):
    P.keys(value, ("object", "utf16"), "complete actual String and UTF16 contents")
    identity = ref(value["object"], "java.lang.String")
    require(type(value["utf16"]) is list and len(value["utf16"]) <= 256,
            "bounded complete actual String units")
    for unit in value["utf16"]:
        P.integer(unit, 0, 65535, "actual UTF16 code unit")
    encoded = expected.encode("utf-16-be")
    equal(value["utf16"], [int.from_bytes(encoded[i:i + 2], "big") for i in range(0, len(encoded), 2)],
          "exact source String content")
    return identity


def collection(value, kind, expected_entries):
    P.keys(value, ("object", "size", "entries"), "complete actual collection")
    identity = ref(value["object"], "java.util." + kind)
    equal(P.integer(value["size"], 0, 256, "actual collection size"), len(expected_entries),
          "actual collection entry count")
    equal(value["entries"], expected_entries, "complete actual collection entries and order")
    return identity


def manager(value, side, raw, key=None, mapped=None):
    P.keys(value, ("object", "storage", "named_cache", "ordered_list", "id_map"), "complete actual manager")
    identity = ref(value["object"], "hc" if side == "client" else "es")
    storage = ref(value["storage"], "mx" if side == "client" else "ie")
    equal(value["storage"], raw["world"]["storage"], "manager bound to the actual held World storage")
    entries = [] if key is None else [{"ordinal": 0, "key": key["object"], "key_string": key, "value": mapped}]
    named = collection(value["named_cache"], "HashMap", entries)
    ordered = collection(value["ordered_list"], "ArrayList", [] if key is None else [{"index": 0, "value": mapped}])
    ids = collection(value["id_map"], "HashMap", [])
    require(len({identity, storage, named, ordered, ids}) == 5, "distinct actual manager and collection owners")
    return {"object": identity, "storage": storage, "named": named, "ordered": ordered, "ids": ids}


def map_data(value, side, name_record, dirty):
    P.keys(value, ("object", "name", "dirty", "x_center", "z_center", "dimension", "scale", "update_counter",
                   "colors", "trackers", "decorations", "tracker_cache"), "complete actual MapData")
    identity = ref(value["object"], "iu" if side == "client" else "fv")
    equal(value["name"], name_record, "actual inherited final name String owner and contents")
    require(type(value["dirty"]) is bool and value["dirty"] is dirty, "actual inherited dirty cell")
    for name in ("x_center", "z_center", "update_counter"):
        equal(P.integer(value[name], -(1 << 31), (1 << 31) - 1, name), 0, "default MapData " + name)
    for name in ("dimension", "scale"):
        equal(P.integer(value[name], -128, 127, name), 0, "default signed-byte MapData " + name)
    colors = value["colors"]
    P.keys(colors, ("object", "bytes", "sha256", "hex"), "complete actual colors backing")
    array = ref(colors["object"], "[B")
    equal(P.integer(colors["bytes"], 0, 4 * 1024 * 1024, "colors length"), 16384, "actual default colors length")
    raw = P.raw_hex(colors["hex"], 16384, "all actual default colors")
    equal(raw, bytes(16384), "all default colors zero")
    equal(colors["sha256"], hashlib.sha256(raw).hexdigest(), "full actual colors SHA256")
    h = collection(value["trackers"], "ArrayList", [])
    i = collection(value["decorations"], "ArrayList", [])
    j = collection(value["tracker_cache"], "HashMap", [])
    require(len({identity, array, h, i, j}) == 5, "distinct MapData, colors and tracker owners")
    return {"object": identity, "colors": array, "trackers": h, "decorations": i, "tracker_cache": j}


def binding(owner, name, descriptor, modifiers=1):
    return {"class": owner, "method": name, "descriptor": descriptor, "modifiers": modifiers}


def source_bindings(value, side, jar):
    base, map_type, saved, world, storage, compound = (
        ("hm", "iu", "hc", "fd", "wt", "nu") if side == "client" else
        ("fa", "fv", "es", "dj", "om", "iq"))
    methods = {}
    for role, owner, name, descriptor, modifiers in (
        ("value_mark_dirty", base, "a", "()V", 1),
        ("value_set_dirty", base, "a", "(Z)V", 1),
        ("value_get_dirty", base, "b", "()Z", 1),
        ("value_read", base, "a", "(L" + compound + ";)V", 1025),
        ("value_write", base, "b", "(L" + compound + ";)V", 1025),
        ("map_read", map_type, "a", "(L" + compound + ";)V", 1),
        ("map_write", map_type, "b", "(L" + compound + ";)V", 1),
        ("world_put", world, "a", "(Ljava/lang/String;L" + base + ";)V", 1),
        ("world_load", world, "a", "(Ljava/lang/Class;Ljava/lang/String;)L" + base + ";", 1),
        ("manager_put", saved, "a", "(Ljava/lang/String;L" + base + ";)V", 1),
        ("manager_load", saved, "a", "(Ljava/lang/Class;Ljava/lang/String;)L" + base + ";", 1),
        ("manager_write", saved, "a", "(L" + base + ";)V", 2),
        ("map_constructor", map_type, "<init>", "(Ljava/lang/String;)V", 1),
        ("manager_constructor", saved, "<init>", "(L" + storage + ";)V", 1),
        ("base_constructor", base, "<init>", "(Ljava/lang/String;)V", 1)):
        methods[role] = binding(owner, name, descriptor, modifiers)
    for role, expected in methods.items():
        equal(value["bindings"].get(role), expected, "exact original named-data method " + role)
    fields = {}
    for role, owner, name, descriptor, modifiers in (
        ("value_name", base, "a", "Ljava/lang/String;", 17), ("value_dirty", base, "b", "Z", 2),
        ("map_x_center", map_type, "b", "I", 1), ("map_z_center", map_type, "c", "I", 1),
        ("map_dimension", map_type, "d", "B", 1), ("map_scale", map_type, "e", "B", 1),
        ("map_colors", map_type, "f", "[B", 1), ("map_update_counter", map_type, "g", "I", 1),
        ("map_trackers", map_type, "h", "Ljava/util/List;", 1),
        ("map_decorations", map_type, "i", "Ljava/util/List;", 1),
        ("map_tracker_cache", map_type, "j", "Ljava/util/Map;", 2)):
        fields[role] = {"class": owner, "field": name, "descriptor": descriptor, "modifiers": modifiers}
    for role, expected in fields.items():
        equal(value["field_bindings"].get(role), expected, "exact original named-data field " + role)
    require(type(value["class_origins"]) is dict and base in value["class_origins"] and map_type in value["class_origins"],
            "actual base and subtype source origins")
    url = jar.resolve().as_uri().replace("file:///", "file:/", 1)
    equal({name: value["class_origins"][name] for name in (base, map_type)}, {base: url, map_type: url},
          "named-data classes from pinned original")
    W.original_bindings({"bindings": {k: v for k, v in value["bindings"].items() if k not in methods},
                         "field_bindings": {k: v for k, v in value["field_bindings"].items() if k not in fields},
                         "class_origins": {k: v for k, v in value["class_origins"].items() if k not in (base, map_type)}}, side, jar)


def project_bindings(document):
    equal(document["source_helpers"], [{"source": name, "class": Path(name).stem, "modifiers": 17}
                                       for name in W.SOURCES], "five unchanged public source helper selections")
    random = "NaturalPopulationObservation$RandomFields"
    observer = "CacheCallbacksObservation$Observer"
    natural = "NaturalPopulationObservation$Observation"
    bridge = "PopulationPersistenceObservation$ObservationBridge"
    persistence = "PopulationPersistenceObservation$Persistence"
    full = "PopulationWorldSaveObservation$FullSave"
    expected = {}
    for role, owner, descriptor in (
        ("random_fields_constructor", random, "()V"),
        ("observer_constructor", observer, "(Ljava/net/URL;Ljava/lang/ClassLoader;Ljava/nio/file/Path;ZLjava/lang/String;)V"),
        ("natural_constructor", natural, "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/ClassLoader;Ljava/net/URL;ZL" + random + ";Ljava/lang/String;)V"),
        ("bridge_constructor", bridge, "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Object;Ljava/lang/Class;)V"),
        ("persistence_constructor", persistence, "(L" + bridge + ";ZLjava/nio/file/Path;)V"),
        ("full_save_adapter_constructor", full, "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/ClassLoader;Ljava/net/URL;ZLjava/lang/reflect/Method;)V")):
        expected[role] = binding(owner, "<init>", descriptor, 0)
    expected.update({
        "natural_observe": binding(natural, "observe", "(Ljava/lang/String;Ljava/lang/String;)Ljava/util/Map;", 0),
        "base_boundary": binding(persistence, "boundary", "(II)Ljava/lang/Object;", 0),
        "observer_close": binding(observer, "close", "()V", 0),
        "json": binding("ChunkObservation", "json", "(Ljava/lang/Object;)Ljava/lang/String;", 10)})
    equal(document["project_bindings"], expected, "all checked unchanged private composition descriptors")


def numerical(states, side, raw_world):
    require(type(states) is list and len(states) == 24, "five origin and nineteen named numerical states")
    scratch = ("q", "d", "e", "f", "g", "h", "r", "s", "t", "w")
    owner_classes = {"generator": "yf" if side == "client" else "pk", "world": "fd" if side == "client" else "dj",
                     "random": "java.util.Random", "cave": "sq" if side == "client" else "lt",
                     "cave_random": "java.util.Random", "random_seed": "java.util.concurrent.atomic.AtomicLong",
                     "cave_random_seed": "java.util.concurrent.atomic.AtomicLong"}
    biome_array = "[L" + ("kd" if side == "client" else "gs") + ";"
    first_owners = None
    for index, state in enumerate(states):
        P.keys(state, ("provider", "aux"), "complete original numerical state")
        provider, aux = state["provider"], state["aux"]
        P.keys(provider, ("scratch", "biomes", "generator_random", "cave_random", "identities"), "complete provider state")
        P.keys(provider["scratch"], scratch, "complete ten provider raw64 arrays including w")
        P.keys(provider["identities"], (*owner_classes, *scratch, "v"), "complete provider owner/array identities")
        P.keys(aux, ("manager", "world_random", "world_random_attribution", "world_random_identity",
                     "world_random_seed_identity", "drain_depth", "chunk_static_a", "falling", "identities", "biome_statics"),
               "complete numerical auxiliary state")
        P.keys(aux["manager"], ("a", "b", "c", "d", "identities"), "complete temperature/rainfall/biome manager state")
        P.keys(aux["manager"]["identities"], ("manager", "a", "b", "c", "d"), "actual climate owner and buffer identities")
        P.keys(aux["identities"], ("world", "dimension", "cache", "storage", "loader"), "actual auxiliary owner graph")
        array_values = {}

        def array(value, identity, *, biome=False):
            if value is None:
                equal(identity, {"ref": None, "class": None}, "null numerical backing has null identity")
                return
            P.keys(value, ("length", "values"), "complete numerical backing")
            length = P.integer(value["length"], 0, 32768, "complete bounded numerical array")
            key = ref(identity, biome_array if biome else "[D")
            if biome:
                require(type(value["values"]) is str and len(value["values"]) == length
                        and re.fullmatch(r"[a-m-]*", value["values"]) is not None, "all registered biome labels")
            else:
                P.raw_hex(value["values"], length * 8, "all raw binary64 array bits")
            observed = {"class": identity["class"], "array": value}
            if key in array_values:
                equal(observed, array_values[key], "same actual numerical backing has the same class and complete contents")
            array_values[key] = observed

        for name in scratch:
            array(provider["scratch"][name], provider["identities"][name])
        array(provider["biomes"], provider["identities"]["v"], biome=True)
        for name in ("a", "b", "c", "d"):
            array(aux["manager"][name], aux["manager"]["identities"][name], biome=name == "d")
        owners = {}
        for name, expected_class in owner_classes.items():
            owners["provider." + name] = ref(provider["identities"][name], expected_class)
        owners["manager"] = ref(aux["manager"]["identities"]["manager"], "xv" if side == "client" else "ph")
        owners["world_random"] = ref(aux["world_random_identity"], "java.util.Random")
        owners["world_seed"] = ref(aux["world_random_seed_identity"], "java.util.concurrent.atomic.AtomicLong")
        require(len(set(owners.values())) == len(owners), "distinct numerical owners and Random seed cells")
        if first_owners is None:
            first_owners = owners
        equal(owners, first_owners, "all actual numerical owners retained")
        for name, raw_name in (("generator", "generator"), ("world", "world")):
            equal(provider["identities"][name], raw_world[raw_name], "numerical owner matches actual cache graph")
        equal(aux["manager"]["identities"]["manager"], raw_world["manager"], "same actual manager owner")
        for name, raw_name in (("world", "world"), ("dimension", "provider"), ("cache", "cache"),
                               ("storage", "storage"), ("loader", "loader")):
            equal(aux["identities"][name], raw_world[raw_name], "same actual numerical/World owner " + name)
        P.keys(aux["biome_statics"], tuple("abcdefghijklm"), "all thirteen actual biome static owners")
        static_ids = []
        for value in aux["biome_statics"].values():
            P.keys(value, ("ref", "class"), "actual biome static reference")
            require(type(value["class"]) is str and re.fullmatch(r"[a-z]+", value["class"]) is not None,
                    "actual pinned original biome class")
            static_ids.append(ref(value, value["class"]))
        require(len(set(static_ids)) == 13, "distinct actual biome statics")
        equal(aux["biome_statics"], states[0]["aux"]["biome_statics"], "all biome static owners retained")
        equal(P.integer(aux["drain_depth"], 0, 1000, "passive drain depth"), 0, "no passive drain introduced")
        require(type(aux["chunk_static_a"]) is bool and type(aux["falling"]) is bool, "actual shared Boolean fields")
        for value in (provider["generator_random"], provider["cave_random"], aux["world_random"]):
            P.copied_random(value, original=True)
        equal(aux["world_random_attribution"], "per-run-constructor-input; never replaced or reseeded", "World.r input attribution")
        equal(aux["world_random"], states[0]["aux"]["world_random"], "independently seeded actual World.r unchanged")
        if index >= 5:
            equal(state, states[4], "complete numerical arrays/aliases/RNG unchanged after origin")


def named_nbt(raw, value):
    require(type(raw) is bytes and 0 < len(raw) <= 4 * 1024 * 1024, "complete bounded named gzip")
    stream = zlib.decompressobj(31)
    plain = stream.decompress(raw, 4 * 1024 * 1024 + 1)
    require(stream.eof and not stream.unused_data and not stream.unconsumed_tail
            and 0 < len(plain) <= 4 * 1024 * 1024, "one complete named gzip member without trailing bytes")
    tree = P.canonical_nbt(P.STORAGE.nbt(plain))
    fields = {"dimension": (1, value["dimension"]), "xCenter": (3, value["x_center"]),
              "zCenter": (3, value["z_center"]), "scale": (1, value["scale"]),
              "width": (2, 128), "height": (2, 128), "colors": (7, value["colors"]["hex"])}
    children = [{"type": kind, "name_utf16": name.encode("utf-16-be").hex(), "value": payload}
                for name, (kind, payload) in fields.items()]
    expected = P.canonical_nbt({"type": 10, "name_utf16": "", "value": [
        {"type": 10, "name_utf16": "data".encode("utf-16-be").hex(), "value": children}]})
    equal(tree, expected, "exact seven typed MapData fields under lowercase data, bound to live value")
    return {"tree": tree, "gzip": {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
            "tags": 7, "colors_bytes": 16384}


def saved_chunks(disk, interval, cached):
    records = P.region_records(W.filtered_region_disk(disk), interval, saved=True)
    owners = {tuple(chunk["coordinates"]): chunk for chunk in cached["world"]["chunks"] if not chunk["blank"]}
    equal(set(owners), set(P.COORDINATES), "four genuine retained normal Chunk owners")
    for record in records:
        tree = record["tree"]
        equal((tree["type"], tree["name_utf16"], len(tree["value"])), (10, "", 1), "exact saved Chunk root")
        level = P.STORAGE.level.child(tree, "Level")
        require(level is not None, "saved Level compound")
        equal((level["type"], len(level["value"])), (10, 11), "all eleven saved Level tags")
        coordinates = (record["slot"] & 31, record["slot"] >> 5)
        owner = owners[coordinates]
        expected = {"xPos": (3, coordinates[0]), "zPos": (3, coordinates[1]),
                    "LastUpdate": (4, 1234), "TerrainPopulated": (1, int(owner["flags"]["n"]))}
        for name, role in (("Blocks", "blocks"), ("Data", "metadata"), ("SkyLight", "skylight"),
                           ("BlockLight", "block_light"), ("HeightMap", "heightmap")):
            expected[name] = (7, owner["data"][role]["values"])
        for name in ("Entities", "TileEntities"):
            expected[name] = (9, {"element_type": 1, "items": []})
        equal({row["name_utf16"] for row in level["value"]},
              {name.encode("utf-16-be").hex() for name in expected}, "complete exact saved Level names")
        for name, (kind, payload) in expected.items():
            equal(P.STORAGE.level.child(level, name),
                  {"type": kind, "name_utf16": name.encode("utf-16-be").hex(), "value": payload},
                  "complete source-bound saved Chunk field " + name)
    return records


def project_original(document, side, jar):
    """Validate a complete original-only observation and derive its expected state."""
    require(type(side) is str and side in HASHES, "known original named-data side")
    observed_identities = reference_classes(document)
    P.keys(document, ("schema", "scope", "profile", "experiment", "native_comparison", "tested_commit", "side",
                      "seed_hex", "jar_sha256", "construction_epoch_ms", "source_helpers", "project_bindings",
                      "population", "named_data"), "complete named MapData experiment wrapper")
    equal({key: document[key] for key in ("schema", "scope", "profile", "experiment", "native_comparison",
                                         "tested_commit", "side", "seed_hex", "jar_sha256")},
          {"schema": 1, "scope": "original-game-population-named-map-data-experiment", "profile": PROFILE,
           "experiment": True, "native_comparison": False, "tested_commit": None,
           "side": side, "seed_hex": C.SEED, "jar_sha256": C.HASHES[side]}, "finite original-only experiment attribution")
    P.epoch(document["construction_epoch_ms"])
    project_bindings(document)
    prefix = C.validate_original_observation(document["population"], side, "origin", jar)
    public = W.public_origins()[side]
    equal(prefix["initial"], public["initial"], "unchanged complete public constructor projection")
    equal(len(prefix["operations"]), 4, "four genuine origin acquisitions")
    for observed, expected in zip(prefix["operations"], public["operations"]):
        names = ("index", "method", "coordinates", "result", "status", "snapshot")
        equal({name: observed[name] for name in names}, {name: expected[name] for name in names},
              "complete unchanged public origin acquisition")
    require(type(document["population"]["states"]) is list and len(document["population"]["states"]) == 5,
            "all five original origin numerical states")
    value = document["named_data"]
    P.keys(value, ("schema", "scope", "full_world_save", "unload", "world_restart", "bindings", "field_bindings",
                   "class_origins", "initial", "time_api", "map_constructor", "dirty_mark", "put", "saves",
                   "reader_constructor", "reader_load", "reader_cache_hit", "limits"), "complete named-data experiment")
    equal({key: value[key] for key in ("schema", "scope", "full_world_save", "unload", "world_restart")},
          {"schema": 1, "scope": "original-game-held-world-named-map-data-experiment", "full_world_save": True,
           "unload": False, "world_restart": False}, "actual held World scope without restart")
    source_bindings(value, side, jar)
    require(type(value["saves"]) is list and len(value["saves"]) == 2, "two actual full World saves")
    operations = [value["time_api"], value["map_constructor"], value["dirty_mark"], value["put"],
                  *value["saves"], value["reader_constructor"], value["reader_load"], value["reader_cache_hit"]]
    boundaries = all_boundaries(value)
    equal(len(boundaries), 19, "all nineteen precise operation boundaries")
    for index, operation in enumerate(operations):
        low, high = P.epoch(operation["epoch_ms"])
        require(low >= document["construction_epoch_ms"][1], "API interval follows genuine construction")
        if index:
            require(low >= operations[index - 1]["epoch_ms"][1], "chronological actual API intervals")
        equal(operation["before"], boundaries[0] if index == 0 else operations[index - 1]["after"],
              "complete chronological boundary continuity")

    name_record, key_record = value["map_constructor"]["name"], value["put"]["key"]
    name_id, key_id = string(name_record, NAME), string(key_record, LOOKUP)
    require(name_id != key_id and NAME != LOOKUP, "distinct actual name and lookup String owners/contents")
    map_type = "iu" if side == "client" else "fv"
    original_value = value["map_constructor"]["result"]
    loaded_value = value["reader_load"]["result"]
    require(ref(original_value, map_type) != ref(loaded_value, map_type), "fresh separate-manager readback value")
    equal(value["reader_cache_hit"]["result"], loaded_value, "real cache hit returned exactly the loaded owner")
    _, _, finish, _, acquire, _ = C.PUBLIC.driver.helpers()
    callbacks = document["population"]["callbacks"]
    ids = acquire.identities(callbacks)
    for row in callbacks["operations"]:
        for entry in row["after"]["world"]["insertion_list"]["entries"]:
            if entry["value"] not in ids:
                ids[entry["value"]] = len(ids)
    raw_last = callbacks["operations"][-1]["after"]
    projected, retained, numerical_states = [], 0, []
    world_owners, reader_owners, original_graph, loaded_graph = None, None, None, None
    for index, boundary in enumerate(boundaries):
        P.keys(boundary, ("state", "collections", "map_data"), "complete named boundary")
        state = boundary["state"]
        P.keys(state, ("cache", "numerical", "info", "disk"), "unchanged complete base boundary")
        C.PUBLIC.driver.retained_snapshot_owners(state["cache"], raw_last, acquire)
        cache = C.PUBLIC.driver.snapshot_projection(state["cache"], ids, acquire, finish)
        equal(state["info"]["owner"], boundaries[0]["state"]["info"]["owner"], "held Info owner retained")
        info = P.info(state["info"], side, original=True)
        disk = P.disk(state["disk"], original=True)
        P.session(disk, document["construction_epoch_ms"])
        retained += disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "cumulative complete retained-file budget")
        projected.append({"cache": cache, "info": info, "disk": disk,
                          "numerical": copy.deepcopy(state["numerical"]),
                          "collections": copy.deepcopy(boundary["collections"]),
                          "map_data": copy.deepcopy(boundary["map_data"])})
        numerical_states.append(state["numerical"])
        graph = boundary["collections"]
        P.keys(graph, ("world", "players", "saved_data", "reader_manager"), "complete actual World/manager graph")
        equal(graph["world"], state["cache"]["world"]["world"], "same actual held World")
        player = collection(graph["players"], "ArrayList", [])
        current_world = manager(graph["saved_data"], side, state["cache"],
                                key_record if index >= 8 else None, original_value)
        if world_owners is None:
            world_owners = current_world
        equal(current_world, world_owners, "same actual World.z and all three manager collection owners")
        equal(graph["players"], boundaries[0]["collections"]["players"], "same genuine empty Player List")
        active_ids = [ref(graph["world"], "fd" if side == "client" else "dj"), player, *current_world.values()]
        if index < 14:
            equal(graph["reader_manager"], None, "separate reader not constructed early")
        else:
            current_reader = manager(graph["reader_manager"], side, state["cache"],
                                     name_record if index >= 16 else None, loaded_value)
            if reader_owners is None:
                reader_owners = current_reader
            equal(current_reader, reader_owners, "same separate real manager and collection owners")
            equal(current_reader["storage"], current_world["storage"], "both real managers share the genuine handler")
            active_ids.extend(item for key, item in current_reader.items() if key != "storage")
        require(len(set(active_ids)) == len(active_ids), "distinct real World/player/manager/backing owners")
        require(type(boundary["map_data"]) is list, "complete distinct observed MapData inventory")
        equal(len(boundary["map_data"]), 0 if index < 4 else (2 if index >= 16 else 1), "actual MapData lifetime schedule")
        for map_index, record in enumerate(boundary["map_data"]):
            expected_value = original_value if map_index == 0 else loaded_value
            equal(record["object"], expected_value, "ordered distinct live MapData owner")
            expected_dirty = map_index == 0 and 6 <= index <= 9
            current_graph = map_data(record, side, name_record, expected_dirty)
            if map_index == 0:
                if original_graph is None:
                    original_graph = current_graph
                equal(current_graph, original_graph, "original MapData and all backings retained")
                expected_record = copy.deepcopy(boundaries[4]["map_data"][0])
                expected_record["dirty"] = expected_dirty
                equal(record, expected_record, "only the source dirty cell changes on original MapData")
            else:
                if loaded_graph is None:
                    loaded_graph = current_graph
                equal(current_graph, loaded_graph, "loaded MapData and backings retained")
                equal(record, boundaries[16]["map_data"][1], "readback remains entirely clean and unchanged")
            active_ids.extend(current_graph.values())
        require(len(set(active_ids)) == len(active_ids), "distinct complete manager/MapData/backing graph")
    numerical(document["population"]["states"] + numerical_states, side, raw_last["world"])
    equal(projected[0]["cache"], prefix["operations"][-1]["snapshot"], "complete population/named experiment join")
    initial_info = {"seed": C.SEED, "spawn_x": 0, "spawn_y": 64, "spawn_z": 0, "time": "0000000000000000",
                    "last_played": "0000000000000000", "size_on_disk": "0000000000000000", "player": None,
                    "player_dimension": 0, "level_name_utf16": "cache-callback-reference".encode("utf-16-be").hex(),
                    "version": 0, "raining": False, "rain_time": 0, "thundering": False, "thunder_time": 0}
    equal(projected[0]["info"], initial_info, "all fifteen source initial Info fields")
    for index, row in enumerate(projected):
        expected_cache = copy.deepcopy(projected[0]["cache"])
        expected_info = dict(initial_info)
        if index >= 2:
            expected_cache["context"]["world_time"] = P.TIME
            expected_info["time"] = P.TIME
        if index >= 10:
            for chunk in expected_cache["world"]["chunks"]:
                if not chunk["blank"]:
                    chunk["flags"].update(o=False, q=False)
                    chunk["time"] = P.TIME
            expected_info.update(version=19132, size_on_disk="0000000000006000")
        equal(row["cache"], expected_cache, "complete cache/source transition boundary " + str(index))
        equal(row["info"], expected_info, "all live Info fields, clock and version boundary " + str(index))

    world = boundaries[0]["collections"]["world"]
    time, construction, mark, put = operations[:4]
    P.keys(time, ("binding", "receiver", "returned_void", "epoch_ms", "before", "after", "value", "previous"), "complete time call")
    equal({key: time[key] for key in ("binding", "receiver", "returned_void", "value", "previous")},
          {"binding": value["bindings"]["world_time_set"], "receiver": world, "returned_void": True,
           "value": 1234, "previous": "0000000000000000"}, "actual World time arguments/result")
    P.keys(construction, ("binding", "name", "result", "epoch_ms", "before", "after"), "actual MapData constructor event")
    equal(construction["binding"], value["bindings"]["map_constructor"], "actual MapData constructor binding")
    for operation, role, receiver, extra in ((mark, "value_mark_dirty", original_value, ()),
                                             (put, "world_put", world, ("key", "value"))):
        P.keys(operation, ("binding", "receiver", "returned_void", "epoch_ms", "before", "after", *extra), "complete original mutator event")
        equal({key: operation[key] for key in ("binding", "receiver", "returned_void")},
              {"binding": value["bindings"][role], "receiver": receiver, "returned_void": True},
              "actual source mutator receiver and return")
    equal(put["value"], original_value, "actual value supplied to World.put")
    for index, saved in enumerate(value["saves"]):
        P.keys(saved, ("binding", "receiver", "returned_void", "epoch_ms", "before", "after", "index", "force", "progress"), "complete actual full save")
        equal({name: saved[name] for name in ("index", "binding", "receiver", "force", "progress", "returned_void")},
              {"index": index, "binding": value["bindings"]["world_save"], "receiver": world,
               "force": True, "progress": None, "returned_void": True}, "actual two full World a(true,null) saves")
    reader_constructor = value["reader_constructor"]
    P.keys(reader_constructor, ("binding", "storage", "result", "epoch_ms", "before", "after"), "actual separate manager constructor")
    equal(reader_constructor["binding"], value["bindings"]["manager_constructor"], "actual manager constructor binding")
    equal(reader_constructor["storage"], raw_last["world"]["storage"], "same actual constructor storage argument")
    equal(reader_constructor["result"], boundaries[14]["collections"]["reader_manager"]["object"], "actual fresh reader manager result")
    for event in (value["reader_load"], value["reader_cache_hit"]):
        P.keys(event, ("binding", "receiver", "requested_class", "requested_class_object", "name", "result", "epoch_ms", "before", "after"), "actual Class/String load")
        equal({key: event[key] for key in ("binding", "receiver", "requested_class", "name")},
              {"binding": value["bindings"]["manager_load"], "receiver": reader_constructor["result"],
               "requested_class": map_type, "name": name_record},
              "same actual manager, MapData Class and original final-name String input")
        ref(event["requested_class_object"], "java.lang.Class")
    equal(value["reader_load"]["requested_class_object"], value["reader_cache_hit"]["requested_class_object"], "same actual requested Class owner")
    equal(boundaries[18], boundaries[17], "cache-hit identity, complete graph, bytes and numerical state unchanged")

    disks = [P.disk_files(row["disk"]) for row in projected]
    dirs = [{"path": path, "kind": "directory"} for path in (".", "world", "world/data", "world/region")]
    for index, row in enumerate(projected):
        equal([entry for entry in row["disk"]["entries"] if entry["kind"] == "directory"], dirs,
              "complete source directory inventory retained")
        expected_files = {P.REGION, P.SESSION}
        if index >= 10:
            expected_files.update((W.LEVEL, NAMED_FILE))
        if index >= 12:
            expected_files.add(W.OLD_LEVEL)
        equal(set(disks[index]), expected_files, "complete exact file set, final name rather than lookup/ID filename")
        equal(disks[index][P.SESSION], disks[0][P.SESSION], "actual session.lock unchanged throughout")
        if index < 10:
            equal(row["disk"], projected[0]["disk"], "constructor/dirty/put/time perform no disk writes")
        elif index >= 12:
            equal(row["disk"], projected[12]["disk"], "reader construction/load/cache hit preserve every actual file byte")
    P.region_records(W.filtered_region_disk(projected[0]["disk"]), time["epoch_ms"], saved=False)
    for index in range(10, 19):
        equal(disks[index][P.REGION], disks[10][P.REGION], "second save and readback preserve complete region bytes")
        equal(disks[index][NAMED_FILE], disks[10][NAMED_FILE], "clean second save/readback preserve complete named gzip")
    equal(projected[11]["disk"], projected[10]["disk"], "continuous second save disk entry")
    equal(disks[12][W.OLD_LEVEL], disks[10][W.LEVEL], "exact first metadata rotates to level.dat_old")
    metadata = [W.metadata(disks[after][W.LEVEL], projected[before]["info"], saved["epoch_ms"])
                for before, after, saved in ((9, 10, value["saves"][0]), (11, 12, value["saves"][1]))]
    equal([row["size_on_disk"] for row in metadata], [0, 24576], "metadata samples source Info before Chunk saving")
    named = named_nbt(disks[10][NAMED_FILE], boundaries[9]["map_data"][0])
    chunks = saved_chunks(projected[10]["disk"], value["saves"][0]["epoch_ms"], projected[9]["cache"])
    equal(value["limits"], {"output_bytes": C.MAX_BYTES, "collection_entries": 256, "inventory_entries": 256,
                            "file_bytes": 4 * 1024 * 1024, "snapshot_file_bytes": 8 * 1024 * 1024,
                            "retained_file_bytes": 8 * 1024 * 1024, "actual_retained_file_bytes": retained}, "all complete observation limits")
    summary = {"passed": True, "unchanged_public_origin": True, "origin_states": 5, "named_boundaries": 19,
            "numerical_states": 24, "copied_random_records": 72, "array_descriptors": 360,
            "actual_managers": 2, "actual_map_values": 2,
            "observed_identity_classes": observed_identities,
            "world_manager_retained": True, "separate_manager_same_storage": True,
            "default_colors_bytes": 16384, "dirty_sequence": [False, True, True, False, False],
            "full_world_saves": 2, "named_file": NAMED_FILE, "named_record": {key: named[key] for key in ("gzip", "tags", "colors_bytes")},
            "metadata": [{key: row[key] for key in ("gzip", "last_played", "size_on_disk", "tags", "player_present")} for row in metadata],
            "complete_saved_chunks": len(chunks), "typed_chunk_arrays": 20, "loaded_name_owner_preserved": True,
            "fresh_loaded_colors_owner": True, "cache_hit_same_owner_and_complete_state": True,
            "cache_hit_io_limit": "Read calls are not instrumented; pinned source cache-hit return and every retained disk byte are checked.",
            "id_maps_unchanged": True}
    return {"population": prefix, "boundaries": projected, "records": chunks,
            "metadata": metadata, "named_record": named, "summary": summary}


