#!/usr/bin/env python3
"""Compare coordinator-owned load and population callback boundaries.

Original loaders and terrain features remain external callback implementations.
The C adapter imports their complete observed results at those explicit seams.
"""

from __future__ import annotations

import argparse
import copy
from dataclasses import asdict
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import sys
import types
import uuid


ROOT = Path(__file__).resolve().parents[3]
MAX_RECORD = 32 * 1024 * 1024
MAX_C_RECORD = 64 * 1024 * 1024
SEED = "0000000000000017"
CASES = (("client", "load"), ("server", "load"),
         ("client", "population"), ("server", "population"))
SOURCES = ("tests/reference/chunk/ChunkObservation.java",
           "tests/reference/cache/CacheCallbacksObservation.java",
           "tests/reference/cache/CacheCallbacksController.java")


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def same_json(left, right):
    """Compare JSON values without Python's bool/int/float coercion."""
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return set(left) == set(right) and all(same_json(left[name], right[name]) for name in left)
    if type(left) is list:
        return len(left) == len(right) and all(same_json(a, b) for a, b in zip(left, right))
    return left == right


def helpers():
    require(Path(__file__).resolve() ==
            (ROOT / "scripts/reference/cache/compare_callbacks.py").resolve(),
            "canonical executing source identity")
    path = ROOT / "scripts/reference/cache/compare_acquire.py"
    data = path.read_bytes()
    require(len(data) <= 2 * 1024 * 1024, "bounded acquisition helper source")
    module = types.ModuleType("_callbacks_acquire")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    common, old, finish, normal, executed = module.helpers()
    executed.append({"path": str(path), "bytes": len(data),
                     "sha256": hashlib.sha256(data).hexdigest()})
    return common, old, finish, normal, module, executed


def fingerprint_inventory(common, root):
    value = common.world_inventory(root, hashes=True)
    require(value["complete"] is True and value["error"] is None and
            value["exists"] is True and len(value["entries"]) <= 256,
            "complete owned world inventory")
    names = set()
    total = 0
    for row in value["entries"]:
        name = row["path"]
        require(type(name) is str and name and name not in names and
                "\\" not in name and ":" not in name and
                all(part not in ("", ".", "..") for part in name.split("/")) and
                row["kind"] in ("file", "directory"),
                "unique relative ordinary world entry")
        names.add(name)
        if row["kind"] == "file":
            require(type(row["bytes"]) is int and 0 <= row["bytes"] <= MAX_RECORD and
                    type(row["sha256"]) is str and
                    re.fullmatch(r"[0-9a-f]{64}", row["sha256"]),
                    "complete bounded world file fingerprint")
            total += row["bytes"]
    require(total <= MAX_RECORD, "bounded complete copied world")
    return value


def inventory_contents(value):
    return {name: item for name, item in value.items() if name != "root"}


def copy_world(common, baseline, destination):
    before = fingerprint_inventory(common, baseline)
    require(not destination.exists() and not destination.is_symlink() and
            destination.parent.is_dir(), "fresh owned world copy destination")
    shutil.copytree(baseline, destination)
    copied = fingerprint_inventory(common, destination)
    require(inventory_contents(copied) == inventory_contents(before) and
            fingerprint_inventory(common, baseline) == before,
            "complete baseline copied without mutation")
    return copied


def preserved_reload_files(before, after):
    require(before["complete"] is True and after["complete"] is True,
            "complete reload inventories")
    first = {row["path"]: row for row in before["entries"]}
    last = {row["path"]: row for row in after["entries"]}
    require(set(first) == set(last), "reload created or removed world entries")
    require(any(name.endswith(".mcr") for name in first) and
            any(name.endswith("/level.dat") for name in first),
            "genuine saved region and world metadata")
    for name, row in first.items():
        if name.endswith("/session.lock"):
            require(row["kind"] == last[name]["kind"] == "file" and
                    row["bytes"] == last[name]["bytes"] == 8,
                    "only fixed-size original session lock may change")
        else:
            require(row == last[name], "reload mutated saved input: " + name)


def runtime(common, args, finish):
    require(not any(os.environ.get(name) for name in finish.JAVA_ENVIRONMENT),
            "injected Java environment")
    tools = common.runtime_tools(args)
    release = tools["jdk_release"].read_text(encoding="utf-8")
    version = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
    if version is None or int(version.group(1)) < 17:
        raise common.Unavailable("callback tracing requires JDK17 or newer")
    jars = {side: getattr(args, side).resolve() for side in finish.JAR_HASHES}
    for side, jar in jars.items():
        if not jar.is_file():
            raise common.Unavailable("missing supplied " + side + " original archive")
        require(common.file_record(jar, byte_limit=2 * 1024 * 1024)["sha256"] ==
                finish.JAR_HASHES[side], "pinned original archive")
    return tools, jars


def run_child(common, name, command, directory, *, seconds=120, world=None,
              stdin=None, limit=MAX_RECORD):
    return common.run_live(command, directory / (name + "-out.txt"),
        directory / (name + "-err.txt"), directory, seconds=seconds,
        stdout_limit=limit, stderr_limit=MAX_RECORD, world_root=world,
        stdin_path=stdin)


