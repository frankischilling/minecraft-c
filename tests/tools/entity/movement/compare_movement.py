"""Compare native movement with the retained, explicitly partial original family.

This consumer never imports observation producers or launches Java. Native stream
bytes go directly from pipes to deterministic gzip files; expected rows remain in
their original captures. See --help for the qualified scope and build contract.
"""

from __future__ import annotations

import argparse
import collections
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[4]
TOTAL = 59654
SERVER_EXECUTED = 35748
RESERVE = 128 * 1024 * 1024
PROBE = "tests/entity/movement/movement_probe.c"
CONTROLLER = "tests/tools/entity/movement/compare_movement.py"
HISTORIC_ROOT = PureWindowsPath(r"C:\Users\admin\minecraft")
HISTORIC_FAMILY = HISTORIC_ROOT / ".local/population-staging/entity-movement-original-a"
ANCHORS = {
    "qualification-a/report.json": (46996, "553ceadaaca4e24087cb1d3808d09eeb2cf6fd2835610074f3680018caba0dfa"),
    "qualification-a/server.scope.json": (895959, "fb7cc810118f7d0eaf00aa709d649ff926237a2cfb7d4c4153b0ad2f2227986b"),
    "preflight-a.json": (18652, "1f0eb5611227cb417fd9f8b0e7bd1bb6da2e26206d64f4d4ea35ea82f1302c82"),
    "execution-a/input.tsv": (20669974, "dba278637d7eebe9a6cae7c6b688bcce7d8eab47552d4ecfb573434c6e706894"),
}
OWNER_FACTS = "box=e0,watcher=w0,random=r0,World=null,passenger=null,riding=null"
OPERATIONS = {
    "move", "flag", "sneak", "fall-state", "fall", "damage", "fire", "wet",
    "attacked", "dead", "portal", "walk-enabled", "contact", "damage-seq", "fire-seq",
}
LIMITATIONS = [
    "Original qualification is complete client plus an intact server subset, not a complete paired family.",
    "Server original exit code is unknown; ENOSPC left empty final receipts and a stream-write failure.",
    "Original historical before-pin objects did not survive; frozen producer policy and retained after-pins are separate evidence.",
    "Server state-only calls preserve native pool history and have no accepted original result comparison.",
    "Watcher preparation uses explicit native typed private fields, not a completed production watcher-update method.",
    "Fresh Item constructor entropy is retained independently and is not compared across processes.",
    "The native probe relinks immutable qualified core archives; this is not a fresh core build or a full current test-suite run.",
    "Only actual Item and Falling constructors with NULL World are covered; no complete gameplay or compression-runtime claim.",
    "No original JVM, private corpus/model producer, or private qualifier is executed by this controller.",
]


class Invalid(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise Invalid(message)


def fingerprint(path):
    digest = hashlib.sha256()
    size = 0
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(block)
            digest.update(block)
    return {"bytes": size, "sha256": digest.hexdigest()}


def fp_only(value):
    require(isinstance(value, dict), "Fingerprint must be an object")
    size, digest = value.get("bytes"), value.get("sha256")
    require(type(size) is int and size >= 0, "Invalid fingerprint byte count")
    require(isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest),
            "Invalid fingerprint SHA-256")
    return {"bytes": size, "sha256": digest}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON member: " + key)
        result[key] = value
    return result


def load_json(path):
    with Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream, object_pairs_hook=unique_object)


def write_json(path, value):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=True)
        stream.write("\n")


def json_array(path):
    """Read one JSON array member at a time, including complete trailing validation."""
    decoder = json.JSONDecoder(object_pairs_hook=unique_object)
    with Path(path).open("r", encoding="utf-8", newline="") as stream:
        buffer, position, eof = "", 0, False

        def refill():
            nonlocal buffer, position, eof
            buffer = buffer[position:] + stream.read(65536)
            position = 0
            eof = len(buffer) == 0 or stream.tell() == os.fstat(stream.fileno()).st_size

        def whitespace():
            nonlocal position
            while True:
                while position < len(buffer) and buffer[position].isspace():
                    position += 1
                if position < len(buffer) or eof:
                    return
                refill()

        refill()
        whitespace()
        require(position < len(buffer) and buffer[position] == "[", "Expected JSON array")
        position += 1
        first = True
        while True:
            whitespace()
            require(position < len(buffer), "Truncated JSON array")
            if buffer[position] == "]":
                position += 1
                break
            if not first:
                require(buffer[position] == ",", "Missing JSON array separator")
                position += 1
                whitespace()
                require(position < len(buffer) and buffer[position] != "]", "Trailing JSON comma")
            while True:
                try:
                    value, end = decoder.raw_decode(buffer, position)
                    position = end
                    break
                except json.JSONDecodeError:
                    if eof:
                        raise
                    refill()
            yield value
            first = False
        whitespace()
        require(position == len(buffer) and eof, "Trailing JSON array data")


def relative_path(key):
    require(isinstance(key, str) and "\\" not in key, "Expected slash-separated relative path")
    path = PurePosixPath(key)
    require(not path.is_absolute() and bool(path.parts) and
            all(part not in ("..", ".") for part in path.parts) and ":" not in key,
            "Unsafe relative path: " + key)
    return ROOT.joinpath(*path.parts)


class Pins:
    def __init__(self):
        self.values = {}
        self.native_inputs = None

    def add(self, path, expected=None):
        path = Path(path).resolve(strict=True)
        actual = fingerprint(path)
        if expected is not None:
            require(actual == fp_only(expected), "Fingerprint mismatch: " + str(path))
        key = str(path)
        require(key not in self.values or self.values[key] == actual,
                "Conflicting evidence pin: " + key)
        self.values[key] = actual
        return actual

    def recheck(self):
        failures = []
        if self.native_inputs is not None and self.native_inputs != native_inventory():
            failures.append("Native input inventory changed")
        for name, before in self.values.items():
            try:
                require(fingerprint(name) == before, "Changed evidence: " + name)
            except (OSError, Invalid) as error:
                failures.append(str(error))
        require(not failures, "Before/after pin failures: " + "; ".join(failures))


def pin_objects(value):
    if isinstance(value, dict):
        if {"path", "bytes", "sha256"}.issubset(value):
            yield value
        for child in value.values():
            yield from pin_objects(child)
    elif isinstance(value, list):
        for child in value:
            yield from pin_objects(child)


