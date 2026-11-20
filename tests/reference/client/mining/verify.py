#!/usr/bin/env python3
"""Verify static client mining class/Code identities without running Java."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PINS = HERE / "source-pins.json"
PARSER = ROOT / "tests/reference/client/notice/classfile.py"
CLIENT = {"bytes": 1465375,
          "sha256": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"}
MAX_METADATA = 2 * 1024 * 1024
MAX_JAR = 8 * 1024 * 1024
MAX_CLASS = 2 * 1024 * 1024
CLASS_KEYS = {"class_bytes", "class_sha256", "major", "minor", "methods"}
METHOD_KEYS = {"name", "descriptor", "access", "code_bytes", "code_sha256",
               "code_attribute_sha256", "max_stack", "max_locals"}


class Unavailable(Exception):
    pass


def need(condition, message):
    if not condition:
        raise ValueError(message)


def raw_fingerprint(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def bounded(path, limit):
    need(path.is_file() and path.stat().st_size <= limit, "bounded regular input: " + path.as_posix())
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    need(len(raw) <= limit, "input grew beyond byte limit: " + path.as_posix())
    return raw


def unique(pairs):
    value = {}
    for name, item in pairs:
        need(name not in value, "duplicate metadata key: " + name)
        value[name] = item
    return value


def metadata(path):
    return json.loads(bounded(path, MAX_METADATA), object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def digest(value):
    return type(value) is str and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def integer(value, maximum):
    return type(value) is int and 0 <= value <= maximum


def signature(method):
    return method["name"], method["descriptor"]


def validate_pins(pins):
    need(type(pins) is dict and type(pins.get("schema")) is int and pins["schema"] == 1,
         "mining source schema")
    need(pins["client_jar"] == {"path": "b1.7.3.jar", **CLIENT}, "unchanged original archive pin")
    classes = pins["classes"]
    need(type(classes) is dict and len(classes) == pins["class_count"] == 19 and
         sum(len(value["methods"]) for value in classes.values()) == pins["method_count"] == 58,
         "curated selection of 19 classes and 58 methods")
    for name, entry in classes.items():
        need(re.fullmatch(r"[A-Za-z0-9_$/]+", name) is not None and set(entry) == CLASS_KEYS,
             "class identity shape: " + name)
        need(integer(entry["class_bytes"], MAX_CLASS) and entry["class_bytes"] > 0 and
             digest(entry["class_sha256"]) and integer(entry["major"], 65535) and
             integer(entry["minor"], 65535), "bounded class/version identity: " + name)
        methods = entry["methods"]
        need(type(methods) is list and 0 < len(methods) <= 128 and
             len({signature(method) for method in methods}) == len(methods),
             "unique bounded selected methods: " + name)
        for method in methods:
            need(set(method) == METHOD_KEYS and type(method["name"]) is str and
                 type(method["descriptor"]) is str and integer(method["access"], 65535) and
                 integer(method["code_bytes"], 65535) and method["code_bytes"] > 0 and
                 digest(method["code_sha256"]) and digest(method["code_attribute_sha256"]) and
                 integer(method["max_stack"], 65535) and integer(method["max_locals"], 65535),
                 "complete method/Code/attribute/stack identity: " + name)
    need(pins["complete_method_inventories"] == ["fk", "vg"], "complete Glass/Log inventories")
    need(pins["declared_method_absences"] == [{"class": "fk", "name": "a",
         "descriptor": "(Lfd;Lgs;IIII)V", "basis": "complete declared-method inventory"}],
         "explicit Glass declared-harvest absence")
    need(type(pins["existing_method_pin_agreements"]) is list and
         len(pins["existing_method_pin_agreements"]) == 46, "complete prior 46-pin agreement selection")
    return classes


def project_path(value):
    need(type(value) is str and "\\" not in value and value.startswith("tests/reference/") and
         value.endswith("/source-pins.json") and
         all(part not in ("", ".", "..") for part in value.split("/")),
         "normalized forward-slash project metadata path")
    path = (ROOT / value).resolve(strict=True)
    need(path.is_relative_to(ROOT) and path.is_file(), "project-contained metadata path")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True, help="locally supplied original client JAR")
    args = parser.parse_args()
    report = {"schema": 1, "status": "FAIL", "scope": "Static archive/class/selected Code/Code-attribute "
              "and stack identities only; no JVM, original runtime, native gameplay, pixels or audio executed.",
              "input_before": {}, "input_after": {}}
    inputs = {"verifier": Path(__file__).resolve(), "pins": PINS, "parser": PARSER}
    limits = {role: MAX_METADATA for role in inputs}
    frozen = False
    exit_code = 1
    try:
        for role, path in inputs.items():
            report["input_before"][role] = {"path": path.as_posix(), **raw_fingerprint(bounded(path, limits[role]))}
        pins = metadata(PINS)
        classes = validate_pins(pins)
        for row in pins["existing_method_pin_agreements"]:
            need(type(row) is dict and set(row) == {"metadata", "class", "name", "descriptor"},
                 "prior pin agreement fields")
            path = project_path(row["metadata"])
            inputs.setdefault(row["metadata"], path)
            limits[row["metadata"]] = MAX_METADATA
        client = args.client.resolve()
        if not client.is_file():
            raise Unavailable("Original client archive is missing: " + client.as_posix())
        inputs["client"] = client
        limits["client"] = MAX_JAR
        before = {role: {"path": path.as_posix(), **raw_fingerprint(bounded(path, limits[role]))}
                  for role, path in inputs.items()}
        need(all(before[role] == value for role, value in report["input_before"].items()),
             "verifier/pins/parser changed during setup")
        report["input_before"] = before
        frozen = True
        raw = bounded(client, MAX_JAR)
        need(raw_fingerprint(raw) == CLIENT == {key: before["client"][key] for key in CLIENT},
             "exact original archive bytes/hash")
        spec = importlib.util.spec_from_file_location("mining_static_classfile", PARSER)
        classfile = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(classfile)
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            names = archive.namelist()
            need(len(names) <= 4096 and len(names) == len(set(names)), "bounded unique original ZIP inventory")
            for name, expected in classes.items():
                info = archive.getinfo(name + ".class")
                need(info.file_size == expected["class_bytes"] <= MAX_CLASS and
                     not info.flag_bits & 1, "bounded unencrypted selected class: " + name)
                with archive.open(info) as stream:
                    raw_class = stream.read(MAX_CLASS + 1)
                need(len(raw_class) == info.file_size, "complete bounded class read: " + name)
                actual = classfile.inspect_class(raw_class)
                if name not in pins["complete_method_inventories"]:
                    selected = {signature(method) for method in expected["methods"]}
                    actual["methods"] = [method for method in actual["methods"] if signature(method) in selected]
                need(actual == expected, "exact class/Code/attribute/max-stack pins: " + name)
        for absence in pins["declared_method_absences"]:
            need(absence["class"] in pins["complete_method_inventories"] and
                 (absence["name"], absence["descriptor"]) not in
                 {signature(method) for method in classes[absence["class"]]["methods"]},
                 "complete declared-method absence: " + absence["class"])
        previous = {name: metadata(path) for name, path in inputs.items() if name.startswith("tests/")}
        for row in pins["existing_method_pin_agreements"]:
            known_classes = previous[row["metadata"]]["classes"]
            known = known_classes.get(row["class"], known_classes.get(row["class"].replace("/", ".")))
            need(known is not None, "prior class pin exists: " + row["class"])
            selected = (row["name"], row["descriptor"])
            older = [method for method in known["methods"] if signature(method) == selected]
            current = [method for method in classes[row["class"]]["methods"] if signature(method) == selected]
            need(len(older) == len(current) == 1 and all(current[0].get(key) == value
                 for key, value in older[0].items()), "unchanged existing method agreement: " + row["class"])
        report.update(status="PASS", class_count=len(classes), method_count=pins["method_count"],
                      complete_method_inventories=pins["complete_method_inventories"],
                      glass_declared_harvest_absent=True, existing_method_agreements=46,
                      verified_code_attributes=58, verified_stack_limits=58)
        exit_code = 0
    except Unavailable as error:
        report.update(status="UNAVAILABLE", error={"type": type(error).__name__, "message": str(error)})
        exit_code = 77
    except BaseException as error:
        report.update(status="FAIL", error={"type": type(error).__name__, "message": str(error)})
        exit_code = 130 if isinstance(error, KeyboardInterrupt) else 1
    finally:
        failures = {}
        for role, path in inputs.items():
            try:
                report["input_after"][role] = {"path": path.as_posix(), **raw_fingerprint(bounded(path, limits[role]))}
            except Exception as error:
                failures[role] = {"type": type(error).__name__, "message": str(error)}
        if any(report["input_after"].get(role) != value
               for role, value in report["input_before"].items()) or (
                frozen and report["input_before"] != report["input_after"]):
            failures["continuity"] = "Verifier, metadata, parser or client changed"
        if failures:
            report.update(status="FAIL", finalization_errors=failures)
            exit_code = 1
        print(json.dumps(report, indent=2))
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