def success(process, context):
    require(process.stop_reason is None and process.timed_out is False and
            type(process.returncode) is int and process.returncode == 0,
            context + " failed: " + str(asdict(process)))


def operation_plan(scenario):
    require(scenario in ("prepare", "load", "population"), "fixed callback scenario")
    coordinates = ((1, 0),) if scenario == "prepare" else (
        ((1, 0), (1, 0)) if scenario == "load" else
        ((1, 0), (0, 1), (1, 1), (1, 1)))
    return tuple(("c", x, z) for x, z in coordinates)


def integer(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ": integer bounds")
    return value


def boolean(value, context):
    require(type(value) is bool, context + ": actual boolean")
    return value


def signed_time(value):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]{16}", value),
            "complete raw 64-bit time")
    raw = int(value, 16)
    return raw if raw < 1 << 63 else raw - (1 << 64)


def raw_array(value, count, context):
    require(type(value) is dict and set(value) == {"length", "values"} and
            type(value["length"]) is int and value["length"] == count and
            type(value["values"]) is str and
            re.fullmatch(r"[0-9a-f]{" + str(count * 2) + "}", value["values"]),
            context + ": complete fixed array")
    return bytes.fromhex(value["values"])


def snapshot_projection(value, ids, acquire, finish):
    require(type(value) is dict and set(value) == {
        "world", "cached_locals", "world_time", "world_force_load",
        "provider_force_load", "spawn", "opacity", "opacity_identity_retained",
        "opacity_values_retained"}, "complete original callback snapshot")
    require(value["spawn"] == [0, 64, 0] and
            all(type(item) is int for item in value["spawn"]),
            "qualifying genuine seed23 spawn")
    require(value["world_force_load"] is False and
            value["provider_force_load"] is None,
            "actual base-world force flags and absent provider flag")
    require(value["opacity_identity_retained"] is True and
            value["opacity_values_retained"] is True,
            "original opacity identity and values retained")
    raw_array(value["opacity"], 1024, "actual opacity registry")
    signed_time(value["world_time"])
    world = value["world"]
    require(world["cache"]["class"] in ("ok", "jb"),
            "genuine base-world cache dispatch")
    fallback = world["fallback"]
    require(fallback in ids and ids[fallback] == 0, "retained fallback ID0")
    chunks = acquire.chunk_map(value)
    require(fallback in chunks and chunks[fallback]["empty"] is True and
            chunks[fallback]["minimum_height"] == 0 and
            raw_array(chunks[fallback]["heightmap"], 256, "blank backing heightmap") == bytes(256),
            "actual finite-corpus blank backing state")
    projected = acquire.world_projection(value, ids, finish)
    boolean(world["no_sky"], "original no-sky flag")
    return {"context": {"dedicated": False, "world_time": value["world_time"],
        "world_force": value["world_force_load"], "provider_force": None,
        "spawn": [value["spawn"][0], value["spawn"][2]],
        "no_sky": world["no_sky"]}, "world": projected}


def retained_snapshot_owners(value, initial, acquire):
    first, current = initial["world"], value["world"]
    for name in ("world", "cache", "cache_world", "provider", "provider_world",
                 "storage", "loader", "generator", "manager", "fallback"):
        require(current[name] == first[name], "retained original context owner: " + name)
    for name in ("map", "insertion_list", "pending_set", "queue"):
        require(current[name]["object"] == first[name]["object"],
                "retained original collection owner: " + name)
    require(value["opacity"] == initial["opacity"] and
            value["spawn"] == initial["spawn"], "retained opacity and spawn")
    first_chunks, chunks = acquire.chunk_map(initial), acquire.chunk_map(value)
    for identity, chunk in first_chunks.items():
        require(identity in chunks, "all prior chunk identities retained")
        for name in ("object", "coordinates", "empty", "array_references"):
            require(chunks[identity][name] == chunk[name],
                    "retained original chunk owner/dispatch: " + name)