def retained_path(name, original):
    """Map only the frozen family, two authority files, source plan and JARs."""
    path = PureWindowsPath(name)
    try:
        relative = path.relative_to(HISTORIC_FAMILY)
        require(".." not in relative.parts, "Unsafe retained family path")
        return original.joinpath(*relative.parts)
    except ValueError:
        pass
    allowed = {
        "b1.7.3.jar", "b1.7.3_server.jar",
        ".local/population-staging/entity-movement-plan-a.md",
        ".local/population-staging/provider-activation-retained-78965c1/authority/client.javap",
        ".local/population-staging/provider-activation-retained-78965c1/authority/server.javap",
    }
    try:
        relative = path.relative_to(HISTORIC_ROOT).as_posix()
    except ValueError:
        relative = None
    if relative in allowed:
        return relative_path(relative)
    java_root = PureWindowsPath(r"C:\Program Files\Eclipse Adoptium\jdk-17.0.17.10-hotspot\bin")
    require(path in (java_root / "java.exe", java_root / "javac.exe"),
            "Unrecognized historical evidence path: " + name)
    return None  # Historical external Windows tools: never invoked or remapped on Linux.


def audit_authority(preflight, original, pins):
    result = {}
    for side in ("client", "server"):
        audit = preflight["descriptor_audits"][side]
        path = retained_path(audit["authority"]["path"], original)
        pins.add(path, audit["authority"])
        bodies = audit["selected_complete_bodies"]
        require(len(bodies) == 29, "Unexpected authority method inventory")
        requests = collections.defaultdict(list)
        for body in bodies:
            require(type(body["first_LF"]) is int and
                    0 < body["first_LF"] <= body["last_LF"], "Invalid authority range")
            requests[body["first_LF"]].append(body)
        active, completed = [], []
        with path.open("rb") as stream:
            for number, line in enumerate(stream, 1):
                for body in requests.get(number, []):
                    active.append([body, hashlib.sha256(), 0])
                remaining = []
                for body, digest, size in active:
                    digest.update(line)
                    size += len(line)
                    if number == body["last_LF"]:
                        require({"bytes": size, "sha256": digest.hexdigest()} == fp_only(body),
                                "Authority body mismatch: " + side + " " + body["owner"] + "." + body["method"])
                        completed.append(body)
                    else:
                        remaining.append([body, digest, size])
                active = remaining
        require(len(completed) == 29 and not active, "Incomplete authority audit")
        result[side] = {"authority": audit["authority"], "complete_bodies": completed}
    return result


def validate_evidence(original, pins):
    for relative, (size, digest) in ANCHORS.items():
        pins.add(original / relative, {"bytes": size, "sha256": digest})
    qualification = load_json(original / "qualification-a/report.json")
    require(qualification["status"] == "QUALIFIED_COMPLETE_CLIENT_AND_INTACT_SERVER_SUBSET" and
            qualification["errors"] == [] and qualification["full_paired_family_complete"] is False,
            "Original qualification status is not the accepted partial family")
    scope = load_json(original / "qualification-a/server.scope.json")
    expected = list(range(24073)) + list(range(25412, 25442)) + list(range(35742, 35748))
    require(scope["intact_ids"] == [case_id(i) for i in expected], "Server selection differs from frozen scope")
    selected = set(expected)
    require(scope["missing_or_corrupt_ids"] == [case_id(i) for i in range(TOTAL) if i not in selected],
            "Server missing-row inventory differs")
    require(scope["gaps"] == [[24072, 25412], [25441, 35742]], "Server gap inventory differs")
    require(qualification["client"]["rows"] == TOTAL and
            qualification["client"]["all_actual_exception_traces"] == 10874 and
            qualification["server"]["intact25field_rows"] == 24109 and
            qualification["server"]["exact_exit_code"] is None,
            "Original qualification counts differ")
    closure = qualification["process_closure"]
    require(closure["client_durable_exit_code"] == 0 and closure["server_exit_code"] is None and
            closure["outer_exit_code"] == 1 and closure["termination"] is False and
            closure["restart"] is False and closure["no_live_original_handle"] is True,
            "Original process closure differs")
    external = []
    for item in pin_objects(qualification):
        path = retained_path(item["path"], original)
        if path is None:
            external.append(item)
        else:
            pins.add(path, item)
    preflight = load_json(original / "preflight-a.json")
    require(preflight["status"] == "PASS" and preflight["rows_per_side"] == TOTAL and
            preflight["input_fields"] == 20 and preflight["output_fields"] == 25,
            "Original preflight contract differs")
    for item in pin_objects(preflight):
        path = retained_path(item["path"], original)
        if path is not None:
            pins.add(path, item)
    authority = audit_authority(preflight, original, pins)
    return qualification, scope, expected, {
        "qualification": fingerprint(original / "qualification-a/report.json"),
        "scope": fingerprint(original / "qualification-a/server.scope.json"),
        "authority": authority,
        "historical_external_tools_not_invoked": external,
        "original_process_closure": closure,
        "original_limitations": qualification["limitations"],
    }


def native_inventory():
    keys = {"CMakeLists.txt", ".clang-format"}
    for directory in ("include", "src", "tests", "scripts"):
        for path in (ROOT / directory).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc":
                keys.add(path.relative_to(ROOT).as_posix())
    return keys


