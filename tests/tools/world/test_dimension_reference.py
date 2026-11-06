#!/usr/bin/env python3
"""Capture original Dimension calls, or replay their exact measured projection.

Capture runs only the pinned compiler and two original -Xint observers. Replay
runs only the supplied native probe. Neither mode computes expected angles,
light entries, trigonometry, or approximate/normalized floating point results.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[3]
OBSERVER = ROOT / "tests/reference/world/DimensionObservation.java"
OBSERVER_SHA256 = "ce24595652f28a0fe244dae41186b2b8cebe33b5fdbd2b543a25f869eac30959"
PROCESS = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
HISTORICAL_RECORDER = ROOT / "tests/reference/world/dimension/recorder.py"
HISTORICAL_RECORDER_PIN = {
    "bytes": 52843, "sha256": "2b10393e1e006a1b7ce4f8df732bc72bed1410753a38176d8eaab9dd6313a69a"}
HISTORICAL_CAPTURE_PIN = {
    "bytes": 13776, "sha256": "419adbbdb4acb4577b8b2527271f0bb19b6ff8011bd1e42119ee1aa5db67e27c"}
JARS = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
CLASSES = {"DimensionObservation.class", "DimensionObservation$Binding.class",
           "DimensionObservation$Snapshot.class"}
KINDS = {"client": {"wd": "-1", "rh": "0", "ay": "1"},
         "server": {"nz": "-1", "kp": "0", "aj": "1"}}
TIMES = (-(1 << 63), -(1 << 63) + 1, -48001, -48000, -47999,
         -24001, -24000, -23999, -18001, -18000, -17999, -6001, -6000,
         -5999, -1, 0, 1, 5998, 5999, 6000, 6001, 11999, 12000, 12001,
         17999, 18000, 18001, 23999, 24000, 24001, 47999, 48000, 48001,
         (1 << 63) - 2, (1 << 63) - 1)
PARTIALS = ("80000000", "00000000", "00000001", "80000001", "3effffff",
            "3f000000", "3f000001", "3f7fffff", "3f800000", "3f800001",
            "bf800000", "40000000", "c0000000", "46ea6000", "4752f000",
            "47985800", "7f7fffff", "ff7fffff", "00800000", "80800000")
REPRESENTATIVE = (0, 1, 33, 34, 14, 15, 16, 17, 18, 19,
                  20, 21, 22, 23, 24, 25, 26, 27, 28, 29)
DECIMAL = re.compile(r"(?:0|-?[1-9][0-9]*)\Z")
CLASS_NAME = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*\Z")
HEX8 = re.compile(r"[0-9a-f]{8}\Z")
MAX_BYTES = 4 * 1024 * 1024
MAX_STDERR = 1024 * 1024
SCOPE = {
    "compared": "Actual factory/null/kind, eight-field scalar/reference projection, constructor array bits, standalone real light-table mutation, array identity relations, and exact raw celestial returns for the fixed corpus",
    "original_only": "Actual normal null-World bind outcome, diagnostic and AFTER_NULL_BIND state/relations; both fixed-kind bind skips are unexecuted",
    "identity": "A bijection of encountered live owner/array/field references; no JVM native identity/hash or address equivalence",
    "array_lengths": "Original observed lengths versus native documented READY constructor allocations f[16]/h[4]; native API exposes pointers without dynamic length descriptors",
    "excluded": "Declaring Java class/member names are source-binding metadata, not native class objects; no full World/biome bind, startup, clocks, intrinsic selection, all-runtime or unrestricted-input claim",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
    return {"bytes": size, "sha256": digest.hexdigest()}


def recorder_binding(recorded, public_capture=None):
    """Bind captured source bytes separately from the current replay checker.

    The historical recorder is immutable evidence, never imported or executed.
    The known public capture must retain that recorder; a current capture must
    carry the actual current recorder pin and a different capture identity.
    """
    require(type(recorded) is dict and set(recorded) == {"bytes", "sha256"} and
            type(recorded["bytes"]) is int and 0 < recorded["bytes"] <= 128 * 1024 and
            type(recorded["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", recorded["sha256"]),
            "complete recorded recorder fingerprint")
    require(HISTORICAL_RECORDER.is_file() and not HISTORICAL_RECORDER.is_symlink() and
            HISTORICAL_RECORDER.stat().st_size == HISTORICAL_RECORDER_PIN["bytes"] and
            fingerprint(HISTORICAL_RECORDER) == HISTORICAL_RECORDER_PIN,
            "immutable historical recorder bytes")
    checker = Path(__file__).resolve()
    checker_pin = fingerprint(checker)
    if recorded == HISTORICAL_RECORDER_PIN:
        actual, family = HISTORICAL_RECORDER, "historical-recorder"
        if public_capture is not None:
            require(public_capture == HISTORICAL_CAPTURE_PIN, "historical public capture/recorder family")
    else:
        require(recorded == checker_pin, "current recorded recorder bytes")
        actual, family = checker, "current-recorder"
        if public_capture is not None:
            require(type(public_capture) is dict and set(public_capture) == {"bytes", "sha256"} and
                    type(public_capture["bytes"]) is int and 0 < public_capture["bytes"] <= 512 * 1024 and
                    type(public_capture["sha256"]) is str and
                    re.fullmatch(r"[0-9a-f]{64}", public_capture["sha256"]) and
                    public_capture != HISTORICAL_CAPTURE_PIN, "current public capture/recorder family")
    return {"family": family,
            "actual_recorder": {"path": actual.relative_to(ROOT).as_posix(), **recorded},
            "current_checker": {"path": checker.relative_to(ROOT).as_posix(), **checker_pin}}


def captured_recorder_binding(directory):
    path = directory / "capture.json"
    require(path.stat().st_size <= 512 * 1024, "bounded original metadata")
    report = json.loads(path.read_text(encoding="ascii"))
    return recorder_binding(report["inputs_before"]["recorder"])


def public_provenance(directory, provenance_sha256):
    path = directory / "dimension_provenance.json"
    require(path.stat().st_size <= 128 * 1024, "bounded public provenance")
    require(type(provenance_sha256) is str and re.fullmatch(r"[0-9a-f]{64}", provenance_sha256) and
            fingerprint(path)["sha256"] == provenance_sha256, "pinned public Dimension provenance")
    return json.loads(path.read_text(encoding="ascii"))


def encoded(rows):
    return ("\n".join("\t".join(str(value) for value in row) for row in rows) + "\n").encode("ascii")


def write_bytes(path, raw):
    with path.open("xb") as stream:
        stream.write(raw)


def write_json(path, value):
    with path.open("x", encoding="ascii", newline="\n") as stream:
        json.dump(value, stream, sort_keys=True, indent=2, ensure_ascii=True)
        stream.write("\n")


def decimal(value, low, high):
    require(len(value) <= 20 and DECIMAL.fullmatch(value) is not None, "canonical decimal")
    result = int(value)
    require(low <= result <= high, "decimal range")
    return result


def raw_rows(raw):
    require(0 < len(raw) <= MAX_BYTES and raw.endswith(b"\n") and b"\r" not in raw,
            "bounded complete literal LF stream")
    require(all(value == 9 or value == 10 or 32 <= value <= 126 for value in raw),
            "ASCII protocol fields")
    rows = [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]
    require(len(rows) <= 60000 and all(all(field for field in row) for row in rows),
            "bounded nonempty protocol fields")
    return rows


def corpus_request():
    """The observer's input schedule only; never a result/angle calculation."""
    commands = [["DIMENSION_NATIVE_REQUEST_V1"]]
    for slot, requested in enumerate((-1, 0, 1)):
        commands.extend([["FACTORY", str(slot), str(requested)], ["SNAP", str(slot)],
                         ["TABLE", str(slot)], ["SNAP", str(slot)]])
        if requested == 0:
            pairs = ((selected_time, "3f800000") for selected_time in range(-23999, 24000))
            commands.extend(["ANGLE", str(slot), str(selected_time), partial]
                            for selected_time, partial in pairs)
            commands.extend(["ANGLE", str(slot), str(selected_time), partial]
                            for selected_time in TIMES for partial in PARTIALS)
        else:
            commands.extend(["ANGLE", str(slot), str(TIMES[index]), partial]
                            for index, partial in zip(REPRESENTATIVE, PARTIALS))
        commands.append(["SNAP", str(slot)])
    commands.extend(["FACTORY", str(slot), str(requested)] for slot, requested in
                    enumerate((-(1 << 31), -2, 2, (1 << 31) - 1), start=3))
    commands.extend([["FACTORY", "7", "0"], ["SNAP", "7"], ["END"]])
    return encoded(commands)


