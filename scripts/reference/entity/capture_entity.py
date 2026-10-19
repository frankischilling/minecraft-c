#!/usr/bin/env python3
"""Capture pinned original Entity constructors; Math inputs remain explicitly inferred."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
PROCESS_HELPER = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
HELPER = ROOT / "tests/reference/entity/EntityConstructionObservation.java"
HELPER_SHA256 = "90efbd2e831a3b508933adfd59e7038b55761e3e52822784e753b42af418dc27"
JARS = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
BASE = ("aD:aG:I aE:aH:D aF:aI:Z aG:aJ:R aH:aK:R aI:aL:R "
        "aJ:aM:D aK:aN:D aL:aO:D aM:aP:D aN:aQ:D aO:aR:D "
        "aP:aS:D aQ:aT:D aR:aU:D aS:aV:F aT:aW:F aU:aX:F aV:aY:F "
        "aW:aZ:R aX:ba:Z aY:bb:Z aZ:bc:Z ba:bd:Z bb:be:Z bc:bf:Z bd:bg:Z be:bh:Z "
        "bf:bi:F bg:bj:F bh:bk:F bi:bl:F bj:bm:F bk:bn:F b:b:I "
        "bl:bo:D bm:bp:D bn:bq:D bo:br:F bp:bs:F bq:bt:Z br:bu:F bs:bv:R "
        "bt:bw:I bu:bx:I bv:by:I bw:bz:I bx:bA:Z by:bB:I bz:bC:I c:c:Z "
        "bA:-:R bB:-:R bC:bD:Z bD:bE:R bE:bF:F d:d:D e:e:D bF:bG:Z "
        "bG:bH:I bH:bI:I bI:bJ:I bJ:-:I bK:-:I bL:-:I bM:bK:Z").split()
ITEM = "a:a:R e:e:I b:b:I c:c:I f:f:I d:d:F".split()
DECIMAL = re.compile(r"(?:0|[1-9][0-9]{0,3})\Z")
HEX = {width: re.compile(r"[0-9a-f]{" + str(width) + r"}\Z") for width in (2, 8, 16)}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def identity(value, nullable=True):
    require(DECIMAL.fullmatch(value) is not None, "canonical bounded identity")
    require(nullable or value != "0", "nonnull identity required")


def parse(raw, side):
    require(len(raw) <= 131072 and raw.endswith(b"\n") and b"\r" not in raw,
            "bounded literal LF output required")
    lines = raw[:-1].decode("ascii").split("\n")
    require(len(lines) == 455, "complete 455-row corpus required")
    rows = [line.split("\t") for line in lines]
    require(rows[0] == ["ENTITY_CONSTRUCTION_V1", side], "side-specific header")
    require(rows[1] == ["POLICY", "MATH_INPUTS_INFERRED_FROM_POST_STATE",
                        "NO_CLOCK_OR_U_OBSERVATION"], "explicit inference policy")
    cursor = 2

    def take(kind, case, length):
        nonlocal cursor
        require(cursor < len(rows) - 1, "missing constructor row")
        fields = rows[cursor]
        cursor += 1
        require(len(fields) == length and fields[:2] == [kind, str(case)],
                "constructor row kind/case/width")
        return fields

    for case in range(5):
        fields = take("CASE", case, 7)
        require(fields[2] == ("WORLD" if case == 0 else "FULL"), "constructor overload")
        require(all(HEX[16].fullmatch(value) for value in fields[3:6]), "coordinate bits")
        identity(fields[6])
        supplied = fields[6]
        require((supplied != "0") == (case in (1, 3)), "supplied stack case")
        fields = take("IDENTITY", case, 9)
        for value in fields[2:]:
            identity(value)
        require(all(value != "0" for value in fields[2:7] + fields[8:]), "owned identities")
        require(len(set(fields[2:7] + fields[8:])) == 6 and fields[7] == supplied,
                "distinct owners and supplied stack identity")
        entity, bounds, watcher, record, private, _, math = fields[2:]
        references = {("B", "aW"): bounds, ("B", "bs"): private,
                      ("B", "bD"): watcher, ("I", "a"): supplied}
        for scope, specification in (("B", BASE), ("I", ITEM)):
            for token in specification:
                client, server, tag = token.split(":")
                fields = take("FIELD", case, 8)
                present = "0" if side == "server" and server == "-" else "1"
                require(fields[2:7] == [scope, client, server, tag, present], "field schema/presence")
                value = fields[7]
                if present == "0":
                    require(value == "-", "absent field marker")
                elif tag == "R":
                    identity(value)
                    require(value == references.get((scope, client), "0"), "reference-field graph")
                elif tag == "Z":
                    require(value in ("0", "1"), "boolean field")
                else:
                    require(HEX[16 if tag == "D" else 8].fullmatch(value), "primitive raw bits")
        fields = take("BOUNDS", case, 9)
        require(fields[2] == bounds and all(HEX[16].fullmatch(value) for value in fields[3:]),
                "bounds identity/bits")
        fields = take("WATCH", case, 11)
        require(fields[2] == watcher and fields[4] == record, "watcher/record identity")
        identity(fields[3], False)
        identity(fields[8], False)
        require(fields[5:8] == ["00000000", "00000000", "00"]
                and fields[9:] == ["1", "0"], "typed Byte0 constructor state")
        for kind, owner in (("PRIVATE_RANDOM", private), ("MATH_POST", math)):
            fields = take(kind, case, 6)
            require(fields[2] == owner and all(HEX[16].fullmatch(value) for value in fields[3:5])
                    and fields[5] in ("0", "1"), "Random identity/state/cache")
            require(int(fields[3], 16) < (1 << 48), "48-bit observed state")
            if kind == "PRIVATE_RANDOM":
                require(fields[4:] == ["0000000000000000", "0"], "fresh private cache")
        for ordinal in range(1, 2 if case == 0 else 5):
            fields = take("INFERRED_DRAW", case, 4)
            require(fields[2] == str(ordinal) and HEX[16].fullmatch(fields[3])
                    and int(fields[3], 16) < 0x3ff0000000000000, "inferred double input bits")
        fields = take("STACK", case, 7)
        require(fields[2] == supplied, "same supplied stack")
        require(all(HEX[8].fullmatch(value) for value in fields[3:]) if supplied != "0"
                else fields[3:] == ["-"] * 4, "numeric stack or null marker")
        fields = take("CLASSES", case, 8)
        require(fields[2] == f"{case + 1:08x}" and fields[4:6] == ["00000000", "0"]
                and fields[7] == "7", "class counter/pool/type count")
        identity(fields[3], False)
        identity(fields[6], False)
        keys = ["java.lang.Byte", "java.lang.Short", "java.lang.Integer", "java.lang.Float",
                "java.lang.String", "iz" if side == "client" else "fy",
                "br" if side == "client" else "ay"]
        for ordinal, key in enumerate(keys):
            fields = take("TYPE", case, 6)
            require(fields[2] == str(ordinal) and fields[4:] == [key, f"{ordinal:08x}"],
                    "ordered original type key/value")
            identity(fields[3], False)
    require(cursor == len(rows) - 1 and rows[-1] == ["END", "5"], "complete constructor footer")
    return {"cases": 5, "rows": 455, "field_rows": 360, "inferred_draws": 17,
            "private_random_snapshots": 5, "math_post_snapshots": 5}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    require(hashlib.sha256(PROCESS_HELPER.read_bytes()).hexdigest() == PROCESS_SHA256,
            "reviewed process helper required")
    specification = importlib.util.spec_from_file_location("entity_capture_process", PROCESS_HELPER)
    process = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(process)
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one JDK required")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "helper": HELPER,
              "recorder": Path(__file__).resolve(), "process_helper": PROCESS_HELPER,
              "modules": jdk / "lib/modules", "jvm": jdk / "bin/server/jvm.dll",
              "java_native": jdk / "bin/java.dll", "release": jdk / "release",
              "source_archive": jdk / "lib/src.zip",
              "client": options.client_jar.resolve(strict=True),
              "server": options.server_jar.resolve(strict=True)}
    before = {key: process.fingerprint(path) for key, path in inputs.items()}
    require(before["process_helper"]["sha256"] == PROCESS_SHA256, "recorded process-helper pin")
    require(before["helper"]["sha256"] == HELPER_SHA256, "frozen Entity observer required")
    require(all(before[key]["sha256"] == value for key, value in JARS.items()), "pinned originals")
    require(all(before[key]["sha256"] == value for key, value in process.PROFILE.items()),
            "retained Windows JDK profile required")
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    classes = directory / "classes"
    receipts = []
    report = {"status": "failed", "scope": "Original null-world Entity constructor observations",
              "inputs_before": before, "children": receipts,
              "limitations": ["Math inputs are inferred from actual poststate, not method-exit traces",
                              "No clock/U/prehistory or original world/caller-failure observation",
                              "Separate original sides may have different real entropy"]}
    try:
        classes.mkdir()
        compiled = process.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, HELPER],
                               directory, "compile", receipts)
        require(not compiled, "unexpected compiler stdout")
        class_files = sorted(classes.glob("*.class"))
        require([path.name for path in class_files] == ["EntityConstructionObservation.class"],
                "exact compiled observer artifact required")
        report["compiled_classes"] = {path.name: process.fingerprint(path) for path in class_files}
        observations = {}
        for side in JARS:
            raw = process.run([java, "-Xint", "-Xmx256m",
                               "--add-opens", "java.base/java.lang=ALL-UNNAMED",
                               "--add-opens", "java.base/java.util=ALL-UNNAMED",
                               "-cp", classes, "EntityConstructionObservation", side, inputs[side]],
                              directory, side, receipts)
            observations[side] = {"counts": parse(raw, side),
                                  "stdout": process.fingerprint(directory / (side + ".stdout"))}
        require(len(receipts) == 3 and all(receipt.get("reaped") is True
                and receipt.get("exit_code") == 0 and receipt.get("stderr", {}).get("bytes") == 0
                for receipt in receipts), "three successful reaped children required")
        report["compiled_classes_after"] = {
            path.name: process.fingerprint(path) for path in sorted(classes.glob("*.class"))}
        require(report["compiled_classes_after"] == report["compiled_classes"],
                "compiled observer artifacts changed")
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
    print("Captured five constructors per original side; Math inputs remain explicitly inferred.")


if __name__ == "__main__":
    main()
