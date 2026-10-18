#!/usr/bin/env python3
"""Compare bounded in-memory region writes with untouched original region classes.

Global addressing, cache lifetime and files remain separate original-only evidence.
The C boundary receives observed compressed bytes and explicit timestamp bits.
"""
from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
import uuid
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
MAX_RECORD = 32 * 1024 * 1024
MAX_OUTPUT = 128 * 1024 * 1024
MAX_FILE = 4 * 1024 * 1024
MAX_FIXTURE = 8 * 1024 * 1024
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
JARS = {side: (None, value) for side, value in JAR_HASHES.items()}
ORIGINAL_CLASS_MANIFEST_SHA = "6af2a6fcd8175bfad9772c8e0d86fea09d5eec0bf20aa55ddaa416415779ccc8"
JAVA_SOURCE = "tests/reference/storage/RegionWriteObservation.java"
SOURCES = ("CMakeLists.txt", "include/mc/storage/region/region.h", "include/mc/storage/region/writer.h",
           "src/storage/region/region.c", "src/storage/region/writer.c", "tests/storage/region/test_writer.c",
           "tests/storage/region/writer_probe.c", JAVA_SOURCE, "scripts/reference/storage/compare_region_write.py",
           "scripts/reference/storage/export_region_write.py", "tests/tools/storage/test_region_write.py")
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")
LABELS = ("A-first-stream", "B-exact-two", "C-three", "B-shrink-gap", "D-reuse-gap", "A-same-sector-rewrite",
          "C-grow-four", "E-reuse-two", "F-below-multiple", "G-exact-grow-two", "reject-256-new", "reject-256-existing")
IMAGE_PATH = "allocation/region/r.0.0.mcr"
CASE_LAYOUT = (("client", "fresh"), ("client", "import-copy"), ("server", "fresh"), ("server", "import-copy"))
_latest_process = None

class Failure(Exception):
    pass

class Unavailable(Exception):
    pass

def check(condition, message):
    if not condition:
        raise Failure(message)

require = check

def keys(value, expected, label):
    check(type(value) is dict and set(value) == set(expected), label + ": complete keys")

def integer(value, low, high, label):
    check(type(value) is int and low <= value <= high, label + ": exact integer bounds")

