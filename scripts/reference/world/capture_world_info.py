#!/usr/bin/env python3
"""Capture unmodified original WorldInfo graphs; retain clock outputs separately."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
PROCESS_HELPER = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
HELPER = ROOT / "tests/reference/world/WorldInfoObservation.java"
HELPER_SHA256 = "69bea70d6d21914687ceee6fa9211b57e367884f22dd14e6d03610869c7afb4d"
JARS = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
FIELD_TYPES = "JIIIJJJCISIZIZI"
TAG_TYPES = (4, 3, 3, 3, 4, 4, 4, 8, 3, 3, 1, 3, 1, 10)
HEX = {n: re.compile(r"[0-9a-f]{" + str(n) + r"}\Z") for n in (2, 8, 16)}
ID = re.compile(r"(?:0|[1-9][0-9]{0,3})\Z")
UNITS = re.compile(r"(?:[0-9a-f]{4}){1,64}\Z")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def identity(value, nullable=True):
    require(ID.fullmatch(value) is not None and (nullable or value != "0"), "identity schema")


def text(fields):
    require(len(fields) == 5, "text row width")
    identity(fields[2])
    length = fields[3]
    if fields[2] == "0":
        require(fields[3:] == ["-1", "-"], "null text marker")
    else:
        require(ID.fullmatch(length) is not None and int(length) <= 64, "text length")
        require(fields[4] == "-" if length == "0" else
                UNITS.fullmatch(fields[4]) is not None and len(fields[4]) == 4 * int(length),
                "text code units")


def parse(raw, side):
    require(len(raw) <= 131072 and raw.endswith(b"\n") and b"\r" not in raw,
            "bounded literal LF output")
    lines = raw[:-1].decode("ascii").split("\n")
    rows = [line.split("\t") for line in lines]
    client = side == "client"
    require(len(rows) == (432 if client else 326), "complete original row count")
    require(rows[:3] == [["WORLD_INFO_OBSERVATION_V1", side],
                        ["POLICY", "OBJECT_GRAPH_ONLY", "EXCLUDE_ONLY_CLOCK_OUTPUT_FROM_STABLE_FIXTURE"],
                        ["CAPABILITY", "CLIENT_PLAYER_SET", "1" if client else "0"]],
            "header/policy/capability")
    cursor = 3
    snapshots, serials, clocks, refusals = [], [], [], []

    def take(kind, ordinal, count):
        nonlocal cursor
        require(cursor < len(rows) - 1, "missing observation row")
        row = rows[cursor]
        cursor += 1
        require(len(row) == count and row[:2] == [kind, str(ordinal)], "row schema/order")
        return row

    while cursor < len(rows) - 1:
        row = rows[cursor]
        require(len(row) >= 2 and ID.fullmatch(row[1]) is not None, "event ordinal")
        ordinal = int(row[1])
        if row[0] == "SNAPSHOT":
            identity(take("SNAPSHOT", ordinal, 3)[2], False)
            snapshots.append(ordinal)
            for selector, kind in zip("abcdefghijklmno", FIELD_TYPES):
                field = take("FIELD", ordinal, 5)
                require(field[2:4] == [selector, kind], "complete field mapping")
                if kind in "CS":
                    identity(field[4])
                elif kind == "Z":
                    require(field[4] in ("0", "1"), "boolean field")
                else:
                    require(HEX[16 if kind == "J" else 8].fullmatch(field[4]) is not None,
                            "scalar raw bits")
            text(take("NAME", ordinal, 5))
            player = take("PLAYER", ordinal, 10)
            for index in (2, 3, 6, 8):
                identity(player[index])
            if player[2] == "0":
                require(player[3:] == ["0", "-1", "-", "0", "-", "0", "-"],
                        "absent Player schema")
            else:
                text(["NAME", player[1], player[3], player[4], player[5]])
                require(all(player[index] != "0" for index in (3, 6, 8)) and
                        all(HEX[8].fullmatch(player[index]) for index in (7, 9)),
                        "Player typed children")
        elif row[0] == "SERIAL":
            serial = take("SERIAL", ordinal, 5)
            identity(serial[2], False)
            identity(serial[3], False)
            require(serial[4] in ("13", "14"), "serializer child count")
            serials.append(ordinal)
            for key, tag_type in enumerate(TAG_TYPES):
                field = take("SERIAL_FIELD", ordinal, 5)
                require(field[2:4] == [str(key), str(tag_type)], "serialized key/type schema")
                if key == 5:
                    require(field[4] == "SAMPLED", "separate actual clock marker")
                    sample = take("CLOCK_OUTPUT", ordinal, 3)
                    require(HEX[16].fullmatch(sample[2]) is not None, "actual clock raw bits")
                    clocks.append(ordinal)
                elif tag_type == 8:
                    identity(field[4], False)
                    text(take("SERIAL_NAME", ordinal, 5))
                elif tag_type == 10:
                    identity(field[4])
                    require((field[4] != "0") == (serial[4] == "14"), "optional selected Player")
                else:
                    width = 16 if tag_type == 4 else 8 if tag_type == 3 else 2
                    require(HEX[width].fullmatch(field[4]) is not None, "serialized scalar raw bits")
        elif row[0] == "EXCEPTION":
            refusal = take("EXCEPTION", ordinal, 8)
            identity(refusal[2], False)
            require(refusal[3:] == ["java.lang.IllegalArgumentException", "1", "1", "1", "1"],
                    "actual null-name refusal")
            refusals.append(ordinal)
        else:
            raise ValueError("unknown observation event")
    wanted_snapshots = list(range(19)) if client else list(range(11)) + [15, 16, 17, 18]
    wanted_serials = list(range(5)) if client else [0, 3, 4]
    require(snapshots == wanted_snapshots and serials == wanted_serials and clocks == serials
            and refusals == [5], "complete finite event inventory")
    require(rows[-1] == ["END", str(len(snapshots)), str(len(serials)), str(len(clocks)), "1"],
            "complete original footer")
    stable = b"".join(line.encode("ascii") + b"\n" for line in lines
                      if not line.startswith("CLOCK_OUTPUT\t"))
    return {"rows": len(rows), "snapshots": len(snapshots), "serializations": len(serials),
            "clock_outputs": len(clocks), "refusals": 1, "stable_rows": len(stable.splitlines())}, stable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    require(hashlib.sha256(PROCESS_HELPER.read_bytes()).hexdigest() == PROCESS_SHA256,
            "reviewed process helper")
    spec = importlib.util.spec_from_file_location("world_info_capture_process", PROCESS_HELPER)
    process = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(process)
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one pinned JDK")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "helper": HELPER,
              "recorder": Path(__file__).resolve(), "process_helper": PROCESS_HELPER,
              "modules": jdk / "lib/modules", "jvm": jdk / "bin/server/jvm.dll",
              "java_native": jdk / "bin/java.dll", "release": jdk / "release",
              "source_archive": jdk / "lib/src.zip",
              "client": options.client_jar.resolve(strict=True),
              "server": options.server_jar.resolve(strict=True)}
    before = {key: process.fingerprint(path) for key, path in inputs.items()}
    require(before["helper"]["sha256"] == HELPER_SHA256, "frozen original observer")
    require(all(before[key]["sha256"] == value for key, value in JARS.items()), "pinned originals")
    require(all(before[key]["sha256"] == value for key, value in process.PROFILE.items()),
            "pinned Windows JDK profile")
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    receipts = []
    report = {"status": "failed", "scope": "Unmodified original WorldInfo object graphs",
              "inputs_before": before, "children": receipts,
              "limitations": ["Only CLOCK_OUTPUT rows are removed from stable fixtures",
                              "No exact clock equality, internal clock-count or JVM bootstrap proof",
                              "No World startup, storage handler, binary NBT or live-player-list route"]}
    try:
        classes = directory / "classes"
        classes.mkdir()
        compiled = process.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, HELPER],
                               directory, "compile", receipts)
        require(not compiled, "unexpected compiler stdout")
        class_files = sorted(classes.glob("*.class"))
        require([path.name for path in class_files] ==
                ["WorldInfoObservation$Binding.class", "WorldInfoObservation.class"],
                "exact compiled observer artifacts")
        report["compiled_classes"] = {path.name: process.fingerprint(path) for path in class_files}
        observations = {}
        for side in JARS:
            raw = process.run([java, "-Xint", "-Xmx256m", "-cp", classes,
                               "WorldInfoObservation", side, inputs[side]], directory, side, receipts)
            counts, stable = parse(raw, side)
            target = directory / (side + ".stable.tsv")
            target.write_bytes(stable)
            observations[side] = {"counts": counts, "stdout": process.fingerprint(directory / (side + ".stdout")),
                                  "stable": process.fingerprint(target)}
        require(len(receipts) == 3 and all(item.get("reaped") is True and item.get("exit_code") == 0
                and item.get("stderr", {}).get("bytes") == 0 for item in receipts),
                "three successful reaped children")
        report["compiled_classes_after"] = {
            path.name: process.fingerprint(path) for path in sorted(classes.glob("*.class"))}
        require(report["compiled_classes_after"] == report["compiled_classes"], "compiled artifacts changed")
        report.update(status="passed", observations=observations)
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            report["inputs_after"] = {key: process.fingerprint(path) for key, path in inputs.items()}
            require(report["inputs_after"] == before, "capture inputs changed")
        except BaseException as error:
            report["status"] = "failed"
            report["input_error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            with (directory / "capture.json").open("x", encoding="ascii", newline="\n") as stream:
                json.dump(report, stream, indent=2, sort_keys=True)
                stream.write("\n")
    print("Captured original WorldInfo graphs and null-name refusals; real clock rows retained separately.")


if __name__ == "__main__":
    main()