def parse_projection(request, projection):
    """Validate an exported finite corpus without inventing its return bits."""
    require(request == corpus_request(), "complete canonical exported input schedule")
    commands = raw_rows(request)
    rows = raw_rows(projection)
    require(rows[0] == ["DIMENSION_NATIVE_OBSERVATION_V1"], "exported projection header")
    cursor = 1
    references, owners = {}, {}
    counts = {"factories": 0, "snapshots": 0, "tables": 0, "angles": 0}

    def take(kind, width):
        nonlocal cursor
        require(cursor < len(rows), "missing exported projection row")
        row = rows[cursor]
        cursor += 1
        require(len(row) == width and row[0] == kind, "exported projection schema/order")
        return row

    def ref(value, kind):
        number = decimal(value, 0, 128)
        if number:
            if number not in references:
                require(number == len(references) + 1, "exported first-encounter identity order")
                references[number] = kind
            require(references[number] == kind, "exported reference type consistency")
        return value

    for command in commands[1:-1]:
        operation, slot = command[:2]
        if operation == "FACTORY":
            row = take("FACTORY", 5)
            require(row[1:3] == command[1:] and row[4] in ("-", "-1", "0", "1"),
                    "exported actual factory result")
            owner = ref(row[3], "owner")
            require((owner == "0") == (row[4] == "-"), "exported nullable kind")
            owners[slot] = (owner, row[4])
            counts["factories"] += 1
        elif operation == "SNAP":
            owner, kind = owners[slot]
            row = take("STATE", 10)
            require(owner != "0" and row[1:4] == [slot, owner, kind], "exported live snapshot owner")
            ref(row[4], "world")
            ref(row[5], "biome")
            require(all(value in ("0", "1") for value in row[6:9]), "exported boolean fields")
            decimal(row[9], -(1 << 31), (1 << 31) - 1)
            for field, bound in (("f", 16), ("h", 4)):
                array = take("ARRAY", 6)
                require(array[1:3] == [slot, field], "exported array order")
                reference = ref(array[3], "array")
                length = decimal(array[4], -1, 16)
                require((reference == "0" and length == -1 and array[5] == "-") or
                        (reference != "0" and length == bound and len(array[5]) == 8 * length and
                         re.fullmatch(r"[0-9a-f]+", array[5])), "exported API storage/raw bits")
            counts["snapshots"] += 1
        elif operation == "TABLE":
            require(take("TABLE", 2)[1:] == [slot], "exported table operation")
            counts["tables"] += 1
        else:
            row = take("ANGLE", 5)
            require(row[1:4] == command[1:] and HEX8.fullmatch(row[4]),
                    "exported measured angle input/return")
            counts["angles"] += 1
    footer = take("END", 7)
    require(cursor == len(rows) and footer[1:] == [str(counts[name]) for name in
            ("factories", "snapshots", "tables", "angles")] +
            [str(len(references)), str(len(rows) - 1)], "complete exported projection footer")
    return {"request": request, "projection": projection, "projected_rows": rows,
            "compared_counts": {**counts, "references": len(references), "rows": len(rows)}}