def validate_observation(value, side, scenario, jar, acquire, finish):
    require(type(value) is dict and set(value) == {
        "schema", "scope", "side", "scenario", "seed_hex", "jar_sha256",
        "platform_parent", "class_origins", "bindings", "setup_metadata",
        "time_api", "initial", "operations", "saved", "external_effects"},
        "complete original callback document")
    require(type(value["schema"]) is int and value["schema"] == 1 and
            value["scope"] == "cache-callback-boundary" and value["side"] == side and
            value["scenario"] == scenario and value["seed_hex"] == SEED and
            value["jar_sha256"] == finish.JAR_HASHES[side] and
            value["platform_parent"] is True and
            (type(value["saved"]) is dict if scenario == "prepare" else value["saved"] is None),
            "actual original callback provenance")
    require(value["external_effects"] == "Original loader, generation and terrain-feature "
            "internals remain opaque; only cache coordinator transitions are compared.",
            "explicit external callback scope")
    bindings = dict(zip(("world", "cache", "storage", "loader", "generator"),
        ("fd", "ok", "mx", "ld", "yf") if side == "client" else
        ("dj", "jb", "ie", "hb", "pk")))
    require(value["bindings"] == bindings, "actual persistent base-world constructor route")
    origins = value["class_origins"]
    require(type(origins) is dict and 20 <= len(origins) <= 128 and
            set(bindings.values()) <= set(origins) and
            all(type(name) is str and type(url) is str and
                url == jar.resolve().as_uri().replace("file:///", "file:/", 1)
                for name, url in origins.items()),
            "all observed original classes from exact selected archive")
    setup = value["setup_metadata"]
    require(type(setup) is dict and set(setup) == {
        "spawn", "constructor_map_count", "constructor_insertion_count",
        "constructor_pending_count", "constructor_queue_count", "provider_numerical_state"},
        "complete genuine constructor metadata")
    initial_count = 0 if scenario == "load" else 1
    require(setup["spawn"] == [0, 64, 0] and
            all(type(item) is int for item in setup["spawn"]) and
            type(setup["constructor_map_count"]) is int and
            setup["constructor_map_count"] == initial_count and
            type(setup["constructor_insertion_count"]) is int and
            setup["constructor_insertion_count"] == initial_count and
            type(setup["constructor_pending_count"]) is int and
            setup["constructor_pending_count"] == 0 and
            type(setup["constructor_queue_count"]) is int and
            setup["constructor_queue_count"] == 0 and
            setup["provider_numerical_state"] ==
            "outside cache-callback seam; per-VM Random seeds retained by read-only debugger",
            "genuine constructor baseline and explicit provider boundary")
    if scenario == "load":
        api = value["time_api"]
        require(type(api) is dict and set(api) == {
            "class", "method", "signature", "modifiers", "value", "before", "after"} and
            api["class"] == bindings["world"] and api["method"] == "a" and
            api["signature"] == "(J)V" and type(api["modifiers"]) is int and
            api["modifiers"] == 1 and type(api["value"]) is int and api["value"] == 1234 and
            api["before"] == "0000000000000000" and api["after"] == "00000000000004d2",
            "real nonzero public world-time setter call")
    else:
        require(value["time_api"] is None, "population profile has no time mutation")
    schedule = operation_plan(scenario)
    require(type(value["operations"]) is list and len(value["operations"]) == len(schedule),
            "complete fixed original callback schedule")
    ids = acquire.identities(value)
    initial = snapshot_projection(value["initial"], ids, acquire, finish)
    require(len(initial["world"]["chunks"]) == initial_count + 1 and
            len(initial["world"]["map"]) == len(initial["world"]["insertions"]) == initial_count and
            initial["world"]["pending"] == initial["world"]["queue"] == [],
            "complete actual constructor cache baseline")
    previous, previous_raw = initial, value["initial"]
    expected = {"schema": 1, "scope": "cache-callback-boundary", "seed_hex": SEED,
                "initial": initial, "operations": []}
    for index, (row, (method, x, z)) in enumerate(zip(value["operations"], schedule)):
        require(type(row) is dict and set(row) == {
            "index", "method", "coordinates", "result", "before", "after"} and
            type(row["index"]) is int and row["index"] == index and
            row["method"] == method and row["coordinates"] == [x, z] and
            all(type(item) is int for item in row["coordinates"]),
            "exact ordered original operation")
        for snapshot in (row["before"], row["after"]):
            retained_snapshot_owners(snapshot, value["initial"], acquire)
            retained_snapshot_owners(snapshot, previous_raw, acquire)
        before = snapshot_projection(row["before"], ids, acquire, finish)
        require(before == previous, "continuous complete original cache state")
        new = [identity for identity in acquire.chunk_map(row["after"]) if identity not in ids]
        require(len(new) == (1 if index < (1 if scenario == "load" else 3) else 0),
                "only actual outer load/generation adds a chunk identity")
        result = acquire.ref(row["result"])
        require(result is not None and (not new or new == [result]),
                "fresh result is exact newly observed original chunk")
        for identity in new:
            ids[identity] = len(ids)
        require(result in ids, "returned retained identity")
        after = snapshot_projection(row["after"], ids, acquire, finish)
        returned = after["world"]["chunks"][ids[result]]
        require(returned["coordinates"] == [x, z] and not returned["blank"] and
                returned["flags"]["c"] is True,
                "actual ordinary result activated at requested coordinates")
        require(after["context"] == initial["context"], "original live context retained")
        if scenario == "load":
            require(returned["time"] == "00000000000004d2" and
                    returned["data"]["minimum_height"] == 0 and
                    returned["flags"] == {"c": True, "n": False, "o": False, "p": False, "q": False} and
                    after["world"]["queue"] == [],
                    "loaded flags, nonzero stamp and unchanged deserialized minimum")
        if not new:
            require(after == before and row["after"]["cached_locals"] == row["before"]["cached_locals"],
                    "repeated cache hit preserves full local values and owners")
        expected["operations"].append({"index": index, "method": method,
            "coordinates": [x, z], "result": ids[result], "status": "ok", "snapshot": after})
        previous, previous_raw = after, row["after"]
    if scenario == "population":
        populated = [chunk for chunk in previous["world"]["chunks"]
                     if chunk["flags"]["n"]]
        require(len(populated) == 1 and populated[0]["coordinates"] == [0, 0] and
                populated[0]["flags"]["o"] is True,
                "one naturally populated northwest chunk marked dirty")
    elif scenario == "prepare":
        retained_snapshot_owners(value["saved"], previous_raw, acquire)
        saved = snapshot_projection(value["saved"], ids, acquire, finish)
        after_save = copy.deepcopy(previous)
        for chunk in after_save["world"]["chunks"]:
            if not chunk["blank"]:
                chunk["flags"]["o"] = False
        require(saved == after_save,
                "actual save preserves all complete payloads and clears normal dirty flags")
    return expected, ids


