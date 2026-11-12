"""Lossless numerical observations from one independently audited original run.

The package retains the original JSON, passive JDI trace and constructor-input
tape byte for byte. Its audit receipt is inherited attribution, not a substitute
for repeating the classfile/source audit with the supplied original archive.
No native observation supplies an expected value or enters this package.
"""
from __future__ import annotations

import base64
import hashlib
import importlib.util
import json
from pathlib import Path, PureWindowsPath
import re
import sys
import zlib

ROOT = Path(__file__).resolve().parents[5]
DEFAULT = ROOT / "tests/reference/world/population/dedicated/fixtures.json"
MAX_PACKAGE = 8 * 1024 * 1024
MAX_COMPRESSED = 6 * 1024 * 1024
MAX_RATIO = 512
MAX_DECODED = 32 * 1024 * 1024
SCOPE = "original-only-dedicated-natural-unload-fixture"
PROFILE = "dedicated-natural-unload"
JAR = {"bytes": 503100, "sha256": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
PAYLOADS = {
    "original": {"bytes": 25289891, "sha256": "4ef184f2cf8c7589d24b1fd7b95c08aea066c9ccba493e152e97af4188f47721"},
    "trace": {"bytes": 213017, "sha256": "f276d3d83435992aaa34b46bd37af64efd8abcec138315f535359037bf7d3a2c"},
    "constructor_inputs": {"bytes": 338, "sha256": "865547539c74d4c4ee7d1bf2a4750f1e930e77654f5b95a0c7b0dfebdf202233"},
}
OBSERVERS = {
    "ChunkObservation.java": (30950, "213c7ffb98eb8aaf2aeca9e38d65ba528ab5f4f42e0fab4c07b341ffd93b899e"),
    "CacheCallbacksObservation.java": (22611, "5aac2c1a0bf28e8d2f45d2b504eba66f27faa4990b537210ef066736eab7482f"),
    "ChunkLifecycleObservation.java": (57791, "5be3fe26d8bd7e9ce47fbe8f1adb9718bbe204b7279a3f7ab023856088a4644e"),
    "NaturalPopulationObservation.java": (26130, "f6bc4f17eb2132c2c22b961355b593e530f083de0b198b8936a24371a358f1ba"),
    "PopulationDedicatedObservation.java": (49038, "69008516e301e7c9a96248140df40575bb57ca842b54894d318fbe300ce1482c"),
    "PopulationDedicatedRandomObserver.java": (58883, "865608dc532533a3b03aaf6e93e1cfeca544f8f28c02942667e236b911a04699"),
}
ROOT_SEMANTICS = "Original project-owned fresh test roots and class-origin URLs are retained verbatim; native roots are independently chosen absolute paths. No captured path is opened during replay."
CLOCK_SEMANTICS = "Original epoch intervals and native clock inputs are retained verbatim. Each side validates its own measured intervals; replay never replaces a native clock with an original epoch."
AUDIT_SCOPE = "Inherited successful original-only classfile/source/PC and Random audit. Public loading rechecks graph, trace, tape and file invariants without repeating that archive-dependent qualification."
WRAPPER = ("schema", "scope", "profile", "side", "seed_hex", "experiment", "qualified", "native_comparison",
    "tested_commit", "custom_observer_uncommitted", "passed", "jar_sha256", "metadata_preparation", "world_is_new",
    "construction_epoch_ms", "threads_before", "initial", "final", "threads_after_operations", "class_origins",
    "bindings", "field_bindings", "construction_trace", "source_helpers", "limits", "projection_scope",
    "snapshot_compatibility", "storage_preparation", "failure", "region_close_completed", "disk_after_close",
    "threads_after_close", "operations", "states", "state_encoding", "blobs", "blob_encoding", "metadata_preparation_calls")


def comparison():
    name = "_dedicated_fixture_comparison"
    if name not in sys.modules:
        spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name("comparison.py"))
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
    return sys.modules[name]


def need(value, message):
    if not value:
        raise ValueError(message)


def fingerprint(raw):
    need(type(raw) is bytes, "complete byte input")
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def _keys(value, names, label):
    need(type(value) is dict and set(value) == set(names), label + ": exact keys")