def parse_original(raw, side):
    """Check the exact helper grammar; derive returns only from measured rows."""
    require(side in KINDS, "original side")
    rows = raw_rows(raw)
    require(len(rows) == 48875, "complete original row inventory")
    cursor = 0
    references = {}
    owners = {}
    projected = [["DIMENSION_NATIVE_OBSERVATION_V1"]]
    commands = [["DIMENSION_NATIVE_REQUEST_V1"]]
    excluded = []
    counts = {"factories": 0, "nonnull_factories": 0, "states": 0,
              "tables": 0, "angles": 0, "relations": 0}
    native_states = 0

    def take(kind, width, prefix=()):
        nonlocal cursor
        require(cursor < len(rows), "missing original " + kind)
        row = rows[cursor]
        cursor += 1
        require(len(row) == width and row[0] == kind and
                row[1:1 + len(prefix)] == list(prefix),
                f"original row {cursor}: {kind} schema/order")
        return row

    def ref(value, class_name):
        result = decimal(value, 0, 128)
        require((result == 0) == (class_name == "-"), "null reference/class agreement")
        if result:
            require(class_name == "[F" or CLASS_NAME.fullmatch(class_name), "reference class")
            if result in references:
                require(references[result] == class_name, "stable original mirror type")
            else:
                require(result == len(references) + 1, "original first-encounter identity order")
                references[result] = class_name
        return str(result)

    def known(value):
        result = decimal(value, 0, 128)
        require(result == 0 or result in references, "relation uses known reference")
        return str(result)

    def relation(label, left, right, native=True):
        row = take("REL", 5, (label,))
        require(known(row[2]) == left and known(row[3]) == right and
                row[4] == ("1" if left == right else "0"), "actual relation consistency")
        counts["relations"] += 1
        if not native:
            excluded.append(row)

    def factory(slot, role, requested):
        row = take("FACTORY", 6, (str(slot), role, str(requested)))
        owner = ref(row[4], row[5])
        require(row[5] == "-" or row[5] in KINDS[side], "original concrete returned kind")
        kind = "-" if owner == "0" else KINDS[side][row[5]]
        owners[slot] = (owner, row[5], kind)
        counts["factories"] += 1
        counts["nonnull_factories"] += owner != "0"
        commands.append(["FACTORY", str(slot), str(requested)])
        projected.append(["FACTORY", str(slot), str(requested), owner, kind])
        return owner

    def state(slot, stage, native=True):
        nonlocal native_states
        owner, class_name, kind = owners[slot]
        require(owner != "0", "nonnull original snapshot receiver")
        row = take("STATE", 13, (stage, str(slot), owner, class_name))
        world = ref(row[5], row[6])
        biome = ref(row[7], row[8])
        require(row[6] in ("-", "fd" if side == "client" else "dj") and
                row[8] in ("-", "xv" if side == "client" else "ph"), "field reference types")
        require(all(value in ("0", "1") for value in row[9:12]), "boolean fields")
        decimal(row[12], -(1 << 31), (1 << 31) - 1)
        if native:
            commands.append(["SNAP", str(slot)])
            projected.append(["STATE", str(slot), owner, kind, world, biome, *row[9:13]])
            native_states += 1
        else:
            excluded.append(row)
        arrays = {}
        for field in ("f", "h"):
            array = take("ARRAY", 9, (stage, str(slot), owner, field))
            array_ref = ref(array[5], array[6])
            length = decimal(array[7], -1, 16)
            require(array[6] in ("-", "[F") and (array_ref == "0") == (length == -1),
                    "array reference/type/length")
            require(array[8] == "-" if length <= 0 else
                    len(array[8]) == 8 * length and re.fullmatch(r"[0-9a-f]+", array[8]),
                    "complete raw float array bits")
            arrays[field] = array_ref
            if native:
                projected.append(["ARRAY", str(slot), field, array_ref, array[7], array[8]])
            else:
                excluded.append(array)
        counts["states"] += 1
        return arrays

    def resolved(slot, operation, name, descriptor):
        owner = owners[slot][0]
        row = take("RESOLVED", 7, (str(slot), owner, operation))
        require(row[4] in (*KINDS[side], "xa" if side == "client" else "os") and
                row[5:] == [name, descriptor], "resolved original virtual binding")
        return row

    def angle(slot, corpus, selected_time, partial):
        owner = owners[slot][0]
        row = take("ANGLE", 8, (str(slot), owner, corpus, str(counts["angles"]),
                                str(selected_time), partial))
        require(HEX8.fullmatch(row[7]), "measured return raw float bits")
        commands.append(["ANGLE", str(slot), row[5], row[6]])
        projected.append(["ANGLE", str(slot), row[5], row[6], row[7]])
        counts["angles"] += 1

    take("DIMENSION_OBSERVATION_V1", 2, (side,))
    take("CORPUS", 6, ("35", "20", "-23999", "23999", "3f800000"))
    for index, selected_time in enumerate(TIMES):
        take("TIME", 4, (str(index), str(selected_time), f"{selected_time & ((1 << 64) - 1):016x}"))
    for index, partial in enumerate(PARTIALS):
        take("PARTIAL", 3, (str(index), partial))
    constructed = []
    for slot, requested in enumerate((-1, 0, 1)):
        require(factory(slot, "MAIN", requested) != "0", "main factory receiver")
        before = state(slot, "CONSTRUCTED")
        constructed.append(before)
        relation(f"factory-{slot}.f-h", before["f"], before["h"])
        for previous in range(slot):
            for field in ("f", "h"):
                relation(f"factory-{slot}.{field}-vs-{previous}.{field}",
                         before[field], constructed[previous][field])
        light = resolved(slot, "LIGHT", "e" if side == "client" else "c", "()V")
        resolved(slot, "ANGLE", "a", "(JF)F")
        take("TABLE", 5, (str(slot), owners[slot][0], light[4], light[5]))
        counts["tables"] += 1
        commands.append(["TABLE", str(slot)])
        projected.append(["TABLE", str(slot)])
        after = state(slot, "AFTER_LIGHT")
        for field in ("f", "h"):
            relation(f"factory-{slot}.light-{field}-stable", before[field], after[field])
        if requested == 0:
            for selected_time in range(-23999, 24000):
                angle(slot, "FULL", selected_time, "3f800000")
            for selected_time in TIMES:
                for partial in PARTIALS:
                    angle(slot, "SELECTED", selected_time, partial)
        else:
            for time_index, partial in zip(REPRESENTATIVE, PARTIALS):
                angle(slot, "REPRESENTATIVE", TIMES[time_index], partial)
        final = state(slot, "AFTER_ANGLES")
        for field in ("f", "h"):
            relation(f"factory-{slot}.angle-{field}-stable", after[field], final[field])
    for slot, requested in enumerate((-(1 << 31), -2, 2, (1 << 31) - 1), start=3):
        factory(slot, "INVALID_CONTROL", requested)
    require(factory(7, "NULL_BIND_NORMAL_CONTROL", 0) != "0", "normal control receiver")
    before = state(7, "BEFORE_NULL_BIND")
    relation("null-bind.f-h", before["f"], before["h"])
    excluded.append(resolved(7, "BIND_NULL", "a", "(Lfd;)V" if side == "client" else "(Ldj;)V"))
    bind = take("BIND_NULL", 10, ("7", owners[7][0]))
    require(bind[3] in ("RETURN", "THROW"), "actual bind outcome")
    length = decimal(bind[5], -1, 4096)
    require(bind[6] == "-" if length <= 0 else
            len(bind[6]) == 4 * length and re.fullmatch(r"[0-9a-f]+", bind[6]),
            "bind exception nullable UTF-16 diagnostic")
    if bind[3] == "RETURN":
        require(bind[4:] == ["-", "-1", "-", "-", "-", "-1"], "normal return diagnostic")
    else:
        require(CLASS_NAME.fullmatch(bind[4]), "actual exception class")
        require((bind[7] == "-") == (bind[8] == "-"), "nullable first exception frame")
        if bind[7] != "-":
            require(CLASS_NAME.fullmatch(bind[7]) and
                    re.fullmatch(r"[A-Za-z_$<>][A-Za-z0-9_$<>]*", bind[8]), "first exception frame")
        else:
            require(bind[9] == "-1", "absent exception-frame line")
        decimal(bind[9], -(1 << 31), (1 << 31) - 1)
    excluded.append(bind)
    after = state(7, "AFTER_NULL_BIND", native=False)
    for field in ("f", "h"):
        relation(f"null-bind.{field}-stable", before[field], after[field], native=False)
    for requested in (-1, 1):
        excluded.append(take("SKIP", 4, (str(requested), "BIND_NULL",
                                        "FIXED_BIOME_REGISTRY_CLOSURE_NOT_ADMITTED")))
    footer = take("END", 9)
    require(footer[1:] == [str(counts[name]) for name in
            ("factories", "nonnull_factories", "states", "tables", "angles", "relations")]
            + [str(len(references)), str(len(rows) - 1)] and cursor == len(rows),
            "complete actual original footer")
    require(counts == {"factories": 8, "nonnull_factories": counts["nonnull_factories"],
                       "states": 11, "tables": 3, "angles": 48739, "relations": 24},
            "complete fixed operation corpus")
    native_refs = set()
    for row in projected[1:]:
        indices = {"FACTORY": (3,), "STATE": (2, 4, 5), "ARRAY": (3,)}.get(row[0], ())
        native_refs.update(row[index] for index in indices if row[index] != "0")
    commands.append(["END"])
    projected.append(["END", "8", str(native_states), "3", "48739",
                      str(len(native_refs)), str(len(projected))])
    require(encoded(commands) == corpus_request(), "original-derived complete request schedule")
    parse_projection(encoded(commands), encoded(projected))
    return {"counts": counts, "references": len(references), "rows": len(rows),
            "request": encoded(commands), "projection": encoded(projected),
            "projected_rows": projected, "original_only": excluded}


