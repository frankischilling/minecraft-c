#!/usr/bin/env python3
"""Constructed copied-world lifecycle protocol contracts, without an original oracle."""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("lifecycle_capture", ROOT / "scripts/reference/java/compare_int_set.py")
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)
MAX_STREAM = 4 * 1024 * 1024
MAX_WORLD = 4 * 1024 * 1024
MAX_NBT = 1024 * 1024
TOKEN = "0123456789abcdef"
MAP = bytes(range(256)).hex()
ARRAYS = {"blocks": 32768, "metadata": 16384, "skylight": 16384, "block_light": 16384, "heightmap": 256}
CHECKS = 0


def require(value, message):
    global CHECKS
    CHECKS += 1
    CAPTURE.require(value, message)


def same(left, right):
    return CAPTURE.encoded(left, sorted_keys=True) == CAPTURE.encoded(right, sorted_keys=True)


def keys(value, names, message):
    require(type(value) is dict and set(value) == set(names), message)


def integer(value, low, high):
    require(type(value) is int and low <= value <= high, "exact bounded integer")


def raw_hex(value, length=None, limit=MAX_NBT):
    require(type(value) is str and len(value) % 2 == 0 and len(value) <= limit * 2 and
            re.fullmatch("[0-9a-f]*", value) is not None, "bounded lowercase hex")
    result = bytes.fromhex(value)
    require(length is None or len(result) == length, "complete hex width")
    return result


def long_bits(value):
    return f"{value & ((1 << 64) - 1):016x}"


def ordinary(path):
    require(not path.is_symlink() and not (getattr(path.lstat(), "st_file_attributes", 0) & 0x400),
            "ordinary owned path: " + str(path))


def inventory(root):
    ordinary(root)
    require(root.is_dir(), "owned world directory")
    result, total, entries, files = {}, 0, 0, 0
    for path in sorted(root.rglob("*")):
        entries += 1
        require(entries <= 128, "bounded world entries")
        ordinary(path)
        require(path.is_dir() or path.is_file(), "ordinary directory or file")
        relative = path.relative_to(root).as_posix()
        if path.is_dir():
            result[relative + "/"] = {"directory": True}
        else:
            files += 1
            total += path.stat().st_size
            require(files <= 64 and total <= MAX_WORLD, "bounded complete world inventory")
            result[relative] = CAPTURE.file_fingerprint(path)
    return result


class Reader:
    """Small bounded typed NBT reader for constructed files; no C decoder reuse."""

    def __init__(self, raw):
        require(type(raw) is bytes and len(raw) <= MAX_NBT, "bounded raw NBT")
        self.raw, self.offset, self.tags = raw, 0, 0

    def take(self, count):
        require(0 <= count <= len(self.raw) - self.offset, "complete NBT input")
        result = self.raw[self.offset:self.offset + count]
        self.offset += count
        return result

    def number(self, form):
        return struct.unpack(form, self.take(struct.calcsize(form)))[0]

    def text(self):
        raw = self.take(self.number(">H"))
        units, offset = [], 0
        while offset < len(raw):
            first = raw[offset]
            offset += 1
            if 0 < first < 128:
                value = first
            elif 0xc0 <= first <= 0xdf:
                require(offset < len(raw) and raw[offset] & 0xc0 == 0x80, "MUTF continuation")
                value = ((first & 31) << 6) | (raw[offset] & 63)
                offset += 1
                require(value == 0 or value >= 128, "canonical MUTF pair")
            else:
                require(0xe0 <= first <= 0xef and offset + 1 < len(raw) and
                        raw[offset] & 0xc0 == 0x80 and raw[offset + 1] & 0xc0 == 0x80,
                        "MUTF triple")
                value = ((first & 15) << 12) | ((raw[offset] & 63) << 6) | (raw[offset + 1] & 63)
                offset += 2
                require(value >= 2048, "canonical MUTF triple")
            units.append(value)
        return "".join(f"{unit:04x}" for unit in units)

    def tag(self, kind=None, named=True, depth=1):
        self.tags += 1
        require(depth <= 64 and self.tags <= 65536, "bounded NBT tree")
        kind = self.number(">B") if kind is None else kind
        require(1 <= kind <= 10, "NBT value type")
        name = self.text() if named else ""
        if kind in (1, 2, 3, 4):
            value = self.number({1: ">b", 2: ">h", 3: ">i", 4: ">q"}[kind])
        elif kind in (5, 6):
            value = self.take(4 if kind == 5 else 8).hex()
        elif kind == 7:
            count = self.number(">i")
            require(0 <= count <= MAX_NBT, "bounded NBT byte array")
            value = self.take(count).hex()
        elif kind == 8:
            value = self.text()
        elif kind == 9:
            element, count = self.number(">B"), self.number(">i")
            require(0 <= count <= 65536 and (1 <= element <= 10 or count == 0), "bounded NBT list")
            value = {"element_type": element,
                     "items": [self.tag(element, False, depth + 1) for _ in range(count)]}
        else:
            value = []
            while True:
                element = self.number(">B")
                if element == 0:
                    break
                value.append(self.tag(element, True, depth + 1))
            require(len(value) == len({entry["name_utf16"] for entry in value}), "unique compound names")
        return {"type": kind, "name_utf16": name, "value": value}


def nbt(raw):
    reader = Reader(raw)
    result = reader.tag()
    require(reader.offset == len(raw) and result["type"] == 10, "complete named compound")
    return result


