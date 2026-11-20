#!/usr/bin/env python3
"""Capture actual original Biome graphs and neutral declared query/control results.

This recorder never invokes a native implementation or manufactures oracle data.
Original console streams and the helper's explicit protocol file stay separate.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "tests/reference/world/BiomeRegistryObservation.java"
PROCESS_HELPER = ROOT / "scripts/reference/java/compare_int_set.py"
PROCESS_SHA256 = "cd34681283655cb9670be475c158547df7b5894e80feccc0dda8c168ffdcf05c"
HELPER_SHA256 = "6fabed340cda463d49f0e5c4c7f4df32df49ad6763416795041b27f30ceb8d21"
MAX_PROTOCOL = 4 * 1024 * 1024
JARS = {
    "client": {"bytes": 1465375, "sha256": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"},
    "server": {"bytes": 503100, "sha256": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"},
}
TYPES = {
    "client": ("yj", "tf", "kd", "rb", "fs", "kd", "g", "fs", "fs", "fs", "kd", "t", "ry"),
    "server": ("pp", "ma", "gs", "kk", "dw", "gs", "f", "dw", "dw", "dw", "gs", "m", "le"),
}
ENTITIES = {
    "client": {"cn", "uz", "fr", "gb", "uw", "dl", "wh", "ww", "bx", "xt", "gi", "bp", "ya"},
    "server": {"bn", "ne", "dv", "ec", "nb", "ce", "oc", "or", "bd", "pf", "eh", "aw", "pi"},
}
MODES = {"interpreted": ["-Xint"], "tier1": ["-XX:TieredStopAtLevel=1"], "default": []}


def require(value, message):
    if not value:
        raise ValueError(message)


def integer(value, minimum=-(2**31), maximum=2**31 - 1):
    require(re.fullmatch(r"0|-?[1-9][0-9]{0,9}", value) is not None,
            "canonical signed decimal")
    number = int(value)
    require(minimum <= number <= maximum, "bounded integer")
    return number


def literal_rows(raw, maximum):
    require(0 < len(raw) <= maximum and raw.endswith(b"\n") and b"\r" not in raw,
            "bounded literal LF data")
    require(all(value in (9, 10) or 32 <= value < 127 for value in raw), "ASCII TAB/LF data")
    return [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]


def declarations(raw):
    rows = literal_rows(raw, 256 * 1024)
    require(rows[0] == ["BIOME_REGISTRY_REQUEST_V1"] and rows[-1] == ["END"],
            "neutral declaration framing")
    operations, labels, snapshots = rows[1:-1], set(), 2
    require(len(operations) <= 4096, "bounded operation count")
    for row in operations:
        require(len(row) >= 2 and re.fullmatch(r"[A-Za-z0-9_.-]{1,48}", row[1]) is not None
                and row[1] not in labels and row[1] not in ("baseline", "warm-rebuild"),
                "unique neutral operation labels")
        labels.add(row[1])
        if row[0] in ("F", "D"):
            digits = 8 if row[0] == "F" else 16
            require(len(row) == 4 and all(re.fullmatch("[0-9a-f]{" + str(digits) + "}", item)
                                          for item in row[2:]), "typed raw query bits")
        elif row[0] == "SET_BYTE":
            require(len(row) == 5 and row[2] in "abcdefghijklm" and len(row[2]) == 1
                    and row[3] in ("p", "q"), "public byte control declaration")
            integer(row[4], -128, 127)
        elif row[0] == "SET_TABLE":
            require(len(row) == 4 and (row[3] == "NULL" or
                    len(row[3]) == 1 and row[3] in "abcdefghijklm"), "array control declaration")
            integer(row[2], 0, 4095)
        else:
            require(row[0] in ("REBUILD", "SNAPSHOT") and len(row) == 2,
                    "known declaration with no expected result")
            snapshots += row[0] == "SNAPSHOT"
    require(snapshots <= 8, "bounded graph snapshot count")
    return operations


def protocol(raw, side, operations):
    """Validate the complete observed wire, never compute a game query result."""
    rows = literal_rows(raw, MAX_PROTOCOL)
    cursor, snapshots = 0, 0
    references, named, entity_names = {}, {}, {}
    table_identity = None

    def take(kind, width):
        nonlocal cursor
        require(cursor < len(rows), "missing " + kind)
        row = rows[cursor]
        cursor += 1
        require(len(row) == width and row[0] == kind, "row schema/order: " + kind)
        return row

    def identity(value, kind, nullable=False):
        if value == "NULL":
            require(nullable, "nonnull " + kind)
            return
        require(re.fullmatch(r"R:[1-9][0-9]{0,2}", value) is not None, "literal identity")
        if value not in references:
            require(len(references) < 512 and value == "R:" + str(len(references) + 1),
                    "first-seen actual identity numbering")
            references[value] = kind
        require(references[value] == kind, "identity kind/bijection")

    def exception(value):
        require(re.fullmatch(r"[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*", value)
                is not None, "actual unwrapped exception class name")

    def snapshot(label, controlled):
        nonlocal snapshots, table_identity
        require(take("SNAPSHOT", 2) == ["SNAPSHOT", label], "snapshot label")
        biomes, strings, lists, records, classes = set(), set(), set(), set(), set()
        for index, selector in enumerate("abcdefghijklm"):
            row = take("NAMED", 4)
            require(row[1:3] == [label, selector], "named source order")
            owner = row[3]
            identity(owner, "biome")
            require(owner not in biomes, "distinct named owners")
            biomes.add(owner)
            if selector not in named:
                named[selector] = owner
            require(named[selector] == owner, "stable actual named identities")
            row = take("BIOME", 11)
            require(row[1:4] == [label, owner, TYPES[side][index]], "Biome owner/concrete type")
            name = row[4]
            identity(name, "string")
            require(name not in strings, "distinct source name String identities")
            strings.add(name)
            integer(row[5])
            integer(row[6], -128, 127)
            integer(row[7], -128, 127)
            integer(row[8])
            require(row[9] in ("0", "1") and row[10] in ("0", "1"), "actual flags")
            text = take("STRING", 5)
            require(text[1:3] == [label, name], "name identity")
            length = integer(text[3], 0, 15)
            require(text[4] == "-" if length == 0 else
                    re.fullmatch("[0-9a-f]{" + str(4 * length) + "}", text[4]) is not None,
                    "literal UTF16 name units")
            for field in "stu":
                row = take("LIST", 6)
                require(row[1:4] == [label, owner, field], "three actual list fields")
                list_ref = row[4]
                identity(list_ref, "list")
                require(list_ref not in lists, "distinct list owners")
                lists.add(list_ref)
                length = integer(row[5], 0, 5)
                for ordinal in range(length):
                    row = take("ELEMENT", 5)
                    require(row[1:4] == [label, list_ref, str(ordinal)], "actual list order")
                    record = row[4]
                    identity(record, "record")
                    require(record not in records, "distinct live spawn records")
                    records.add(record)
                    row = take("RECORD", 5)
                    require(row[1:3] == [label, record], "actual record owner")
                    entity = row[3]
                    identity(entity, "entity-class")
                    integer(row[4])
                    if entity not in classes:
                        actual = take("ENTITY_CLASS", 4)
                        require(actual[1:3] == [label, entity] and actual[3] in ENTITIES[side],
                                "actual side-specific entity Class")
                        require(entity_names.setdefault(entity, actual[3]) == actual[3],
                                "stable Class name identity")
                        classes.add(entity)
        require((len(biomes), len(strings), len(lists), len(records), len(classes)) ==
                (13, 13, 39, 115, 13), "complete source-sized graph")
        require(len({entity_names[value] for value in classes}) == 13,
                "thirteen distinct Class-name bindings")
        table = take("TABLE", 4)
        require(table[1] == label and table[3] == "4096", "complete table extent")
        identity(table[2], "table")
        if table_identity is None:
            table_identity = table[2]
        require(table[2] == table_identity, "one actual stable table after the graph traversal")
        for index in range(4096):
            row = take("TABLE_SLOT", 4)
            require(row[1:3] == [label, str(index)], "complete actual table order")
            require(row[3] in biomes or controlled and row[3] == "NULL", "actual named/NULL table alias")
        require(take("END_SNAPSHOT", 6) == ["END_SNAPSHOT", label, "13", "39", "115", "13"],
                "snapshot closure")
        snapshots += 1

    require(take("BIOME_REGISTRY_V1", 2) == ["BIOME_REGISTRY_V1", side], "side/header")
    require(take("POLICY", 4) == ["POLICY", "UNTOUCHED_ORIGINAL_BOOTSTRAP",
            "EXPLICIT_MUTATION_CONTROLS", "NOT_FULL_WORLD_OR_GAME_STARTUP"], "scope policy")
    require(take("BOOTSTRAP", 2) == ["BOOTSTRAP", "RETURN"], "ordinary class initialization returned")
    snapshot("baseline", False)
    snapshot("warm-rebuild", False)
    for operation in operations:
        kind, label = operation[:2]
        if kind == "SNAPSHOT":
            snapshot(label, True)
        elif kind in ("F", "D"):
            row = take("QUERY", 7)
            require(row[1:5] == [label, kind, *operation[2:]], "literal query input correspondence")
            require(row[5] in ("RETURN", "THROW"), "actual target outcome")
            if row[5] == "THROW":
                exception(row[6])
            else:
                require(row[6] == "NULL" or row[6] in named.values(), "actual returned named alias")
        elif kind in ("SET_BYTE", "SET_TABLE"):
            row = take("CONTROL", len(operation) + 1)
            require(row == ["CONTROL", label, kind, *operation[2:]], "explicit actual mutation control")
        else:
            require(cursor < len(rows) and rows[cursor][:3] == ["CONTROL", label, "REBUILD"],
                    "actual helper invocation")
            row = rows[cursor]
            cursor += 1
            require(row[3:] == ["RETURN"] or len(row) == 5 and row[3] == "THROW",
                    "actual rebuild outcome")
            if row[3] == "THROW":
                exception(row[4])
    require(take("END", 4) == ["END", str(snapshots), str(len(operations)), str(len(references))]
            and cursor == len(rows) and len(references) == 194, "complete observation closure")
    return {"rows": len(rows), "snapshots": snapshots, "operations": len(operations),
            "references": len(references), "biomes": 13, "lists": 39,
            "live_records": 115, "entity_classes": 13, "table_slots_per_snapshot": 4096}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client-jar", "server-jar", "java", "javac", "requests", "output-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    options = parser.parse_args()
    require(hashlib.sha256(PROCESS_HELPER.read_bytes()).hexdigest() == PROCESS_SHA256,
            "frozen bounded/reaped child runner")
    spec = importlib.util.spec_from_file_location("biome_registry_process", PROCESS_HELPER)
    process = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(process)
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one supplied modular JDK")
    require(all(not os.environ.get(name) for name in
                ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")),
            "no ambient Java agent/options/classpath injection")
    inputs = process.jdk_inputs(java, javac)
    inputs.update({"python": Path(sys.executable).resolve(strict=True),
                   "helper": HELPER.resolve(strict=True), "recorder": Path(__file__).resolve(),
                   "process_helper": PROCESS_HELPER.resolve(strict=True),
                   "requests": options.requests.resolve(strict=True),
                   "client": options.client_jar.resolve(strict=True),
                   "server": options.server_jar.resolve(strict=True)})
    require(not options.output_dir.exists(), "fresh capture output root")
    work = process.capture_root(options.output_dir, [str(inputs["python"]), str(java), str(javac)])
    records, outputs = [], {}
    report = {
        "schema": 1, "status": "failed", "tested_commit": None,
        "scope": "actual original Biome class bootstrap, complete live graph and declared controls",
        "input_paths": {key: str(path) for key, path in inputs.items()},
        "children": records, "observations": outputs,
        "limits": {"child_seconds": 30, "overall_seconds": 300,
                   "stream_bytes_each": process.MAX_OUTPUT, "protocol_bytes_each": MAX_PROTOCOL},
        "limitations": ["Ordinary Biome class initialization return is not full game/World startup",
                        "Mutation controls deliberately change actual p/q or table cells only",
                        "Actual Java target exception names do not imply native Throwable identities",
                        "No failed-class Field.get, fake READY, heap census or OOM timing equivalence",
                        "No original assets/classfiles/JARs are copied into public fixtures"],
    }
    classes = work / "classes"
    try:
        before = {key: process.file_fingerprint(path) for key, path in inputs.items()}
        report["inputs_before"] = before
        require(before["helper"]["sha256"] == HELPER_SHA256, "frozen reviewed observer")
        require(all(before[side] == pin for side, pin in JARS.items()), "pinned original JAR bytes")
        require(inputs["requests"].stat().st_size <= 256 * 1024, "bounded request input")
        operations = declarations(inputs["requests"].read_bytes())
        report["declarations"] = {"operations": len(operations),
                                   "snapshots": 2 + sum(row[0] == "SNAPSHOT" for row in operations),
                                   "queries": sum(row[0] in ("F", "D") for row in operations)}
        deadline = time.monotonic() + 300
        for role, command in (("java-version", [str(java), "-version"]),
                              ("javac-version", [str(javac), "-version"])):
            process.run(command, work=work, name=role, records=records, deadline=deadline)
        classes.mkdir(exist_ok=False)
        lookup = work / "empty-lookup"
        lookup.mkdir(exist_ok=False)
        stdout, stderr = process.run(
            [str(javac), "-J-Xmx256m", "--release", "17", "-encoding", "UTF-8",
             "-proc:none", "-implicit:none", "-classpath", str(lookup), "-sourcepath", str(lookup),
             "-d", str(classes), str(HELPER)], work=work, name="compile", records=records,
            deadline=deadline)
        require(not any(lookup.iterdir()), "empty compiler class/source lookup retained")
        require(not stdout and not stderr, "quiet successful one-time helper compilation")
        compiled = {path.name: process.file_fingerprint(path) for path in sorted(classes.glob("*.class"))}
        require(set(compiled) == {"BiomeRegistryObservation.class", "BiomeRegistryObservation$Request.class"},
                "exact project observer compiled class inventory")
        report["compiled_classes"] = compiled
        for side in JARS:
            first = None
            for mode, flags in MODES.items():
                name = side + "-" + mode
                target = work / (name + ".tsv")
                outputs[name] = {"protocol_path": str(target), "side": side, "mode": mode}
                _, stderr = process.run(
                    [str(java), *flags, "-Xmx256m", "-cp", str(classes),
                     "BiomeRegistryObservation", side, str(inputs[side]),
                     str(inputs["requests"]), str(target)], work=work, name=name,
                    records=records, deadline=deadline)
                require(not stderr, "successful observer stderr retained without filtering")
                require(target.is_file() and not target.is_symlink()
                        and target.stat().st_size <= MAX_PROTOCOL, "bounded explicit protocol file")
                raw = target.read_bytes()
                outputs[name].update(protocol=process.fingerprint(raw), counts=protocol(raw, side, operations))
                if first is None:
                    first = raw
                else:
                    require(raw == first, "three actual execution modes have identical complete graph/results")
        require(len(records) == 9 and all(row["reaped"] is True and row["exit"] == 0
                and row["timed_out"] is False and row["output_limited"] is False for row in records),
                "all nine actual children successful/bounded/reaped")
        report.update(status="passed", three_modes_equal_per_side=True)
    except BaseException as error:
        report["error"] = type(error).__name__ + ": " + str(error)
    finally:
        try:
            report["inputs_after"] = {key: process.file_fingerprint(path) for key, path in inputs.items()}
            require(report.get("inputs_before") == report["inputs_after"], "all measured inputs unchanged")
            if classes.exists():
                report["compiled_classes_after"] = {
                    path.name: process.file_fingerprint(path) for path in sorted(classes.glob("*.class"))}
                if "compiled_classes" in report:
                    require(report["compiled_classes_after"] == report["compiled_classes"],
                            "compiled observer artifacts unchanged")
            for observation in outputs.values():
                target = Path(observation["protocol_path"])
                if target.is_file():
                    require(target.stat().st_size <= MAX_PROTOCOL, "retained protocol bound")
                    observation["retained_protocol"] = process.file_fingerprint(target)
        except BaseException as error:
            report["status"] = "failed"
            report["preservation_error"] = type(error).__name__ + ": " + str(error)
        report_path = work / "report.json"
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="ascii")
    print(str(report_path))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