def compare_native(raw, observation):
    rows = raw_rows(raw)
    expected = observation["projected_rows"]
    require(len(rows) == len(expected), "complete native row inventory")
    forward, reverse = {"0": "0"}, {"0": "0"}
    for ordinal, (actual, target) in enumerate(zip(rows, expected), start=1):
        require(len(actual) == len(target) and actual[0] == target[0],
                f"native row {ordinal}: schema/order")
        indices = {"FACTORY": (3,), "STATE": (2, 4, 5), "ARRAY": (3,)}.get(actual[0], ())
        for index, (value, wanted) in enumerate(zip(actual, target)):
            if index not in indices:
                require(value == wanted,
                        f"native row {ordinal} {actual[0]} column {index}: {value!r} != measured {wanted!r}")
                continue
            decimal(value, 0, 128)
            if wanted not in forward:
                require(value not in reverse and value != "0" and
                        int(value) == len(reverse), "distinct native first-encounter reference")
                forward[wanted], reverse[value] = value, wanted
            require(forward[wanted] == value and reverse[value] == wanted,
                    f"native row {ordinal}: actual reference identity relation")
    require(len(reverse) - 1 == int(rows[-1][5]), "native live reference inventory")
    return {"rows": len(rows), "factories": int(rows[-1][1]),
            "snapshots": int(rows[-1][2]), "tables": int(rows[-1][3]),
            "angles": int(rows[-1][4]), "references": len(reverse) - 1}


def parser_guards(original, side, native, observation):
    """Corrupt actual text only; these controls are never expected producers."""
    outcomes = {"original": {}, "native": {}}
    original_rows = raw_rows(original) if original is not None else None
    native_rows = raw_rows(native)

    def find(rows, kind):
        return next(index for index, row in enumerate(rows) if row[0] == kind)

    def reject(group, name, changed, check):
        try:
            check(encoded(changed))
        except ValueError as error:
            outcomes[group][name] = {"status": "rejected", "reason": str(error)}
        else:
            raise ValueError("corrupted " + group + " stream accepted: " + name)

    original_controls = ("wrong_side", "missing_angle", "angle_order", "malformed_float",
                         "relation_lie", "array_width", "footer_count") if original is not None else ()
    for name in original_controls:
        changed = [row[:] for row in original_rows]
        angle_index = find(changed, "ANGLE")
        if name == "wrong_side":
            changed[0][1] = "server" if side == "client" else "client"
        elif name == "missing_angle":
            del changed[angle_index]
        elif name == "angle_order":
            changed[angle_index], changed[angle_index + 1] = changed[angle_index + 1], changed[angle_index]
        elif name == "malformed_float":
            changed[angle_index][7] = "NOTHEX00"
        elif name == "relation_lie":
            index = find(changed, "REL")
            changed[index][4] = "0" if changed[index][4] == "1" else "1"
        elif name == "array_width":
            changed[find(changed, "ARRAY")][8] += "00000000"
        else:
            changed[-1][5] = str(int(changed[-1][5]) + 1)
        reject("original", name, changed, lambda candidate: parse_original(candidate, side))
    for name in ("return_bit", "input_bit", "array_alias", "array_value", "kind",
                 "missing_state", "footer_count"):
        changed = [row[:] for row in native_rows]
        angle_index = find(changed, "ANGLE")
        if name in ("return_bit", "input_bit"):
            column = 4 if name == "return_bit" else 3
            changed[angle_index][column] = f"{int(changed[angle_index][column], 16) ^ 1:08x}"
        elif name == "array_alias":
            index = find(changed, "ARRAY")
            changed[index + 1][3] = changed[index][3]
        elif name == "array_value":
            index = find(changed, "ARRAY")
            bits = changed[index][5]
            require(bits != "-", "actual nonempty array guard premise")
            changed[index][5] = f"{int(bits[:8], 16) ^ 1:08x}" + bits[8:]
        elif name == "kind":
            index = find(changed, "FACTORY")
            changed[index][4] = "0" if changed[index][4] != "0" else "1"
        elif name == "missing_state":
            del changed[find(changed, "STATE")]
        else:
            changed[-1][4] = str(int(changed[-1][4]) + 1)
        reject("native", name, changed, lambda candidate: compare_native(candidate, observation))
    return outcomes