def packed(raw):
    need(type(raw) is bytes and 0 < len(raw) <= MAX_DECODED, "bounded complete payload")
    compressed = zlib.compress(raw, 9)
    need(0 < len(compressed) <= MAX_COMPRESSED and len(raw) <= MAX_RATIO * len(compressed), "bounded compression ratio")
    return {"codec": "zlib-base64-v1", **fingerprint(raw), "compressed_bytes": len(compressed),
            "compressed_sha256": hashlib.sha256(compressed).hexdigest(),
            "base64": base64.b64encode(compressed).decode("ascii")}


def unpack(value, expected):
    _keys(value, ("codec", "bytes", "sha256", "compressed_bytes", "compressed_sha256", "base64"), "lossless payload")
    need(value["codec"] == "zlib-base64-v1" and
         {"bytes": value["bytes"], "sha256": value["sha256"]} == expected, "fixed original payload attribution")
    size = value["compressed_bytes"]
    need(type(size) is int and 0 < size <= MAX_COMPRESSED and
         type(value["base64"]) is str and len(value["base64"]) == 4 * ((size + 2) // 3), "bounded complete base64")
    need(0 < expected["bytes"] <= MAX_DECODED and expected["bytes"] <= MAX_RATIO * size, "fixed decode/ratio bound")
    try:
        compressed = base64.b64decode(value["base64"], validate=True)
        need(len(compressed) == size and base64.b64encode(compressed).decode("ascii") == value["base64"], "canonical compressed byte count")
        need(type(value["compressed_sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", value["compressed_sha256"]) is not None and
             hashlib.sha256(compressed).hexdigest() == value["compressed_sha256"], "compressed payload digest")
        decoder = zlib.decompressobj()
        raw = decoder.decompress(compressed, expected["bytes"] + 1)
        need(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail, "one complete compressed stream")
    except (ValueError, zlib.error) as error:
        raise ValueError("invalid bounded payload: " + str(error)) from error
    need(fingerprint(raw) == expected, "complete original payload bytes/SHA256")
    return raw


class _OriginalOwner:
    def __init__(self, C, original):
        self.C, self.ledger, self.immutable = C, C.Ledger(), {}
        self.blobs, self.used_blobs = {}, set()
        need(type(original["blobs"]) is dict and 0 < len(original["blobs"]) <= 4096, "bounded source blob dictionary")
        for digest, value in original["blobs"].items():
            C.raw_hex(digest, 32, "blob digest")
            C.keys(value, ("bytes", "hex"), "complete lossless source blob")
            raw = C.raw_hex(value["hex"], C.integer(value["bytes"], 0, 8 * 1024 * 1024))
            need(hashlib.sha256(raw).hexdigest() == digest, "independent source blob hash")
            self.blobs[digest] = raw
        need(sum(map(len, self.blobs.values())) <= MAX_DECODED, "complete decoded source blob budget")

    def blob(self, ref):
        self.C.keys(ref, ("bytes", "sha256"), "source blob reference")
        need(ref["sha256"] in self.blobs, "every source blob exists")
        raw = self.blobs[ref["sha256"]]
        need(type(ref["bytes"]) is int and ref["bytes"] == len(raw), "complete referenced source bytes")
        self.used_blobs.add(ref["sha256"])
        return raw


def _state_index(value, count):
    _keys(value, ("state_reference",), "interned state reference")
    index = value["state_reference"]
    need(type(index) is int and 0 <= index < count, "existing interned source state")
    return index


def validate_original(original, trace, tape):
    """Recheck offline invariants; inherit the separate source/classfile audit."""
    C = comparison()
    C.keys(original, WRAPPER, "complete original wrapper")
    C.equal(tuple(original[key] for key in ("schema", "scope", "profile", "side", "seed_hex")),
            (1, "original-only-dedicated-natural-population-unload-prototype", PROFILE, "server", C.SEED), "fixed legacy original observation")
    C.integer(original["schema"], 1, 1, "typed original schema")
    for key in ("experiment", "qualified", "native_comparison", "custom_observer_uncommitted",
                "passed", "region_close_completed", "world_is_new"):
        C.boolean(original[key])
    C.equal(tuple(original[key] for key in ("experiment", "qualified", "native_comparison", "tested_commit",
            "custom_observer_uncommitted", "passed", "failure", "region_close_completed", "world_is_new")),
            (True, False, False, None, True, True, None, True, False), "honest original experimental attribution")
    C.equal(original["jar_sha256"], JAR["sha256"], "fixed supplied original archive")
    construction = C.epoch(original["construction_epoch_ms"])
    jdi = C.parse_trace(trace, tape, legacy=True)
    C.parse_tape(tape)
    owner = _OriginalOwner(C, original)
    states = original["states"]
    need(type(states) is list and 0 < len(states) <= 64, "complete bounded state dictionary")
    graphs = [C.Graph(owner, state) for state in states]
    operations = original["operations"]
    need(type(operations) is list and len(operations) == 17, "complete operation schedule")
    previous = _state_index(original["initial"], len(states))
    boundaries = [(previous, "operation_0_before")]
    for index, operation in enumerate(operations):
        C.keys(operation, ("index", "action", "receiver", "class", "method", "descriptor", "arguments", "before", "epoch_ms", "result", "failure", "after"), "complete original operation")
        C.equal((operation["index"], operation["action"], operation["failure"]), (index, C.ACTIONS[index], None), "original schedule and completion")
        C.epoch(operation["epoch_ms"])
        C.equal(_state_index(operation["before"], len(states)), previous, "continuous operation-before state")
        if C.COORDS[index] is not None:
            C.equal(operation["arguments"], list(C.COORDS[index]), "actual acquisition/drop coordinates")
        previous = _state_index(operation["after"], len(states))
        boundaries.append((previous, "operation_" + str(index) + "_after"))
    C.equal(_state_index(original["final"], len(states)), previous, "actual final source state")
    C.equal({index for index, _ in boundaries}, set(range(len(states))), "every source state is reached")
    root = None
    object_observations, queue_observations, nbt_files = 0, 0, 0
    for ordinal, (index, marker) in enumerate(boundaries):
        state, g = states[index], graphs[index]
        world = state["graph"]["world"]
        C.integer(C.signed(g.field(world, "dj", "I")), *construction, "World clock in its own constructor interval")
        cache = g.field(world, "dp", "C")
        C.equal(g.field(world, "dj", "v"), cache, "same actual base/dedicated cache")
        C.equal(g.field(cache, "he", "h"), world, "actual dedicated cache World")
        C.equal(g.field(world, "dp", "E"), False, "actual dedicated saving-disable field")
        g.ref(g.field(world, "dp", "F"), C.SERVER)
        index_owner = g.field(world, "dp", "G")
        g.ref(index_owner, "go")
        C.equal(g.field(index_owner, "go", "b"), 0, "finite actual empty Entity index")
        dedicated = state["dedicated"]
        C.keys(dedicated, ("world", "base_cache", "fields"), "actual dedicated projection")
        C.equal((dedicated["world"], dedicated["base_cache"]), (world, cache), "same actual dedicated roles")
        C.equal({row["field"]: (row["descriptor"], row["modifiers"], row["value"]) for row in dedicated["fields"]},
                {name: (desc, modifiers, g.field(world, "dp", name)) for name, (desc, modifiers) in C.FIELDS["dp"].items()}, "complete dedicated projection agrees with graph")
        random = g.obj(g.field(world, "dj", "r"), "java.util.Random")
        C.equal(C.random_value(random["state"]), C.random_value(jdi["states"][(marker, "World_r")]["value"]), "used actual World Random matches passive JDI boundary")
        seed = g.obj(random["seed_owner"], "java.util.concurrent.atomic.AtomicLong")["raw_long"]
        C.equal(C.word(seed), int(random["state"]["seed48"], 16), "actual Random seed cell")
        ordered = g.collection(g.field(world, "dj", "E"), "java.util.TreeSet")
        membership = g.collection(g.field(world, "dj", "F"), "java.util.HashSet")
        C.equal({row["ref"] for row in ordered}, {row["ref"] for row in membership}, "scheduled ordered/membership owners agree")
        C.equal(len({row["ref"] for row in ordered}), len(ordered), "scheduled occurrences are unique")
        ordering = [(C.signed(g.field(row, "ki", "e")), C.signed(g.field(row, "ki", "g"))) for row in ordered]
        C.equal(ordering, sorted(ordering), "actual scheduled time/sequence order")
        queue_observations += len(ordered)
        store = g.field(world, "dj", "w")
        paths = {}
        for field, role in (("b", ""), ("c", "players"), ("d", "data")):
            file = g.obj(g.field(store, "dq", field), "java.io.File")
            value = C.utf16(g.text(file["path"]))
            C.equal(file["absolute"], True, "actual absolute source File")
            need(PureWindowsPath(value).is_absolute(), "captured original Windows root")
            paths[role] = PureWindowsPath(value)
        need(paths[""].name == "world" and paths[""].parent.name == "original-parent", "project-owned original test root role")
        C.equal(paths["players"], paths[""] / "players", "actual Player directory binding")
        C.equal(paths["data"], paths[""] / "data", "actual SavedData directory binding")
        if root is None:
            root = str(paths[""])
        C.equal(str(paths[""]), root, "retained source World root")
        files, _ = C.Comparison.disk(owner, state["disk"], True)
        need(C.SESSION in files and len(files[C.SESSION]) == 8, "complete actual session lock")
        C.equal(files[C.SESSION].hex(), g.field(store, "dq", "e"), "actual source Store token matches session file")
        for name, raw in files.items():
            if name in (C.LEVEL, C.OLD):
                tree = C.parse_nbt(C.decompress(raw, True, 65536))
                data = C.compound(tree, "", {"Data"})["Data"]
                tags = C.compound(data, "Data", {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed", "LevelName", "version", "raining", "rainTime", "thundering", "thunderTime"})
                C.equal((tags["RandomSeed"]["type"], tags["RandomSeed"]["value"]), (4, 23), "actual typed metadata seed")
                C.equal((tags["version"]["type"], tags["version"]["value"]), (3, 19132), "actual typed metadata version")
                nbt_files += 1
        object_observations += len(g.objects)
    C.Comparison.disk(owner, original["disk_after_close"], True)
    # Preparation also retains file observations before the live World exists.
    # Validate those complete references without manufacturing a native peer.
    pending = [original]
    while pending:
        value = pending.pop()
        if type(value) is dict:
            if set(value) == {"bytes", "sha256"}:
                owner.blob(value)
            pending.extend(value.values())
        elif type(value) is list:
            pending.extend(value)
    C.equal(owner.used_blobs, set(owner.blobs), "every lossless source blob is reached")
    return {"scope": "offline original graph/trace/tape/file invariants; inherited classfile/source audit",
            "boundaries": len(boundaries), "interned_states": len(states), "source_blobs": len(owner.blobs),
            "object_observations": object_observations, "scheduled_occurrences": queue_observations,
            "metadata_file_observations": nbt_files, "original_world_root": root,
            "source_classfile_audit_repeated": False}


def _provenance(value):
    _keys(value, ("source_head", "source_tree", "source_dirty", "tested_commit", "custom_observer_uncommitted",
                  "server_jar", "observer_sources", "inherited_audit", "root_semantics", "clock_semantics"), "complete original provenance")
    need(value["source_head"] == "8cb4fe416626c0f0e3fbcd85b712a0029cab89b9" and
         value["source_tree"] == "755200e5c2048fbd6ce2778f409d88a6bd06ad33" and value["source_dirty"] is True and
         value["tested_commit"] is None and value["custom_observer_uncommitted"] is True, "preserved dirty original attribution")
    need(value["server_jar"] == JAR and value["root_semantics"] == ROOT_SEMANTICS and
         value["clock_semantics"] == CLOCK_SEMANTICS, "original authority/root/clock semantics")
    need(type(value["observer_sources"]) is dict and len(value["observer_sources"]) == 6, "six original observer source pins")
    need(value["observer_sources"] == {name: {"bytes": size, "sha256": digest}
         for name, (size, digest) in OBSERVERS.items()}, "frozen original observer source fingerprints")
    audit = value["inherited_audit"]
    _keys(audit, ("passed", "semantic_audit_complete", "native_comparison", "qualification", "tested_commit",
                  "scope", "artifact", "auditor", "capture_receipt", "trace", "original", "constructor_inputs"), "inherited independent original audit")
    need((audit["passed"], audit["semantic_audit_complete"], audit["native_comparison"], audit["qualification"], audit["tested_commit"]) ==
         (True, True, False, False, None) and audit["scope"] == AUDIT_SCOPE, "honest inherited audit scope")
    need(all(type(audit[key]) is bool for key in ("passed", "semantic_audit_complete", "native_comparison", "qualification")), "typed inherited audit flags")
    for role in PAYLOADS:
        need(audit[role] == PAYLOADS[role], "audit binds original-only payload: " + role)
    for record in [*value["observer_sources"].values(), audit["artifact"], audit["auditor"], audit["capture_receipt"]]:
        _keys(record, ("bytes", "sha256"), "complete source/audit fingerprint")
        need(type(record["bytes"]) is int and 0 < record["bytes"] <= 2 * 1024 * 1024 and
             type(record["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is not None, "bounded source/audit pin")
    need(audit["artifact"] == {"bytes": 906030, "sha256": "5ec5219d9f80f199edbecf878908726021459237450c28c340b41aa2cd45e540"} and
         audit["capture_receipt"] == {"bytes": 77438, "sha256": "88919bd9e511e8821908389176fd6ea8160b4e2252f66a4aec893edfa4b755cb"} and
         audit["auditor"] == {"bytes": 100442, "sha256": "2a08d7da405c19f8d29edcba83f7ea0e74d556a7639cba8de7cc286422c0ffe6"}, "frozen original audit provenance")


def serialize_package(package):
    raw = (json.dumps(package, ensure_ascii=True, allow_nan=False, separators=(",", ":")) + "\n").encode("ascii")
    need(0 < len(raw) <= MAX_PACKAGE, "bounded complete public package")
    return raw


def export_package(original, trace, constructor_inputs, provenance):
    """Package exact original-only bytes; this function accepts no native input."""
    raws = {"original": original, "trace": trace, "constructor_inputs": constructor_inputs}
    need({name: fingerprint(raw) for name, raw in raws.items()} == PAYLOADS, "audited original-only payload pins")
    _provenance(provenance)
    validate_original(comparison().strict_json(original), trace, constructor_inputs)
    result = {"schema": 1, "scope": SCOPE, "profile": PROFILE, "side": "server", "seed_hex": "0000000000000017",
              "provenance": provenance, "payloads": {name: packed(raw) for name, raw in raws.items()}}
    serialize_package(result)
    return result


def decode_package(package):
    _keys(package, ("schema", "scope", "profile", "side", "seed_hex", "provenance", "payloads"), "complete public dedicated fixture")
    need(type(package["schema"]) is int and package["schema"] == 1 and package["scope"] == SCOPE and
         package["profile"] == PROFILE and package["side"] == "server" and package["seed_hex"] == "0000000000000017", "fixed public fixture profile")
    _provenance(package["provenance"])
    _keys(package["payloads"], PAYLOADS, "original-only payload roles")
    raws = {name: unpack(package["payloads"][name], expected) for name, expected in PAYLOADS.items()}
    need(sum(map(len, raws.values())) <= MAX_DECODED, "complete decoded package budget")
    original = comparison().strict_json(raws["original"])
    validation = validate_original(original, raws["trace"], raws["constructor_inputs"])
    return {**raws, "original_value": original, "validation": validation, "provenance": package["provenance"]}


def load_package(path=DEFAULT):
    path = Path(path)
    need(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= MAX_PACKAGE, "bounded ordinary dedicated fixture")
    with path.open("rb") as incoming:
        raw = incoming.read(MAX_PACKAGE + 1)
    need(0 < len(raw) <= MAX_PACKAGE, "complete bounded package bytes")
    return decode_package(comparison().strict_json(raw))
