#!/usr/bin/env python3
"""Compare retained finite dedicated GAME graphs without launching producers.

compare() consumes complete original/native observations, a passive JDI trace
and constructor-input tape. Callers must separately validate original/JDI
semantics and capture provenance. The original graph remains the oracle;
tape rows supply constructor inputs only. Explicit representation rules retain
game fields, arrays, aliases and disk effects without claiming JVM heap parity
or complete game/server qualification. Current observations use schema 2;
schema-1 original fixtures require the explicit legacy_original adapter flag.
Native schema 1 is never admitted by this comparator.
"""
from __future__ import annotations

import argparse
from collections import Counter
import copy
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[5]
MAX = 64 * 1024 * 1024
MASK = (1 << 64) - 1
I32 = (-(1 << 31), (1 << 31) - 1)
SERVER = "net.minecraft.server.MinecraftServer"
SEED = "0000000000000017"
PROFILE = "dedicated-natural-unload"
ACTIONS = ("origin_c",) * 5 + ("world_time_1234", "full_world_save_0", "full_world_save_1",
    "protected_drop_0_0") + ("far_c",) * 4 + ("drop_16_0", "cache_maintenance",
    "reload_c_16_0", "repeated_c_16_0")
COORDS = ((0, 0), (1, 0), (0, 1), (1, 1), (1, 1), None, None, None, (0, 0),
    (16, 0), (17, 0), (16, 1), (17, 1), (16, 0), None, (16, 0), (16, 0))
INFO = ("seed", "spawn_x", "spawn_y", "spawn_z", "time", "last_played", "size_on_disk",
    "player", "player_dimension", "level_name", "version", "raining", "rain_time",
    "thundering", "thunder_time")
INFO_DESCS = ("J", "I", "I", "I", "J", "J", "J", "Liq;", "I", "Ljava/lang/String;",
    "I", "Z", "I", "Z", "I")
ARRAYS = {"blocks": 32768, "metadata": 16384, "skylight": 16384,
    "block_light": 16384, "heightmap": 256}
REGION, LEVEL, OLD, SESSION = ("world/region/r.0.0.mcr", "world/level.dat",
    "world/level.dat_old", "world/session.lock")


def need(value, message):
    if not value:
        raise ValueError(message)


def equal(a, b, message):
    if a != b:
        raise ValueError(message + ": " + repr(a)[:400] + " != " + repr(b)[:400])


def keys(value, expected, message):
    need(type(value) is dict and set(value) == set(expected), message + ": exact keys")


def integer(value, low=I32[0], high=I32[1], label="integer"):
    need(type(value) is int and low <= value <= high, label + ": typed range")
    return value


def boolean(value, label="Boolean"):
    need(type(value) is bool, label + ": actual Boolean")
    return value


def raw_hex(value, length, label="raw bytes"):
    need(type(value) is str and len(value) == 2 * length and re.fullmatch(r"[0-9a-f]*", value) is not None,
         label + ": complete canonical hex")
    return bytes.fromhex(value)


def word(value):
    return int.from_bytes(raw_hex(value, 8, "long bits"), "big")


def signed(value):
    n = word(value)
    return n - (1 << 64) if n >= 1 << 63 else n


