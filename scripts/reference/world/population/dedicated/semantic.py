"""Pure original graph, class-file, typed NBT, disk and thread invariants.

No child processes or native expected states are used. The finite dedicated
seed23 profile rejects unsupported payloads rather than normalizing them.
"""
from __future__ import annotations

from collections import Counter, deque
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import stat
import struct
import sys
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[5]
PIN = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"
TITLE = "dedicated-population-reference"
MAX = 64 * 1024 * 1024
HELPERS = {n + ".class" for n in (
    "ChunkObservation", "ChunkObservation$Model", "ChunkObservation$SetupUnavailable",
    "CacheCallbacksObservation", "CacheCallbacksObservation$Observer", "CacheCallbacksObservation$SetupUnavailable",
    "ChunkLifecycleObservation", "ChunkLifecycleObservation$Observer", "ChunkLifecycleObservation$SetupUnavailable",
    "NaturalPopulationObservation", "NaturalPopulationObservation$Observation", "NaturalPopulationObservation$RandomFields",
    "PopulationDedicatedObservation", "PopulationDedicatedObservation$RandomReader", "PopulationDedicatedObservation$Original",
    "PopulationDedicatedObservation$Graph", "PopulationDedicatedObservation$Experiment")}


def source_path(value):
    """Interpret a producer path independently of the verifier's host OS."""
    require(type(value) is str and value and "\0" not in value, "complete producer path")
    windows = PureWindowsPath(value)
    return windows if windows.is_absolute() else PurePosixPath(value)


def source_child(parent, leaf):
    parent = source_path(str(parent))
    require(parent.is_absolute(), "absolute producer storage parent")
    return parent.joinpath(*PurePosixPath(leaf).parts)


def require(value, message):
    if not value:
        raise ValueError(message)


def equal(a, b, message):
    todo = [(a,b)]
    while todo:
        x,y = todo.pop()
        require(type(x) is type(y), message + ": value type")
        if type(x) is dict:
            require(x.keys() == y.keys(), message + ": keys")
            todo.extend((x[k],y[k]) for k in x)
        elif type(x) in (list,tuple):
            require(len(x) == len(y), message + ": count")
            todo.extend(zip(x,y))
        else:
            require(x == y, message)


def keys(value, names, message):
    require(type(value) is dict and set(value) == set(names), message + ": complete keys")


def integer(value, low, high, message):
    require(type(value) is int and low <= value <= high, message + ": integer bounds")
    return value


def raw_hex(value, size, message):
    require(type(value) is str and len(value) == size * 2 and re.fullmatch(r"[0-9a-f]*", value) is not None,
            message + ": complete canonical hex")
    return bytes.fromhex(value)


def fp(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def ordinary(path, directory=False):
    info = path.lstat()
    require(not path.is_symlink() and not getattr(info, "st_file_attributes", 0) & 0x400,
            "ordinary non-reparse path: " + str(path))
    require(stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode), "ordinary path kind")
    return info


def pin(path):
    a = ordinary(path)
    require(0 <= a.st_size <= 512 * 1024 * 1024, "bounded selected file")
    h, count = hashlib.sha256(), 0
    with path.open("rb") as stream:
        while raw := stream.read(1024 * 1024):
            count += len(raw)
            require(count <= 512 * 1024 * 1024, "stable selected file bound")
            h.update(raw)
    b = ordinary(path)
    equal((a.st_size, a.st_mtime_ns, a.st_ino), (b.st_size, b.st_mtime_ns, b.st_ino), "file stable during read")
    equal(count, a.st_size, "complete selected file read")
    return {"bytes": count, "sha256": h.hexdigest()}


def read(path, limit=MAX):
    require(ordinary(path).st_size <= limit, "bounded input: " + str(path))
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    require(len(raw) <= limit, "bounded complete read")
    equal(fp(raw), pin(path), "stable input bytes")
    return raw


def strict_json(raw):
    def pairs(items):
        value = {}
        for k, v in items:
            require(k not in value, "duplicate JSON key: " + k)
            value[k] = v
        return value
    def bad(v):
        raise ValueError("nonfinite JSON constant: " + v)
    require(0 < len(raw) <= MAX, "complete bounded JSON")
    value = json.loads(raw, object_pairs_hook=pairs, parse_constant=bad)
    stack, count = [(value, 0)], 0
    while stack:
        v, depth = stack.pop()
        count += 1
        require(count <= 8_000_000 and depth <= 64, "bounded complete JSON structure")
        if type(v) is dict:
            stack.extend((x, depth + 1) for x in v.values())
        elif type(v) is list:
            stack.extend((x, depth + 1) for x in v)
        else:
            require(v is None or type(v) in (str, bool, int, float), "ordinary JSON value")
    return value


def encoded(value):
    raw = (json.dumps(value, ensure_ascii=True, separators=(",", ":"), allow_nan=False) + "\n").encode("ascii")
    # Java's frozen encoder uses \u00xx for controls rather than Python's
    # shorthand escapes. Even backslash runs represent literal source text.
    controls = {b"b":b"0008", b"f":b"000c", b"n":b"000a", b"r":b"000d", b"t":b"0009"}
    def control(match):
        slashes, letter = match.groups()
        return slashes[:-1] + b"\\u" + controls[letter] if len(slashes) & 1 else match.group(0)
    raw = re.sub(rb'(\\+)([bfnrt])', control, raw)
    require(b"\x7f" not in raw, "captured helper strings contain no unescaped DEL")
    return raw


def epoch(value):
    require(type(value) is list and len(value) == 2, "two complete measured epoch endpoints")
    low, high = [integer(x, 0, (1 << 63) - 1, "epoch") for x in value]
    require(low <= high, "ordered epoch interval")
    return low, high


def inventory(root):
    ordinary(root, True)
    paths = sorted(root.rglob("*"))
    require(len(paths) <= 2048, "bounded retained capture inventory")
    result = {}
    for p in paths:
        if p.is_dir():
            ordinary(p, True)
        else:
            result[p.relative_to(root).as_posix()] = pin(p)
    return result


class Binary:
    def __init__(self, raw):
        self.raw, self.p = raw, 0
    def take(self, n):
        require(type(n) is int and 0 <= n <= len(self.raw) - self.p, "complete binary bytes")
        value = self.raw[self.p:self.p + n]
        self.p += n
        return value
    def number(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))[0]
    def u2(self):
        return self.number(">H")


def mutf(raw):
    units, p = [], 0
    while p < len(raw):
        a = raw[p]
        p += 1
        if 1 <= a <= 127:
            v = a
        elif a & 0xe0 == 0xc0:
            require(p < len(raw) and raw[p] & 0xc0 == 0x80, "MUTF pair")
            v = ((a & 31) << 6) | (raw[p] & 63)
            p += 1
            require(v >= 128 or (a == 0xc0 and v == 0), "canonical MUTF pair")
        else:
            require(a & 0xf0 == 0xe0 and p + 1 < len(raw) and raw[p] & 0xc0 == 0x80 and raw[p + 1] & 0xc0 == 0x80, "MUTF triple")
            v = ((a & 15) << 12) | ((raw[p] & 63) << 6) | (raw[p + 1] & 63)
            p += 2
            require(v >= 2048, "canonical MUTF triple")
        units.append(v)
    return b"".join(struct.pack(">H", x) for x in units).decode("utf-16-be", "surrogatepass")


