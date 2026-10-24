"""Capture genuine registered Block shape methods without a substitute World.

Only the reflection helper is compiled. Getter/contribution calls receive null
World; source-unused World successes are useful bounded observations, while
reached World-dependent failures remain explicit exclusions from nonnull-World
parity. Direct moving-piston geometry-helper calls are separately labeled and
do not construct or qualify a positive piston Tile.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
JARS = {
    "CLIENT": ("b1.7.3.jar", 1465375,
               "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"),
    "SERVER": ("b1.7.3_server.jar", 503100,
               "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"),
}
NULL_GETTERS = {
    6, 8, 9, 10, 11, 27, 28, 30, 31, 32, 37, 38, 39, 40, 50, 51, 55, 59,
    63, 66, 68, 69, 70, 72, 75, 76, 77, 83, 90,
}
WORLD_GETTERS = {36, 64, 65, 71, 78, 92, 96}
WORLD_CONTRIBUTIONS = WORLD_GETTERS | {34, 53, 67}


def pin(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def write(path, value):
    path.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def bits(value):
    return struct.pack(">d", float(value)).hex()


def single(value):
    return struct.pack(">f", float(value)).hex()


def box(values):
    return ",".join(v if isinstance(v, str) else bits(v) for v in values)


def corpus():
    rows = []
    unit = box([0, 0, 0, 1, 1, 1])
    any_box = box(["7ff8000000000042"] * 6)
    custom = ",".join(single(v) for v in (0.125, 0.25, 0.5, 0.75, 0.875, 1.25))
    inverted = ",".join(single(v) for v in (3, 4, 5, -3, -4, -5))

    def add(label, block_id, operation="get", *, xyz=(0, 0, 0), bounds="ctor",
            query=unit, destination="marker", pool="reset", repeats=1,
            stored=0, progress="00000000", direction=0, category="registered-null-world"):
        row = {"id": f"s{len(rows):05d}", "label": label, "operation": operation,
               "block_id": block_id, "coordinates": list(xyz), "bounds": bounds,
               "query": query, "destination": destination, "pool_mode": pool,
               "repeats": repeats, "stored_id": stored, "progress_bits": progress,
               "direction": direction, "category": category}
        row["line"] = "\t".join(map(str, [row["id"], operation, block_id, *xyz, bounds,
            query, destination, pool, repeats, stored, progress, direction]))
        rows.append(row)

    for block_id in range(1, 97):
        add("constructor-getter-origin", block_id)
    coordinate_sets = [
        (1, 127, -1), (16777217, 16777217, 16777217),
        (-16777217, -16777217, -16777217), (2147483647,)*3,
        (-2147483648,)*3, (2147483647, -2147483648, -1),
    ]
    for xyz in coordinate_sets:
        for block_id in range(1, 97):
            add("getter-coordinate-edge", block_id, xyz=xyz)
    for query_name, query, destination in [
        ("unit", unit, "marker"),
        ("far", box([10, 10, 10, 11, 11, 11]), "marker"),
        ("touch-x", box([1, 0, 0, 2, 1, 1]), "marker"),
        ("all-nan", any_box, "marker"),
        ("null-query", "-", "marker"),
        ("null-destination", unit, "null"),
    ]:
        for block_id in range(1, 97):
            add("contribution-" + query_name, block_id, "contribute",
                query=query, destination=destination)
    for name, bounds in [("custom", custom), ("inverted", inverted)]:
        for block_id in range(1, 97):
            add("setter-" + name + "-getter", block_id, bounds=bounds)
            add("setter-" + name + "-contribution", block_id, "contribute",
                bounds=bounds, query=any_box)
    for block_id in range(1, 97):
        add("held-pool-slot-alias-getter", block_id, pool="alias")
        add("reset-between-two-getters", block_id, pool="between", repeats=2)
    selected = [1, 8, 29, 33, 34, 36, 53, 60, 64, 65, 67, 78, 81, 85, 88, 92, 96]
    for block_id in selected:
        add("used-pool-two-getters", block_id, pool="used", repeats=2)
        add("same-slot-repeated-contribution", block_id, "contribute", pool="between",
            repeats=2, query=any_box)
    float_edges = [
        "80000000,00000000,80000000,3f800000,3f800000,3f800000",
        "7fc00042,3e800000,3f000000,3f800000,7fc00017,3f800000",
        "7f800042,ff800017,7f800000,ff800000,00000001,80000001",
    ]
    for block_id in selected:
        for bounds in float_edges:
            add("float-setter-raw-edge", block_id, bounds=bounds, pool="alias")
            add("float-setter-raw-edge-contribution", block_id, "contribute",
                bounds=bounds, query=any_box, pool="alias")
    stored_ids = [0, 36, 1, 6, 8, 34, 53, 60, 65, 78, 81, 85, 88, 92, 96, 97, -1, 256]
    for stored in stored_ids:
        add("moving-direct-stored-id", 36, "moving", stored=stored,
            progress=single(0.25), category="moving-geometry-helper")
    progress_values = [single(v) for v in (0, -0.0, 0.25, 1, 1.25)]
    progress_values += ["7f800000", "ff800000", "7fc00042", "7f800042"]
    for progress in progress_values:
        for direction in (-1, 0, 1, 2, 3, 4, 5, 6):
            add("moving-direct-progress-direction", 36, "moving", stored=1,
                progress=progress, direction=direction, category="moving-geometry-helper")
    for xyz in coordinate_sets[1:]:
        for stored in (1, 60, 81, 85, 88):
            for direction in range(6):
                add("moving-direct-coordinate-edge", 36, "moving", stored=stored,
                    xyz=xyz, progress=single(0.25), direction=direction,
                    category="moving-geometry-helper")
    return rows


def execute(output, label, command, cwd, input_path=None):
    record = {"label": label, "command": command, "cwd": str(cwd),
              "stdin": pin(input_path) if input_path else None, "start_ns": time.time_ns()}
    write(output / (label + ".command.json"), record)
    handle = input_path.open("rb") if input_path else subprocess.DEVNULL
    try:
        with (output / (label + ".stdout")).open("wb") as stdout, \
                (output / (label + ".stderr")).open("wb") as stderr:
            process = subprocess.Popen(command, cwd=cwd, stdin=handle, stdout=stdout, stderr=stderr)
            record["pid"] = process.pid
            write(output / (label + ".live.json"), record)
            record["returncode"] = process.wait()
            record["reaped"], record["terminated"] = True, False
    except OSError as error:
        record["launch_error"], record["returncode"] = str(error), None
    finally:
        if input_path:
            handle.close()
    record["end_ns"] = time.time_ns()
    record["stdout"], record["stderr"] = pin(output / (label + ".stdout")), pin(output / (label + ".stderr"))
    write(output / (label + ".result.json"), record)
    print(label, record["returncode"], flush=True)
    return record


def classify(case, fields):
    if fields[3] != "OK":
        if case["operation"] == "get" and case["block_id"] in WORLD_GETTERS:
            return "reached-null-World-getter; exclude from nonnull-World parity"
        if case["operation"] == "contribute" and case["block_id"] in WORLD_CONTRIBUTIONS:
            return "reached-null-World-contribution; exclude from nonnull-World parity"
        if case["operation"] == "contribute" and case["query"] == "-":
            return "reached-null-query after actual pooled shape"
        if case["operation"] == "contribute" and case["destination"] == "null":
            return "reached-null-destination after actual intersection"
        return "direct-moving-helper reached failure; no positive Tile claim"
    if case["operation"] == "moving":
        return "direct-moving-helper only; no positive Tile/World claim"
    return "original source-unused World route; native pool binding may require actual World"


def valid_values(fields):
    def coordinate(token):
        return token == "-" or re.fullmatch(r"[0-9a-f]{16}(?:,[0-9a-f]{16}){5}", token) is not None
    def identity(token, size):
        return token == "-" or re.fullmatch(r"p[0-9]+", token) is not None and int(token[1:]) < size
    try:
        cursor_before, cursor_after, size_before, size_after = map(int, fields[4:8])
        if not (0 <= cursor_before <= size_before <= size_after and 0 <= cursor_after <= size_after):
            return False
        if not all(coordinate(fields[i]) for i in (9,12,13,15,18)) or fields[12] == "-" or fields[13] == "-":
            return False
        if not all(identity(fields[i], size_after) for i in (8,14,17)):
            return False
        pool = [] if fields[16] == "_" else fields[16].split(";")
        if len(pool) != size_after:
            return False
        for index, value in enumerate(pool):
            name, coords = value.split(":", 1)
            if name != "p" + str(index) or coords == "-" or not coordinate(coords):
                return False
        count = int(fields[10])
        if count < -1 or count == -1 and fields[11] != "-" or count == 0 and fields[11] != "_":
            return False
        listed = fields[11].split(";") if count > 0 else []
        if len(listed) != max(0,count):
            return False
        for value in listed:
            name, coords = value.split(":", 1)
            if not (name == "m" or name != "-" and identity(name, size_after)) or \
                    coords == "-" or not coordinate(coords):
                return False
        return True
    except (ValueError, IndexError):
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True, help="fresh retained capture directory")
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    args = parser.parse_args()
    repo, output = args.repo.resolve(), args.output.resolve()
    if output.exists():
        parser.error("output must be new; evidence is never overwritten")
    output.mkdir(parents=True, exist_ok=False)
    helper = HERE / "BlockShapeObservation.java"
    report = {"status": "FAIL", "scope": "original registered getter/contribution null-World routes and direct moving helper",
              "limitations": ["not nonnull-World metadata/Tile parity", "no World or fake backend constructed",
                  "no actual positive piston Tile", "not native parity or whole-game acceptance",
                  "actual constructor bounds are restored through original float setter at explicit case boundaries",
                  "selected tools/helper identities do not pin the complete host JVM runtime"],
              "producer_before": pin(__file__), "helper_before": pin(helper),
              "host": {"os": os.name, "python": sys.version}, "processes": [], "sides": {}, "errors": []}
    try:
        java_path = shutil.which(args.java) if args.java else None
        javac_path = shutil.which(args.javac) if args.javac else None
        if java_path is None or javac_path is None:
            raise RuntimeError("java and javac required")
        java, javac = Path(java_path).resolve(), Path(javac_path).resolve()
        report["java_before"], report["javac_before"] = pin(java), pin(javac)
        report["jars_before"] = {}
        for side, (filename, length, digest) in JARS.items():
            actual = pin(repo / filename)
            if actual["bytes"] != length or actual["sha256"] != digest:
                raise RuntimeError("original JAR identity mismatch")
            report["jars_before"][side] = actual
        for label, command in [("java-version", [str(java), "-version"]), ("javac-version", [str(javac), "-version"])]:
            report["processes"].append(execute(output, label, command, repo))
        classes = output / "classes"
        classes.mkdir()
        process = execute(output, "compile-helper", [str(javac), "-encoding", "UTF-8", "-d", str(classes), str(helper)], repo)
        report["processes"].append(process)
        if process["returncode"] != 0:
            raise RuntimeError("reflection helper compilation failed")
        report["class_before"] = pin(classes / "BlockShapeObservation.class")
        cases = corpus()
        write(output / "cases.json", cases)
        input_path = output / "input.tsv"
        input_path.write_bytes(("\n".join(case["line"] for case in cases) + "\n").encode("ascii"))
        report["cases"], report["input_before"] = pin(output / "cases.json"), pin(input_path)
        report["expected_rows_per_side"] = len(cases)
        for side, (filename, _, _) in JARS.items():
            inventory = output / (side.lower() + ".inventory.tsv")
            process = execute(output, side.lower() + "-original", [str(java), "-Xint",
                "-Dfile.encoding=US-ASCII", "-cp", str(classes), "BlockShapeObservation",
                str(repo / filename), side, str(inventory)], repo, input_path)
            report["processes"].append(process)
            raw = (output / (side.lower() + "-original.stdout")).read_bytes().splitlines(keepends=True)
            header = ("BLOCK_SHAPES_V1\t" + side).encode("ascii")
            locations = [i for i,line in enumerate(raw) if line.rstrip(b"\r\n") == header]
            if locations != [2] or [line.rstrip(b"\r\n") for line in raw[:2]] != [b"16 achievements", b"151 recipes"]:
                raise RuntimeError("genuine complete Block bootstrap/header not reached")
            (output / (side.lower() + ".bootstrap.stdout")).write_bytes(b"".join(raw[:2]))
            rows_path = output / (side.lower() + ".rows.stdout")
            rows_path.write_bytes(b"".join(raw[3:]))
            lines = rows_path.read_text("ascii").splitlines()
            results, invalid, counts = [], [], {}
            for index, case in enumerate(cases):
                line = lines[index] if index < len(lines) else None
                fields = line.split("\t") if line else []
                valid = len(fields) == 19 and fields[:3] == [case["id"], case["operation"], str(case["block_id"])]
                if valid:
                    valid = fields[3] in ("OK", "java.lang.NullPointerException", "java.lang.ArrayIndexOutOfBoundsException") and \
                        valid_values(fields)
                    try:
                        cursor_before, cursor_after, size_before, size_after = map(int, fields[4:8])
                        valid = valid and 0 <= cursor_before <= size_before <= size_after and 0 <= cursor_after <= size_after
                        if case["label"] == "constructor-getter-origin":
                            if case["block_id"] in WORLD_GETTERS:
                                valid = valid and fields[3] == "java.lang.NullPointerException" and cursor_after == 0
                            elif case["block_id"] in NULL_GETTERS:
                                valid = valid and fields[3] == "OK" and fields[8] == "-" and cursor_after == 0
                            else:
                                valid = valid and fields[3] == "OK" and fields[8] == "p0" and cursor_after == 1
                    except ValueError:
                        valid = False
                if not valid:
                    invalid.append(case["id"])
                category = classify(case, fields) if len(fields) == 19 else "missing/invalid row"
                counts[category] = counts.get(category, 0) + 1
                results.append({"id": case["id"], "label": case["label"], "line": line,
                                "classification": category, "valid": valid})
            write(output / (side.lower() + ".results.json"), results)
            inv = inventory.read_text("ascii").splitlines() if inventory.exists() else []
            inventory_valid = len(inv) == 97 and inv[0] == "BLOCK_SHAPE_REGISTRY_V1" and \
                [line.split("\t")[0] for line in inv[1:]] == [str(i) for i in range(1,97)]
            diagnostics = (output / (side.lower() + "-original.stderr")).read_text("ascii").splitlines()
            failed_rows = [r for r in results if r["line"] and r["line"].split("\t")[3] != "OK"]
            diagnostic_valid = len(diagnostics) == len(failed_rows)
            for detail,result in zip(diagnostics, failed_rows):
                parts = detail.split("\t")
                try:
                    trace = bytes.fromhex(parts[2]).decode("utf-8")
                    diagnostic_valid = diagnostic_valid and parts[:2] == ["EXCEPTION",result["id"]] and \
                        trace.startswith(result["line"].split("\t")[3]) and "\tat " in trace
                except (ValueError,IndexError,UnicodeDecodeError):
                    diagnostic_valid = False
            report["sides"][side] = {"status": "PASS" if not invalid and len(lines) == len(cases)
                and process["returncode"] == 0 and inventory_valid and diagnostic_valid else "FAIL",
                "rows": len(lines), "invalid_case_ids": invalid, "classifications": counts,
                "exception_rows": len(failed_rows), "complete_exception_traces": diagnostic_valid,
                "raw_rows": pin(rows_path), "results": pin(output / (side.lower() + ".results.json")),
                "bootstrap": pin(output / (side.lower() + ".bootstrap.stdout")),
                "inventory": pin(inventory) if inventory.exists() else None}
        report["paired_shape_rows_byte_identical"] = (output / "client.rows.stdout").read_bytes() == \
            (output / "server.rows.stdout").read_bytes()
        for key,path in [("producer", Path(__file__)), ("helper", helper), ("java", java),
                         ("javac", javac), ("class", classes / "BlockShapeObservation.class"),
                         ("input", input_path)]:
            report[key + "_after"] = pin(path)
            report[key + "_stable"] = report[key + "_before"] == report[key + "_after"]
        report["jars_after"] = {side: pin(repo / row[0]) for side,row in JARS.items()}
        report["jars_stable"] = report["jars_before"] == report["jars_after"]
        if all(side["status"] == "PASS" for side in report["sides"].values()) and \
                report["paired_shape_rows_byte_identical"] and report["jars_stable"] and \
                all(report[key + "_stable"] for key in ("producer","helper","java","javac","class","input")):
            report["status"] = "PASS"
    except Exception as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write(output / "report.json", report)
    print(json.dumps({"status": report["status"], "sides": report["sides"], "errors": report["errors"],
                      "output": str(output)}), flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