def field(tree, name, kind):
    require(tree["type"] == 10, "compound field owner")
    encoded = name.encode("utf-16-be").hex()
    values = [entry for entry in tree["value"] if entry["name_utf16"] == encoded]
    require(len(values) == 1 and values[0]["type"] == kind, "exact NBT field: " + name)
    return values[0]


def inflate(raw, window):
    decoder = zlib.decompressobj(window)
    result = decoder.decompress(raw, MAX_NBT + 1)
    require(len(result) <= MAX_NBT and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "one bounded complete compressed member")
    return result


def disk_info(root):
    raw = (root / "level.dat").read_bytes()
    require(len(raw) <= MAX_NBT, "bounded metadata file")
    return nbt(inflate(raw, 31))


def disk_chunk(root):
    path = root / "region/r.0.0.mcr"
    image = path.read_bytes()
    require(8192 <= len(image) <= MAX_WORLD and len(image) % 4096 == 0, "complete region image")
    location = struct.unpack_from(">I", image, 16 * 4)[0]
    start, sectors = (location >> 8) * 4096, location & 255
    require(sectors > 0 and start >= 8192 and start + sectors * 4096 <= len(image), "allocated slot16")
    length = struct.unpack_from(">I", image, start)[0]
    require(1 < length <= sectors * 4096 - 4 and image[start + 4] in (1, 2), "complete region payload")
    tree = nbt(inflate(image[start + 5:start + 4 + length], 31 if image[start + 4] == 1 else 15))
    return tree, struct.unpack_from(">I", image, 4096 + 16 * 4)[0], image, start, sectors


def expected_arrays():
    state, blocks = 1, bytearray()
    for _ in range(32768):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        blocks.append(state >> 24)
    return {"blocks": bytes(blocks).hex(), **{key: bytes(size).hex() for key, size in ARRAYS.items() if key != "blocks"}}


def check_chunk_tree(tree, arrays, last_update):
    level = field(tree, "Level", 10)
    for name, value in (("xPos", 16), ("zPos", 0)):
        require(field(level, name, 3)["value"] == value, "constructed saved coordinates")
    require(field(level, "TerrainPopulated", 1)["value"] == 1 and
            field(level, "LastUpdate", 4)["value"] == last_update, "saved population/world clock")
    for key, name in (("blocks", "Blocks"), ("metadata", "Data"), ("skylight", "SkyLight"),
                      ("block_light", "BlockLight"), ("heightmap", "HeightMap")):
        require(field(level, name, 7)["value"] == arrays[key], "all constructed saved array bytes")
    for name in ("Entities", "TileEntities"):
        value = field(level, name, 9)["value"]
        require(value == {"element_type": 1, "items": []}, "known-empty serialized runtime collections")


def check_info_tree(tree, world_time, last_played, size):
    data = field(tree, "Data", 10)
    expected = {"RandomSeed": (4, 23), "SpawnX": (3, 0), "SpawnY": (3, 0), "SpawnZ": (3, 0),
                "Time": (4, world_time), "LastPlayed": (4, last_played), "SizeOnDisk": (4, size),
                "LevelName": (8, "004c006900660065d8000000"), "version": (3, 19132),
                "rainTime": (3, 0), "raining": (1, 0), "thunderTime": (3, 0), "thundering": (1, 0)}
    require(len(tree["value"]) == 1 and len(data["value"]) == len(expected), "complete constructed metadata fields")
    for name, (kind, value) in expected.items():
        require(field(data, name, kind)["value"] == value, "complete constructed metadata: " + name)


def validate_error(value):
    keys(value, ("status", "operation", "system_error"), "diagnostic fields")
    integer(value["status"], 0, 9)
    integer(value["system_error"], 0, 0xffffffff)
    require((value["operation"] is None and value["system_error"] == 0) if value["status"] == 0 else
            type(value["operation"]) is str and 0 < len(value["operation"]) <= 256, "diagnostic representation")


def validate_info(value):
    keys(value, ("seed", "spawn", "time", "last_played", "size_on_disk", "level_name", "version",
                 "rain_time", "raining", "thunder_time", "thundering", "has_player", "player_dimension"),
         "complete metadata view")
    for name in ("seed", "time", "last_played", "size_on_disk"):
        raw_hex(value[name], 8)
    raw_hex(value["level_name"], limit=131070)
    require(len(value["level_name"]) % 4 == 0, "complete UTF16 units")
    require(type(value["spawn"]) is list and len(value["spawn"]) == 3, "spawn triple")
    for coordinate in value["spawn"]:
        integer(coordinate, -(1 << 31), (1 << 31) - 1)
    for name in ("version", "rain_time", "thunder_time", "player_dimension"):
        integer(value[name], -(1 << 31), (1 << 31) - 1)
    for name in ("raining", "thundering", "has_player"):
        require(type(value[name]) is bool, "exact metadata Boolean")


def validate_chunk(value, payloads, blank=False):
    keys(value, ("id", "coordinates", "blank", "flags", "last_save_time", "minimum_height", "payload_version"),
         "complete chunk header")
    integer(value["id"], 0 if blank else 1, 4096)
    require(value["blank"] is blank and type(value["coordinates"]) is list and
            len(value["coordinates"]) == 2, "chunk coordinates/blank type")
    for coordinate in value["coordinates"]:
        integer(coordinate, -(1 << 31), (1 << 31) - 1)
    keys(value["flags"], "cnopq", "all runtime flags")
    require(all(type(bit) is bool for bit in value["flags"].values()), "exact runtime flag Booleans")
    raw_hex(value["last_save_time"], 8)
    if blank:
        require(value["id"] == 0 and value["minimum_height"] is None and value["payload_version"] is None,
                "blank has no fabricated payload")
    else:
        integer(value["minimum_height"], -(1 << 31), (1 << 31) - 1)
        integer(value["payload_version"], 1, 4096)
        require(value["id"] in payloads and value["payload_version"] == payloads[value["id"]][0],
                "chunk references latest complete payload")