def fixture_request_guards(observation):
    rows = raw_rows(observation["request"])
    angle_index = next(index for index, row in enumerate(rows) if row[0] == "ANGLE")
    outcomes = {}
    for name in ("wrong_header", "missing_angle", "angle_order", "malformed_partial",
                 "time_overflow", "duplicate_slot", "trailing_command"):
        changed = [row[:] for row in rows]
        if name == "wrong_header":
            changed[0][0] = "WRONG_SIDE_OR_PROTOCOL"
        elif name == "missing_angle":
            del changed[angle_index]
        elif name == "angle_order":
            changed[angle_index], changed[angle_index + 1] = changed[angle_index + 1], changed[angle_index]
        elif name == "malformed_partial":
            changed[angle_index][3] = "7F800000"
        elif name == "time_overflow":
            changed[angle_index][2] = "9223372036854775808"
        elif name == "duplicate_slot":
            changed.insert(2, rows[1][:])
        else:
            changed.append(["SNAP", "0"])
        try:
            parse_projection(encoded(changed), observation["projection"])
        except ValueError as error:
            outcomes[name] = {"status": "rejected", "reason": str(error)}
        else:
            raise ValueError("corrupted exported request accepted: " + name)
    return outcomes


def process_helper():
    require(fingerprint(PROCESS)["sha256"] == PROCESS_SHA256, "reviewed process helper required")
    spec = importlib.util.spec_from_file_location("dimension_original_process", PROCESS)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def compiled_files(directory):
    return {path.relative_to(directory).as_posix(): fingerprint(path)
            for path in sorted(directory.rglob("*")) if path.is_file()}


def summary(observation):
    return {key: observation[key] for key in ("counts", "references", "rows", "original_only")}


def capture(options):
    process = process_helper()
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one pinned Windows JDK required")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "helper": OBSERVER,
              "recorder": Path(__file__).resolve(), "process_helper": PROCESS,
              "modules": jdk / "lib/modules", "jvm": jdk / "bin/server/jvm.dll",
              "java_native": jdk / "bin/java.dll", "release": jdk / "release",
              "source_archive": jdk / "lib/src.zip",
              "client": options.client_jar.resolve(strict=True),
              "server": options.server_jar.resolve(strict=True)}
    before = {name: fingerprint(path) for name, path in inputs.items()}
    require(before["helper"]["sha256"] == OBSERVER_SHA256, "reviewed observer required")
    require(all(before[name]["sha256"] == digest for name, digest in process.PROFILE.items()),
            "retained Windows Temurin17 profile required")
    require(all(before[side]["sha256"] == digest for side, digest in JARS.items()),
            "retained original archive pair required")
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    receipts = []
    report = {"schema": "DIMENSION_ORIGINAL_CAPTURE_V1", "status": "failed",
              "scope": SCOPE, "inputs_before": before, "children": receipts,
              "paths": {name: str(path) for name, path in inputs.items()},
              "classes_directory": str(directory / "classes")}
    try:
        for name in ("helper", "recorder", "process_helper"):
            write_bytes(directory / (name + inputs[name].suffix), inputs[name].read_bytes())
        classes = directory / "classes"
        classes.mkdir()
        compiled = process.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, OBSERVER],
                               directory, "compile", receipts)
        require(not compiled, "unexpected compiler stdout")
        class_before = compiled_files(classes)
        report["classes_before"] = class_before
        require(set(class_before) == CLASSES, "exact fresh compiled observer classes")
        observations = {}
        for side in ("client", "server"):
            raw = process.run([java, "-Xint", "-Xmx256m", "-cp", classes,
                               "DimensionObservation", side, inputs[side]], directory, side, receipts)
            observation = parse_original(raw, side)
            write_bytes(directory / (side + ".request.tsv"), observation["request"])
            write_bytes(directory / (side + ".projected.tsv"), observation["projection"])
            observations[side] = {**summary(observation),
                                  "raw": fingerprint(directory / (side + ".stdout")),
                                  "request": fingerprint(directory / (side + ".request.tsv")),
                                  "projection": fingerprint(directory / (side + ".projected.tsv"))}
        class_after = compiled_files(classes)
        require(class_before == class_after, "compiled original helper changed")
        require(len(receipts) == 3 and all(child["exit_code"] == 0 and child["reaped"] is True and
                child["stderr"]["bytes"] == 0 and "failure" not in child for child in receipts),
                "three successful reaped original-only children")
        report.update(status="captured", classes_before=class_before, classes_after=class_after,
                      observations=observations)
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            report["inputs_after"] = {name: fingerprint(path) for name, path in inputs.items()}
            require(report["inputs_after"] == before, "original capture inputs changed")
        except BaseException as error:
            report.update(status="failed", input_error=f"{type(error).__name__}: {error}")
            raise
        finally:
            write_json(directory / "capture.json", report)
    print("CAPTURED: two complete original Dimension transcripts; 48,739 real angle calls per side.")


