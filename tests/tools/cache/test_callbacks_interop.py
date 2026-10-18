"""MCC1 transport and constructed callback contracts; these are not original parity cases."""

from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/cache/callback_fixtures.json"
MAX_FIXTURE = 8 * 1024 * 1024
MAX_ORIGINAL = 32 * 1024 * 1024
MAX_BYTES = 64 * 1024 * 1024
SEED = "0000000000000017"
CASES = (("client", "load"), ("server", "load"), ("client", "population"), ("server", "population"))
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
HELPERS = ("ChunkObservation.java", "CacheCallbacksObservation.java", "CacheCallbacksController.java")
MODULES = {"scripts/reference/caves/compare_caves.py", "scripts/reference/caves/fixtures.py",
           "scripts/reference/chunk/compare_chunk.py", "scripts/reference/chunk/compare_finish.py",
           "scripts/reference/generation/compare_normal.py", "scripts/reference/cache/compare_acquire.py",
           "scripts/reference/cache/callback_protocol.py"}
LENGTHS = {"blocks": 32768, "metadata": 16384, "skylight": 16384, "block_light": 16384, "heightmap": 256}


def load_driver():
    spec = importlib.util.spec_from_file_location("_callback_transport_driver", ROOT / "scripts/reference/cache/compare_callbacks.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


driver = load_driver()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def strict_json(raw):
    def pairs(values):
        result = {}
        for key, value in values:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result

    def finite(value):
        number = float(value)
        require(math.isfinite(number), "non-finite JSON number")
        return number

    return json.loads(raw, object_pairs_hook=pairs, parse_float=finite,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("non-finite JSON number")))


def unpack(value, limit=MAX_BYTES):
    require(type(value) is dict and set(value) == {"bytes", "sha256", "zlib_base64"} and
            type(value["bytes"]) is int and 0 < value["bytes"] <= limit and type(value["sha256"]) is str and
            len(value["sha256"]) == 64 and all(character in "0123456789abcdef" for character in value["sha256"]) and
            type(value["zlib_base64"]) is str and len(value["zlib_base64"]) <= 8 * 1024 * 1024, "bounded packed transport")
    encoded = base64.b64decode(value["zlib_base64"], validate=True)
    require(base64.b64encode(encoded).decode("ascii") == value["zlib_base64"], "canonical base64")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(encoded, value["bytes"] + 1)
    require(len(raw) == value["bytes"] and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and
            hashlib.sha256(raw).hexdigest() == value["sha256"], "complete packed transport fingerprint")
    return raw


def packed(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
            "zlib_base64": base64.b64encode(zlib.compress(raw)).decode("ascii")}