def validate_saved_payload(preparation, loaded):
    require(preparation["scenario"] == "prepare" and loaded["scenario"] == "load" and
            preparation["side"] == loaded["side"] and
            preparation["seed_hex"] == loaded["seed_hex"] == SEED,
            "same original side and seed for copied-world reload")

    def selected(snapshot):
        rows = [row for row in snapshot["cached_locals"] if row["coordinates"] == [1, 0]]
        require(len(rows) == 1, "one complete saved/loaded target payload")
        return rows[0]

    saved = selected(preparation["saved"])
    restored = selected(loaded["operations"][0]["after"])
    for name in ("blocks", "metadata", "skylight", "block_light", "heightmap"):
        require(same_json(saved["data"][name], restored["data"][name]),
                "complete original saved/loaded array unchanged: " + name)
    require(saved["data"]["minimum_height"] == 66 and
            restored["data"]["minimum_height"] == 0 and
            saved["flags"]["o"] is False and restored["flags"]["o"] is False,
            "deserialization preserves saved arrays without recomputing the minimum")


def encode_snapshot(value, dedicated):
    require(type(value) is dict and set(value) == {"context", "world"},
            "complete MCC1 snapshot projection")
    context, world = value["context"], value["world"]
    require(type(context) is dict and set(context) == {
        "dedicated", "world_time", "world_force", "provider_force", "spawn", "no_sky"},
        "complete MCC1 live context")
    require(context["dedicated"] is dedicated and type(dedicated) is bool,
            "stable dispatch policy")
    world_force = boolean(context["world_force"], "world force flag")
    if dedicated:
        provider_force = boolean(context["provider_force"], "dedicated provider force flag")
    else:
        require(context["provider_force"] is None, "base provider force field is absent")
        provider_force = False
    require(type(context["spawn"]) is list and len(context["spawn"]) == 2,
            "complete spawn x/z")
    spawn = [integer(item, -(1 << 31), (1 << 31) - 1, "spawn")
             for item in context["spawn"]]
    no_sky = boolean(context["no_sky"], "no-sky flag")
    require(type(world) is dict and set(world) == {
        "map", "insertions", "pending", "chunks", "queue", "depth"},
        "complete MCC1 cache world")
    arrays = (world["chunks"], world["map"], world["insertions"],
              world["pending"], world["queue"])
    require(all(type(rows) is list for rows in arrays), "complete MCC1 collection arrays")
    limits = ((1, 32), (0, 64), (0, 64), (0, 64), (0, 32768))
    for rows, (low, high) in zip(arrays, limits):
        integer(len(rows), low, high, "MCC1 collection length")
    depth = integer(world["depth"], -(1 << 31), (1 << 31) - 1, "enqueue depth")
    data = bytearray(struct.pack(">q??iii?IIIII", signed_time(context["world_time"]),
        world_force, provider_force, *spawn, depth, no_sky, *map(len, arrays)))
    count = len(world["chunks"])
    for number, chunk in enumerate(world["chunks"]):
        require(type(chunk) is dict and set(chunk) == {
            "id", "coordinates", "blank", "flags", "time", "data"} and
            type(chunk["id"]) is int and chunk["id"] == number,
            "complete contiguous stable chunk identities")
        require(type(chunk["coordinates"]) is list and len(chunk["coordinates"]) == 2,
                "chunk coordinate pair")
        coordinates = [integer(item, -(1 << 31), (1 << 31) - 1, "chunk coordinate")
                       for item in chunk["coordinates"]]
        blank = boolean(chunk["blank"], "blank dispatch")
        require(number != 0 or blank, "ID0 is retained blank fallback")
        flags = chunk["flags"]
        require(type(flags) is dict and set(flags) == {"c", "n", "o", "p", "q"},
                "complete chunk flags")
        flags_ = [boolean(flags[name], "chunk flag") for name in ("o", "c", "n", "p", "q")]
        local = chunk["data"]
        if blank:
            require(local is None, "blank dispatch has no local-array projection")
            minimum, height = 0, bytes(256)
        else:
            require(type(local) is dict and set(local) == {
                "blocks", "metadata", "skylight", "block_light", "heightmap", "minimum_height"},
                "all five normal chunk arrays")
            minimum = integer(local["minimum_height"], -(1 << 31), (1 << 31) - 1,
                              "minimum height")
            height = raw_array(local["heightmap"], 256, "heightmap")
        data += struct.pack(">ii??????qi", *coordinates, blank, *flags_,
                            signed_time(chunk["time"]), minimum)
        data += height
        if not blank:
            for name, length in (("blocks", 32768), ("metadata", 16384),
                                 ("skylight", 16384), ("block_light", 16384)):
                data += raw_array(local[name], length, name)
    seen = set()
    for row in world["map"]:
        require(type(row) is list and len(row) == 2, "map key/identity pair")
        key = integer(row[0], -(1 << 31), (1 << 31) - 1, "map key")
        identity = integer(row[1], 0, count - 1, "mapped identity")
        require(key not in seen, "unique map key")
        seen.add(key)
        data += struct.pack(">iI", key, identity)
    for identity in world["insertions"]:
        data += struct.pack(">I", integer(identity, 0, count - 1, "insertion identity"))
    seen.clear()
    for key in world["pending"]:
        key = integer(key, -(1 << 31), (1 << 31) - 1, "pending key")
        require(key not in seen, "unique pending membership")
        seen.add(key)
        data += struct.pack(">i", key)
    for row in world["queue"]:
        require(type(row) is list and len(row) == 7, "complete queued light region")
        kind = integer(row[0], 0, 1, "light kind")
        bounds = [integer(item, -(1 << 31), (1 << 31) - 1, "light bound")
                  for item in row[1:]]
        require(all(bounds[axis] <= bounds[axis + 3] for axis in range(3)),
                "ordered light bounds")
        data += struct.pack(">Iiiiiii", kind, *bounds)
    return data