def validate_stream(raw, commands):
    require(type(raw) is bytes and len(raw) <= MAX_STREAM and raw.endswith(b"\n") and
            all(byte < 128 for byte in raw), "bounded complete ASCII stream")
    payloads, responses, pending_payloads = {}, [], set()
    largest, counts = 0, {"auxiliary": 0, "provider": 0, "level": 0, "chunks": 0}
    previous_alive, retired = set(), set()
    for line in raw.splitlines(keepends=True):
        value = CAPTURE.strict_json(line)
        require(type(value) is dict, "JSON object record")
        if value.get("kind") == "PAYLOAD":
            keys(value, ("kind", "id", "version", *ARRAYS), "complete payload fields")
            integer(value["id"], 1, 4096)
            integer(value["version"], 1, 4096)
            require(value["id"] not in retired, "retired payload identity never resurrects")
            arrays = {name: raw_hex(value[name], size).hex() for name, size in ARRAYS.items()}
            previous = payloads.get(value["id"])
            if previous is None:
                require(value["id"] == largest + 1 and value["version"] == 1, "monotonic new identity")
                largest = value["id"]
            else:
                require(value["version"] == previous[0] + 1 and not same(arrays, previous[1]),
                        "changed complete payload increments its version")
            require(value["id"] not in pending_payloads, "one payload per identity per response")
            pending_payloads.add(value["id"])
            payloads[value["id"]] = (value["version"], arrays)
            continue
        keys(value, ("kind", "schema", "command_index", "command", "category", "status", "result", "completed",
                     "returned_id", "error", "callbacks", "state"), "complete response fields")
        require(value["kind"] == "RESPONSE" and type(value["schema"]) is int and value["schema"] == 1,
                "response schema")
        index = len(responses)
        require(index < len(commands), "no unsolicited response")
        command = commands[index].split("\t", 1)[0]
        integer(value["command_index"], index + 1, index + 1)
        require(value["command"] == command, "response command binding")
        category = ("acquire" if command in ("ACQUIRE", "GET", "DROP") else "lifecycle" if command in
                    ("SAVE", "WORLD_SAVE", "CLIENT_SAVE", "MAINTAIN") else "storage" if command in
                    ("OPEN", "CLOSE", "COMPETE", "RETIRE") else "adapter")
        require(value["category"] == category, "command status category")
        integer(value["status"], 0, 6 if category == "acquire" else 4 if category == "lifecycle" else 9)
        validate_error(value["error"])
        for name in ("result", "completed"):
            require(value[name] is None or type(value[name]) is bool, "exact optional output Boolean")
        has_result = command in ("MAINTAIN", "ELIGIBLE") and value["status"] == 0
        has_completed = command in ("SAVE", "CLIENT_SAVE") and value["status"] == 0
        require((value["result"] is not None) == has_result and
                (value["completed"] is not None) == has_completed, "only assigned successful outputs")
        require(type(value["callbacks"]) is list and len(value["callbacks"]) <= 16, "bounded callbacks")
        for event in value["callbacks"]:
            keys(event, ("kind", "stage", "ordinal"), "callback fields")
            require(event["kind"] in ("auxiliary", "provider", "progress"), "callback kind")
            if event["kind"] == "progress":
                integer(event["stage"], 0, 1)
                name = "level" if event["stage"] == 0 else "chunks"
            else:
                require(event["stage"] is None, "non-progress stage absent")
                name = event["kind"]
            counts[name] += 1
            integer(event["ordinal"], counts[name], counts[name])
        state = value["state"]
        keys(state, ("open", "dimension", "policy", "clocks", "live", "info", "retained_info", "metadata_hex",
                     "owned_count", "reservation_count", "chunks", "fallback", "map", "list", "pending",
                     "pending_membership", "callback_counts", "last_error", "regions"), "complete state fields")
        require(type(state["open"]) is bool and same(state["callback_counts"], counts), "typed state/callback totals")
        keys(state["clocks"], ("saved_at_ms", "timestamp", "timestamp_count", "timestamp_index", "timestamp_queue"),
             "clock input fields")
        clocks = state["clocks"]
        raw_hex(clocks["saved_at_ms"], 8)
        raw_hex(clocks["timestamp"], 4)
        integer(clocks["timestamp_count"], 0, 256)
        integer(clocks["timestamp_index"], 0, clocks["timestamp_count"])
        require(type(clocks["timestamp_queue"]) is list and len(clocks["timestamp_queue"]) == clocks["timestamp_count"],
                "complete queued timestamps")
        for timestamp in clocks["timestamp_queue"]:
            raw_hex(timestamp, 4)
        integer(state["owned_count"], 0, 256)
        integer(state["reservation_count"], 0, 256 - state["owned_count"])
        require(type(state["chunks"]) is list and len(state["chunks"]) == state["owned_count"], "every owned identity")
        alive = []
        for chunk in state["chunks"]:
            validate_chunk(chunk, payloads)
            alive.append(chunk["id"])
        require(alive == sorted(set(alive)) and pending_payloads <= set(alive), "ordered unique live identity inventory")
        require(not set(alive) & retired, "retired handle never resurrects")
        retired.update(previous_alive - set(alive))
        previous_alive = set(alive)
        pending_payloads.clear()
        keys(state["regions"], ("count", "bytes"), "region owner usage")
        integer(state["regions"]["count"], 0, 256)
        integer(state["regions"]["bytes"], 0, 128 * 1024 * 1024)
        if state["open"]:
            integer(state["dimension"], -1, 0)
            require(state["policy"] in ("base", "dedicated"), "runtime policy")
            keys(state["live"], ("time", "spawn", "world_force_load", "provider_force_load", "saving_disabled"),
                 "complete live scalars")
            raw_hex(state["live"]["time"], 8)
            require(type(state["live"]["spawn"]) is list and len(state["live"]["spawn"]) == 3, "live spawn")
            for coordinate in state["live"]["spawn"]:
                integer(coordinate, -(1 << 31), (1 << 31) - 1)
            for name in ("world_force_load", "provider_force_load", "saving_disabled"):
                require(type(state["live"][name]) is bool, "live flag Boolean")
            validate_info(state["info"])
            validate_info(state["retained_info"])
            overlay = {**state["retained_info"], "time": state["live"]["time"], "spawn": state["live"]["spawn"]}
            require(same(state["info"], overlay), "fresh live metadata overlay retains all other fields")
            nbt(raw_hex(state["metadata_hex"]))
            validate_chunk(state["fallback"], payloads, True)
            validate_error(state["last_error"])
        else:
            require(all(state[name] is None for name in ("dimension", "policy", "live", "info", "retained_info",
                                                        "metadata_hex", "fallback", "last_error")) and
                    state["owned_count"] == state["reservation_count"] == 0 and
                    state["regions"] == {"count": 0, "bytes": 0}, "closed owner has no borrowed views")
        require(type(state["map"]) is list and len(state["map"]) <= 256 and
                type(state["list"]) is list and len(state["list"]) <= 512, "bounded live graph")
        map_keys = []
        for row in state["map"]:
            require(type(row) is list and len(row) == 2, "map row")
            integer(row[0], -(1 << 31), (1 << 31) - 1)
            integer(row[1], 0, largest)
            require(row[1] == 0 or row[1] in alive, "map binds alive identity")
            map_keys.append(row[0])
        require(len(map_keys) == len(set(map_keys)), "unique map keys")
        for identity in state["list"]:
            integer(identity, 0, largest)
            require(identity == 0 or identity in alive, "list binds alive identity")
        for name in ("pending", "pending_membership"):
            require(type(state[name]) is list and len(state[name]) <= 256, "bounded pending representation")
            for key in state[name]:
                integer(key, -(1 << 31), (1 << 31) - 1)
            require(len(state[name]) == len(set(state[name])), "unique pending keys")
        require(set(state["pending"]) == set(state["pending_membership"]), "pending history/membership agree")
        if not state["open"]:
            require(not state["map"] and not state["list"] and not state["pending"], "closed graph empty")
        if command in ("ACQUIRE", "GET") and value["status"] == 0:
            integer(value["returned_id"], 0, largest)
            require(value["returned_id"] == 0 or value["returned_id"] in alive, "returned alive identity")
        else:
            require(value["returned_id"] is None, "error/non-acquisition output preserved")
        responses.append(value)
    require(len(responses) == len(commands) and not pending_payloads, "all complete responses, no trailing payload")
    return responses, payloads