def validate_capture(directory):
    require((directory / "capture.json").stat().st_size <= 512 * 1024, "bounded original metadata")
    report = json.loads((directory / "capture.json").read_text(encoding="ascii"))
    require(report["schema"] == "DIMENSION_ORIGINAL_CAPTURE_V1" and report["status"] == "captured",
            "successful original capture required")
    require(report["scope"] == SCOPE and report["inputs_before"] == report["inputs_after"],
            "original input/scope binding")
    before = report["inputs_before"]
    process = process_helper()
    require(set(before) == {"java", "javac", "helper", "recorder", "process_helper", "modules",
                            "jvm", "java_native", "release", "source_archive", "client", "server"},
            "complete measured original inputs")
    for name, digest in {**process.PROFILE, **JARS, "helper": OBSERVER_SHA256,
                         "process_helper": PROCESS_SHA256}.items():
        require(before[name]["sha256"] == digest, "original source/profile/archive pin: " + name)
    binding = recorder_binding(before["recorder"])
    actual_recorder = ROOT / binding["actual_recorder"]["path"]
    for name, path in (("recorder", actual_recorder), ("helper", OBSERVER),
                       ("process_helper", PROCESS)):
        require(before[name] == fingerprint(path) == fingerprint(directory / (name + path.suffix)),
                "original retained source binding: " + name)
    class_files = compiled_files(directory / "classes")
    require(set(class_files) == CLASSES and class_files == report["classes_before"] == report["classes_after"],
            "retained compiled helper bytes")
    children = report["children"]
    require(len(children) == 3, "original compiler and two JVM receipts")
    for child, name in zip(children, ("compile", "client", "server")):
        require(child["exit_code"] == 0 and child["reaped"] is True and "failure" not in child,
                "original natural exit/reap")
        require(child["stdout"] == fingerprint(directory / (name + ".stdout")) and
                child["stderr"] == fingerprint(directory / (name + ".stderr")) and
                child["stderr"]["bytes"] == 0, "retained original child streams")
        command = child["command"]
        if name == "compile":
            require(child["stdout"]["bytes"] == 0 and
                    command == [report["paths"]["javac"], "-J-Xmx256m", "-encoding", "UTF-8",
                                "-d", report["classes_directory"], report["paths"]["helper"]],
                    "actual original compiler command")
        else:
            # Capture directories may be copied intact to another host. Recorded
            # paths remain evidence; the original classes argument is from that
            # receipt, not the local replay directory's spelling.
            require(len(command) == 8 and command[:5] == [report["paths"]["java"], "-Xint", "-Xmx256m", "-cp", report["classes_directory"]]
                    and command[5:] == ["DimensionObservation", name, report["paths"][name]],
                    "actual isolated original JVM command")
    observations = {}
    for side in ("client", "server"):
        require((directory / (side + ".stdout")).stat().st_size <= MAX_BYTES, "bounded original transcript")
        observation = parse_original((directory / (side + ".stdout")).read_bytes(), side)
        expected = report["observations"][side]
        require(summary(observation) == {key: expected[key] for key in summary(observation)},
                "original measured inventory/diagnostics")
        for suffix, key, raw in (("stdout", "raw", None),
                                 ("request.tsv", "request", observation["request"]),
                                 ("projected.tsv", "projection", observation["projection"])):
            path = directory / (side + "." + suffix)
            require(fingerprint(path) == expected[key], "original retained stream digest")
            if raw is not None:
                require(path.read_bytes() == raw, "projection derived solely from actual raw transcript")
        observations[side] = observation
    return report, observations


def export(options):
    """Publish exact observed projections and hash provenance, without a child."""
    source = options.capture_dir.resolve(strict=True)
    paths = [path for path in sorted(source.rglob("*")) if path.is_file()]
    paths.extend((Path(__file__).resolve(), HISTORICAL_RECORDER, OBSERVER, PROCESS))
    require(len(paths) <= 30, "bounded original export inputs")
    before = {str(path): fingerprint(path) for path in paths}
    report, observations = validate_capture(source)
    binding = recorder_binding(report["inputs_before"]["recorder"], fingerprint(source / "capture.json"))
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    shared = all(observations["client"][key] == observations["server"][key]
                 for key in ("request", "projection"))
    side_files = {}
    files = {}
    for side in ("client", "server"):
        prefix = "dimension" if shared else "dimension_" + side
        names = {"request": prefix + "_request.tsv", "projection": prefix + "_expected.tsv"}
        side_files[side] = names
        for key, name in names.items():
            if name not in files:
                write_bytes(directory / name, observations[side][key])
                files[name] = fingerprint(directory / name)
    provenance = {
        "schema": "DIMENSION_ORIGINAL_PROJECTION_V1", "scope": SCOPE,
        "transformation": "Request schedule and result projection extracted from complete original transcripts; no result recomputation, tolerance, float normalization or class-name impersonation",
        "shared_pair": shared, "sides": side_files, "files": files,
        "origin": {"capture": fingerprint(source / "capture.json"),
                   "raw_transcripts": {side: report["observations"][side]["raw"] for side in KINDS},
                   "inputs": report["inputs_before"], "classes": report["classes_before"]},
        "observations": {side: {"original_counts": observations[side]["counts"],
                                "original_rows": observations[side]["rows"],
                                "compared": parse_projection(observations[side]["request"],
                                                             observations[side]["projection"])["compared_counts"]}
                         for side in KINDS},
        "private_retention": "Complete original stdout/stderr, receipts, null-bind diagnostics and after-state remain in the retained original capture; this public projection does not replay them",
        "export_binding": binding,
    }
    require({str(path): fingerprint(path) for path in paths} == before, "original export inputs changed")
    write_json(directory / "dimension_provenance.json", provenance)
    print(json.dumps({"status": "exported", "shared_pair": shared,
                      "provenance": fingerprint(directory / "dimension_provenance.json")}, sort_keys=True))


def validate_fixtures(directory, provenance_sha256):
    provenance = public_provenance(directory, provenance_sha256)
    require(provenance["schema"] == "DIMENSION_ORIGINAL_PROJECTION_V1" and provenance["scope"] == SCOPE,
            "public original projection schema/scope")
    inputs = provenance["origin"]["inputs"]
    process = process_helper()
    require(fingerprint(OBSERVER)["sha256"] == OBSERVER_SHA256, "retained original observer source")
    for name, digest in {**process.PROFILE, **JARS, "helper": OBSERVER_SHA256,
                         "process_helper": PROCESS_SHA256}.items():
        require(inputs[name]["sha256"] == digest, "public original profile/source/archive binding: " + name)
    binding = recorder_binding(inputs["recorder"], provenance["origin"]["capture"])
    if binding["family"] == "current-recorder" or "export_binding" in provenance:
        require(provenance.get("export_binding") == binding, "explicit exported recorder/checker binding")
    require(set(provenance["origin"]["classes"]) == CLASSES, "public compiled observer inventory")
    require(set(provenance["origin"]["raw_transcripts"]) == set(KINDS) and
            set(provenance["sides"]) == set(KINDS), "both original-side bindings")
    observations = {}
    used = set()
    for side in KINDS:
        names = provenance["sides"][side]
        require(set(names) == {"request", "projection"}, "public paired projection members")
        raw = {}
        for key, name in names.items():
            require(re.fullmatch(r"dimension(?:_(?:client|server))?_(?:request|expected)\.tsv", name),
                    "fixed public fixture file name")
            require(fingerprint(directory / name) == provenance["files"][name], "immutable observed projection bytes")
            require(provenance["files"][name]["bytes"] <= MAX_BYTES, "bounded published transcript projection")
            raw[key] = (directory / name).read_bytes()
            used.add(name)
        observation = parse_projection(raw["request"], raw["projection"])
        require(observation["compared_counts"] == provenance["observations"][side]["compared"] and
                provenance["observations"][side]["original_rows"] == 48875 and
                provenance["observations"][side]["original_counts"] == {
                    "factories": 8, "nonnull_factories": 4, "states": 11,
                    "tables": 3, "angles": 48739, "relations": 24}, "complete published original inventory")
        observations[side] = observation
    shared = all(observations["client"][key] == observations["server"][key]
                 for key in ("request", "projection"))
    require(provenance["shared_pair"] is shared and set(provenance["files"]) == used,
            "measured paired equality and complete fixture inventory")
    return provenance, observations