def same_json(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return set(left) == set(right) and all(same_json(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(same_json(a, b) for a, b in zip(left, right))
    return left == right

def strict_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            check(key not in result, "duplicate JSON key")
            result[key] = value
        return result
    def finite(value):
        number = float(value)
        check(math.isfinite(number), "finite JSON value")
        return number
    try:
        return json.loads(raw, object_pairs_hook=pairs, parse_float=finite,
                          parse_constant=lambda _: (_ for _ in ()).throw(Failure("nonfinite JSON value")))
    except (UnicodeError, json.JSONDecodeError) as failure:
        raise Failure("invalid complete JSON") from failure

def safe_relative(value):
    check(type(value) is str and value and not any(c in value for c in "\\:\0") and
          not value.startswith("/") and all(p not in ("", ".", "..") for p in value.split("/")), "safe relative artifact path")
    return value

def checked_bytes(path, limit=MAX_RECORD):
    path = Path(path)
    check(path.is_file() and not path.is_symlink(), "ordinary retained file: " + str(path))
    for parent in path.parents:
        check(not parent.is_symlink(), "artifact path has no symbolic-link components")
    check(path.stat().st_size <= limit, "bounded retained file")
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    check(len(raw) <= limit and len(raw) == path.stat().st_size, "complete stable bounded file")
    return raw

def fingerprint(path, limit=MAX_RECORD):
    raw = path.read_bytes() if isinstance(path, Evidence) else checked_bytes(path, limit)
    check(len(raw) <= limit, "bounded fingerprint bytes")
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}

def blob(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}

def validate_fingerprint(value, limit=MAX_RECORD, minimum=0):
    keys(value, ("bytes", "sha256"), "complete fingerprint")
    integer(value["bytes"], minimum, limit, "fingerprint bytes")
    check(type(value["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", value["sha256"]), "fingerprint SHA256")
    return value

def inflate(raw, limit):
    stream = zlib.decompressobj()
    try:
        result = stream.decompress(raw, limit + 1)
    except zlib.error as failure:
        raise Failure("invalid compressed evidence") from failure
    check(len(result) <= limit and stream.eof and not stream.unused_data and not stream.unconsumed_tail,
          "one complete bounded zlib stream")
    return result

class Evidence:
    """Same bounded artifact lookup for private captures and lossless public blobs."""
    def __init__(self, root=None, mapping=None, relative=""):
        self.root, self.mapping, self.relative = root, mapping, relative
    def __truediv__(self, item):
        value = safe_relative(str(item))
        return Evidence(self.root, self.mapping, self.relative + ("/" if self.relative else "") + value)
    def read_bytes(self):
        if self.mapping is not None:
            check(self.relative in self.mapping, "complete attributed public evidence bytes: " + self.relative)
            return self.mapping[self.relative]
        return checked_bytes(self.root / self.relative, MAX_FILE)
    def __str__(self):
        return self.relative

def read_region(path):
    raw = path.read_bytes()
    check(8192 <= len(raw) <= 4 * 1024 * 1024 and len(raw) % 4096 == 0, "complete aligned original region image")
    return raw, list(struct.unpack(">1024I", raw[:4096])), list(struct.unpack(">1024I", raw[4096:8192]))


def signed(value):
    return value if value < 0x80000000 else value - 0x100000000


def written_ranges(event):
    return {second & 0xffffffff for second in range(event["before_ms"] // 1000, event["after_ms"] // 1000 + 1)}


def audit(record, case):
    check(record["schema"] == 1 and record["side"] in JARS and record["mode"] in ("observed", "control"), "complete probe identity")
    check(record["jar_sha256"] == JARS[record["side"]][1], "original archive attribution")
    check(len(record["events"]) < 1000 and record["final_state"]["cache_count"] == 0, "bounded complete sequence and closed cache")
    snapshots = {row["label"]: row for row in record["events"] if row["kind"] == "snapshot"}
    operation = {row["label"]: row for row in record["events"] if row["kind"] in ("stream_close", "direct_commit")}
    growth = {row["label"]: row["result"] for row in record["events"] if row["kind"] == "growth"}
    for row in record["events"]:
        if row["kind"] in ("stream_close", "direct_commit"):
            check(row["before_ms"] <= row["after_ms"] and row["after_ms"] - row["before_ms"] < 5000, "bounded observed commit clock interval")
            for key in ("plain", "compressed"):
                if key in row:
                    suffix = ".plain" if key == "plain" else ".zlib"
                    path = case / "snapshots/inputs" / (row[key]["sha256"] + suffix)
                    check(fingerprint(path) == row[key], "retained actual complete input bytes")
            if "compressed" in row:
                encoded = (case / "snapshots/inputs" / (row["compressed"]["sha256"] + ".zlib")).read_bytes()
                check(blob(inflate(encoded, MAX_FILE)) == row["plain"], "constructed protected input is valid complete zlib")
    previous_regions = {}
    for snapshot in snapshots.values():
        directory = case / "snapshots" / snapshot["directory"]
        for name, expected in snapshot["files"].items():
            check(fingerprint(directory / name) == expected, "retained raw before/after snapshot")
        for region in snapshot["state"]["held_regions"]:
            raw, locations, timestamps = read_region(directory / region["path"])
            if region["handle_valid"]:
                check(list(map(signed, locations)) == region["locations"] and list(map(signed, timestamps)) == region["timestamps"], "live raw header matches actual original descriptor arrays")
            elif region["object"] in previous_regions:
                expected_closed = copy.deepcopy(previous_regions[region["object"]])
                expected_closed["handle_valid"] = False
                check(same_json(region, expected_closed), "closed region retains its own prior arrays and allocation state")
            occupied = {0, 1}
            for location in region["locations"]:
                if location:
                    start, count = location >> 8, location & 255
                    check(start >= 2 and count > 0 and start + count <= len(region["free"]), "valid original allocation range")
                    allocated = set(range(start, start + count))
                    check(not occupied & allocated, "disjoint original allocations")
                    occupied.update(allocated)
            check(region["free"] == [i not in occupied for i in range(len(region["free"]))], "actual original free-sector vector")
            previous_regions[region["object"]] = copy.deepcopy(region)
    for name, expected in record["final_files"].items():
        check(fingerprint(case / "snapshots/final" / name) == expected, "complete final raw region image")
    latest_owner = {row["path"]: row["object"] for row in record["final_state"]["held_regions"]}
    for region in record["final_state"]["held_regions"]:
        raw, locations, timestamps = read_region(case / "snapshots/final" / region["path"])
        if latest_owner[region["path"]] == region["object"]:
            check(list(map(signed, locations)) == region["locations"] and list(map(signed, timestamps)) == region["timestamps"], "complete final latest original descriptor arrays")
        if region["object"] in previous_regions:
            check(same_json(region, previous_regions[region["object"]]), "complete retained final region owners")
    check(all(not row["handle_valid"] for row in record["final_state"]["held_regions"]), "all original file descriptors closed")

    expected_names = {f"mapping/region/r.{x >> 5}.{z >> 5}.mcr" for x, z in
                      [(0, 0), (31, 31), (32, 32), (-1, -1), (-32, -32), (-33, -33), (-1, 32),
                       (-2147483648, 2147483647), (2147483647, -2147483648)]}
    check({name for name in record["final_files"] if name.startswith("mapping/")} == expected_names, "negative/extreme global filename mapping")
    for name in expected_names:
        check((case / "snapshots/final" / name).read_bytes() == bytes(8192), "fresh actual empty headers")
    cache_names = {f"cache/region/r.{i}.0.mcr" for i in range(257)}
    check({name for name in record["final_files"] if name.startswith("cache/")} == cache_names, "complete257-region cache corpus")
    for name in cache_names:
        check((case / "snapshots/final" / name).read_bytes() == bytes(8192), "cache corpus complete unchanged empty headers")
    check(growth["initial-growth"] == 8192 and growth["initial-reset"] == 0 and growth["reopen-growth"] == 0, "initial/consumed/reopened growth counter")

    # Constants chosen to distinguish the known allocator branches, not a copied allocator implementation.
    expected_steps = [("A-first-stream",0,2,1,3,4096),("B-exact-two",1,3,2,5,8192),
        ("C-three",2,5,3,8,12288),("B-shrink-gap",1,3,1,8,0),("D-reuse-gap",3,4,1,8,0),
        ("A-same-sector-rewrite",0,2,1,8,0),("C-grow-four",2,8,4,12,16384),
        ("E-reuse-two",4,5,2,12,0),("F-below-multiple",5,7,1,12,0),("G-exact-grow-two",6,12,2,14,8192)]
    file_name = "allocation/region/r.0.0.mcr"
    expected_payloads, latest_timestamps = {}, {}
    previous = bytes(8192)
    for label, index, sector, count, file_sectors, increase in expected_steps:
        row = operation[label]
        check(growth[label + "-growth"] == increase and growth[label + "-reset"] == 0, "discriminating growth/consume/reset: " + label)
        expected_payloads[index] = row["plain"]
        latest_timestamps[index] = written_ranges(row)
        if snapshots:
            snap = snapshots[label]
            raw, locations, timestamps = read_region(case / "snapshots" / snap["directory"] / file_name)
            check(len(raw) == file_sectors * 4096 and locations[index] == sector * 256 + count, "discriminating location and allocation: " + label)
            for slot, payload in expected_payloads.items():
                start = (locations[slot] >> 8) * 4096
                length = struct.unpack_from(">I", raw, start)[0]
                check(raw[start + 4] == 2, "actual written compression marker2")
                decoded = inflate(raw[start + 5:start + 4 + length], MAX_FILE)
                check({"bytes": len(decoded), "sha256": hashlib.sha256(decoded).hexdigest()} == payload, "all current/unrelated original payloads retained")
                check(timestamps[slot] in latest_timestamps[slot], "actual wallclock timestamp within original commit interval")
            start, length = sector * 4096, struct.unpack_from(">I", raw, sector * 4096)[0]
            allowed = [(index * 4, index * 4 + 4), (4096 + index * 4, 4100 + index * 4), (start, start + length + 4)]
            check(all(a == b or any(begin <= i < end for begin, end in allowed) for i, (a, b) in enumerate(zip(previous, raw))), "all unrelated original bytes preserved")
            previous = raw
    final, locations, timestamps = read_region(case / "snapshots/final" / file_name)
    check(len(final) == 14 * 4096 and locations[:8] == [513,769,2052,1025,1282,1793,3074,0], "complete final original allocations")
    for slot, payload in expected_payloads.items():
        start = (locations[slot] >> 8) * 4096
        length = struct.unpack_from(">I", final, start)[0]
        decoded = inflate(final[start + 5:start + 4 + length], MAX_FILE)
        check({"bytes": len(decoded), "sha256": hashlib.sha256(decoded).hexdigest()} == payload, "complete final original payload")
        check(timestamps[slot] in latest_timestamps[slot], "final observed/control timestamp within exact original commit interval")
    check(all(value == 0 for value in locations[7:] + timestamps[7:]), "all untouched final header slots preserved")
    for region in record["final_state"]["held_regions"]:
        if region["path"] == file_name:
            for slot, timestamp in enumerate(region["timestamps"]):
                if region["locations"][slot]:
                    check(slot in latest_timestamps and (timestamp & 0xffffffff) in latest_timestamps[slot], "each closed/reopened owner timestamp independently bound to its original write interval")
                else:
                    check(timestamp == 0, "unwritten owner timestamp remains zero")
    check(operation["F-below-multiple"]["compressed"]["bytes"] == 4090 and
          operation["B-exact-two"]["compressed"]["bytes"] == 4091, "adjacent exact-multiple allocation inputs")
    for label in ("reject-256-new", "reject-256-existing"):
        check(operation[label]["compressed"]["bytes"] == 1044475 and growth[label + "-growth"] == growth[label + "-reset"] == 0, ">=256-sector early return")
        if snapshots:
            raw = (case / "snapshots" / snapshots[label]["directory"] / file_name).read_bytes()
            check(raw == previous, "oversized commit leaves entire file byte-identical")
    before = next(row for row in record["events"] if row["kind"] == "before_close" and row["label"] == "A-first-stream")
    check(before["present"] is False, "first output not committed before close")
    if snapshots:
        check((case / "snapshots" / snapshots["A-first-stream-before-close"]["directory"] / file_name).read_bytes() == bytes(8192), "no file mutation before first output close")
        full, hit, cleared = (snapshots[name]["state"] for name in ("cache-full-256", "cache-hit-at-256", "cache-miss-257"))
        check(full["cache_count"] == hit["cache_count"] == 256 and cleared["cache_count"] == 1, "256 live entries; hit retained; next miss clears whole cache")
        old_ids = {row["object"] for row in full["cache_entries_raw_order"]}
        check(all(not row["handle_valid"] for row in cleared["held_regions"] if row["object"] in old_ids), "whole-cache clear closes all256 old handles")
        check(all(row["handle_valid"] for row in full["held_regions"] if row["object"] in old_ids), "all256 live handles retained before clear")
    return {"protocol_actions": sum(row["kind"] != "snapshot" for row in record["events"]), "snapshots": len(snapshots),
            "final_files": len(record["final_files"]), "allocation_bytes": record["final_files"][file_name]["bytes"],
            "checks": "global paths/empty headers/close commit/allocator branches/timestamps/growth/unrelated bytes/cache lifetime"}


def compare(a, a_case, b, b_case):
    def calls(record):
        result = []
        for original in record["events"]:
            if original["kind"] == "snapshot":
                continue
            row = copy.deepcopy(original)
            row.pop("before_ms", None); row.pop("after_ms", None)
            result.append(row)
        return result
    check(calls(a) == calls(b), "identical original method sequence, arguments, identities and results")
    states = []
    for record in (a, b):
        state = copy.deepcopy(record["final_state"])
        for region in state["held_regions"]:
            if region["path"] == "allocation/region/r.0.0.mcr":
                region["timestamps"] = ["range-checked-written-slot" if region["locations"][i] else value for i, value in enumerate(region["timestamps"])]
        states.append(state)
    check(states[0] == states[1], "complete final original state except individually range-checked timestamp slots")
    check(set(a["final_files"]) == set(b["final_files"]), "same complete original file set")
    differences = []
    for name in a["final_files"]:
        left = (a_case / "snapshots/final" / name).read_bytes()
        right = (b_case / "snapshots/final" / name).read_bytes()
        check(len(left) == len(right), "same complete file length")
        slots = set(range(7)) if name == "allocation/region/r.0.0.mcr" else set()
        for i, (x, y) in enumerate(zip(left, right)):
            if x != y:
                check(4096 <= i < 8192 and (i - 4096) // 4 in slots, "only explicit observed written timestamp slots may differ")
                differences.append({"file": name, "offset": i, "left": x, "right": y})
    return {"recorded_call_sequences_exact": True, "files_byte_equal_except_explicit_written_timestamp_slots": True,
            "raw_timestamp_byte_differences": differences, "artifact_normalization": "None; every raw file retained, timestamp values independently range-checked.",
            "comparison_exclusions": "Only each written timestamp slot's raw value; no other file bytes or final state fields omitted. This is not a JDI trace of every nested original invocation."}


def expected_plan(observed):
    result = []
    def add(kind, label): result.append((kind, label))
    def snap(label):
        if observed: add("snapshot", label)
    def flush(label): add("flush", label); snap(label)
    def growth(label): add("growth", label + "-growth"); add("growth", label + "-reset")
    def write(label, stream=False, accepted=True):
        if stream: add("before_close", label); snap(label + "-before-close")
        add("stream_close" if stream else "direct_commit", label)
        if accepted: add("read", label)
        growth(label); snap(label)
    for i in range(9): add("open", "mapping-" + str(i))
    snap("mapping-open"); flush("mapping-flush")
    add("open", "allocation-empty"); snap("allocation-empty")
    add("growth", "initial-growth"); add("growth", "initial-reset")
    for label in LABELS:
        write(label, label.startswith("A-"), not label.startswith("reject-"))
        if label == "C-grow-four":
            flush("allocation-midpoint-flush"); add("open", "allocation-midpoint-reopen")
            add("growth", "midpoint-reopen-growth"); snap("allocation-midpoint-reopened")
    for i in range(7): add("read", "retained-" + str(i) + ",0")
    snap("allocation-retained"); flush("allocation-flush")
    add("open", "allocation-reopen"); add("growth", "reopen-growth")
    for i in range(7): add("read", "reopen-" + str(i) + ",0")
    snap("allocation-reopened"); flush("allocation-reopen-flush")
    for i in range(256): add("open", "cache-fill-" + str(i))
    snap("cache-full-256"); add("open", "cache-hit-at-256"); snap("cache-hit-at-256")
    add("open", "cache-miss-257"); snap("cache-miss-257"); flush("cache-final-flush")
    return result


def expected_bindings(side):
    cache, region, buffer = ("rj", "qj", "hz") if side == "client" else ("kq", "jx", "fh")
    methods = {}
    for role, owner, name, descriptor, modifiers in (
        ("open", cache, "a", "(Ljava/io/File;II)L" + region + ";", 41),
        ("input", cache, "c", "(Ljava/io/File;II)Ljava/io/DataInputStream;", 9),
        ("output", cache, "d", "(Ljava/io/File;II)Ljava/io/DataOutputStream;", 9),
        ("growth", cache, "b", "(Ljava/io/File;II)I", 9), ("flush", cache, "a", "()V", 41),
        ("commit", region, "a", "(II[BI)V", 36), ("contains", region, "c", "(II)Z", 1),
        ("buffer_close", buffer, "close", "()V", 1)):
        methods[role] = dict(owner=owner, name=name, descriptor=descriptor, modifiers=modifiers)
    fields = {}
    for role, owner, name, descriptor, modifiers in (
        ("cache_entries", cache, "a", "Ljava/util/Map;", 26), ("region_file", region, "b", "Ljava/io/File;", 18),
        ("region_handle", region, "c", "Ljava/io/RandomAccessFile;", 2), ("locations", region, "d", "[I", 18),
        ("timestamps", region, "e", "[I", 18), ("free", region, "f", "Ljava/util/ArrayList;", 2), ("counter", region, "g", "I", 2)):
        fields[role] = dict(owner=owner, name=name, descriptor=descriptor, modifiers=modifiers)
    return {"methods": methods, "fields": fields}


def validate_original(value, files, side, mode, jar=None, public=False):
    keys(value, ("schema", "scope", "side", "mode", "jar_sha256", "platform_parent", "class_origins", "bindings",
                 "held_references", "limits", "events", "final_state", "final_files"), "original region document")
    integer(value["schema"], 1, 1, "original schema")
    check(value["scope"] == "untouched original low-level region methods; helper-owned files only" and
          value["side"] == side and value["mode"] == mode and value["jar_sha256"] == JAR_HASHES[side] and
          value["platform_parent"] is True and same_json(value["bindings"], expected_bindings(side)), "actual untouched original method/field/source bindings")
    origin = "supplied:" + side if public else Path(jar).resolve().as_uri().replace("file:///", "file:/", 1)
    names = ("rj", "qj", "hz") if side == "client" else ("kq", "jx", "fh")
    check(value["class_origins"] == dict.fromkeys(names, origin), "all selected classes originate in the supplied archive")
    check(value["held_references"] == "Every returned region is strongly retained by helper; no SoftReference collection claim." and
          same_json(value["limits"], {"heap_mib": 512, "files": 300, "file_bytes": MAX_FILE, "inventory_bytes": 8 * 1024 * 1024}), "explicit bounded observation and ownership scope")
    check(type(value["events"]) is list and all(type(row) is dict for row in value["events"]) and
          [(row.get("kind"), row.get("label")) for row in value["events"]] == expected_plan(mode == "observed"), "complete exact ordered finite original call/snapshot schedule")
    cache, held, seen, snapshot_index, payloads = {}, [], set(), 0, {}
    mapping = [(0,0),(31,31),(32,32),(-1,-1),(-32,-32),(-33,-33),(-1,32),(-2147483648,2147483647),(2147483647,-2147483648)]
    growth_by_label = dict(zip(LABELS, (4096,8192,12288,0,0,0,16384,0,0,8192,0,0)))
    write_coordinates = dict(zip(LABELS, ((0,0),(1,0),(2,0),(1,0),(3,0),(0,0),(2,0),(4,0),(5,0),(6,0),(7,0),(1,0))))
    compressed_lengths = dict(zip(LABELS, (None,4091,8190,100,100,None,12283,4091,4090,4091,1044475,1044475)))

    def fingerprints(rows):
        check(type(rows) is dict and set(rows) == seen, "complete current original file inventory")
        total = 0
        for name, fingerprint_value in rows.items():
            safe_relative(name); validate_fingerprint(fingerprint_value, MAX_FILE, 8192)
            total += fingerprint_value["bytes"]
        check(len(rows) <= 300 and total <= 8 * 1024 * 1024, "bounded full original file inventory")

    def state(actual):
        keys(actual, ("cache_count", "cache_entries_raw_order", "held_regions"), "original state")
        integer(actual["cache_count"], 0, 256, "cache count")
        check(type(actual["cache_entries_raw_order"]) is list and len(actual["cache_entries_raw_order"]) == len(cache) == actual["cache_count"], "complete raw map membership")
        entries = {}
        for row in actual["cache_entries_raw_order"]:
            keys(row, ("path", "live", "object"), "original cache entry")
            safe_relative(row["path"]); integer(row["object"], 0, len(held)-1, "retained map owner")
            check(row["live"] is True and row["path"] not in entries, "retained live unique region")
            entries[row["path"]] = row["object"]
        check(entries == cache, "actual map points at the requested retained identities")
        check(type(actual["held_regions"]) is list and len(actual["held_regions"]) == len(held), "complete retained region owners")
        for i, row in enumerate(actual["held_regions"]):
            keys(row, ("object", "path", "handle_valid", "locations", "timestamps", "free", "growth"), "retained region")
            integer(row["object"], i, i, "stable ordered owner ID")
            check(row["path"] == held[i]["path"] and type(row["handle_valid"]) is bool and row["handle_valid"] == held[i]["live"], "original path and open/closed owner state")
            integer(row["growth"], 0, MAX_FILE, "original growth")
            check(row["growth"] == held[i]["growth"], "actual original growth ownership")
            for name in ("locations", "timestamps"):
                check(type(row[name]) is list and len(row[name]) == 1024, "all1024 original header fields")
                for item in row[name]: integer(item, -(1 << 31), (1 << 31)-1, "original header bits")
            check(type(row["free"]) is list and 2 <= len(row["free"]) <= MAX_FILE//4096 and all(type(b) is bool for b in row["free"]), "complete original free-sector vector")

    for row in value["events"]:
        kind, label = row["kind"], row["label"]
        extras = {"open": ("root","coordinates","before_ms","after_ms","object"), "growth": ("root","coordinates","result"),
                  "flush": (), "snapshot": ("directory","state","files"), "before_close": ("coordinates","present"),
                  "stream_close": ("root","coordinates","before_ms","after_ms","plain"), "read": ("coordinates","payload"),
                  "direct_commit": ("root","coordinates","before_ms","after_ms","plain","compressed","expected_accepted","scope")}[kind]
        keys(row, ("kind","label",*extras), "typed original event")
        if "coordinates" in row:
            check(type(row["coordinates"]) is list and len(row["coordinates"]) == 2, "two original coordinates")
            for item in row["coordinates"]: integer(item, -(1 << 31), (1 << 31)-1, "signed original coordinate")
        if "root" in row: check(row["root"] in ("mapping","allocation","cache"), "fixed fresh region root")
        if "before_ms" in row:
            integer(row["before_ms"], 0, (1 << 63)-1, "original wallclock start")
            integer(row["after_ms"], row["before_ms"], row["before_ms"]+5000, "original wallclock end")
        for field in ("plain","payload","compressed"):
            if field in row: validate_fingerprint(row[field], MAX_FILE)
        if kind == "open":
            if label.startswith("mapping-"): root, coordinates = "mapping", mapping[int(label[8:])]
            elif label.startswith("cache-"): root, coordinates = "cache", ((256*32,0) if label == "cache-miss-257" else (0,0) if label == "cache-hit-at-256" else (int(label[11:])*32,0))
            else: root, coordinates = "allocation", (0,0)
            check(row["root"] == root and row["coordinates"] == list(coordinates), "exact global/open call arguments")
            path = f"{root}/region/r.{coordinates[0] >> 5}.{coordinates[1] >> 5}.mcr"
            if path not in cache:
                if len(cache) >= 256:
                    for number in cache.values(): held[number]["live"] = False
                    cache.clear()
                cache[path] = len(held); held.append(dict(path=path,live=True,growth=0 if path in seen else 8192)); seen.add(path)
            integer(row["object"], cache[path], cache[path], "actual returned region identity")
        elif kind == "flush":
            for number in cache.values(): held[number]["live"] = False
            cache.clear()
        elif kind == "growth":
            check(row["root"] == "allocation" and row["coordinates"] == [0,0], "growth getter arguments")
            number = cache[IMAGE_PATH]; integer(row["result"], held[number]["growth"], held[number]["growth"], "growth consumed exactly once")
            held[number]["growth"] = 0
        elif kind in ("stream_close","direct_commit"):
            check(row["root"] == "allocation" and row["coordinates"] == list(write_coordinates[label]), "fixed actual local write arguments")
            held[cache[IMAGE_PATH]]["growth"] += growth_by_label[label]
            if kind == "direct_commit":
                check(row["expected_accepted"] is (not label.startswith("reject-")) and row["compressed"]["bytes"] == compressed_lengths[label] and
                      row["scope"] == "constructed valid-zlib protected-method input; not a world/NBT save", "explicit exact protected-input boundary")
            else: check(row["plain"]["bytes"] == (1024 if label == "A-first-stream" else 2048), "fixed buffered stream input length")
            if not label.startswith("reject-"): payloads[tuple(row["coordinates"])] = row["plain"]
        elif kind == "before_close":
            check(row["coordinates"] == [0,0] and row["present"] is (label != "A-first-stream"), "actual buffered-close presence result")
        elif kind == "read":
            coordinates = write_coordinates[label] if label in write_coordinates else (int(label.split("-")[1].split(",")[0]),0)
            check(row["coordinates"] == list(coordinates), "exact original retained read arguments")
            check(row["payload"] == payloads[coordinates], "actual original read bytes equal the last accepted input")
        elif kind == "snapshot":
            check(row["directory"] == f"{snapshot_index:03d}-{label}", "fixed unique owned snapshot directory")
            snapshot_index += 1; fingerprints(row["files"]); state(row["state"])
    fingerprints(value["final_files"]); state(value["final_state"])
    result = audit(value, files)
    check(result["protocol_actions"] == 341 and result["snapshots"] == (27 if mode == "observed" else 0) and len(held) == 267, "entire original fixed corpus")
    if mode == "observed":
        middle = next(row for row in value["events"] if row["kind"] == "snapshot" and row["label"] == "allocation-midpoint-reopened")
        before = next(row for row in value["events"] if row["kind"] == "snapshot" and row["label"] == "C-grow-four")
        check(middle["files"] == before["files"], "actual original close/reopen preserves entire raw allocation image")
    return result


def encode_replay(mode, initial, writes):
    check(mode in ("fresh","import-copy") and type(initial) is bytes and len(initial) <= MAX_RECORD and
          (not initial if mode == "fresh" else bool(initial)) and type(writes) is list and len(writes) <= 32, "bounded MRW1 initial state and count")
    result = bytearray(b"MRW1" + bytes([mode == "import-copy"]) + struct.pack(">I", len(initial)) + initial + struct.pack(">I", len(writes)))
    for row in writes:
        keys(row, ("x","z","timestamp","payload"), "MRW1 write")
        for name in ("x","z"): integer(row[name], -(1 << 31), (1 << 31)-1, "MRW1 coordinate bits")
        integer(row["timestamp"], 0, 0xffffffff, "MRW1 timestamp bits")
        check(type(row["payload"]) is bytes and len(row["payload"]) <= MAX_RECORD, "bounded opaque compressed input")
        result += struct.pack(">4I", row["x"] & 0xffffffff, row["z"] & 0xffffffff, row["timestamp"], len(row["payload"])) + row["payload"]
    check(len(result) <= MAX_RECORD, "bounded complete MRW1 input")
    return bytes(result)


def encode_states(states):
    check(type(states) is list and 1 <= len(states) <= 33, "bounded MRS1 state count")
    result = bytearray(b"MRS1" + struct.pack(">I", len(states)))
    for row in states:
        keys(row, ("status","growth","image"), "complete MRS1 state")
        integer(row["status"], 0, 6, "C writer status")
        integer(row["growth"], 0, 0xffffffff, "C growth")
        check(type(row["image"]) is bytes and len(row["image"]) <= MAX_RECORD, "bounded complete MRS1 image")
        result += struct.pack(">3I",row["status"],row["growth"],len(row["image"])) + row["image"]
    check(len(result) <= MAX_OUTPUT, "bounded MRS1 output")
    return bytes(result)


def derive_profiles(record, files):
    snapshots = {row["label"]: row for row in record["events"] if row["kind"] == "snapshot"}
    actions = {row["label"]: row for row in record["events"] if row["kind"] in ("stream_close","direct_commit")}
    growth = {row["label"]: row["result"] for row in record["events"] if row["kind"] == "growth"}
    def image(label): return (files / "snapshots" / snapshots[label]["directory"] / IMAGE_PATH).read_bytes()
    profiles = {}
    for mode in ("fresh","import-copy"):
        initial = image("allocation-empty" if mode == "fresh" else "allocation-midpoint-reopened")
        labels = LABELS if mode == "fresh" else LABELS[7:]
        states = [dict(status=0,growth=growth["initial-growth"] if mode == "fresh" else growth["midpoint-reopen-growth"],image=initial)]
        writes = []
        for label in labels:
            row, expected = actions[label], image(label)
            x,z = row["coordinates"]; index = x + z * 32
            location = struct.unpack_from(">I",expected,index*4)[0]
            timestamp = struct.unpack_from(">I",expected,4096+index*4)[0]
            if row["kind"] == "direct_commit": payload = (files / "snapshots/inputs" / (row["compressed"]["sha256"] + ".zlib")).read_bytes()
            else:
                offset = (location >> 8)*4096; length = struct.unpack_from(">I",expected,offset)[0]
                check(expected[offset+4] == 2, "actual observed stream selector")
                payload = expected[offset+5:offset+4+length]
            writes.append(dict(x=x,z=z,timestamp=timestamp,payload=payload))
            states.append(dict(status=4 if label.startswith("reject-") else 0,growth=growth[label+"-growth"],image=expected))
        profiles[mode] = {"input": encode_replay(mode,b"" if mode == "fresh" else initial,writes), "expected": encode_states(states), "writes": len(writes)}
    return profiles


def packed(raw):
    check(type(raw) is bytes and len(raw) <= MAX_OUTPUT, "bounded lossless fixture stream")
    return {**blob(raw), "zlib_base64":base64.b64encode(zlib.compress(raw,9)).decode("ascii")}


def unpack(value, limit=MAX_RECORD):
    keys(value,("bytes","sha256","zlib_base64"),"packed fixture stream")
    validate_fingerprint({k:value[k] for k in ("bytes","sha256")},limit)
    check(type(value["zlib_base64"]) is str and len(value["zlib_base64"]) <= MAX_FIXTURE, "bounded encoded fixture stream")
    try: compressed=base64.b64decode(value["zlib_base64"],validate=True)
    except (ValueError,base64.binascii.Error) as failure: raise Failure("strict fixture base64") from failure
    raw=inflate(compressed,limit)
    check(blob(raw)=={k:value[k] for k in ("bytes","sha256")},"lossless fixture length and SHA256")
    return raw


def pool_evidence(record, pool):
    check(type(record) is dict and type(record.get("events")) is list and
          len(record["events"]) < 1000 and type(record.get("final_files")) is dict,
          "complete bounded public original evidence graph")
    mapping={}
    def add(path, value):
        validate_fingerprint(value,MAX_FILE)
        check(value["sha256"] in pool and blob(pool[value["sha256"]])==value,"complete attributed original raw blob")
        if path in mapping: check(mapping[path]==pool[value["sha256"]],"consistent repeated evidence path")
        mapping[path]=pool[value["sha256"]]
    for row in record["events"]:
        check(type(row) is dict and type(row.get("kind")) is str,
              "typed public original evidence event")
        if row["kind"]=="snapshot":
            check(type(row.get("files")) is dict and type(row.get("directory")) is str,
                  "complete public snapshot evidence graph")
            for name,value in row["files"].items(): add("snapshots/"+safe_relative(row["directory"])+"/"+safe_relative(name),value)
        for field,suffix in (("plain",".plain"),("compressed",".zlib")):
            if field in row:
                validate_fingerprint(row[field],MAX_FILE)
                add("snapshots/inputs/"+row[field]["sha256"]+suffix,row[field])
    for name,value in record["final_files"].items(): add("snapshots/final/"+safe_relative(name),value)
    return Evidence(mapping=mapping)


def validate_public_metadata(value):
    keys(value,("schema","format","scope","status_semantics","provenance","originals","blobs","cases"),"region writer fixture")
    integer(value["schema"],1,1,"fixture schema")
    check(value["format"]=="MRW1/MRS1" and value["scope"]=="region-memory-write" and
          value["status_semantics"]=="Original write methods return void. Status4 denotes the C record-too-large diagnostic for the observed unchanged-image branch; other retained writes use status0.","explicit finite writer boundary and status mapping")
    provenance=value["provenance"]
    keys(provenance,("mode","manifest_sha256","tested_commit","sources","helper_classes","helper_class_version","original_classes","jar_sha256","tools","exporter_sha256","counts","controls"),"complete public provenance")
    check(provenance["mode"]=="original-only" and provenance["jar_sha256"]==JAR_HASHES,"original-only supplied archive provenance")
    integer(provenance["helper_class_version"],61,61,"Java17 helper class version")
    check(provenance["tested_commit"] is None or type(provenance["tested_commit"]) is str and re.fullmatch(r"[0-9a-f]{40}",provenance["tested_commit"]),"explicit source label")
    for name in ("manifest_sha256","exporter_sha256"):
        check(type(provenance[name]) is str and re.fullmatch(r"[0-9a-f]{64}",provenance[name]),"complete provenance SHA256")
    check(type(provenance["sources"]) is dict and set(provenance["sources"])==set(SOURCES),"exact relevant project source closure")
    for row in provenance["sources"].values(): validate_fingerprint(row,2*1024*1024,1)
    check(provenance["exporter_sha256"]==provenance["sources"]["scripts/reference/storage/export_region_write.py"]["sha256"],"exporter identity belongs to the captured source closure")
    check(type(provenance["helper_classes"]) is dict and set(provenance["helper_classes"])=={"RegionWriteObservation.class"},"exact helper class inventory")
    validate_fingerprint(provenance["helper_classes"]["RegionWriteObservation.class"],1024*1024,8)
    check(type(provenance["original_classes"]) is dict and set(provenance["original_classes"])==set(JAR_HASHES),"both original class inventories")
    for side,names in (("client",("rj","qj","hz")),("server",("kq","jx","fh"))):
        check(type(provenance["original_classes"][side]) is dict and set(provenance["original_classes"][side])==set(names),"exact pinned original classes")
        for row in provenance["original_classes"][side].values(): validate_fingerprint(row,16384,1)
    check(hashlib.sha256(json.dumps(provenance["original_classes"],sort_keys=True,separators=(",",":")).encode()).hexdigest()==ORIGINAL_CLASS_MANIFEST_SHA,
          "class fingerprints independently pinned to the supplied archives")
    check(type(provenance["tools"]) is dict and set(provenance["tools"])=={"java","javac"},"complete required JDK tools")
    for name,row in provenance["tools"].items():
        keys(row,("fingerprint","major","output"),"public JDK attribution")
        validate_fingerprint(row["fingerprint"],128*1024*1024,1);integer(row["major"],17,99,"JDK17+ tool")
        check(type(row["output"]) is str and 0<len(row["output"])<=32768 and re.search(r'(?:version\s+"|javac\s+)'+str(row["major"])+r'\b',row["output"]),"actual retained JDK version output")
    check(same_json(provenance["counts"],{"original_runs":4,"observed_snapshots":54,"protocol_actions":1364,"profiles":4,"replay_writes":34,"replay_successes":26,"replay_rejections":8}),"complete finite observed and replay corpus")
    check(type(provenance["controls"]) is dict and set(provenance["controls"])==set(JAR_HASHES),"both original control records")
    for row in provenance["controls"].values(): validate_fingerprint(row,MAX_RECORD,1)
    check(type(value["originals"]) is dict and set(value["originals"])==set(JAR_HASHES),"both sides retained")
    check(type(value["cases"]) is list and len(value["cases"])==4,"all four continuous C profiles")
    for index,(row,(side,mode)) in enumerate(zip(value["cases"],CASE_LAYOUT)):
        keys(row,("index","side","profile","writes","input","expected"),"complete fixture profile")
        integer(row["index"],index,index,"ordered unique case index")
        check(row["side"]==side and row["profile"]==mode,"fixed side/profile order")
        integer(row["writes"],12 if mode=="fresh" else 5,12 if mode=="fresh" else 5,"complete continuous write count")


def validate_fixture_value(value):
    validate_public_metadata(value)
    check(type(value["blobs"]) is dict and 1<=len(value["blobs"])<=128,"bounded deduplicated raw original blobs")
    pool,total={},0
    for digest,row in value["blobs"].items():
        check(type(digest) is str and re.fullmatch(r"[0-9a-f]{64}",digest),"raw blob address")
        raw=unpack(row,MAX_FILE);check(hashlib.sha256(raw).hexdigest()==digest,"content-addressed original bytes")
        total+=len(raw);check(total<=64*1024*1024,"bounded total raw evidence bytes");pool[digest]=raw
    records,files,used={}, {}, set()
    for side,row in value["originals"].items():
        keys(row,("observed","control","raw_observed","raw_control"),"complete original records")
        for mode in ("observed","control"):
            validate_fingerprint(row["raw_"+mode],MAX_RECORD,1)
            record=strict_json(unpack(row[mode],MAX_RECORD));evidence=pool_evidence(record,pool)
            validate_original(record,evidence,side,mode,public=True)
            records[side,mode]=record;files[side,mode]=evidence
            used.update(hashlib.sha256(raw).hexdigest() for raw in evidence.mapping.values())
        check(row["raw_control"]==value["provenance"]["controls"][side],"raw control fingerprint bound to provenance")
        compare(records[side,"observed"],files[side,"observed"],records[side,"control"],files[side,"control"])
    check(used==set(pool),"no unattributed raw blob omissions/extras")
    compare(records["client","observed"],files["client","observed"],records["server","observed"],files["server","observed"])
    result=[]
    for row in value["cases"]:
        derived=derive_profiles(records[row["side"],"observed"],files[row["side"],"observed"])[row["profile"]]
        raw,expected=unpack(row["input"],MAX_RECORD),unpack(row["expected"],MAX_OUTPUT)
        check(raw==derived["input"] and expected==derived["expected"],"full original-only MRW1 input and MRS1 expectations rederived")
        result.append({"record":row,"input":raw,"expected":expected})
    return value,result


def load_fixture(path):
    return validate_fixture_value(strict_json(checked_bytes(path,MAX_FIXTURE)))


def _live_inventory(root, count_limit, byte_limit):
    """Sample resources while the owned child can remove ordinary entries.

    Missing entries still consume the traversal budget. This is no final-state
    evidence; default inventory remains the quiescent strict path.
    """
    root = Path(root)
    result, total, enumerated, pending = {}, 0, 0, [root]
    while pending:
        directory = pending.pop()
        try:
            info = os.stat(directory, follow_symlinks=False)
        except FileNotFoundError:
            continue
        check(not stat.S_ISLNK(info.st_mode) and
              not (getattr(info, "st_file_attributes", 0) & 0x400),
              "owned artifacts have no symbolic links/reparse points")
        check(stat.S_ISDIR(info.st_mode), "ordinary artifact directory")
        try:
            with os.scandir(directory) as entries:
                for entry in entries:
                    check(enumerated < count_limit, "bounded live artifact traversal")
                    enumerated += 1
                    try:
                        info = os.stat(entry.path, follow_symlinks=False)
                    except FileNotFoundError:
                        continue
                    check(not stat.S_ISLNK(info.st_mode) and
                          not (getattr(info, "st_file_attributes", 0) & 0x400),
                          "owned artifacts have no symbolic links/reparse points")
                    if stat.S_ISDIR(info.st_mode):
                        pending.append(Path(entry.path))
                    else:
                        check(stat.S_ISREG(info.st_mode), "ordinary artifact entries")
                        total += info.st_size
                        check(total <= byte_limit, "bounded complete artifact inventory")
                        result[Path(entry.path).relative_to(root).as_posix()] = {"bytes": info.st_size}
        except FileNotFoundError:
            # Queued directories can disappear before opening or during iteration.
            continue
    return result


def inventory(root, hashes=False, count_limit=10000, byte_limit=192 * 1024 * 1024,
              *, live_poll=False):
    if live_poll:
        check(not hashes, "live artifact polling cannot hash final state")
        return _live_inventory(root, count_limit, byte_limit)
    result, total = {}, 0
    if not root.exists(): return result
    for path in sorted(root.rglob("*")):
        check(not path.is_symlink(), "owned artifacts have no symbolic links")
        if path.is_file():
            size = path.stat().st_size; total += size
            check(len(result) < count_limit and total <= byte_limit, "bounded complete artifact inventory")
            result[path.relative_to(root).as_posix()] = fingerprint(path, MAX_OUTPUT) if hashes else {"bytes": size}
        else: check(path.is_dir(), "ordinary artifact entries")
    return result


def process(command, directory, label, timeout=90, stdin=None, stdout_limit=32768):
    global _latest_process
    output, errors = directory / (label + ".stdout"), directory / (label + ".stderr")
    input_file = None
    if stdin is not None:
        check(type(stdin) is bytes and len(stdin) <= MAX_RECORD, "bounded process input")
        input_file = directory / (label + ".stdin"); input_file.write_bytes(stdin)
    started = time.monotonic(); timed_out, stopped = False, None
    polls, supervision_error = 0, None
    with output.open("xb") as stdout, errors.open("xb") as stderr:
        with input_file.open("rb") if input_file else open(os.devnull,"rb") as stream:
            child = subprocess.Popen(list(map(str,command)),cwd=ROOT,stdin=stream,stdout=stdout,stderr=stderr)
            try:
                last_inventory = -1
                while child.poll() is None:
                    elapsed = time.monotonic()-started
                    if elapsed >= timeout: timed_out=True; stopped="deadline"; child.kill(); break
                    if output.stat().st_size > stdout_limit or errors.stat().st_size > 32768:
                        stopped="stream bound"; child.kill(); break
                    if int(elapsed) != last_inventory:
                        polls += 1
                        inventory(directory,count_limit=6000,byte_limit=160*1024*1024,live_poll=True); last_inventory=int(elapsed)
                    time.sleep(.05)
                child.wait(timeout=10)
                inventory(directory,count_limit=6000,byte_limit=160*1024*1024)
            except BaseException as failure:
                supervision_error = {'class': type(failure).__name__, 'message': str(failure)[:4096]}
                child.kill(); child.wait(timeout=10); raise
            finally:
                # Observe after reaping, before hashing or success validation.
                # Sizes describe actual direct-file output, including overshoot.
                def observed_size(stream):
                    try:
                        return os.fstat(stream.fileno()).st_size
                    except (OSError, ValueError):
                        return None
                _latest_process = {'label': label, 'directory': str(directory), 'command': list(map(str, command)),
                                   'returncode': child.returncode, 'timed_out': timed_out, 'stop_reason': stopped,
                                   'seconds': time.monotonic()-started, 'inventory_polls': polls,
                                   'stdout_file': output.name, 'stderr_file': errors.name,
                                   'stdout_observed': observed_size(stdout), 'stderr_observed': observed_size(stderr),
                                   'supervision_error': supervision_error}
    check(output.stat().st_size <= stdout_limit and errors.stat().st_size <= 32768, "bounded complete child streams")
    return {"command": list(map(str,command)),"returncode":child.returncode,"timed_out":timed_out,"stop_reason":stopped,
            "seconds":time.monotonic()-started,"stdout_file":output.name,"stderr_file":errors.name,
            "stdout":fingerprint(output,stdout_limit),"stderr":fingerprint(errors,32768)}


def successful(value, *, label=None, stdout_limit=MAX_OUTPUT):
    keys(value, ("command", "returncode", "timed_out", "stop_reason", "seconds",
                 "stdout_file", "stderr_file", "stdout", "stderr"), "bounded process")
    check(type(value["returncode"]) is int and value["returncode"] == 0 and
          value["timed_out"] is False and value["stop_reason"] is None, "complete successful process")
    check(type(value["command"]) is list and 1 <= len(value["command"]) <= 64 and
          all(type(arg) is str and 1 <= len(arg) <= 4096 for arg in value["command"]),
          "complete bounded process command")
    check(type(value["seconds"]) in (int, float) and 0 <= value["seconds"] <= 120 and
          math.isfinite(value["seconds"]), "finite bounded process duration")
    for field, limit in (("stdout", stdout_limit), ("stderr", 32768)):
        validate_fingerprint(value[field], limit)
        name = safe_relative(value[field+"_file"])
        if label is not None:
            check(name == label+"."+field, "canonical "+label+" "+field+" stream identity")


def class_records(jar, side):
    result = {}
    with zipfile.ZipFile(jar) as archive:
        for name in (("rj","qj","hz") if side == "client" else ("kq","jx","fh")):
            raw = archive.read(name+".class")
            check(len(raw) >= 8 and raw[:4] == b"\xca\xfe\xba\xbe" and struct.unpack_from(">H",raw,6)[0] == 49, "pinned original classfile version")
            result[name] = blob(raw)
    return result


def compiled_classes(directory):
    files = sorted(directory.glob("*.class"))
    check([p.name for p in files] == ["RegionWriteObservation.class"], "exact one project helper class")
    result = {}
    for path in files:
        raw = checked_bytes(path,1024*1024)
        check(len(raw) >= 8 and raw[:4] == b"\xca\xfe\xba\xbe" and struct.unpack_from(">H",raw,6)[0] == 61, "Java17 helper class version")
        result[path.name] = blob(raw)
    return result


def tool_path(selected, name):
    value = selected or shutil.which(name)
    if value is None or not Path(value).is_file(): raise Unavailable("missing JDK tool: " + name)
    return Path(value).resolve()


def read_record(directory, side, mode, jar):
    raw = checked_bytes(directory / "record.json")
    value = strict_json(raw)
    summary = validate_original(value,Evidence(root=directory),side,mode,jar)
    return value, summary


def validate_work_dir(work_dir, protected=()):
    """Validate output ownership without importing another driver."""
    work = Path(os.path.abspath(work_dir))
    for component in reversed((work, *work.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        check(not stat.S_ISLNK(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & 0x400),
              "output link/reparse component refused")
        check(stat.S_ISDIR(info.st_mode), "output must have ordinary directory components")
    resolved = work.resolve()
    if resolved.is_relative_to(ROOT):
        parts = resolved.relative_to(ROOT).parts
        check(parts and (parts[0] in (".local", "build") or parts[0].startswith("build-")),
              "output inside public source/input layout refused")
    for selected in protected:
        if selected is None:
            continue
        path = Path(selected).resolve()
        check(resolved != path and resolved not in path.parents,
              "output would contain/alias an attributed input")
        if work.exists() and path.exists():
            check(not os.path.samefile(work, path), "output aliases an input")
    return work


def prepare(args, report):
    check(Path(__file__).resolve() == (ROOT / "scripts/reference/storage/compare_region_write.py").resolve(), "canonical executing driver source identity")
    sources = {name: fingerprint(ROOT/name,2*1024*1024) for name in SOURCES}
    report["inputs_before"] = {str(ROOT/name):value for name,value in sources.items()}
    report["source_fingerprints"] = sources
    for name in JAVA_ENVIRONMENT:
        check(name not in os.environ, "injected Java environment is forbidden: " + name)
    check(Path(JAVA_SOURCE).name == "RegionWriteObservation.java", "canonical helper source name")
    jars = {}
    for side in JAR_HASHES:
        value = getattr(args,side)
        if value is None or not Path(value).is_file(): raise Unavailable("missing locally supplied " + side + " archive")
        path = Path(value).absolute()
        check(fingerprint(path,4*1024*1024)["sha256"] == JAR_HASHES[side], "wrong supplied archive: " + side)
        jars[side] = path.resolve()
        report["inputs_before"][str(jars[side])] = fingerprint(jars[side],4*1024*1024)
    executable = None
    if args.mode == "compare":
        check(args.executable is not None and Path(args.executable).is_file(), "built C writer probe is required for comparison")
        executable = Path(args.executable).resolve()
    else: check(args.executable is None, "original-only mode cannot execute or attribute a C result")
    java,javac = tool_path(args.java,"java"),tool_path(args.javac,"javac")
    inputs = [*(ROOT/name for name in SOURCES),*jars.values(),java,javac,Path(sys.executable).resolve()]
    if executable is not None: inputs.append(executable)
    report["inputs_before"].update({str(path):fingerprint(path,128*1024*1024) for path in inputs})
    return sources, jars, executable, java, javac


def run(args, directory, report, prepared):
    sources, jars, executable, java, javac = prepared
    report["original_classes"] = {side:class_records(path,side) for side,path in jars.items()}
    report["tools"] = {}
    for name,path in (("java",java),("javac",javac)):
        result = process([path,"-version"],directory,name,20); successful(result,label=name,stdout_limit=32768)
        text = b"".join(checked_bytes(directory/result[field+"_file"],32768)
                        for field in ("stdout","stderr")).decode("utf8",errors="replace")
        match = re.search(r'(?:version\s+"|javac\s+)(\d+)',text)
        check(match is not None, "actual JDK version output: " + name)
        if int(match.group(1)) < 17: raise Unavailable("JDK17+ required: " + name)
        report["tools"][name] = dict(path=str(path),fingerprint=fingerprint(path,128*1024*1024),major=int(match.group(1)),output=text,process=result)
    source_root = directory/"sources"
    for name,expected in sources.items():
        raw = checked_bytes(ROOT/name,2*1024*1024);check(blob(raw)==expected,"stable exact attributed source")
        target = source_root/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    classes = directory/"classes";classes.mkdir()
    command = [javac,"--release","17","-encoding","UTF-8","-d",classes,source_root/JAVA_SOURCE]
    report["compile"] = process(command,directory,"compile",45);successful(report["compile"],label="compile",stdout_limit=32768)
    check(report["compile"]["stdout"]["bytes"] == report["compile"]["stderr"]["bytes"] == 0,"silent exact helper compilation")
    report["classes_before"] = compiled_classes(classes)
    records = {}
    for side,jar in jars.items():
        for mode in ("observed","control"):
            case = directory/(side+"-"+mode);case.mkdir()
            command = [java,"-Xmx512m","-cp",classes,"RegionWriteObservation",jar,side,mode,case/"storage",case/"record.json",case/"snapshots"]
            result = process(command,case,"child",90);successful(result,label="child",stdout_limit=32768)
            check(result["stdout"]["bytes"] == result["stderr"]["bytes"] == 0,"silent complete original execution; any exit77 is failure")
            value,summary = read_record(case,side,mode,jar)
            report["original_runs"].append({"side":side,"mode":mode,"directory":case.name,"process":result,"record":fingerprint(case/"record.json"),"summary":summary})
            records[side,mode]=value
        left,right=records[side,"observed"],records[side,"control"]
        report["controls"].append({"side":side,**compare(left,Evidence(root=directory/(side+"-observed")),right,Evidence(root=directory/(side+"-control")))})
    report["client/server"] = compare(records["client","observed"],Evidence(root=directory/"client-observed"),records["server","observed"],Evidence(root=directory/"server-observed"))
    for index,(side,mode) in enumerate(CASE_LAYOUT):
        derived = derive_profiles(records[side,"observed"],Evidence(root=directory/(side+"-observed")))[mode]
        replay,expected = directory/f"case-{index}.mrw",directory/f"case-{index}.mrs"
        replay.write_bytes(derived["input"]);expected.write_bytes(derived["expected"])
        row = {"index":index,"side":side,"profile":mode,"writes":derived["writes"],"input_file":replay.name,"expected_file":expected.name,
               "input":blob(derived["input"]),"expected":blob(derived["expected"])}
        if executable is not None:
            result = process([executable],directory,"c-case-"+str(index),30,derived["input"],MAX_OUTPUT);successful(result,label="c-case-"+str(index),stdout_limit=MAX_OUTPUT)
            check(result["stderr"]["bytes"]==0 and checked_bytes(directory/result["stdout_file"],MAX_OUTPUT)==derived["expected"], "byte-exact complete original/C MRS1 output")
            row["c_process"] = result
        report["cases"].append(row)
    report["classes_after"] = compiled_classes(classes)
    check(report["classes_after"]==report["classes_before"],"compiled helpers preserved")
    inventory(directory,hashes=False)


def main(argv=None):
    global _latest_process
    _latest_process = None
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client",type=Path);parser.add_argument("--server",type=Path)
    parser.add_argument("--java");parser.add_argument("--javac");parser.add_argument("--executable",type=Path)
    parser.add_argument("--mode",choices=("compare","original-only"),default="compare")
    parser.add_argument("--work-dir",type=Path,default=ROOT/".local/reference-region-write")
    parser.add_argument("--tested-commit")
    args=parser.parse_args(argv)
    directory = None
    report={"schema":1,"scope":"region-memory-write","mode":args.mode,"status":"running","tested_commit":args.tested_commit,
            "source_label":"Caller label only; exact sources/helpers/tools are fingerprinted.","python":platform.python_version(),"platform":platform.platform(),
            "original_runs":[],"controls":[],"cases":[],"errors":[],
            "limits":{"java_heap_mib":512,"child_seconds":90,"original_record_bytes":MAX_RECORD,"input_bytes":MAX_RECORD,"c_output_bytes":MAX_OUTPUT,
                      "policy":"Bounded children and artifact inventories; no OS process-tree or filesystem quota."}}
    code=1
    try:
        check(args.tested_commit is None or re.fullmatch(r"[0-9a-f]{40}",args.tested_commit),"explicit40hex tested source label")
        protected = [*(ROOT/name for name in SOURCES), Path(sys.executable),
                     args.client, args.server, args.executable, args.java, args.javac]
        work = validate_work_dir(args.work_dir, protected)
        prepared = prepare(args, report)
        work = validate_work_dir(work, report["inputs_before"])
        work.mkdir(parents=True, exist_ok=True)
        candidate = work/("capture-"+uuid.uuid4().hex)
        candidate.mkdir()
        directory = candidate.resolve()
        run(args,directory,report,prepared);report["status"]="pass";code=0
    except Unavailable as failure:
        report["status"]="unavailable";report["unavailable_reason"]=str(failure);code=77
    except (Failure,OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError,zipfile.BadZipFile) as failure:
        report["status"]="fail";report["errors"].append(type(failure).__name__+": "+str(failure));code=1
    finally:
        if "inputs_before" in report:
            try:
                report["inputs_after"]={name:fingerprint(Path(name),128*1024*1024) for name in report["inputs_before"]}
                check(report["inputs_after"]==report["inputs_before"],"source/archive/tool/executable preservation after any outcome")
                if "classes_before" in report:
                    report["classes_after"]=compiled_classes(directory/"classes")
                    check(report["classes_after"]==report["classes_before"],"helper preservation after any outcome")
            except (Failure,OSError) as failure:
                report["status"]="fail";report["errors"].append(str(failure));code=1
        if report['status'] == 'fail' and _latest_process is not None:
            report['failed_process'] = _latest_process
        if directory is None:
            print(json.dumps(report), file=sys.stderr)
        else:
            try:
                (directory/"manifest.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")
                print(directory/"manifest.json")
            except OSError as failure:
                report["status"]="fail";code=1
                report["errors"].append("cannot retain outcome manifest: "+str(failure))
                if _latest_process is not None:
                    report['failed_process'] = _latest_process
                print(json.dumps(report), file=sys.stderr)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