def run_child(executable, commands, name, work, report, deadline, expected_exit=0, request=None):
    request = ("\n".join(commands) + "\n").encode("ascii") if request is None else request
    stdin = work / (name + ".stdin")
    stdin.write_bytes(request)
    try:
        stdout, stderr = CAPTURE.run([str(executable)], request, work=work, name=name,
                                     records=report["processes"], deadline=deadline)
    except CAPTURE.Failure:
        record = report["processes"][-1]
        require(expected_exit != 0 and record["exit"] == expected_exit and record["reaped"] and
                not record["timed_out"] and not record["output_limited"], "only expected rejected child failure")
        stdout, stderr = (work / (name + ".stdout")).read_bytes(), (work / (name + ".stderr")).read_bytes()
    finally:
        if report["processes"] and report["processes"][-1]["name"] == name:
            report["processes"][-1]["stdin"] = CAPTURE.file_fingerprint(stdin)
            report["processes"][-1]["expected_exit"] = expected_exit
    record = report["processes"][-1]
    require(record["exit"] == expected_exit, "exact child status")
    require(stderr.replace(b"\r\n", b"\n") == (b"" if expected_exit == 0 else
            b"lifecycle probe parse, resource or snapshot failure\n"), "no unexpected native diagnostics")
    return stdout


def copied_world(baseline, work, name, report):
    before = inventory(baseline)
    root = work / (name + "-\u00e9-\u4e16\u754c")
    require(not root.exists() and not root.is_symlink(), "fresh copied world")
    shutil.copytree(baseline, root)
    require(inventory(root) == before == inventory(baseline), "complete immutable baseline copy")
    report["copies"][name] = {"root": str(root), "before": before}
    return root


def opening(root, policy="dedicated"):
    return "\t".join(("OPEN", str(root).encode("utf-8").hex(), TOKEN, MAP, "0", policy))


