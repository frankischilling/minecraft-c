"""Validate complete packed-nibble observations without computing access results."""

from __future__ import annotations

import hashlib
import json
import re
from typing import Any


MAX_RECORD_BYTES = 2 * 1024 * 1024
PACKED_BYTES = 16384
CELL_COUNT = 32768
JARS = {
    "client": ("af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c", "wi"),
    "server": ("033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d", "ob"),
}
WRITES = (
    (0, 0, 0, 0), (0, 1, 0, 15), (15, 126, 15, 16), (15, 127, 15, 31),
    (0, 126, 15, -1), (0, 127, 15, -16), (15, 0, 0, -2147483648),
    (15, 1, 0, 2147483647), (3, 0, 11, 15), (3, 1, 11, 0),
    (11, 126, 3, 31), (11, 127, 3, 16), (1, 64, 14, -16),
    (1, 65, 14, -1), (14, 62, 1, 2147483647), (14, 63, 1, -2147483648),
)
COMMON_CONTROLS = (
    "fresh_zero_checked", "array_identity_unchanged", "initial_getters_readonly",
    "written_getters_readonly", "final_getters_readonly",
)
ORIGINAL_CONTROLS = COMMON_CONTROLS + ("original_class_loader_verified", "jar_unchanged")
COMMON_FIELDS = (
    "schema", "constructor_cells", "packed_length", "fresh_packed", "prepared_packed",
    "getter_order", "initial_getters", "writes", "final_packed", "final_getters",
)
RECORD_FIELDS = COMMON_FIELDS + ("side", "controls")
ORIGINAL_FIELDS = RECORD_FIELDS + ("jar_sha256_before", "jar_sha256_after", "bindings")