class Classes:
    """Independent pinned class metadata; no javap/JVM execution."""
    def __init__(self, jar):
        self.archive = zipfile.ZipFile(jar)
        entries = self.archive.infolist()
        require(len(entries) <= 4096 and len({x.filename for x in entries}) == len(entries), "unique bounded original archive")
        self.names = {x.filename[:-6].replace("/", ".") for x in entries if x.filename.endswith(".class")}
        self.cache = {}
    def get(self, name):
        if name not in self.cache:
            require(name in self.names, "class belongs to pinned original")
            member = self.archive.getinfo(name.replace(".", "/") + ".class")
            require(0 < member.file_size <= 2 * 1024 * 1024 and not member.flag_bits & 1, "bounded original class")
            value = self.parse(self.archive.read(member))
            equal(value["name"], name, "actual original binary class name")
            self.cache[name] = value
        return self.cache[name]
    @staticmethod
    def parse(raw):
        r = Binary(raw)
        equal(r.take(4), b"\xca\xfe\xba\xbe", "class magic")
        minor, major, size = r.u2(), r.u2(), r.u2()
        cp, i = [None] * size, 1
        while i < size:
            t = r.number(">B")
            if t == 1:
                v = mutf(r.take(r.u2()))
            elif t in (3, 4):
                v = r.take(4)
            elif t in (5, 6):
                cp[i] = (t, r.take(8))
                i += 2
                continue
            elif t in (7, 8, 16, 19, 20):
                v = r.u2()
            elif t in (9, 10, 11, 12, 17, 18):
                v = (r.u2(), r.u2())
            elif t == 15:
                v = (r.number(">B"), r.u2())
            else:
                raise ValueError("unsupported class constant-pool tag")
            cp[i] = (t, v)
            i += 1
        def utf(i):
            require(0 < i < size and cp[i][0] == 1, "actual UTF8 constant")
            return cp[i][1]
        def cls(i):
            if i == 0:
                return None
            require(cp[i][0] == 7, "actual Class constant")
            return utf(cp[i][1]).replace("/", ".")
        mods, name, parent = r.u2(), cls(r.u2()), cls(r.u2())
        interfaces = [cls(r.u2()) for _ in range(r.u2())]
        def attrs():
            values = {}
            for _ in range(r.u2()):
                n = utf(r.u2())
                require(n not in values, "unique class attribute")
                values[n] = r.take(r.number(">I"))
            return values
        def members(methods):
            values = []
            for _ in range(r.u2()):
                m, n, d = r.u2(), utf(r.u2()), utf(r.u2())
                a = attrs()
                value = {"name": n, "descriptor": d, "modifiers": m}
                if methods and "Code" in a:
                    code = Binary(a["Code"])
                    code.take(4)
                    value["code"] = code.take(code.number(">I"))
                values.append(value)
            require(len({(v["name"], v["descriptor"]) for v in values}) == len(values), "unique class members")
            return values
        fields, methods = members(False), members(True)
        attrs()
        equal(r.p, len(raw), "complete class-file bytes")
        return {"name": name, "super": parent, "interfaces": interfaces, "modifiers": mods,
                "major": major, "minor": minor, "fields": fields, "methods": methods, "fingerprint": fp(raw)}
    def lineage(self, name):
        values = []
        while name in self.names:
            require(name not in values and len(values) <= 64, "bounded acyclic original hierarchy")
            values.append(name)
            name = self.get(name)["super"]
        return values
    def fields(self, name):
        return [{"declaring_class": owner, "field": m["name"], "descriptor": m["descriptor"], "modifiers": m["modifiers"]}
                for owner in self.lineage(name)
                for m in sorted(self.get(owner)["fields"], key=lambda v: v["name"])
                if not m["modifiers"] & 8]
    def member(self, owner, name, descriptor, method=False):
        values = [v for v in self.get(owner)["methods" if method else "fields"] if v["name"] == name and v["descriptor"] == descriptor]
        require(len(values) == 1, "one actual declared original member")
        return values[0]


def load_level():
    spec = importlib.util.spec_from_file_location("dedicated_audit_public_nbt", ROOT / "scripts/reference/storage/compare_level.py")
    require(spec is not None and spec.loader is not None, "public pure NBT support")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decompressed(raw, gzip=True, limit=2 * 1024 * 1024):
    require(0 < len(raw) <= 4 * 1024 * 1024, "complete bounded compressed bytes")
    stream = zlib.decompressobj(31 if gzip else 15)
    value = stream.decompress(raw, limit + 1)
    require(stream.eof and not stream.unused_data and not stream.unconsumed_tail and 0 < len(value) <= limit,
            "one complete bounded compressed member")
    return value


def nbt(raw, level):
    reader = level.Reader(raw)
    tree = reader.tag()
    equal(reader.p, len(raw), "all typed NBT bytes consumed")
    equal(level.encode(tree), raw, "exact typed NBT and canonical MUTF re-encoding")
    return tree


def text(value):
    require(type(value) is str and len(value) <= 32768 and len(value) % 4 == 0 and re.fullmatch(r"[0-9a-f]*", value) is not None,
            "complete bounded UTF16 code units")
    return bytes.fromhex(value).decode("utf-16-be", "surrogatepass")


def signed(value):
    return int.from_bytes(raw_hex(value, 8, "actual long bits"), "big", signed=True)


def random_state(value):
    keys(value, ("seed48", "have_next_next_gaussian", "next_next_gaussian_bits", "next_longs"), "complete actual Random")
    seed = int.from_bytes(raw_hex(value["seed48"], 6, "raw48 seed"), "big")
    require(type(value["have_next_next_gaussian"]) is bool, "actual Gaussian flag")
    raw_hex(value["next_next_gaussian_bits"], 8, "all raw Gaussian bits")
    require(type(value["next_longs"]) is list and len(value["next_longs"]) == 8, "eight complete copied continuations")
    expected = []
    for _ in range(8):
        parts = []
        for _ in range(2):
            seed = (seed * 0x5deece66d + 11) & ((1 << 48) - 1)
            x = seed >> 16
            parts.append(x if x < (1 << 31) else x - (1 << 32))
        expected.append(f"{((parts[0] << 32) + parts[1]) & ((1 << 64) - 1):016x}")
    equal(value["next_longs"], expected, "all eight copied Random.nextLong continuations")