def plan(root):
    return ["CLOCKS\t" + long_bits(900000444) + "\t13579bdf",
            "TIMESTAMPS\t3\t11111111\t22222222\t33333333", opening(root),
            "ELIGIBLE\t0\t0", "ELIGIBLE\t0\t1", "GET\t9\t0", "ACQUIRE\t16\t0", "GET\t32784\t0", "CLOSE",
            "ELIGIBLE\t1\t1", "TIME\t" + long_bits(4000), "ENTITY_HINT\t1\t1", "ELIGIBLE\t1\t1",
            "DIRTY\t1", "SAVE\t1", "ELIGIBLE\t1\t1", "TIME\t" + long_bits(5000), "DIRTY\t1",
            "ENTITY_HINT\t1\t1", "COMPETE\tffffffffffffffff", "SAVE\t1", "COMPETE\t" + TOKEN,
            "WORLD_SAVE\t1", "CLOCKS\t" + long_bits(900000555) + "\t44444444", "ENTITY_HINT\t1\t1",
            "TIME\t" + long_bits(5599), "ELIGIBLE\t1\t0", "TIME\t" + long_bits(5600), "ELIGIBLE\t1\t0",
            "CLIENT_SAVE\t1", "DROP\t16\t0", "MAINTAIN", "RETIRE\t1", "ACQUIRE\t16\t0",
            "DROP\t16\t0", "TIMESTAMPS\t0", "MAINTAIN", "RETIRE\t2", "CLOSE"]


def check_constructed(responses, payloads, arrays, size):
    for index, row in enumerate(responses):
        state = row["state"]
        identity = 1 if 6 <= index <= 31 else 2 if 33 <= index <= 36 else None
        mapped = 1 if 6 <= index <= 30 else 2 if 33 <= index <= 35 else None
        pending = [1048576] if index in (30, 34, 35) else []
        require(state["owned_count"] == (0 if identity is None else 1) and state["reservation_count"] == 0 and
                state["map"] == ([] if mapped is None else [[1048576, mapped]]) and
                state["list"] == ([] if mapped is None else [mapped]) and
                state["pending"] == state["pending_membership"] == pending, "complete constructed graph at every step")
        queued = [] if index == 0 or index >= 35 else ["11111111", "22222222", "33333333"]
        cursor = 0 if index < 14 or index >= 35 else 1 if index < 20 else 2 if index < 22 else 3
        clocks = {"saved_at_ms": long_bits(900000444 if index < 23 else 900000555),
                  "timestamp": "13579bdf" if index < 23 else "44444444", "timestamp_count": len(queued),
                  "timestamp_index": cursor, "timestamp_queue": queued}
        require(same(state["clocks"], clocks), "every explicit clock/queued input state")
        if state["open"]:
            require(state["live"]["spawn"] == [0, 0, 0] and not state["live"]["world_force_load"] and
                    not state["live"]["provider_force_load"] and not state["live"]["saving_disabled"],
                    "constructed live defaults")
            info = state["info"]
            world_time = 5600 if index >= 27 else 5599 if index >= 25 else 5000 if index >= 16 else 4000 if index >= 10 else 3000
            require(state["live"]["time"] == long_bits(world_time), "exact live world time input")
            expected = {"seed": long_bits(23), "spawn": [0, 0, 0], "time": state["live"]["time"],
                        "last_played": long_bits(900000123), "size_on_disk": long_bits(size),
                        "level_name": "004c006900660065d8000000", "version": 19132, "rain_time": 0,
                        "raining": False, "thunder_time": 0, "thundering": False,
                        "has_player": False, "player_dimension": 0}
            require(same(info, expected), "every constructed metadata field")
            metadata_time, last_played = (5000, 900000444) if index >= 22 else (3000, 900000123)
            check_info_tree(nbt(bytes.fromhex(state["metadata_hex"])), metadata_time, last_played, size)
            require(state["retained_info"]["time"] == long_bits(metadata_time) and
                    state["last_error"]["status"] == (6 if index in (20, 21) else 0),
                    "retained metadata clock and unmodified diagnostics")
            require(same(state["fallback"], {"id": 0, "coordinates": [0, 0], "blank": True,
                    "flags": {"c": False, "n": False, "o": False, "p": True, "q": False},
                    "last_save_time": long_bits(0), "minimum_height": None, "payload_version": None}),
                    "complete native blank fallback")
        for chunk in state["chunks"]:
            dirty = identity == 1 and (index == 13 or 17 <= index <= 21)
            entity_hint = identity == 1 and (11 <= index <= 13 or 18 <= index <= 21 or 24 <= index <= 30)
            last_save = 5600 if identity == 2 or index == 31 else 5000 if index >= 20 else 4000 if index >= 14 else 3000
            require(chunk["id"] == identity and chunk["last_save_time"] == long_bits(last_save) and
                    chunk["flags"] == {"c": mapped is not None, "n": True, "o": dirty, "p": False, "q": entity_hint},
                    "every constructed chunk flag/time transition")
            require(chunk["coordinates"] == [16, 0] and chunk["minimum_height"] == 0 and
                    chunk["payload_version"] == 1 and not chunk["flags"]["p"] and chunk["flags"]["n"],
                    "constructed loaded header")
            require(same(payloads[chunk["id"]][1], arrays), "five full constructed array expectations")
    for index, row in enumerate(responses):
        expected = 9 if index == 8 else 3 if index == 20 else 1 if index == 29 else 0
        require(row["status"] == expected, "constructed command result at " + str(index))
    if len(responses) > 5:
        require(responses[3]["result"] is False and responses[4]["result"] is False and
                responses[5]["returned_id"] == 0 and not responses[5]["state"]["map"], "denied blank and eligibility")
    if len(responses) > 8:
        require(responses[6]["returned_id"] == responses[7]["returned_id"] == 1 and
                same(responses[7]["state"], responses[8]["state"]), "packed alias hit and BUSY-close preservation")
    if len(responses) > 14:
        chunk = responses[14]["state"]["chunks"][0]
        require(responses[9]["result"] is False and responses[12]["result"] is True and
                chunk["flags"] == {"c": True, "n": True, "o": False, "p": False, "q": False} and
                chunk["last_save_time"] == long_bits(4000) and responses[14]["completed"] is True,
                "force eligibility/empty serializer q reset/dirty reset/stamp")
    if len(responses) > 20:
        state = responses[20]["state"]
        require(state["last_error"]["status"] == 6 and state["chunks"][0]["flags"]["q"] and
                state["chunks"][0]["flags"]["o"] and state["chunks"][0]["last_save_time"] == long_bits(5000) and
                state["clocks"]["timestamp_index"] == 2 and responses[20]["completed"] is None,
                "session failure retains pre-callback stamp and dirty/q while clock input advances")
    if len(responses) > 22:
        row = responses[22]
        require([event["kind"] for event in row["callbacks"]] == ["progress", "auxiliary", "progress"] and
                [event["stage"] for event in row["callbacks"]] == [0, None, 1] and
                row["completed"] is None and row["state"]["clocks"]["timestamp_index"] == 3,
                "outer world save callback ordering and discarded completion")
    if len(responses) > 31:
        require(responses[26]["result"] is False and responses[28]["result"] is True,
                "exact 600-tick incremental threshold")
        state = responses[31]["state"]
        require(responses[30]["state"]["pending"] == [1048576] and responses[31]["result"] is False and
                state["owned_count"] == 1 and not state["map"] and not state["list"] and not state["pending"] and
                not state["chunks"][0]["flags"]["c"] and not state["chunks"][0]["flags"]["q"] and
                state["chunks"][0]["last_save_time"] == long_bits(5600), "native unload retains inactive owner")
    if len(responses) > 33:
        require(not responses[32]["state"]["chunks"] and responses[33]["returned_id"] == 2 and
                responses[33]["state"]["chunks"][0]["id"] == 2 and set(payloads) == {1, 2},
                "explicit retirement and new identity on reacquisition")
    if len(responses) == 39:
        require(responses[-1]["state"]["open"] is False and responses[-1]["status"] == 0 and
                responses[-1]["state"]["callback_counts"] == {"auxiliary": 1, "provider": 2, "level": 1, "chunks": 1},
                "complete explicit teardown and callback counts")