class ObservationError(ValueError):
    """An observation is malformed, incomplete, or differs from its counterpart."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ObservationError(message)


def object_keys(value: Any, keys: tuple[str, ...], label: str) -> None:
    require(type(value) is dict and set(value) == set(keys), label + " fields differ")


def integer(value: Any, low: int, high: int, label: str) -> None:
    require(type(value) is int and low <= value <= high, label + " integer range")


def hex_bytes(value: Any, count: int, label: str, *, getters: bool = False) -> bytes:
    require(type(value) is str and len(value) == count * 2 and
            re.fullmatch(r"[0-9a-f]*", value) is not None, label + " hex/cardinality")
    data = bytes.fromhex(value)
    if getters:
        require(all(item <= 15 for item in data), label + " getter range")
    return data


def validate_record(value: Any, side: str) -> dict[str, Any]:
    require(side in (*JARS, "c"), "unknown observation side")
    original = side in JARS
    object_keys(value, ORIGINAL_FIELDS if original else RECORD_FIELDS, "nibbles record")
    require(type(value["schema"]) is int and value["schema"] == 1 and
            value["side"] == side, "nibbles schema/side")
    if original:
        expected, owner = JARS[side]
        require(value["jar_sha256_before"] == expected == value["jar_sha256_after"],
                "observed archive hashes")
        require(value["bindings"] == {
            "owner": owner, "array": "a:[B", "constructor": owner + "(I)V",
            "getter": "a(III)I", "setter": "a(IIII)V",
        }, "observed original bindings")
    integer(value["constructor_cells"], CELL_COUNT, CELL_COUNT, "constructor cells")
    integer(value["packed_length"], PACKED_BYTES, PACKED_BYTES, "packed length")
    require(hex_bytes(value["fresh_packed"], PACKED_BYTES, "fresh packed") == bytes(PACKED_BYTES),
            "fresh packed bytes are not zero")
    prepared = hex_bytes(value["prepared_packed"], PACKED_BYTES, "prepared packed")
    # This formula validates input preparation. It is not a getter or setter oracle.
    require(prepared == bytes((i * 73 + (i >> 3) * 19 + (i >> 8) * 151 + 0xa7) & 255
                              for i in range(PACKED_BYTES)), "prepared input recipe")
    require(value["getter_order"] == "X/Z/Y", "getter traversal order")
    hex_bytes(value["initial_getters"], CELL_COUNT, "initial getters", getters=True)
    require(type(value["writes"]) is list and len(value["writes"]) == len(WRITES),
            "literal write cardinality")
    for index, (item, request) in enumerate(zip(value["writes"], WRITES)):
        object_keys(item, ("x", "y", "z", "value", "observed", "packed"), "write snapshot")
        for name, expected in zip(("x", "y", "z", "value"), request):
            integer(item[name], expected, expected, "literal write " + name)
        integer(item["observed"], 0, 15, "written-coordinate getter")
        hex_bytes(item["packed"], PACKED_BYTES, "write snapshot " + str(index))
    final_packed = hex_bytes(value["final_packed"], PACKED_BYTES, "final packed")
    require(final_packed == bytes.fromhex(value["writes"][-1]["packed"]),
            "final packed differs from last write snapshot")
    hex_bytes(value["final_getters"], CELL_COUNT, "final getters", getters=True)
    controls = ORIGINAL_CONTROLS if original else COMMON_CONTROLS
    object_keys(value["controls"], controls, "nibbles controls")
    require(all(type(value["controls"][name]) is bool and value["controls"][name]
                for name in controls), "nibbles control failed")
    return value


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        require(key not in value, "duplicate JSON field: " + key)
        value[key] = item
    return value


def _bad_constant(value: str) -> None:
    raise ObservationError("non-finite JSON constant: " + value)


def parse_record(raw: bytes, side: str) -> dict[str, Any] | None:
    if not raw:
        return None
    require(len(raw) < MAX_RECORD_BYTES and raw.endswith(b"\n") and raw.count(b"\n") == 1,
            "exactly one complete bounded nibbles record required")
    try:
        value = json.loads(raw.decode("utf-8", errors="strict"),
                           object_pairs_hook=_unique_object, parse_constant=_bad_constant)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise ObservationError("invalid observation JSON: " + str(error)) from error
    return validate_record(value, side)


def first_difference(left: Any, right: Any, path: str = "$") -> str | None:
    if type(left) is not type(right):
        return path + ": types differ"
    if type(left) is dict:
        if set(left) != set(right):
            return path + ": fields differ"
        for key in left:
            difference = first_difference(left[key], right[key], path + "." + key)
            if difference is not None:
                return difference
        return None
    if type(left) is list:
        if len(left) != len(right):
            return path + ": lengths differ"
        for index, (before, after) in enumerate(zip(left, right)):
            difference = first_difference(before, after, path + "[" + str(index) + "]")
            if difference is not None:
                return difference
        return None
    if left == right:
        return None
    if type(left) is str:
        index = next((i for i, pair in enumerate(zip(left, right)) if pair[0] != pair[1]),
                     min(len(left), len(right)))
        return path + ": first differing character " + str(index)
    return path + ": " + repr(left) + " != " + repr(right)


def compare_records(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    validate_record(left, left["side"])
    validate_record(right, right["side"])
    pair = left["side"] + "/" + right["side"]
    for field in COMMON_FIELDS:
        difference = first_difference(left[field], right[field], "$." + field)
        require(difference is None, pair + " " + str(difference))
    for name in COMMON_CONTROLS:
        require(left["controls"][name] == right["controls"][name], pair + " control " + name)
    return {
        "pair": pair,
        "status": "pass",
        "fresh_packed_bytes_exact": PACKED_BYTES,
        "prepared_packed_bytes_exact": PACKED_BYTES,
        "initial_getter_values_exact": CELL_COUNT,
        "write_snapshots_exact": len(WRITES),
        "written_packed_bytes_exact": PACKED_BYTES * len(WRITES),
        "written_getter_values_exact": len(WRITES),
        "final_packed_bytes_exact": PACKED_BYTES,
        "final_getter_values_exact": CELL_COUNT,
        "access_expected_results_computed": False,
    }


def reference_vectors(client: dict[str, Any], server: dict[str, Any]) -> dict[str, Any]:
    """Retain observed deltas and getters from paired originals for public regressions."""
    validate_record(client, "client")
    validate_record(server, "server")
    compare_records(client, server)
    initial = bytes.fromhex(client["initial_getters"])
    final = bytes.fromhex(client["final_getters"])
    previous = bytes.fromhex(client["prepared_packed"])
    writes = []
    for item in client["writes"]:
        current = bytes.fromhex(item["packed"])
        # Select the coordinate in the recorded X/Z/Y traversal, not in packed storage.
        cursor = (item["x"] * 16 + item["z"]) * 128 + item["y"]
        writes.append({
            **{name: item[name] for name in ("x", "y", "z", "value", "observed")},
            "initial_getter": initial[cursor], "final_getter": final[cursor],
            "changed_bytes": [{"offset": index, "before": before, "after": after}
                              for index, (before, after) in enumerate(zip(previous, current))
                              if before != after],
            "packed_sha256": hashlib.sha256(current).hexdigest(),
        })
        previous = current
    return {
        "schema": 1, "basis": "paired-original-records", "constructor_cells": CELL_COUNT,
        "packed_length": PACKED_BYTES, "getter_order": "X/Z/Y",
        "jars": {side: {"sha256": value["jar_sha256_before"], "bindings": value["bindings"]}
                 for side, value in (("client", client), ("server", server))},
        "prepared_sha256": hashlib.sha256(bytes.fromhex(client["prepared_packed"])).hexdigest(),
        "initial_getters_sha256": hashlib.sha256(initial).hexdigest(),
        "final_getters_sha256": hashlib.sha256(final).hexdigest(),
        "writes": writes,
        "access_expected_results_computed": False,
    }


def lifecycle(raw: bytes, side: str) -> dict[str, Any]:
    require(side in JARS, "lifecycle requires an original side")
    phase, markers, unavailable, controls = "class", [], [], []
    owner = JARS[side][1]
    sequence = (
        (f"NIBBLES_SETUP_BEGIN side={side} class={owner}", "class", "initializing"),
        (f"NIBBLES_SETUP_DONE side={side} class={owner}", "initializing", "ready"),
        (f"NIBBLES_MEASURE_BEGIN side={side}", "ready", "measurement"),
        (f"NIBBLES_MEASURE_DONE side={side}", "measurement", "done"),
    )
    control = ("NIBBLES_CONTROL fresh_zero array_identity initial_getters_readonly "
               "written_getters_readonly final_getters_readonly")
    for line in raw.splitlines():
        if line.startswith(b"NIBBLES_"):
            require(not unavailable, "lifecycle marker after unavailable")
            try:
                text = line.decode("ascii", errors="strict")
            except UnicodeError as error:
                raise ObservationError("non-ASCII lifecycle marker") from error
            if text.startswith("NIBBLES_CONTROL"):
                require(text == control and phase == "measurement" and not controls,
                        "invalid/duplicate/out-of-order control marker")
                controls.append(text)
                continue
            require(len(markers) < len(sequence), "excess lifecycle marker")
            expected, previous, following = sequence[len(markers)]
            require(text == expected and phase == previous, "invalid/out-of-order lifecycle marker")
            if following == "done":
                require(len(controls) == 1, "measurement completed without control marker")
            phase = following
            markers.append(text)
        elif line.startswith(b"UNAVAILABLE:"):
            try:
                text = line.decode("ascii", errors="strict")
            except UnicodeError as error:
                raise ObservationError("non-ASCII unavailable marker") from error
            require(phase in ("class", "initializing") and not unavailable and
                    re.fullmatch(r"UNAVAILABLE: nibbles setup (?:archive (?:size/type|"
                                 r"missing/unreadable|SHA-256 differs)|"
                                 r"(?:class load|initialization) [A-Za-z0-9_.$]+)", text) is not None,
                    "invalid/post-setup unavailable marker")
            require((" setup initialization " in text and phase == "initializing") or
                    (" setup initialization " not in text and phase == "class"),
                    "unavailable marker does not match setup phase")
            unavailable.append(text)
    return {"phase": phase, "markers": markers, "controls": controls, "unavailable": unavailable}


def classify_original(record: dict[str, Any] | None, markers: dict[str, Any],
                      returncode: int | None, stop_reason: str | None,
                      timed_out: bool) -> str:
    if (record is None and markers["phase"] in ("class", "initializing") and
            len(markers["unavailable"]) == 1 and returncode == 77 and
            stop_reason is None and not timed_out):
        return "unavailable"
    require(returncode == 0 and not stop_reason and not timed_out and record is not None and
            markers["phase"] == "done" and len(markers["markers"]) == 4 and
            len(markers["controls"]) == 1 and not markers["unavailable"],
            "original observation did not cleanly complete")
    return "pass"