def json_equal(actual, expected):
    if type(actual) is not type(expected):
        return False
    if type(expected) is dict:
        return actual.keys() == expected.keys() and all(json_equal(actual[key], value) for key, value in expected.items())
    if type(expected) is list:
        return len(actual) == len(expected) and all(json_equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


def keys(value, names, context):
    require(type(value) is dict and set(value) == set(names), context + ": complete keys")


def fingerprint(value, limit=MAX_BYTES, *, empty=False):
    keys(value, ("bytes", "sha256"), "fingerprint")
    require(type(value["bytes"]) is int and (0 if empty else 1) <= value["bytes"] <= limit and
            type(value["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", value["sha256"]), "bounded fingerprint")


def relative_path(value):
    return (type(value) is str and bool(value) and not value.startswith("/") and ":" not in value and "\\" not in value and
            all(part not in ("", ".", "..") for part in value.split("/")))


def validate_inventory(value):
    keys(value, ("exists", "entries", "files", "directories", "bytes", "complete", "error"), "world inventory")
    require(value["exists"] is True and value["complete"] is True and value["error"] is None and
            type(value["entries"]) is list and len(value["entries"]) <= 256, "complete bounded world inventory")
    seen, files, directories, total = set(), 0, 0, 0
    for entry in value["entries"]:
        name = entry["path"]
        require(relative_path(name) and name not in seen, "unique relative inventory path")
        seen.add(name)
        if entry["kind"] == "file":
            keys(entry, ("path", "kind", "bytes", "sha256"), "inventory file")
            fingerprint({key: entry[key] for key in ("bytes", "sha256")}, MAX_ORIGINAL, empty=True)
            files += 1
            total += entry["bytes"]
        else:
            require(entry == {"path": name, "kind": "directory"}, "ordinary inventory directory")
            directories += 1
    require(all(type(value[name]) is int for name in ("files", "directories", "bytes")) and
            (value["files"], value["directories"], value["bytes"]) == (files, directories, total) and
            total <= MAX_ORIGINAL, "exact complete world inventory totals")


def validate_metadata(value):
    keys(value, ("schema", "format", "scope", "encoding", "external_effects", "projection", "provenance", "preparations", "cases"),
         "public callback fixture")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["format"] == "MCC1" and
            value["scope"] == "cache-callback-boundary" and type(value["encoding"]) is str and "MCC1" in value["encoding"] and
            type(value["projection"]) is str and value["projection"] and value["external_effects"] ==
            "Original loader, generator and terrain-feature bodies are observed callback effects; C computes coordinator transitions only.",
            "explicit complete callback fixture scope")
    origin = value["provenance"]
    counts = dict(cases=4, operations=12, external_callbacks=16, successful_loads=2, opaque_generations=6,
                  opaque_populations=2, stage_events=394, marker_inventories=24, complete_untouched_controls=4)
    keys(origin, ("mode", "jar_sha256", "source_fingerprints", "helper_sources", "helper_classes", "helper_class_version",
                  "executed_source_modules", "tool_versions", "jdk_modules", "exporter_sha256", "manifest_sha256",
                  "binding_representation", *counts), "callback provenance")
    require(origin["mode"] == "original-only" and origin["jar_sha256"] == JAR_HASHES and
            origin["binding_representation"] == "fingerprints-only" and type(origin["helper_class_version"]) is int and
            origin["helper_class_version"] == 61, "original-only pinned archive and Java17 helper provenance")
    for name in ("exporter_sha256", "manifest_sha256"):
        require(type(origin[name]) is str and re.fullmatch(r"[0-9a-f]{64}", origin[name]), "complete producer fingerprint")
    for name, count in counts.items():
        require(type(origin[name]) is int and origin[name] == count, "complete fixed original corpus counts")
    sources = origin["source_fingerprints"]
    required = MODULES | {"scripts/reference/cache/compare_callbacks.py", "tests/world/cache/callback_probe.c", "CMakeLists.txt",
                          "src/world/cache/acquire.c", "src/world/cache/cache.c", "src/core/java/java.c",
                          "include/mc/world/cache/acquire.h", "include/mc/world/cache/cache.h", "include/mc/world/chunk/state.h"}
    required |= {"tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name for name in HELPERS}
    require(type(sources) is dict and len(sources) == 55 and required <= set(sources), "complete captured project source closure")
    for name, record in sources.items():
        require(relative_path(name) and (name == "CMakeLists.txt" or name.startswith(("scripts/", "tests/", "src/", "include/"))),
                "relative project source attribution")
        fingerprint(record, 512 * 1024 * 1024)
    keys(origin["helper_sources"], HELPERS, "all copied helper source fingerprints")
    for name in HELPERS:
        relative = "tests/reference/" + ("chunk/" if name == "ChunkObservation.java" else "cache/") + name
        require(json_equal(origin["helper_sources"][name], sources[relative]), "copied helper source attribution")
    expected_classes = {"ChunkObservation.class", "ChunkObservation$Model.class", "ChunkObservation$SetupUnavailable.class",
                        "CacheCallbacksObservation.class", "CacheCallbacksObservation$Observer.class", "CacheCallbacksObservation$SetupUnavailable.class",
                        "CacheCallbacksController.class", "CacheCallbacksController$Session.class"}
    keys(origin["helper_classes"], expected_classes, "complete captured helper class families")
    for record in origin["helper_classes"].values():
        fingerprint(record, 2 * 1024 * 1024)
    keys(origin["executed_source_modules"], MODULES, "complete executed interpretation modules")
    for name, record in origin["executed_source_modules"].items():
        require(json_equal(record, sources[name]), "executed source module attribution")
    keys(origin["tool_versions"], ("java", "javac", "javap"), "all attributed JDK tools")
    for name, record in origin["tool_versions"].items():
        keys(record, ("major", "output", "executable"), "tool version")
        require(type(record["major"]) is int and 17 <= record["major"] <= 999 and
                type(record["output"]) is str and 0 < len(record["output"]) <= 131073, "bounded JDK17 tool evidence")
        fingerprint(record["executable"], 512 * 1024 * 1024)
    keys(origin["jdk_modules"], ("required", "bytes", "sha256"), "JDK module evidence")
    require(origin["jdk_modules"]["required"] == ["jdk.jdi", "jdk.jdwp.agent"], "both debugger modules required")
    fingerprint({key: origin["jdk_modules"][key] for key in ("bytes", "sha256")}, MAX_ORIGINAL)
    keys(value["preparations"], ("client", "server"), "both genuine saved preparations")
    for prepared in value["preparations"].values():
        keys(prepared, ("snapshot", "snapshot_record", "world", "baseline"), "complete saved preparation")
        fingerprint(prepared["snapshot_record"], MAX_ORIGINAL)
        for name in ("world", "baseline"):
            validate_inventory(prepared[name])
        require(json_equal(prepared["world"], prepared["baseline"]), "entire saved world copied unchanged")
    rows = value["cases"]
    require(type(rows) is list and len(rows) == 4, "all four fixed original contexts")
    common_keys = {"index", "side", "scenario", "seed_hex", "schedule", "operations", "callbacks", "stage_events", "marker_inventories",
                   "control_equal", "jar_sha256", "bindings", "input", "expected", "protocol", "snapshot_record", "trace_record", "control_record",
                   "replay_input", "replay_record", "projection_record", "world_after", "control_world_after"}
    for index, (row, (side, scenario)) in enumerate(zip(rows, CASES)):
        keys(row, common_keys | ({"world_before", "control_world_before"} if scenario == "load" else set()), "complete fixed original case")
        require(type(row["index"]) is int and row["index"] == index and row["side"] == side and row["scenario"] == scenario and
                row["seed_hex"] == SEED and row["schedule"] == [list(item) for item in driver.operation_plan(scenario)], "ordered complete fixed corpus")
        for method, x, z in row["schedule"]:
            require(type(x) is int and type(z) is int, "actual integer scheduled coordinates")
        expected_counts = dict(operations=2 if scenario == "load" else 4, callbacks=1 if scenario == "load" else 7,
                               stage_events=42 if scenario == "load" else 155, marker_inventories=4 if scenario == "load" else 8)
        for name, count in expected_counts.items():
            require(type(row[name]) is int and row[name] == count, "complete case counts")
        expected_bindings = dict(zip(("world", "cache", "storage", "loader", "generator"),
                                     ("fd", "ok", "mx", "ld", "yf") if side == "client" else ("dj", "jb", "ie", "hb", "pk")))
        require(row["control_equal"] is True and row["jar_sha256"] == JAR_HASHES[side] and row["bindings"] == expected_bindings,
                "genuine base-world route and entire untouched control")
        for name in ("snapshot_record", "trace_record", "control_record", "replay_input", "replay_record", "projection_record"):
            fingerprint(row[name], MAX_ORIGINAL if name in ("snapshot_record", "trace_record", "control_record") else MAX_BYTES)
        require(json_equal(row["snapshot_record"], row["control_record"]), "entire original control fingerprint equality")
        for name, record in (("input", "replay_input"), ("expected", "projection_record")):
            require(type(row[name]) is dict and json_equal({key: row[name].get(key) for key in ("bytes", "sha256")}, row[record]),
                    "retained complete replay/projection fingerprints")
        keys(row["protocol"], ("binding_representation", "snapshot", "trace"), "complete public behavioral protocol")
        require(row["protocol"]["binding_representation"] == "fingerprints-only", "public proof omits original bytecode")
        for name in ("world_after", "control_world_after"):
            validate_inventory(row[name])
        if scenario == "load":
            for before, after in (("world_before", "world_after"), ("control_world_before", "control_world_after")):
                validate_inventory(row[before])
                require(json_equal(row[before], value["preparations"][side]["baseline"]), "complete immutable saved baseline supplied")
                driver.preserved_reload_files(row[before], row[after])


def public_documents(row):
    snapshot_raw = unpack(row["protocol"]["snapshot"], MAX_ORIGINAL)
    trace_raw = unpack(row["protocol"]["trace"], MAX_ORIGINAL)
    for raw in (snapshot_raw, trace_raw):
        require(b'"bytecode_hex"' not in raw and b"file:/" not in raw and re.search(rb"[A-Za-z]:[\\/]", raw) is None,
                "public protocol has no original bytecode or private host path")
    snapshot, trace = strict_json(snapshot_raw), strict_json(trace_raw)
    require(type(snapshot["class_origins"]) is dict and 20 <= len(snapshot["class_origins"]) <= 128 and
            set(snapshot["class_origins"].values()) == {"supplied:" + row["side"]}, "logical selected archive origins")
    expected_command = ["tool:java", "-Xint", "-Xmx512m",
        "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", "helper:classes", "CacheCallbacksObservation",
        "supplied:" + row["side"], row["side"], row["scenario"], SEED, "case:world", "case:snapshots.json"]
    require(trace["child_command"] == expected_command, "exact logical original child command")
    for name, record in trace["bindings"].items():
        keys(record, ("class", "method", "signature", "modifiers", "bytecode_length", "bytecode_sha256"), "fingerprint-only binding")
        require(type(record["bytecode_length"]) is int and 0 < record["bytecode_length"] <= 1048576 and
                type(record["bytecode_sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", record["bytecode_sha256"]), "complete original binding fingerprint")
    return snapshot, trace


def rebind_public(snapshot, trace, side, scenario, root):
    snapshot, trace = copy.deepcopy(snapshot), copy.deepcopy(trace)
    jar, classes, java = root / (side + " selected.jar"), root / "classes", root / "java"
    snapshot["class_origins"] = dict.fromkeys(snapshot["class_origins"], jar.resolve().as_uri().replace("file:///", "file:/", 1))
    if trace is not None:
        arguments = [str(jar), side, scenario, SEED, str(root / "world"), str(root / "snapshots.json")]
        trace["child_command"] = [str(java), "-Xint", "-Xmx512m",
            "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", str(classes), "CacheCallbacksObservation", *arguments]
        driver.validate_command(trace["child_command"], arguments, {"java": java}, classes)
    return snapshot, trace, jar


def load_fixture(path=FIXTURE):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= MAX_FIXTURE, "bounded ordinary public fixture")
    value = strict_json(path.read_bytes())
    validate_metadata(value)
    common, old, finish, normal, acquire, executed = driver.helpers()
    protocol, source = driver.load_protocol()
    fake_root = Path(tempfile.gettempdir()).resolve() / "mc-callback-public-proof"
    prepared = {}
    for side, record in value["preparations"].items():
        raw = unpack(record["snapshot"], MAX_ORIGINAL)
        require(b"file:/" not in raw and re.search(rb"[A-Za-z]:[\\/]", raw) is None, "public saved preparation host provenance")
        document = strict_json(raw)
        require(set(document["class_origins"].values()) == {"supplied:" + side}, "logical saved preparation origin")
        document, _, jar = rebind_public(document, None, side, "prepare", fake_root)
        driver.validate_observation(document, side, "prepare", jar, acquire, finish)
        prepared[side] = document
    decoded = []
    for row in value["cases"]:
        original, measured = public_documents(row)
        snapshot, trace, jar = rebind_public(original, measured, row["side"], row["scenario"], fake_root)
        expected, ids = driver.validate_observation(snapshot, row["side"], row["scenario"], jar, acquire, finish)
        if row["scenario"] == "load":
            driver.validate_saved_payload(prepared[row["side"]], snapshot)
        replay, expected = protocol.validate_trace(snapshot, trace, expected, ids, acquire, finish, public_proof=True)
        raw, projected = unpack(row["input"]), strict_json(unpack(row["expected"]))
        require(driver.encode_replay(replay) == raw and json_equal(decode_replay(raw), replay), "exact complete original MCC1 transport")
        require(json_equal(projected, expected), "full projected behavior derived from original protocol")
        require(len(replay["scripts"]) == row["callbacks"] and len(projected["operations"]) == row["operations"], "complete replay corpus")
        decoded.append(dict(record=row, input=raw, expected=projected, snapshot=original, trace=measured, replay=replay))
    require(json_equal(decoded[0]["expected"], decoded[1]["expected"]) and
            json_equal(decoded[2]["expected"], decoded[3]["expected"]), "complete shared client/server scenarios")
    return value, decoded


class Reader:
    def __init__(self, raw):
        require(type(raw) is bytes and 0 < len(raw) <= MAX_BYTES, "bounded MCC1 bytes")
        self.raw, self.position = raw, 0

    def take(self, size):
        require(0 <= size <= len(self.raw) - self.position, "complete MCC1 bytes")
        value = self.raw[self.position:self.position + size]
        self.position += size
        return value

    def read(self, format):
        return struct.unpack(format, self.take(struct.calcsize(format)))

    def boolean(self):
        value = self.take(1)[0]
        require(value <= 1, "canonical MCC1 boolean")
        return bool(value)

    def snapshot(self, dedicated):
        time, = self.read(">q")
        world_force, provider_force = self.boolean(), self.boolean()
        require(dedicated or not provider_force, "absent base force field uses false slot")
        spawn_x, spawn_z, depth = self.read(">iii")
        no_sky = self.boolean()
        nc, ne, ni, np, nq = self.read(">5I")
        require(1 <= nc <= 32 and max(ne, ni, np) <= 64 and nq <= 32768, "bounded MCC1 collections")
        chunks = []
        for number in range(nc):
            x, z = self.read(">ii")
            blank, dirty, active, populated, never, entities = (self.boolean() for _ in range(6))
            saved, minimum = self.read(">qi")
            height = self.take(256)
            data = None
            if blank:
                require(minimum == 0 and height == bytes(256), "canonical blank backing is independently observed zero")
            else:
                data = {name: {"length": count, "values": self.take(count).hex()}
                        for name, count in LENGTHS.items() if name != "heightmap"}
                data.update(heightmap={"length": 256, "values": height.hex()}, minimum_height=minimum)
            chunks.append({"id": number, "coordinates": [x, z], "blank": blank,
                           "flags": dict(c=active, n=populated, o=dirty, p=never, q=entities),
                           "time": f"{saved & ((1 << 64) - 1):016x}", "data": data})
        require(chunks[0]["blank"], "blank fallback ID0")
        mappings, seen = [], set()
        for _ in range(ne):
            key, number = self.read(">iI")
            require(key not in seen and number < nc, "unique mapped key and retained identity")
            mappings.append([key, number])
            seen.add(key)
        insertions = [self.read(">I")[0] for _ in range(ni)]
        require(all(number < nc for number in insertions), "retained ordered insertion identities")
        pending = [self.read(">i")[0] for _ in range(np)]
        require(len(set(pending)) == len(pending), "unique pending membership")
        queue = [list(self.read(">I6i")) for _ in range(nq)]
        require(all(region[0] <= 1 for region in queue), "actual light kind")
        return {"context": dict(dedicated=dedicated, world_time=f"{time & ((1 << 64) - 1):016x}", world_force=world_force,
                                provider_force=provider_force if dedicated else None, spawn=[spawn_x, spawn_z], no_sky=no_sky),
                "world": dict(map=mappings, insertions=insertions, pending=pending, chunks=chunks, queue=queue, depth=depth)}


def decode_replay(raw):
    reader = Reader(raw)
    require(reader.take(4) == b"MCC1", "MCC1 magic")
    seed, = reader.read(">q")
    require(seed == 23, "fixed callback seed")
    dedicated = reader.boolean()
    operations, scripts = reader.read(">II")
    require(1 <= operations <= 16 and scripts <= 32, "bounded complete schedule")
    value = {"seed_hex": SEED, "initial": reader.snapshot(dedicated), "operations": [], "scripts": []}
    for _ in range(operations):
        method, x, z = reader.read(">Iii")
        require(method <= 3, "known coordinator operation")
        value["operations"].append({"method": ("b", "c", "d", "populate")[method], "coordinates": [x, z]})
    for _ in range(scripts):
        kind, x, z, result, returned = reader.read(">IiiII")
        require(kind <= 2 and (result <= 1 if kind == 0 else result == 0), "known callback kind/result")
        before, after = reader.snapshot(dedicated), reader.snapshot(dedicated)
        require(len(after["world"]["chunks"]) >= len(before["world"]["chunks"]), "retained identity inventory")
        for a, b in zip(before["world"]["chunks"], after["world"]["chunks"]):
            require(a["coordinates"] == b["coordinates"] and a["blank"] == b["blank"], "stable identity dispatch/coordinates")
        returns_chunk = kind == 1 or kind == 0 and result == 0
        if returns_chunk:
            require(returned == len(before["world"]["chunks"]) and len(after["world"]["chunks"]) == returned + 1 and
                    not after["world"]["chunks"][returned]["blank"], "one fresh normal result before publication")
        else:
            require(returned == 0xffffffff, "no value at this seam")
        value["scripts"].append({"kind": ("load", "generate", "populate")[kind], "coordinates": [x, z],
            "result": ("loaded" if result == 0 else "miss") if kind == 0 else "success",
            "returned_id": returned if returns_chunk else None, "before": before, "after": after})
    require(reader.position == len(raw), "no trailing MCC1 bytes")
    return value


def independent_encoding(model):
    """Encode the documented wire shape without the production driver's helpers."""
    dedicated = model["initial"]["context"]["dedicated"]

    def snapshot_bytes(value):
        context, world = value["context"], value["world"]
        raw = bytearray(struct.pack(">QBBiiiB5I", int(context["world_time"], 16), context["world_force"],
            context["provider_force"] if dedicated else False, *context["spawn"], world["depth"], context["no_sky"],
            *(len(world[name]) for name in ("chunks", "map", "insertions", "pending", "queue"))))
        for chunk in world["chunks"]:
            local, flags = chunk["data"], chunk["flags"]
            raw += struct.pack(">ii6BQi", *chunk["coordinates"], chunk["blank"],
                *(flags[name] for name in ("o", "c", "n", "p", "q")), int(chunk["time"], 16),
                0 if local is None else local["minimum_height"])
            raw += bytes(256) if local is None else bytes.fromhex(local["heightmap"]["values"])
            if local is not None:
                for name in ("blocks", "metadata", "skylight", "block_light"):
                    raw += bytes.fromhex(local[name]["values"])
        for key, identity in world["map"]:
            raw += struct.pack(">iI", key, identity)
        for identity in world["insertions"]:
            raw += struct.pack(">I", identity)
        for key in world["pending"]:
            raw += struct.pack(">i", key)
        for region in world["queue"]:
            raw += struct.pack(">I6i", *region)
        return raw

    raw = bytearray(b"MCC1" + struct.pack(">QBII", int(model["seed_hex"], 16), dedicated,
                                         len(model["operations"]), len(model["scripts"])))
    raw += snapshot_bytes(model["initial"])
    for operation in model["operations"]:
        raw += struct.pack(">Iii", ("b", "c", "d", "populate").index(operation["method"]), *operation["coordinates"])
    for script in model["scripts"]:
        raw += struct.pack(">IiiII", ("load", "generate", "populate").index(script["kind"]), *script["coordinates"],
                           script["result"] == "miss", 0xffffffff if script["returned_id"] is None else script["returned_id"])
        raw += snapshot_bytes(script["before"]) + snapshot_bytes(script["after"])
    return bytes(raw)


def chunk(number, coordinates=(0, 0), *, blank=False, active=False, dirty=False, populated=False, time=99):
    data = None
    if not blank:
        octets = {"blocks": 3, "metadata": 0x12, "skylight": 0x34, "block_light": 0x56, "heightmap": 17}
        data = {name: {"length": count, "values": f"{octets[name]:02x}" * count} for name, count in LENGTHS.items()}
        data["minimum_height"] = 0
    return {"id": number, "coordinates": list(coordinates), "blank": blank,
            "flags": dict(c=active, n=populated, o=dirty, p=True, q=True), "time": f"{time:016x}", "data": data}


def snapshot(*, dedicated=False, chunks=None, mapping=None, insertions=None, pending=None, queue=None, time=1234):
    return {"context": dict(dedicated=dedicated, world_time=f"{time:016x}", world_force=False,
                            provider_force=False if dedicated else None, spawn=[0, 0], no_sky=False),
            "world": dict(map=mapping or [], insertions=insertions or [], pending=pending or [],
                          chunks=chunks or [chunk(0, blank=True, time=0)], queue=queue or [], depth=0)}


def external(kind, coordinates, before, after, returned=None, result=None):
    return dict(kind=kind, coordinates=list(coordinates), result=result or ("loaded" if kind == "load" else "success"),
                returned_id=returned, before=before, after=after)


def summary(kind, value, identity, coordinates, *, script=None, slice=None):
    target = None if identity is None else value["world"]["chunks"][identity]
    return dict(kind=kind, effect="coordinator" if kind in ("tiles", "entities") else "observed-original-replay",
                x=coordinates[0], z=coordinates[1], id=identity, slice=slice, script=script,
                owner_matched=True, context_matched=True, map_count=len(value["world"]["map"]),
                list_count=len(value["world"]["insertions"]), pending_count=len(value["world"]["pending"]),
                active=None if target is None else target["flags"]["c"],
                dirty=None if target is None else target["flags"]["o"],
                populated=None if target is None else target["flags"]["n"],
                last_save_time=None if target is None else target["time"])


def normalized(value):
    result = copy.deepcopy(value)
    result["world"]["map"].sort()
    result["world"]["pending"].sort()
    return result


def expected_document(model, final, result, callbacks):
    return dict(schema=1, seed_hex=SEED, scope="cache-callback-boundary", initial=normalized(model["initial"]), operations=[
        dict(index=number, method=operation["method"], coordinates=operation["coordinates"], result=result[number], status="ok",
             snapshot=normalized(final), callbacks=callbacks if number == 0 else [])
        for number, operation in enumerate(model["operations"])])


def load_case(mismatched_coordinates=False):
    initial = snapshot(pending=[65536])
    before = copy.deepcopy(initial)
    before["world"]["pending"] = []
    after = copy.deepcopy(before)
    after["context"]["world_time"] = "000000000000162e"
    coordinates = [99, -7] if mismatched_coordinates else [1, 0]
    after["world"]["chunks"].append(chunk(1, coordinates))
    model = dict(seed_hex=SEED, initial=initial, operations=[dict(method=method, coordinates=[1, 0]) for method in ("c", "c", "b")],
                 scripts=[external("load", [1, 0], before, after, returned=1)])
    final = copy.deepcopy(after)
    final["world"]["map"] = [[65536, 1]]
    final["world"]["insertions"] = [1]
    final["world"]["chunks"][1]["flags"]["c"] = True
    final["world"]["chunks"][1]["time"] = "000000000000162e"
    events = [summary("load_enter", before, None, [1, 0], script=0), summary("load_return", after, 1, [1, 0], script=0),
              summary("tiles", final, 1, coordinates)]
    events += [summary("entities", final, 1, coordinates, slice=number) for number in range(8)]
    return model, expected_document(model, final, [1, 1, 1], events)


def population_case():
    initial = snapshot(chunks=[chunk(0, blank=True, time=0), chunk(1, active=True), chunk(2, [1, 0], active=True),
                               chunk(3, [0, 1], active=True)], mapping=[[0, 1], [65536, 2], [1, 3]], insertions=[1, 2, 3])
    generated = copy.deepcopy(initial)
    generated["world"]["chunks"].append(chunk(4, [1, 1], dirty=True))
    generated["world"]["queue"] = [[0, 16, 0, 16, 16, 17, 16]]
    published = copy.deepcopy(generated)
    published["world"]["chunks"][4]["flags"]["c"] = True
    published["world"]["map"].append([65537, 4])
    published["world"]["insertions"].append(4)
    before = copy.deepcopy(published)
    before["world"]["chunks"][1]["flags"]["n"] = True
    after = copy.deepcopy(before)
    data = after["world"]["chunks"][1]["data"]
    for name, octet in (("blocks", "07"), ("metadata", "9a"), ("heightmap", "13")):
        data[name]["values"] = octet + data[name]["values"][2:]
    data["minimum_height"] = 7
    after["world"]["queue"].append([1, 0, 5, 0, 0, 6, 0])
    model = dict(seed_hex=SEED, initial=initial,
        operations=[dict(method="c", coordinates=[1, 1]), dict(method="populate", coordinates=[0, 0]), dict(method="c", coordinates=[1, 1])],
        scripts=[external("load", [1, 1], initial, initial, result="miss"), external("generate", [1, 1], initial, generated, returned=4),
                 external("populate", [0, 0], before, after)])
    final = copy.deepcopy(after)
    final["world"]["chunks"][1]["flags"]["o"] = True
    events = [summary("load_enter", initial, None, [1, 1], script=0), summary("load_return", initial, None, [1, 1], script=0),
              summary("generate_enter", initial, None, [1, 1], script=1), summary("generate_return", generated, 4, [1, 1], script=1),
              summary("tiles", published, 4, [1, 1])]
    events += [summary("entities", published, 4, [1, 1], slice=number) for number in range(8)]
    events += [summary("populate_enter", before, 1, [0, 0], script=2), summary("populate_return", after, 1, [0, 0], script=2)]
    return model, expected_document(model, final, [4, None, 4], events)


def denied_fallback_case():
    initial = snapshot(dedicated=True)
    before = copy.deepcopy(initial)
    before["world"]["chunks"][0]["flags"]["n"] = True
    model = dict(seed_hex=SEED, initial=initial,
        operations=[dict(method="populate", coordinates=[9, 0]), dict(method="populate", coordinates=[9, 0]), dict(method="b", coordinates=[9, 0])],
        scripts=[external("populate", [9, 0], before, before)])
    events = [summary("populate_enter", before, 0, [9, 0], script=0), summary("populate_return", before, 0, [9, 0], script=0)]
    return model, expected_document(model, before, [None, None, 0], events)


class CallbackTransportTests(unittest.TestCase):
    def test_exact_json_types_do_not_coerce_flags_or_integer_fields(self):
        self.assertTrue(json_equal({"flags": [True, False], "count": 1}, {"count": 1, "flags": [True, False]}))
        self.assertFalse(json_equal({"flag": True}, {"flag": 1}))
        self.assertFalse(json_equal({"count": 1}, {"count": 1.0}))

    def test_encoder_round_trip_all_constructed_cases(self):
        for name, case in constructed_cases().items():
            with self.subTest(name=name):
                model, _ = case
                encoded = driver.encode_replay(model)
                self.assertEqual(decode_replay(encoded), model)
                self.assertEqual(encoded, independent_encoding(model))

    def test_encoder_refuses_omissions_invalid_metadata_and_noncanonical_values(self):
        for name in ("seed", "operation", "empty_schedule", "bool_coordinate", "dispatch", "base_force", "array", "identity", "fallback", "duplicate_key",
                     "mapped_null", "duplicate_pending", "queue_kind", "missing_context", "script", "returned", "missing_snapshot"):
            model = copy.deepcopy(load_case()[0])
            initial = model["initial"]
            if name == "seed": model["seed_hex"] = "0000000000000018"
            elif name == "operation": model["operations"][0]["method"] = "unknown"
            elif name == "empty_schedule": model["operations"], model["scripts"] = [], []
            elif name == "bool_coordinate": model["operations"][0]["coordinates"][0] = True
            elif name == "dispatch": initial["context"]["dedicated"] = 1
            elif name == "base_force": initial["context"]["provider_force"] = False
            elif name == "array": model["scripts"][0]["after"]["world"]["chunks"][1]["data"]["blocks"]["values"] = "00"
            elif name == "identity": initial["world"]["chunks"][0]["id"] = 1
            elif name == "fallback": initial["world"]["chunks"][0]["blank"] = False
            elif name == "duplicate_key": initial["world"]["map"] = [[0, 0], [0, 0]]
            elif name == "mapped_null": initial["world"]["map"] = [[0, None]]
            elif name == "duplicate_pending": initial["world"]["pending"] = [0, 0]
            elif name == "queue_kind": initial["world"]["queue"] = [[2, 0, 0, 0, 1, 1, 1]]
            elif name == "missing_context": del initial["context"]["world_time"]
            elif name == "script": model["scripts"][0]["kind"] = "other"
            elif name == "returned": model["scripts"][0]["returned_id"] = None
            else: del model["scripts"][0]["after"]
            with self.subTest(name=name), self.assertRaises((driver.Failure, KeyError)):
                driver.encode_replay(model)

    def test_transport_rejects_truncated_trailing_unknown_and_boolean_bytes(self):
        model = load_case()[0]
        raw = driver.encode_replay(model)
        for changed in malformed_inputs(raw):
            with self.subTest(size=len(changed)), self.assertRaises(ValueError):
                decode_replay(changed)

    def test_lossless_packing_and_strict_json(self):
        raw = driver.encode_replay(denied_fallback_case()[0])
        value = packed(raw)
        self.assertEqual(unpack(value), raw)
        stream = base64.b64decode(value["zlib_base64"])
        for name in ("digest", "length", "base64", "truncated", "trailing", "second", "bomb"):
            changed = dict(value)
            if name == "digest": changed["sha256"] = "0" * 64
            elif name == "length": changed["bytes"] -= 1
            elif name == "base64": changed["zlib_base64"] = "!"
            else:
                data = stream[:-1] if name == "truncated" else stream + b"x" if name == "trailing" else stream + stream
                if name == "bomb": data = zlib.compress(bytes(1048576))
                changed["zlib_base64"] = base64.b64encode(data).decode("ascii")
            with self.subTest(name=name), self.assertRaises((ValueError, zlib.error)):
                unpack(changed)
        for raw in (b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":1e999}'):
            with self.assertRaises(ValueError):
                strict_json(raw)


class CallbackFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture, cls.decoded = load_fixture()

    def test_all_four_original_inputs_and_full_projections_are_present(self):
        self.assertEqual(len(self.decoded), 4)
        self.assertEqual(sum(len(row["expected"]["operations"]) for row in self.decoded), 12)
        self.assertEqual(sum(len(row["replay"]["scripts"]) for row in self.decoded), 16)
        for row in self.decoded:
            self.assertEqual(driver.encode_replay(row["replay"]), row["input"])
            self.assertTrue(json_equal(row["replay"], decode_replay(row["input"])))

    def test_missing_contexts_preparations_controls_counts_and_weak_provenance_fail(self):
        for name in ("schema", "mode", "missing_case", "duplicate_case", "case_order", "side", "scenario", "seed", "schedule",
                     "preparation", "control", "control_hash", "counts", "source", "source_path", "source_hash", "class", "module",
                     "tool", "modules", "binding_representation"):
            value = copy.deepcopy(self.fixture)
            origin, row = value["provenance"], value["cases"][0]
            if name == "schema": value["schema"] = True
            elif name == "mode": origin["mode"] = "live-original-three-way"
            elif name == "missing_case": value["cases"].pop()
            elif name == "duplicate_case": value["cases"][3] = row
            elif name == "case_order": value["cases"].reverse()
            elif name == "side": row["side"] = "server"
            elif name == "scenario": row["scenario"] = "population"
            elif name == "seed": row["seed_hex"] = "0000000000000018"
            elif name == "schedule": row["schedule"][0][1] = 2
            elif name == "preparation": del value["preparations"]["server"]
            elif name == "control": row["control_equal"] = False
            elif name == "control_hash": row["control_record"]["sha256"] = "0" * 64
            elif name == "counts": row["callbacks"] -= 1
            elif name == "source": del origin["source_fingerprints"]["src/world/cache/acquire.c"]
            elif name == "source_path": origin["source_fingerprints"]["../private.c"] = origin["source_fingerprints"].pop("src/world/cache/acquire.c")
            elif name == "source_hash": origin["source_fingerprints"]["src/world/cache/acquire.c"]["sha256"] = "invalid"
            elif name == "class": del origin["helper_classes"]["CacheCallbacksController.class"]
            elif name == "module": origin["executed_source_modules"]["scripts/reference/cache/callback_protocol.py"]["sha256"] = "0" * 64
            elif name == "tool": origin["tool_versions"]["java"]["major"] = 16
            elif name == "modules": origin["jdk_modules"]["required"] = ["jdk.jdi"]
            else: row["protocol"]["binding_representation"] = "raw"
            with self.subTest(guard=name), self.assertRaises((ValueError, KeyError, driver.Failure)):
                validate_metadata(value)

    def test_preserved_world_inventories_and_baseline_payload_fingerprints_are_required(self):
        for name in ("incomplete", "path", "counts", "baseline", "region", "extra", "session"):
            value = copy.deepcopy(self.fixture)
            row = value["cases"][0]
            if name == "incomplete": row["world_after"]["complete"] = False
            elif name == "path": row["world_after"]["entries"][0]["path"] = "../outside"
            elif name == "counts": row["world_after"]["bytes"] += 1
            elif name == "baseline": value["preparations"]["client"]["baseline"]["bytes"] += 1
            elif name == "region": next(item for item in row["world_after"]["entries"] if item["path"].endswith(".mcr"))["sha256"] = "0" * 64
            elif name == "extra": row["world_after"]["entries"].append({"path": "world/extra", "kind": "directory"})
            else: next(item for item in row["world_after"]["entries"] if item["path"].endswith("session.lock"))["bytes"] = 7
            with self.subTest(guard=name), self.assertRaises((ValueError, KeyError, driver.Failure)):
                validate_metadata(value)

    def test_public_protocol_rejects_original_bytes_private_paths_and_changed_child_commands(self):
        for name in ("bytecode", "origin", "child", "hash"):
            row = copy.deepcopy(self.fixture["cases"][0])
            if name == "origin":
                snapshot = strict_json(unpack(row["protocol"]["snapshot"], MAX_ORIGINAL))
                snapshot["class_origins"]["fd"] = "file:/private.jar"
                row["protocol"]["snapshot"] = packed(json.dumps(snapshot).encode("ascii"))
            else:
                trace = strict_json(unpack(row["protocol"]["trace"], MAX_ORIGINAL))
                if name == "bytecode": trace["bindings"]["acquire"]["bytecode_hex"] = "00"
                elif name == "child": trace["child_command"][11] = "foreign:world"
                else: trace["bindings"]["acquire"]["bytecode_sha256"] = "invalid"
                row["protocol"]["trace"] = packed(json.dumps(trace).encode("ascii"))
            with self.subTest(guard=name), self.assertRaises(ValueError):
                public_documents(row)

    def test_fixture_size_and_lossless_input_projection_fingerprints_are_enforced(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.json"
            with path.open("wb") as output:
                output.truncate(MAX_FIXTURE + 1)
            with self.assertRaises(ValueError):
                load_fixture(path)
        for name in ("input", "expected"):
            value = copy.deepcopy(self.fixture)
            value["cases"][0][name]["sha256"] = "0" * 64
            with self.subTest(record=name), self.assertRaises(ValueError):
                validate_metadata(value)


def constructed_cases():
    return {"loaded_live_time": load_case(), "requested_key_differs_from_chunk": load_case(True),
            "population_body_delta": population_case(), "dedicated_denied_fallback_population": denied_fallback_case()}


def malformed_inputs(raw):
    # Header, body truncation, and suffix corruption are rejected before any call.
    empty = decode_replay(raw)
    empty["operations"], empty["scripts"] = [], []
    # Retain a complete initial snapshot with no trailing operation or script bodies.
    empty_schedule = independent_encoding(empty)
    return [b"BAD1" + raw[4:], raw[:-1], raw + b"x", raw[:12] + b"\x02" + raw[13:],
            raw[:13] + struct.pack(">I", 17) + raw[17:], raw[:17] + struct.pack(">I", 33) + raw[21:], empty_schedule]


def run_probe(executable, raw=None, arguments=()):
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        process = subprocess.run([str(executable), *arguments], input=raw, stdout=stdout, stderr=stderr, timeout=30, check=False)
        require(stdout.tell() <= MAX_BYTES and stderr.tell() <= 32768, "bounded callback probe streams")
        stdout.seek(0)
        stderr.seek(0)
        return process.returncode, stdout.read(), stderr.read()


def verify_constructed(executable):
    for name, (model, expected) in constructed_cases().items():
        raw = driver.encode_replay(model)
        require(decode_replay(raw) == model, "independent complete MCC1 transport: " + name)
        status, output, errors = run_probe(executable, raw)
        require(status == 0 and not errors and json_equal(strict_json(output), expected), "constructed complete C contract: " + name)
    raw = driver.encode_replay(load_case()[0])
    for changed in malformed_inputs(raw):
        status, output, _ = run_probe(executable, changed)
        require(status == 2 and not output, "malformed MCC1 must fail2 without stdout")
    for name in ("flags", "payload", "map", "list", "pending", "queue", "time", "n_before", "order", "missing", "surplus"):
        model = copy.deepcopy(population_case()[0])
        if name in ("flags", "payload", "map", "list", "pending", "queue", "time"):
            before = copy.deepcopy(model["scripts"][0]["before"])
            model["scripts"][0]["before"] = before
            if name == "flags": before["world"]["chunks"][1]["flags"]["o"] = True
            elif name == "payload": before["world"]["chunks"][1]["data"]["blocks"]["values"] = "07" + "03" * 32767
            elif name == "map": before["world"]["map"][0][0] = 2
            elif name == "list": before["world"]["insertions"].reverse()
            elif name == "pending": before["world"]["pending"] = [123]
            elif name == "queue": before["world"]["queue"] = [[1, 0, 0, 0, 1, 1, 1]]
            else: before["context"]["world_time"] = "00000000000004d3"
        elif name == "n_before": model["scripts"][2]["before"]["world"]["chunks"][1]["flags"]["n"] = False
        elif name == "order": model["scripts"][0], model["scripts"][1] = model["scripts"][1], model["scripts"][0]
        elif name == "missing": model["scripts"].pop()
        else: model["scripts"].append(copy.deepcopy(model["scripts"][-1]))
        status, _, _ = run_probe(executable, driver.encode_replay(model))
        require(status == 1, "callback entry/effect mismatch must fail1: " + name)
    status, output, errors = run_probe(executable, arguments=("--audit-owners",))
    expected_audit = {"schema": 1, "scope": "constructed-owner-guard", "checks": 10,
                      "all_valid_descriptors_rejected": True}
    require(status == 0 and not errors and json_equal(strict_json(output), expected_audit),
            "valid alternative cache/pending/chunk descriptors must fail ownership checks")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, default=FIXTURE)
    args = parser.parse_args(argv)
    _, cases = load_fixture(args.fixtures)
    executable = args.executable.resolve(strict=True)
    for row in cases:
        status, output, errors = run_probe(executable, row["input"])
        require(status == 0 and not errors and json_equal(strict_json(output), row["expected"]),
                "complete original/C callback boundary mismatch: case " + str(row["record"]["index"]))
    verify_constructed(executable)
    print("callback original fixtures: 4 continuous inputs, 12 operations, 16 explicit external callbacks; constructed: 4 cases and 28 guards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
