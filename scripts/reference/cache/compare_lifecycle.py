#!/usr/bin/env python3
"""Replay original lifecycle API boundaries through the native world owner.

Original captures supply chunk bytes, inputs, results and complete copied files.
The native process performs its own load, save, unload and region I/O. Entity
membership bodies remain an external seam; only their observed stale q flag is
replayed. The original/control validator separately checks instrumentation.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import queue
import re
import shutil
import struct
import subprocess
import sys
import threading
import time
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[3]
MAX_RECORD = 64 * 1024 * 1024
MAX_FILE = 8 * 1024 * 1024
MAX_WORLD = 64 * 1024 * 1024
MAX_STREAM = 128 * 1024 * 1024
ARRAYS = {"blocks": 32768, "metadata": 16384, "skylight": 16384,
          "block_light": 16384, "heightmap": 256}
PIN = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
       "base": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
       "dedicated": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
EXECUTED = {}


class Failure(Exception):
    pass


def require(value, message):
    if not value:
        raise Failure(message)


def exact(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(exact(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return type(left) in (int, bool, str, float, type(None)) and left == right


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def invalid_constant(value):
    raise Failure("nonfinite JSON: " + value)


def strict_json(raw):
    return json.loads(raw, object_pairs_hook=unique, parse_constant=invalid_constant)


def fingerprint(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def ordinary(path):
    return not path.is_symlink() and not (getattr(path.lstat(), "st_file_attributes", 0) & 0x400)


def read(path, limit=MAX_RECORD):
    require(path.is_file() and ordinary(path) and path.stat().st_size <= limit,
            "bounded ordinary input: " + str(path))
    raw = path.read_bytes()
    require(len(raw) <= limit, "input grew beyond bound")
    return raw


def file_fingerprint(path):
    require(path.is_file() and ordinary(path), "ordinary fingerprint input")
    digest, count = hashlib.sha256(), 0
    with path.open("rb") as source:
        while raw := source.read(1024 * 1024):
            count += len(raw)
            require(count <= 1024 * 1024 * 1024, "fingerprint input bound")
            digest.update(raw)
    return {"bytes": count, "sha256": digest.hexdigest()}


def load(name, path, *, expected=None):
    raw = read(path, 2 * 1024 * 1024)
    require(expected is None or fingerprint(raw) == expected, "executed Python source matches archived input")
    EXECUTED[str(path.resolve())] = fingerprint(raw)
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    exec(compile(raw, str(path), "exec"), module.__dict__)
    return module


def int32(value):
    require(type(value) is int and -(1 << 31) <= value < (1 << 31), "signed int32")
    return value


def bounded_int(value, low, high):
    require(type(value) is int and low <= value <= high, "exact bounded integer")


def keys(value, expected):
    require(type(value) is dict and set(value) == set(expected), "complete JSON fields")


def diagnostic(value):
    keys(value, ("status", "operation", "system_error"))
    bounded_int(value["status"], 0, 9)
    bounded_int(value["system_error"], 0, 0xffffffff)
    require((value["operation"] is None and value["system_error"] == 0) if value["status"] == 0 else
            type(value["operation"]) is str and 0 < len(value["operation"]) <= 256, "typed native diagnostic")


def validate_info(value):
    keys(value, ("seed", "spawn", "time", "last_played", "size_on_disk", "level_name", "version",
                 "rain_time", "raining", "thunder_time", "thundering", "has_player", "player_dimension"))
    for name in ("seed", "time", "last_played", "size_on_disk"):
        hex_bytes(value[name], 8)
    require(type(value["level_name"]) is str and len(value["level_name"]) <= 262140 and
            len(value["level_name"]) % 4 == 0, "complete bounded UTF16 units")
    hex_bytes(value["level_name"], len(value["level_name"]) // 2)
    require(type(value["spawn"]) is list and len(value["spawn"]) == 3, "spawn triple")
    for coordinate in value["spawn"]:
        int32(coordinate)
    for name in ("version", "rain_time", "thunder_time", "player_dimension"):
        int32(value[name])
    for name in ("raining", "thundering", "has_player"):
        require(type(value[name]) is bool, "exact native metadata Boolean")


def hex_bytes(value, length):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]{" + str(length * 2) + "}", value),
            "complete lowercase hex bytes")
    return bytes.fromhex(value)


def inventory(root):
    require(root.is_dir() and ordinary(root), "ordinary owned world root")
    result, total, count = {}, 0, 0
    for path in sorted(root.rglob("*")):
        count += 1
        require(count <= 1024 and ordinary(path), "bounded ordinary world paths")
        if path.is_file():
            raw = read(path, MAX_FILE)
            total += len(raw)
            require(len(result) < 256 and total <= MAX_WORLD, "complete world byte bound")
            result[path.relative_to(root).as_posix()] = fingerprint(raw)
        else:
            require(path.is_dir(), "ordinary world directory")
    return result


def fresh_work(base, protected):
    base = base.absolute()
    for path in (base, *base.parents):
        require(ordinary(path) if path.exists() else not path.is_symlink(), "ordinary output ancestors")
    base = base.resolve()
    if base.is_relative_to(ROOT):
        parts = base.relative_to(ROOT).parts
        require(parts and (parts[0] in (".local", "build") or parts[0].startswith("build-")),
                "repository capture requires ignored output directory")
    else:
        require(not ROOT.is_relative_to(base), "output cannot contain source root")
    for path in protected:
        path = path.resolve()
        require(not base.is_relative_to(path) and not path.is_relative_to(base), "output overlaps input")
    base.mkdir(parents=True, exist_ok=True)
    work = base / ("capture-" + uuid.uuid4().hex)
    work.mkdir()
    return work


class Original:
    """Strict dictionary references retain full immutable states without expansion."""
    def __init__(self, record):
        require(record["schema"] == 1 and type(record["schema"]) is int and
                record["context"] in PIN and record["jar_sha256"] == PIN[record["context"]],
                "pinned original lifecycle profile")
        self.record = record
        self.states = record.get("chunk_states", [])
        require(type(self.states) is list and len(self.states) <= 65536, "chunk dictionary bound")
        self.blobs = {}
        require(type(record["blobs"]) is dict and len(record["blobs"]) <= 4096, "blob dictionary bound")
        total = 0
        for sha, row in record["blobs"].items():
            require(type(row) is dict and set(row) == {"bytes", "hex"} and
                    type(row["bytes"]) is int and 0 <= row["bytes"] <= MAX_FILE,
                    "complete blob entry")
            raw = hex_bytes(row["hex"], row["bytes"])
            require(fingerprint(raw)["sha256"] == sha, "blob bytes match fingerprint")
            total += len(raw)
            require(total <= MAX_RECORD, "complete blob dictionary byte bound")
            self.blobs[sha] = raw
        self.counts = {"states": 0, "chunks": 0, "arrays": 0, "files": 0, "region_slots": 0}
        self.retained_time = record.get("initial", {}).get("metadata", {}).get("time")
        metadata = record.get("initial", {}).get("metadata", {})
        self.retained_spawn = [metadata.get("spawn_x"), metadata.get("spawn_y"), metadata.get("spawn_z")]

    def blob(self, row):
        require(type(row) is dict and set(row) in ({"bytes", "sha256"}, {"id", "bytes", "sha256"}),
                "complete blob reference")
        require(row["sha256"] in self.blobs and type(row["bytes"]) is int, "resolved blob reference")
        raw = self.blobs[row["sha256"]]
        require(len(raw) == row["bytes"], "exact blob size")
        return raw

    def chunk(self, row):
        if type(row) is dict and set(row) == {"chunk_reference"}:
            index = row["chunk_reference"]
            require(type(index) is int and 0 <= index < len(self.states), "chunk reference index")
            row = self.states[index]
        require(type(row) is dict and set(row) == {"id", "class", "world", "coordinates", "blank",
                "flags", "last_save", "minimum", "arrays", "tiles", "slices_owner", "entities"},
                "complete helper chunk")
        return row

    def files(self, state):
        require(type(state["files"]) is dict and len(state["files"]) <= 256, "complete file map")
        result = {}
        for name, row in state["files"].items():
            require(type(name) is str and name and "\\" not in name and ":" not in name and
                    all(part not in ("", ".", "..") for part in name.split("/")), "relative world file")
            result[name] = self.blob(row)
        require(sum(map(len, result.values())) <= MAX_WORLD, "complete file byte bound")
        return result


class Session:
    """Retained, bounded interactive native process, including failed streams."""
    def __init__(self, executable, directory, records):
        self.directory, self.lines = directory, queue.Queue(maxsize=32)
        self.payloads, self.index = {}, 0
        self.latest, self.largest = {}, 0
        self.counts = {"auxiliary": 0, "provider": 0, "level": 0, "chunks": 0}
        self.started, self.input_bytes = time.monotonic(), 0
        self.record = {"command": [str(executable)], "cwd": str(directory), "exit": None,
                       "reaped": False, "timeout": False, "output_limited": False,
                       "seconds_limit": 180, "stream_limit": MAX_STREAM}
        records.append(self.record)
        self.incoming = (directory / "native.stdin").open("xb")
        self.outgoing = (directory / "native.stdout").open("xb")
        self.errors = (directory / "native.stderr").open("w+b")
        self.child = subprocess.Popen([str(executable)], cwd=directory, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=self.errors)
        self.record["pid"] = self.child.pid
        def reader():
            count = 0
            try:
                while True:
                    raw = self.child.stdout.readline(4 * 1024 * 1024 + 1)
                    if not raw:
                        self.lines.put(None, timeout=5)
                        return
                    count += len(raw)
                    remaining = max(0, MAX_STREAM - self.outgoing.tell())
                    self.outgoing.write(raw[:remaining])
                    self.outgoing.flush()
                    if len(raw) > 4 * 1024 * 1024 or not raw.endswith(b"\n") or count > MAX_STREAM:
                        self.record["output_limited"] = True
                        raise Failure("native output line/stream bound")
                    self.lines.put(raw, timeout=5)
            except BaseException as error:
                try:
                    self.lines.put(error, timeout=5)
                except queue.Full:
                    pass
            finally:
                self.record["stdout_total_bytes"] = count
        self.reader = threading.Thread(target=reader, daemon=True)
        self.reader.start()

    def request(self, *words):
        text = "\t".join(str(word) for word in words)
        raw = text.encode("ascii") + b"\n"
        require(len(raw) <= 4097 and b"\n" not in raw[:-1] and self.index < 4096,
                "bounded native request")
        self.input_bytes += len(raw)
        require(self.input_bytes <= 1024 * 1024, "native input bound")
        self.incoming.write(raw)
        self.incoming.flush()
        errors = []
        def writer():
            try:
                self.child.stdin.write(raw)
                self.child.stdin.flush()
            except BaseException as error:
                errors.append(error)
        write_thread = threading.Thread(target=writer, daemon=True)
        write_thread.start()
        write_thread.join(10)
        require(not write_thread.is_alive() and not errors, "complete bounded native input write")
        self.index += 1
        deadline = min(self.started + 180, time.monotonic() + 30)
        emitted_payloads = set()
        while True:
            if time.monotonic() >= deadline:
                self.record["timeout"] = True
                raise Failure("native response timeout")
            require(os.fstat(self.errors.fileno()).st_size <= MAX_STREAM, "native stderr bound")
            try:
                line = self.lines.get(timeout=min(0.1, deadline - time.monotonic()))
            except queue.Empty:
                continue
            require(type(line) is bytes, "native response missing: " + repr(line))
            value = strict_json(line)
            require(type(value) is dict, "native JSON object")
            if value.get("kind") == "PAYLOAD":
                require(set(value) == {"kind", "id", "version", *ARRAYS}, "complete native payload")
                identity, version = value["id"], value["version"]
                require(type(identity) is int and identity > 0 and type(version) is int and version > 0 and
                        (identity, version) not in self.payloads, "unique native payload version")
                bounded_int(identity, 1, 4096)
                bounded_int(version, 1, 4096)
                require(identity not in emitted_payloads, "one complete payload per identity per response")
                emitted_payloads.add(identity)
                arrays = {role: hex_bytes(value[role], size) for role, size in ARRAYS.items()}
                if identity in self.latest:
                    previous = self.latest[identity]
                    require(version == previous + 1 and arrays != self.payloads[identity, previous],
                            "changed native arrays increment their version")
                else:
                    require(identity == self.largest + 1 and version == 1, "monotonic native identity")
                    self.largest = identity
                self.payloads[identity, version] = arrays
                self.latest[identity] = version
                continue
            require(value.get("kind") == "RESPONSE" and type(value["schema"]) is int and value["schema"] == 1 and
                    type(value["command_index"]) is int and
                    value["command_index"] == self.index and value["command"] == words[0],
                    "native command/response correspondence")
            self.validate(value, words[0])
            return value

    def validate(self, value, command):
        keys(value, ("kind", "schema", "command_index", "command", "category", "status", "result", "completed",
                     "returned_id", "error", "callbacks", "state"))
        category = ("acquire" if command in ("ACQUIRE", "GET", "DROP") else "lifecycle" if command in
                    ("SAVE", "WORLD_SAVE", "CLIENT_SAVE", "MAINTAIN") else "storage" if command in
                    ("OPEN", "CLOSE", "COMPETE", "RETIRE") else "adapter")
        require(value["category"] == category, "native command status category")
        bounded_int(value["status"], 0, 6 if category == "acquire" else 4 if category == "lifecycle" else 9)
        diagnostic(value["error"])
        for name in ("result", "completed"):
            require(value[name] is None or type(value[name]) is bool, "typed optional native output")
        require((value["result"] is not None) == (command in ("ELIGIBLE", "MAINTAIN") and value["status"] == 0) and
                (value["completed"] is not None) == (command in ("SAVE", "CLIENT_SAVE") and value["status"] == 0),
                "only successful assigned outputs")
        require(type(value["callbacks"]) is list and len(value["callbacks"]) <= 16, "complete bounded callbacks")
        for event in value["callbacks"]:
            keys(event, ("kind", "stage", "ordinal"))
            require(event["kind"] in ("auxiliary", "provider", "progress"), "native callback kind")
            if event["kind"] == "progress":
                bounded_int(event["stage"], 0, 1)
                name = "level" if event["stage"] == 0 else "chunks"
            else:
                require(event["stage"] is None, "non-progress callback stage")
                name = event["kind"]
            self.counts[name] += 1
            bounded_int(event["ordinal"], self.counts[name], self.counts[name])
        state = value["state"]
        keys(state, ("open", "dimension", "policy", "clocks", "live", "info", "retained_info", "metadata_hex",
                     "owned_count", "reservation_count", "chunks", "fallback", "map", "list", "pending",
                     "pending_membership", "callback_counts", "last_error", "regions"))
        require(type(state["open"]) is bool and state["open"], "expected open native runtime")
        bounded_int(state["dimension"], -1, 0)
        require(state["policy"] in ("base", "dedicated"), "native runtime policy")
        require(exact(state["callback_counts"], self.counts), "complete exact callback totals")
        clocks = state["clocks"]
        keys(clocks, ("saved_at_ms", "timestamp", "timestamp_count", "timestamp_index", "timestamp_queue"))
        hex_bytes(clocks["saved_at_ms"], 8)
        hex_bytes(clocks["timestamp"], 4)
        bounded_int(clocks["timestamp_count"], 0, 256)
        bounded_int(clocks["timestamp_index"], 0, clocks["timestamp_count"])
        require(type(clocks["timestamp_queue"]) is list and len(clocks["timestamp_queue"]) == clocks["timestamp_count"],
                "complete native clock queue")
        for timestamp in clocks["timestamp_queue"]:
            hex_bytes(timestamp, 4)
        live = state["live"]
        keys(live, ("time", "spawn", "world_force_load", "provider_force_load", "saving_disabled"))
        hex_bytes(live["time"], 8)
        require(type(live["spawn"]) is list and len(live["spawn"]) == 3, "native live spawn")
        for coordinate in live["spawn"]:
            int32(coordinate)
        for name in ("world_force_load", "provider_force_load", "saving_disabled"):
            require(type(live[name]) is bool, "exact native live Boolean")
        validate_info(state["info"])
        validate_info(state["retained_info"])
        require(exact(state["info"], state["retained_info"] | {"time": live["time"], "spawn": live["spawn"]}),
                "fresh retained metadata overlay")
        require(type(state["metadata_hex"]) is str and len(state["metadata_hex"]) <= 2 * 1024 * 1024,
                "bounded encoded retained metadata")
        NBT(hex_bytes(state["metadata_hex"], len(state["metadata_hex"]) // 2))
        diagnostic(state["last_error"])
        bounded_int(state["owned_count"], 0, 256)
        bounded_int(state["reservation_count"], 0, 256 - state["owned_count"])
        require(type(state["chunks"]) is list and len(state["chunks"]) == state["owned_count"], "every owned native chunk")
        alive = set()
        for chunk in [*state["chunks"], state["fallback"]]:
            keys(chunk, ("id", "coordinates", "blank", "flags", "last_save_time", "minimum_height", "payload_version"))
            fallback = chunk is state["fallback"]
            bounded_int(chunk["id"], 0 if fallback else 1, 0 if fallback else 4096)
            require(type(chunk["coordinates"]) is list and len(chunk["coordinates"]) == 2 and
                    chunk["blank"] is fallback, "native chunk coordinates/blank dispatch")
            for coordinate in chunk["coordinates"]:
                int32(coordinate)
            keys(chunk["flags"], "cnopq")
            require(all(type(flag) is bool for flag in chunk["flags"].values()), "exact native chunk Boolean")
            hex_bytes(chunk["last_save_time"], 8)
            if fallback:
                require(chunk["minimum_height"] is None and chunk["payload_version"] is None, "virtual native blank data")
            else:
                int32(chunk["minimum_height"])
                bounded_int(chunk["payload_version"], 1, 4096)
                require(chunk["id"] not in alive and self.latest.get(chunk["id"]) == chunk["payload_version"],
                        "unique native chunk with latest complete payload")
                alive.add(chunk["id"])
        keys(state["regions"], ("count", "bytes"))
        bounded_int(state["regions"]["count"], 0, 256)
        bounded_int(state["regions"]["bytes"], 0, MAX_STREAM)
        require(type(state["map"]) is list and len(state["map"]) <= 256 and
                type(state["list"]) is list and len(state["list"]) <= 512, "complete bounded native graph")
        map_keys = set()
        for row in state["map"]:
            require(type(row) is list and len(row) == 2, "native map row")
            int32(row[0])
            bounded_int(row[1], 0, self.largest)
            require(row[0] not in map_keys and (row[1] == 0 or row[1] in alive), "unique native key/alive identity")
            map_keys.add(row[0])
        for identity in state["list"]:
            bounded_int(identity, 0, self.largest)
            require(identity == 0 or identity in alive, "native list alive identity")
        for name in ("pending", "pending_membership"):
            require(type(state[name]) is list and len(state[name]) <= 256, "bounded native pending")
            for key in state[name]:
                int32(key)
            require(len(state[name]) == len(set(state[name])), "unique native pending keys")
        require(set(state["pending"]) == set(state["pending_membership"]), "complete native pending membership")
        if command in ("ACQUIRE", "GET") and value["status"] == 0:
            bounded_int(value["returned_id"], 0, self.largest)
            require(value["returned_id"] == 0 or value["returned_id"] in alive, "native returned alive identity")
        else:
            require(value["returned_id"] is None, "native non-acquisition output unchanged")

    def finish(self, successful):
        try:
            if successful:
                self.child.stdin.close()
                try:
                    self.child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.record["timeout"] = True
                    raise Failure("native EOF cleanup timeout")
                self.reader.join(10)
                require(not self.reader.is_alive() and self.lines.get(timeout=1) is None and self.lines.empty(),
                        "complete native stream with no extra responses")
                require(self.child.returncode == 0, "native unsuccessful exit")
                require(os.fstat(self.errors.fileno()).st_size == 0, "native internal/sanitizer stderr")
        finally:
            if self.child.poll() is None:
                self.child.kill()
            self.child.wait(timeout=10)
            self.record["exit"], self.record["reaped"] = self.child.returncode, True
            if not self.child.stdin.closed:
                self.child.stdin.close()
            self.child.stdout.close()
            self.reader.join(10)
            require(not self.reader.is_alive(), "native reader reaped")
            self.incoming.close()
            self.outgoing.close()
            size = os.fstat(self.errors.fileno()).st_size
            self.record["stderr_total_bytes"] = size
            self.record["output_limited"] |= size > MAX_STREAM
            self.errors.truncate(min(size, MAX_STREAM))
            self.errors.close()
            self.record["seconds"] = round(time.monotonic() - self.started, 6)
            for name in ("stdin", "stdout", "stderr"):
                path = self.directory / ("native." + name)
                self.record[name] = file_fingerprint(path)


class NBT:
    """Bounded typed NBT; compound order alone has no semantic comparison role."""
    def __init__(self, raw):
        require(len(raw) <= 1024 * 1024, "NBT byte bound")
        self.raw, self.position, self.nodes = raw, 0, 0
        self.tree = self.tag(0)
        require(self.position == len(raw), "complete NBT without trailing bytes")

    def take(self, count):
        require(type(count) is int and 0 <= count <= len(self.raw) - self.position, "complete NBT field")
        start = self.position
        self.position += count
        return self.raw[start:self.position]

    def number(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))[0]

    def text(self):
        return self.take(self.number(">H"))

    def tag(self, depth):
        kind = self.number(">B")
        require(1 <= kind <= 11, "named NBT kind")
        return kind, self.text(), self.value(kind, depth + 1)

    def value(self, kind, depth):
        self.nodes += 1
        require(depth <= 64 and self.nodes <= 65536, "NBT depth/node bounds")
        if kind in (1, 2, 3, 4):
            return self.number({1: ">b", 2: ">h", 3: ">i", 4: ">q"}[kind])
        if kind in (5, 6):
            return self.take(4 if kind == 5 else 8)
        if kind in (7, 11):
            count = self.number(">i")
            require(0 <= count <= 1024 * 1024, "NBT array count")
            return self.take(count * (4 if kind == 11 else 1))
        if kind == 8:
            return self.text()
        if kind == 9:
            element, count = self.number(">B"), self.number(">i")
            require(0 <= element <= 11 and 0 <= count <= 65536 and (count == 0 or element != 0),
                    "NBT list shape")
            return element, tuple(self.value(element, depth + 1) for _ in range(count))
        require(kind == 10, "NBT compound kind")
        children = {}
        while True:
            require(self.position < len(self.raw), "terminated NBT compound")
            if self.raw[self.position] == 0:
                self.position += 1
                break
            child = self.tag(depth + 1)
            require(child[1] not in children, "unique NBT compound names")
            children[child[1]] = child
        return tuple(children[name] for name in sorted(children))


def inflate(raw, gzip=False):
    decoder = zlib.decompressobj(31 if gzip else 15)
    plain = decoder.decompress(raw, 1024 * 1024 + 1)
    require(len(plain) <= 1024 * 1024 and decoder.eof and not decoder.unconsumed_tail and
            not decoder.unused_data, "complete bounded compressed NBT member")
    return plain


def nbt_child(parent, name, kind=None):
    require(parent[0] == 10, "required NBT compound")
    found = [child for child in parent[2] if child[1] == name.encode("ascii")]
    require(len(found) == 1 and (kind is None or found[0][0] == kind), "required typed NBT " + name)
    return found[0]


def region_slots(raw):
    require(len(raw) >= 8192 and len(raw) % 4096 == 0, "complete sector-aligned region")
    slots, extents = {}, []
    for index in range(1024):
        location = struct.unpack_from(">I", raw, 4 * index)[0]
        if not location:
            continue
        start, count = (location >> 8) * 4096, location & 255
        require(start >= 8192 and count > 0 and start + count * 4096 <= len(raw), "bounded region extent")
        require(all(start + count * 4096 <= a or start >= b for a, b in extents), "disjoint region allocations")
        extents.append((start, start + count * 4096))
        length = struct.unpack_from(">I", raw, start)[0]
        selector = raw[start + 4]
        require(1 <= length <= count * 4096 - 4 and selector in (1, 2), "typed region record")
        plain = inflate(raw[start + 5:start + 4 + length], selector == 1)
        slots[index] = {"start": start, "end": start + count * 4096,
                        "encoded_end": start + 4 + length,
                        "timestamp": struct.unpack_from(">I", raw, 4096 + 4 * index)[0],
                        "selector": selector, "tree": NBT(plain).tree}
    return slots


def compare_files(original, actual_root, written, counters, semantic_metadata):
    """All names and untouched bytes exact; written records compare every NBT tag."""
    actual = inventory(actual_root)
    require(set(actual) == set(original), "complete native/original file inventory")
    for name, expected in original.items():
        actual = read(actual_root / name, MAX_FILE)
        counters["files"] += 1
        if name in semantic_metadata:
            require(NBT(inflate(actual, True)).tree == NBT(inflate(expected, True)).tree,
                    "all native/original metadata NBT fields: " + name)
        elif name.endswith(".mcr") and name in written:
            require(len(actual) == len(expected) and actual[:8192] == expected[:8192],
                    "exact region length/location/timestamp headers: " + name)
            left, right = region_slots(expected), region_slots(actual)
            require(left.keys() == right.keys(), "complete native/original region slots")
            ranges = []
            for index in written[name]:
                require(index in left and index in right, "actual original written slot")
                a, b = left[index], right[index]
                require(a["start"] == b["start"] and a["end"] == b["end"] and
                        a["selector"] == b["selector"] and a["tree"] == b["tree"],
                        "complete native/original written chunk NBT: " + name + ":" + str(index))
                padding = max(a["encoded_end"], b["encoded_end"])
                require(actual[padding:a["end"]] == expected[padding:a["end"]],
                        "all original/native written-slot trailing padding")
                ranges.append((a["start"], a["end"]))
                counters["region_slots"] += 1
            position = 8192
            for start, end in sorted(ranges):
                require(actual[position:start] == expected[position:start], "all untouched region bytes")
                position = end
            require(actual[position:] == expected[position:], "all untouched trailing region bytes")
        else:
            require(actual == expected, "unchanged native/original raw file: " + name)


def raw_files(root):
    names = inventory(root)
    return {name: read(root / name, MAX_FILE) for name in names}


def preserve_step(before, after, written, metadata_write):
    permitted_metadata = {"level.dat", "level.dat_old", "level.dat_new"} if metadata_write else set()
    require((set(before) ^ set(after)) <= permitted_metadata, "only actual metadata rotation changes file names")
    for name in set(before) & set(after):
        left, right = before[name], after[name]
        if name in permitted_metadata:
            continue
        if name not in written:
            require(left == right, "no native write outside this operation's actual original writes: " + name)
            continue
        a, b = region_slots(left), region_slots(right)
        require(len(left) == len(right) and a.keys() == b.keys(), "loaded-only saves retain complete region allocation size")
        ranges = []
        for index in written[name]:
            require(index in a and index in b, "written slot exists in both region states")
            ranges.extend(((4 * index, 4 * index + 4), (4096 + 4 * index, 4100 + 4 * index),
                           (a[index]["start"], a[index]["end"]), (b[index]["start"], b[index]["end"])))
        position = 0
        for start, end in sorted(ranges):
            if start > position:
                require(left[position:start] == right[position:start], "native operation preserves all unwritten region bytes")
            position = max(position, end)
        require(left[position:] == right[position:], "native operation preserves unwritten region suffix")


def metadata_written(trace_operation):
    returns = [event for event in trace_operation["events"] if event["role"] == "save_info" and event["edge"] == "return"]
    require(len(returns) <= 1, "one actual successful metadata save per API")
    return bool(returns)


def info_projection(metadata):
    require(metadata["player"] is None, "finite lifecycle profile has no Player document")
    return {"seed": metadata["seed"],
            "spawn": [metadata["spawn_x"], metadata["spawn_y"], metadata["spawn_z"]],
            "time": metadata["time"], "last_played": metadata["last_played"],
            "size_on_disk": metadata["size_on_disk"], "has_player": False,
            "player_dimension": metadata["player_dimension"],
            "level_name": metadata["level_name"].encode("utf-16-be", "surrogatepass").hex(),
            "version": metadata["version"], "raining": metadata["raining"],
            "rain_time": metadata["rain_time"], "thundering": metadata["thundering"],
            "thunder_time": metadata["thunder_time"]}


def compare_state(source, expected, actual, identities, session, *, external_membership=False):
    require(actual["open"] is True and actual["dimension"] == 0 and
            actual["policy"] == ("dedicated" if source.record["context"] == "dedicated" else "base"),
            "native runtime profile")
    expected_live = {"time": expected["metadata"]["time"],
                     "spawn": [expected["metadata"]["spawn_x"], expected["metadata"]["spawn_y"], expected["metadata"]["spawn_z"]],
                     "world_force_load": expected["world_force_load"],
                     "provider_force_load": expected["provider_force_load"] or False,
                     "saving_disabled": expected["save_off"]}
    require(exact(actual["live"], expected_live), "complete native/original live world state")
    require(exact(actual["info"], info_projection(expected["metadata"])), "complete native/original live metadata")
    retained = info_projection(expected["metadata"]) | {"time": source.retained_time, "spawn": source.retained_spawn}
    require(exact(actual["retained_info"], retained), "retained metadata clock/spawn follow actual metadata-save history")
    require(actual["reservation_count"] == 0, "no load-only runtime reservations")
    require(expected["queue"]["values"] == [] and all(row["values"] == [] for row in expected["registrations"].values()),
            "explicit finite empty queue/registration seam")
    rows = [source.chunk(row) for row in expected["chunks"]]
    require(len({row["id"] for row in rows}) == len(rows), "unique complete original chunk inventory")
    require(set(identities) == {row["id"] for row in rows}, "all original chunk identities bound")
    normal = {row["id"]: row for row in actual["chunks"]}
    require(len(normal) == len(actual["chunks"]) == actual["owned_count"] == len(rows) - 1,
            "complete native owned chunk inventory")
    for row in rows:
        identity = identities[row["id"]]
        chunk = actual["fallback"] if identity == 0 else normal[identity]
        flags = row["flags"]
        require(set(flags) == {"c", "n", "o", "p", "q"} and all(type(v) is bool for v in flags.values()),
                "complete exact original chunk flags")
        require(exact(chunk["coordinates"], row["coordinates"]) and chunk["blank"] is row["blank"] and
                exact(chunk["flags"], flags) and chunk["last_save_time"] == row["last_save"],
                "native/original chunk identity/header: " + str(row["id"]))
        require(len(row["entities"]) == 8 and row["tiles"]["entries"] == [], "finite eight slices and empty tiles")
        if not external_membership:
            require(all(item["members"] == [] for item in row["entities"]), "empty native entity serialization seam")
        if identity:
            require(chunk["minimum_height"] == row["minimum"], "native/original minimum height")
            payload = session.payloads.get((identity, chunk["payload_version"]))
            require(payload is not None and set(row["arrays"]) == set(ARRAYS), "complete native/original payload arrays")
            for role, size in ARRAYS.items():
                raw = source.blob(row["arrays"][role]["data"])
                require(len(raw) == size and payload[role] == raw, "native/original complete array: " + role)
                source.counts["arrays"] += 1
        else:
            require(chunk["minimum_height"] is None and chunk["payload_version"] is None,
                    "explicit native blank has no allocated data")
        source.counts["chunks"] += 1
    expected_map = [[int32(key), identities[identity]] for key, identity in expected["map"]["entries"]]
    require(len({key for key, _ in expected_map}) == len(expected_map) and
            len({key for key, _ in actual["map"]}) == len(actual["map"]) and
            exact(sorted(actual["map"]), sorted(expected_map)), "complete packed-key map membership")
    require(exact(actual["list"], [identities[value] for value in expected["insertions"]["values"]]),
            "exact insertion list order and aliases")
    require(exact(actual["pending"], expected["pending"]["values"]) and
            sorted(actual["pending_membership"]) == sorted(expected["pending"]["values"]),
            "exact fresh pending iteration and complete membership")
    source.counts["states"] += 1


def timestamp_inputs(trace_operation, coordinates):
    values, written, writing = [], {}, []
    for event in trace_operation["events"]:
        if event["role"] == "write":
            if event["edge"] == "breakpoint" and event["code_index"] == 0:
                require(len(event["arguments"]) == 2 and type(event["arguments"][1]) is dict,
                        "actual original loader main arguments")
                identity = event["arguments"][1]["object_id"]
                require(identity in coordinates, "original loader target identity")
                writing.append(coordinates[identity])
            elif event["edge"] == "return":
                require(writing, "paired original loader return")
                writing.pop()
        if event["role"] == "timestamp" and event["edge"] == "breakpoint" and event["code_index"] == 0:
            require(len(event["arguments"]) == 3, "actual region timestamp setter arguments")
            local_x, local_z, timestamp = map(int32, event["arguments"])
            require(len(writing) == 1, "timestamp belongs to exact loader invocation")
            x, z = writing[-1]
            require(local_x == (x & 31) and local_z == (z & 31), "timestamp local/global coordinate binding")
            values.append(f"{timestamp & 0xffffffff:08x}")
            name = f"region/r.{x >> 5}.{z >> 5}.mcr"
            written.setdefault(name, set()).add((x & 31) + 32 * (z & 31))
    require(len(values) <= 256, "bounded complete timestamp inputs")
    return values, written


def save_clock(source, operation, saves_metadata):
    after = source.files(operation["after"])
    if not saves_metadata:
        return 0
    require("level.dat" in after, "successful original metadata write")
    tree = NBT(inflate(after["level.dat"], True)).tree
    return nbt_child(nbt_child(tree, "Data", 10), "LastPlayed", 4)[2] & ((1 << 64) - 1)


def operation_command(operation, identities):
    action, signature, args = operation["action"], operation["signature"], operation["arguments"]
    if action == "competing_handler_constructor":
        require(signature == "(Ljava/io/File;Ljava/lang/String;Z)V" and operation["method"] == "<init>" and
                len(args) == 3 and args[1] == "world" and args[2] is False, "actual competing handler constructor")
        token = operation["after"]["competing_handler"]["token"]
        hex_bytes(token, 8)
        return "COMPETE", token
    if action == "time":
        require(signature == "(J)V" and len(args) == 1 and set(args[0]) == {"long"}, "time API descriptor")
        hex_bytes(args[0]["long"], 8)
        return "TIME", args[0]["long"]
    if action in ("acquire", "drop"):
        require(signature in ("(II)Llm;", "(II)Lhi;") if action == "acquire" else signature == "(II)V",
                "acquisition/drop API descriptor")
        require(len(args) == 2, "coordinate API arguments")
        return ("ACQUIRE" if action == "acquire" else "DROP"), int32(args[0]), int32(args[1])
    if action == "mark_dirty":
        require(signature == "()V" and args == [], "dirty API descriptor")
        return "DIRTY", identities[operation["receiver"]]
    if signature == "(Z)Z":
        require(len(args) == 1 and type(args[0]) is bool, "eligibility API descriptor")
        return "ELIGIBLE", identities[operation["receiver"]], int(args[0])
    if signature in ("(ZLyb;)Z", "(ZLpj;)Z"):
        require(len(args) == 2 and type(args[0]) is bool and args[1] is None, "cache save API arguments")
        return "SAVE", int(args[0])
    if signature in ("(ZLyb;)V", "(ZLpj;)V"):
        require(len(args) == 2 and type(args[0]) is bool and args[1] is None, "world save API arguments")
        return "WORLD_SAVE", int(args[0])
    if action in ("client_zero_pass", "client_nonzero_pass"):
        require(signature == "(I)Z" and len(args) == 1, "client save API descriptor")
        return "CLIENT_SAVE", int32(args[0])
    if signature == "()Z" and action in ("empty_maintenance", "unload_one", "unload100", "unload_last"):
        require(args == [], "maintenance API arguments")
        return ("MAINTAIN",)
    if action in ("api_owned_membership_add", "api_owned_membership_remove"):
        require(signature in ("(Lsn;)V", "(Llq;)V") and len(args) == 1, "external entity membership API")
        return "ENTITY_HINT", identities[operation["receiver"]], 1
    raise Failure("unsupported original API action/descriptor: " + action + ":" + signature)


def bind_acquired(result, response, identities):
    require(type(result) is dict and set(result) == {"reference"} and type(result["reference"]) is int and
            type(response["returned_id"]) is int, "exact original/native acquired identity")
    identity, native = result["reference"], response["returned_id"]
    if identity in identities:
        require(identities[identity] == native, "existing acquired identity remains stable")
    else:
        require(native not in identities.values(), "new original identity maps to new native owner")
        identities[identity] = native


def compare_batch_boundary(source, trace_source, state, actual, identities, original_to_jdi, session):
    trace = trace_source.record
    headers = trace["chunk_headers"]
    def header(row):
        if type(row) is dict and set(row) == {"header_reference"}:
            index = row["header_reference"]
            bounded_int(index, 0, len(headers) - 1)
            row = headers[index]
        require(type(row) is dict and set(row) == {"id", "class", "world", "coordinates", "flags", "minimum", "last_save"},
                "complete original batch chunk header")
        return row
    linked = {original_to_jdi[original]: native for original, native in identities.items()}
    native = {row["id"]: row for row in actual["chunks"]} | {0: actual["fallback"]}
    require(set(native) == set(linked.values()) and actual["owned_count"] == len(linked) - 1,
            "exact native owned identity set at every individual batch boundary")
    expected_live = {"time": state["metadata"]["e"], "spawn": [state["metadata"][letter] for letter in "bcd"],
                     "world_force_load": state["world_force_load"], "provider_force_load": state["provider_force_load"] or False,
                     "saving_disabled": state["save_off"]}
    require(exact(actual["live"], expected_live), "complete individual batch live clock/spawn/policy flags")
    observed = set()
    for row in state["chunks"]:
        if type(row) is dict and set(row) == {"chunk_reference"}:
            index = row["chunk_reference"]
            bounded_int(index, 0, len(trace_source.states) - 1)
            row = trace_source.states[index]
        full = type(row) is dict and "chunk" in row
        current = header(row["chunk"] if full else row)
        identity = current["id"]
        require(identity in linked and identity not in observed, "complete unique batch original identity")
        observed.add(identity)
        value = native[linked[identity]]
        require(exact(value["coordinates"], current["coordinates"]) and exact(value["flags"], current["flags"]) and
                value["last_save_time"] == current["last_save"] and
                (value["blank"] and current["class"] in ("li", "hg") or
                 not value["blank"] and current["class"] in ("lm", "hi") and value["minimum_height"] == current["minimum"]),
                "complete original/native individual batch-call header")
        if full and not value["blank"]:
            require(set(row["arrays"]) == set(ARRAYS), "all individual batch target arrays")
            payload = session.payloads[linked[identity], value["payload_version"]]
            for role, size in ARRAYS.items():
                raw = trace_source.blob(row["arrays"][role]["data"])
                require(len(raw) == size and raw == payload[role], "complete individual batch target bytes")
                source.counts["batch_arrays"] = source.counts.get("batch_arrays", 0) + 1
        source.counts["batch_headers"] = source.counts.get("batch_headers", 0) + 1
    require(observed == set(linked), "every retained individual batch chunk observed")
    expected_map = [[int32(row["key"]), linked[header(row["chunk"])["id"]]] for row in state["map"]["entries"]]
    expected_list = [linked[header(row)["id"]] if type(row) is dict else linked[row]
                     for row in state["insertions"]["values"]]
    require(exact(sorted(actual["map"]), sorted(expected_map)) and
            exact(actual["list"], expected_list) and
            exact(actual["pending"], state["pending"]["values"]), "complete individual batch-call cache/list/pending graph")
    fields = state["metadata"]
    metadata = {name: fields[letter] for name, letter in zip(
        ("seed", "spawn_x", "spawn_y", "spawn_z", "time", "last_played", "size_on_disk", "player", "player_dimension",
         "level_name", "version", "raining", "rain_time", "thundering", "thunder_time"), "abcdefghijklmno")}
    require(exact(actual["info"], info_projection(metadata)), "complete individual batch-call live metadata")
    source.counts["batch_boundaries"] = source.counts.get("batch_boundaries", 0) + 1


def replay_batch(source, trace_source, operation, traced, identities, original_to_jdi, session):
    admission = operation["action"] == "batch_acquire101"
    require(operation["action"] in ("batch_acquire101", "batch_drop101") and
            exact(operation["arguments"], {"first": 1, "last": 101, "multiplier": 16, "z": 0}) and
            operation["signature"] == ("(II)Lhi;" if admission else "(II)V") and
            type(operation["calls"]) is list and len(operation["calls"]) == 101, "explicit complete original setup batch")
    role = "acquire" if admission else "drop"
    returns = [event for event in traced["events"] if event["role"] == role and event["edge"] == "return" and
               event["class"] == operation["class"] and event["method"] == operation["method"] and
               event["signature"] == operation["signature"] and event["receiver"] == original_to_jdi[operation["receiver"]]]
    require(len(returns) == 101, "each batch call has an actual bound original return")
    callbacks, summaries = [], []
    for index, (call, returned) in enumerate(zip(operation["calls"], returns)):
        keys(call, ("index", "arguments", "result"))
        require(type(call["index"]) is int and call["index"] == index and
                exact(call["arguments"], [16 * (index + 1), 0]), "individual original setup batch input")
        command = ("ACQUIRE" if admission else "DROP", *call["arguments"])
        response = session.request(*command)
        require(type(response["status"]) is int and response["status"] == 0, "successful individual native setup call")
        if admission:
            bind_acquired(call["result"], response, identities)
            require(returned["result"]["object_id"] == original_to_jdi[call["result"]["reference"]],
                    "individual helper/JDI/native acquisition identity")
        else:
            require(call["result"] is None and returned["result"] is None, "individual drop void result")
        compare_batch_boundary(source, trace_source, returned["state"], response["state"], identities, original_to_jdi, session)
        callbacks.extend(response["callbacks"])
        summaries.append({"index": index, "command": command, "native_command_index": response["command_index"], "passed": True})
    response = dict(response)
    response["callbacks"] = callbacks
    return response, summaries


def replay_capture(capture, executable, work, report, *, block_map_record=None):
    source = Original(strict_json(read(capture / "observed.json")))
    trace = strict_json(read(capture / "trace.json"))
    trace_source = Original(trace)
    record = source.record
    require(trace["context"] == record["context"] and trace["scenario"] == record["mode"] and
            trace["jar_sha256"] == record["jar_sha256"] and
            len(trace["operations"]) == len(record["operations"]), "same original helper/trace profile")
    registry = source
    if "block_map" not in record:
        require(block_map_record is not None, "actual original registry block map required")
        registry = Original(strict_json(read(block_map_record)))
        require(registry.record["jar_sha256"] == record["jar_sha256"], "same original registry archive")
        report["development_external_registry_record"] = str(block_map_record)
    block_map = registry.blob(registry.record["block_map"])
    require(len(block_map) == 256, "complete actual original block normalization table")
    baseline = capture / "preparation" / "world"
    before = inventory(baseline)
    target = work / "native-world"
    shutil.copytree(baseline, target)
    require(inventory(target) == before and inventory(baseline) == before,
            "immutable complete baseline copied byte for byte")
    report["baseline"] = before
    report["context"], report["scenario"] = record["context"], record["mode"]
    report["record"] = file_fingerprint(capture / "observed.json")
    report["trace"] = file_fingerprint(capture / "trace.json")
    identities = {record["initial"]["fallback"]: 0}
    original_to_jdi = {}
    for row in trace["identity_ids"]:
        require(set(row) == {"reference", "object_id"} and type(row["reference"]) is int and
                type(row["object_id"]) is int and row["reference"] not in original_to_jdi and
                row["object_id"] not in original_to_jdi.values(), "bijective helper/JDI identities")
        original_to_jdi[row["reference"]] = row["object_id"]
    successful = False
    session = Session(executable, work, report["processes"])
    written, operations, semantic_metadata = {}, [], set()
    report["operations"] = operations
    try:
        opened = session.request("OPEN", str(target).encode("utf-8").hex(),
                                 record["initial"]["handler_token"], block_map.hex(), 0,
                                 "dedicated" if record["context"] == "dedicated" else "base")
        require(opened["status"] == 0, "native runtime opens actual original-created baseline")
        actual = opened["state"]
        compare_state(source, record["initial"], actual, identities, session)
        compare_files(source.files(record["initial"]), target, written, source.counts, semantic_metadata)
        native_before = raw_files(target)
        for index, (operation, traced) in enumerate(zip(record["operations"], trace["operations"])):
            require(type(operation["index"]) is int and operation["index"] == index and
                    traced["index"] == index and operation["action"] == traced["action"],
                    "complete contiguous original operation schedule")
            summary = {"index": index, "action": operation["action"], "passed": False}
            operations.append(summary)
            external_before = index > 0 and record["operations"][index - 1]["action"] == "api_owned_membership_add"
            compare_state(source, operation["before"], actual, identities, session,
                          external_membership=external_before)
            chunks = [source.chunk(row) for row in operation["after"]["chunks"]]
            coordinates = {original_to_jdi[row["id"]]: row["coordinates"] for row in chunks}
            stamps, slots = timestamp_inputs(traced, coordinates)
            saves_metadata = metadata_written(traced)
            clock = save_clock(source, operation, saves_metadata)
            session.request("CLOCKS", f"{clock:016x}", stamps[0] if stamps else "00000000")
            session.request("TIMESTAMPS", len(stamps), *stamps)
            if operation["action"] in ("batch_acquire101", "batch_drop101"):
                command = ("BATCH",)
                require(not stamps and not saves_metadata, "setup batch has no original save writes")
                response, summary["calls"] = replay_batch(source, trace_source, operation, traced, identities, original_to_jdi, session)
            else:
                command = operation_command(operation, identities)
                response = session.request(*command)
            summary["command"] = command
            actual = response["state"]
            fatal_session = operation["action"] in ("fatal_cache_save", "fatal_world_save")
            if fatal_session:
                exception = operation["exception"]
                require(type(exception) is dict and set(exception) == {"id", "class", "message"} and
                        exception["class"] == ("us" if record["context"] == "client" else "my") and
                        exception["message"] == "The save is being accessed from another location, aborting" and
                        traced["exception"]["class"] == exception["class"] and
                        traced["exception"]["object_id"] == original_to_jdi[exception["id"]] and
                        response["status"] == 3 and response["error"]["status"] == 6 and
                        actual["last_error"]["status"] == 6 and response["result"] is None and response["completed"] is None,
                        "actual original session exception maps to native fatal/session diagnostic")
            else:
                require(operation["exception"] is None and type(response["status"]) is int and response["status"] == 0,
                        "matching successful original/native API outcome")
            expected_hooks = ["auxiliary" if event["role"] == "auxiliary" else "provider"
                              for event in traced["events"] if event["role"] in ("auxiliary", "provider_maintenance") and
                              event["edge"] == "breakpoint" and event["code_index"] == 0]
            actual_hooks = [event["kind"] for event in response["callbacks"] if event["kind"] in ("auxiliary", "provider")]
            require(exact(actual_hooks, expected_hooks), "actual ordered native/original auxiliary/provider boundaries")
            result = operation["result"]
            if fatal_session:
                require(result is None, "escaping original session failure assigns no result")
            elif command[0] == "COMPETE":
                require(exact(result, {"reference": operation["after"]["competing_handler"]["id"]}),
                        "actual competing original handler identity retained separately from chunks")
            elif command[0] == "ACQUIRE":
                bind_acquired(result, response, identities)
            elif command[0] in ("SAVE", "CLIENT_SAVE"):
                require(type(result) is bool and response["completed"] is result, "exact save cutoff/completion result")
            elif command[0] in ("ELIGIBLE", "MAINTAIN"):
                require(type(result) is bool and response["result"] is result, "exact original/native boolean result")
            else:
                require(result is None, "original void API result")
            require(actual["clocks"]["timestamp_count"] == len(stamps) and
                    actual["clocks"]["timestamp_index"] == len(stamps), "all original timestamp inputs consumed exactly")
            for name, indices in slots.items():
                written.setdefault(name, set()).update(indices)
            native_after = raw_files(target)
            if command[0] == "COMPETE":
                require(native_after["session.lock"] == bytes.fromhex(command[1]) and
                        {name: raw for name, raw in native_before.items() if name != "session.lock"} ==
                        {name: raw for name, raw in native_after.items() if name != "session.lock"},
                        "only real competing constructor session bytes change")
            else:
                preserve_step(native_before, native_after, slots, saves_metadata)
            native_before = native_after
            if saves_metadata:
                source.retained_time = operation["after"]["metadata"]["time"]
                source.retained_spawn = [operation["after"]["metadata"][name] for name in ("spawn_x", "spawn_y", "spawn_z")]
                if "level.dat" in semantic_metadata:
                    semantic_metadata.add("level.dat_old")
                else:
                    semantic_metadata.discard("level.dat_old")
                semantic_metadata.add("level.dat")
            compare_state(source, operation["after"], actual, identities, session,
                          external_membership=operation["action"] == "api_owned_membership_add")
            compare_files(source.files(operation["after"]), target, written, source.counts, semantic_metadata)
            require(NBT(hex_bytes(actual["metadata_hex"], len(actual["metadata_hex"]) // 2)).tree ==
                    NBT(inflate(native_after["level.dat"], True)).tree,
                    "native retained encoded metadata matches actual current disk document")
            summary.update({"passed": True, "main_writes": len(stamps), "metadata_clock": f"{clock:016x}",
                            "native_command_index": response["command_index"]})
        compare_state(source, record["final"], actual, identities, session)
        compare_files(source.files(record["final"]), target, written, source.counts, semantic_metadata)
        report["identity_pairs"] = [[original, native] for original, native in identities.items()]
        report["comparisons"] = source.counts
        successful = True
    finally:
        active_error = sys.exc_info()[0] is not None
        cleanup_errors = []
        pre_eof = None
        try:
            pre_eof = raw_files(target)
        except Exception as error:
            cleanup_errors.append(error)
        try:
            session.finish(successful)
        except Exception as error:
            cleanup_errors.append(error)
        for name, root in (("native_files_after", target), ("baseline_after", baseline)):
            try:
                report[name] = inventory(root)
            except Exception as error:
                cleanup_errors.append(error)
        try:
            require(pre_eof is not None and raw_files(target) == pre_eof,
                    "native EOF/destruction never saves or mutates world files")
            require(report.get("baseline_after") == before, "original preparation baseline preserved")
        except Exception as error:
            cleanup_errors.append(error)
        if cleanup_errors:
            report["cleanup_errors"] = [type(error).__name__ + ": " + str(error) for error in cleanup_errors]
            if not active_error:
                raise cleanup_errors[0]
    return source.counts


def source_paths(strict=False):
    paths = {Path(__file__).resolve(), ROOT / "CMakeLists.txt"}
    for directory in ("include/mc", "src/core", "src/world", "src/storage", "tests/world/runtime", "tests/tools/world"):
        paths.update(path for path in (ROOT / directory).rglob("*") if path.is_file() and
                     path.suffix in (".h", ".c", ".py"))
    if strict:
        paths.add(ROOT / "scripts/reference/cache/lifecycle_controls.py")
    return sorted(paths)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--development", action="store_true",
                        help="retain incomplete original/control proof as development evidence only")
    parser.add_argument("--block-map-record", type=Path,
                        help="development-only original registry record from the same archive")
    args = parser.parse_args(argv)
    report = {"schema": 1, "scope": "native-original-lifecycle-api-and-files", "status": "running",
              "tested_commit": None, "processes": [], "errors": [], "preservation_errors": [],
              "limitations": ["loaded-only dimension0; generation and population are not invoked",
                              "entity membership body is external; only observed stale q is replayed",
                              "blank chunk data is virtual and not allocated by the native owner",
                              "map compares complete membership; insertion and pending orders are exact",
                              "written compression bytes and compound tag order are retained but compared semantically",
                              "native callbacks are checked at API boundaries; original trace has separate validation"]}
    work, inputs, baseline = None, {}, None
    try:
        capture, executable = args.capture.resolve(), args.executable.resolve()
        require(capture.is_dir() and ordinary(capture), "ordinary original capture directory")
        require(args.development or args.block_map_record is None, "external registry record is development-only")
        work = fresh_work(args.work_dir, (capture, executable, Path(sys.executable).resolve()))
        originals = [capture / name for name in ("manifest.json", "observed.json", "control.json", "trace.json")]
        preparation_record = capture / "preparation.json"
        if preparation_record.is_file():
            originals.append(preparation_record)
        else:
            require(args.development, "canonical preparation snapshot required for accepted capture")
            report["development_capture_gaps"] = ["canonical preparation snapshot absent; only retained baseline and operation records replayed"]
        if args.block_map_record is not None:
            originals.append(args.block_map_record.resolve())
        archived_sources = source_paths(not args.development)
        paths = [*archived_sources, *originals, executable, Path(sys.executable).resolve()]
        inputs = {str(path): file_fingerprint(path) for path in paths}
        report["inputs_before"] = inputs
        report["python"] = {"executable": sys.executable, "version": sys.version}
        for path in archived_sources:
            target = work / "source" / path.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
            require(file_fingerprint(target) == inputs[str(path)], "complete source archive")
        if args.development:
            report["original_controls"] = {"status": "unaccepted-development",
                                           "reason": "strict original/control proof not requested"}
        else:
            controls_path = ROOT / "scripts/reference/cache/lifecycle_controls.py"
            controls = load("_lifecycle_controls", controls_path, expected=inputs[str(controls_path)])
            report["original_controls"] = controls.validate_capture(capture)
        baseline = inventory(capture / "preparation" / "world")
        report["original_worlds_before"] = {name: inventory(capture / name / "world") for name in
                                           ("preparation", "observed-world", "control-world")}
        replay_capture(capture, executable, work, report, block_map_record=args.block_map_record)
        report["status"] = "development-pass" if args.development else "pass"
    except Exception as error:
        report["status"] = "fail"
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        if work is not None:
            report["executed_python"] = EXECUTED
            after = {}
            expected_inputs = dict(inputs)
            for name, expected in EXECUTED.items():
                if name in expected_inputs and expected_inputs[name] != expected:
                    report["preservation_errors"].append("executed source differs from archived input: " + name)
                else:
                    expected_inputs[name] = expected
            for name, expected in expected_inputs.items():
                try:
                    after[name] = file_fingerprint(Path(name))
                    require(after[name] == expected, "measured input preserved: " + name)
                except Exception as error:
                    report["preservation_errors"].append(str(error))
            report["inputs_after"] = after
            if baseline is not None:
                try:
                    report["original_worlds_after"] = {name: inventory(capture / name / "world") for name in
                                                       ("preparation", "observed-world", "control-world")}
                    require(report["original_worlds_before"] == report["original_worlds_after"],
                            "all original/control worlds preserved")
                except Exception as error:
                    report["preservation_errors"].append(str(error))
            if report["preservation_errors"]:
                report["status"] = "fail"
            path = work / "report.json"
            path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps({"status": report["status"], "report": str(path),
                              "errors": report["errors"], "preservation_errors": report["preservation_errors"]}))
        else:
            print(json.dumps({"status": "fail", "errors": report["errors"]}))
    return 0 if report["status"] in ("pass", "development-pass") else 1


if __name__ == "__main__":
    raise SystemExit(main())
