#!/usr/bin/env python3
"""Capture original SavedData ID operations, real handlers and retained files."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
PROCESS_HELPER = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
HELPER = ROOT / "tests/reference/world/SavedDataIdObservation.java"
HELPER_SHA256 = "dca5cf6d736433877b719b1c25b311a280337bb46588fdf3c848c7b4defd5600"
JARS = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
        "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
CASES = ("null-store", "missing", "prepared", "raw-reload", "stale-session", "two-owners",
         "malformed", "directory", "null-empty", "long-utf", "proxy-path")
LABEL = re.compile(r"[A-Za-z0-9.-]+\Z")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parse(raw, side, directory):
    require(0 < len(raw) <= 1 << 20 and raw.endswith(b"\n") and b"\r" not in raw,
            "bounded literal-LF original output")
    rows = [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]
    require(rows[0] == ["SAVED_DATA_IDS_V1", side], "original header")
    keys, owners, counts, cases = {}, {}, {}, []
    pending = []
    map_context = None
    previous_key = None
    final_files = {}
    memory = []
    for row in rows[1:-1]:
        kind = row[0]
        counts[kind] = counts.get(kind, 0) + 1
        if pending:
            require(kind == "VALUE", "incomplete original map")
        if kind == "KEY":
            require(len(row) == 4, "key width")
            key, length = int(row[1]), int(row[2])
            require(key == len(keys) and -1 <= length <= 21846, "key identity/length")
            if length < 1:
                require(row[3] == "-" and (key == 0) == (length == -1), "null/empty key")
            else:
                require(re.fullmatch(r"[0-9a-f]{" + str(4 * length) + "}", row[3]), "UTF-16 key")
            keys[key] = None if length == -1 else row[3] if length else ""
        elif kind == "CASE":
            require(len(row) == 2, "case width")
            cases.append(row[1])
        elif kind == "NEW":
            require(len(row) == 4 and int(row[2]) == len(owners) + 1 and row[3] in ("0", "1"),
                    "owner publication")
            owners[int(row[2])] = row[1]
            if row[3] == "0":
                require(row[1] == "null-store.new", "unexpected memory owner")
                memory.append(row)
        elif kind == "NEXT":
            require(len(row) == 6 and int(row[2]) in owners and int(row[4]) in keys and
                    0 <= int(row[3]) <= 65565 and -32768 <= int(row[5]) <= 32767,
                    "actual signed ID return")
            if row[1].startswith("null-store."):
                memory.append(row)
        elif kind == "MAP":
            require(len(row) == 4 and int(row[2]) in owners and 0 <= int(row[3]) <= 16,
                    "map snapshot")
            pending = list(range(int(row[3])))
            map_context = row[1:3]
            previous_key = None
            if row[1].startswith("null-store."):
                memory.append(row)
        elif kind == "VALUE":
            require(len(row) == 6 and row[1:3] == map_context and pending and int(row[3]) == pending.pop(0) and
                    int(row[2]) in owners and int(row[4]) in keys and -32768 <= int(row[5]) <= 32767,
                    "actual Short map entry")
            value_key = keys[int(row[4])]
            display = (0, "") if value_key is None else (1, value_key)
            require(previous_key is None or previous_key < display, "canonical map display")
            previous_key = display
            if row[1].startswith("null-store."):
                memory.append(row)
        elif kind == "DIAG":
            require(len(row) == 5 and LABEL.fullmatch(row[1]) and
                    row[4] == "diagnostics/" + row[1] + ".stderr", "diagnostic path")
            actual = (directory / row[4]).read_bytes()
            require(len(actual) == int(row[3]) <= 1 << 16, "complete diagnostic bytes")
            first = actual.decode("utf-8").splitlines()[0].split(":", 1)[0] if actual else "-"
            require(row[2] == first, "actual diagnostic category")
        elif kind == "FILE":
            require(len(row) == 6 and LABEL.fullmatch(row[1]) and
                    re.fullmatch(r"[a-z-]+/(?:data/idcounts[.]dat|session[.]lock)", row[2]), "file path")
            require(row[3] in ("MISSING", "DIRECTORY", "REGULAR"), "file kind")
            if row[3] == "REGULAR":
                value = bytes.fromhex(row[5]) if row[5] != "-" else b""
                require(len(value) == int(row[4]) <= 1 << 20 and
                        (directory / ("snapshots/" + row[1] + ".bin")).read_bytes() == value,
                        "actual lossless file snapshot")
            else:
                require(row[4:] == ["0", "-"], "nonregular file schema")
            final_files[row[2]] = row
        elif kind == "STORAGE":
            require(len(row) == 3 and row[2] in ("REAL", "PROXY_PATH_ONLY"), "storage route")
        elif kind == "SESSION_CAPTURE":
            require(len(row) == 4 and row[1] in CASES and row[2] == "session-created/" + row[1] + ".bin" and
                    row[3] == "8" and len((directory / row[2]).read_bytes()) == 8,
                    "retained real constructor clock token")
        elif kind == "PATH":
            require(len(row) == 7 and row[1] == "proxy-path" and int(row[3]) == counts[kind] and
                    row[4:] == ["8", "006900640063006f0075006e00740073", "proxy-path/data/idcounts.dat"],
                    "proxy filename trace")
        elif kind == "REL":
            require(row == ["REL", "two-owners.same-counter-map", "0"], "independent original maps")
        else:
            raise ValueError("unknown original row: " + kind)
    require(not pending and cases == list(CASES), "complete original cases/maps")
    for kind, number in {"CASE": 11, "NEW": 16, "NEXT": 38, "MAP": 49, "FILE": 33,
                         "PATH": 3, "DIAG": 54, "STORAGE": 10, "SESSION_CAPTURE": 9, "REL": 1}.items():
        require(counts.get(kind) == number, "complete original " + kind + " count")
    require(rows[-1] == ["END", "11", "16", "65566", "38", "49", "33", "3", "54",
                         str(len(keys)), str(len(rows) - 1)], "original footer")
    for relative, row in final_files.items():
        path = directory / relative
        if row[3] == "REGULAR":
            require(path.is_file() and path.read_bytes() ==
                    (directory / ("snapshots/" + row[1] + ".bin")).read_bytes(), "terminal file bytes")
        elif row[3] == "DIRECTORY":
            require(path.is_dir(), "terminal directory")
        else:
            require(not path.exists(), "terminal missing file")
    require(sum(row[0] == "NEXT" for row in memory) == 15 and
            sum(row[0] == "MAP" for row in memory) == 7, "actual memory observations")
    return {"rows": len(rows), "counts": counts, "keys": keys, "memory": memory}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    require(hashlib.sha256(PROCESS_HELPER.read_bytes()).hexdigest() == PROCESS_SHA256, "process helper pin")
    spec = importlib.util.spec_from_file_location("saved_data_process", PROCESS_HELPER)
    process = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(process)
    java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one pinned JDK")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "helper": HELPER, "recorder": Path(__file__).resolve(),
              "process_helper": PROCESS_HELPER, "modules": jdk / "lib/modules",
              "jvm": jdk / "bin/server/jvm.dll", "java_native": jdk / "bin/java.dll",
              "release": jdk / "release", "source_archive": jdk / "lib/src.zip",
              "client": args.client_jar.resolve(strict=True), "server": args.server_jar.resolve(strict=True)}
    before = {name: process.fingerprint(path) for name, path in inputs.items()}
    require(before["helper"]["sha256"] == HELPER_SHA256, "frozen observer pin")
    require(all(before[name]["sha256"] == value for name, value in JARS.items()), "original JAR pins")
    require(all(before[name]["sha256"] == value for name, value in process.PROFILE.items()), "pinned JDK profile")
    directory = args.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    children = []
    report = {"status": "failed", "scope": "Original SavedData IDs with real storage handlers and one proxy control",
              "inputs_before": before, "children": children,
              "limitations": ["Constructed SavedData operations, no full World construction or named subtype",
                              "Raw original files and diagnostics retained; real session clocks are not stable expectations",
                              "Native memory replay is separate; collection-dependent collision persistence remains unfinished"]}
    try:
        classes = directory / "classes"
        classes.mkdir()
        require(not process.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, HELPER],
                                directory, "compile", children), "compiler stdout")
        artifacts = sorted(classes.glob("*.class"))
        expected = ["SavedDataIdObservation" + suffix + ".class" for suffix in
                    ("$Action", "$Binding", "$LimitedBytes", "$Owner", "")]
        require([path.name for path in artifacts] == sorted(expected), "exact observer classes")
        report["classes_before"] = {path.name: process.fingerprint(path) for path in artifacts}
        observations = {}
        for side in JARS:
            work = directory / (side + "-worlds")
            raw = process.run([java, "-Xint", "-Xmx256m", "-cp", classes,
                               "SavedDataIdObservation", side, work, inputs[side]], directory, side, children)
            observations[side] = parse(raw, side, work)
            observations[side]["files"] = {path.relative_to(work).as_posix(): process.fingerprint(path)
                                           for path in sorted(work.rglob("*")) if path.is_file()}
        report["observations"] = observations
        report["classes_after"] = {path.name: process.fingerprint(path) for path in sorted(classes.glob("*.class"))}
        require(report["classes_before"] == report["classes_after"], "observer classes changed")
        require(len(children) == 3 and all(row.get("reaped") and row.get("exit_code") == 0 and
                row.get("stderr", {}).get("bytes") == 0 for row in children), "complete reaped children")
        report["status"] = "passed"
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            report["inputs_after"] = {name: process.fingerprint(path) for name, path in inputs.items()}
            require(report["inputs_after"] == before, "capture inputs changed")
        except BaseException as error:
            report["status"] = "failed"
            report["input_error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            (directory / "capture.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                                    encoding="utf-8", newline="\n")
    print("Captured both original SavedData ID owners, real handlers, diagnostics and complete file snapshots.")


if __name__ == "__main__":
    main()