def utf16(value):
    need(type(value) is str and len(value) % 4 == 0 and len(value) <= 262144, "complete UTF16 bound")
    return raw_hex(value, len(value) // 2, "UTF16").decode("utf-16-be", "surrogatepass")


def captured_path(value):
    """Read a captured path using its syntax, independently of this host."""
    need(type(value) is str, "captured path text")
    windows = PureWindowsPath(value)
    return windows if windows.is_absolute() else PurePosixPath(value)


def epoch(value):
    need(type(value) is list and len(value) == 2, "two measured epoch endpoints")
    lo, hi = (integer(x, 0, (1 << 63) - 1, "epoch") for x in value)
    need(lo <= hi, "ordered epoch")
    return lo, hi


def fingerprint(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def bounded_read(path, limit=MAX):
    path = Path(path)
    need(path.is_file() and not path.is_symlink() and path.stat().st_size <= limit, "bounded ordinary input")
    raw = path.read_bytes()
    need(len(raw) <= limit, "complete bounded input")
    return raw


def strict_json(raw):
    def pairs(rows):
        result = {}
        for name, value in rows:
            need(name not in result, "duplicate JSON key")
            result[name] = value
        return result
    def noninteger(value):
        raise ValueError("unexpected JSON floating/nonfinite number: " + value)
    return json.loads(raw, object_pairs_hook=pairs, parse_float=noninteger, parse_constant=noninteger)


def public_support():
    name = "_private_dedicated_public_persistence"
    if name not in sys.modules:
        path = ROOT / "scripts/reference/world/population/compare_persistence.py"
        spec = importlib.util.spec_from_file_location(name, path)
        need(spec is not None and spec.loader is not None, "public parsing support")
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
    return sys.modules[name]


def random_value(value, native=False):
    return public_support().copied_random(value, original=not native)


# Exact observed declaring-class fields, checked against the separately pinned
# original class-file audit. Case is significant, including all World shadows.
DECLARATIONS = {
    "dp": "C:Lhe;:1 D:Z:1 E:Z:1 F:Lnet/minecraft/server/MinecraftServer;:2 G:Lgo;:2",
    "dj": "a:Z:1 b:Ljava/util/List;:1 B:Z:1 c:Ljava/util/List;:1 C:Ljava/util/List;:2 d:Ljava/util/List;:1 D:Ljava/util/List;:2 e:Ljava/util/List;:1 E:Ljava/util/TreeSet;:2 f:I:1 F:Ljava/util/Set;:2 g:I:4 G:Ljava/util/List;:2 h:I:20 H:J:2 i:F:4 I:J:2 j:F:4 J:Z:2 k:F:4 K:Ljava/util/ArrayList;:2 l:F:4 L:Z:2 m:I:4 M:I:2 n:I:1 N:Z:2 o:Z:1 O:Z:2 p:I:4 P:Ljava/util/Set;:2 q:I:1 Q:I:2 r:Ljava/util/Random;:1 R:Ljava/util/List;:2 s:Z:1 t:Los;:17 u:Ljava/util/List;:4 v:Lbl;:4 w:Lom;:20 x:Lct;:4 y:Z:1 z:Les;:1",
    "hi": "b:[B:1 c:Z:1 d:Ldj;:1 e:Lob;:1 f:Lob;:1 g:Lob;:1 h:[B:1 i:I:1 j:I:17 k:I:17 l:Ljava/util/Map;:1 m:[Ljava/util/List;:1 n:Z:1 o:Z:1 p:Z:1 q:Z:1 r:J:1",
    "he": "a:Z:1 b:Ljava/util/Set;:2 c:Lhi;:2 d:Lbl;:2 e:Lan;:2 f:Ljava/util/Map;:2 g:Ljava/util/List;:2 h:Ldp;:2",
    SERVER: "c:Lcu;:1 d:Lky;:1 e:[Ldp;:1 f:Lfc;:1 g:Z:1 h:I:0 i:Ljava/lang/String;:1 j:I:1 k:[Lnl;:1 l:Z:1 m:Z:1 n:Z:1 o:Z:1 p:Lmc;:2 q:Z:2 r:Ljava/util/List;:2 s:Ljava/util/List;:2",
    "go": "a:[Log;:130 b:I:130 c:I:2 d:F:18 e:I:194",
    "os": "a:Ldj;:1 b:Lph;:1 c:Z:1 d:Z:1 e:Z:1 f:[F:1 g:I:1 h:[F:2",
    "dq": "b:Ljava/io/File;:18 c:Ljava/io/File;:18 d:Ljava/io/File;:18 e:J:18",
    "ct": "a:J:2 b:I:2 c:I:2 d:I:2 e:J:2 f:J:2 g:J:2 h:Liq;:2 i:I:2 j:Ljava/lang/String;:2 k:I:2 l:Z:2 m:I:2 n:Z:2 o:I:2",
    "es": "a:Lom;:2 b:Ljava/util/Map;:2 c:Ljava/util/List;:2 d:Ljava/util/Map;:2",
    "ob": "a:[B:17",
    "pk": "a:Lmp;:1 b:Lmp;:1 c:Lmp;:1 d:[D:0 e:[D:0 f:[D:0 g:[D:0 h:[D:0 i:[[I:0 j:Ljava/util/Random;:2 k:Lmp;:2 l:Lmp;:2 m:Lmp;:2 n:Lmp;:2 o:Lmp;:2 p:Ldj;:2 q:[D:2 r:[D:2 s:[D:2 t:[D:2 u:Ldy;:2 v:[Lgs;:2 w:[D:2",
    "hb": "a:Ljava/io/File;:18",
    "ph": "a:[D:1 b:[D:1 c:[D:1 d:[Lgs;:1 e:Lmq;:2 f:Lmq;:2 g:Lmq;:2",
    "mp": "a:[Lab;:2 b:I:2", "dy": "a:I:4 b:Ljava/util/Random;:4",
    "mq": "a:[Lbi;:2 b:I:2", "ab": "a:D:1 b:D:1 c:D:1 d:[I:2",
    "bi": "a:D:1 b:D:1 c:D:1 e:[I:2",
    "gs": "n:Ljava/lang/String;:1 o:I:1 p:B:1 q:B:1 r:I:1 s:Ljava/util/List;:4 t:Ljava/util/List;:4 u:Ljava/util/List;:4 v:Z:2 w:Z:2",
    "aq": "a:Ljava/lang/Class;:1 b:I:1", "lv": "a:Lco;:17 b:I:1 c:I:1 d:I:1 e:I:1 f:I:1 g:I:1",
    "co": "c:I:17", "ki": "a:I:1 b:I:1 c:I:1 d:I:1 e:J:1 g:J:2",
}
LINEAGES = {name: (name,) for name in DECLARATIONS}
LINEAGES.update({"dp": ("dp", "dj"), "hg": ("hi",), "ie": ("dq",), "kp": ("os",), "lt": ("dy",)})
LINEAGES.update({name: ("gs",) for name in ("dw", "f", "kk", "le", "m", "ma", "pp")})
FIELDS = {owner: {name: (desc, int(mods)) for name, desc, mods in (token.split(":") for token in raw.split())}
    for owner, raw in DECLARATIONS.items()}
COLLECTIONS = {"java.util.ArrayList", "java.util.HashSet", "java.util.TreeSet",
    "java.util.Collections$SynchronizedRandomAccessList"}
KINDS = {"mc_world_runtime", "mc_server_runtime", "mc_world_provider", "mc_normal_generator",
    "mc_chunk_cache", "mc_chunk_pending", "mc_world_reference_list", "mc_world_active_chunks",
    "mc_scheduled_ordered", "mc_scheduled_membership", "mc_scheduled_tick", "mc_world_dimension",
    "mc_biome_manager", "mc_biome", "mc_biome_spawn_list", "mc_biome_spawn", "mc_biome_entity_class",
    "mc_entity_class_literal", "mc_world_info", "mc_world_info_name", "mc_registry_string",
    "mc_world_saved_data", "native.saved_named_cache", "native.saved_ordered_list", "native.saved_id_map",
    "mc_world_store", "mc_world_store_loader", "native.path_binding", "mc_chunk_state", "mc_world_chunk",
    "mc_chunk_data", "mc_world_chunk_nibble", "mc_world_chunk_list_array", "mc_world_chunk_entity_list",
    "mc_world_chunk_tile_map", "C.uint8_span", "C.double_span", "C.float_span", "mc_biome_reference_array",
    "C.mc_noise_perlin_group", "mc_noise_perlin", "C.mc_noise_simplex_group", "mc_noise_simplex",
    "C.uint16_permutation_span", "C.int32_matrix", "C.int32_row", "mc_world_light_region",
    "mc_world_light_type", "mc_server_reference_list", "mc_server_synchronized_list", "mc_server_tracker_array",
    "mc_world_entity_index", "C.entity_index_pointer_table", "mc_java_random_profile", "mc_java_math_random_owner",
    "C.constructor_input_span", "mc_java_random", "C.mc_chunk_state_pointer_span", "mc_world_dimension_biome",
    "mc_temperature_array", "C.utf8_path_span", "mc_java_int_set"}


class Ledger:
    def __init__(self):
        self.source_classes, self.native_classes = {}, {}
        self.forward, self.reverse = {}, {}
        self.exclusions, self.clock_values = {}, {}
        self.counts = Counter()

    def source(self, value, kind=None, nullable=False):
        if value is None:
            need(nullable, "required source owner")
            return None
        keys(value, ("ref", "class"), "source owner")
        identity = integer(value["ref"], 1, 16384, "source owner identity")
        need(type(value["class"]) is str and 0 < len(value["class"]) <= 256, "source owner class")
        if kind is not None:
            need(value["class"] in ({kind} if type(kind) is str else set(kind)), "source role class: " + repr(kind))
        if identity in self.source_classes:
            equal(self.source_classes[identity], value["class"], "retained source owner class")
        self.source_classes[identity] = value["class"]
        return identity

    def native(self, value, kind=None, nullable=False):
        if value is None:
            need(nullable, "required native owner")
            return None
        keys(value, ("ref", "native_kind"), "native owner")
        identity, actual = value["ref"], value["native_kind"]
        need(type(identity) is str and re.fullmatch(r"d(?:0|[1-9][0-9]*)", identity) is not None,
             "canonical native identity")
        integer(int(identity[1:]), 0, 8191, "native identity capacity")
        need(actual in KINDS, "declared native owner kind")
        if kind is not None:
            equal(actual, kind, "native role kind")
        if identity in self.native_classes:
            equal(self.native_classes[identity], actual, "retained typed native owner")
        self.native_classes[identity] = actual
        return identity

    def bind(self, source, native, kind, label):
        left, right = self.source(source, nullable=True), self.native(native, kind, nullable=True)
        need((left is None) == (right is None), label + ": nullable identity")
        if left is None:
            return
        if left in self.forward:
            equal(self.forward[left], right, label + ": source alias/lifetime")
        if right in self.reverse:
            equal(self.reverse[right], left, label + ": native alias/lifetime")
        self.forward[left], self.reverse[right] = right, left
        self.counts["identity_comparisons"] += 1

    def exclude(self, side, owner, rule, detail):
        need(rule in EXCLUSIONS, "declared representation exclusion")
        key = (side, str(owner), rule)
        self.exclusions[key] = {"side": side, "owner": owner, "rule": rule, "detail": detail}

    def clock(self, role, original, native):
        word(original); word(native)
        pair = (original, native)
        if role in self.clock_values:
            equal(self.clock_values[role], pair, "retained measured clock role " + role)
        self.clock_values[role] = pair
        return ("measured_clock", role)


EXCLUSIONS = {
    "random_inline_cell": "Java AtomicLong seed owner is checked in the source; C stores state inline.",
    "fallback_children": "Native embedded fallback has arrays and state, with no normal Chunk child handles.",
    "noise_aggregate": "C group spans/grid rows/permutations have declared native subobject and integer-width representations.",
    "boxed_key": "Actual Java Integer keys are compared by complete signed value; C has no boxed owner.",
    "enum_name": "Light enum role/name/ordinal are checked; C has no separate enum-name String owner.",
    "path_binding": "Actual Java File/String graph and native UTF8 Store binding use separate host roots.",
    "map_iteration": "Native map transport/backing order is checked separately; no JVM HashMap backing parity is claimed.",
    "membership_iteration": "F membership and record aliases are compared; its native bucket order is not Java HashSet order.",
    "cave_split": "Native cave range and Random are split fields, without a separate native cave object.",
    "native_private": "Typed C diagnostics, budgets, retained ledgers and host lifetime storage have no Java heap counterpart.",
    "server_backing": "The actual C synchronized-list backing is private; Java wrapper contents and wrapper identity are compared.",
    "observer_roots": "Retained-Chunk discovery order is helper transport, separate from actual cache insertion order.",
    "constructor_metadata": "Preparation/construction helper records are validated as inputs and effects, not JVM reflection-object parity.",
    "nbt_compound_order": "Only Compound child order is canonicalized after unique-name checks; List order and every tag type/name/value remain exact.",
}


class Graph:
    def __init__(self, owner, state):
        self.owner, self.l = owner, owner.ledger
        keys(state, ("dedicated", "graph", "info", "disk", "old_far", "reloaded_far"), "source state")
        self.state = state
        graph = state["graph"]
        keys(graph, ("world", "retained_chunks", "shared", "objects"), "source graph")
        need(type(graph["objects"]) is list and 0 < len(graph["objects"]) <= 8192, "complete graph bound")
        self.objects, self.used, self.fields_used = {}, set(), set()
        for obj in graph["objects"]:
            need(type(obj) is dict and "object" in obj, "described source object")
            identity = self.l.source(obj["object"])
            need(identity not in self.objects, "unique object description")
            self.objects[identity] = obj
        for identity, obj in self.objects.items():
            kind = obj["object"]["class"]
            if kind in LINEAGES:
                keys(obj, ("object", "fields", *(["enum_name", "enum_ordinal"] if kind == "co" else [])), "source instance")
                expected = {(decl, name): (desc, mods) for decl in LINEAGES[kind]
                    for name, (desc, mods) in FIELDS[decl].items()}
                need(type(obj["fields"]) is list, "actual field list")
                actual = {}
                for row in obj["fields"]:
                    keys(row, ("declaring_class", "field", "descriptor", "modifiers", "value"), "source field")
                    key = (row["declaring_class"], row["field"])
                    need(key not in actual, "unique case-sensitive source field")
                    actual[key] = (row["descriptor"], integer(row["modifiers"], 0, 65535, "field modifiers"))
                    self.primitive(row["descriptor"], row["value"])
                equal(actual, expected, "complete fixed declaring-class manifest for " + kind)
            elif kind == "java.lang.String":
                keys(obj, ("object", "utf16"), "source String"); utf16(obj["utf16"])
                old = owner.immutable.setdefault((identity, "String"), obj["utf16"])
                equal(old, obj["utf16"], "retained immutable String contents")
            elif kind == "java.lang.Integer":
                keys(obj, ("object", "value_type", "value"), "boxed key")
                equal(obj["value_type"], "I", "Integer primitive type"); integer(obj["value"])
            elif kind == "java.lang.Class":
                keys(obj, ("object", "class_literal"), "canonical source Class")
                need(type(obj["class_literal"]) is str, "canonical class name")
                old = owner.immutable.setdefault((identity, "Class"), obj["class_literal"])
                equal(old, obj["class_literal"], "retained actual Class role")
            elif kind == "java.util.Random":
                keys(obj, ("object", "seed_owner", "state"), "source Random")
                self.ref(obj["seed_owner"], "java.util.concurrent.atomic.AtomicLong")
                random_value(obj["state"])
            elif kind == "java.util.concurrent.atomic.AtomicLong":
                keys(obj, ("object", "raw_long"), "source seed cell"); word(obj["raw_long"])
            elif kind == "java.io.File":
                keys(obj, ("object", "path", "absolute"), "source File")
                self.ref(obj["path"], "java.lang.String"); boolean(obj["absolute"])
            elif kind in COLLECTIONS or kind == "java.util.HashMap":
                keys(obj, ("object", "size", "entries"), "source collection")
                need(type(obj["entries"]) is list, "complete collection entries")
                equal(len(obj["entries"]), integer(obj["size"], 0, 32768, "collection size"), "all source occurrences")
                for index, item in enumerate(obj["entries"]):
                    if kind == "java.util.HashMap":
                        keys(item, ("ordinal", "key", "value"), "map occurrence")
                        equal(integer(item["ordinal"], 0, 32767), index, "actual source map ordinal")
                        self.ref(item["key"], nullable=True); self.ref(item["value"], nullable=True)
                    else:
                        self.ref(item, nullable=True)
            elif kind.startswith("["):
                component = kind[1:].replace(".", "/")
                primitive = component in ("B", "I", "F", "D")
                keys(obj, ("object", "component", "length", "big_endian_data" if primitive else "entries"), "source array")
                equal(obj["component"], component, "source component descriptor")
                length = integer(obj["length"], 0, 32768, "array length")
                old = owner.immutable.setdefault((identity, "array_shape"), (component, length))
                equal(old, (component, length), "same actual source array shape")
                if primitive:
                    equal(len(owner.blob(obj["big_endian_data"])), length * {"B": 1, "I": 4, "F": 4, "D": 8}[component], "all primitive bytes")
                else:
                    need(type(obj["entries"]) is list and len(obj["entries"]) == length, "all reference array entries")
                    for item in obj["entries"]:
                        self.primitive(component, item)
            else:
                raise ValueError("unexplained source class: " + kind)
        self.ref(graph["world"], "dp")

    def primitive(self, desc, value):
        if desc == "Z": boolean(value)
        elif desc in ("B", "I"): integer(value, *({"B": (-128, 127), "I": I32}[desc]))
        elif desc in ("J", "D", "F"): raw_hex(value, 4 if desc == "F" else 8, "primitive bits")
        else:
            need(desc.startswith(("L", "[")), "known source descriptor")
            self.ref(value, nullable=True)
            if value is None: return
            actual = value["class"]
            if desc.startswith("["):
                equal(actual, desc.replace("/", "."), "actual source array assignment type")
                return
            expected = desc[1:-1].replace("/", ".")
            assignable = {"dj": {"dp"}, "hi": {"hi", "hg"}, "os": {"kp"}, "om": {"ie"}, "dy": {"lt"},
                "bl": {"he", "pk"}, "an": {"hb"}, "gs": {"gs", "dw", "f", "kk", "le", "m", "ma", "pp"},
                "java.util.List": {"java.util.ArrayList", "java.util.Collections$SynchronizedRandomAccessList"},
                "java.util.Set": {"java.util.HashSet", "java.util.TreeSet"}, "java.util.Map": {"java.util.HashMap"}}
            need(actual == expected or actual in assignable.get(expected, set()), "actual source assignability for " + desc)

    def ref(self, value, kind=None, nullable=False):
        identity = self.l.source(value, kind, nullable)
        if identity is not None:
            need(identity in self.objects, "every graph reference described")
            equal(self.objects[identity]["object"], value, "source owner declaration")
        return identity

    def obj(self, ref, kind=None):
        identity = self.ref(ref, kind)
        self.used.add(identity)
        return self.objects[identity]

    def field(self, ref, declaring, name):
        obj = self.obj(ref)
        need("fields" in obj, "actual source field receiver")
        rows = [r for r in obj["fields"] if (r["declaring_class"], r["field"]) == (declaring, name)]
        need(len(rows) == 1, "source field " + declaring + "." + name)
        self.fields_used.add((ref["ref"], declaring, name))
        return rows[0]["value"]

    def collection(self, ref, kind=None):
        obj = self.obj(ref, kind or COLLECTIONS | {"java.util.HashMap"})
        return obj["entries"]

    def text(self, ref):
        return self.obj(ref, "java.lang.String")["utf16"]

    def array(self, ref, component):
        obj = self.obj(ref, "[" + component.replace("/", "."))
        return self.owner.blob(obj["big_endian_data"])

    def boxed(self, ref):
        obj = self.obj(ref, "java.lang.Integer")
        self.l.exclude("source", ref["ref"], "boxed_key", "complete signed Integer value compared")
        return obj["value"]

    def finish(self):
        missing = []
        for identity, obj in self.objects.items():
            if identity not in self.used:
                missing.append((identity, obj["object"]["class"], "owner"))
            for row in obj.get("fields", []):
                key = (identity, row["declaring_class"], row["field"])
                if key not in self.fields_used:
                    missing.append(key)
        need(not missing, "unexplained source graph owners/fields: " + repr(missing[:16]))
        self.l.counts["source_object_observations"] += len(self.objects)
        self.l.counts["source_field_observations"] += len(self.fields_used)


class Comparison:
    def __init__(self, original, native, trace, tape, *, legacy_original=False):
        self.o, self.n, self.ledger = original, native, Ledger()
        self.legacy_original = boolean(legacy_original, "explicit legacy original adapter")
        self.shapes, self.native_shapes, self.random_seeds = {}, {}, {}
        self.immutable = {}
        self.native_links, self.native_link_reverse = {}, {}
        self.payload_forward, self.payload_reverse = {}, {}
        self.jdi_aliases = {}
        self.blobs, self.used_blobs = {}, set()
        self.trace = parse_trace(trace, tape, legacy=self.legacy_original)
        self.tape = parse_tape(tape)
        self.boundaries = []
        self.disk_reports = []
        self.original_root = self.native_root = None

    def native_link(self, role, parent, child, kind):
        parent = parent["ref"] if type(parent) is dict else parent
        value = self.ledger.native(child, kind)
        key = (role, parent)
        if key in self.native_links: equal(self.native_links[key], value, "retained actual native relation " + role)
        reverse = (role, value)
        if reverse in self.native_link_reverse: equal(self.native_link_reverse[reverse], parent, "distinct actual native child " + role)
        self.native_links[key], self.native_link_reverse[reverse] = value, parent
        return value

    def payload_alias(self, source, native):
        left, right = self.ledger.source(source), self.ledger.native(native)
        if left in self.payload_forward: equal(self.payload_forward[left], right, "same actual primitive payload storage")
        if right in self.payload_reverse: equal(self.payload_reverse[right], left, "no unexplained primitive-array storage alias")
        self.payload_forward[left], self.payload_reverse[right] = right, left

    def blob(self, value):
        keys(value, ("bytes", "sha256"), "source blob reference")
        size = integer(value["bytes"], 0, 8 * 1024 * 1024, "blob length")
        digest = value["sha256"]
        need(type(digest) is str and digest in self.blobs, "existing complete source blob")
        raw = self.blobs[digest]
        equal(len(raw), size, "complete blob length")
        self.used_blobs.add(digest)
        return raw

    def shape(self, source, native, count, component):
        for table, identity in ((self.shapes, self.ledger.source(source)),
                                (self.native_shapes, self.ledger.native(native))):
            value = (component, count)
            if identity in table:
                equal(table[identity], value, "retained array length/component")
            table[identity] = value

    def span(self, g, source, native, component, length=None):
        if source is None:
            equal(native, None, "actual nullable array")
            return None
        obj = g.obj(source, "[" + component)
        count = obj["length"]
        if length is not None:
            equal(count, length, "fixed source array length")
        if component == "B":
            keys(native, ("owner", "data"), "native byte span")
            keys(native["data"], ("length", "values"), "native byte payload")
            equal(integer(native["data"]["length"], 0, 32768), count, "complete byte extent")
            payload, kind = raw_hex(native["data"]["values"], count), "C.uint8_span"
        else:
            keys(native, ("owner", "length", "values"), "native numerical span")
            equal(integer(native["length"], 0, 32768), count, "complete numerical extent")
            payload = raw_hex(native["values"], count * {"D": 8, "F": 4}[component])
            kind = {"D": "C.double_span", "F": "C.float_span"}[component]
        self.ledger.bind(source, native["owner"], kind, "primitive array")
        self.payload_alias(source, native["owner"])
        self.shape(source, native["owner"], count, component)
        equal(payload, self.blob(obj["big_endian_data"]), "all original/native primitive-array bits")
        self.ledger.counts["array_comparisons"] += 1
        return payload

    def name(self, g, source, native, registry=False):
        if source is None:
            equal(native, None, "nullable String role")
            return None
        keys(native, ("owner", "utf16"), "actual native name")
        self.ledger.bind(source, native["owner"], "mc_registry_string" if registry else "mc_world_info_name", "name identity")
        equal(g.text(source), native["utf16"], "complete UTF16 name")
        utf16(native["utf16"])
        return native["utf16"]

    def ordinary_random(self, g, source, native):
        keys(native, ("owner", "value"), "ordinary Random")
        self.ledger.bind(source, native["owner"], "mc_java_random", "Random owner")
        obj = g.obj(source, "java.util.Random")
        source_value, actual = random_value(obj["state"]), random_value(native["value"], True)
        equal(source_value, actual, "complete used Random without normalization")
        cell = g.obj(obj["seed_owner"], "java.util.concurrent.atomic.AtomicLong")
        equal(cell["raw_long"], "0000" + source_value["seed48"], "actual source AtomicLong value")
        identity = source["ref"]
        if identity in self.random_seeds:
            equal(self.random_seeds[identity], obj["seed_owner"]["ref"], "retained source seed owner")
        for other, seed in self.random_seeds.items():
            need(other == identity or seed != obj["seed_owner"]["ref"], "distinct Random inline-cell correspondence")
        self.random_seeds[identity] = obj["seed_owner"]["ref"]
        self.ledger.exclude("source", obj["seed_owner"]["ref"], "random_inline_cell", "checked actual seed and stable owner")
        self.ledger.counts["random_comparisons"] += 1
        return actual

    def collection(self, g, source, native, kind, empty=False):
        keys(native, ("owner", "size", "entries"), "native collection")
        self.ledger.bind(source, native["owner"], kind, "collection identity")
        source_kind = {"mc_world_reference_list": "java.util.ArrayList", "mc_world_active_chunks": "java.util.HashSet",
            "mc_biome_spawn_list": "java.util.ArrayList", "mc_world_chunk_tile_map": "java.util.HashMap",
            "mc_world_chunk_entity_list": "java.util.ArrayList", "mc_server_reference_list": "java.util.ArrayList",
            "mc_server_synchronized_list": "java.util.Collections$SynchronizedRandomAccessList",
            "native.saved_named_cache": "java.util.HashMap", "native.saved_ordered_list": "java.util.ArrayList",
            "native.saved_id_map": "java.util.HashMap"}[kind]
        entries = g.collection(source, source_kind)
        equal(integer(native["size"], 0, 4096), len(entries), "actual collection size")
        need(type(native["entries"]) is list and len(native["entries"]) == len(entries), "complete native entries")
        if empty:
            equal(entries, [], "finite actual empty collection")
            equal(native["entries"], [], "native actual empty collection")
        return entries

    def biome_array(self, g, source, native):
        if source is None:
            equal(native, None, "nullable Biome array")
            return
        keys(native, ("owner", "length", "entries"), "native Biome array")
        obj = g.obj(source, "[Lgs;")
        self.ledger.bind(source, native["owner"], "mc_biome_reference_array", "Biome array identity")
        equal(integer(native["length"], 0, 32768), obj["length"], "Biome array full extent")
        need(type(native["entries"]) is list and len(native["entries"]) == obj["length"], "all native Biome references")
        self.shape(source, native["owner"], obj["length"], "Lgs;")
        for left, right in zip(obj["entries"], native["entries"]):
            self.ledger.bind(left, right, "mc_biome", "actual Biome array alias")
        self.ledger.counts["array_comparisons"] += 1

    def permutation(self, g, source, native):
        keys(native, ("owner", "length", "element_bits", "values"), "native permutation")
        equal((native["length"], native["element_bits"]), (512, 16), "native permutation representation")
        self.ledger.bind(source, native["owner"], "C.uint16_permutation_span", "permutation array role")
        values = list(struct.unpack(">512i", g.array(source, "I")))
        need(type(native["values"]) is list and len(native["values"]) == 512, "complete permutation values")
        actual = [integer(x, 0, 65535, "actual uint16 cell") for x in native["values"]]
        equal(values, actual, "every original int/native uint16 permutation value")
        self.ledger.exclude("native", native["owner"]["ref"], "noise_aggregate", "uint16 storage; complete signed source int values compared")
        self.ledger.counts["array_comparisons"] += 1

    def noise(self, g, source, native, simplex=False):
        keys(native, ("native_span", "count", "layers"), "native noise group")
        declaring, layer_class, member = ("mq", "bi", "e") if simplex else ("mp", "ab", "d")
        self.ledger.bind(source, native["native_span"], "C.mc_noise_simplex_group" if simplex else "C.mc_noise_perlin_group", "noise aggregate")
        array_ref = g.field(source, declaring, "a")
        layers = g.obj(array_ref, "[L" + layer_class + ";")
        count = g.field(source, declaring, "b")
        equal((integer(native["count"], 0, 32), layers["length"], len(native["layers"])), (count, count, count), "actual noise layer extent")
        self.ledger.exclude("source", array_ref["ref"], "noise_aggregate", "array wrapper absent; every retained layer owner compared")
        for left, right in zip(layers["entries"], native["layers"]):
            offsets = ("offset_x", "offset_z", "offset_unused") if simplex else ("offset_x", "offset_y", "offset_z")
            keys(right, ("owner", *offsets, "permutation"), "native noise layer")
            self.ledger.bind(left, right["owner"], "mc_noise_simplex" if simplex else "mc_noise_perlin", "noise layer owner")
            for field, role in zip("abc", offsets):
                raw_hex(right[role], 8, "noise offset")
                equal(g.field(left, layer_class, field), right[role], "raw noise offset " + role)
            self.permutation(g, g.field(left, layer_class, member), right["permutation"])

    def grid(self, g, source, native):
        keys(native, ("owner", "shape", "element_bits", "rows"), "native provider grid")
        equal((native["shape"], native["element_bits"]), ([32, 32], 32), "actual C matrix shape")
        self.ledger.bind(source, native["owner"], "C.int32_matrix", "provider matrix role")
        obj = g.obj(source, "[[I")
        equal((obj["length"], len(native["rows"])), (32, 32), "all grid rows")
        for left, right in zip(obj["entries"], native["rows"]):
            keys(right, ("owner", "values"), "native grid row")
            self.ledger.bind(left, right["owner"], "C.int32_row", "actual row role")
            need(type(right["values"]) is list and len(right["values"]) == 32, "complete native grid row")
            equal(list(struct.unpack(">32i", g.array(left, "I"))), [integer(x) for x in right["values"]], "complete actual grid values")
        self.ledger.exclude("native", native["owner"]["ref"], "noise_aggregate", "C matrix/row subobjects; no Java row allocation claim")

    def normal(self, g, source, native, world, manager):
        keys(native, ("owner", "world", "world_binding_location", "biome_manager", "biome_owner_correspondence",
            "world_seed", "generator_random", "cave", "noise", "scratch", "v", "w", "grid"), "native normal provider")
        self.ledger.bind(source, native["owner"], "mc_normal_generator", "normal numerical owner")
        self.ledger.bind(g.field(source, "pk", "p"), native["world"], "mc_world_runtime", "normal World binding")
        equal(native["world"], world, "normal same World")
        equal(native["world_binding_location"], "outer_provider.world", "declared native World field location")
        self.ledger.native(native["biome_manager"], "mc_biome_manager")
        equal(native["biome_owner_correspondence"], manager["owner"], "normal actual manager owner correspondence")
        equal(native["world_seed"], SEED, "actual constructor seed")
        self.ordinary_random(g, g.field(source, "pk", "j"), native["generator_random"])
        cave = g.field(source, "pk", "u")
        keys(native["cave"], ("representation", "range", "random"), "native split cave")
        equal(native["cave"]["representation"], "split_fields_in_mc_normal_generator", "actual cave representation")
        equal(g.field(cave, "dy", "a"), integer(native["cave"]["range"]), "cave range")
        self.ordinary_random(g, g.field(cave, "dy", "b"), native["cave"]["random"])
        self.ledger.exclude("source", cave["ref"], "cave_split", "range and same Random compared separately")
        groups = {"k": "low", "l": "high", "m": "mix", "n": "surface_primary", "o": "surface_secondary",
            "a": "column_scale", "b": "column_bias", "c": "later"}
        keys(native["noise"], groups.values(), "all eight terrain noise groups")
        for field, role in groups.items():
            self.noise(g, g.field(source, "pk", field), native["noise"][role])
        keys(native["scratch"], "qdefghrst", "all published scratch fields")
        for field in "qdefghrst":
            self.span(g, g.field(source, "pk", field), native["scratch"][field], "D")
        self.biome_array(g, g.field(source, "pk", "v"), native["v"])
        w = g.field(source, "pk", "w")
        if w is None:
            equal(native["w"], None, "actual null temperature array")
        else:
            keys(native["w"], ("owner", "native_payload"), "actual temperature array")
            self.ledger.bind(w, native["w"]["owner"], "mc_temperature_array", "temperature handle lifetime")
            # One source array corresponds to the real handle; backing storage
            # is checked separately, without binding a second source identity.
            span = native["w"]["native_payload"]
            keys(span, ("owner", "length", "values"), "native temperature backing")
            self.ledger.native(span["owner"], "C.double_span")
            self.native_link("temperature_payload", native["w"]["owner"], span["owner"], "C.double_span")
            self.payload_alias(w, span["owner"])
            obj = g.obj(w, "[D")
            equal(integer(span["length"], 0, 32768), obj["length"], "full temperature extent")
            equal(raw_hex(span["values"], obj["length"] * 8), self.blob(obj["big_endian_data"]), "all actual temperature bits")
            self.shape(w, native["w"]["owner"], obj["length"], "D")
            self.ledger.exclude("native", span["owner"]["ref"], "native_private", "actual temperature backing belongs to compared handle")
        self.grid(g, g.field(source, "pk", "i"), native["grid"])

    def dimension_manager(self, g, world, native):
        d, m = native["dimension"], native["manager"]
        keys(d, ("owner", *"abcdefgh"), "all eight native Dimension fields")
        source = g.field(world, "dj", "t")
        self.ledger.bind(source, d["owner"], "mc_world_dimension", "Dimension owner")
        self.ledger.bind(g.field(source, "os", "a"), d["a"], "mc_world_runtime", "Dimension World")
        manager = g.field(source, "os", "b")
        self.ledger.bind(manager, d["b"], "mc_world_dimension_biome", "Dimension manager owner")
        for field in "cde":
            equal(g.field(source, "os", field), boolean(d[field]), "Dimension flag")
        equal(g.field(source, "os", "g"), integer(d["g"]), "Dimension ID")
        self.span(g, g.field(source, "os", "f"), d["f"], "F", 16)
        self.span(g, g.field(source, "os", "h"), d["h"], "F", 4)
        keys(m, ("owner", "native_numerical_subobject", *"abcd", "simplex"), "complete native manager")
        equal(m["owner"], d["b"], "actual same manager owner")
        self.ledger.native(m["native_numerical_subobject"], "mc_biome_manager")
        self.native_link("numerical_manager", m["owner"], m["native_numerical_subobject"], "mc_biome_manager")
        equal(m["native_numerical_subobject"], native["normal"]["biome_manager"], "same embedded numerical manager")
        for field in "abc":
            self.span(g, g.field(manager, "ph", field), m[field], "D")
        self.biome_array(g, g.field(manager, "ph", "d"), m["d"])
        keys(m["simplex"], ("temperature", "rainfall", "auxiliary"), "all manager noise groups")
        for field, role in zip("efg", ("temperature", "rainfall", "auxiliary")):
            self.noise(g, g.field(manager, "ph", field), m["simplex"][role], True)
        self.ledger.exclude("native", m["native_numerical_subobject"]["ref"], "native_private", "actual embedded manager linked to compared GAME wrapper")

    def biomes(self, g, native, ordinal):
        shared = g.state["graph"]["shared"]
        keys(shared, ("enqueue_depth", "chunk_static_a", "falling", "biome_statics", "selected_static_scope"), "source selected statics")
        keys(native, ("observation", "statics", "objects"), "native Biome observation")
        if ordinal == 0:
            equal((shared["falling"], shared["biome_statics"]), (None, None), "no initial Biome/Block static read")
            equal(native, {"observation": "unobserved_before_first_acquire", "statics": None, "objects": []}, "initial native Biome scope")
            return
        equal(native["observation"], "read_after_acquire", "reached Biome scope")
        keys(shared["biome_statics"], "abcdefghijklm", "all original Biomes")
        keys(native["statics"], "abcdefghijklm", "all native Biomes")
        for role in "abcdefghijklm":
            self.ledger.bind(shared["biome_statics"][role], native["statics"][role], "mc_biome", "Biome static role")
        expected = {r["ref"]: r for r in shared["biome_statics"].values()}
        equal(len(expected), 13, "thirteen distinct actual Biome owners")
        actual = {self.ledger.native(r["owner"], "mc_biome"): r for r in native["objects"]}
        equal(len(actual), len(native["objects"]), "distinct complete native Biome table")
        equal(set(actual), {self.ledger.forward[k] for k in expected}, "all actual Biome owners")
        kinds = {"gs": 0, "dw": 1, "ma": 2, "m": 3, "le": 4, "pp": 5, "kk": 6, "f": 7}
        server_names = ("lp", "be", "ez", "kf", "hl", "ep", "ec", "dv", "bn", "ip", "ne", "nb", "aw",
            "pi", "oc", "ce", "bd", "or", "pf", "eh", "kg", "gl", "pr", "eb", "lq", "em")
        logical = getattr(self, "logical_classes", {})
        spawn_total = 0
        for identity, source in expected.items():
            value = actual[self.ledger.forward[identity]]
            keys(value, ("owner", "kind", *"nopqrvwstu"), "all native Biome fields")
            equal(integer(value["kind"], 0, 7), kinds[source["class"]], "canonical Biome kind")
            self.name(g, g.field(source, "gs", "n"), value["n"], True)
            for field in "opqrvw":
                v = value[field]
                boolean(v) if field in "vw" else integer(v, *((-128, 127) if field in "pq" else I32))
                equal(g.field(source, "gs", field), v, "Biome field " + field)
            for field in "stu":
                source_list, target = g.field(source, "gs", field), value[field]
                entries = self.collection(g, source_list, target, "mc_biome_spawn_list")
                spawn_total += len(entries)
                for left, right in zip(entries, target["entries"]):
                    keys(right, ("owner", "logical_class", "canonical_class", "canonical_kind", "canonical_server_name", "weight"), "native spawn entry")
                    self.ledger.bind(left, right["owner"], "mc_biome_spawn", "spawn owner")
                    cls = g.field(left, "aq", "a")
                    obj = g.obj(cls, "java.lang.Class")
                    self.ledger.bind(cls, right["canonical_class"], "mc_entity_class_literal", "canonical Class role")
                    kind = integer(right["canonical_kind"], 0, len(server_names) - 1, "canonical selector")
                    equal((right["canonical_server_name"], server_names[kind]), (obj["class_literal"], obj["class_literal"]), "exact canonical source Class")
                    handle = self.ledger.native(right["logical_class"], "mc_biome_entity_class")
                    canonical = self.ledger.native(right["canonical_class"], "mc_entity_class_literal")
                    if handle in logical:
                        equal(logical[handle], canonical, "retained logical/canonical Class join")
                    for other, c in logical.items():
                        need(c != canonical or other == handle, "one actual logical handle per canonical Class")
                    logical[handle] = canonical
                    self.ledger.exclude("native", handle, "native_private", "logical handle passively joins the actual compared canonical Class")
                    equal(g.field(left, "aq", "b"), integer(right["weight"]), "spawn weight")
                    self.ledger.counts["spawn_comparisons"] += 1
        self.logical_classes = logical
        equal(spawn_total, 115, "all 115 actual static spawn occurrences")

    def chunk(self, g, source, target, world, fallback=False):
        base = ("owner", "coordinates", "blank", "data_owner", "minimum_height")
        keys(target, (*base, "normal_children", "state", "arrays") if fallback else (*base, "native_header", "fields"), "complete native Chunk")
        self.ledger.bind(source, target["owner"], "mc_chunk_state", "Chunk identity")
        g.ref(source, "hg" if fallback else "hi")
        self.ledger.native(target["data_owner"], "mc_chunk_data")
        self.native_link("chunk_data", target["owner"], target["data_owner"], "mc_chunk_data")
        self.ledger.exclude("native", target["data_owner"]["ref"], "native_private", "actual Chunk data storage relates to semantic Chunk owner")
        equal(boolean(target["blank"]), fallback, "actual blank predicate")
        equal(target["coordinates"], [g.field(source, "hi", "j"), g.field(source, "hi", "k")], "actual immutable Chunk coordinates")
        for coordinate in target["coordinates"]: integer(coordinate)
        equal(integer(target["minimum_height"]), g.field(source, "hi", "i"), "current minimum height")
        equal(g.field(source, "hi", "d"), world, "actual original Chunk World owner")
        if fallback:
            equal(target["normal_children"], "absent_for_embedded_fallback", "explicit fallback representation")
            flags = {"c": "active", "n": "populated", "o": "dirty", "p": "never_save", "q": "has_entities", "r": "last_save_time"}
            keys(target["state"], flags.values(), "actual fallback state")
            for field, role in flags.items():
                value = target["state"][role]
                word(value) if field == "r" else boolean(value)
                equal(g.field(source, "hi", field), value, "fallback state " + field)
            keys(target["arrays"], ARRAYS, "complete fallback arrays")
            self.span(g, g.field(source, "hi", "b"), target["arrays"]["blocks"], "B", 32768)
            self.span(g, g.field(source, "hi", "h"), target["arrays"]["heightmap"], "B", 256)
            for field, role in zip("efg", ("metadata", "skylight", "block_light")):
                nibble = g.field(source, "hi", field)
                self.span(g, g.field(nibble, "ob", "a"), target["arrays"][role], "B", 16384)
                self.ledger.exclude("source", nibble["ref"], "fallback_children", "actual source wrapper distinct; complete payload compared")
            for field in "lm":
                child = g.field(source, "hi", field)
                if field == "l":
                    equal(g.collection(child, "java.util.HashMap"), [], "actual fallback Tile map empty")
                else:
                    obj = g.obj(child, "[Ljava.util.List;")
                    equal(obj["length"], 8, "actual fallback has eight slices")
                    need(len({v["ref"] for v in obj["entries"]}) == 8, "distinct source fallback slice owners")
                    for entry in obj["entries"]:
                        equal(g.collection(entry, "java.util.ArrayList"), [], "actual fallback slice empty")
                        self.ledger.exclude("source", entry["ref"], "fallback_children", "empty slice has no native fallback handle")
                self.ledger.exclude("source", child["ref"], "fallback_children", "empty source collection/array has no native fallback handle")
            return
        self.ledger.native(target["native_header"], "mc_world_chunk")
        self.native_link("chunk_header", target["owner"], target["native_header"], "mc_world_chunk")
        self.ledger.exclude("native", target["native_header"]["ref"], "native_private", "real native header of the same compared Chunk state")
        value = target["fields"]
        keys(value, tuple(FIELDS["hi"]), "all seventeen native Chunk fields")
        self.span(g, g.field(source, "hi", "b"), value["b"], "B", 32768)
        self.span(g, g.field(source, "hi", "h"), value["h"], "B", 256)
        self.ledger.bind(world, value["d"], "mc_world_runtime", "actual native Chunk d")
        for field in "cijknopqr":
            actual = value[field]
            if field in "cnopq": boolean(actual)
            elif field == "r": word(actual)
            else: integer(actual)
            equal(g.field(source, "hi", field), actual, "Chunk field " + field)
        equal(value["i"], target["minimum_height"], "actual duplicated minimum projection")
        equal([value["j"], value["k"]], target["coordinates"], "actual duplicated coordinates")
        for field in "efg":
            nibble = g.field(source, "hi", field)
            keys(value[field], ("owner", "a"), "native nibble fields")
            self.ledger.bind(nibble, value[field]["owner"], "mc_world_chunk_nibble", "nibble child")
            self.span(g, g.field(nibble, "ob", "a"), value[field]["a"], "B", 16384)
        self.collection(g, g.field(source, "hi", "l"), value["l"], "mc_world_chunk_tile_map", True)
        slices = g.field(source, "hi", "m")
        keys(value["m"], ("owner", "length", "entries"), "actual native slice array")
        self.ledger.bind(slices, value["m"]["owner"], "mc_world_chunk_list_array", "slice array owner")
        obj = g.obj(slices, "[Ljava.util.List;")
        equal((obj["length"], value["m"]["length"], len(value["m"]["entries"])), (8, 8, 8), "all eight slices")
        for left, right in zip(obj["entries"], value["m"]["entries"]):
            self.collection(g, left, right, "mc_world_chunk_entity_list", True)

    def cache(self, g, world, native):
        c = native["cache"]
        keys(c, ("owner", "force_load", "world", "fallback", "loader", "generator", "map", "insertion_list", "pending"), "all native cache roles")
        source = g.field(world, "dj", "v")
        self.ledger.bind(source, c["owner"], "mc_world_provider", "dedicated cache owner")
        equal(g.field(source, "he", "a"), boolean(c["force_load"]), "actual provider force flag")
        self.ledger.bind(g.field(source, "he", "h"), c["world"], "mc_world_runtime", "cache World")
        equal(c["world"], native["base_world"]["owner"], "same actual World")
        fallback = g.field(source, "he", "c")
        self.ledger.bind(fallback, c["fallback"], "mc_chunk_state", "actual fallback owner")
        generator, loader = g.field(source, "he", "d"), g.field(source, "he", "e")
        equal(c["generator"], native["normal"]["owner"], "actual embedded generator relation")
        self.ledger.bind(loader, c["loader"], "mc_world_store_loader", "actual loader role")
        source_map, source_list, source_pending = (g.field(source, "he", k) for k in "fgb")
        m = c["map"]
        keys(m, ("owner", "size", "order", "entries", "native_entry_order"), "complete actual native map")
        self.ledger.bind(source_map, m["owner"], "mc_chunk_cache", "cache map role")
        equal(m["order"], "signed_key_transport", "declared canonical map transport")
        rows = g.collection(source_map, "java.util.HashMap")
        source_values = {g.boxed(row["key"]): row["value"] for row in rows}
        equal(len(source_values), len(rows), "unique signed source cache keys")
        equal(integer(m["size"], 0, 32), len(rows), "complete native map size")
        need(type(m["entries"]) is list and len(m["entries"]) == len(rows), "all native map entries")
        actual_keys = []
        for row in m["entries"]:
            keys(row, ("key", "value"), "native map entry")
            key = integer(row["key"]); actual_keys.append(key)
            need(key in source_values, "actual native map key exists in original")
            self.ledger.bind(source_values[key], row["value"], "mc_chunk_state", "exact cache identity value")
        equal(actual_keys, sorted(source_values), "complete signed-key map transport")
        need(type(m["native_entry_order"]) is list, "native map backing order")
        equal(sorted(integer(x) for x in m["native_entry_order"]), actual_keys, "full actual native map backing order")
        self.ledger.exclude("source", source_map["ref"], "map_iteration", "source iteration retained in original; complete key/value map compared")
        insertion = c["insertion_list"]
        keys(insertion, ("native_owner", "size", "entries"), "actual native insertion list")
        self.ledger.bind(source_list, insertion["native_owner"], "C.mc_chunk_state_pointer_span", "cache insertion list role")
        entries = g.collection(source_list, "java.util.ArrayList")
        equal(integer(insertion["size"], 0, 32), len(entries), "all insertion occurrences")
        equal(len(insertion["entries"]), len(entries), "complete native insertion list")
        for left, right in zip(entries, insertion["entries"]): self.ledger.bind(left, right, "mc_chunk_state", "insertion order/alias")
        pending = c["pending"]
        keys(pending, ("owner", "size", "order", "keys", "selection_history"), "actual native pending owner")
        self.ledger.bind(source_pending, pending["owner"], "mc_chunk_pending", "pending Set role")
        expected = [g.boxed(x) for x in g.collection(source_pending, "java.util.HashSet")]
        equal(integer(pending["size"], 0, 32), len(expected), "pending complete size")
        equal(pending["order"], "native_insertion_survivor", "native pending backing order")
        equal(sorted(integer(x) for x in pending["keys"]), sorted(expected), "complete pending values")
        history = pending["selection_history"]
        keys(history, ("native_owner", "order", "keys"), "actual selection history")
        self.ledger.native(history["native_owner"], "mc_java_int_set")
        self.native_link("pending_history", pending["owner"], history["native_owner"], "mc_java_int_set")
        equal(history["order"], "actual_JDK_profile_selection", "actual history profile")
        equal([integer(x) for x in history["keys"]], expected, "actual pending iterator/selection order")
        self.ledger.exclude("native", history["native_owner"]["ref"], "native_private", "actual separate selection-history owner")
        self.normal(g, generator, native["normal"], c["world"], native["manager"])
        return source, fallback, loader, source_values

    def world_server(self, g, n, ordinal):
        world = g.state["graph"]["world"]
        b = n["base_world"]
        keys(b, ("owner", "fields", "collections"), "actual base World")
        self.ledger.bind(world, b["owner"], "mc_world_runtime", "World lifetime")
        keys(b["fields"], FIELDS["dj"], "all 43 base fields")
        for field, (desc, _) in FIELDS["dj"].items():
            left, right = g.field(world, "dj", field), b["fields"][field]
            if desc[0] not in ("L", "["):
                g.primitive(desc, right)
                if field == "I":
                    integer(signed(left), *epoch(self.o["construction_epoch_ms"]), "source constructor World.I")
                    integer(signed(right), *epoch(self.n["construction_epoch_ms"]), "native constructor World.I")
                    self.ledger.clock("World.I", left, right)
                else:
                    equal(left, right, "base World scalar " + field)
        kinds = {"C": "mc_world_reference_list", "b": "mc_world_reference_list", "D": "mc_world_reference_list",
            "c": "mc_world_reference_list", "G": "mc_world_reference_list", "d": "mc_world_reference_list",
            "e": "mc_world_reference_list", "u": "mc_world_reference_list", "K": "mc_world_reference_list",
            "R": "mc_world_reference_list", "P": "mc_world_active_chunks", "E": "mc_scheduled_ordered",
            "F": "mc_scheduled_membership", "r": "mc_java_random", "t": "mc_world_dimension", "v": "mc_world_provider",
            "w": "mc_world_store", "x": "mc_world_info", "z": "mc_world_saved_data"}
        for field, kind in kinds.items():
            self.ledger.bind(g.field(world, "dj", field), b["fields"][field], kind, "World field " + field)
        keys(b["collections"], ("C", "b", "D", "c", "G", "d", "e", "u", "K", "R", "P"), "all actual collection descriptors")
        for field in ("b", "D", "c", "G", "d", "e", "u", "K", "R", "P"):
            self.collection(g, g.field(world, "dj", field), b["collections"][field], kinds[field], True)
            equal(b["collections"][field]["owner"], b["fields"][field], "same source-field native descriptor")
        lights = b["collections"]["C"]
        keys(lights, ("owner", "size", "entries", "entries_in"), "actual light List descriptor")
        equal(lights["owner"], b["fields"]["C"], "same actual C List")
        equal(lights["entries"], None, "light records carried once")
        equal(lights["entries_in"], "lights.records", "complete light record location")
        equal(integer(lights["size"], 0, 4096), len(g.collection(g.field(world, "dj", "C"))), "light count")
        source_d = g.state["dedicated"]
        keys(source_d, ("world", "base_cache", "fields"), "source dedicated duplicate view")
        equal(source_d["world"], world, "source dedicated World")
        equal(source_d["base_cache"], g.field(world, "dj", "v"), "source derived/base alias")
        expected = [{"field": f, "descriptor": desc, "modifiers": mods, "value": g.field(world, "dp", f)}
            for f, (desc, mods) in FIELDS["dp"].items()]
        equal(source_d["fields"], expected, "exact five source dedicated fields")
        d = n["dedicated"]
        keys(d, ("fields", "entity_index"), "native dedicated view")
        keys(d["fields"], FIELDS["dp"], "all native derived fields")
        for field, kind in (("C", "mc_world_provider"), ("F", "mc_server_runtime"), ("G", "mc_world_entity_index")):
            self.ledger.bind(g.field(world, "dp", field), d["fields"][field], kind, "dedicated field " + field)
        for field in "DE": equal(g.field(world, "dp", field), boolean(d["fields"][field]), "dedicated Boolean shadow")
        equal(d["fields"]["C"], b["fields"]["v"], "native C equals base v")
        idx, source_idx = d["entity_index"], g.field(world, "dp", "G")
        keys(idx, ("owner", *"abcde"), "actual Entity index")
        equal(idx["owner"], d["fields"]["G"], "same G owner")
        keys(idx["a"], ("owner", "length", "entries"), "actual index table")
        source_table = g.field(source_idx, "go", "a")
        self.ledger.bind(source_table, idx["a"]["owner"], "C.entity_index_pointer_table", "index table")
        table = g.obj(source_table, "[Log;")
        equal((idx["a"]["length"], idx["a"]["entries"]), (table["length"], table["entries"]), "complete empty actual go table")
        need(all(v is None for v in table["entries"]), "finite empty index nodes")
        for field in "bcde":
            raw_hex(idx[field], 4) if field == "d" else integer(idx[field])
            equal(g.field(source_idx, "go", field), idx[field], "index field " + field)
        self.server(g, g.field(world, "dp", "F"), n["server"])
        equal(n["server"]["owner"], d["fields"]["F"], "same actual server")
        return world

    def server(self, g, source, n):
        keys(n, ("owner", "fields", "r", "s", "k", "native_private"), "complete native Server")
        self.ledger.bind(source, n["owner"], "mc_server_runtime", "server owner")
        keys(n["fields"], FIELDS[SERVER], "all eighteen Server fields")
        for field, (desc, _) in FIELDS[SERVER].items():
            left, right = g.field(source, SERVER, field), n["fields"][field]
            if field in "rski": continue
            if desc[0] in ("L", "["):
                equal((left, right), (None, None), "finite actual null Server service")
            else:
                g.primitive(desc, right); equal(left, right, "Server scalar " + field)
        self.name(g, g.field(source, SERVER, "i"), n["fields"]["i"])
        for field, kind in (("r", "mc_server_reference_list"), ("s", "mc_server_synchronized_list")):
            self.collection(g, g.field(source, SERVER, field), n[field], kind, True)
            equal(n[field]["owner"], n["fields"][field], "actual Server collection field")
        arr = g.field(source, SERVER, "k")
        keys(n["k"], ("owner", "length", "entries"), "actual Server tracker array")
        self.ledger.bind(arr, n["k"]["owner"], "mc_server_tracker_array", "Server tracker array")
        obj = g.obj(arr, "[Lnl;")
        equal((obj["length"], obj["entries"], n["k"]["length"], n["k"]["entries"]), (2, [None, None], 2, [None, None]), "actual empty tracker cells")
        equal(n["k"]["owner"], n["fields"]["k"], "same Server k")
        p = n["native_private"]
        keys(p, ("s_backing", "lifetime_started", "references"), "Server lifetime adaptation")
        backing = self.ledger.native(p["s_backing"], "mc_server_reference_list")
        self.native_link("server_s_backing", n["owner"], p["s_backing"], "mc_server_reference_list")
        need(backing != n["r"]["owner"]["ref"], "distinct real synchronized backing")
        equal(boolean(p["lifetime_started"]), True, "actual inert lifetime started")
        equal(integer(p["references"], 0, 100), 2, "actual caller and World server holds")
        self.ledger.exclude("native", backing, "server_backing", "real distinct empty backing and inert waiter are checked")

    def lights_scheduled(self, g, world, n):
        lights = n["lights"]
        keys(lights, ("owner", "size", "records", "types"), "complete light queue")
        source_list = g.field(world, "dj", "C")
        self.ledger.bind(source_list, lights["owner"], "mc_world_reference_list", "same light queue")
        source_entries = g.collection(source_list, "java.util.ArrayList")
        equal((integer(lights["size"], 0, 4096), len(lights["records"])), (len(source_entries), len(source_entries)), "all ordered light occurrences")
        types = {}
        for row in lights["types"]:
            keys(row, ("owner", "kind", "c"), "complete native light type")
            identity = self.ledger.native(row["owner"], "mc_world_light_type")
            need(identity not in types, "unique type record")
            types[identity] = row
        reached_types = set()
        for left, right in zip(source_entries, lights["records"]):
            keys(right, ("owner", "a", "bounds"), "complete native light record")
            self.ledger.bind(left, right["owner"], "mc_world_light_region", "light record identity/merge")
            co = g.field(left, "lv", "a")
            self.ledger.bind(co, right["a"], "mc_world_light_type", "actual shared light type")
            identity = right["a"]["ref"]
            need(identity in types, "every actual light type described")
            row, obj = types[identity], g.obj(co, "co")
            ordinal = integer(obj["enum_ordinal"], 0, 1, "actual Enum ordinal")
            equal(integer(row["kind"], 0, 1), ordinal, "Light semantic enum kind")
            equal(utf16(g.text(obj["enum_name"])), ("Sky", "Block")[ordinal], "actual enum name value")
            self.ledger.exclude("source", obj["enum_name"]["ref"], "enum_name", "actual name checked; any source-only String alias remains recorded")
            equal(g.field(co, "co", "c"), integer(row["c"]), "actual light maximum field")
            bounds = [g.field(left, "lv", field) for field in "bcdefg"]
            need(type(right["bounds"]) is list and len(right["bounds"]) == 6, "all six native light bounds")
            equal([integer(v) for v in right["bounds"]], bounds, "ordered actual light bounds")
            reached_types.add(identity)
        equal(set(types), reached_types, "no unexplained light type records")
        sched = n["scheduled"]
        keys(sched, ("E", "F", "records"), "complete scheduler")
        e, f = g.field(world, "dj", "E"), g.field(world, "dj", "F")
        for role, owner, kind, order in (("E", e, "mc_scheduled_ordered", "actual_structural_inorder"),
                                       ("F", f, "mc_scheduled_membership", "native_bucket_traversal")):
            keys(sched[role], ("owner", "size", "order", "entries"), "actual scheduler collection")
            self.ledger.bind(owner, sched[role]["owner"], kind, "scheduler collection " + role)
            equal(sched[role]["order"], order, "declared actual scheduling order")
            equal(integer(sched[role]["size"], 0, 4096), len(g.collection(owner)), "all scheduler occurrences")
            equal(len(sched[role]["entries"]), sched[role]["size"], "complete native scheduler entries")
        originals = {}
        for left, right in zip(g.collection(e, "java.util.TreeSet"), sched["E"]["entries"]):
            self.ledger.bind(left, right, "mc_scheduled_tick", "exact structural E order and alias")
            originals[left["ref"]] = left
        source_f = g.collection(f, "java.util.HashSet")
        equal({v["ref"] for v in source_f}, set(originals), "finite actual E/F share complete records")
        actual_f = [self.ledger.native(v, "mc_scheduled_tick") for v in sched["F"]["entries"]]
        equal(len(actual_f), len(set(actual_f)), "native F has no duplicate identities")
        equal(set(actual_f), {self.ledger.forward[v["ref"]] for v in source_f}, "F membership and shared SAME records")
        records = {}
        for row in sched["records"]:
            keys(row, ("owner", "x", "y", "z", "block", "due_time", "sequence"), "complete scheduled record")
            identity = self.ledger.native(row["owner"], "mc_scheduled_tick")
            need(identity not in records, "one union record description")
            records[identity] = row
        equal(set(records), set(actual_f), "no dropped/orphan scheduled descriptions")
        for identity, ref in originals.items():
            row = records[self.ledger.forward[identity]]
            for field, role in zip("abcdeg", ("x", "y", "z", "block", "due_time", "sequence")):
                word(row[role]) if field in "eg" else integer(row[role])
                equal(g.field(ref, "ki", field), row[role], "actual scheduled field " + role)
        self.ledger.exclude("source", f["ref"], "membership_iteration", "complete set of globally matched SAME records compared")
        self.ledger.counts["light_record_comparisons"] += len(source_entries)
        self.ledger.counts["scheduled_record_comparisons"] += len(originals)

    def info(self, source, native, g=None, preparation=False):
        keys(source, ("object" if preparation else "owner", "fields"), "actual Info duplicate view")
        source_ref = source["object" if preparation else "owner"]
        self.ledger.source(source_ref, "ct")
        keys(native, ("owner", *INFO), "all fifteen native Info fields")
        self.ledger.bind(source_ref, native["owner"], "mc_world_info", "Info owner")
        need(type(source["fields"]) is list and len(source["fields"]) == 15, "complete source Info field view")
        values = {}
        for index, (row, role, desc) in enumerate(zip(source["fields"], INFO, INFO_DESCS)):
            keys(row, ("field", "descriptor", "value") if preparation else ("field", "role", "descriptor", "modifiers", "value"), "Info field")
            equal((row["field"], row["descriptor"]), (chr(97 + index), desc), "exact Info declaration")
            if not preparation:
                equal((row["role"], row["modifiers"]), (role, 2), "exact Info role/modifiers")
            left = row["value"]
            if role == "level_name":
                keys(left, ("object", "utf16"), "actual Info String view")
                self.ledger.source(left["object"], "java.lang.String")
                keys(native[role], ("owner", "utf16"), "actual Info name")
                self.ledger.bind(left["object"], native[role]["owner"], "mc_world_info_name", "Info name alias")
                utf16(left["utf16"]); utf16(native[role]["utf16"])
                equal(left["utf16"], native[role]["utf16"], "entire Info UTF16 name")
                if g is not None:
                    equal(g.field(source_ref, "ct", row["field"]), left["object"], "Info String view is actual cell")
                    equal(g.text(left["object"]), left["utf16"], "Info graph String agrees")
                values[role] = left["utf16"]
                continue
            if desc == "J": word(left); word(native[role])
            elif desc == "Z": boolean(left); boolean(native[role])
            elif desc == "I": integer(left); integer(native[role])
            else: equal((left, native[role]), (None, None), "finite absent Player compound")
            if g is not None:
                equal(g.field(source_ref, "ct", row["field"]), left, "Info duplicate agrees with actual graph cell")
            if role == "last_played" and not preparation:
                self.ledger.clock("prepared_metadata.LastPlayed", left, native[role])
            elif role != "last_played" or left == "0000000000000000":
                equal(left, native[role], "Info field " + role)
            values[role] = left
        return values

    def saved_paths(self, g, world, loader, native):
        value = native["saved_data"]
        keys(value, ("owner", "native_storage_binding", "transport_store_correspondence", "named_cache",
            "ordered_list", "id_map", "native_last_caught"), "complete actual SavedData")
        source = g.field(world, "dj", "z")
        self.ledger.bind(source, value["owner"], "mc_world_saved_data", "SavedData manager")
        store = g.field(world, "dj", "w")
        equal(g.field(source, "es", "a"), store, "actual SavedData same handler")
        equal(value["transport_store_correspondence"], native["base_world"]["fields"]["w"], "actual native transport Store correspondence")
        binding = self.ledger.native(value["native_storage_binding"], "native.path_binding")
        self.native_link("saved_storage_binding", value["owner"], value["native_storage_binding"], "native.path_binding")
        self.ledger.exclude("native", binding, "path_binding", "actual manager copied path binding; not a Java handler")
        for field, role, kind in (("b", "named_cache", "native.saved_named_cache"),
            ("c", "ordered_list", "native.saved_ordered_list"), ("d", "id_map", "native.saved_id_map")):
            self.collection(g, g.field(source, "es", field), value[role], kind, True)
        self.storage_error(value["native_last_caught"])
        path_values, file_refs = {}, {}
        for field, leaf in (("b", ""), ("c", "players"), ("d", "data")):
            ref = g.field(store, "dq", field)
            path_values[leaf], file_refs[leaf] = self.file(g, ref), ref
        source_path = captured_path(path_values[""])
        need(source_path.is_absolute() and source_path.name == "world", "actual source live handler world folder")
        base = str(source_path)
        for leaf in ("players", "data"):
            child = captured_path(path_values[leaf])
            equal(type(child), type(source_path), "same source producer path flavor")
            equal(child.parts, (source_path / leaf).parts, "actual source handler child File binding")
        loader_file = g.field(loader, "hb", "a")
        loader_path = captured_path(self.file(g, loader_file))
        equal(type(loader_path), type(source_path), "same source loader path flavor")
        equal(loader_path.parts, source_path.parts, "actual loader world-directory File")
        equal(loader_file, file_refs[""], "actual loader/handler same File alias")
        private_root = native["native_private"]["store_root"]
        keys(private_root, ("native_span", "utf8_bytes"), "actual native UTF8 Store root")
        self.ledger.native(private_root["native_span"], "C.utf8_path_span")
        self.native_link("store_path", native["base_world"]["fields"]["w"], private_root["native_span"], "C.utf8_path_span")
        keys(private_root["utf8_bytes"], ("length", "values"), "actual native path bytes")
        raw = raw_hex(private_root["utf8_bytes"]["values"], integer(private_root["utf8_bytes"]["length"], 1, 32768))
        need(b"\0" not in raw, "non-NUL native path")
        path = raw.decode("utf-8", "strict")
        # Parse the producer's path independently of this verifier's host OS
        # and Python's changing treatment of root-relative Windows paths.
        native_path = captured_path(path)
        need(native_path.is_absolute(), "actual absolute native Store binding")
        need(native_path.name == "world", "native actual world path role")
        if self.original_root is None:
            self.original_root, self.native_root = base, path
        equal((self.original_root, self.native_root), (base, path), "retained actual per-run path roots")
        self.ledger.exclude("native", private_root["native_span"]["ref"], "path_binding", "complete actual UTF8 path bytes checked independently of source host path")
        return g.field(store, "dq", "e")

    def file(self, g, ref):
        obj = g.obj(ref, "java.io.File")
        value = utf16(g.text(obj["path"]))
        equal(obj["absolute"], captured_path(value).is_absolute(), "actual source File absolute bit")
        self.ledger.exclude("source", ref["ref"], "path_binding", "actual path/absolute fields checked; no fabricated C File owner")
        self.ledger.exclude("source", obj["path"]["ref"], "path_binding", "complete source path String checked by actual role")
        return value

    @staticmethod
    def storage_error(value):
        keys(value, ("status", "status_name", "operation", "system_error"), "actual native diagnostic")
        equal(integer(value["status"], 0, 100), 0, "finite successful native diagnostic")
        equal(value["status_name"], "ok", "actual native status spelling")
        need(value["operation"] is None or type(value["operation"]) is str, "actual diagnostic operation")
        equal(integer(value["system_error"], 0, (1 << 32) - 1), 0, "no caught native I/O error")

    def profile(self, n, ordinal):
        p = n["profile"]
        keys(p, ("owner", "uniquifier", "constructor_admitted", "inputs", "math", "native_private"), "actual complete Random profile")
        profile = self.ledger.native(p["owner"], "mc_java_random_profile")
        self.native_link("world_profile", n["base_world"]["owner"], p["owner"], "mc_java_random_profile")
        self.ledger.exclude("native", profile, "native_private", "actual shared constructor owner, not a Java object projection")
        equal(boolean(p["constructor_admitted"]), True, "genuine constructor admitted")
        marker = "operation_0_before" if ordinal == 0 else "operation_" + str(ordinal - 1) + "_after"
        observed = self.trace["boundaries"][marker]
        equal(p["uniquifier"], observed["uniquifier"], "full-width shared U without normalization")
        indices = [i for i, u in enumerate(["001ed8b55fac9dec", *[r["committed_uniquifier"] for r in self.tape]]) if u == p["uniquifier"]]
        need(len(indices) == 1, "unique constructor prefix U")
        inputs = p["inputs"]
        keys(inputs, ("native_span", "bound", "count", "consumed", "failure", "values"), "actual bound constructor input span")
        span = self.ledger.native(inputs["native_span"], "C.constructor_input_span")
        self.native_link("profile_inputs", p["owner"], inputs["native_span"], "C.constructor_input_span")
        equal((boolean(inputs["bound"]), integer(inputs["count"], 0, 65536), integer(inputs["consumed"], 0, 65536),
            integer(inputs["failure"], 0, 3)), (True, 9, indices[0], 0), "actual complete constructor input prefix")
        need(type(inputs["values"]) is list and len(inputs["values"]) == 9, "all declared input rows")
        for row in inputs["values"]:
            keys(row, ("committed_uniquifier", "nanotime"), "native constructor row")
            word(row["committed_uniquifier"]); word(row["nanotime"])
        equal(inputs["values"], self.tape, "native used the independently observed constructor inputs only")
        self.ledger.exclude("native", span, "native_private", "borrowed immutable constructor-input artifact, no expected state")
        value = random_value(n["world_random"]["value"], True)
        world_state = self.trace["states"][(marker, "World_r")]
        equal(value, world_state["value"], "actual World.r JDI/graph/native boundary agreement")
        if ordinal == 0:
            equal(random_value(n["normal"]["cave"]["random"]["value"], True),
                self.trace["states"][("constructor_3_default_return", "ordinary")]["value"],
                "actual fresh cave stream retains its observed constructor continuation")
        alias = ("World_r", world_state["owner"])
        previous = self.jdi_aliases.setdefault(alias, n["world_random"]["owner"]["ref"])
        equal(previous, n["world_random"]["owner"]["ref"], "JDI/helper/native World.r role alias")
        m = p["math"]
        keys(m, ("phase", "owner", "value"), "actual Math holder state")
        original_math = self.trace["math"][marker]
        ready = original_math["initialized"]
        equal(integer(m["phase"], 0, 3), 2 if ready else 0, "same actual Math initialization boundary")
        if not ready:
            equal((m["owner"], m["value"]), (None, None), "actual cold Math owner")
        else:
            ref = self.ledger.native(m["owner"], "mc_java_math_random_owner")
            math_state = self.trace["states"][(marker, "Math")]
            equal(random_value(m["value"], True), math_state["value"], "complete used Math state without normalization")
            alias = ("Math", math_state["owner"])
            previous_alias = self.jdi_aliases.setdefault(alias, ref)
            equal(previous_alias, ref, "JDI/native actual Math role alias")
            previous = getattr(self, "math_ref", ref)
            equal(previous, ref, "retained actual native Math owner")
            self.math_ref = ref
            need(ref != n["world_random"]["owner"]["ref"], "distinct actual Math and World Random owners")
        private = p["native_private"]
        keys(private, ("allocation_bytes", "max_allocation_bytes", "state_storage", "state_offset"), "actual Math representation")
        equal(private["state_storage"], "inline_atomic_in_math_owner", "actual atomic storage role")
        used = integer(private["allocation_bytes"], 1, 1 << 40)
        need(used <= integer(private["max_allocation_bytes"], 1, (1 << 64) - 1), "actual profile budget")
        equal(integer(private["state_offset"], 0, 64), 0, "actual atomic state offset")

    def native_private(self, n, ordinal):
        value = n["native_private"]
        keys(value, ("world_construction", "dedicated_construction", "registry", "scheduled_class", "loader", "store_root", "store_session_token",
            "store_chunks", "store_pending_stages", "provider", "random_inline_state", "jvm_internal_heap_identity",
            "server_waiter_representation", "world_native_lifetime", "world_ledgers", "world_last_error",
            "world_last_registry_error", "world_last_random_status"), "complete declared native private state")
        word(value["store_session_token"])
        c = value["world_construction"]
        keys(c, ("mode", "phase", "entry", "last_store_pc", "failed_call_pc", "contexts_bound", "cache_registered", "direct_allocation_bytes"), "actual World construction")
        equal(tuple(integer(c[k], 0, 10) for k in ("mode", "phase", "entry")), (1, 2, 4), "real ready dedicated GAME route")
        for k in ("last_store_pc", "failed_call_pc"): integer(c[k], 0, 65535)
        for k in ("contexts_bound", "cache_registered"): equal(boolean(c[k]), True, "actual bound cache contexts")
        integer(c["direct_allocation_bytes"], 1, 1 << 40)
        keys(value["dedicated_construction"], ("last_store_pc", "failed_call_pc"), "derived constructor sites")
        for x in value["dedicated_construction"].values(): integer(x, 0, 65535)
        keys(value["registry"], ("blocks_phase", "biomes_phase"), "actual cold-order Registry state")
        expected_phase = 0 if ordinal == 0 else 2
        equal(tuple(integer(value["registry"][k], 0, 3) for k in ("blocks_phase", "biomes_phase")), (expected_phase, expected_phase), "no observer prewarm; actual reached static owners")
        s = value["scheduled_class"]
        keys(s, ("initialized", "next"), "actual scheduler class state")
        boolean(s["initialized"]); word(s["next"])
        if not s["initialized"]: equal(s["next"], "0000000000000000", "cold sequence default")
        for row in n["scheduled"]["records"]:
            need(s["initialized"] and signed(row["sequence"]) < signed(s["next"]), "actual retained sequence precedes current counter")
        loader = value["loader"]
        keys(loader, ("owner", "store", "dimension", "last_error"), "actual native loader binding")
        equal(loader["owner"], n["cache"]["loader"], "same loader subobject")
        equal(loader["store"], n["base_world"]["fields"]["w"], "actual loader same Store")
        equal(integer(loader["dimension"]), 0, "normal loader dimension")
        self.storage_error(loader["last_error"])
        equal(integer(value["store_chunks"], 0, 32), len(n["retained_chunks"]["objects"]) - 1, "all actual Store normal owners")
        equal(integer(value["store_pending_stages"], 0, 32), 0, "no unpublished stage hidden at successful boundary")
        p = value["provider"]
        keys(p, ("max_chunks", "direct_allocation_bytes", "max_direct_allocation_bytes"), "actual provider allocation bounds")
        equal(integer(p["max_chunks"], 0, 32), 32, "actual configured capacity")
        need(integer(p["direct_allocation_bytes"], 1, 1 << 40) <= integer(p["max_direct_allocation_bytes"], 1, 1 << 40), "actual provider budget")
        r = value["random_inline_state"]
        keys(r, ("storage", "state_offset", "state_bytes", "gaussian_value_offset", "gaussian_flag_offset"), "actual native Random cell layout")
        equal(r, {"storage": "inline_cells_in_actual_random_owner", "state_offset": 0, "state_bytes": 8,
            "gaussian_value_offset": 8, "gaussian_flag_offset": 16}, "actual qualified x64 inline cells without fake AtomicLongs")
        equal(value["jvm_internal_heap_identity"], False, "explicit heap representation limit")
        equal(value["server_waiter_representation"], "native_lifetime_condition", "inert native lifetime adaptation")
        lifetime = value["world_native_lifetime"]
        keys(lifetime, ("active_calls", "local_leases", "persistent_bindings"), "native World lifetime")
        equal(integer(lifetime["active_calls"], 0, 100), 0, "quiescent observation")
        equal(integer(lifetime["local_leases"], 0, 100), 0, "balanced local leases")
        integer(lifetime["persistent_bindings"], 1, 100)
        keys(value["world_ledgers"], ("entity_membership", "entity_deferral", "nearby_entities", "collision_boxes", "tile_membership", "tile_deferral"), "all actual native typed ledgers")
        for item in value["world_ledgers"].values(): equal(integer(item, 0, 32768), 0, "actual typed holds empty")
        self.storage_error(value["world_last_error"])
        error = value["world_last_registry_error"]
        keys(error, ("status", "message", "initializer_pc", "block_id", "constructor_class", "constructor_pc", "profile_status", "active_initializer_class", "active_initializer_pc", "item_argument", "format"), "actual Registry diagnostic")
        equal(integer(error["status"], 0, 100), 0, "successful Registry state")
        equal(integer(error["profile_status"], 0, 100), 0, "successful nested Random constructor")
        for key in ("message", "constructor_class", "active_initializer_class"):
            need(error[key] is None or type(error[key]) is str, "actual nullable diagnostic text")
        for key in ("initializer_pc", "block_id", "constructor_pc", "active_initializer_pc", "item_argument"): integer(error[key])
        fmt = error["format"]
        keys(fmt, ("status", "offset", "length", "value", "flags", "conversion", "date_time"), "actual formatting diagnostic")
        equal(integer(fmt["status"], 0, 100), 0, "no formatting failure")
        for key in ("offset", "length"): integer(fmt[key], 0, (1 << 64) - 1)
        integer(fmt["flags"], 0, (1 << 32) - 1); integer(fmt["conversion"], 0, 65535)
        integer(fmt["value"]); boolean(fmt["date_time"])
        equal(integer(value["world_last_random_status"], 0, 100), 0, "successful World profile status")
        self.ledger.exclude("native", n["base_world"]["owner"]["ref"], "native_private", "every declared private scalar/diagnostic/binding checked; not JVM fields")

    def disk(self, value, original):
        keys(value, ("complete", "entries", "files", "directories", "bytes"), "complete recursive disk inventory")
        equal(boolean(value["complete"]), True, "actual complete disk capture")
        need(type(value["entries"]) is list and 1 <= len(value["entries"]) <= 256, "bounded complete inventory")
        paths, files, dirs, total = {}, {}, [], 0
        for row in value["entries"]:
            need(type(row) is dict and row.get("kind") in ("file", "directory"), "ordinary actual disk entry")
            expected = ("path", "kind")
            if row["kind"] == "file": expected += ("data",) if original else ("bytes", "hex")
            keys(row, expected, "complete disk entry")
            path = row["path"]
            need(type(path) is str and path not in paths and len(path) <= 1024 and
                (path == "." or all(re.fullmatch(r"[A-Za-z0-9_.-]+", p) is not None and p not in (".", "..") for p in path.split("/"))), "ordinary unique relative path")
            need(path == "." or paths.get(path.rpartition("/")[0] or ".") == "directory", "actual inventory parent")
            paths[path] = row["kind"]
            if row["kind"] == "directory":
                dirs.append(path)
            else:
                raw = self.blob(row["data"]) if original else raw_hex(row["hex"], integer(row["bytes"], 0, 4 * 1024 * 1024))
                need(len(raw) <= 4 * 1024 * 1024, "per-file byte bound")
                files[path] = raw; total += len(raw)
        equal(list(paths), sorted(paths), "sorted complete inventory")
        equal(value["entries"][0], {"path": ".", "kind": "directory"}, "actual inventoried root")
        equal(tuple(integer(value[k], 0, 8 * 1024 * 1024) for k in ("files", "directories", "bytes")), (len(files), len(dirs), total), "complete disk totals")
        need(total <= 8 * 1024 * 1024, "complete disk budget")
        return files, dirs

    def metadata(self, raw, before, interval):
        tree = parse_nbt(decompress(raw, True, 65536))
        data = compound(tree, "", {"Data"})["Data"]
        children = compound(data, "Data", {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed", "LevelName", "version", "raining", "rainTime", "thundering", "thunderTime"})
        stamp = children["LastPlayed"]
        equal(stamp["type"], 4, "actual metadata clock type")
        integer(stamp["value"], *epoch(interval), "actual metadata clock in its own measured writer interval")
        expected = {"RandomSeed": (4, signed(before["seed"])), "SpawnX": (3, before["spawn_x"]),
            "SpawnY": (3, before["spawn_y"]), "SpawnZ": (3, before["spawn_z"]), "Time": (4, signed(before["time"])),
            "SizeOnDisk": (4, signed(before["size_on_disk"])), "LastPlayed": (4, stamp["value"]),
            "LevelName": (8, before["level_name"]), "version": (3, 19132), "raining": (1, int(before["raining"])),
            "rainTime": (3, before["rain_time"]), "thundering": (1, int(before["thundering"])), "thunderTime": (3, before["thunder_time"])}
        for name, (kind, value) in expected.items():
            equal(children[name], tag(kind, name, value), "complete typed metadata tag " + name)
        return {"tree": tree, "last_played": f"{stamp['value'] & MASK:016x}", "bytes": fingerprint(raw)}

    def region(self, raw, intervals):
        need(type(raw) is bytes and len(raw) >= 8192 and len(raw) % 4096 == 0, "complete region sector inventory")
        records, allocated = {}, set()
        for slot in range(1024):
            location = struct.unpack_from(">I", raw, slot * 4)[0]
            stamp = struct.unpack_from(">I", raw, 4096 + slot * 4)[0]
            if location == 0:
                equal(stamp, 0, "every unoccupied timestamp cell")
                continue
            need(slot in intervals, "no unexplained occupied region slot")
            lo, hi = epoch(intervals[slot])
            integer(stamp, lo // 1000, hi // 1000, "actual region timestamp in its own source save interval")
            start, count = (location >> 8) * 4096, location & 255
            need(start >= 8192 and count > 0 and start + count * 4096 <= len(raw), "complete region record extent")
            sectors = set(range(start // 4096, start // 4096 + count))
            need(not allocated & sectors, "disjoint actual region records")
            allocated |= sectors
            size = struct.unpack_from(">I", raw, start)[0]
            need(1 < size <= count * 4096 - 4 and raw[start + 4] in (1, 2), "actual complete compressed record")
            compressed = raw[start + 5:start + 4 + size]
            tree = parse_nbt(decompress(compressed, raw[start + 4] == 1, 2 * 1024 * 1024))
            level = compound(tree, "", {"Level"})["Level"]
            compound(level, "Level", {"xPos", "zPos", "LastUpdate", "TerrainPopulated", "Blocks", "Data", "SkyLight", "BlockLight", "HeightMap", "Entities", "TileEntities"})
            equal(raw[start + 4 + size:start + count * 4096], bytes(count * 4096 - 4 - size), "all allocated record padding")
            records[slot] = {"tree": tree, "level": level, "sector": start // 4096, "sectors": count,
                "timestamp": stamp, "compression": raw[start + 4], "compressed": compressed}
        equal(allocated, set(range(2, len(raw) // 4096)), "complete allocated-sector accounting")
        equal(set(records), set(intervals), "every expected occupied slot present")
        return records

    def saved_chunk(self, g, ref, level):
        expected = {"xPos": (3, g.field(ref, "hi", "j")), "zPos": (3, g.field(ref, "hi", "k")),
            "LastUpdate": (4, 1234), "TerrainPopulated": (1, int(g.field(ref, "hi", "n")))}
        for name, field in (("Blocks", "b"), ("Data", "e"), ("SkyLight", "f"), ("BlockLight", "g"), ("HeightMap", "h")):
            array = g.field(ref, "hi", field)
            if field in "efg": array = g.field(array, "ob", "a")
            expected[name] = (7, g.array(array, "B").hex())
        for name in ("Entities", "TileEntities"): expected[name] = (9, {"element_type": 1, "items": []})
        actual = compound(level, "Level", set(expected))
        for name, (kind, value) in expected.items(): equal(actual[name], tag(kind, name, value), "actual presave Chunk full typed tag " + name)

    def persistence(self):
        # Both runs retain their actual clock values. Only these named samples
        # receive a correspondence after type/range and within-run joins pass.
        o_ops, n_ops = self.o["operations"], self.n["operations"]
        prep = self.o["metadata_preparation"]
        keys(prep, ("scope", "handler", "info", "readback_info", "supplied_players", "operations", "level_dat", "session_lock"), "complete metadata preparation")
        equal(prep["operations"], self.o["metadata_preparation_calls"], "same actual preparation operations")
        need(type(prep["operations"]) is list and len(prep["operations"]) == 4, "four actual preparation APIs")
        p_interval = prep["operations"][1]["epoch_ms"]
        equal(prep["scope"], "actual original metadata APIs only; no prepared Chunk or World mutation", "honest preparation scope")
        self.ledger.source(prep["handler"], "ie")
        keys(prep["supplied_players"], ("object", "size"), "actual supplied preparation List")
        self.ledger.source(prep["supplied_players"]["object"], "java.util.ArrayList")
        equal(integer(prep["supplied_players"]["size"], 0, 32768), 0, "actual empty preparation Player list")
        for row, action in zip(prep["operations"], ("actual_info_spawn", "actual_handler_metadata_save", "actual_prepared_info_readback", "actual_preparation_handler_close")):
            keys(row, ("action", "receiver", "class", "method", "descriptor", "arguments", "result", "epoch_ms"), "source preparation API")
            equal(row["action"], action, "actual preparation order"); epoch(row["epoch_ms"])
        equal(prep["operations"][0]["arguments"], [0, 64, 0], "preparation through actual spawn API")
        prep_methods = (("ct", "a", "(III)V", prep["info"]["object"], [0, 64, 0], None),
            ("ie", "a", "(Lct;Ljava/util/List;)V", prep["handler"], [prep["info"]["object"], prep["supplied_players"]["object"]], None),
            ("dq", "c", "()Lct;", prep["handler"], [], prep["readback_info"]["object"]),
            ("ie", "e", "()V", prep["handler"], [], None))
        for row, expected in zip(prep["operations"], prep_methods):
            equal(tuple(row[k] for k in ("class", "method", "descriptor", "receiver", "arguments", "result")), expected, "exact actual preparation call binding/input/result")
        self.ledger.exclude("source", prep["handler"]["ref"], "constructor_metadata", "actual preparation handler is separate from live handler; no native JVM handler object")
        self.ledger.exclude("source", prep["supplied_players"]["object"]["ref"], "constructor_metadata", "actual empty List selects NULL Player; native common writer records that selected value")
        a = self.info(prep["info"], self.n["prepared_info"], preparation=True)
        b = self.info(prep["readback_info"], self.n["preparation_readback_info"], preparation=True)
        original_p = self.metadata(self.blob(prep["level_dat"]), a, p_interval)
        native_p = self.metadata(self.boundaries[0]["native_files"][LEVEL], native_info_values(self.n["prepared_info"]), self.n["preparation_epoch_ms"])
        equal(b["last_played"], original_p["last_played"], "actual original metadata readback clock")
        equal(self.n["preparation_readback_info"]["last_played"], native_p["last_played"], "actual native metadata readback clock")
        self.ledger.clock("prepared_metadata.LastPlayed", original_p["last_played"], native_p["last_played"])
        prep_session = self.blob(prep["session_lock"])
        equal(len(prep_session), 8, "actual preparation session token")
        integer(int.from_bytes(prep_session, "big", signed=True), 0, epoch(prep["operations"][0]["epoch_ms"])[0], "original preparation token precedes its first API sample")
        integer(signed(self.n["preparation_session_token"]), *epoch(self.n["preparation_epoch_ms"]), "native preparation session token in actual prepare interval")
        for side in ("original", "native"):
            files = [r[side + "_files"] for r in self.boundaries]
            dirs = [r[side + "_dirs"] for r in self.boundaries]
            intervals = [row["epoch_ms"] for row in (o_ops if side == "original" else n_ops)]
            token = self.boundaries[0]["source_token"] if side == "original" else self.n["live_session_token"]
            integer(signed(token), *epoch((self.o if side == "original" else self.n)["construction_epoch_ms"]), "actual live session constructor input")
            equal(files[0][SESSION], raw_hex(token, 8), "actual live token matches session file")
            initial_level = self.blob(prep["level_dat"]) if side == "original" else files[0][LEVEL]
            for i, current in enumerate(files):
                expected_files = {LEVEL, SESSION} | ({REGION} if i else set()) | ({OLD} if i >= 7 else set())
                equal(set(current), expected_files, "complete actual " + side + " disk names at boundary " + str(i))
                equal(dirs[i], [".", "world", "world/data"] + (["world/region"] if i else []), "complete actual directory inventory")
                equal(current[SESSION], files[0][SESSION], "actual session bytes never changed")
                if i < 7:
                    equal(current[LEVEL], initial_level, "no metadata write before full World save")
                    if i: equal(current[REGION], bytes(8192), "actual untouched acquisition-created region")
                if i >= 8:
                    equal((current[LEVEL], current[OLD]), (files[8][LEVEL], files[8][OLD]), "no later metadata write on far/drop/maintain/reload")
                if 7 <= i < 15: equal(current[REGION], files[7][REGION], "whole region bytes preserved before genuine maintenance")
                if i >= 15: equal(current, files[15], "reload/repeated hit preserve complete disk bytes")
            for op in (6, 7):
                equal(files[op + 1][OLD], files[op][LEVEL], "exact metadata byte rotation")
            equal(files[7][REGION], files[8][REGION], "clean second full save preserves all region bytes")
            first_intervals = {slot: intervals[6] for slot in (0, 1, 32, 33)}
            first = self.region(files[7][REGION], first_intervals)
            final = self.region(files[-1][REGION], {**first_intervals, 16: intervals[14]})
            for slot in first: equal(final[slot], first[slot], "later operations preserve each whole saved origin record")
            for index in (6, 7):
                before = self.boundaries[index]["original_info"] if side == "original" else native_info_values(self.boundaries[index]["native"]["info"])
                metadata = self.metadata(files[index + 1][LEVEL], before, intervals[index])
                self.boundaries[index + 1][side + "_metadata"] = metadata
            # The original graph supplies all expected typed data. The native
            # boundary comparison already proved those SAME presave owners.
            presave = self.boundaries[6]
            for slot, row in first.items():
                ref = presave["source_map"][packed(slot & 31, slot >> 5)]
                self.saved_chunk(presave["graph"], ref, row["level"])
            self.saved_chunk(self.boundaries[14]["graph"], o_ops[9]["result"], final[16]["level"])
            self.disk_reports.append({"side": side, "first_region": first, "final_region": final})
        for index, role in ((0, "prepared_metadata.LastPlayed"), (7, "full_save_0.LastPlayed"), (8, "full_save_1.LastPlayed")):
            left = original_p if index == 0 else self.boundaries[index]["original_metadata"]
            right = native_p if index == 0 else self.boundaries[index]["native_metadata"]
            self.ledger.clock(role, left["last_played"], right["last_played"])
            a_tree, b_tree = copy.deepcopy(left["tree"]), copy.deepcopy(right["tree"])
            # Replace only the already type/range/join-validated sample with its
            # explicitly named input role; every other tag stays exact.
            for tree in (a_tree, b_tree):
                data = compound(tree, "", {"Data"})["Data"]
                for child in data["value"]:
                    if child["name_utf16"] == u16("LastPlayed"): child["value"] = ["measured_clock", role]
            equal(a_tree, b_tree, "all metadata types/names/values except the separately bound clock input")
        for side in ("source", "native"):
            self.ledger.exclude(side, "disk.NBT", "nbt_compound_order", "complete decoded trees and raw file fingerprints retained; no other NBT field masking")
        for phase in ("first_region", "final_region"):
            left, right = (row[phase] for row in self.disk_reports)
            equal(set(left), set(right), "all corresponding region slots")
            for slot in left:
                for key in ("tree", "sector", "sectors", "compression"):
                    equal(left[slot][key], right[slot][key], "complete corresponding region " + key)
        equal(self.disk(self.o["disk_after_close"], True), (self.boundaries[-1]["original_files"], self.boundaries[-1]["original_dirs"]), "original close preserves all bytes")
        equal(self.disk(self.n["disk_after_close"], False), (self.boundaries[-1]["native_files"], self.boundaries[-1]["native_dirs"]), "native close preserves all bytes")

    def boundary(self, state, n, ordinal):
        keys(n, ("schema", "scope", "ordinal", "base_world", "dedicated", "server", "cache", "normal", "dimension",
            "manager", "biomes", "retained_chunks", "lights", "scheduled", "info", "saved_data", "shared",
            "world_random", "profile", "disk", "correspondences", "native_private"), "complete native boundary")
        equal((integer(n["schema"], 2, 2), n["scope"], integer(n["ordinal"], 0, 17)),
            (2, "native-dedicated-game-domain-state", ordinal), "actual complete boundary ordinal/profile")
        g = Graph(self, state)
        world = self.world_server(g, n, ordinal)
        source_cache, fallback, loader, source_map = self.cache(g, world, n)
        self.dimension_manager(g, world, n)
        self.biomes(g, n["biomes"], ordinal)
        chunk_view = n["retained_chunks"]
        keys(chunk_view, ("count", "old_far", "reloaded_far", "objects"), "complete retained native Chunk roots")
        refs = state["graph"]["retained_chunks"]
        need(type(refs) is list and len({v["ref"] for v in refs}) == len(refs), "distinct retained source Chunk owners")
        equal(integer(chunk_view["count"], 1, 33), len(refs), "every retained original/native Chunk")
        need(type(chunk_view["objects"]) is list and len(chunk_view["objects"]) == len(refs), "all native retained Chunk descriptions")
        self.ledger.bind(state["old_far"], chunk_view["old_far"], "mc_chunk_state", "retained old far root")
        self.ledger.bind(state["reloaded_far"], chunk_view["reloaded_far"], "mc_chunk_state", "retained reloaded root")
        equal(state["old_far"], self.o["operations"][9]["result"] if ordinal >= 10 else None, "exact old-far root lifetime")
        equal(state["reloaded_far"], self.o["operations"][15]["result"] if ordinal >= 16 else None, "exact reloaded root lifetime")
        actual_chunks = {}
        for row in chunk_view["objects"]:
            identity = self.ledger.native(row["owner"], "mc_chunk_state")
            need(identity not in actual_chunks, "unique native retained Chunk owner")
            actual_chunks[identity] = row
        for ref in refs:
            need(ref["ref"] in self.ledger.forward, "retained source owner tied to actual cache/operation/root")
            identity = self.ledger.forward[ref["ref"]]
            need(identity in actual_chunks, "complete retained native Chunk owner")
            self.chunk(g, ref, actual_chunks[identity], world, ref == fallback)
        equal(set(actual_chunks), {self.ledger.forward[r["ref"]] for r in refs}, "no unknown/drop/deduplicated retained Chunk")
        self.ledger.exclude("source", world["ref"], "observer_roots", "all retained identities compared without requiring helper discovery order")
        self.lights_scheduled(g, world, n)
        info = self.info(state["info"], n["info"], g)
        token = self.saved_paths(g, world, loader, n)
        shared, expected_shared = n["shared"], state["graph"]["shared"]
        keys(shared, ("world_enqueue_depth", "world_drain_depth", "chunk_static_a", "falling"), "selected native shared state")
        equal(integer(shared["world_enqueue_depth"]), integer(expected_shared["enqueue_depth"]), "same actual shared enqueue counter")
        equal(integer(shared["world_drain_depth"]), g.field(world, "dj", "M"), "same actual drain field")
        equal(boolean(shared["chunk_static_a"]), boolean(expected_shared["chunk_static_a"]), "same actual Chunk static")
        keys(shared["falling"], ("observation", "value"), "falling observation scope")
        if ordinal == 0:
            equal(shared["falling"], {"observation": "unobserved_before_first_acquire", "value": None}, "no initial active static use")
            equal(expected_shared["selected_static_scope"], "not read before first observed Chunk acquisition; avoid observer class initialization", "original initial static scope")
        else:
            boolean(expected_shared["falling"])
            equal(shared["falling"], {"observation": "read_after_acquire", "value": expected_shared["falling"]}, "same actual reached falling mode")
            boolean(shared["falling"]["value"])
            equal(expected_shared["selected_static_scope"], "read after actual Chunk acquisition initialized Block/Biome owners", "original reached static scope")
        actual_world_random = self.ordinary_random(g, g.field(world, "dj", "r"), n["world_random"])
        equal(n["world_random"]["owner"], n["base_world"]["fields"]["r"], "same actual World.r cell")
        self.profile(n, ordinal)
        self.native_private(n, ordinal)
        equal(n["correspondences"], CORRESPONDENCES, "exact reviewed representation declarations")
        o_files, o_dirs = self.disk(state["disk"], True)
        n_files, n_dirs = self.disk(n["disk"], False)
        equal(set(o_files), set(n_files), "complete cross-run file set")
        equal(o_dirs, n_dirs, "complete cross-run directory set")
        initial_token = self.boundaries[0]["source_token"] if self.boundaries else token
        equal(token, initial_token, "actual source Store token retained at every boundary")
        need(SESSION in o_files and SESSION in n_files, "actual session file at every boundary")
        equal(o_files[SESSION], raw_hex(token, 8), "actual source Store token joins boundary session bytes")
        native_token = n["native_private"]["store_session_token"]
        equal(native_token, self.n["live_session_token"], "actual native Store token retained at every boundary")
        equal(n_files[SESSION], raw_hex(native_token, 8), "actual native Store token joins boundary session bytes")
        g.finish()
        self.boundaries.append({"graph": g, "native": n, "source_map": source_map, "original_info": info,
            "source_token": token, "original_files": o_files, "native_files": n_files,
            "original_dirs": o_dirs, "native_dirs": n_dirs, "world_random": actual_world_random})

    def run(self):
        o, n = self.o, self.n
        attribution = ("custom_observer_uncommitted",) if self.legacy_original else ("mechanical_capture_only", "source_context")
        keys(o, ("schema", "scope", "profile", "side", "seed_hex", "experiment", "qualified", "native_comparison",
            "tested_commit", *attribution, "passed", "jar_sha256", "metadata_preparation", "world_is_new",
            "construction_epoch_ms", "threads_before", "initial", "final", "threads_after_operations", "class_origins",
            "bindings", "field_bindings", "construction_trace", "source_helpers", "limits", "projection_scope",
            "snapshot_compatibility", "storage_preparation", "failure", "region_close_completed", "disk_after_close",
            "threads_after_close", "operations", "states", "state_encoding", "blobs", "blob_encoding", "metadata_preparation_calls"), "complete original wrapper")
        keys(n, ("schema", "scope", "profile", "side", "seed_hex", "construction_epoch_ms", "preparation_epoch_ms",
            "preparation_session_token", "live_session_token", "prepared_info", "preparation_readback_info", "initial",
            "operations", "reload_transport_guard", "constructor_inputs", "region_close_completed", "disk_after_close",
            "failure", "passed"), "complete native wrapper")
        source_schema = 1 if self.legacy_original else 2
        source_scope = "original-only-dedicated-natural-population-unload" + ("-prototype" if self.legacy_original else "")
        equal(tuple(o[k] for k in ("schema", "scope", "profile", "side", "seed_hex")),
            (source_schema, source_scope, PROFILE, "server", SEED), "requested original profile")
        equal(tuple(n[k] for k in ("schema", "scope", "profile", "side", "seed_hex")),
            (2, "native-game-dedicated-natural-population-unload", PROFILE, "server", SEED), "requested native profile")
        for document in (o, n):
            expected_schema = source_schema if document is o else 2
            integer(document["schema"], expected_schema, expected_schema, "wrapper schema")
            equal(boolean(document["passed"]), True, "actual producer passed")
            equal(document["failure"], None, "no hidden producer failure")
            equal(boolean(document["region_close_completed"]), True, "actual region owners closed")
            epoch(document["construction_epoch_ms"])
        equal(boolean(o["world_is_new"]), False, "actual loaded seed23 World route")
        equal(o["jar_sha256"], public_support().HASHES["server"], "pinned unchanged original archive")
        equal(o["tested_commit"], None, "honest original experimental source attribution")
        if self.legacy_original:
            equal(boolean(o["custom_observer_uncommitted"]), True, "retained legacy uncommitted observer attribution")
        else:
            equal(boolean(o["mechanical_capture_only"]), True, "mechanical original capture is not semantic qualification")
            equal(o["source_context"], "outer-capture-report", "original source attribution belongs to its capture receipt")
        equal(tuple(boolean(o[k]) for k in ("experiment", "qualified", "native_comparison")), (True, False, False), "original-only input is not a prior native result")
        epoch(n["preparation_epoch_ms"])
        equal(o["source_helpers"], ["ChunkLifecycleObservation", "ChunkObservation", "CacheCallbacksObservation", "NaturalPopulationObservation"], "frozen original helper dependencies")
        equal(o["limits"], {"output_bytes": MAX, "state_bytes": 8 * 1024 * 1024, "objects_per_state": 8192,
            "array_elements": 32768, "collection_entries": 32768, "disk_entries": 256,
            "file_bytes": 4 * 1024 * 1024, "snapshot_file_bytes": 8 * 1024 * 1024,
            "retained_file_bytes": 16 * 1024 * 1024}, "declared original finite bounds")
        for value in o["limits"].values(): integer(value, 0, MAX)
        need(type(o["blobs"]) is dict and 0 < len(o["blobs"]) <= 4096, "complete bounded source blob dictionary")
        for digest, value in o["blobs"].items():
            raw_hex(digest, 32, "blob SHA256")
            keys(value, ("bytes", "hex"), "complete lossless blob")
            raw = raw_hex(value["hex"], integer(value["bytes"], 0, 8 * 1024 * 1024))
            equal(hashlib.sha256(raw).hexdigest(), digest, "independently recomputed source blob")
            self.blobs[digest] = raw
        need(sum(map(len, self.blobs.values())) <= MAX, "complete decoded source blob budget")
        need(type(o["states"]) is list and 0 < len(o["states"]) <= 64, "actual state dictionary")
        for state in o["states"]:
            keys(state, ("dedicated", "graph", "info", "disk", "old_far", "reloaded_far"), "interned complete source state")
        equal(o["state_encoding"], "state-reference-v1; each graph contains actual integer identities and complete reached fields", "frozen original state transport")
        equal(o["blob_encoding"], "SHA256-indexed lossless bytes; primitive arrays use big-endian raw bits", "frozen original byte transport")
        def state_ref(value):
            keys(value, ("state_reference",), "actual source state reference")
            return integer(value["state_reference"], 0, len(o["states"]) - 1, "interned state reference")
        need(type(o["operations"]) is list and type(n["operations"]) is list and len(o["operations"]) == len(n["operations"]) == 17, "complete actual seventeen calls")
        previous = state_ref(o["initial"])
        used = {previous}
        self.boundary(o["states"][previous], n["initial"], 0)
        for index, (left, right) in enumerate(zip(o["operations"], n["operations"])):
            keys(left, ("index", "action", "receiver", "class", "method", "descriptor", "arguments", "before", "epoch_ms", "result", "failure", "after"), "complete original call")
            keys(right, ("index", "action", "epoch_ms", "native_domain", "native_status", "result", "error", "after"), "complete native call")
            equal((integer(left["index"], 0, 16), integer(right["index"], 0, 16), left["action"], right["action"]), (index, index, ACTIONS[index], ACTIONS[index]), "exact call/action sequence")
            equal(state_ref(left["before"]), previous, "all original before states preserve full temporal continuity")
            before = self.boundaries[-1]
            expected_class = "dj" if index in (5, 6, 7) else "he"
            method = "a" if index in (5, 6, 7, 14) else "d" if index in (8, 13) else "c"
            desc = "(J)V" if index == 5 else "(ZLpj;)V" if index in (6, 7) else "()Z" if index == 14 else "(II)V" if index in (8, 13) else "(II)Lhi;"
            args = [{"long": "00000000000004d2"}] if index == 5 else [True, None] if index in (6, 7) else [] if index == 14 else list(COORDS[index])
            equal((left["class"], left["method"], left["descriptor"], left["arguments"]), (expected_class, method, desc, args), "actual unchanged source method/input schedule")
            receiver = before["graph"].state["graph"]["world"] if expected_class == "dj" else before["graph"].field(before["graph"].state["graph"]["world"], "dj", "v")
            equal(left["receiver"], receiver, "actual held original receiver")
            domain = "info" if index == 5 else "lifecycle" if index in (6, 7, 14) else "acquisition"
            equal((right["native_domain"], integer(right["native_status"], 0, 100), right["error"], left["failure"]), (domain, 0, None, None), "actual calls succeeded in their native domain")
            epoch(left["epoch_ms"]); epoch(right["epoch_ms"])
            for document, row in ((o, left), (n, right)):
                preceding = document["construction_epoch_ms"] if index == 0 else document["operations"][index - 1]["epoch_ms"]
                need(epoch(preceding)[1] <= epoch(row["epoch_ms"])[0], "actual sequential per-run clock intervals")
            if method == "c": self.ledger.bind(left["result"], right["result"], "mc_chunk_state", "actual returned Chunk owner")
            elif index == 14:
                equal(boolean(left["result"]), boolean(right["result"]), "actual maintenance Boolean")
            else: equal((left["result"], right["result"]), (None, None), "actual void return")
            previous = state_ref(left["after"]); used.add(previous)
            self.boundary(o["states"][previous], right["after"], index + 1)
            if index in (4, 8, 16):
                equal(left["before"], left["after"], "source repeated/protected no-op preserves complete state")
                a, b = copy.deepcopy(before["native"]), copy.deepcopy(right["after"])
                a.pop("ordinal"); b.pop("ordinal")
                equal(a, b, "native repeated/protected no-op preserves complete state including private owners")
        equal(state_ref(o["final"]), previous, "actual original final state continuity")
        equal(used, set(range(len(o["states"]))), "all interned original states reached")
        self.persistence()
        equal(set(self.blobs), self.used_blobs, "all original dictionary bytes explained by full state/preparation/disk")
        guard = n["reload_transport_guard"]
        keys(guard, ("identity_map", "complete", "saved_block_bytes_checked"), "actual finite native Registry guard")
        equal((boolean(guard["identity_map"]), boolean(guard["complete"]), integer(guard["saved_block_bytes_checked"], 0, 1 << 30)), (True, True, 5 * 32768), "actual guard traversed all five real saved block arrays")
        keys(n["constructor_inputs"], ("bound", "count", "consumed", "failure"), "terminal actual tape assertion")
        boolean(n["constructor_inputs"]["bound"])
        for key in ("count", "consumed", "failure"): integer(n["constructor_inputs"][key], 0, 65536)
        equal(n["constructor_inputs"], {"bound": True, "count": 9, "consumed": 9, "failure": 0}, "all and only genuine constructor inputs consumed")
        old, new = o["operations"][9]["result"], o["operations"][15]["result"]
        need(old != new, "actual reload creates a distinct Chunk")
        equal(o["operations"][16]["result"], new, "actual repeated hit is reloaded owner")
        final = self.boundaries[-1]
        equal((final["graph"].state["old_far"], final["graph"].state["reloaded_far"]), (old, new), "old/reloaded roots retained through final boundary")
        old_arrays, new_arrays = {}, {}
        for field in "befgh":
            for ref, out in ((old, old_arrays), (new, new_arrays)):
                array = final["graph"].field(ref, "hi", field)
                if field in "efg": array = final["graph"].field(array, "ob", "a")
                out[field] = (array["ref"], final["graph"].array(array, "B"))
        equal({k: v[1] for k, v in old_arrays.items()}, {k: v[1] for k, v in new_arrays.items()}, "all five actual saved/reloaded arrays equal")
        need(not {v[0] for v in old_arrays.values()} & {v[0] for v in new_arrays.values()}, "all five actual decoded arrays have fresh owners")
        equal(tuple(final["graph"].field(old, "hi", k) for k in ("c", "o", "i")), (False, True, 64), "retained old owner preserves source unload state")
        equal(tuple(final["graph"].field(new, "hi", k) for k in ("c", "o", "i")), (True, False, 0), "fresh loaded owner preserves source decoder minimum/dirty state")
        self.ledger.exclude("source", "original_wrapper", "constructor_metadata", "source/class/thread/reflection provenance is audited separately; this comparator consumes actual game graph, inputs and effects")
        for field in ("threads_before", "threads_after_operations", "threads_after_close", "class_origins", "bindings",
            "field_bindings", "construction_trace", "source_helpers", "limits", "projection_scope", "snapshot_compatibility", "storage_preparation"):
            raw = json.dumps(o[field], sort_keys=True, ensure_ascii=True, allow_nan=False, separators=(",", ":")).encode("ascii")
            self.ledger.exclude("source", "wrapper." + field, "constructor_metadata", "separate original audit prerequisite; retained value fingerprint " + json.dumps(fingerprint(raw), sort_keys=True))
        covered = set(self.ledger.reverse) | set(self.payload_reverse) | set(self.jdi_aliases.values())
        covered |= {str(row["owner"]) for row in self.ledger.exclusions.values() if row["side"] == "native"}
        equal(set(self.ledger.native_classes) - covered, set(), "every native typed owner is compared or explicitly accounted")
        return self.report()

    def report(self):
        compression = []
        for phase in ("first_region", "final_region"):
            left, right = (row[phase] for row in self.disk_reports)
            for slot in left:
                compression.append({"phase": phase, "slot": slot,
                    "original": fingerprint(left[slot]["compressed"]),
                    "native": fingerprint(right[slot]["compressed"]),
                    "raw_compressed_equal": left[slot]["compressed"] == right[slot]["compressed"],
                    "all_typed_nbt_equal": True})
        return {"passed": True, "scope": "finite-dedicated-game-domain-correspondence",
            "profile": PROFILE, "side": "server", "seed_hex": SEED,
            "native_comparison": True, "qualification_complete": False, "tested_commit": None,
            "original_input_contract": {"schema": self.o["schema"], "legacy_original_adapter": self.legacy_original,
                "tested_commit": self.o["tested_commit"],
                **({"custom_observer_uncommitted": self.o["custom_observer_uncommitted"], "source_dirty": True}
                   if self.legacy_original else {"mechanical_capture_only": self.o["mechanical_capture_only"],
                                               "source_context": self.o["source_context"]})},
            "actual_store_session_token_boundaries": {"original": len(self.boundaries), "native": len(self.boundaries)},
            "independent_original_and_jdi_audits_required": True,
            "jdi_trace_scope": "constructor input/U and complete boundary Random/Math records checked here; caller/thread/stack/nested-draw semantics require the separate pinned original audit",
            "source_and_process_attribution": "provided by the separate root qualification journal; no processes run by this module",
            "boundaries": 18, "calls": 17, "original_interned_states": len(self.o["states"]),
            "counts": dict(self.ledger.counts), "logical_alias_pairs": len(self.ledger.forward),
            "actual_original_owner_count": len(self.ledger.source_classes),
            "actual_native_typed_owner_count": len(self.ledger.native_classes),
            "actual_native_relations": [{"role": role, "parent": parent, "child": child}
                for (role, parent), child in sorted(self.native_links.items())],
            "jdi_role_aliases": [{"role": role, "original_jdi_owner": owner, "native": native}
                for (role, owner), native in sorted(self.jdi_aliases.items())],
            "primitive_payload_aliases": [{"source": source, "native_storage": native}
                for source, native in sorted(self.payload_forward.items())],
            "source_native_aliases": [{"source": key, "native": value,
                "source_class": self.ledger.source_classes[key], "native_kind": self.ledger.native_classes[value]}
                for key, value in sorted(self.ledger.forward.items())],
            "representation_rules": EXCLUSIONS,
            "actual_representation_exclusions": sorted(self.ledger.exclusions.values(), key=lambda row: (row["side"], str(row["owner"]), row["rule"])),
            "measured_clock_correspondences": {name: {"original": pair[0], "native": pair[1]}
                for name, pair in sorted(self.ledger.clock_values.items())},
            "actual_path_bindings": {"original": self.original_root, "native": self.native_root},
            "original_preparation_session_clock": "actual observed 8-byte token is nonnegative and precedes first preparation API; no unrecorded lower-bound sample is invented",
            "compression": compression,
            "metadata_compression_scope": "all gzip streams fully decoded/validated; actual LastPlayed is joined to its own clock interval and raw files are retained, so cross-run compressed bytes are not declared equal",
            "complete_disk_boundaries": [{"ordinal": i, "original": {k: fingerprint(v) for k, v in row["original_files"].items()},
                "native": {k: fingerprint(v) for k, v in row["native_files"].items()}} for i, row in enumerate(self.boundaries)],
            "native_draw_callsites_observed": False,
            "native_operation_arguments": "fixed producer schedule/source audit; outputs export action/result/state rather than raw arguments",
            "jvm_internal_heap_identity": False, "general_compression_equivalence": False,
            "whole_population_equivalence": False, "server_startup_or_general_restart": False,
            "limits": {"json_bytes_each": MAX, "trace_bytes": 16 * 1024 * 1024, "states": 18, "source_objects_per_state": 8192,
                "native_refs": 8192, "disk_entries": 256, "disk_bytes_per_boundary": 8 * 1024 * 1024}}


CORRESPONDENCES = {
    "scope": "finite_game_fields_arrays_and_aliases", "references": "actual_C_pointer_plus_native_kind",
    "random": "semantic_owner_and_state_inline_not_AtomicLong_heap_identity",
    "chunk": "mc_chunk_state_semantic_owner_with_native_header_relation",
    "fallback": "embedded_state_with_actual_arrays_no_normal_child_handles",
    "provider": "actual_outer_cache_and_embedded_numerical_generator",
    "cave": "range_and_random_split_fields_no_separate_C_owner",
    "noise_groups_grid_rows": "actual_C_spans_and_subobjects_not_Java_array_objects",
    "permutations": "actual_uint16_cells_numeric_values_not_Java_int_storage",
    "storage": "actual_native_path_binding_with_declared_Store_correspondence",
    "membership_order": "native_bucket_order_only",
    "initial_shared": "no_Block_or_Biome_active_use_from_observer",
    "math_profile": "passive_native_actual_state_requires_separate_constructor_input_evidence"}


def u16(name):
    return name.encode("utf-16-be", "surrogatepass").hex()


def tag(kind, name, value):
    return {"type": kind, "name_utf16": u16(name), "value": value}


def compound(value, name, names):
    keys(value, ("type", "name_utf16", "value"), "complete typed Compound")
    equal((value["type"], value["name_utf16"]), (10, u16(name)), "actual Compound type/name")
    need(type(value["value"]) is list, "all Compound children")
    result = {}
    for child in value["value"]:
        keys(child, ("type", "name_utf16", "value"), "all typed children")
        decoded = utf16(child["name_utf16"])
        need(decoded not in result, "no duplicate Compound names")
        result[decoded] = child
    equal(set(result), set(names), "complete exact Compound names")
    return result


def parse_nbt(raw):
    return public_support().canonical_nbt(public_support().STORAGE.nbt(raw))


def decompress(raw, gzip, limit):
    need(type(raw) is bytes and 0 < len(raw) <= 4 * 1024 * 1024, "bounded compressed input")
    stream = zlib.decompressobj(31 if gzip else 15)
    plain = stream.decompress(raw, limit + 1)
    need(stream.eof and not stream.unused_data and not stream.unconsumed_tail and 0 < len(plain) <= limit,
         "one complete bounded compressed stream")
    return plain


def packed(x, z):
    result = (0x80000000 if x < 0 else 0) | ((x & 32767) << 16) | (32768 if z < 0 else 0) | (z & 32767)
    return result - (1 << 32) if result >= 1 << 31 else result


def native_info_values(value):
    result = {k: value[k] for k in INFO}
    result["level_name"] = value["level_name"]["utf16"]
    return result


def parse_tape(raw):
    need(type(raw) is bytes and len(raw) <= 65536 and raw.startswith(b"minecraft-c-random-constructor-inputs-v1\n"), "input-only constructor tape header")
    lines = raw.decode("ascii", "strict").splitlines(keepends=True)
    need(len(lines) == 10 and all(row.endswith("\n") and not row.endswith("\r\n") for row in lines), "exact nine-row canonical tape")
    previous, result = 8682522807148012, []
    for row in lines[1:]:
        need(re.fullmatch(r"[0-9a-f]{16}\t(?:0|[1-9][0-9]*|-[1-9][0-9]*)\n", row) is not None, "exact constructor-input row")
        u, sample = row.rstrip("\n").split("\t")
        previous = (previous * 1181783497276652981) & MASK
        equal(word(u), previous, "full global U progression")
        sample = integer(int(sample), -(1 << 63), (1 << 63) - 1, "signed native nanoTime input")
        result.append({"committed_uniquifier": u, "nanotime": f"{sample & MASK:016x}"})
    return result


def parse_trace(raw, tape_raw, *, legacy=False):
    boolean(legacy, "explicit legacy trace adapter")
    need(type(raw) is bytes and 0 < len(raw) <= 16 * 1024 * 1024 and raw.endswith(b"\n"), "complete bounded passive JDI trace")
    tape = parse_tape(tape_raw)
    rows = [line.split("\t") for line in raw.decode("ascii", "strict").splitlines()]
    prefixes = {"binding", "boundary", "breakpoint", "caller", "constructor", "count", "draw", "draw_argument", "draw_scope", "frame", "identity", "life", "math", "meta", "result", "stack", "state", "uniquifier"}
    need(len(rows) <= 25000 and all(row and row[0] in prefixes and all(len(v) <= 8192 for v in row) for row in rows), "reviewed JDI record families and field bounds")
    protocol = "dedicated-random-observation-1" if legacy else "dedicated-random-observation-2"
    equal(rows[0], ["meta", "protocol", protocol], "selected passive observer protocol")
    equal([row for row in rows if row[:2] == ["meta", "protocol"]], [rows[0]], "one actual trace protocol declaration")
    for name, value in (("tested_commit", "null"), ("native_comparison", "false")):
        equal([row for row in rows if row[:2] == ["meta", name]], [["meta", name, value]], "actual trace attribution " + name)
    for name, value in (("source_context", "outer-capture-report"), ("mechanical_capture_only", "true")):
        expected = [] if legacy else [["meta", name, value]]
        equal([row for row in rows if row[:2] == ["meta", name]], expected, "selected trace source contract " + name)
    equal(rows[-1], ["result", "complete"], "complete passive observer termination")
    need(sum(row[0] == "result" for row in rows) == 1, "no partial/duplicate trace result")
    states, math, boundaries, constructors = {}, {}, {}, {}
    current_u = "001ed8b55fac9dec"
    active, stage, next_id, unique_owner, boundary_order = None, None, 1, None, []
    for row in rows:
        if row[0] == "constructor":
            need(len(row) >= 4, "constructor trace shape")
            number = integer(int(row[1]), 1, 9, "constructor ordinal")
            role = row[2]
            if role == "entry":
                need(len(row) == 10 and active is None and number == next_id, "complete serial actual constructor entry")
                equal(row[5], current_u, "no hidden shared U advance")
                equal(row[8:], ["ordinary", "java.util.Random"], "actual observed ordinary defaults")
                active, stage = number, "entry"
                constructors[number] = {"owner": row[3], "sample": None}
            else:
                need(active == number, "actual constructor nesting/order")
                if role == "uniquifier_entry":
                    need(len(row) == 4 and stage == "entry", "actual uniquifier entry order")
                    equal(row[3], current_u, "actual entry U"); stage = role
                elif role == "uniquifier_return":
                    need(len(row) == 5 and stage == "uniquifier_entry", "actual uniquifier return order")
                    if unique_owner is None: unique_owner = row[3]
                    equal(row[3], unique_owner, "one retained actual shared AtomicLong")
                    equal(row[4], tape[number - 1]["committed_uniquifier"], "actual committed U is tape input")
                    current_u, stage = row[4], role
                elif role == "nano_time_return":
                    need(len(row) == 4 and stage == "uniquifier_return", "actual nanoTime return order")
                    equal(row[3], tape[number - 1]["nanotime"], "actual native nanoTime return is tape input")
                    constructors[number]["sample"], stage = row[3], role
                elif role == "delegate_entry":
                    need(len(row) == 4 and stage == "nano_time_return", "actual seed delegate order")
                    equal(word(row[3]), word(current_u) ^ word(constructors[number]["sample"]), "actual delegate input derives from observed constructor inputs")
                    stage = role
                elif role == "return":
                    need(len(row) == 5 and stage == "delegate_entry", "actual default return order")
                    equal(row[3:], [constructors[number]["owner"], current_u], "actual constructor owner and unchanged U at return")
                    active, stage, next_id = None, None, next_id + 1
                else:
                    raise ValueError("unreviewed constructor trace role: " + role)
        elif row[0] == "state":
            need(len(row) == 9 and row[2] in ("ordinary", "World_r", "Math"), "exact complete passive Random state record")
            key = (row[1], row[2])
            need(key not in states, "one passive state per marker/role")
            seed = raw_hex(row[5], 8)
            equal(seed[:2], b"\0\0", "actual 48-bit JDI seed encoding")
            need(row[7] in ("0", "1"), "actual JDI Boolean")
            value = {"seed48": row[5][4:], "have_next_next_gaussian": row[7] == "1",
                "next_next_gaussian_bits": row[6], "next_longs": row[8].split(",")}
            random_value(value)
            states[key] = {"owner": row[3], "seed_owner": row[4], "value": value}
        elif row[0] == "boundary":
            need(len(row) == 7 and active is None, "quiescent complete JDI boundary")
            index = integer(int(row[1]), 0, 16)
            need(row[3] in ("before", "after"), "actual operation boundary direction")
            equal(row[2], ACTIONS[index], "actual JDI schedule")
            equal(row[6], current_u, "actual U at boundary")
            marker = "operation_" + str(index) + "_" + row[3]
            need(marker not in boundaries, "unique JDI boundary")
            boundaries[marker] = {"world": row[4], "cache": row[5], "uniquifier": row[6]}
            boundary_order.append((index, row[3]))
        elif row[0] == "math":
            need(len(row) == 8 and row[2] in ("absent", "loaded") and row[3] in ("0", "1") and row[4] in ("0", "1") and row[5] == "0", "complete passive Math holder record")
            need(row[1] not in math, "one Math observation per marker")
            initialized = row[4] == "1"
            need(not initialized or row[2:4] == ["loaded", "1"], "initialized actual Math class")
            need((row[6] != "-" and row[7] != "-") if initialized else row[6:8] == ["-", "-"], "actual nullable Math owners")
            math[row[1]] = {"initialized": initialized, "owner": row[6], "seed_owner": row[7]}
    need(active is None and next_id == 10 and len(constructors) == 9, "all nine constructors fully returned")
    need(len({row["owner"] for row in constructors.values()}) == 9, "nine distinct actual default owners")
    equal(boundary_order, [(i, role) for i in range(17) for role in ("before", "after")], "all actual before/after marker order")
    world_owner, world_seed, math_owner, math_seed = None, None, None, None
    previous = None
    for index in range(17):
        for direction in ("before", "after"):
            marker = "operation_" + str(index) + "_" + direction
            need((marker, "World_r") in states and marker in math, "every boundary has actual Random/Math observation")
            observed = states[(marker, "World_r")]
            if world_owner is None: world_owner, world_seed = observed["owner"], observed["seed_owner"]
            equal((observed["owner"], observed["seed_owner"]), (world_owner, world_seed), "actual retained JDI World.r and cell")
            if direction == "before" and previous is not None:
                equal(observed, previous, "no hidden World.r change between original operations")
            if direction == "after": previous = observed
            if math[marker]["initialized"]:
                need((marker, "Math") in states, "actual initialized Math complete state")
                m = states[(marker, "Math")]
                equal((m["owner"], m["seed_owner"]), (math[marker]["owner"], math[marker]["seed_owner"]), "actual Math class holder owner")
                if math_owner is None: math_owner, math_seed = m["owner"], m["seed_owner"]
                equal((m["owner"], m["seed_owner"]), (math_owner, math_seed), "retained actual Math and cell")
                need(math_owner != world_owner and math_seed != world_seed, "separate actual Math and World Randoms/cells")
    seed_owners = {}
    for number, row in constructors.items():
        expected_seed = ((word(tape[number - 1]["committed_uniquifier"]) ^ word(tape[number - 1]["nanotime"])) ^ 0x5DEECE66D) & ((1 << 48) - 1)
        for suffix in ("delegate_return", "default_return"):
            observed = states[("constructor_" + str(number) + "_" + suffix, "ordinary")]
            equal(observed["owner"], row["owner"], "actual returned constructor owner")
            equal((observed["value"]["seed48"], observed["value"]["have_next_next_gaussian"], observed["value"]["next_next_gaussian_bits"]),
                (f"{expected_seed:012x}", False, "0000000000000000"), "fresh actual constructor fields derive only from tape inputs")
            if number in seed_owners: equal(seed_owners[number], observed["seed_owner"], "same actual seed cell across constructor returns")
            seed_owners[number] = observed["seed_owner"]
    equal(len(set(seed_owners.values())), 9, "distinct actual default Random AtomicLong owners")
    equal(world_owner, constructors[2]["owner"], "actual World.r is the second observed default owner")
    equal(math_owner, constructors[9]["owner"], "actual Math is the ninth observed default owner")
    equal((world_seed, math_seed), (seed_owners[2], seed_owners[9]), "actual retained constructor seed owners")
    for index in range(17):
        equal(math["operation_" + str(index) + "_before"]["initialized"], index > 12, "actual Math first-use placement before call")
        equal(math["operation_" + str(index) + "_after"]["initialized"], index >= 12, "actual Math first-use placement after far population")
    return {"states": states, "math": math, "boundaries": boundaries}


def compare(original, native, trace_bytes, constructor_input_bytes, *, legacy_original=False):
    """Compare current native values; opt in explicitly for untouched schema-1 originals."""
    return Comparison(original, native, trace_bytes, constructor_input_bytes,
                      legacy_original=legacy_original).run()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--constructor-inputs", type=Path, required=True)
    parser.add_argument("--legacy-original", action="store_true", help="Explicitly read an untouched schema-1 original and protocol-1 trace")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    inputs = {"original": bounded_read(args.original), "native": bounded_read(args.native),
        "trace": bounded_read(args.trace, 16 * 1024 * 1024), "constructor_inputs": bounded_read(args.constructor_inputs, 65536)}
    result = compare(strict_json(inputs["original"]), strict_json(inputs["native"]), inputs["trace"], inputs["constructor_inputs"],
                     legacy_original=args.legacy_original)
    result["inputs"] = {name: fingerprint(raw) for name, raw in inputs.items()}
    result["comparator"] = fingerprint(bounded_read(Path(__file__), 2 * 1024 * 1024))
    raw = json.dumps(result, indent=2, ensure_ascii=True, allow_nan=False).encode("utf-8") + b"\n"
    if args.report is not None:
        need(not args.report.exists() and args.report.parent.is_dir(), "new report in an existing owned folder")
        with args.report.open("xb") as stream: stream.write(raw)
    else:
        sys.stdout.buffer.write(raw)


if __name__ == "__main__":
    main()