def run_native(probe, request, directory, name, receipts, expected_exit=0):
    require(0 < len(request) <= MAX_BYTES, "bounded native request")
    request_path = directory / (name + ".stdin")
    stdout_path = directory / (name + ".stdout")
    stderr_path = directory / (name + ".stderr")
    write_bytes(request_path, request)
    receipt = {"command": [str(probe)], "pid": None, "exit_code": None, "reaped": False,
               "request": fingerprint(request_path), "name": name, "expected_exit": expected_exit}
    receipts.append(receipt)
    started = time.monotonic()
    child = None
    failure = None
    try:
        with request_path.open("rb") as incoming, stdout_path.open("xb") as outgoing, stderr_path.open("xb") as errors:
            try:
                child = subprocess.Popen([str(probe)], stdin=incoming, stdout=outgoing, stderr=errors)
                receipt["pid"] = child.pid
                while child.poll() is None:
                    if time.monotonic() - started >= 120:
                        failure = "native child deadline exceeded"
                    if os.fstat(outgoing.fileno()).st_size > MAX_BYTES or os.fstat(errors.fileno()).st_size > MAX_STDERR:
                        failure = "native child output threshold exceeded"
                    if failure:
                        break
                    try:
                        child.wait(timeout=0.05)
                    except subprocess.TimeoutExpired:
                        pass
            finally:
                if child is not None:
                    try:
                        if child.poll() is None:
                            child.kill()
                    except OSError as error:
                        failure = f"owned native child termination failed: {error}"
                    finally:
                        try:
                            child.wait(timeout=5)
                        except (OSError, subprocess.TimeoutExpired) as error:
                            failure = f"owned native child wait failed: {error}"
                    receipt.update(exit_code=child.returncode, reaped=child.returncode is not None)
    except BaseException as error:
        receipt["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        receipt["seconds"] = time.monotonic() - started
        if failure:
            receipt["failure"] = failure
        for key, path in (("stdout", stdout_path), ("stderr", stderr_path)):
            if path.is_file():
                receipt[key] = fingerprint(path)
    require(failure is None and child is not None and receipt["reaped"], "owned native completion: " + str(failure))
    require(receipt["exit_code"] == expected_exit, "native exit status")
    require(receipt["stdout"]["bytes"] <= MAX_BYTES and receipt["stderr"]["bytes"] <= MAX_STDERR,
            "final native output bounds")
    if expected_exit == 0:
        require(receipt["stderr"]["bytes"] == 0, "successful native stderr must be empty")
    else:
        require(receipt["stderr"]["bytes"] > 0, "native refusal diagnostic required")
        require(stderr_path.read_bytes().startswith(b"dimension observation probe: "),
                "actual native protocol refusal diagnostic")
    return stdout_path.read_bytes()


def execute_replay(probe, observations, directory, receipts, raw_original=None):
    comparisons, guards = {}, {}
    for side in ("client", "server"):
        observation = observations[side]
        raw = run_native(probe, observation["request"], directory, side, receipts)
        comparisons[side] = compare_native(raw, observation)
        original = raw_original[side] if raw_original is not None else None
        guards[side] = parser_guards(original, side, raw, observation)
        if original is None:
            guards[side].pop("original")
            guards[side]["exported_request"] = fixture_request_guards(observation)
        else:
            write_json(directory / (side + ".original-only.json"), observation["original_only"])
    refusals = {
        "bad_header": b"BAD_HEADER\nEND\n",
        "duplicate_slot": b"DIMENSION_NATIVE_REQUEST_V1\nFACTORY\t0\t0\nFACTORY\t0\t0\nEND\n",
        "nonfinite_input": b"DIMENSION_NATIVE_REQUEST_V1\nFACTORY\t0\t0\nANGLE\t0\t0\t7f800000\nEND\n",
        "after_end": b"DIMENSION_NATIVE_REQUEST_V1\nEND\nFACTORY\t0\t0\n",
    }
    for name, request in refusals.items():
        raw = run_native(probe, request, directory, "refusal-" + name, receipts, expected_exit=1)
        require(not any(line == b"END" or line.startswith(b"END\t") for line in raw.splitlines()),
                "refused native request must not publish successful END")
    require(len(receipts) == 6, "two complete native replays and four natural refusal controls")
    require(sum(len(group) for side in guards.values() for group in side.values()) == 28,
            "complete verifier corruption-control inventory")
    return {"comparisons": comparisons, "parser_guards": guards,
            "rejected_corruptions": 28, "protocol_refusals": 4}


def replay(options):
    source = options.capture_dir.resolve(strict=True)
    probe = options.probe.resolve(strict=True)
    require(probe.is_file(), "native probe must be a regular file")
    directory = options.work_dir.resolve() / ("capture-" + uuid.uuid4().hex)
    directory.mkdir(parents=True, exist_ok=False)
    inputs = {"probe": probe, "checker": Path(__file__).resolve(), "observer": OBSERVER,
              "process_helper": PROCESS, "capture": source / "capture.json",
              "dimension": ROOT / "src/world/dimension/world_provider.c",
              "dimension_header": ROOT / "include/mc/world/dimension/world_provider.h",
              "dimension_internal": ROOT / "src/world/dimension/world_provider_internal.h",
              "probe_source": ROOT / "tests/world/dimension/dimension_observation_probe.c",
              "biome": ROOT / "src/world/biome/biome.c",
              "biome_header": ROOT / "include/mc/world/biome/biome.h",
              "biome_internal": ROOT / "src/world/biome/biome_internal.h",
              "cmake": ROOT / "CMakeLists.txt"}
    for path in sorted(source.rglob("*")):
        if path.is_file():
            inputs["original/" + path.relative_to(source).as_posix()] = path
    receipts = []
    report = {"schema": "DIMENSION_NATIVE_REPLAY_V1", "status": "failed", "scope": SCOPE,
              "children": receipts,
              "environment": {name: os.environ.get(name) for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
              "capture": str(source)}
    before = None
    try:
        binding = captured_recorder_binding(source)
        inputs["actual_recorder"] = ROOT / binding["actual_recorder"]["path"]
        require(len(inputs) <= 40 and all(path.is_file() for path in inputs.values()), "bounded complete replay inputs")
        before = {name: fingerprint(path) for name, path in inputs.items()}
        report["inputs_before"] = before
        for name, path in inputs.items():
            if name != "probe" and not name.startswith("original/"):
                destination = directory / (name + path.suffix)
                write_bytes(destination, path.read_bytes())
                require(fingerprint(destination) == before[name], "retained replay input copy: " + name)
        original_report, observations = validate_capture(source)
        require(recorder_binding(original_report["inputs_before"]["recorder"]) == binding,
                "retained recorder selection changed")
        report["recorder_binding"] = binding
        report["original_capture"] = {"inputs": original_report["inputs_before"],
                                      "classes": original_report["classes_before"]}
        result = execute_replay(probe, observations, directory, receipts,
                                {side: (source / (side + ".stdout")).read_bytes() for side in KINDS})
        report.update(status="passed", **result)
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            if before is not None:
                report["inputs_after"] = {name: fingerprint(path) for name, path in inputs.items()}
                require(report["inputs_after"] == before, "native replay inputs changed")
        except BaseException as error:
            report.update(status="failed", input_error=f"{type(error).__name__}: {error}")
            raise
        finally:
            write_json(directory / "comparison.json", report)
    print("PASS: 48,739 exact measured Dimension angle results per side; fields/tables/identities match; 28 corruptions and four invalid native requests rejected.")


def verify(options):
    source = options.fixture_dir.resolve(strict=True)
    probe = options.probe.resolve(strict=True)
    directory = options.work_dir.resolve() / ("capture-" + uuid.uuid4().hex)
    directory.mkdir(parents=True, exist_ok=False)
    inputs = {"probe": probe, "checker": Path(__file__).resolve(), "observer": OBSERVER,
              "process_helper": PROCESS, "provenance": source / "dimension_provenance.json",
              "dimension": ROOT / "src/world/dimension/world_provider.c",
              "dimension_header": ROOT / "include/mc/world/dimension/world_provider.h",
              "dimension_internal": ROOT / "src/world/dimension/world_provider_internal.h",
              "probe_source": ROOT / "tests/world/dimension/dimension_observation_probe.c",
              "biome": ROOT / "src/world/biome/biome.c",
              "biome_header": ROOT / "include/mc/world/biome/biome.h",
              "biome_internal": ROOT / "src/world/biome/biome_internal.h",
              "cmake": ROOT / "CMakeLists.txt"}
    inputs.update({"fixture/" + path.name: path for path in sorted(source.glob("dimension*.tsv"))})
    before = None
    receipts = []
    report = {"schema": "DIMENSION_PUBLIC_REPLAY_V1", "status": "failed", "scope": SCOPE,
              "children": receipts, "provenance_sha256": options.provenance_sha256,
              "environment": {name: os.environ.get(name) for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS")}}
    try:
        preliminary = public_provenance(source, options.provenance_sha256)
        binding = recorder_binding(preliminary["origin"]["inputs"]["recorder"], preliminary["origin"]["capture"])
        inputs["actual_recorder"] = ROOT / binding["actual_recorder"]["path"]
        require(len(inputs) <= 18 and all(path.is_file() for path in inputs.values()), "bounded complete public replay inputs")
        before = {name: fingerprint(path) for name, path in inputs.items()}
        report["inputs_before"] = before
        for name, path in inputs.items():
            if name != "probe":
                destination = directory / (name.replace("/", "-") + path.suffix)
                write_bytes(destination, path.read_bytes())
                require(fingerprint(destination) == before[name], "retained public replay input copy: " + name)
        provenance, observations = validate_fixtures(source, options.provenance_sha256)
        require(recorder_binding(provenance["origin"]["inputs"]["recorder"], provenance["origin"]["capture"]) == binding,
                "public recorder selection changed")
        report["recorder_binding"] = binding
        report["original_provenance"] = provenance
        report.update(status="passed", **execute_replay(probe, observations, directory, receipts))
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            if before is not None:
                report["inputs_after"] = {name: fingerprint(path) for name, path in inputs.items()}
                require(report["inputs_after"] == before, "public native replay inputs changed")
        except BaseException as error:
            report.update(status="failed", input_error=f"{type(error).__name__}: {error}")
            raise
        finally:
            write_json(directory / "comparison.json", report)
    print("PASS: paired original Dimension projections match exact native fields/tables/identities and 48,739 angles per side; 28 corruptions and four invalid native requests rejected.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    original = modes.add_parser("capture", help="compile and run only the pinned original observers")
    for name in ("java", "javac", "client-jar", "server-jar", "output-dir"):
        original.add_argument("--" + name, type=Path, required=True)
    native = modes.add_parser("replay", help="replay a complete retained original capture against the native probe")
    for name in ("capture-dir", "probe", "work-dir"):
        native.add_argument("--" + name, type=Path, required=True)
    exporter = modes.add_parser("export", help="export measured original projections and sanitized provenance; no child")
    for name in ("capture-dir", "output-dir"):
        exporter.add_argument("--" + name, type=Path, required=True)
    public = modes.add_parser("verify", help="ordinary native replay using only public original-derived fixtures")
    for name in ("fixture-dir", "probe", "work-dir"):
        public.add_argument("--" + name, type=Path, required=True)
    public.add_argument("--provenance-sha256", required=True)
    options = parser.parse_args()
    try:
        {"capture": capture, "replay": replay, "export": export, "verify": verify}[options.mode](options)
    except Exception as error:
        print(f"FAIL: Dimension reference: {type(error).__name__}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