def validate_build(receipt_path, executable, pins):
    pins.add(receipt_path)
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == 1 and receipt.get("status") == "passed", "Invalid build receipt status")
    binary = receipt["binary"]
    require(Path(binary["path"]).resolve(strict=True) == executable, "Build receipt binary path differs")
    pins.add(executable, binary)
    inputs = receipt["native_inputs"]
    require(set(inputs) == native_inventory(), "Build receipt does not inventory all current native inputs")
    pins.native_inputs = set(inputs)
    for key, value in inputs.items():
        pins.add(relative_path(key), value)
    probe = receipt["compiled_probe_source"]
    require(probe["path"] == PROBE and fp_only(probe) == fp_only(inputs[PROBE]),
            "Compiled probe source does not match current input pin")
    require(CONTROLLER in inputs, "Controller omitted from build receipt")
    core = receipt["qualified_core"]
    source_receipt = core["source_report"]
    source_path = Path(source_receipt["path"]).resolve(strict=True)
    pins.add(source_path, source_receipt)
    report = load_json(source_path)
    require(report["status"] == "passed" and not report.get("retention_errors"),
            "Qualified core report did not pass retention")
    require(core["source_root"] == report["source_root"], "Qualified core source root differs")
    require(report["inputs_before"] == report["inputs_after"] and
            report["dependencies_before"] == report["dependencies_after"],
            "Qualified core before/after inputs differ")
    production = core["production_inputs"]
    required = {key for key in inputs if key.startswith(("include/", "src/"))}
    require(required.issubset(production), "Qualified core omits current production inputs")
    for key, value in production.items():
        require(key in inputs and fp_only(value) == fp_only(inputs[key]) and
                key in report["inputs_before"] and
                fp_only(value) == fp_only(report["inputs_before"][key]),
                "Current/core input join failed: " + key)
    # Fixture inclusion is part of the compiled probe translation unit, not merely
    # an unrelated current-tree pin. Require its immutable qualified input too.
    probe_text = relative_path(PROBE).read_text(encoding="utf-8")
    fixtures = []
    for include in re.findall(r'^\s*#\s*include\s+"([^"]+\.c)"', probe_text, re.MULTILINE):
        path = (relative_path(PROBE).parent / include).resolve(strict=True)
        key = path.relative_to(ROOT).as_posix()
        require(key in production, "Probe fixture missing qualified-core join: " + key)
        fixtures.append(key)
    libraries, product_keys = core["libraries"], core["product_keys"]
    require(bool(libraries) and set(libraries) == set(product_keys), "Core library product keys differ")
    for name, value in libraries.items():
        key = product_keys[name]
        require(key in report["products"] and fp_only(value) == fp_only(report["products"][key]),
                "Qualified archive product join failed: " + name)
        pins.add(name, value)
    dependencies = core["dependency_inputs"]
    require(bool(dependencies) and set(dependencies) == set(report["dependencies_before"]),
            "Qualified dependency input inventory differs")
    for key, value in dependencies.items():
        require(key in report["dependencies_before"] and
                fp_only(value) == fp_only(report["dependencies_before"][key]),
                "Qualified dependency join failed: " + key)
        pins.add(relative_path(key), value)
    for name, value in receipt.get("link_dependencies", {}).items():
        pins.add(name, value)
    require(bool(receipt["tools"]), "Build tools not pinned")
    for name, value in receipt["tools"].items():
        pins.add(name, value)
    require(Path(sys.executable).resolve(strict=True) in
            {Path(name).resolve(strict=True) for name in receipt["tools"]},
            "Executing Python interpreter omitted from tool pins")
    commands = receipt["commands"]
    require(bool(commands) and bool(core["compile_profile"]), "Build commands/profile missing")
    all_tokens = []
    for command in commands:
        require(command["exit_code"] == 0 and command["reaped"] is True and
                command["terminated"] is False and command["timeout"] is None and
                command["output_cap"] is None, "Build command lacks natural uncapped zero exit")
        tokens = command["command"]
        require(isinstance(tokens, list) and tokens and all(isinstance(t, str) for t in tokens),
                "Build command must retain exact argument vector")
        all_tokens.extend(tokens)
    require(not any(t.lower() in ("-ffast-math", "-ofast", "/fp:fast") for t in all_tokens),
            "Unsupported fast floating-point build option")
    require(any(t.lower() in ("/fp:strict", "-fno-fast-math", "-ffp-model=strict") for t in all_tokens),
            "Strict floating-point compile option absent from build command")
    return {"receipt": {"path": str(receipt_path), **fingerprint(receipt_path)},
            "qualified_source_report": source_receipt, "qualified_source_root": core["source_root"],
            "qualified_libraries": libraries, "product_keys": product_keys,
            "current_only_link_dependencies": receipt.get("link_dependencies", {}),
            "compile_profile": core["compile_profile"], "compiled_fixtures": fixtures,
            "commands": commands, "tools": receipt["tools"],
            "claim": "new probe relink against byte-identical qualified core; no new full core build"}


def case_id(number):
    return "e%06d" % number


def ordinal(value):
    require(re.fullmatch(r"e[0-9]{6}", value) is not None, "Invalid row ID")
    number = int(value[1:])
    require(0 <= number < TOTAL, "Out-of-range row ID")
    return number


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    require(re.fullmatch(r"0|-?[1-9][0-9]*", value) is not None, "Noncanonical decimal: " + value)
    parsed = int(value)
    require(low <= parsed <= high, "Out-of-range decimal: " + value)
    return parsed


def raw_vector(value, count, width):
    parts = value.split(",")
    require(len(parts) == count and all(re.fullmatch(r"[0-9a-f]{%d}" % width, p) for p in parts),
            "Invalid raw-bit vector")
    return parts