def inspect_files(root, baseline, arrays, size, timestamp, world_time, metadata_time, last_played, token):
    after, before = inventory(root), inventory(baseline)
    require(set(after) == set(before), "no leaked temporary/new files")
    allowed = {"session.lock", "region/r.0.0.mcr", "level.dat", "level.dat_old"}
    require(all(after[name] == before[name] for name in before if name not in allowed), "all unrelated files preserved")
    require((root / "session.lock").read_bytes() == bytes.fromhex(token), "exact constructor session bytes")
    tree, clock, image, start, sectors = disk_chunk(root)
    check_chunk_tree(tree, arrays, world_time)
    require(clock == timestamp, "actual committed timestamp header cell")
    _, _, original, original_start, original_sectors = disk_chunk(baseline)
    require(start == original_start and sectors == original_sectors and len(image) == len(original) and
            image[:4096] == original[:4096] and image[4096:4096 + 16 * 4] == original[4096:4096 + 16 * 4] and
            image[4096 + 17 * 4:start] == original[4096 + 17 * 4:start] and
            image[start + sectors * 4096:] == original[start + sectors * 4096:],
            "same-run write preserves allocation and every unrelated header/dead sector byte")
    check_info_tree(disk_info(root), metadata_time, last_played, size)
    if metadata_time == 3000:
        require((root / "level.dat").read_bytes() == (baseline / "level.dat").read_bytes() and
                (root / "level.dat_old").read_bytes() == (baseline / "level.dat_old").read_bytes(),
                "chunk-only/failing save never writes metadata")
    else:
        require((root / "level.dat_old").read_bytes() == (baseline / "level.dat").read_bytes(), "checked metadata rotation")
    return after


