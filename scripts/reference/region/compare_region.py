#!/usr/bin/env python3
"""Compare immutable C region inspection with untouched readers on fresh copies."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import struct
import sys
import types
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[3]
MAX_BYTES = 32 * 1024 * 1024
JAR_HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
              "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def keys(value, expected, context):
    require(type(value) is dict and set(value) == set(expected), context + ": complete keys")


def integer(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ": integer bounds")


def hex_value(value, maximum, minimum=0):
    require(type(value) is str and minimum * 2 <= len(value) <= maximum * 2 and
            len(value) % 2 == 0 and re.fullmatch(r"[0-9a-f]*", value) is not None, "complete bounded raw hex")


def outcome(value, success, payload=None):
    require(type(value) is dict and type(value.get("status")) is str, "typed original outcome")
    if value["status"] == "exception":
        keys(value, ("status", "class"), "original exception")
        require(type(value["class"]) is str and 1 <= len(value["class"]) <= 256 and
                re.fullmatch(r"[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*", value["class"]),
                "original exception class")
    else:
        require(value["status"] in success, "actual original outcome kind")
        keys(value, ("status", payload) if payload is not None and value["status"] == success[0] else ("status",), "original outcome")


def helpers():
    require(Path(__file__).resolve() == (ROOT / "scripts/reference/region/compare_region.py").resolve(), "canonical executing source identity")
    path = ROOT / "scripts/reference/chunk/compare_chunk.py"
    data = path.read_bytes()
    module = types.ModuleType("_region_chunk")
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    common, executed = module.shared_helper(ROOT)
    executed.append({"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    fixture, record = module.source_module(Path(__file__).with_name("fixtures.py"), "_region_inputs")
    executed.append(record)
    nbt_fixture, record = module.source_module(ROOT / "scripts/reference/nbt/fixtures.py", "_region_nbt_inputs")
    executed.append(record)
    previous = sys.modules.get("fixtures")
    sys.modules["fixtures"] = nbt_fixture
    try:
        nbt, record = module.source_module(ROOT / "scripts/reference/nbt/compare_nbt.py", "_region_nbt")
    finally:
        if previous is None:
            sys.modules.pop("fixtures", None)
        else:
            sys.modules["fixtures"] = previous
    executed.append(record)
    return common, nbt, fixture, executed


def plans(cases):
    require(len(cases) == 36 and len({case["label"] for case in cases}) == 36, "complete distinct 36-case corpus")
    result = []
    for case in cases:
        require(re.fullmatch(r"[a-z0-9-]{1,96}", case["label"]) and type(case["bytes"]) is bytes and
                len(case["bytes"]) < 65536 and case["queries"] and
                len({tuple(pair) for pair in case["queries"]}) == len(case["queries"]), "bounded distinct generated region case")
        for coordinates in case["queries"]:
            require(len(coordinates) == 2 and all(type(item) is int and -(1 << 31) <= item < 1 << 31 for item in coordinates),
                    "signed local query coordinates")
            result.append({"index": len(result), "label": case["label"], "coordinates": tuple(coordinates), "bytes": case["bytes"]})
    require(len(result) == 49, "all 49 ordered queries")
    layout = [[case["label"], case["queries"]] for case in cases]
    require(hashlib.sha256(json.dumps(layout, separators=(",", ":")).encode("ascii")).hexdigest() ==
            "6846b73ad91e96cc9a987aa5e8a3c650994bde6d69b452f6444081ee0add8871", "fixed complete ordered region corpus")
    return result


def validate_original(value, side, corpus, copies, common, jar):
    keys(value, ("schema", "side", "jar", "platform_parent", "class_origins", "records"), "original document")
    keys(value["jar"], ("bytes", "sha256"), "original archive fingerprint")
    require(type(value["schema"]) is int and value["schema"] == 1 and value["side"] == side and
            value["jar"]["sha256"] == JAR_HASHES[side] and type(value["jar"]["bytes"]) is int and
            value["jar"]["bytes"] == jar.stat().st_size and value["platform_parent"] is True and
            type(value["records"]) is list and len(value["records"]) == len(corpus) == len(copies) == 49,
            "complete original provenance/corpus")
    names = ("qj", "as", "nu") if side == "client" else ("jx", "ag", "iq")
    origins = value["class_origins"]
    keys(origins, ("region", "utility", "compound"), "original class origins")
    for entry in origins.values():
        keys(entry, ("name", "origin", "loader"), "original class provenance")
    require(
            all(origins[role]["name"] == name and origins[role]["loader"] == "isolated" for role, name in zip(("region", "utility", "compound"), names)) and
            all(item["origin"] == jar.resolve().as_uri().replace("file:///", "file:/", 1) for item in origins.values()), "exact selected original classes")
    for row, plan, path in zip(value["records"], corpus, copies):
        keys(row, ("index", "x", "z", "input_before", "input_after", "constructor", "locations", "timestamps", "growth", "free_sectors", "present", "root", "drain", "close"), "complete original query")
        require(type(row["index"]) is int and type(row["x"]) is int and type(row["z"]) is int and
                row["index"] == plan["index"] and (row["x"], row["z"]) == plan["coordinates"], "original ordered query identity")
        for name in ("input_before", "input_after"):
            keys(row[name], ("bytes", "sha256"), "original copy fingerprint")
            integer(row[name]["bytes"], 0, 65536, "original file size")
            require(type(row[name]["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", row[name]["sha256"]), "actual file hash")
        expected = {"bytes": len(plan["bytes"]), "sha256": hashlib.sha256(plan["bytes"]).hexdigest()}
        after_record = common.file_record(path, byte_limit=65536)
        require(row["input_before"] == expected and row["input_after"] == {key: after_record[key] for key in ("bytes", "sha256")},
                "original independent before/after copy fingerprints")
        outcome(row["constructor"], ("ok",))
        if row["constructor"]["status"] == "exception":
            require(all(row[key] is None for key in ("locations", "timestamps", "growth", "free_sectors", "present", "root", "drain")) and
                    row["close"] == {"status": "not_created"}, "failed original constructor has no invented state")
            continue
        require(row["constructor"] == {"status": "ok"} and type(row["growth"]) is int and -(1 << 31) <= row["growth"] < 1 << 31 and
                type(row["free_sectors"]) is str and
                re.fullmatch(r"[01]{0,16}", row["free_sectors"]) and
                all(re.fullmatch(r"[0-9a-f]{8192}", row[key]) for key in ("locations", "timestamps")), "complete original constructor metadata")
        after = path.read_bytes()
        require(len(after) >= 8192 and row["locations"] == after[:4096].hex() and row["timestamps"] == after[4096:8192].hex(),
                "original arrays match actual post-constructor header")
        outcome(row["present"], ("ok",), "value")
        if row["present"]["status"] == "ok":
            require(type(row["present"]["value"]) is bool, "actual boolean presence")
        outcome(row["root"], ("root", "null"), "canonical_hex")
        if row["root"]["status"] == "root":
            hex_value(row["root"]["canonical_hex"], 1024 * 1024, 4)
        outcome(row["drain"], ("drain", "null"), "hex")
        if row["drain"]["status"] == "drain":
            hex_value(row["drain"]["hex"], 1024 * 1024)
        outcome(row["close"], ("ok",))


def format_expectation(data, x, z):
    """Independent file-geometry diagnostics, including both overlap owners."""
    if len(data) < 8192:
        return {"open": "truncated region header", "entry": None}
    locations = struct.unpack_from(">1024I", data)
    timestamps = struct.unpack_from(">1024I", data, 4096)
    entries = []
    for packed, timestamp in zip(locations, timestamps):
        offset, extent = (packed >> 8) * 4096, (packed & 255) * 4096
        flags = 0
        if packed:
            flags |= 1 if offset < 8192 else 0
            flags |= 2 if extent == 0 else 0
            flags |= 4 if offset + extent > len(data) else 0
        entries.append({"location": packed, "timestamp": timestamp, "issues": flags,
                        "record_length": 0, "byte_offset": offset if packed else 0,
                        "extent_bytes": extent if packed else 0, "selector": 0})
    for i, left in enumerate(entries):
        if not left["location"] or not left["extent_bytes"]:
            continue
        for right in entries[i + 1:]:
            if right["location"] and right["extent_bytes"] and left["byte_offset"] < right["byte_offset"] + right["extent_bytes"] and right["byte_offset"] < left["byte_offset"] + left["extent_bytes"]:
                left["issues"] |= 8
                right["issues"] |= 8
    for entry in entries:
        if not entry["location"] or entry["issues"]:
            continue
        offset = entry["byte_offset"]
        length = struct.unpack_from(">I", data, offset)[0]
        entry["record_length"] = length
        if length < 1 or length >= 1 << 31:
            entry["issues"] |= 16
        elif length + 4 > entry["extent_bytes"] or offset + length + 4 > len(data):
            entry["issues"] |= 32
        else:
            entry["selector"] = data[offset + 4]
            if entry["selector"] not in (1, 2):
                entry["issues"] |= 64
    entry = entries[x + z * 32] if 0 <= x < 32 and 0 <= z < 32 else None
    return {"open": "ok", "locations": data[:4096].hex(), "timestamps": data[4096:8192].hex(), "entry": entry,
            "view": {"length": len(data), "trailing_bytes": len(data) % 4096,
                     "present_entries": sum(bool(item) for item in locations), "invalid_entries": sum(bool(item["issues"]) for item in entries)}}


def compare_c(actual, original, plan, nbt):
    validate_c(actual)
    data, (x, z) = plan["bytes"], plan["coordinates"]
    expected = format_expectation(data, x, z)
    require(actual["open_status"] == expected["open"], "C region open status")
    if expected["open"] != "ok":
        require(actual["view"] is None and actual["locations"] is None and actual["timestamps"] is None, "C rejected short header preservation")
        return {"profile": "diagnostic-departure", "reason": "C requires a complete header; originals may repair owned copies"}
    require(all(actual[key] == expected[key] for key in ("view", "locations", "timestamps", "entry")), "complete immutable C metadata/diagnostics")
    entry = expected["entry"]
    if entry is None:
        require(actual["record_status"] == "invalid local coordinate", "C local coordinate validation")
        return {"profile": "diagnostic-departure", "reason": "C validates both coordinates; original presence may alias or throw"}
    if entry["issues"]:
        status = "unsupported compression selector" if entry["issues"] == 64 else "invalid region record"
        require(actual["record_status"] == status and actual["payload_hex"] is None, "C diagnostic rejection")
        return {"profile": "diagnostic-departure", "reason": "C rejects inconsistent extents/overlaps/lengths/selectors", "issues": entry["issues"]}
    require(original["constructor"] == {"status": "ok"} and original["close"] == {"status": "ok"} and
            original["locations"] == expected["locations"] and original["timestamps"] == expected["timestamps"], "valid original immutable header")
    if not entry["location"]:
        require(actual["record_status"] == "missing region entry" and original["present"] == {"status": "ok", "value": False} and
                original["root"] == original["drain"] == {"status": "null"}, "actual missing-region behavior")
        return {"profile": "compatible", "kind": "missing"}
    require(actual["record_status"] == "ok" and original["present"] == {"status": "ok", "value": True}, "actual present-region behavior")
    begin, length = entry["byte_offset"], entry["record_length"]
    payload = data[begin + 5:begin + 4 + length]
    require(actual["payload_hex"] == payload.hex(), "exact declared payload")
    try:
        decoder = zlib.decompressobj(31 if entry["selector"] == 1 else 15)
        raw = decoder.decompress(payload, 1024 * 1024 + 1)
        valid = decoder.eof and not decoder.unconsumed_tail and len(raw) <= 1024 * 1024
    except zlib.error:
        valid = False
    if not valid:
        require(actual["compression_status"] != "ok" and original["drain"]["status"] in ("exception", "null"), "actual invalid-container behavior")
        return {"profile": "diagnostic-departure", "reason": "C validates the whole container before NBT; original prefix reads can precede trailer failure"}
    require(actual["compression_status"] == "ok" and actual["raw_hex"] == raw.hex() and
            actual["compressed_consumed"] == len(payload) - len(decoder.unused_data) and
            actual["compressed_trailing_bytes"] == len(decoder.unused_data), "complete C decompression and consumption")
    require(original["drain"] == {"status": "drain", "hex": raw.hex()} and original["root"]["status"] == "root" and
            actual["nbt_status"] == "ok" and actual["nbt_consumed"] == len(raw) and actual["raw_trailing_bytes"] == 0,
            "actual complete decompressed stream/root")
    c_wire, original_wire = bytes.fromhex(actual["canonical_hex"]), bytes.fromhex(original["root"]["canonical_hex"])
    require(nbt.compound_order_wire(c_wire) == nbt.compound_order_wire(original_wire), "canonical compound differs beyond child order")
    return {"profile": "compatible", "kind": "root", "compound_order_only": c_wire != original_wire}


def validate_c(value):
    processing = ("payload_hex", "compression_status", "compressed_consumed", "compressed_trailing_bytes", "raw_hex",
                  "nbt_status", "nbt_consumed", "raw_trailing_bytes", "canonical_hex")
    keys(value, ("open_status", "view", "locations", "timestamps", "entry", "record_status", *processing), "complete C record")
    require(value["open_status"] in ("ok", "invalid argument", "truncated region header", "resource limit"), "C open status")
    if value["open_status"] != "ok":
        require(all(value[key] is None for key in value if key != "open_status"), "rejected C open has no downstream state")
        return
    keys(value["view"], ("length", "trailing_bytes", "present_entries", "invalid_entries"), "C view")
    for key, maximum in (("length", 16 * 1024 * 1024), ("trailing_bytes", 4095), ("present_entries", 1024), ("invalid_entries", 1024)):
        integer(value["view"][key], 0, maximum, "C view " + key)
    for key in ("locations", "timestamps"):
        hex_value(value[key], 4096, 4096)
    entry = value["entry"]
    if entry is not None:
        keys(entry, ("location", "timestamp", "issues", "record_length", "byte_offset", "extent_bytes", "selector"), "C entry")
        for key in ("location", "timestamp", "record_length"):
            integer(entry[key], 0, (1 << 32) - 1, "C entry " + key)
        integer(entry["issues"], 0, 127, "C diagnostics")
        integer(entry["selector"], 0, 255, "C selector")
        for key in ("byte_offset", "extent_bytes"):
            integer(entry[key], 0, (1 << 64) - 1, "C entry " + key)
    require(value["record_status"] in ("ok", "invalid argument", "invalid local coordinate", "missing region entry", "invalid region record", "unsupported compression selector"), "C record status")
    if value["record_status"] != "ok":
        require(all(value[key] is None for key in processing), "rejected C record has no downstream state")
        return
    hex_value(value["payload_hex"], 16 * 1024 * 1024)
    require(value["compression_status"] in ("ok", "invalid argument", "invalid compressed stream", "truncated compressed stream", "resource limit", "allocation failed", "compression backend error"), "C compression status")
    if value["compression_status"] != "ok":
        require(all(value[key] is None for key in processing[2:]), "failed C decompression has no downstream state")
        return
    for key in ("compressed_consumed", "compressed_trailing_bytes"):
        integer(value[key], 0, len(value["payload_hex"]) // 2, "C compression consumption")
    require(value["compressed_consumed"] + value["compressed_trailing_bytes"] == len(value["payload_hex"]) // 2,
            "complete C compressed consumption")
    hex_value(value["raw_hex"], 1024 * 1024)
    require(value["nbt_status"] in ("ok", "invalid argument", "truncated input", "invalid tag type", "invalid length", "invalid modified UTF-8", "resource limit", "allocation failed", "invalid tag tree", "output buffer too small", "root is not a compound"), "C NBT status")
    if value["nbt_status"] == "ok":
        hex_value(value["canonical_hex"], 1024 * 1024, 4)
    else:
        require(value["canonical_hex"] is None, "failed C NBT has no serialization")
    if value["nbt_consumed"] is None or value["raw_trailing_bytes"] is None:
        require(value["nbt_status"] != "ok" and value["nbt_consumed"] is None and value["raw_trailing_bytes"] is None,
                "C parse-failure consumption state")
    else:
        for key in ("nbt_consumed", "raw_trailing_bytes"):
            integer(value[key], 0, len(value["raw_hex"]) // 2, "C NBT consumption")
        require(value["nbt_consumed"] + value["raw_trailing_bytes"] == len(value["raw_hex"]) // 2, "complete C raw consumption")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--tested-commit", default="")
    for name in ("java", "javac", "javap"):
        parser.add_argument("--" + name)
    args = parser.parse_args(argv)
    report, common, output, inputs = None, None, None, {}
    status = 1
    try:
        require(Path(__file__).resolve() == (ROOT / "scripts/reference/region/compare_region.py").resolve(), "canonical executing source identity")
        common, nbt, fixture, executed = helpers()
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "full lowercase source ID")
        require(not any(os.environ.get(name) for name in JAVA_ENVIRONMENT), "injected Java environment")
        tools = common.runtime_tools(args)
        release = tools["jdk_release"].read_text(encoding="utf-8")
        version = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if version is None or int(version.group(1)) < 17:
            raise common.Unavailable("region observation requires JDK17 or newer")
        if not args.executable.is_file():
            raise common.Unavailable("missing C region executable")
        jars = {side: getattr(args, side).resolve() for side in JAR_HASHES}
        for side, jar in jars.items():
            if not jar.is_file():
                raise common.Unavailable("missing supplied " + side + " JAR")
            require(common.file_record(jar, byte_limit=2 * 1024 * 1024)["sha256"] == JAR_HASHES[side], "original JAR hash")
        corpus = plans(fixture.cases())
        sources = [ROOT / "tests/reference/region/RegionObservation.java"]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT / "CMakeLists.txt"] + sources
        attributed += list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h"))
        attributed += [ROOT / "tests/storage/region/region_probe.c"] + [Path(item["path"]) for item in executed]
        attributed += list(jars.values()) + list(tools.values())
        attributed.append(args.executable.resolve(strict=True))
        inputs = {str(path.resolve()): common.file_record(path, byte_limit=512 * 1024 * 1024) for path in attributed}
        for item in executed:
            require(inputs[str(Path(item["path"]).resolve())]["sha256"] == item["sha256"], "executed-source attribution")
        work = common.no_link_components(args.work_dir)
        common.samefile_or_ancestor(work, [Path(name) for name in inputs])
        work.mkdir(parents=True, exist_ok=True)
        output = work / ("capture-" + uuid.uuid4().hex)
        output.mkdir()
        output = output.resolve()
        report = {"schema": 1, "status": "running", "tested_commit": args.tested_commit or None,
                  "mode": "live-original-three-way", "fixture_definitions": 36, "queries_per_side": 49,
                  "source_label": "Caller label; actual source, helper and executable fingerprints retained.",
                  "started_utc": common.utc_now(), "artifact_root": str(output), "python": sys.version,
                  "platform": platform.platform(), "inputs_before": inputs, "executed_source_modules": executed,
                  "cases": [], "errors": [], "limits": {"child_seconds": 90,
                  "heap_mib": 512, "stream_bytes": MAX_BYTES, "copies_per_original": 49,
                  "policy": "Bounded children and observed inventories, not an OS process-tree or filesystem quota."}}
        common.write_json(output / "manifest.json", report)
        for name in ("source", "classes", "empty", "compile-work"):
            (output / name).mkdir()
        for source in sources:
            (output / "source" / source.name).write_bytes(source.read_bytes())
        common.tool_versions(tools, output, report)

        def run(name, command, directory, seconds=90, world=None, stdin=None):
            process = common.run_live(command, directory / (name + "-out.txt"), directory / (name + "-err.txt"),
                                      directory, seconds=seconds, stdout_limit=MAX_BYTES, stderr_limit=MAX_BYTES, world_root=world, stdin_path=stdin)
            require(not process.stop_reason and process.returncode == 0,
                    name + " failed: " + str(asdict(process)))
            return asdict(process)

        report["compile"] = run("compile", [str(tools["javac"]), "-J-Xmx512m", "--release", "17",
            "-proc:none", "-implicit:none", "-encoding", "UTF-8", "-classpath", str(output / "empty"),
            "-sourcepath", str(output / "empty"), "--module-path", str(output / "empty"),
            "-d", str(output / "classes")] + [str(output / "source" / path.name) for path in sources], output / "compile-work", 30)
        classes = sorted((output / "classes").glob("*.class"))
        names = ("RegionObservation",)
        require(1 <= len(classes) <= 8 and all(any(path.stem == n or path.stem.startswith(n + "$") for n in names) for path in classes),
                "compiled helper inventory")
        report["classes_before"] = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024) for path in classes}
        original_inputs = output / "inputs"
        original_inputs.mkdir()
        files = {}
        for case in fixture.cases():
            path = original_inputs / (case["label"] + ".fixture")
            path.write_bytes(case["bytes"])
            files[case["label"]] = path
        report["fixture_inputs_before"] = {name: common.file_record(path, byte_limit=65536) for name, path in files.items()}
        requests = output / "c-requests.tsv"
        requests.write_text("".join(str(files[plan["label"]]) + "\t" + "\t".join(map(str, plan["coordinates"])) + "\n" for plan in corpus), encoding="utf-8", newline="\n")
        report["c_process"] = run("c", [str(args.executable.resolve())], output, stdin=requests)
        actual = [common.strict_json(line) for line in (output / "c-out.txt").read_bytes().splitlines()]
        require(len(actual) == len(corpus), "all C query records")
        originals = {}
        report["sides"] = {}
        for side, jar in jars.items():
            side_row = {}
            report["sides"][side] = side_row
            for mode in ("observed", "control"):
                directory = output / (side + "-" + mode)
                directory.mkdir()
                copies_root = directory / "copies"
                copies_root.mkdir()
                copies = []
                for plan in corpus:
                    path = copies_root / (str(plan["index"]) + "-" + plan["label"] + ".fixture")
                    path.write_bytes(plan["bytes"])
                    copies.append(path)
                request = directory / "requests.tsv"
                request.write_text("".join(str(path) + "\t" + "\t".join(map(str, plan["coordinates"])) + "\n" for path, plan in zip(copies, corpus)), encoding="utf-8", newline="\n")
                snapshot = directory / "snapshots.json"
                side_row[mode + "_process"] = run(side + "-" + mode,
                    [str(tools["java"]), "-Xmx512m", "-cp", str(output / "classes"), "RegionObservation",
                     str(jar), side, str(copies_root), str(request), str(snapshot)], directory, world=copies_root)
                require(snapshot.stat().st_size <= MAX_BYTES, "complete original record bound")
                value = common.strict_json(snapshot.read_bytes())
                validate_original(value, side, corpus, copies, common, jar)
                side_row[mode + "_record"] = common.file_record(snapshot, byte_limit=MAX_BYTES)
                side_row[mode + "_copies"] = common.world_inventory(copies_root, hashes=True)
                require(side_row[mode + "_copies"]["complete"], "complete original-copy inventory")
                if mode == "observed":
                    originals[side] = value
                else:
                    require(snapshot.read_bytes() == (output / (side + "-observed") / "snapshots.json").read_bytes(), "entire untouched original control differs")
                    side_row["control_equal"] = True
            for c_value, original, plan in zip(actual, originals[side]["records"], corpus):
                try:
                    comparison = compare_c(c_value, original, plan, nbt)
                except Failure as error:
                    raise Failure(f"C/{side} query {plan['index']} {plan['label']} {plan['coordinates']}: {error}") from error
                if side == "client":
                    report["cases"].append({"index": plan["index"], "fixture": plan["label"], "coordinates": list(plan["coordinates"]),
                        "input": report["fixture_inputs_before"][plan["label"]], "C": c_value, "comparisons": {"C/client": comparison}})
                else:
                    report["cases"][plan["index"]]["comparisons"]["C/server"] = comparison
            common.write_json(output / "manifest.json", report)
        require(originals["client"]["records"] == originals["server"]["records"], "full client/server original records differ")
        for row in report["cases"]:
            row["comparisons"]["client/server"] = "pass"
        report["fixture_inputs_after"] = {name: common.file_record(path, byte_limit=65536) for name, path in files.items()}
        require(report["fixture_inputs_after"] == report["fixture_inputs_before"], "immutable fixture inputs changed")
        report["compatible_queries"] = sum(row["comparisons"]["C/client"]["profile"] == "compatible" for row in report["cases"])
        report["diagnostic_departures"] = len(corpus) - report["compatible_queries"]
        report["status"], status = "pass", 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable)
        status = 77 if unavailable else 1
        if report is not None:
            report["status"] = "unavailable" if unavailable else "fail"
            report["errors"].append(str(error))
        print(("UNAVAILABLE: " if unavailable else "FAILURE: ") + str(error), file=sys.stderr)
    finally:
        if report is not None:
            report["inputs_after"] = {}
            for name, before in inputs.items():
                try:
                    after = common.file_record(Path(name), byte_limit=before["bytes"])
                    report["inputs_after"][name] = after
                    require(after == before, "input changed: " + name)
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            if "classes_before" in report:
                try:
                    after = {path.name: common.file_record(path, byte_limit=2 * 1024 * 1024)
                             for path in sorted((output / "classes").glob("*.class"))}
                    report["classes_after"] = after
                    require(after == report["classes_before"], "compiled helpers changed")
                except Exception as error:
                    report["errors"].append(str(error))
                    report["status"], status = "fail", 1
            report["finished_utc"] = common.utc_now()
            common.write_json(output / "manifest.json", report)
            print(report["status"].upper() + ": " + str(output / "manifest.json"))
    return status


if __name__ == "__main__":
    sys.exit(main())