def encode_replay(value):
    require(type(value) is dict and set(value) == {
        "seed_hex", "initial", "operations", "scripts"}, "complete MCC1 replay model")
    require(value["seed_hex"] == SEED, "fixed genuine seed23 callback profile")
    dedicated = value["initial"]["context"]["dedicated"]
    boolean(dedicated, "dispatch policy")
    operations, scripts = value["operations"], value["scripts"]
    require(type(operations) is list and 1 <= len(operations) <= 16 and
            type(scripts) is list and len(scripts) <= 32, "bounded replay schedule")
    data = bytearray(b"MCC1")
    data += struct.pack(">q?II", signed_time(value["seed_hex"]), dedicated,
                        len(operations), len(scripts))
    data += encode_snapshot(value["initial"], dedicated)
    methods = {"b": 0, "c": 1, "d": 2, "populate": 3}
    for operation in operations:
        require(type(operation) is dict and set(operation) == {"method", "coordinates"} and
                type(operation["method"]) is str and operation["method"] in methods and
                type(operation["coordinates"]) is list and len(operation["coordinates"]) == 2,
                "complete replay operation")
        coordinates = [integer(item, -(1 << 31), (1 << 31) - 1, "operation coordinate")
                       for item in operation["coordinates"]]
        data += struct.pack(">Iii", methods[operation["method"]], *coordinates)
    for script in scripts:
        require(type(script) is dict and set(script) == {
            "kind", "coordinates", "result", "returned_id", "before", "after"} and
            type(script["kind"]) is str and script["kind"] in ("load", "generate", "populate"),
            "complete external callback script")
        kind = {"load": 0, "generate": 1, "populate": 2}[script["kind"]]
        result = script["result"]
        require(type(result) is str and result in
                (("loaded", "miss") if kind == 0 else ("success",)),
                "explicit original callback result")
        returned = script["returned_id"]
        if kind == 2 or result == "miss":
            require(returned is None, "no returned chunk at this callback seam")
            returned = (1 << 32) - 1
        else:
            integer(returned, 0, len(script["after"]["world"]["chunks"]) - 1,
                    "callback returned identity")
        require(type(script["coordinates"]) is list and len(script["coordinates"]) == 2,
                "callback coordinate pair")
        coordinates = [integer(item, -(1 << 31), (1 << 31) - 1, "callback coordinate")
                       for item in script["coordinates"]]
        data += struct.pack(">IiiII", kind, *coordinates, int(result == "miss"), returned)
        data += encode_snapshot(script["before"], dedicated)
        data += encode_snapshot(script["after"], dedicated)
        require(len(data) <= MAX_C_RECORD, "bounded complete MCC1 input")
    require(len(data) <= MAX_C_RECORD, "bounded complete MCC1 input")
    return bytes(data)