def negative_guards(raw, commands):
    documents = [CAPTURE.strict_json(line) for line in raw.splitlines()]
    response = next(index for index, row in enumerate(documents) if row.get("kind") == "RESPONSE" and row["state"]["chunks"])
    controls = []
    mutations = ("bool-status", "float-coordinate", "integer-flag", "unknown-map-id", "missing-info-field", "short-payload")
    for name in mutations:
        changed = copy.deepcopy(documents)
        row = changed[response]
        if name == "bool-status":
            row["status"] = False
        elif name == "float-coordinate":
            row["state"]["chunks"][0]["coordinates"][0] = 16.0
        elif name == "integer-flag":
            row["state"]["chunks"][0]["flags"]["c"] = 1
        elif name == "unknown-map-id":
            row["state"]["map"][0][1] = 4096
        elif name == "missing-info-field":
            del row["state"]["info"]["rain_time"]
        else:
            next(item for item in changed if item.get("kind") == "PAYLOAD")["heightmap"] = "00"
        try:
            validate_stream(b"".join(CAPTURE.encoded(item) for item in changed), commands)
        except CAPTURE.Failure:
            controls.append(name)
        else:
            raise CAPTURE.Failure("typed validation accepted " + name)
    for raw_json in (b'{"kind":"RESPONSE","kind":"PAYLOAD"}', b'{"value":NaN}', b'{"value":Infinity}'):
        try:
            CAPTURE.strict_json(raw_json)
        except CAPTURE.Failure:
            controls.append("strict-json")
        else:
            raise CAPTURE.Failure("strict JSON rejection guard accepted")
    return controls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--prepare-executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    work, baseline, inputs = None, None, {}
    report = {"schema": 1, "scope": "constructed native lifecycle protocol/integration contracts; no original oracle",
              "tested_commit": None, "passed": False, "processes": [], "copies": {}, "preservation_errors": [],
              "limits": {"child_seconds": 30, "total_seconds": 90, "stream_bytes": MAX_STREAM,
                         "world_files": 64, "world_bytes": MAX_WORLD}}
    exit_status = 1
    try:
        for path in (args.executable, args.prepare_executable):
            ordinary(path)
            require(path.is_file(), "native executable exists")
        executable, preparation = args.executable.resolve(), args.prepare_executable.resolve()
        require(executable != preparation, "distinct protocol and preparation executables")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = {"executable": executable, "prepare_executable": preparation, "python": Path(sys.executable).resolve()}
        sources = {Path(__file__).resolve(), Path(SPEC.origin).resolve(), ROOT / "CMakeLists.txt",
                   ROOT / "tests/world/runtime/lifecycle_probe.c", ROOT / "tests/world/runtime/test_runtime.c"}
        sources.update(ROOT.glob("src/**/*.c"))
        sources.update(ROOT.glob("src/**/*.h"))
        sources.update(ROOT.glob("include/**/*.h"))
        paths.update({"source:" + path.relative_to(ROOT).as_posix(): path for path in sorted(sources)})
        inputs = {name: (path, CAPTURE.file_fingerprint(path)) for name, path in paths.items()}
        report["inputs"] = {name: {"path": str(path), **fingerprint} for name, (path, fingerprint) in inputs.items()}
        for name, (path, _) in inputs.items():
            if name.startswith("source:"):
                target = work / "sources" / name.removeprefix("source:")
                target.parent.mkdir(parents=True, exist_ok=True)
                source = path.read_bytes()
                require(CAPTURE.fingerprint(source) == inputs[name][1], "archived source matches before fingerprint")
                target.write_bytes(source)
        deadline = time.monotonic() + 90
        baseline = work / "baseline-\u00e9-\u4e16\u754c"
        require(not baseline.exists() and not baseline.is_symlink(), "fresh preparation root")
        command = [str(preparation), "ownership-and-runtime", str(baseline).encode("utf-8").hex()]
        (work / "prepare.stdin").write_bytes(b"")
        stdout, stderr = CAPTURE.run(command, work=work, name="prepare", records=report["processes"], deadline=deadline)
        report["processes"][-1]["stdin"] = CAPTURE.file_fingerprint(work / "prepare.stdin")
        report["processes"][-1]["expected_exit"] = 0
        match = re.fullmatch(rb"PASS: ([0-9]+) constructed runtime/owner checks \(ownership-and-runtime\)\r?\n", stdout)
        require(not stderr and match is not None and int(match[1]) >= 800, "complete successful native preparation")
        report["preparation_checks"] = int(match[1])
        report["baseline"] = inventory(baseline)
        arrays = expected_arrays()
        _, timestamp, image, _, _ = disk_chunk(baseline)
        size = len(image)
        check_chunk_tree(disk_chunk(baseline)[0], arrays, 3000)
        require(timestamp == 0xfedcba98 and (baseline / "session.lock").read_bytes().hex() == TOKEN,
                "constructed preparation final clocks/token")
        check_info_tree(disk_info(baseline), 3000, 900000123, size)
        final_raw, final_commands = None, None
        phases = (("eof-dirty", 14), ("chunk-save", 15), ("session-failure", 21), ("world-save", 23), ("complete", 39))
        for name, count in phases:
            root = copied_world(baseline, work, name, report)
            commands = plan(root)[:count]
            raw = run_child(executable, commands, name, work, report, deadline)
            responses, payloads = validate_stream(raw, commands)
            check_constructed(responses, payloads, arrays, size)
            if name == "eof-dirty":
                require(inventory(root) == report["baseline"], "EOF cleanup and BUSY close perform no saves/file mutations")
            else:
                values = ((0x11111111, 4000, 3000, 900000123, TOKEN) if name == "chunk-save" else
                          (0x11111111, 4000, 3000, 900000123, "ffffffffffffffff") if name == "session-failure" else
                          (0x33333333, 5000, 5000, 900000444, TOKEN) if name == "world-save" else
                          (0x44444444, 5600, 5000, 900000444, TOKEN))
                inspect_files(root, baseline, arrays, size, *values)
            report["copies"][name]["after"] = inventory(root)
            report["copies"][name]["responses"] = len(responses)
            if name == "complete":
                final_raw, final_commands = raw, commands
        root = copied_world(baseline, work, "base-client", report)
        commands = [opening(root, "base"), "ACQUIRE\t16\t0", "GET\t17\t0", "TIME\t" + long_bits(4000), "DIRTY\t1",
                    "ENTITY_HINT\t1\t1", "CLOCKS\t" + long_bits(900000666) + "\tabcdef01", "CLIENT_SAVE\t0", "SAVE\t0",
                    "ENTITY_HINT\t1\t1", "TIME\t" + long_bits(4599), "ELIGIBLE\t1\t0", "TIME\t" + long_bits(4600),
                    "ELIGIBLE\t1\t0", "CLIENT_SAVE\t-1", "RETIRE\t1", "CLOSE"]
        rows, payloads = validate_stream(run_child(executable, commands, "base-client", work, report, deadline), commands)
        for index, row in enumerate(rows):
            check_info_tree(nbt(bytes.fromhex(row["state"]["metadata_hex"])),
                            4000 if index >= 7 else 3000, 900000666 if index >= 7 else 900000123, size)
        require(all(row["status"] == (4 if index == 2 else 9 if index in (15, 16) else 0)
                    for index, row in enumerate(rows)) and rows[2]["error"]["status"] == 2 and
                rows[2]["returned_id"] is None and same(rows[2]["state"]["map"], rows[1]["state"]["map"]) and
                same(rows[3]["state"]["last_error"], rows[2]["state"]["last_error"]),
                "base load-only miss is fatal without fallback; live-info query preserves diagnostics")
        require(rows[7]["completed"] is True and rows[7]["state"]["chunks"][0]["last_save_time"] == long_bits(4000) and
                not rows[7]["state"]["chunks"][0]["flags"]["q"] and
                [event["kind"] for event in rows[7]["callbacks"]] == ["auxiliary"] and
                rows[8]["completed"] is True, "base client metadata+incremental route")
        require(rows[11]["result"] is False and rows[13]["result"] is True and rows[14]["completed"] is True and
                not rows[14]["callbacks"] and rows[14]["state"]["chunks"][0]["last_save_time"] == long_bits(4600) and
                not rows[14]["state"]["chunks"][0]["flags"]["q"] and
                rows[14]["state"]["retained_info"]["time"] == long_bits(4000) and
                same(rows[14]["state"], rows[15]["state"]) and same(rows[15]["state"], rows[16]["state"]),
                "negative client pass skips metadata and both BUSY operations preserve full state")
        require(same(payloads[1][1], arrays), "base load retains all constructed array bytes")
        inspect_files(root, baseline, arrays, size, 0xabcdef01, 4600, 4000, 900000666, TOKEN)
        report["copies"]["base-client"]["after"] = inventory(root)
        bad_requests = {"unknown": b"UNKNOWN\n", "unknown-repeat": b"UNKNOWN\nUNKNOWN\n",
                        "bad-hex": b"TIME\tzzzzzzzzzzzzzzzz\n", "bad-bool": b"SAVE\t2\n",
                        "short-queue": b"TIMESTAMPS\t2\t00000001\n", "bad-eligibility": b"ELIGIBLE\t1\t2\n",
                        "bad-session": b"COMPETE\t0\n", "overlong": b"X" * 4097 + b"\n",
                        "partial-eof": b"DIRTY\t1"}
        for name, bad in bad_requests.items():
            root = copied_world(baseline, work, "reject-" + name, report)
            prefix = [opening(root), "ACQUIRE\t16\t0", "TIME\t" + long_bits(9999), "DIRTY\t1", "ENTITY_HINT\t1\t1"]
            request = ("\n".join(prefix) + "\n").encode("ascii") + bad
            raw = run_child(executable, prefix, "reject-" + name, work, report, deadline, 2, request)
            rows, payloads = validate_stream(raw, prefix)
            require(rows[-1]["state"]["chunks"][0]["flags"]["o"] and
                    rows[-1]["state"]["chunks"][0]["flags"]["q"] and
                    same(payloads[1][1], arrays), "complete retained partial responses before rejection")
            require(inventory(root) == report["baseline"], "failed parse teardown never saves dirty live state")
            report["copies"]["reject-" + name]["after"] = inventory(root)
        require(run_child(executable, [], "reject-empty", work, report, deadline, 2, b"") == b"", "empty input rejected")
        report["typed_negative_guards"] = negative_guards(final_raw, final_commands)
        report["passed"] = True
        exit_status = 0
    except (CAPTURE.Failure, OSError, ValueError, KeyError, TypeError, struct.error, zlib.error) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
    finally:
        if work is not None:
            after = {}
            for name, (path, before) in inputs.items():
                try:
                    after[name] = CAPTURE.file_fingerprint(path)
                    require(after[name] == before, "input unchanged: " + name)
                except (CAPTURE.Failure, OSError) as error:
                    report["preservation_errors"].append(str(error))
            report["inputs_after"] = after
            if baseline is not None and baseline.exists() and "baseline" in report:
                try:
                    report["baseline_after"] = inventory(baseline)
                    require(report["baseline_after"] == report["baseline"], "immutable complete baseline preserved")
                except (CAPTURE.Failure, OSError) as error:
                    report["preservation_errors"].append(str(error))
            if report["preservation_errors"]:
                report["passed"] = False
                exit_status = 1
            report["checks"] = CHECKS
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print("Capture: " + str(work))
    if exit_status == 0:
        print(f"PASS: {CHECKS} constructed lifecycle protocol checks; no original-oracle claim")
    return exit_status


if __name__ == "__main__":
    sys.exit(main())