class Observation:
    def __init__(self, document, classes):
        self.d, self.classes = document, classes
        self.identities, self.blobs, self.used_blobs = {}, {}, set()
        self.array_shapes, self.counts = {}, Counter()
        self.class_counts, self.reached, self.literals = Counter(), set(), set()
    def ref(self, value, expected=None, nullable=True):
        if value is None:
            require(nullable, "required actual owner")
            return None
        keys(value, ("ref", "class"), "complete actual owner reference")
        identity = integer(value["ref"], 1, 16384, "global actual owner identity")
        kind = value["class"]
        require(type(kind) is str and 1 <= len(kind) <= 256, "bounded actual owner class")
        if identity in self.identities:
            equal(kind, self.identities[identity], "actual owner class retained globally")
        self.identities[identity] = kind
        if expected is not None:
            equal(kind, expected, "actual owner class")
        return identity
    def blob(self, value):
        keys(value, ("bytes", "sha256"), "complete lossless byte reference")
        length = integer(value["bytes"], 0, 8 * 1024 * 1024, "complete blob length")
        require(type(value["sha256"]) is str and value["sha256"] in self.blobs, "existing SHA-indexed blob")
        raw = self.blobs[value["sha256"]]
        equal(len(raw), length, "blob reference and complete data lengths agree")
        self.used_blobs.add(value["sha256"])
        return raw
    def load_blobs(self):
        require(type(self.d["blobs"]) is dict and 1 <= len(self.d["blobs"]) <= 4096, "complete bounded blob dictionary")
        total = 0
        for digest, value in self.d["blobs"].items():
            require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None, "canonical blob SHA256 key")
            keys(value, ("bytes", "hex"), "complete blob data")
            raw = raw_hex(value["hex"], integer(value["bytes"], 0, 8 * 1024 * 1024, "blob bound"), "all actual blob bytes")
            equal(hashlib.sha256(raw).hexdigest(), digest, "every blob SHA256 independently recomputed")
            total += len(raw)
            require(total <= MAX, "complete unique decoded blob budget")
            self.blobs[digest] = raw
        self.counts["unique_blob_bytes"] = total
    def primitive(self, desc, value):
        if desc == "Z":
            require(type(value) is bool, "actual Boolean cell")
        elif desc in ("B", "S", "C", "I"):
            bounds = {"B": (-128,127), "S": (-32768,32767), "C": (0,65535), "I": (-(1 << 31),(1 << 31)-1)}
            integer(value, *bounds[desc], "actual primitive cell")
        elif desc in ("J", "D", "F"):
            raw_hex(value, 4 if desc == "F" else 8, "actual primitive raw bits")
        else:
            require(desc.startswith(("L", "[")), "supported actual field descriptor")
            self.ref(value)
    def assignable(self, desc, value):
        self.primitive(desc, value)
        if not desc.startswith(("L", "[")) or value is None:
            return
        actual = value["class"]
        if desc.startswith("["):
            equal(actual, desc.replace("/", "."), "captured array has exact declared runtime type")
            return
        expected = desc[1:-1].replace("/", ".")
        if expected == "java.lang.Object" or actual == expected:
            return
        if actual in self.classes.names:
            todo, seen = [actual], set()
            while todo:
                name = todo.pop()
                if name == expected:
                    return
                if name in seen or name not in self.classes.names:
                    continue
                seen.add(name)
                cls = self.classes.get(name)
                todo.extend([cls["super"], *cls["interfaces"]])
            raise ValueError("actual original class does not implement declared reference type")
        allowed = {
            "java.util.Map": {"java.util.HashMap", "java.util.TreeMap"},
            "java.util.List": {"java.util.ArrayList", "java.util.Collections$SynchronizedRandomAccessList"},
            "java.util.Set": {"java.util.HashSet", "java.util.TreeSet"},
            "java.util.Collection": {"java.util.ArrayList", "java.util.HashSet", "java.util.TreeSet", "java.util.Collections$SynchronizedRandomAccessList"},
        }
        require(actual in allowed.get(expected, set()), "bootstrap owner implements actual declared type")
    def graph(self, graph, acquired):
        keys(graph, ("world", "retained_chunks", "shared", "objects"), "complete original instance graph")
        world = self.ref(graph["world"], "dp", False)
        require(type(graph["retained_chunks"]) is list and 1 <= len(graph["retained_chunks"]) <= 128, "complete retained Chunk inventory")
        retained = [self.ref(x, nullable=False) for x in graph["retained_chunks"]]
        require(len(set(retained)) == len(retained), "distinct retained Chunk owners")
        require(all(x["class"] in ("hi", "hg") for x in graph["retained_chunks"]), "ordinary/fallback retained Chunks")
        shared = graph["shared"]
        keys(shared, ("enqueue_depth", "chunk_static_a", "falling", "biome_statics", "selected_static_scope"), "complete selected shared state")
        integer(shared["enqueue_depth"], -(1 << 31), (1 << 31) - 1, "actual enqueue depth")
        require(type(shared["chunk_static_a"]) is bool, "actual Chunk static Boolean")
        roots = [world, *retained]
        if acquired:
            require(type(shared["falling"]) is bool, "actual initialized Falling static")
            keys(shared["biome_statics"], tuple("abcdefghijklm"), "all thirteen actual biome static owners")
            ids = [self.ref(x, nullable=False) for x in shared["biome_statics"].values()]
            require(len(set(ids)) == 13, "distinct initialized biome owners")
            roots.extend(ids)
            equal(shared["selected_static_scope"], "read after actual Chunk acquisition initialized Block/Biome owners", "post-acquisition static scope")
        else:
            equal((shared["falling"], shared["biome_statics"]), (None, None), "observer avoids initial Block/Biome static initialization")
            equal(shared["selected_static_scope"], "not read before first observed Chunk acquisition; avoid observer class initialization", "constructor static scope")
        require(type(graph["objects"]) is list and 1 <= len(graph["objects"]) <= 8192, "bounded complete object descriptions")
        objects, edges = {}, {}
        for value in graph["objects"]:
            require(type(value) is dict and "object" in value, "complete described owner")
            identity = self.ref(value["object"], nullable=False)
            require(identity not in objects, "one state description per owner")
            objects[identity] = value
        for identity, value in objects.items():
            kind, targets = value["object"]["class"], []
            def edge(ref, desc=None):
                self.assignable(desc, ref) if desc is not None else self.ref(ref)
                if ref is not None:
                    require(ref["ref"] in objects, "every reached reference is fully described")
                    equal(objects[ref["ref"]]["object"], ref, "reference matches described owner class")
                    targets.append(ref["ref"])
            if kind == "java.lang.String":
                keys(value, ("object", "utf16"), "complete String")
                text(value["utf16"])
            elif kind in ("java.lang.Boolean", "java.lang.Byte", "java.lang.Short", "java.lang.Integer", "java.lang.Long", "java.lang.Float", "java.lang.Double", "java.lang.Character"):
                keys(value, ("object", "value_type", "value"), "complete boxed source owner")
                desc = {"Boolean":"Z","Byte":"B","Short":"S","Integer":"I","Long":"J","Float":"F","Double":"D","Character":"C"}[kind.rpartition(".")[2]]
                equal(value["value_type"], desc, "boxed actual primitive type")
                self.primitive(desc, value["value"])
            elif kind == "java.util.Random":
                keys(value, ("object", "seed_owner", "state"), "actual Random owner")
                edge(value["seed_owner"], "Ljava/util/concurrent/atomic/AtomicLong;")
                random_state(value["state"])
                equal(objects[value["seed_owner"]["ref"]]["raw_long"], "0000" + value["state"]["seed48"], "AtomicLong agrees with actual Random state")
                self.counts["unique_state_random_records"] += 1
            elif kind == "java.util.concurrent.atomic.AtomicLong":
                keys(value, ("object", "raw_long"), "actual AtomicLong cell")
                raw_hex(value["raw_long"], 8, "all AtomicLong bits")
            elif kind == "java.io.File":
                keys(value, ("object", "path", "absolute"), "actual File path binding")
                edge(value["path"], "Ljava/lang/String;")
                require(type(value["absolute"]) is bool, "actual File absolute flag")
                equal(value["absolute"], source_path(text(objects[value["path"]["ref"]]["utf16"])).is_absolute(), "File path absoluteness")
            elif kind == "java.lang.Class":
                keys(value, ("object", "class_literal"), "actual Class literal")
                require(value["class_literal"] in self.classes.names, "Class literal from original")
                self.literals.add(value["class_literal"])
            elif kind.startswith("["):
                component = kind[1:].replace(".", "/")
                primitive = component in ("Z", "B", "S", "C", "I", "J", "F", "D")
                keys(value, ("object", "component", "length", "big_endian_data" if primitive else "entries"), "complete actual array")
                equal(value["component"], component, "actual array component descriptor")
                length = integer(value["length"], 0, 32768, "actual array length")
                if identity in self.array_shapes:
                    equal((component, length), self.array_shapes[identity], "array type/length retained across states")
                self.array_shapes[identity] = (component, length)
                if primitive:
                    data = self.blob(value["big_endian_data"])
                    equal(len(data), length * {"Z":1,"B":1,"S":2,"C":2,"I":4,"J":8,"F":4,"D":8}[component], "complete primitive array raw payload")
                    if component == "Z":
                        require(set(data) <= {0,1}, "actual Boolean array payload")
                else:
                    require(type(value["entries"]) is list and len(value["entries"]) == length, "all object-array entries")
                    for item in value["entries"]:
                        edge(item, component)
                self.counts["unique_state_array_records"] += 1
            elif kind in ("java.util.HashMap", "java.util.TreeMap"):
                keys(value, ("object", "size", "entries"), "complete Map")
                size = integer(value["size"], 0, 32768, "Map size")
                require(type(value["entries"]) is list and len(value["entries"]) == size, "all Map occurrences")
                ids = []
                for ordinal, item in enumerate(value["entries"]):
                    keys(item, ("ordinal", "key", "value"), "complete Map occurrence")
                    equal(item["ordinal"], ordinal, "actual Map iteration order")
                    edge(item["key"])
                    edge(item["value"])
                    ids.append(None if item["key"] is None else item["key"]["ref"])
                require(len(set(ids)) == size, "distinct Map key owners")
            elif kind in ("java.util.ArrayList", "java.util.HashSet", "java.util.TreeSet", "java.util.Collections$SynchronizedRandomAccessList"):
                keys(value, ("object", "size", "entries"), "complete Collection")
                size = integer(value["size"], 0, 32768, "Collection size")
                require(type(value["entries"]) is list and len(value["entries"]) == size, "all Collection occurrences")
                for item in value["entries"]:
                    edge(item)
                if kind.endswith("Set"):
                    ids = [None if x is None else x["ref"] for x in value["entries"]]
                    require(len(set(ids)) == size, "distinct Set owner identities")
            else:
                require(kind in self.classes.names, "no opaque unsupported bootstrap payload")
                lineage = self.classes.lineage(kind)
                enum = self.classes.get(lineage[-1])["super"] == "java.lang.Enum"
                keys(value, ("object", "fields", *(["enum_name", "enum_ordinal"] if enum else [])), "complete original instance")
                if enum:
                    edge(value["enum_name"], "Ljava/lang/String;")
                    integer(value["enum_ordinal"], 0, 65535, "Enum ordinal")
                require(type(value["fields"]) is list, "all original instance fields")
                actual = [{k:v[k] for k in ("declaring_class", "field", "descriptor", "modifiers")} for v in value["fields"]]
                equal(actual, self.classes.fields(kind), "complete class-file-derived fields/order/modifiers: " + kind)
                for field in value["fields"]:
                    keys(field, ("declaring_class", "field", "descriptor", "modifiers", "value"), "complete field")
                    if field["descriptor"].startswith(("L", "[")):
                        edge(field["value"], field["descriptor"])
                    else:
                        self.primitive(field["descriptor"], field["value"])
                self.reached.update(lineage)
            edges[identity] = targets
            self.class_counts[kind] += 1
        expected, queue, seen = [], deque(), set()
        for root in roots:
            if root not in seen:
                seen.add(root)
                queue.append(root)
        while queue:
            owner = queue.popleft()
            require(owner in objects, "complete root description")
            expected.append(owner)
            for child in edges[owner]:
                if child not in seen:
                    seen.add(child)
                    queue.append(child)
        equal(list(objects), expected, "exact reachable BFS graph, without orphan or missing descriptions")
        return objects
    @staticmethod
    def field(objects, owner, declaring, name):
        identity = owner["ref"] if type(owner) is dict else owner
        values = [v["value"] for v in objects[identity]["fields"] if v["declaring_class"] == declaring and v["field"] == name]
        require(len(values) == 1, "actual field cell: " + declaring + "." + name)
        return values[0]
    def string(self, objects, owner):
        return text(objects[self.ref(owner, "java.lang.String", False)]["utf16"])
    def byte_array(self, objects, owner):
        return self.blob(objects[self.ref(owner, "[B", False)]["big_endian_data"])
    def chunk(self, objects, owner):
        identity = self.ref(owner, nullable=False)
        require(owner["class"] in ("hi", "hg"), "actual Chunk class")
        f = lambda name: self.field(objects, identity, "hi", name)
        arrays, owners = {}, {}
        for role, name, size in (("Blocks","b",32768),("Data","e",16384),("SkyLight","f",16384),("BlockLight","g",16384),("HeightMap","h",256)):
            ref = f(name)
            if name in ("e", "f", "g"):
                ref = self.field(objects, ref, "ob", "a")
            arrays[role] = self.byte_array(objects, ref)
            equal(len(arrays[role]), size, "all Chunk array bytes")
            owners[role] = ref["ref"]
        tiles, slices = objects[f("l")["ref"]], objects[f("m")["ref"]]
        equal(slices["length"], 8, "eight actual Entity slices")
        entities = [x for section in slices["entries"] for x in objects[section["ref"]]["entries"]]
        return {"owner": identity, "coordinates": (f("j"),f("k")), "world": f("d"), "arrays": arrays,
                "array_owners": owners, "flags": {n:f(n) for n in ("c","n","o","p","q")},
                "last_save": f("r"), "minimum": f("i"), "tiles": tiles["entries"], "entities": entities}