def load_protocol():
    path = ROOT / "scripts/reference/cache/callback_protocol.py"
    data = path.read_bytes()
    require(len(data) <= 2 * 1024 * 1024, "bounded callback protocol source")
    module = types.ModuleType("_callbacks_protocol")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    return module, {"path": str(path), "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest()}


def validate_command(command, arguments, tools, classes):
    expected = [str(tools["java"]), "-Xint", "-Xmx512m",
        "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
        "-cp", str(classes), "CacheCallbacksObservation", *map(str, arguments)]
    require(type(command) is list and command == expected,
            "exact owned interpreted loopback debuggee command")


def direct_unavailable(process, snapshot, directory, prefix):
    require(process.returncode == 77 and process.stop_reason is None and process.timed_out is False and
            not snapshot.exists() and not snapshot.is_symlink(),
            "direct setup77 without any completed snapshot")
    lines = (directory / (prefix + "-err.txt")).read_text(encoding="utf-8").splitlines()
    require("UNAVAILABLE: genuine callback setup" in lines and
            "FAILURE: callback observation" not in lines,
            "owned helper proves genuine pre-observation constructor failure")


def setup_unavailable(common, process, trace, snapshot, side, scenario, arguments, tools, classes, finish):
    require(process.returncode == 77 and process.stop_reason is None and process.timed_out is False and
            not trace.exists() and not trace.is_symlink() and
            not snapshot.exists() and not snapshot.is_symlink(),
            "controller setup77 has no trace or completed snapshot")
    proof_path = Path(str(trace) + ".unavailable.json")
    require(proof_path.is_file() and not proof_path.is_symlink() and
            proof_path.stat().st_size <= 65536, "bounded owned unavailable proof")
    proof = common.strict_json(proof_path.read_bytes())
    require(type(proof) is dict and set(proof) == {
        "schema", "status", "stage", "child_exit", "side", "mode", "scenario",
        "jar_sha256", "begin_markers", "end_markers", "completed_operations",
        "identity_markers", "snapshot_started", "snapshot_exists", "child_command",
        "debuggee_mode", "elapsed_ms"}, "complete original setup-only proof")
    for name, expected in (("schema", 1), ("child_exit", 77), ("begin_markers", 0),
                           ("end_markers", 0), ("completed_operations", 0),
                           ("identity_markers", 0)):
        require(type(proof[name]) is int and proof[name] == expected,
                "no measured original state in setup proof: " + name)
    require(proof["status"] == "unavailable" and proof["stage"] == "original_setup" and
            proof["side"] == side and proof["mode"] == proof["scenario"] == scenario and
            proof["jar_sha256"] == finish.JAR_HASHES[side] and
            proof["snapshot_started"] is False and proof["snapshot_exists"] is False and
            proof["debuggee_mode"] == "interpreted", "exact original setup-only boundary")
    integer(proof["elapsed_ms"], 1, 90000, "bounded unavailable controller")
    validate_command(proof["child_command"], arguments, tools, classes)
    child_error = Path(str(trace) + ".child-stderr.txt")
    require(child_error.is_file() and not child_error.is_symlink() and
            child_error.stat().st_size <= MAX_RECORD, "retained unavailable child stderr")
    lines = child_error.read_text(encoding="utf-8").splitlines()
    require("UNAVAILABLE: genuine callback setup" in lines and
            "FAILURE: callback observation" not in lines,
            "original helper independently reports constructor unavailability")
    return proof


def check_child_streams(common, trace, directory):
    for name in ("child_stdout", "child_stderr"):
        record = trace[name]
        expected_name = "trace.json." + ("child-stdout.txt" if name == "child_stdout" else "child-stderr.txt")
        require(type(record) is dict and set(record) == {"file", "bytes", "sha256"} and
                type(record["file"]) is str and record["file"] == expected_name and
                Path(record["file"]).name == record["file"] and
                "/" not in record["file"] and "\\" not in record["file"] and
                type(record["bytes"]) is int and 0 <= record["bytes"] <= MAX_RECORD and
                type(record["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", record["sha256"]),
                "complete relative original child stream record")
        actual = common.file_record(directory / record["file"], byte_limit=MAX_RECORD)
        require(all(actual[key] == record[key] for key in ("bytes", "sha256")),
                "retained original child stream fingerprint")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--capture-only", action="store_true")
    parser.add_argument("--case", type=int, action="append")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--tested-commit", default="")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    common, report, output, inputs = None, None, None, {}
    baselines = {}
    status, executing, proved_unavailable = 1, False, False
    try:
        common, old, finish, normal, acquire, executed = helpers()
        protocol, protocol_source = load_protocol()
        executed.append(protocol_source)
        require(args.capture_only or args.executable is not None,
                "C executable required outside original-only capture")
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit),
                "full lowercase source ID")
        selected = args.case if args.case is not None else list(range(len(CASES)))
        require(selected and len(set(selected)) == len(selected) and
                all(type(index) is int and 0 <= index < len(CASES) for index in selected),
                "distinct fixed callback case selection")
        tools, jars = runtime(common, args, finish)
        if not args.capture_only and not args.executable.is_file():
            raise common.Unavailable("missing C callback probe executable")
        sources = [ROOT / name for name in SOURCES]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(),
                      ROOT / "CMakeLists.txt", *sources,
                      ROOT / "tests/world/cache/callback_probe.c"]
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [Path(row["path"]) for row in executed] + list(jars.values()) + list(tools.values())
        if not args.capture_only:
            attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024)
                  for path in attributed}
        for row in executed:
            require(inputs[str(Path(row["path"]).resolve())]["sha256"] == row["sha256"],
                    "executed protocol/helper module attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve()
        report = {"schema": 1, "status": "running", "scope": "cache-callback-boundary",
            "mode": "original-only" if args.capture_only else "live-original-three-way",
            "tested_commit": args.tested_commit or None,
            "source_label": "Caller label; actual sources/helpers/executable fingerprinted.",
            "started_utc": common.utc_now(), "artifact_root": str(output),
            "python": sys.version, "platform": platform.platform(),
            "selected_cases": selected, "inputs_before": inputs,
            "executed_source_modules": executed, "preparations": {}, "cases": [], "errors": [],
            "external_effects": "Observed original loader, generator and terrain-feature bodies are replayed at explicit C callbacks; only coordinator transitions are computed by C.",
            "limits": {"controller_seconds": 90, "parent_child_seconds": 120,
                "java_heap_mib": 512, "original_record_bytes": MAX_RECORD,
                "c_record_bytes": MAX_C_RECORD, "world_files": 256,
                "world_bytes": MAX_RECORD,
                "policy": "Bounded processes and inventories, not an OS process-tree/filesystem quota."}}
        common.write_json(output / "manifest.json", report)
        for name in ("source", "classes", "empty", "compile-work"):
            (output / name).mkdir()
        for path in sources:
            (output / "source" / path.name).write_bytes(path.read_bytes())
        common.tool_versions(tools, output, report)
        modules = run_child(common, "jdk-modules", [str(tools["java"]), "--list-modules"], output, seconds=15)
        report["jdk_module_process"] = asdict(modules)
        success(modules, "JDK module preflight")
        available = {line.split("@", 1)[0] for line in
                     (output / "jdk-modules-out.txt").read_text(encoding="utf-8").splitlines()}
        if not {"jdk.jdi", "jdk.jdwp.agent"}.issubset(available):
            raise common.Unavailable("JDK modules jdk.jdi and jdk.jdwp.agent required")
        executing = True
        compiled = run_child(common, "compile", [str(tools["javac"]), "-J-Xmx512m",
            "--release", "17", "--add-modules", "jdk.jdi", "-proc:none", "-implicit:none",
            "-encoding", "UTF-8", "-Xlint:all", "-Werror", "-classpath", str(output / "empty"),
            "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"),
            "-d", str(output / "classes"), *[str(output / "source" / path.name) for path in sources]],
            output / "compile-work", seconds=30)
        report["compile"] = asdict(compiled)
        success(compiled, "compilation")
        classes = sorted((output / "classes").glob("*.class"))
        families = tuple(path.stem for path in sources)
        require(3 <= len(classes) <= 16 and all(any(path.stem == family or
                path.stem.startswith(family + "$") for family in families) for path in classes),
                "complete known compiled helper families")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                                    for path in classes}
        shared, preparation_values = {}, {}
        for index in selected:
            side, scenario = CASES[index]
            if scenario == "load" and side not in baselines:
                directory = output / ("prepare-" + side)
                directory.mkdir()
                world, snapshot = directory / "world", directory / "snapshots.json"
                arguments = [str(jars[side]), side, "prepare", SEED, str(world), str(snapshot)]
                process = run_child(common, "prepare", [str(tools["java"]), "-Xint", "-Xmx512m",
                    "-cp", str(output / "classes"), "CacheCallbacksObservation", *arguments], directory,
                    seconds=90, world=world)
                row = {"process": asdict(process)}
                report["preparations"][side] = row
                if process.returncode == 77 and not process.stop_reason:
                    direct_unavailable(process, snapshot, directory, "prepare")
                    proved_unavailable = True
                    raise common.Unavailable("genuine creator constructor unavailable: " + side)
                success(process, "genuine saved-world preparation")
                require(snapshot.stat().st_size <= MAX_RECORD, "bounded creator snapshot")
                value = common.strict_json(snapshot.read_bytes())
                validate_observation(value, side, "prepare", jars[side], acquire, finish)
                row["snapshot_record"] = common.file_record(snapshot, byte_limit=MAX_RECORD)
                row["world_after"] = fingerprint_inventory(common, world)
                baseline = directory / "baseline"
                row["baseline_before"] = copy_world(common, world, baseline)
                baselines[side], preparation_values[side] = baseline, value
            directory = output / (f"case-{index}-" + side + "-" + scenario)
            directory.mkdir()
            row = {"index": index, "side": side, "scenario": scenario, "comparisons": {}}
            report["cases"].append(row)
            world, snapshot, trace = directory / "world", directory / "snapshots.json", directory / "trace.json"
            if scenario == "load":
                row["world_before"] = copy_world(common, baselines[side], world)
            arguments = [str(jars[side]), side, scenario, SEED, str(world), str(snapshot)]
            process = run_child(common, "controller", [str(tools["java"]), "--add-modules", "jdk.jdi",
                "-Xmx512m", "-cp", str(output / "classes"), "CacheCallbacksController", *arguments, str(trace)],
                directory, world=world)
            row["controller"] = asdict(process)
            if process.returncode == 77 and not process.stop_reason:
                row["unavailable"] = setup_unavailable(common, process, trace, snapshot, side,
                    scenario, arguments, tools, output / "classes", finish)
                proved_unavailable = True
                raise common.Unavailable("genuine original constructor unavailable: case " + str(index))
            success(process, "traced original callback boundaries")
            require(snapshot.stat().st_size <= MAX_RECORD and trace.stat().st_size <= MAX_RECORD,
                    "complete bounded original artifacts")
            value, measured = common.strict_json(snapshot.read_bytes()), common.strict_json(trace.read_bytes())
            validate_command(measured["child_command"], arguments, tools, output / "classes")
            check_child_streams(common, measured, directory)
            expected, ids = validate_observation(value, side, scenario, jars[side], acquire, finish)
            replay_model, expected = protocol.validate_trace(value, measured, expected, ids, acquire, finish)
            if scenario == "load":
                validate_saved_payload(preparation_values[side], value)
            control_world, control = directory / "control-world", directory / "control.json"
            if scenario == "load":
                row["control_world_before"] = copy_world(common, baselines[side], control_world)
            control_arguments = [str(jars[side]), side, scenario, SEED, str(control_world), str(control)]
            process = run_child(common, "control", [str(tools["java"]), "-Xint", "-Xmx512m",
                "-cp", str(output / "classes"), "CacheCallbacksObservation", *control_arguments],
                directory, seconds=90, world=control_world)
            row["control"] = asdict(process)
            if process.returncode == 77 and not process.stop_reason:
                direct_unavailable(process, control, directory, "control")
                proved_unavailable = True
                raise common.Unavailable("genuine untouched constructor unavailable: case " + str(index))
            success(process, "untouched original control")
            require(control.stat().st_size <= MAX_RECORD and control.read_bytes() == snapshot.read_bytes(),
                    "entire untouched control differs")
            row["control_equal"] = True
            for name, path in (("snapshot_record", snapshot), ("trace_record", trace), ("control_record", control)):
                row[name] = common.file_record(path, byte_limit=MAX_RECORD)
            row["world_after"] = fingerprint_inventory(common, world)
            row["control_world_after"] = fingerprint_inventory(common, control_world)
            if scenario == "load":
                preserved_reload_files(row["world_before"], row["world_after"])
                preserved_reload_files(row["control_world_before"], row["control_world_after"])
            replay = directory / "input.mcc"
            replay.write_bytes(encode_replay(replay_model))
            row["replay_input"] = common.file_record(replay, byte_limit=MAX_C_RECORD)
            common.write_json(directory / "replay.json", replay_model)
            common.write_json(directory / "expected.json", expected)
            row["replay_record"] = common.file_record(directory / "replay.json", byte_limit=MAX_C_RECORD)
            row["projection_record"] = common.file_record(directory / "expected.json", byte_limit=MAX_C_RECORD)
            if not args.capture_only:
                process = run_child(common, "c", [str(args.executable.resolve())], directory,
                    seconds=90, stdin=replay, limit=MAX_C_RECORD)
                row["c_process"] = asdict(process)
                success(process, "C coordinator callback boundaries")
                actual = common.strict_json((directory / "c-out.txt").read_bytes())
                require(same_json(actual, expected), "C/" + side + " complete callback boundary differs")
                row["comparisons"]["C/" + side] = "pass"
                row["c_record"] = common.file_record(directory / "c-out.txt", byte_limit=MAX_C_RECORD)
            require(common.file_record(replay, byte_limit=MAX_C_RECORD) == row["replay_input"],
                    "immutable C replay input changed")
            row["operations"] = len(expected["operations"])
            row["callbacks"] = len(replay_model["scripts"])
            row["stage_events"] = sum(len(operation["events"]) for operation in measured["operations"])
            shared.setdefault(scenario, {})[side] = expected
            common.write_json(output / "manifest.json", report)
        report["client/server"] = {}
        for scenario, sides in shared.items():
            if set(sides) == {"client", "server"}:
                require(same_json(sides["client"], sides["server"]),
                        "complete shared client/server callback boundary differs: " + scenario)
                report["client/server"][scenario] = "pass"
            else:
                report["client/server"][scenario] = "not selected; no cross-context result claimed"
        for side, baseline in baselines.items():
            after = fingerprint_inventory(common, baseline)
            report["preparations"][side]["baseline_after"] = after
            require(after == report["preparations"][side]["baseline_before"],
                    "preserved creator baseline changed")
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
            for side, baseline in baselines.items():
                try:
                    after = fingerprint_inventory(common, baseline)
                    row = report["preparations"][side]
                    row["baseline_after"] = after
                    require(after == row["baseline_before"],
                            "preserved creator baseline changed: " + side)
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
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