def input_fields(line, number):
    require(line.endswith(b"\n") and b"\r" not in line, "Input must have complete LF records")
    fields = line[:-1].decode("ascii").split("\t")
    require(len(fields) == 20 and fields[0] == case_id(number) and
            fields[1] == ("I" if number < TOTAL // 2 else "F") and fields[2] in OPERATIONS,
            "Input row identity/schema differs")
    flags = fields[3].split(",")
    require(len(flags) == 6 and set(flags) <= {"0", "1"}, "Invalid flags")
    integer(fields[4], -128, 127)
    for index, count, width in ((5, 6, 16), (6, 3, 16), (7, 3, 16), (12, 3, 16)):
        raw_vector(fields[index], count, width)
    for index in (8, 9, 10, 11, 15):
        raw_vector(fields[index], 1, 8)
    integer(fields[13])
    require(fields[14] in ("0", "1") and fields[16] in ("reset", "used", "alias"), "Invalid input primitive/setup")
    repeat = integer(fields[17], 1)
    if fields[2].endswith("-seq"):
        values = fields[18].split(",")
        require(len(values) == repeat, "Sequence length differs")
        for value in values:
            integer(value)
    else:
        require(repeat == 1 and fields[18] == "-", "Unexpected repeated call")
    contact = integer(fields[19], 0)
    require((fields[2] == "contact" and contact in (1, 28, 30, 70, 72, 81, 88, 90)) or
            (fields[2] != "contact" and contact == 0), "Unexpected contact ID")
    return fields


class Inputs:
    def __init__(self, path):
        self.path = path
        self.offsets = []

    def validate(self, cases_path, categories):
        observed = collections.Counter()
        with self.path.open("rb") as stream:
            for number, case in enumerate(json_array(cases_path)):
                require(number < TOTAL, "Too many cases")
                self.offsets.append(stream.tell())
                line = stream.readline()
                fields = input_fields(line, number)
                sequence = ",".join(map(str, case["sequence"])) if case["sequence"] else "-"
                reconstructed = [case["id"], case["kind"], case["operation"],
                                 ",".join(map(str, case["flags"])), str(case["watcher"]),
                                 ",".join(case["bounds"]), ",".join(case["positions"]),
                                 ",".join(case["velocities"]), case["offset"], case["ysize"],
                                 case["fall"], case["step"], ",".join(case["delta"]),
                                 str(case["integer"]), str(case["boolean"]), case["single"],
                                 case["pool"], str(case["repeat"]), sequence, str(case["contact"])]
                require(reconstructed == fields and case["line"] == "\t".join(fields),
                        "Cases/input join differs: " + fields[0])
                observed[case["category"]] += 1
            require(len(self.offsets) == TOTAL and stream.read(1) == b"", "Input count/remainder differs")
        require(dict(observed) == categories, "Case categories differ from qualification")
        return dict(observed)

    def row(self, number, stream):
        stream.seek(self.offsets[number])
        return input_fields(stream.readline(), number)


def quarter_nonzero(bits, web):
    # Only zero classification is needed for the reached query PC. A binary64
    # subnormal of magnitude <=2 rounds to zero on multiplication by exact .25.
    # Every normal, infinity and NaN is nonzero for Java dcmpl/if comparisons.
    magnitude = int(bits, 16) & ((1 << 63) - 1)
    return magnitude > (2 if web else 0)


def failure(fields):
    flags = list(map(int, fields[3].split(",")))
    if fields[2] == "move" and not flags[0]:
        pc = 394
        if flags[2] and integer(fields[4], -128, 127) & 2:
            delta = fields[12].split(",")
            if quarter_nonzero(delta[0], flags[1]):
                pc = 232
            elif quarter_nonzero(delta[2], flags[1]):
                pc = 313
        return "MOVE", pc
    if fields[2] == "wet" and not flags[5]:
        return "EFFECT", 32
    if fields[2] == "contact" and fields[19] in ("28", "70", "72"):
        return "CONTACT", 1
    return None


def setup(fields, native):
    prefix = ("AABB-assign;private-scalar-fields;private-typed-Byte0;watcher-byte-read;Random.setSeed:"
              if native else "AABB-set;scalar-fields;watcher-update;watcher-byte-read;Random.setSeed:")
    value = prefix + "%016x" % (0x173E0000 + ordinal(fields[0])) + ";pool-reset"
    if fields[16] in ("used", "alias"):
        value += ";pooled-anchor"
    if fields[16] == "alias":
        value += ";pool-reset"
    return value


def snapshot(value, kind):
    groups = value.split("/")
    require(len(groups) == 7, "Snapshot group count differs")
    raw_vector(groups[0], 3, 16)
    raw_vector(groups[1], 3, 16)
    raw_vector(groups[2], 7, 8)
    flags = groups[3].split(",")
    require(len(flags) == 11 and set(flags) <= {"0", "1"}, "Snapshot flags differ")
    values = groups[4].split(",")
    require(len(values) == 3, "Snapshot integer count differs")
    for value in values:
        integer(value)
    if kind == "I":
        integer(groups[5])
    else:
        require(groups[5] == "-", "Falling snapshot has Item state")
    raw_vector(groups[6], 6, 16)
    return groups


def pool_values(value, size):
    if size == 0:
        require(value == "_", "Empty pool contents differ")
        return []
    parts = value.split(";")
    require(len(parts) == size, "Pool length/content differs")
    coordinates = []
    for index, part in enumerate(parts):
        label, separator, raw = part.partition(":")
        require(separator and label == "p%d" % index, "Pool identity label differs")
        raw_vector(raw, 6, 16)
        coordinates.append(raw)
    return coordinates


def validate_output(fields, supplied, constructors, native=False):
    row_id = supplied[0]
    require(len(fields) == 25 and fields[:3] == supplied[:3], "Output row identity/schema differs: " + row_id)
    expected_failure = failure(supplied)
    outcome = ("NATIVE_FAILURE" if native else "java.lang.NullPointerException") if expected_failure else "OK"
    require(fields[3] == outcome and fields[4] in ("-", "0", "1"), "Unexpected outcome: " + row_id)
    require(not expected_failure or fields[4] == "-", "Failure fabricated a return value: " + row_id)
    require(fields[5] == constructors[supplied[1]], "Constructor snapshot differs: " + row_id)
    require(fields[6] == "|".join(supplied), "Actual input reread differs: " + row_id)
    before = snapshot(fields[7], supplied[1])
    snapshot(fields[8], supplied[1])
    ctor = constructors[supplied[1]].split("/")
    prepared = ctor[:]
    prepared[0], prepared[1], prepared[6] = supplied[6], supplied[7], supplied[5]
    scalars = ctor[2].split(",")
    scalars[:4] = supplied[8:12]
    prepared[2] = ",".join(scalars)
    flags = ctor[3].split(",")
    flags[:6] = supplied[3].split(",")
    prepared[3] = ",".join(flags)
    require(before == prepared, "Prepared source fields differ: " + row_id)
    watcher = "w0,wm0,wr0,java.lang.Byte,%s,1,%d" % (supplied[4], int(supplied[4] != "0"))
    require(fields[9:11] == ["e0", "e0"] and fields[11:13] == [watcher, watcher] and
            fields[13] == "g0,stable" and fields[23] == OWNER_FACTS,
            "Owner/watcher/pool descriptor identity differs: " + row_id)
    cursor_before, cursor_after, size_before, size_after = [integer(x, 0) for x in fields[14:18]]
    require(cursor_before <= size_before and cursor_after <= size_after, "Pool cursor outside List")
    require(cursor_before == int(supplied[16] == "used"), "Pool setup cursor differs")
    require(cursor_after == cursor_before + (2 if expected_failure and expected_failure[0] == "MOVE" else 0),
            "Reached source allocation prefix differs: " + row_id)
    require(size_after == max(size_before, cursor_after), "Pool List growth differs: " + row_id)
    pool_values(fields[18], size_before)
    after_pool = pool_values(fields[19], size_after)
    if supplied[16] == "reset":
        require(fields[20:22] == ["-", "-"], "Unexpected pooled anchor")
    else:
        require(fields[20] == "p0" and after_pool and fields[21] == after_pool[0], "Pooled alias differs")
    if int(supplied[17]) != 1:
        calls = fields[22].split(";")
        require(len(calls) == int(supplied[17]), "Intermediate sequence count differs")
        for number, call in enumerate(calls):
            pieces = call.split(":")
            require(len(pieces) == 4 and pieces[0] == str(number) and pieces[1] == "OK" and
                    pieces[2] in ("-", "0", "1") and re.fullmatch(r"[0-9a-f]+", pieces[3]),
                    "Intermediate sequence frame differs")
            snapshot(bytes.fromhex(pieces[3]).decode("ascii"), supplied[1])
    else:
        require(fields[22] == "-", "Unexpected sequence output")
    require(fields[24] == setup(supplied, native), "Setup provenance differs: " + row_id)


def crlf_record(line):
    require(line.endswith(b"\r\n") and b"\r" not in line[:-2] and b"\n" not in line[:-2],
            "Original record framing differs")
    return line[:-2]


def original_rows(original, inputs, scope, selected, constructors):
    directory = original / "execution-a"
    client_offsets, server_offsets = [], {}
    counts = {}
    with inputs.path.open("rb") as source, (directory / "client-original.stdout").open("rb") as raw, \
            (directory / "client.rows.stdout").open("rb") as extracted:
        require([raw.readline() for _ in range(3)] ==
                [b"16 achievements\r\n", b"151 recipes\r\n", b"ENTITY_MOVEMENT_V1\tCLIENT\r\n"],
                "Original client bootstrap differs")
        failed = 0
        for number in range(TOTAL):
            client_offsets.append(raw.tell())
            line = raw.readline()
            require(line == extracted.readline(), "Client extracted/raw row differs")
            fields = crlf_record(line).decode("ascii").split("\t")
            supplied = inputs.row(number, source)
            validate_output(fields, supplied, constructors)
            failed += failure(supplied) is not None
        require(raw.read(1) == b"" and extracted.read(1) == b"", "Original client trailing records")
        require(failed == 10874, "Original client failure inventory differs")
        counts["client"] = {"rows": TOTAL, "successes": TOTAL - failed, "failures": failed}
    malformed = {row["physical_record"]: row for row in scope["malformed_physical_records"]}
    require(set(malformed) == {24077, 24108, 24115}, "Malformed server record locations differ")
    selected_set = set(selected)
    observed, failed = [], 0
    with inputs.path.open("rb") as source, (directory / "client-original.stdout").open("rb") as client, \
            (directory / "server-original.stdout").open("rb") as stream:
        require([stream.readline() for _ in range(3)] ==
                [b"16 achievements\r\n", b"151 recipes\r\n", b"ENTITY_MOVEMENT_V1\tSERVER\r\n"],
                "Original server bootstrap differs")
        physical = 3
        while True:
            offset = stream.tell()
            line = stream.readline()
            if not line:
                break
            physical += 1
            if physical in malformed:
                description = malformed[physical]
                # The frozen receipts hash whole physical records, including
                # CRLF on the two interior records and the entire partial tail.
                payload = line[:-2] if line.endswith(b"\r\n") else line
                require(len(line) == description["bytes"] and
                        hashlib.sha256(line).hexdigest() == description["raw_sha256"] and
                        payload.startswith(description["start"].encode("ascii")),
                        "Malformed server record differs")
                continue
            payload = crlf_record(line)
            fields = payload.decode("ascii").split("\t")
            require(len(fields) == 25, "Unexpected intact server row schema")
            number = ordinal(fields[0])
            require(number in selected_set and number not in server_offsets, "Unexpected intact server ID")
            supplied = inputs.row(number, source)
            validate_output(fields, supplied, constructors)
            client.seek(client_offsets[number])
            require(payload == crlf_record(client.readline()), "Paired retained payload differs")
            server_offsets[number] = offset
            observed.append(number)
            failed += failure(supplied) is not None
        require(physical == 24115 and observed == selected and failed == 4741,
                "Server intact records/count differs")
    counts["server"] = {"rows": len(selected), "successes": len(selected) - failed, "failures": failed,
                        "malformed_records": scope["malformed_physical_records"]}
    valid_count = 0
    for number, row in enumerate(json_array(directory / "client.results.json")):
        require(row == {"id": case_id(number), "valid": True, "failure": None},
                "Client validity record differs")
        valid_count += 1
    require(valid_count == TOTAL, "Client validity count differs")
    return client_offsets, server_offsets, counts


def trace_frame(fields, side):
    reached = failure(fields)
    require(reached is not None, "Unexpected original exception")
    family, pc = reached
    base = "sn" if side == "CLIENT" else "lq"
    if family == "MOVE":
        return "at %s.%s(SourceFile:%d)" % (base, "b" if side == "CLIENT" else "a",
                                            {232: 346, 313: 356, 394: 367}[pc])
    if family == "EFFECT":
        return "at %s.%s(SourceFile:575)" % (base, "af" if side == "CLIENT" else "ac")
    block = ("ph" if side == "CLIENT" else "jn") if fields[19] == "28" else ("bv" if side == "CLIENT" else "bc")
    return "at %s.a(SourceFile:%d)" % (block, 33 if fields[19] == "28" else 79)


def original_traces(original, inputs, selected):
    reports = {}
    selected_set = set(selected)
    for side, expected_count in (("CLIENT", 10874), ("SERVER", 4826)):
        seen, pc_counts, tail = set(), collections.Counter(), []
        with inputs.path.open("rb") as source, \
                (original / ("execution-a/" + side.lower() + "-original.stderr")).open("rb") as stream:
            for line in stream:
                payload = crlf_record(line).decode("ascii")
                if not payload.startswith("EXCEPTION\t"):
                    tail.append(payload)
                    continue
                require(not tail, "Exception after original stderr tail")
                pieces = payload.split("\t")
                require(len(pieces) == 4 and pieces[0] == "EXCEPTION" and pieces[2] == "0" and
                        re.fullmatch(r"[0-9a-f]+", pieces[3]), "Original exception envelope differs")
                number = ordinal(pieces[1])
                require(number not in seen, "Duplicate original exception")
                fields = inputs.row(number, source)
                reached = failure(fields)
                require(reached is not None, "Original exception outside source-derived prefix")
                trace = bytes.fromhex(pieces[3]).decode("utf-8")
                lines = trace.splitlines()
                frame = trace_frame(fields, side)
                helper_row = {"MOVE": 382, "EFFECT": 389, "CONTACT": 394}[reached[0]]
                require(lines and lines[0].split(":", 1)[0] == "java.lang.NullPointerException" and
                        len(lines) >= 4 and lines[1] == "\t" + frame and
                        "\tat EntityMovementObservation.row(EntityMovementObservation.java:%d)" % helper_row in lines and
                        "\tat EntityMovementObservation.main(EntityMovementObservation.java:445)" in lines,
                        "Original trace frame differs: " + fields[0])
                seen.add(number)
                pc_counts["%s:%d" % reached] += 1
        require(len(seen) == expected_count, "Original exception trace count differs")
        if side == "CLIENT":
            require(dict(pc_counts) == {"MOVE:232": 2392, "MOVE:313": 312, "MOVE:394": 8112,
                                       "EFFECT:32": 52, "CONTACT:1": 6},
                    "Source-derived client prefix PC inventory differs")
        with inputs.path.open("rb") as source:
            expected_selected = {i for i in (range(TOTAL) if side == "CLIENT" else selected)
                                 if failure(inputs.row(i, source)) is not None}
        require((seen if side == "CLIENT" else seen & selected_set) == expected_selected,
                "Selected original exception inventory differs")
        expected_tail = [] if side == "CLIENT" else [
            'Exception in thread "main" java.lang.IllegalStateException: stream write failed',
            "\tat EntityMovementObservation.main(EntityMovementObservation.java:450)",
        ]
        require(tail == expected_tail, "Original uncaught stderr tail differs")
        reports[side.lower()] = {"complete_traces": len(seen), "selected_traces": len(expected_selected),
                                "unselected_trace_only": len(seen - expected_selected),
                                "source_prefixes": dict(pc_counts), "uncaught_tail": tail}
    return reports


def validate_entropy(path, inputs, executed, native, allow_partial=False):
    ending = b"\n" if native else b"\r\n"
    header = b"ENTITY_CONSTRUCTOR_ENTROPY_NATIVE_V1" if native else b"ENTITY_CONSTRUCTOR_ENTROPY_V1"
    count, partial = 0, None
    with path.open("rb") as stream:
        require(stream.readline() == header + ending, "Entropy header/framing differs")
        for line in stream:
            if not line.endswith(ending):
                require(allow_partial and stream.read(1) == b"", "Unexpected partial entropy record")
                partial = {"bytes": len(line), "sha256": hashlib.sha256(line).hexdigest()}
                break
            payload = line[:-len(ending)]
            require(b"\r" not in payload and b"\n" not in payload, "Entropy framing differs")
            fields = payload.decode("ascii").split("\t")
            require(len(fields) == 6 and count < executed and fields[0] == case_id(count) and
                    fields[1] == ("I" if count < TOTAL // 2 else "F") and fields[2] == str(count) and
                    fields[4] == ("mc_java_random" if native else "java.util.Random") and
                    fields[5] == "%016x" % (0x173E0000 + count), "Entropy identity/seed differs")
            require((fields[1] == "I" and re.fullmatch(r"[0-9a-f]{8}", fields[3])) or
                    (fields[1] == "F" and fields[3] == "-"), "Entropy primitive field differs")
            count += 1
    require((allow_partial and count == 25306 and partial is not None) or
            (not allow_partial and count == executed and partial is None), "Entropy count differs")
    return {"path": str(path), **fingerprint(path), "complete_records": count,
            "partial_record": partial, "uncontrolled_Item_d_compared": False}


def reserve(path):
    free = shutil.disk_usage(path).free
    require(free >= RESERVE, "Insufficient free space: need 128 MiB reserve before native producer")
    return {"free_bytes": free, "minimum_free_bytes": RESERVE}


def write_ids(path, numbers):
    with path.open("xb") as stream:
        count = 0
        for number in numbers:
            stream.write((case_id(number) + "\n").encode("ascii"))
            count += 1
    return {"path": str(path), **fingerprint(path), "rows": count}


class RetainedSink:
    """Do not drop pending compressed bytes when a filesystem write is partial.

    On storage errors, keep retrying the same bytes. Pipe backpressure is allowed;
    terminating a producer or discarding output is not. Any such error prevents a
    passing receipt even if the filesystem later recovers. There is no timeout.
    """
    def __init__(self, path):
        self.stream = path.open("xb", buffering=0)
        self.errors = []

    def write(self, data):
        view, written = memoryview(data), 0
        while written < len(view):
            try:
                count = self.stream.write(view[written:])
                if not count:
                    raise OSError("Zero-byte compressed capture write")
                written += count
            except OSError as error:
                message = str(error)
                if message not in self.errors:
                    self.errors.append(message)
                    print("Capture storage error; retaining pending bytes and waiting: " + message,
                          file=sys.stderr, flush=True)
                time.sleep(1)
        return written

    def flush(self):
        self.stream.flush()

    def close(self):
        self.stream.close()


def capture_child(executable, side, directory, input_path, executed, selected_count, report):
    report.update({"side": side, "executed": executed, "selected": selected_count,
                   "state_only": executed - selected_count, "timeout": None, "output_cap": None,
                   "terminated": False, "reaped": False, "exit_code": None,
                   "disk_reserve": reserve(directory)})
    command = [str(executable), "entity-movement", side, "selection.ids", "entropy.tsv"]
    report["command"] = command
    report["cwd"] = str(directory)
    write_json(directory / "command.json", report)
    sinks, compressors = {}, {}
    for label in ("stdout", "stderr"):
        sinks[label] = RetainedSink(directory / (label + ".gz"))
        compressors[label] = gzip.GzipFile(filename="", mode="wb", compresslevel=6,
                                           fileobj=sinks[label], mtime=0)
    errors, captures = [], {}
    process, threads, interrupted = None, {}, False
    ready = threading.Event()

    def drain(label):
        digest, size = hashlib.sha256(), 0
        pipe = None
        try:
            ready.wait()
            if process is not None:
                pipe = getattr(process, label)
                while True:
                    block = pipe.read(65536)
                    if not block:
                        break
                    digest.update(block)
                    size += len(block)
                    compressors[label].write(block)
        except BaseException as error:
            errors.append(label + ": " + repr(error))
        finally:
            try:
                if pipe is not None:
                    pipe.close()
                compressors[label].close()
                sinks[label].close()
            except BaseException as error:
                errors.append(label + " close: " + repr(error))
            captures[label] = {"raw": {"bytes": size, "sha256": digest.hexdigest()},
                               "encoding": "gzip", "mtime": 0, "retention": "gzip-from-first-byte",
                               "storage_errors": sinks[label].errors}

    started = time.monotonic()
    try:
        # Establish both drainers before a producer exists. A failed worker start
        # must never leave a live child with an undrained pipe.
        for label in ("stdout", "stderr"):
            thread = threading.Thread(target=drain, args=(label,), name="capture-" + label)
            threads[label] = thread
            thread.start()
        with input_path.open("rb") as stdin:
            process = subprocess.Popen(command, cwd=directory, stdin=stdin,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       start_new_session=os.name != "nt",
                                       creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0)
            report["pid"] = process.pid
            ready.set()
            try:
                write_json(directory / "live.json", report)
            except BaseException as error:
                errors.append("live receipt: " + repr(error))
                if isinstance(error, KeyboardInterrupt):
                    interrupted = True
    except BaseException as error:
        errors.append("producer setup: " + repr(error))
        interrupted = isinstance(error, KeyboardInterrupt)
    finally:
        # Even a receipt/setup exception must leave no unreaped native producer.
        # Separate process groups keep an interrupted controller from forwarding
        # its console interrupt to the producer while natural closure is awaited.
        while True:
            try:
                ready.set()
                break
            except KeyboardInterrupt:
                interrupted = True
        if process is not None:
            while True:
                try:
                    report["exit_code"] = process.wait()
                    report["reaped"] = True
                    break
                except KeyboardInterrupt:
                    interrupted = True
        for thread in threads.values():
            while thread.is_alive():
                try:
                    thread.join()
                except KeyboardInterrupt:
                    interrupted = True
        if process is None:
            for label in compressors:
                if label in captures:
                    continue
                compressors[label].close()
                sinks[label].close()
        report["seconds"] = time.monotonic() - started
        report["interrupted"] = interrupted
        report["capture_errors"] = errors
        report["streams"] = captures
    for label, capture in captures.items():
        path = directory / (label + ".gz")
        capture["path"] = str(path)
        capture["compressed"] = fingerprint(path)
        with path.open("rb") as stream:
            require(stream.read(8) == b"\x1f\x8b\x08\x00\x00\x00\x00\x00",
                    "Capture gzip header is not deterministic mtime-zero/no-filename")
        digest, size = hashlib.sha256(), 0
        with gzip.open(path, "rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
                size += len(block)
        require({"bytes": size, "sha256": digest.hexdigest()} == capture["raw"],
                "Compressed capture round-trip differs")
        capture["decompression_crc_and_raw_hash_checked"] = True
    write_json(directory / "result.json", report)
    require(report["reaped"] and report["exit_code"] == 0 and not interrupted and not errors and
            all(not item["storage_errors"] for item in captures.values()),
            "Native producer did not close naturally with complete error-free capture")


def native_prefix(fields, side):
    family, pc = failure(fields)
    if family == "MOVE":
        return ["PREFIX", fields[0], fields[2], family, "6", str(pc), "0", "1", "0", "1", "0", "0", "-",
                "Entity movement collision query", "Collision arguments", "1" if pc in (232, 313) else "0"]
    if family == "EFFECT":
        return ["PREFIX", fields[0], fields[2], family, "6", "32", "2", "6", "32", "1", "0", "0", "-",
                "Entity wet World rain-at call", "World movement arguments", "0"]
    block = ("ph" if side == "CLIENT" else "jn") if fields[19] == "28" else ("bv" if side == "CLIENT" else "bc")
    return ["PREFIX", fields[0], fields[2], family, "5", "1", "0", "5", "0", "0", "0", "0", block,
            "Block contact World B receiver", "Block contact World B receiver", "0"]


def lf_payload(line):
    require(line.endswith(b"\n") and b"\r" not in line and b"\n" not in line[:-1],
            "Native output must contain complete uniform LF records")
    return line[:-1].decode("ascii")


def compare_side(side, directory, original, inputs, selected, offsets, constructors, executed):
    selected_failure, selected_success, state_failure = 0, 0, 0
    # The actual Registry PrintStream uses the platform separator independently
    # of the probe's binary stdio and LF-only observation protocol.
    separator = b"\r\n" if os.name == "nt" else b"\n"
    with inputs.path.open("rb") as source, \
            (original / ("execution-a/" + side.lower() + "-original.stdout")).open("rb") as expected, \
            gzip.open(directory / "stdout.gz", "rb") as observed, \
            gzip.open(directory / "stderr.gz", "rb") as diagnostics:
        require([observed.readline() for _ in range(3)] ==
                [b"16 achievements" + separator, b"151 recipes" + separator,
                 ("ENTITY_MOVEMENT_NATIVE_V1\t" + side + "\n").encode("ascii")],
                "Native bootstrap/header differs")
        for number in selected:
            supplied = inputs.row(number, source)
            native = lf_payload(observed.readline()).split("\t")
            validate_output(native, supplied, constructors, native=True)
            expected.seek(offsets[number])
            original_fields = crlf_record(expected.readline()).decode("ascii").split("\t")
            # These two exact, independently guarded differences are disclosed;
            # every other byte field, including all pool tails, is compared.
            for index in range(25):
                if index not in (3, 24):
                    require(native[index] == original_fields[index],
                            "%s %s field %d differs" % (side, supplied[0], index + 1))
            if failure(supplied):
                selected_failure += 1
                prefix = lf_payload(diagnostics.readline()).split("\t")
                require(prefix == native_prefix(supplied, side), "Native failure prefix differs: " + supplied[0])
            else:
                selected_success += 1
        selected_set = set(selected)
        for number in range(executed):
            if number not in selected_set and failure(inputs.row(number, source)):
                state_failure += 1
        require(observed.read(1) == b"", "Unexpected trailing native stdout")
        footer = ("PASS: Entity movement replay; side=%s; executed=%d; selected=%d; state_only=%d; "
                  "prefixes=%d; state_only_prefixes=%d; entropy=%d; cleanup=OK" %
                  (side, executed, len(selected), executed - len(selected), selected_failure, state_failure, executed))
        require(lf_payload(diagnostics.readline()) == footer and diagnostics.read(1) == b"",
                "Native footer/ownership closure or diagnostic stream differs")
    require((selected_failure, state_failure) == ((10874, 0) if side == "CLIENT" else (4741, 696)),
            "Native failure inventory differs")
    entropy = validate_entropy(directory / "entropy.tsv", inputs, executed, native=True)
    return {"status": "PASS", "executed": executed, "compared": len(selected),
            "bootstrap_line_ending": "CRLF" if os.name == "nt" else "LF",
            "state_only": executed - len(selected), "unobserved_original": TOTAL - len(selected),
            "successes": selected_success, "source_prefix_failures": selected_failure,
            "state_only_prefixes": state_failure, "fields_per_row": 25,
            "comparison_exceptions": ["guarded original NPE/native NATIVE_FAILURE spelling", "exact disclosed setup provenance"],
            "full_pool_tails_and_sequence_snapshots_compared": True,
            "entropy": entropy, "ownership_footer": footer}


def prepare_side(directory, inputs, selected, executed, pins):
    directory.mkdir()
    report = {"selected": write_ids(directory / "selection.ids", selected),
              "executed": write_ids(directory / "executed.ids", range(executed))}
    selected_set = set(selected)
    report["state_only"] = write_ids(directory / "state-only.ids", (i for i in range(executed) if i not in selected_set))
    report["unobserved_original"] = write_ids(directory / "unobserved-original.ids", (i for i in range(TOTAL) if i not in selected_set))
    if executed == TOTAL:
        input_path = inputs.path
    else:
        input_path = directory / "input.tsv"
        with inputs.path.open("rb") as source, input_path.open("xb") as destination:
            for number in range(executed):
                line = source.readline()
                input_fields(line, number)
                destination.write(line)
        require(input_path.stat().st_size == 12385558, "Server input prefix byte count differs")
    report["input"] = {"path": str(input_path), **pins.add(input_path)}
    for name in ("selection.ids", "executed.ids", "state-only.ids", "unobserved-original.ids"):
        pins.add(directory / name)
    return input_path, report


def main():
    parser = argparse.ArgumentParser(description=(
        "Replay all 59,654 qualified client rows and 24,109 intact server rows, executing "
        "the 35,748-row server prefix to preserve state. Only native processes are launched. "
        "New stdout/stderr are retained losslessly as gzip from the first byte, with no caps or timeouts."))
    parser.add_argument("--original", required=True, type=Path, help="Retained entity-movement-original-a family")
    parser.add_argument("--native", required=True, type=Path, help="New native movement probe executable")
    parser.add_argument("--build-receipt", required=True, type=Path, help="Schema 1 passed immutable-core relink receipt")
    parser.add_argument("--output", required=True, type=Path, help="Fresh exclusive output directory")
    args = parser.parse_args()
    original = args.original.resolve(strict=True)
    executable = args.native.resolve(strict=True)
    receipt = args.build_receipt.resolve(strict=True)
    output = args.output.resolve()
    require(output != original and original not in output.parents and output not in original.parents,
            "Output must be disjoint from retained original evidence")
    require(executable != output and output not in executable.parents and output not in receipt.parents,
            "Output must not contain producer/build evidence")
    output.mkdir(parents=True, exist_ok=False)
    pins = Pins()
    report = {"schema": 1, "status": "FAIL", "errors": [], "sides": {},
              "original": str(original), "output": str(output), "limitations": LIMITATIONS,
              "full_paired_family_complete": False, "original_processes_launched": 0,
              "private_producers_imported_or_executed": False,
              "native_output_retention": "gzip from first byte; all raw bytes hashed and CRC/decompression checked"}
    try:
        report["disk_reserve_before_validation"] = reserve(output)
        qualification, scope, selected, report["evidence"] = validate_evidence(original, pins)
        report["build"] = validate_build(receipt, executable, pins)
        inputs = Inputs(original / "execution-a/input.tsv")
        report["categories"] = inputs.validate(original / "execution-a/cases.json", qualification["client"]["categories"])
        constructors = qualification["client"]["constructor_snapshots"]
        client_offsets, server_offsets, report["original_rows"] = original_rows(
            original, inputs, scope, selected, constructors)
        report["original_traces"] = original_traces(original, inputs, selected)
        report["original_entropy"] = {
            "client": validate_entropy(original / "execution-a/client.entropy.tsv", inputs, TOTAL, False),
            "server": validate_entropy(original / "execution-a/server.entropy.tsv", inputs, SERVER_EXECUTED, False, True),
        }
        pins.recheck()
        for side, selection, offsets, executed in (
                ("CLIENT", list(range(TOTAL)), client_offsets, TOTAL),
                ("SERVER", selected, server_offsets, SERVER_EXECUTED)):
            pins.recheck()
            reserve(output)
            directory = output / side.lower()
            input_path, scope_report = prepare_side(directory, inputs, selection, executed, pins)
            side_report = {"scope": scope_report, "process": {}}
            report["sides"][side.lower()] = side_report
            capture_child(executable, side, directory, input_path, executed, len(selection), side_report["process"])
            side_report["comparison"] = compare_side(
                side, directory, original, inputs, selection, offsets, constructors, executed)
            pins.recheck()
        report["compared_rows"] = TOTAL + len(selected)
        report["executed_rows"] = TOTAL + SERVER_EXECUTED
        report["server_state_only_rows"] = SERVER_EXECUTED - len(selected)
        report["status"] = "PASS"
    except BaseException as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        try:
            pins.recheck()
            report["before_after_pins"] = "PASS"
        except BaseException as error:
            report["errors"].append("Final pin audit: " + str(error))
            report["status"] = "FAIL"
        report["checked_files"] = pins.values
        report["produced_files"] = {
            path.relative_to(output).as_posix(): fingerprint(path)
            for path in sorted(output.rglob("*")) if path.is_file()
        }
        write_json(output / "report.json", report)
    print(report["status"] + ": retained movement comparison; " + str(output / "report.json"))
    for error in report["errors"]:
        print(error, file=sys.stderr)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