ROLES = ("seed","spawn_x","spawn_y","spawn_z","time","last_played","size_on_disk","player",
         "player_dimension","level_name","version","raining","rain_time","thundering","thunder_time")
DESCS = ("J","I","I","I","J","J","J","Liq;","I","Ljava/lang/String;","I","Z","I","Z","I")


def info_record(audit, value, objects=None, preparation=False):
    owner_key = "object" if preparation else "owner"
    keys(value, (owner_key, "fields"), "all fifteen actual Info fields")
    owner = audit.ref(value[owner_key], "ct", False)
    require(type(value["fields"]) is list and len(value["fields"]) == 15, "complete Info field inventory")
    result = {}
    for i, field in enumerate(value["fields"]):
        keys(field, ("field", "descriptor", "value", *([] if preparation else ["role","modifiers"])), "complete Info cell")
        name, desc = chr(ord('a') + i), DESCS[i]
        equal((field["field"], field["descriptor"]), (name, desc), "exact Info declaration and order")
        member = audit.classes.member("ct", name, desc)
        equal(member["modifiers"], 2, "actual private Info declaration")
        if not preparation:
            equal((field["role"], field["modifiers"]), (ROLES[i], 2), "Info role/modifier mapping")
        v = field["value"]
        if desc == "Ljava/lang/String;":
            keys(v, ("object", "utf16"), "actual Info String identity and units")
            audit.ref(v["object"], "java.lang.String", False)
            text(v["utf16"])
            if objects is not None:
                equal(audit.field(objects, owner, "ct", name), v["object"], "Info String cell matches complete graph")
                equal(objects[v["object"]["ref"]]["utf16"], v["utf16"], "all Info String units match actual owner")
            result[ROLES[i]] = v["utf16"]
        else:
            audit.assignable(desc, v)
            if objects is not None:
                equal(audit.field(objects, owner, "ct", name), v, "Info field equals complete actual graph")
            result[ROLES[i]] = v
    return result


def disk_record(audit, value):
    keys(value, ("complete","entries","files","directories","bytes"), "complete original disk inventory")
    require(value["complete"] is True and type(value["entries"]) is list and 1 <= len(value["entries"]) <= 256, "bounded complete disk entries")
    files, dirs, paths, total = {}, [], {}, 0
    for entry in value["entries"]:
        require(type(entry) is dict and entry.get("kind") in ("directory","file"), "ordinary disk entry")
        keys(entry, ("path","kind", *(["data"] if entry["kind"] == "file" else [])), "complete disk entry")
        path = entry["path"]
        require(type(path) is str and path not in paths and len(path) <= 1024 and
                (path == "." or all(re.fullmatch(r"[A-Za-z0-9_.-]+", p) is not None and p not in (".","..") for p in path.split("/"))), "unique ordinary relative disk path")
        require(path == "." or paths.get(path.rpartition("/")[0] or ".") == "directory", "inventoried disk parent")
        paths[path] = entry["kind"]
        if entry["kind"] == "directory":
            dirs.append(path)
        else:
            raw = audit.blob(entry["data"])
            require(len(raw) <= 4 * 1024 * 1024, "complete per-file bound")
            total += len(raw)
            files[path] = raw
    equal(list(paths), sorted(paths), "complete sorted disk inventory")
    equal(value["entries"][0], {"path":".","kind":"directory"}, "inventoried root")
    equal((value["files"],value["directories"],value["bytes"]), (len(files),len(dirs),total), "actual complete disk totals")
    for field in ("files","directories","bytes"):
        integer(value[field], 0, 8 * 1024 * 1024, "disk total")
    require(total <= 8 * 1024 * 1024, "complete snapshot byte budget")
    return files


def metadata(raw, before, interval, level):
    plain = decompressed(raw, limit=65536)
    tree = nbt(plain, level)
    equal((tree["type"],tree["name_utf16"],len(tree["value"])), (10,"",1), "exact metadata root")
    data = level.child(tree, "Data")
    require(data is not None and data["type"] == 10 and len(data["value"]) == 13, "thirteen actual typed metadata fields")
    stamp = level.child(data, "LastPlayed")
    require(stamp is not None and stamp["type"] == 4, "actual typed metadata clock")
    integer(stamp["value"], *epoch(interval), "actual metadata clock lies within its own API interval")
    expected = {
        "RandomSeed":(4,signed(before["seed"])), "SpawnX":(3,before["spawn_x"]),
        "SpawnY":(3,before["spawn_y"]), "SpawnZ":(3,before["spawn_z"]), "Time":(4,signed(before["time"])),
        "SizeOnDisk":(4,signed(before["size_on_disk"])), "LastPlayed":(4,stamp["value"]),
        "LevelName":(8,before["level_name"]), "version":(3,19132), "raining":(1,int(before["raining"])),
        "rainTime":(3,before["rain_time"]), "thundering":(1,int(before["thundering"])), "thunderTime":(3,before["thunder_time"])}
    equal(set(level.children(data)), {level.u16(k) for k in expected}, "all exact metadata tag names without Player")
    for name, (kind,value) in expected.items():
        equal(level.child(data,name), {"type":kind,"name_utf16":level.u16(name),"value":value}, "typed metadata payload: " + name)
    return {"last_played":stamp["value"],"size_on_disk":signed(before["size_on_disk"]),"gzip":fp(raw),"tags":13}


