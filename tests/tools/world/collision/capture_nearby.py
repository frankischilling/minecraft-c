"""Capture genuine paired normal Chunk and concrete Blank gather behavior.

Compile only the adjacent reflection observer and invoke untouched pinned JARs
with the host JVM interpreter. No native build, game launch, World backend,
engine recompilation or Java exception emulation occurs. Output must be fresh;
all commands, streams and failed attempts remain available for inspection.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import random
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

def pin(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}

def write(path, obj):
    path.write_text(json.dumps(obj, sort_keys=True, indent=2) + "\n", encoding="utf-8")

def bits(number):
    return struct.pack(">d", float(number)).hex()

def box(values):
    return ",".join(v if isinstance(v, str) else bits(v) for v in values)

def corpus():
    result = []
    unit = box([0, 0, 0, 1, 1, 1])
    base = [("I", unit), ("F", unit), ("I", box([1, 0, 0, 2, 1, 1])),
            ("F", box([10, 0, 0, 11, 1, 1])), ("I", box([0.5, 0, 0, 1.5, 1, 1]))]
    def add(label, *, kind="N", excluded="-", destination="3", query=unit,
            entities=base, slices=None, expected=None, category="ordinary"):
        if slices is None:
            slices = ["0,1,0,2,4,1,3"] + ["_"] * 7
        source = slices if isinstance(slices, str) else "|".join(slices)
        entity_text = ";".join(f"{kind}:{coords}" for kind, coords in entities) or "_"
        row = {"id": f"n{len(result):05d}", "label": label, "kind": kind,
               "excluded": excluded, "destination": destination, "query": query,
               "entities": [{"kind": k, "bounds_bits": b.split(",")} for k, b in entities],
               "slices": slices, "category": category, "expected": expected}
        row["line"] = "\t".join([row["id"], kind, excluded, destination, query,
                                  entity_text, source])
        result.append(row)
    add("duplicates-source-order-preexisting", expected=["OK", 1, 6, "3,0,1,0,4,1", 1])
    add("identity-excluded-first-not-equal-box", excluded="0",
        expected=["OK", 1, 4, "3,1,4,1", 1])
    add("identity-excluded-second-not-equal-box", excluded="1",
        expected=["OK", 1, 4, "3,0,0,4", 1])
    add("empty-destination", destination="_", expected=["OK", 0, 5, "0,1,0,4,1", 1])
    add("no-clear-preexisting-destination", slices=["_"] * 8,
        expected=["OK", 1, 1, "3", 1])
    add("null-destination-no-matches", destination="-", slices=["2,3"] + ["_"] * 7,
        expected=["OK", -1, -1, "-", 1])
    add("null-destination-reached-match", destination="-",
        expected=["java.lang.NullPointerException", -1, -1, "-", 1])
    add("null-query-before-empty-slices", query="-", slices=["_"] * 8,
        expected=["java.lang.NullPointerException", 1, 1, "3", 1])
    add("null-query-before-null-destination", query="-", destination="-",
        expected=["java.lang.NullPointerException", -1, -1, "-", 1])
    for axis in range(3):
        for sign in (-1, 1):
            candidate = [0, 0, 0, 1, 1, 1]
            candidate[axis], candidate[axis + 3] = ((-1, 0) if sign < 0 else (1, 2))
            add(f"strict-touch-axis-{axis}-side-{sign}", destination="_",
                entities=[("I", box(candidate))], slices=["0"] + ["_"] * 7,
                expected=["OK", 0, 0, "_", 1])
            overlap = candidate.copy()
            overlap[axis + (3 if sign < 0 else 0)] = 1e-12 if sign < 0 else 1-1e-12
            add(f"strict-touch-small-overlap-axis-{axis}-side-{sign}", destination="_",
                entities=[("F", box(overlap))], slices=["0"] + ["_"] * 7,
                expected=["OK", 0, 1, "0", 1])
    wide = box([-1, -1000, -1, 2, 1000, 2])
    eight = [("I" if i % 2 == 0 else "F", wide) for i in range(8)]
    vertical = [str(i) for i in range(8)]
    for value in [-1000, -32, -18, -16, -2, 0, 2, 13.999999999999998, 14,
                  14.000000000000002, 15.999999999999998, 16, 18, 29.999999999999996,
                  30, 32, 110, 112, 114, 125.99999999999999, 126, 128, 130, 1000]:
        add(f"inclusive-expanded-slice-boundary-{value}",
            query=box([0, value, 0, 1, value+0.5, 1]), entities=eight,
            slices=vertical, destination="_")
    add("duplicates-across-slices", query=box([0, 0, 0, 1, 128, 1]), entities=eight,
        slices=["7,0,7", "1,0", "2", "3", "4", "5", "6", "0,7"], destination="_",
        expected=["OK", 0, 12, "7,0,7,1,0,2,3,4,5,6,0,7", 1])
    specials = ["0000000000000000", "8000000000000000", "0000000000000001",
                "8000000000000001", "0010000000000000", "8010000000000000",
                "7fefffffffffffff", "ffefffffffffffff", "7ff0000000000000",
                "fff0000000000000", "7ff8000000000042", "fff8000000000017",
                "7ff0000000000042", "fff0000000000017"]
    for index, special in enumerate(specials):
        for coordinate in range(6):
            query_bits = unit.split(",")
            query_bits[coordinate] = special
            add(f"query-special-{index}-coordinate-{coordinate}",
                query=box(query_bits), destination="_")
            candidate_bits = unit.split(",")
            candidate_bits[coordinate] = special
            add(f"candidate-special-{index}-coordinate-{coordinate}", destination="_",
                entities=[("I" if index % 2 == 0 else "F", box(candidate_bits))],
                slices=["0,0"] + ["_"] * 7)
    for label, bounds in [
        ("inverted-all", [3, 4, 5, -3, -4, -5]),
        ("collapsed", [0, 0, 0, 0, 0, 0]),
        ("all-nan", ["7ff8000000000042"] * 6),
        ("all-infinite", ["fff0000000000000"] * 3 + ["7ff0000000000000"] * 3),
        ("all-negative-zero", ["8000000000000000"] * 6)]:
        add(f"query-{label}", query=box(bounds), destination="_")
        add(f"candidate-{label}", entities=[("F", box(bounds))], destination="_",
            slices=["0,0"] + ["_"] * 7)
    generator = random.Random(0xB173C001)
    choices = list(map(bits, [-1000, -32, -2, -1, 0, 0.25, 0.5, 1, 2, 14, 16, 30, 128, 1000])) + specials
    for index in range(96):
        query = box([generator.choice(choices) for _ in range(6)])
        entities = [("I" if j % 2 == 0 else "F",
                     box([generator.choice(choices) for _ in range(6)])) for j in range(5)]
        slices = [",".join(str(generator.randrange(5)) for _ in range(generator.randrange(5)))
                  or "_" for _ in range(8)]
        add(f"seeded-raw-special-{index}", query=query, entities=entities, slices=slices,
            excluded="-" if index % 3 == 0 else str(index % 5), destination="3")
    # Supplementary malformed Java states do not authorize native adoption of
    # arbitrary objects into trusted ownership ledgers.
    for query in (unit, "-"):
        add(f"normal-null-m-query-{query == '-'}", query=query, slices="-",
            category="malformed-source",
            expected=["java.lang.NullPointerException", 1, 1, "3", 1])
    add("normal-zero-length-m", slices="_", expected=["OK", 1, 1, "3", 1],
        category="malformed-source")
    add("normal-null-first-slice", slices=["!"] + ["_"] * 7,
        expected=["java.lang.NullPointerException", 1, 1, "3", 1], category="malformed-source")
    add("null-candidate-equals-null-excluded-skipped", slices=["n,0,n,1"] + ["_"] * 7,
        expected=["OK", 1, 3, "3,0,1", 1], category="malformed-source")
    add("null-candidate-not-excluded-reached", excluded="3", slices=["0,n,1"] + ["_"] * 7,
        expected=["java.lang.NullPointerException", 1, 2, "3,0", 1], category="malformed-source")
    add("foreign-candidate-cast-after-successful-append", slices=["0,x,1"] + ["_"] * 7,
        expected=["java.lang.ClassCastException", 1, 2, "3,0", 1], category="malformed-source")
    add("later-null-slice-retains-earlier-append", query=box([0, 0, 0, 1, 16, 1]),
        slices=["0", "!"] + ["_"] * 6,
        expected=["java.lang.NullPointerException", 1, 2, "3,0", 1], category="malformed-source")
    for query in (unit, "-", box(["7ff8000000000042"] * 6)):
        for slices in (["0,1,0,2,4,1,3"] + ["_"] * 7, "-", "_", ["!"] * 8):
            for destination in ("3", "_", "-"):
                count = -1 if destination == "-" else 0 if destination == "_" else 1
                add(f"blank-literal-return-{len(result)}", kind="B", query=query,
                    slices=slices, destination=destination,
                    expected=["OK", count, count, destination, 1], category="blank-dispatch")
    return result

def execute(output, label, command, stdin=None):
    record = {"label": label, "command": command, "cwd": str(ROOT),
              "stdin": pin(stdin) if stdin else None, "start_ns": time.time_ns()}
    write(output / f"{label}.command.json", record)
    handle = stdin.open("rb") if stdin else subprocess.DEVNULL
    try:
        with (output / f"{label}.stdout").open("wb") as stdout, \
             (output / f"{label}.stderr").open("wb") as stderr:
            process = subprocess.Popen(command, cwd=ROOT, stdin=handle, stdout=stdout, stderr=stderr)
            record["pid"] = process.pid
            write(output / f"{label}.live.json", record)
            record["returncode"] = process.wait()
            record["reaped"] = True
            record["terminated"] = False
    except OSError as error:
        record["launch_error"] = str(error)
        record["returncode"] = None
    finally:
        if stdin:
            handle.close()
    record["end_ns"] = time.time_ns()
    record["stdout"] = pin(output / f"{label}.stdout")
    record["stderr"] = pin(output / f"{label}.stderr")
    write(output / f"{label}.result.json", record)
    print(label, record["returncode"], flush=True)
    return record

def main():
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT,
                        help="repository containing the pinned client/server JARs")
    parser.add_argument("--output", type=Path, required=True,
                        help="new directory for complete retained original observations")
    parser.add_argument("--java", default=shutil.which("java"), help="Java launcher")
    parser.add_argument("--javac", default=shutil.which("javac"), help="Java compiler")
    args = parser.parse_args()
    ROOT, output = args.repo.resolve(), args.output.resolve()
    if output.exists():
        parser.error("output must be new; existing evidence is never overwritten")
    if not ROOT.is_dir() or not (HERE / "NearbyObservation.java").is_file():
        parser.error("repository and adjacent reflection helper must exist")
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "FAIL", "scope": "original normal Chunk and concrete Blank gather only",
              "limitations": ["not native parity", "not a World or provider/backend observation",
                 "reflection configures original slice ArrayLists and bounds doubles",
                 "no gameplay tick, publication, movement, shape or application lifecycle proof",
                 "host JVM interpreter; no original JVM resource-failure equivalence"],
              "producer_before": pin(__file__), "helper_before": pin(HERE / "NearbyObservation.java"),
              "processes": [], "errors": [], "sides": {}}
    try:
        java_path = shutil.which(args.java) if args.java else None
        javac_path = shutil.which(args.javac) if args.javac else None
        if java_path is None or javac_path is None:
            raise RuntimeError("java and javac executables are required")
        java, javac = Path(java_path).resolve(), Path(javac_path).resolve()
        report["java_before"], report["javac_before"] = pin(java), pin(javac)
        report["original_jars_before"] = {}
        for side, (filename, length, digest) in JARS.items():
            actual = pin(ROOT / filename)
            if actual["bytes"] != length or actual["sha256"] != digest:
                raise RuntimeError(f"{side} original JAR pin mismatch")
            report["original_jars_before"][side] = actual
        report["host"] = {"python": sys.version, "executable": sys.executable}
        report["processes"].append(execute(output, "java-version", [str(java), "-version"]))
        report["processes"].append(execute(output, "javac-version", [str(javac), "-version"]))
        classes = output / "classes"
        classes.mkdir()
        report["processes"].append(execute(output, "compile-helper", [str(javac), "-encoding", "UTF-8",
            "-d", str(classes), str(HERE / "NearbyObservation.java")]))
        if report["processes"][-1]["returncode"] != 0:
            raise RuntimeError("helper compilation failed; failed attempt retained")
        report["class_before"] = pin(classes / "NearbyObservation.class")
        rows = corpus()
        input_path = output / "input.tsv"
        input_path.write_bytes(("\n".join(row["line"] for row in rows) + "\n").encode("ascii"))
        report["input_before"] = pin(input_path)
        write(output / "cases.json", rows)
        report["cases"] = pin(output / "cases.json")
        report["categories"] = {c: sum(row["category"] == c for row in rows)
                                for c in sorted({r["category"] for r in rows})}
        for side, (filename, _, _) in JARS.items():
            process = execute(output, side.lower() + "-original", [str(java), "-Xint",
                "-Dfile.encoding=US-ASCII", "-cp", str(classes), "NearbyObservation",
                str(ROOT / filename), side], input_path)
            report["processes"].append(process)
            lines = (output / (side.lower() + "-original.stdout")).read_text("ascii").splitlines()
            errors, results = [], []
            for index, case in enumerate(rows):
                line = lines[index] if index < len(lines) else None
                fields = line.split("\t") if line else []
                valid = len(fields) == 7 and fields[:2] == [case["id"], case["kind"]]
                if valid:
                    try:
                        actual = [fields[2], int(fields[3]), int(fields[4]), fields[5], int(fields[6])]
                        valid = actual[-1] == 1 and actual[0] in (
                            "OK", "java.lang.NullPointerException", "java.lang.ClassCastException")
                        valid = valid and (case["expected"] is None or actual == case["expected"])
                    except ValueError:
                        valid = False
                if not valid:
                    errors.append(case["id"])
                results.append({"id": case["id"], "label": case["label"], "line": line,
                                "explicit_expected": case["expected"], "valid": valid})
            write(output / (side.lower() + ".results.json"), results)
            setup = (output / (side.lower() + "-original.stderr")).read_text("ascii")
            report["sides"][side] = {"rows": len(lines), "expected_rows": len(rows),
                "invalid_case_ids": errors, "results": pin(output / (side.lower() + ".results.json")),
                "actual_constructor_setup_confirmed": setup.startswith("SETUP\t") and "no-Unsafe" in setup,
                "status": "PASS" if not errors and len(lines) == len(rows) and process["returncode"] == 0
                    and setup.startswith("SETUP\t") and "no-Unsafe" in setup else "FAIL"}
        client = (output / "client-original.stdout").read_bytes()
        server = (output / "server-original.stdout").read_bytes()
        report["paired_rows_identical"] = client == server and bool(client)
        for key, path in [("producer", Path(__file__)), ("helper", HERE / "NearbyObservation.java"),
                          ("java", java), ("javac", javac),
                          ("class", classes / "NearbyObservation.class"), ("input", input_path)]:
            report[key + "_after"] = pin(path)
            report[key + "_stable"] = report[key + "_before"] == report[key + "_after"]
        report["original_jars_after"] = {s: pin(ROOT / row[0]) for s, row in JARS.items()}
        report["original_jars_stable"] = report["original_jars_before"] == report["original_jars_after"]
        if all(s["status"] == "PASS" for s in report["sides"].values()) and \
                report["paired_rows_identical"] and report["original_jars_stable"] and \
                all(report[k + "_stable"] for k in ("producer", "helper", "java", "javac", "class", "input")):
            report["status"] = "PASS"
    except Exception as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write(output / "report.json", report)
    print(json.dumps({"status": report["status"], "output": str(output),
                      "sides": report["sides"], "errors": report["errors"]}), flush=True)
    return 0 if report["status"] == "PASS" else 1

if __name__ == "__main__":
    raise SystemExit(main())