def region(raw, intervals, level):
    require(len(raw) >= 8192 and len(raw) % 4096 == 0, "complete region sector allocation")
    result, allocated = {}, set()
    for slot in range(1024):
        location = struct.unpack_from(">I",raw,slot*4)[0]
        timestamp = struct.unpack_from(">I",raw,4096+slot*4)[0]
        if location == 0:
            equal(timestamp,0,"untouched empty region timestamp")
            continue
        require(slot in intervals,"only source-written region slots")
        low, high = epoch(intervals[slot])
        integer(timestamp,low//1000,high//1000,"source region clock in corresponding save interval")
        start, count = (location >> 8)*4096, location & 255
        require(start >= 8192 and count > 0 and start+count*4096 <= len(raw),"complete occupied region extent")
        sectors = set(range(start//4096,start//4096+count))
        require(not sectors & allocated,"nonoverlapping occupied region sectors")
        allocated.update(sectors)
        length = struct.unpack_from(">I",raw,start)[0]
        require(1 < length <= count*4096-4 and raw[start+4] in (1,2),"complete region compressed record")
        tree = nbt(decompressed(raw[start+5:start+4+length],gzip=raw[start+4]==1),level)
        equal(raw[start+4+length:start+count*4096],bytes(count*4096-4-length),"all newly allocated record padding")
        equal((tree["type"],tree["name_utf16"],len(tree["value"])),(10,"",1),"exact saved Chunk root")
        chunk = level.child(tree,"Level")
        require(chunk is not None and chunk["type"] == 10 and len(chunk["value"]) == 11,"all eleven saved Chunk Level tags")
        result[slot] = {"sector":start//4096,"sectors":count,"compression":raw[start+4],"timestamp":timestamp,"level":chunk}
    equal(allocated,set(range(2,len(raw)//4096)),"complete allocated-sector inventory")
    return result


def saved_chunk(tree, chunk, time, level):
    fields = {"xPos":(3,chunk["coordinates"][0]),"zPos":(3,chunk["coordinates"][1]),
              "LastUpdate":(4,time),"TerrainPopulated":(1,int(chunk["flags"]["n"]))}
    fields.update({name:(7,raw.hex()) for name,raw in chunk["arrays"].items()})
    for name, values in (("Entities",chunk["entities"]),("TileEntities",chunk["tiles"])):
        tag = level.child(tree,name)
        require(tag is not None and tag["type"] == 9,"actual typed saved object list")
        equal(len(tag["value"]["items"]),len(values),"saved object count matches complete live collection")
        require(not values,"nonempty captured Entity/Tile serialization needs an additional concrete codec audit")
        fields[name] = (9,{"element_type":1,"items":[]})
    equal(set(level.children(tree)),{level.u16(k) for k in fields},"complete exact saved Chunk names")
    for name,(kind,value) in fields.items():
        equal(level.child(tree,name),{"type":kind,"name_utf16":level.u16(name),"value":value},"source-bound saved Chunk field: " + name)


def state_details(audit, state, index, construction, owner_refs):
    keys(state,("dedicated","graph","info","disk","old_far","reloaded_far"),"complete dedicated state")
    objects = audit.graph(state["graph"], index > 0)
    d = state["dedicated"]
    keys(d,("world","base_cache","fields"),"five derived fields and actual base cache")
    equal(d["world"],state["graph"]["world"],"same actual dedicated World")
    world = d["world"]
    cache = audit.field(objects,world,"dj","v")
    equal(d["base_cache"],cache,"derived C is actual base v cache")
    expected = [{"field":n,"descriptor":desc,"modifiers":mods,"value":audit.field(objects,world,"dp",n)}
                for n,desc,mods in (("C","Lhe;",1),("D","Z",1),("E","Z",1),("F","Lnet/minecraft/server/MinecraftServer;",2),("G","Lgo;",2))]
    equal(d["fields"],expected,"all five actual dp fields and exact declarations")
    equal(expected[0]["value"],cache,"dp.C equals dj.v")
    equal([expected[1]["value"],expected[2]["value"]],[False,False],"actual unmodified dedicated Boolean fields")
    server = expected[3]["value"]
    audit.ref(server,"net.minecraft.server.MinecraftServer",False)
    equal(server,owner_refs["server"],"actual nonnull constructed server owner")
    for name in ("c","d","e","f","p"):
        equal(audit.field(objects,server,"net.minecraft.server.MinecraftServer",name),None,"startup service remains uninitialized")
    equal(audit.field(objects,world,"dj","w"),owner_refs["storage"],"actual World storage")
    equal(audit.field(objects,cache,"he","h"),world,"actual dedicated cache World")
    equal(audit.field(objects,world,"dj","s"),False,"constructor loaded actual existing metadata")
    equal(audit.field(objects,world,"dp","C"),cache,"retained derived cache cell")
    scheduled = audit.field(objects,world,"dj","E")
    audit.ref(scheduled,"java.util.TreeSet",False)
    membership = audit.field(objects,world,"dj","F")
    audit.ref(membership,"java.util.HashSet",False)
    equal({x["ref"] for x in objects[scheduled["ref"]]["entries"]},
          {x["ref"] for x in objects[membership["ref"]]["entries"]},"actual ordered/membership scheduled owners agree")
    require(scheduled["ref"] != cache["ref"],"base E remains a distinct TreeSet, not derived Boolean E")
    info = info_record(audit,state["info"],objects)
    equal(audit.field(objects,world,"dj","x"),state["info"]["owner"],"actual held Info cell")
    files = disk_record(audit,state["disk"])
    require("world/session.lock" in files and len(files["world/session.lock"]) == 8,"complete live session token")
    token = struct.unpack(">q",files["world/session.lock"])[0]
    integer(token,*epoch(construction),"live session token in actual constructor interval")
    equal(audit.field(objects,owner_refs["storage"],"dq","e"),files["world/session.lock"].hex(),"actual handler token matches session bytes")
    for name,leaf in (("b","world"),("c","world/players"),("d","world/data")):
        file_owner = audit.field(objects,owner_refs["storage"],"dq",name)
        path = audit.string(objects,objects[file_owner["ref"]]["path"])
        observed, expected = source_path(path), source_child(owner_refs["parent"], leaf)
        require(type(observed) is type(expected) and observed.is_absolute(), "same absolute producer storage path flavor")
        equal(observed.parts, expected.parts, "actual handler File path binding")
    fallback = audit.field(objects,cache,"he","c")
    audit.ref(fallback,"hg",False)
    map_ref, list_ref, pending_ref = (audit.field(objects,cache,"he",n) for n in ("f","g","b"))
    audit.ref(map_ref,"java.util.HashMap",False)
    audit.ref(list_ref,"java.util.ArrayList",False)
    audit.ref(pending_ref,"java.util.HashSet",False)
    chunks = {x["ref"]:audit.chunk(objects,x) for x in state["graph"]["retained_chunks"]}
    actual_map = {}
    for row in objects[map_ref["ref"]]["entries"]:
        key = objects[audit.ref(row["key"],"java.lang.Integer",False)]["value"]
        chunk = chunks[audit.ref(row["value"],"hi",False)]
        x,z = chunk["coordinates"]
        packed = ((0x80000000 if x < 0 else 0) | ((x & 32767) << 16) | (32768 if z < 0 else 0) | (z & 32767))
        if packed >= 1 << 31:
            packed -= 1 << 32
        equal(key,packed,"complete integer cache key matches actual Chunk coordinates")
        require((x,z) not in actual_map,"unique actual cache coordinates")
        actual_map[(x,z)] = row["value"]["ref"]
    actual_list = [audit.ref(x,"hi",False) for x in objects[list_ref["ref"]]["entries"]]
    equal(set(actual_list),set(actual_map.values()),"actual cache map/list membership agrees")
    equal(len(actual_list),len(actual_map),"actual cache list has no omitted or duplicate occurrence")
    pending = [objects[audit.ref(x,"java.lang.Integer",False)]["value"] for x in objects[pending_ref["ref"]]["entries"]]
    manager = audit.field(objects,world,"dj","z")
    audit.ref(manager,"es",False)
    equal(audit.field(objects,manager,"es","a"),owner_refs["storage"],"SavedData uses same actual handler")
    id_map = objects[audit.field(objects,manager,"es","d")["ref"]]
    return {"objects":objects,"world":world,"cache":cache,"info":info,"files":files,"chunks":chunks,
            "map":actual_map,"list":actual_list,"pending":pending,"fallback":fallback["ref"],
            "scheduled":objects[scheduled["ref"]]["size"],"ids":id_map["size"],
            "light":objects[audit.field(objects,world,"dj","C")["ref"]]["size"]}


def audit_original(document, jar, parent, level, *, classes=None, jdk_version, legacy=False):
    keys(document,("schema","scope","profile","side","seed_hex","experiment","qualified","native_comparison",
        "tested_commit",*( ["custom_observer_uncommitted"] if legacy else ["mechanical_capture_only","source_context"]),"passed","jar_sha256","metadata_preparation","world_is_new",
        "construction_epoch_ms","threads_before","initial","final","threads_after_operations","class_origins","bindings",
        "field_bindings","construction_trace","source_helpers","limits","projection_scope","snapshot_compatibility",
        "storage_preparation","failure","region_close_completed","disk_after_close","threads_after_close","operations",
        "states","state_encoding","blobs","blob_encoding","metadata_preparation_calls"),"complete dedicated original-only wrapper")
    equal({k:document[k] for k in ("schema","scope","profile","side","seed_hex","experiment","qualified","native_comparison",
        "tested_commit",*( ["custom_observer_uncommitted"] if legacy else ["mechanical_capture_only","source_context"]),"passed","jar_sha256","world_is_new","failure","region_close_completed")},
        {"schema":1 if legacy else 2,"scope":"original-only-dedicated-natural-population-unload-prototype" if legacy else "original-only-dedicated-natural-population-unload","profile":"dedicated-natural-unload",
         "side":"server","seed_hex":"0000000000000017","experiment":True,"qualified":False,"native_comparison":False,
         "tested_commit":None,**({"custom_observer_uncommitted":True} if legacy else {"mechanical_capture_only":True,"source_context":"outer-capture-report"}),"passed":True,"jar_sha256":PIN,"world_is_new":False,
         "failure":None,"region_close_completed":True},"honest finite original-only research attribution")
    equal(document["state_encoding"],"state-reference-v1; each graph contains actual integer identities and complete reached fields","lossless graph encoding")
    equal(document["blob_encoding"],"SHA256-indexed lossless bytes; primitive arrays use big-endian raw bits","lossless byte encoding")
    equal(document["source_helpers"],["ChunkLifecycleObservation","ChunkObservation","CacheCallbacksObservation","NaturalPopulationObservation"],"four unchanged helper selections")
    equal(document["limits"],{"output_bytes":MAX,"state_bytes":8*1024*1024,"objects_per_state":8192,"array_elements":32768,
        "collection_entries":32768,"disk_entries":256,"file_bytes":4*1024*1024,"snapshot_file_bytes":8*1024*1024,
        "retained_file_bytes":16*1024*1024},"complete explicit original observation limits")
    for name in ("projection_scope","snapshot_compatibility","storage_preparation"):
        require(type(document[name]) is str and 40 <= len(document[name]) <= 2048,"explicit bounded research scope")
    classes = Classes(jar) if classes is None else classes
    audit = Observation(document,classes)
    audit.load_blobs()
    # Register references across all retained envelopes, not just graph records.
    todo = [document]
    while todo:
        value = todo.pop()
        if type(value) is dict:
            if set(value) == {"ref","class"}:
                audit.ref(value,nullable=False)
            todo.extend(value.values())
        elif type(value) is list:
            todo.extend(value)
    constructors = document["construction_trace"]
    require(type(constructors) is list and len(constructors) == 5,"all five actual constructor calls")
    expected_ctors = [("ie","(Ljava/io/File;Ljava/lang/String;Z)V"),("ct","(JLjava/lang/String;)V"),
        ("ie","(Ljava/io/File;Ljava/lang/String;Z)V"),("net.minecraft.server.MinecraftServer","()V"),
        ("dp","(Lnet/minecraft/server/MinecraftServer;Lom;Ljava/lang/String;IJ)V")]
    for row,(owner,desc) in zip(constructors,expected_ctors):
        keys(row,("class","descriptor","arguments","result","epoch_ms"),"actual successful constructor event")
        equal((row["class"],row["descriptor"]),(owner,desc),"exact genuine constructor route")
        equal(classes.member(owner,"<init>",desc,True)["modifiers"],1,"actual public original constructor")
        audit.ref(row["result"],owner,False)
        epoch(row["epoch_ms"])
    for row in (constructors[0],constructors[2]):
        require(type(row["arguments"]) is list and len(row["arguments"]) == 3,"actual handler constructor arguments")
        audit.ref(row["arguments"][0],"java.io.File",False)
        equal(row["arguments"][1:], [{"utf16":level.u16("world")},False],"actual handler world directory and flag")
    equal(constructors[1]["arguments"],[{"long":"0000000000000017"},{"utf16":level.u16(TITLE)}],"actual seed/title Info constructor")
    equal(constructors[3]["arguments"],[],"nonnull real MinecraftServer no-argument constructor")
    equal(constructors[4]["arguments"],[constructors[3]["result"],constructors[2]["result"],{"utf16":level.u16(TITLE)},0,{"long":"0000000000000017"}],"actual dp server/handler/title/dimension/seed arguments")
    construction = epoch(document["construction_epoch_ms"])
    require(construction[0] <= constructors[2]["epoch_ms"][0] <= constructors[4]["epoch_ms"][1] <= construction[1],"complete genuine constructor interval")
    prep = document["metadata_preparation"]
    keys(prep,("scope","handler","info","readback_info","supplied_players","operations","level_dat","session_lock"),"complete metadata API preparation")
    equal(prep["scope"],"actual original metadata APIs only; no prepared Chunk or World mutation","explicit API preparation scope")
    equal(prep["handler"],constructors[0]["result"],"actual preparation handler owner")
    keys(prep["supplied_players"],("object","size"),"actual supplied Player list")
    audit.ref(prep["supplied_players"]["object"],"java.util.ArrayList",False)
    equal(prep["supplied_players"]["size"],0,"actual supplied empty Player list")
    pinfo = info_record(audit,prep["info"],preparation=True)
    readback = info_record(audit,prep["readback_info"],preparation=True)
    equal(prep["info"]["object"],constructors[1]["result"],"actual preparation Info object")
    pcalls = prep["operations"]
    equal(document["metadata_preparation_calls"],pcalls,"complete duplicated preparation call records")
    require(type(pcalls) is list and len(pcalls) == 4,"four actual preparation APIs")
    prep_methods = [("actual_info_spawn","ct","a","(III)V",prep["info"]["object"],[0,64,0],None),
        ("actual_handler_metadata_save","ie","a","(Lct;Ljava/util/List;)V",prep["handler"],[prep["info"]["object"],prep["supplied_players"]["object"]],None),
        ("actual_prepared_info_readback","dq","c","()Lct;",prep["handler"],[],prep["readback_info"]["object"]),
        ("actual_preparation_handler_close","ie","e","()V",prep["handler"],[],None)]
    previous = constructors[1]["epoch_ms"][1]
    for row,(action,owner,name,desc,receiver,args,result) in zip(pcalls,prep_methods):
        keys(row,("action","receiver","class","method","descriptor","arguments","result","epoch_ms"),"complete actual preparation call")
        equal({k:row[k] for k in ("action","receiver","class","method","descriptor","arguments","result")},
            {"action":action,"receiver":receiver,"class":owner,"method":name,"descriptor":desc,"arguments":args,"result":result},"exact preparation API arguments and returns")
        low,high = epoch(row["epoch_ms"])
        require(previous <= low <= high <= construction[0],"ordered preparation before new dedicated construction")
        previous = high
    expected_info = {"seed":"0000000000000017","spawn_x":0,"spawn_y":64,"spawn_z":0,"time":"0000000000000000",
        "last_played":"0000000000000000","size_on_disk":"0000000000000000","player":None,"player_dimension":0,
        "level_name":level.u16(TITLE),"version":19132,"raining":False,"rain_time":0,"thundering":False,"thunder_time":0}
    equal(pinfo,expected_info,"all fifteen actual preparation Info fields after metadata API")
    prep_meta = metadata(audit.blob(prep["level_dat"]),pinfo,pcalls[1]["epoch_ms"],level)
    expected_readback = dict(expected_info,last_played=f"{prep_meta['last_played']:016x}")
    equal(readback,expected_readback,"all fifteen actual metadata-readback fields")
    prep_lock = audit.blob(prep["session_lock"])
    equal(len(prep_lock),8,"complete preparation session long")
    integer(struct.unpack(">q",prep_lock)[0],*epoch(constructors[0]["epoch_ms"]),"preparation session in its own handler constructor")
    methods = [("ct","a","(III)V"),("dj","a","(J)V"),("dj","a","(ZLpj;)V"),("dq","c","()Lct;"),
        ("he","a","()Z"),("he","c","(II)Lhi;"),("he","d","(II)V"),("ie","a","(Lct;Ljava/util/List;)V"),("ie","e","()V")]
    expected_bindings = {c+"."+n+d:{"class":c,"method":n,"descriptor":d,"modifiers":1} for c,n,d in methods}
    equal(document["bindings"],expected_bindings,"every exact declared original API binding")
    for c,n,d in methods:
        equal(classes.member(c,n,d,True)["modifiers"],1,"API binding agrees with pinned binary")
    fields = [("ct",chr(ord('a')+i)) for i in range(15)] + [("dj",n) for n in ("A","s","v","w","x")] + [("dp",n) for n in "CDEFG"]
    fields += [("ei","a"),("hi","a"),("kq","a")] + [("gs",n) for n in "abcdefghijklm"] + [("he",n) for n in ("c","f","g","h")]
    fields += [("net.minecraft.server.MinecraftServer",n) for n in ("c","d","e","f","p")]
    expected_fields = {}
    for c,n in fields:
        member = [x for x in classes.get(c)["fields"] if x["name"] == n]
        require(len(member) == 1,"one exact selected field")
        expected_fields[c+"."+n] = {"class":c,"field":n,"descriptor":member[0]["descriptor"],"modifiers":member[0]["modifiers"]}
    equal(document["field_bindings"],expected_fields,"every selected field agrees with actual binary declaration")
    states = document["states"]
    require(type(states) is list and 1 <= len(states) <= 64,"bounded complete interned states")
    state_bytes = [encoded(v) for v in states]
    require(all(len(v) <= 8*1024*1024 for v in state_bytes) and sum(map(len,state_bytes)) <= MAX,"complete state byte budgets")
    require(len({hashlib.sha256(v).hexdigest() for v in state_bytes}) == len(states),"lossless unique state dictionary")
    def state_ref(value):
        keys(value,("state_reference",),"complete state reference")
        return integer(value["state_reference"],0,len(states)-1,"existing exact state")
    equal(state_ref(document["initial"]),0,"actual initial state reference")
    ops = document["operations"]
    require(type(ops) is list and len(ops) == 17,"all seventeen actual original operations")
    schedule = [("origin_c","he","c","(II)Lhi;",list(p)) for p in ((0,0),(1,0),(0,1),(1,1),(1,1))]
    schedule += [("world_time_1234","dj","a","(J)V",[{"long":"00000000000004d2"}]),
        ("full_world_save_0","dj","a","(ZLpj;)V",[True,None]),("full_world_save_1","dj","a","(ZLpj;)V",[True,None]),
        ("protected_drop_0_0","he","d","(II)V",[0,0])]
    schedule += [("far_c","he","c","(II)Lhi;",list(p)) for p in ((16,0),(17,0),(16,1),(17,1))]
    schedule += [("drop_16_0","he","d","(II)V",[16,0]),("cache_maintenance","he","a","()Z",[]),
        ("reload_c_16_0","he","c","(II)Lhi;",[16,0]),("repeated_c_16_0","he","c","(II)Lhi;",[16,0])]
    refs = {"server":constructors[3]["result"],"storage":constructors[2]["result"],"parent":parent}
    details = [state_details(audit,v,i,document["construction_epoch_ms"],refs) for i,v in enumerate(states)]
    equal(details[0]["world"],constructors[4]["result"],"actual dedicated constructor result is held World")
    equal(details[0]["info"],expected_readback,"loaded dedicated Info fields retain genuine prepared metadata")
    equal(details[0]["files"]["world/level.dat"],audit.blob(prep["level_dat"]),"actual prepared metadata read by new handler")
    equal(details[0]["map"],{},"observed constructor cache begins empty")
    equal(details[0]["list"],[],"observed constructor list begins empty")
    equal(details[0]["pending"],[],"observed constructor pending set begins empty")
    previous, used_states, world_random_changes = 0, {0}, []
    random_ids = {identity for identity,value in details[0]["objects"].items() if value["object"]["class"] == "java.util.Random"}
    require(len(random_ids) > 0,"actual reached Random owners")
    world_random = audit.field(details[0]["objects"],details[0]["world"],"dj","r")["ref"]
    for i,(op,(action,owner,name,desc,args)) in enumerate(zip(ops,schedule)):
        keys(op,("index","action","receiver","class","method","descriptor","arguments","before","epoch_ms","result","failure","after"),"complete reached operation")
        equal((op["index"],op["action"],op["class"],op["method"],op["descriptor"],op["arguments"],op["failure"]),
            (i,action,owner,name,desc,args,None),"exact literal actual operation schedule")
        before_index,after_index = state_ref(op["before"]),state_ref(op["after"])
        equal(before_index,previous,"chronological complete before/after state continuity")
        before,after = details[before_index],details[after_index]
        equal(op["receiver"],before["cache"] if owner == "he" else before["world"],"actual operation receiver")
        low,high = epoch(op["epoch_ms"])
        require(low >= (construction[1] if i == 0 else ops[i-1]["epoch_ms"][1]),"chronological actual API clock intervals")
        expected_map,expected_list,expected_pending = dict(before["map"]),list(before["list"]),list(before["pending"])
        if name == "c":
            coordinate = tuple(args)
            value = audit.ref(op["result"],"hi",False)
            if coordinate in expected_map:
                equal(value,expected_map[coordinate],"actual cache hit returns same owner")
            else:
                require(value not in before["chunks"],"actual miss produces fresh unretained owner")
                expected_map[coordinate] = value
                expected_list.append(value)
            equal(after["chunks"][value]["coordinates"],coordinate,"actual returned Chunk coordinates")
        elif action == "drop_16_0":
            equal(op["result"],None,"actual drop void return")
            expected_pending = [16 << 16]
        elif action == "cache_maintenance":
            require(type(op["result"]) is bool,"actual maintenance Boolean")
            equal(before["pending"],[16 << 16],"one actually pending far target")
            target = expected_map.pop((16,0))
            expected_list.remove(target)
            expected_pending = []
            equal(after["chunks"][target]["flags"]["c"],False,"actual far Chunk detached")
            equal(after["chunks"][target]["last_save"],before["info"]["time"],"actual main-save time stamp")
            equal(after["chunks"][target]["arrays"],before["chunks"][target]["arrays"],"all retained old target bytes unchanged by save/detach")
            equal(after["chunks"][target]["flags"]["o"],before["chunks"][target]["flags"]["o"],"source maintenance retains old target dirty field")
        else:
            equal(op["result"],None,"actual void operation result")
        equal(after["map"],expected_map,"complete actual cache membership transition")
        equal(after["list"],expected_list,"complete actual cache insertion-list transition")
        equal(after["pending"],expected_pending,"complete actual pending membership transition")
        if i in (4,8,16):
            equal(after_index,before_index,"repeated hit/protected drop preserve whole complete state")
        after_randoms = {k for k,v in after["objects"].items() if v["object"]["class"] == "java.util.Random"}
        equal(after_randoms,random_ids,"same actual Random owners retained across operations")
        if before["objects"][world_random] != after["objects"][world_random]:
            world_random_changes.append({"operation":i,"action":action,"arguments":args,
                "before":before["objects"][world_random]["state"],"after":after["objects"][world_random]["state"]})
        if name != "c" or action in ("reload_c_16_0","repeated_c_16_0") or i == 4:
            for identity in random_ids:
                equal(after["objects"][identity],before["objects"][identity],"all Random bits/seed owners preserved by non-generation operation")
            for identity in before["objects"].keys() & after["objects"].keys():
                if before["objects"][identity]["object"]["class"].startswith("["):
                    equal(after["objects"][identity],before["objects"][identity],"every retained array descriptor/payload preserved by non-generation operation")
        previous = after_index
        used_states.update((before_index,after_index))
    equal(state_ref(document["final"]),previous,"complete final state continuity")
    equal(used_states,set(range(len(states))),"all interned states reached by schedule")
    old_ref,reloaded_ref = ops[9]["result"],ops[15]["result"]
    require(old_ref != reloaded_ref,"real reload creates distinct Chunk")
    equal(ops[16]["result"],reloaded_ref,"actual repeated lookup retains reloaded owner")
    old,loaded = details[-1]["chunks"][old_ref["ref"]],details[-1]["chunks"][reloaded_ref["ref"]]
    equal(old["arrays"],loaded["arrays"],"all five actual old/decoded arrays agree without normalization")
    require(not set(old["array_owners"].values()) & set(loaded["array_owners"].values()),"all decoded backing arrays have fresh identities")
    for i,(state,detail) in enumerate(zip(states,details)):
        equal(state["old_far"],None if i < state_ref(ops[9]["after"]) else old_ref,"actual old-target retention schedule")
        equal(state["reloaded_far"],None if i < state_ref(ops[15]["after"]) else reloaded_ref,"actual reloaded-target retention schedule")
        expected = dict(expected_readback)
        if i >= state_ref(ops[5]["after"]):
            expected["time"] = "00000000000004d2"
        if i >= state_ref(ops[6]["after"]):
            expected["size_on_disk"] = f"{len(detail['files']['world/region/r.0.0.mcr']):016x}"
        equal(detail["info"],expected,"all fifteen held Info fields and source size/time changes")
        equal(detail["files"]["world/session.lock"],details[0]["files"]["world/session.lock"],"actual live session preserved throughout")
    meta = [prep_meta]
    for i in (6,7):
        before,after = details[state_ref(ops[i]["before"])],details[state_ref(ops[i]["after"])]
        meta.append(metadata(after["files"]["world/level.dat"],before["info"],ops[i]["epoch_ms"],level))
        equal(after["files"]["world/level.dat_old"],before["files"]["world/level.dat"],"exact existing metadata rotates on each full save")
    first,second = details[state_ref(ops[6]["after"])],details[state_ref(ops[7]["after"])]
    equal(first["files"]["world/region/r.0.0.mcr"],second["files"]["world/region/r.0.0.mcr"],"clean second save preserves entire region bytes")
    intervals = {slot:ops[6]["epoch_ms"] for slot in (0,1,32,33)}
    records = region(first["files"]["world/region/r.0.0.mcr"],intervals,level)
    equal(set(records),{0,1,32,33},"all four actual first-save slots")
    presave = details[state_ref(ops[6]["before"])]
    for slot,value in records.items():
        chunk = presave["chunks"][presave["map"][(slot & 31,slot >> 5)]]
        saved_chunk(value["level"],chunk,1234,level)
    intervals[16] = ops[14]["epoch_ms"]
    final_records = region(details[-1]["files"]["world/region/r.0.0.mcr"],intervals,level)
    equal(set(final_records),{0,1,16,32,33},"maintenance writes only the far target's new slot")
    for slot,value in records.items():
        equal(final_records[slot],value,"every original saved record preserved after far operations")
    saved_chunk(final_records[16]["level"],details[state_ref(ops[14]["before"])]["chunks"][old_ref["ref"]],1234,level)
    # Check every retained snapshot's exact file set and every region container.
    for i,detail in enumerate(details):
        files = detail["files"]
        equal([entry["path"] for entry in states[i]["disk"]["entries"] if entry["kind"] == "directory"],
              [".","world","world/data"] + (["world/region"] if i > 0 else []),
              "complete exact source-created directory inventory")
        expected_files = {"world/level.dat","world/session.lock"}
        if i > 0:
            expected_files.add("world/region/r.0.0.mcr")
        if i >= state_ref(ops[6]["after"]):
            expected_files.add("world/level.dat_old")
        equal(set(files),expected_files,"complete exact file set at every state")
        if i < state_ref(ops[6]["after"]):
            equal(files["world/level.dat"],audit.blob(prep["level_dat"]),"generation/time do not save metadata")
            if i > 0:
                equal(files["world/region/r.0.0.mcr"],bytes(8192),"actual acquisition-created empty region header")
        elif i < state_ref(ops[14]["after"]):
            equal(files["world/region/r.0.0.mcr"],first["files"]["world/region/r.0.0.mcr"],"all bytes preserved before actual maintenance")
        if i >= state_ref(ops[7]["after"]):
            for path in ("world/level.dat","world/level.dat_old"):
                equal(files[path],second["files"][path],"far generation/drop/maintenance/readback preserve metadata")
    equal(details[-1]["files"],details[state_ref(ops[14]["after"])]["files"],"reload and repeated hit preserve complete disk bytes")
    closed = disk_record(audit,document["disk_after_close"])
    equal(closed,details[-1]["files"],"source region close preserves complete final files")
    occurrence_refs = [state_ref(document["initial"])] + [state_ref(op[k]) for op in ops for k in ("before","after")] + [state_ref(document["final"])]
    retained_reads = sum(states[i]["disk"]["bytes"] for i in occurrence_refs) + document["disk_after_close"]["bytes"]
    require(retained_reads <= 16*1024*1024,"actual cumulative repeated snapshot file-read budget")
    expected_origins = audit.reached | audit.literals | {c for c,n,d in methods} | {c for c,n in fields} | {c for c,d in expected_ctors} | {"om","pj"}
    equal(document["class_origins"],{n:source_path(str(jar)).as_uri().replace("file:///","file:/",1) for n in expected_origins},"complete original class origins from pinned archive")
    equal(audit.used_blobs,set(audit.blobs),"no unused or omitted raw-byte dictionary entries")
    require(set(audit.identities) == set(range(1,max(audit.identities)+1)),"complete contiguous actual global identity namespace")
    thread_results = []
    for key in ("threads_before","threads_after_operations","threads_after_close"):
        threads = document[key]
        require(type(threads) is list and 1 <= len(threads) <= 64,"complete bounded thread observation")
        ids, inert, main = set(), [], []
        for t in threads:
            keys(t,("id","name","daemon","state","original_frame","frames"),"complete observed thread")
            integer(t["id"],1,(1 << 63)-1,"actual thread ID")
            require(t["id"] not in ids and type(t["daemon"]) is bool and type(t["original_frame"]) is bool,"distinct typed thread observations")
            ids.add(t["id"])
            require(type(t["name"]) is str and type(t["frames"]) is list and len(t["frames"]) <= 128,"complete thread name/frames")
            require(t["state"] in ("NEW","RUNNABLE","BLOCKED","WAITING","TIMED_WAITING","TERMINATED"),"actual thread state")
            require(all(type(frame) is str and len(frame) <= 2048 for frame in t["frames"]),"complete bounded stack frames")
            game_frames = [frame for frame in t["frames"] if re.match(r"(?:[\w.-]+//)?(?:" + "|".join(re.escape(n) for n in classes.names) + r")\.",frame)]
            equal(bool(game_frames),t["original_frame"],"thread original-frame attribution")
            if t["original_frame"]:
                require(t["daemon"] is True and len(t["frames"]) >= 2 and all(re.fullmatch(r"bt\.run\(SourceFile:\d+\)|java\.base@" + re.escape(jdk_version) + r"/java\.lang\.Thread\.sleep\w*\([^)]*\)",f) is not None for f in t["frames"]),"only genuine inert original bt daemon frames")
                inert.append(t["id"])
            elif not t["daemon"]:
                equal(t["name"],"main","only measured main non-daemon")
                main.append(t["id"])
        require(len(inert) == len(main) == 1,"one actual original inert daemon and main")
        thread_results.append({"inert":inert[0],"main":main[0],"threads":len(threads)})
    equal([x["inert"] for x in thread_results],[thread_results[0]["inert"]]*3,"same inert daemon retained after close")
    result = {"passed":True,"native_comparison":False,"qualification_complete":False,"operations":17,
        "unique_states":len(states),"state_observations":len(occurrence_refs),"global_owner_count":len(audit.identities),
        "unique_blobs":len(audit.blobs),"unique_blob_bytes":audit.counts["unique_blob_bytes"],
        "unique_state_random_records":audit.counts["unique_state_random_records"],
        "unique_state_array_records":audit.counts["unique_state_array_records"],"actual_random_owners":sorted(random_ids),
        "world_random_owner":world_random,"world_random_changes":world_random_changes,"metadata":meta,
        "retained_file_read_bytes":retained_reads,"saved_slots":sorted(final_records),"saved_chunk_array_fields":25,
        "initial_classes":dict(Counter(v["object"]["class"] for v in details[0]["objects"].values())),
        "final_classes":dict(Counter(v["object"]["class"] for v in details[-1]["objects"].values())),
        "state_counts":[{"cache":len(v["map"]),"pending":v["pending"],"scheduled":v["scheduled"],"light":v["light"],"id_entries":v["ids"],
            "retained_chunks":len(v["chunks"]),"entities":sum(len(x["entities"]) for x in v["chunks"].values()),"tiles":sum(len(x["tiles"]) for x in v["chunks"].values())} for v in details],
        "old_far":{"owner":old["owner"],"minimum":old["minimum"],"flags":old["flags"]},
        "reloaded_far":{"owner":loaded["owner"],"minimum":loaded["minimum"],"flags":loaded["flags"]},"threads":thread_results,
        "class_names":sorted(classes.names),
        "class_metadata":{name:{**{k:value[k] for k in ("fingerprint","major","super","interfaces","fields")},
            "methods":[{k:method[k] for k in ("name","descriptor","modifiers")} for method in value["methods"]]}
            for name,value in classes.cache.items()}}
    classes.archive.close()
    return result,closed
