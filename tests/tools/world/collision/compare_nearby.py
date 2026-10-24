"""Fresh paired-original/native replay of actual normal/Blank Chunk gather.

This compiles only the adjacent original reflection observer, then runs a
separately built native values probe. No native compilation, World callback
backend, source regeneration or engine launch occurs. Use a fresh output path;
all commands, complete streams and failures remain available for inspection.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

import capture_nearby as capture

HERE = Path(__file__).resolve().parent
SOURCES = (
    "tests/world/runtime/collision/nearby_probe.c",
    "tests/world/runtime/test_entity_publication.c",
    "src/world/runtime/collision/entities.c",
    "src/world/runtime/collision/entities_internal.h",
    "include/mc/world/runtime/collision/entities.h",
    "src/world/chunk/entities.c",
    "src/world/chunk/object_internal.h",
    "src/world/chunk/object_state_internal.h",
    "src/entity/entity_internal.h",
    "include/mc/entity/entity.h",
    "include/mc/entity/bounds/bounds.h",
    "src/world/runtime/runtime_internal.h",
    "tests/tools/world/collision/NearbyObservation.java",
    "tests/tools/world/collision/capture_nearby.py",
    "tests/tools/world/collision/compare_nearby.py",
    "CMakeLists.txt",
)
SETUP = (
    "SETUP\tactual-normal-and-blank-null-World-constructors\t"
    "actual-Item-and-FallingSand-null-World-constructors\t"
    "distinct-original-constructor-owned-bounds\tno-Unsafe"
)
pin, write = capture.pin, capture.write


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
            record["reaped"] = True
            record["terminated"] = False
    except OSError as error:
        record["launch_error"] = str(error)
        record["returncode"] = None
    finally:
        if input_path:
            handle.close()
    record["end_ns"] = time.time_ns()
    record["stdout"] = pin(output / (label + ".stdout"))
    record["stderr"] = pin(output / (label + ".stderr"))
    write(output / (label + ".result.json"), record)
    print(label, record["returncode"], flush=True)
    return record


def verify_capture(original, repo, retained):
    """Rejoin the fresh receipt against actual tools, full inputs and streams."""
    def verify(path, expected=None):
        actual = pin(path)
        if expected is not None and actual != expected:
            raise RuntimeError(f"fresh original pin mismatch: {path}")
        retained.append(actual)
        return actual

    receipt_path = original / "report.json"
    receipt = json.loads(receipt_path.read_text("utf-8"))
    receipt_pin = verify(receipt_path)
    if receipt["status"] != "PASS" or not receipt["paired_rows_identical"]:
        raise RuntimeError("fresh original capture did not pass")
    for key in ("producer", "helper", "java", "javac", "class", "input"):
        if not receipt[key + "_stable"] or receipt[key + "_before"] != receipt[key + "_after"]:
            raise RuntimeError("fresh original input was not stable")
        verify(receipt[key + "_before"]["path"], receipt[key + "_before"])
    if receipt["producer_before"] != pin(HERE / "capture_nearby.py") or \
            receipt["helper_before"] != pin(HERE / "NearbyObservation.java"):
        raise RuntimeError("fresh original producer/helper identity mismatch")
    if receipt["class_before"] != pin(original / "classes/NearbyObservation.class"):
        raise RuntimeError("fresh original compiled helper identity mismatch")
    if sorted(path.name for path in (original / "classes").iterdir()) != ["NearbyObservation.class"]:
        raise RuntimeError("unexpected compiled engine/helper classes")
    if not receipt["original_jars_stable"] or \
            receipt["original_jars_before"] != receipt["original_jars_after"]:
        raise RuntimeError("original JAR stability mismatch")
    for side, (filename, length, digest) in capture.JARS.items():
        actual = verify(repo / filename, receipt["original_jars_before"][side])
        if actual["bytes"] != length or actual["sha256"] != digest:
            raise RuntimeError("original JAR identity mismatch")

    rows = capture.corpus()
    if rows != json.loads((original / "cases.json").read_text("utf-8")):
        raise RuntimeError("complete original corpus regeneration mismatch")
    verify(original / "cases.json", receipt["cases"])
    input_path = original / "input.tsv"
    if input_path.read_bytes() != ("\n".join(r["line"] for r in rows) + "\n").encode("ascii"):
        raise RuntimeError("complete original input join mismatch")
    verify(input_path, receipt["input_before"])
    java, javac = receipt["java_before"]["path"], receipt["javac_before"]["path"]
    commands = {
        "java-version": [java, "-version"],
        "javac-version": [javac, "-version"],
        "compile-helper": [javac, "-encoding", "UTF-8", "-d", str(original / "classes"),
                           str(HERE / "NearbyObservation.java")],
    }
    for side, (filename, _, _) in capture.JARS.items():
        commands[side.lower() + "-original"] = [
            java, "-Xint", "-Dfile.encoding=US-ASCII", "-cp", str(original / "classes"),
            "NearbyObservation", str(repo / filename), side,
        ]
    if [p["label"] for p in receipt["processes"]] != list(commands):
        raise RuntimeError("fresh original process inventory mismatch")
    for process in receipt["processes"]:
        label = process["label"]
        if process["command"] != commands[label] or process["cwd"] != str(repo) or \
                process["returncode"] != 0 or not process["reaped"] or process["terminated"] or \
                process["end_ns"] < process["start_ns"]:
            raise RuntimeError("fresh original command/outcome mismatch")
        expected_input = receipt["input_before"] if label.endswith("-original") else None
        if process["stdin"] != expected_input:
            raise RuntimeError("fresh original process stdin mismatch")
        for suffix in ("command", "live", "result"):
            path = original / (label + "." + suffix + ".json")
            persisted = json.loads(path.read_text("utf-8"))
            if any(process.get(k) != v for k, v in persisted.items()):
                raise RuntimeError("fresh original process record mismatch")
            verify(path)
        if json.loads((original / (label + ".result.json")).read_text("utf-8")) != process:
            raise RuntimeError("fresh original result receipt mismatch")
        for stream in ("stdout", "stderr"):
            verify(original / (label + "." + stream), process[stream])

    by_side = {}
    for side in capture.JARS:
        process = next(p for p in receipt["processes"] if p["label"] == side.lower() + "-original")
        lines = Path(process["stdout"]["path"]).read_text("ascii").splitlines()
        setup = Path(process["stderr"]["path"]).read_text("ascii").splitlines()
        if setup != [SETUP] or len(lines) != len(rows) or len(rows) != 364:
            raise RuntimeError("fresh original constructor receipt/row count mismatch")
        result_path = original / (side.lower() + ".results.json")
        verify(result_path, receipt["sides"][side]["results"])
        recorded = json.loads(result_path.read_text("utf-8"))
        if len(recorded) != len(rows):
            raise RuntimeError("fresh original retained result count mismatch")
        by_id = {}
        for row, line, result in zip(rows, lines, recorded):
            fields = line.split("\t")
            if len(fields) != 7 or fields[:2] != [row["id"], row["kind"]]:
                raise RuntimeError("fresh original result identity mismatch")
            actual = [fields[2], int(fields[3]), int(fields[4]), fields[5], int(fields[6])]
            identities = [] if fields[5] in ("_", "-") else fields[5].split(",")
            if actual[4] != 1 or len(identities) != max(0, actual[2]) or \
                    actual[0] not in ("OK", "java.lang.NullPointerException", "java.lang.ClassCastException"):
                raise RuntimeError("fresh original result shape mismatch")
            if row["expected"] is not None and actual != row["expected"]:
                raise RuntimeError("fresh original explicit expectation mismatch")
            if result != {"id": row["id"], "label": row["label"], "line": line,
                          "explicit_expected": row["expected"], "valid": True}:
                raise RuntimeError("fresh original row/retained result join mismatch")
            by_id[row["id"]] = fields
        by_side[side] = by_id
    if (original / "client-original.stdout").read_bytes() != \
            (original / "server-original.stdout").read_bytes():
        raise RuntimeError("fresh paired original raw output bytes differ")
    return receipt_pin, rows, by_side


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True, type=Path,
                        help="separately built mc_world_nearby_values_probe executable")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[4],
                        help="repository root containing original JARs and native sources")
    parser.add_argument("--output", required=True, type=Path,
                        help="new directory for fresh original capture and native replay")
    parser.add_argument("--java", help="Java launcher (default: java on PATH)")
    parser.add_argument("--javac", help="Java compiler (default: javac on PATH)")
    args = parser.parse_args()
    repo, native, output = (p.resolve() for p in (args.repo, args.native, args.output))
    if output.exists():
        parser.error("output must be new; existing evidence is never overwritten")
    if not repo.is_dir() or not native.is_file():
        parser.error("repository and native executable must exist")
    output.mkdir(parents=True, exist_ok=False)
    report = {
        "status": "FAIL", "scope": "fresh paired-original normal/Blank Chunk gather replay",
        "limitations": [
            "not World nearby/provider traversal parity or whole-game acceptance",
            "native allocation errors are not Java Throwable/OOM equivalence",
            "generic malformed Java collection states are explicitly excluded",
            "native Blank fallback deliberately has no normal Chunk header",
            "executable-to-source build provenance requires separate native build qualification",
            "selected launcher/helper identities do not pin the entire host runtime",
        ],
        "host": {"os": os.name, "python": sys.version},
        "producer_before": pin(__file__), "native_before": pin(native),
        "python_before": pin(sys.executable), "processes": [], "sides": {}, "errors": [],
    }
    retained = []
    try:
        report["source_inputs_before"] = [pin(repo / path) for path in SOURCES]
        original = output / "original"
        command = [sys.executable, str(HERE / "capture_nearby.py"), "--repo", str(repo),
                   "--output", str(original)]
        for option in ("java", "javac"):
            if getattr(args, option):
                command.extend(["--" + option, getattr(args, option)])
        process = execute(output, "fresh-original-capture", command, repo)
        report["processes"].append(process)
        if process["returncode"] != 0:
            raise RuntimeError("fresh original capture failed; complete attempt retained")
        receipt_pin, rows, references = verify_capture(original, repo, retained)
        report["original_capture"] = receipt_pin
        selected = [r for r in rows if r["category"] in ("ordinary", "blank-dispatch")]
        excluded = [
            {"id": r["id"], "label": r["label"], "category": r["category"],
             "reason": "generic malformed Java array/List contents are outside actual native typed ownership"}
            for r in rows if r["category"] not in ("ordinary", "blank-dispatch")
        ]
        if len(selected) != 356 or len(excluded) != 8:
            raise RuntimeError("unexpected supported/excluded original corpus count")
        write(output / "selected.json", selected)
        write(output / "excluded.json", excluded)
        report["selected"], report["excluded"] = pin(output / "selected.json"), pin(output / "excluded.json")
        report["selected_rows_per_side"], report["excluded_rows_per_side"] = len(selected), len(excluded)
        input_path = output / "input.tsv"
        input_path.write_bytes(("\n".join(r["line"] for r in selected) + "\n").encode("ascii"))
        report["input_before"] = pin(input_path)
        for side in capture.JARS:
            process = execute(output, side.lower() + "-native", [
                str(native), "nearby-gather", side,
                str(output / (side.lower() + "-world")).encode("utf-8").hex(),
            ], repo, input_path)
            report["processes"].append(process)
            raw_path = output / (side.lower() + "-native.stdout")
            raw_lines = raw_path.read_bytes().splitlines(keepends=True)
            console = [b"16 achievements", b"151 recipes"] * 2
            if [line.rstrip(b"\r\n") for line in raw_lines[:4]] != console:
                raise RuntimeError("actual two-owner Registry bootstrap console mismatch")
            bootstrap_path = output / (side.lower() + ".bootstrap.stdout")
            rows_path = output / (side.lower() + ".rows.stdout")
            bootstrap_path.write_bytes(b"".join(raw_lines[:4]))
            rows_path.write_bytes(b"".join(raw_lines[4:]))
            report.setdefault("bootstrap_console", {})[side] = {
                "exact_lines": [line.decode("ascii") for line in console],
                "raw_capture": pin(bootstrap_path),
            }
            actual_lines = rows_path.read_text("ascii").splitlines()
            results, mismatches = [], []
            exact, reached = 0, 0
            for index, row in enumerate(selected):
                expected = references[side][row["id"]]
                line = actual_lines[index] if index < len(actual_lines) else None
                fields = line.split("\t") if line else []
                valid, mode = False, "mismatch"
                if len(fields) == 9:
                    try:
                        status, pc = int(fields[7]), int(fields[8])
                        if expected[2] == "OK":
                            valid = fields[:7] == expected and status == 0
                            mode = "exact-success" if valid else mode
                            exact += int(valid)
                        elif expected[2] == "java.lang.NullPointerException":
                            expected_pc = 1 if row["query"] == "-" else \
                                131 if row["destination"] == "-" else None
                            valid = row["kind"] == "N" and fields[:2] == expected[:2] and \
                                fields[2] == "FAIL" and fields[3:7] == expected[3:7] and \
                                status == 5 and pc == expected_pc
                            mode = "reached-native-null-failure-and-prefix" if valid else mode
                            reached += int(valid)
                    except ValueError:
                        valid = False
                if not valid:
                    mismatches.append(row["id"])
                results.append({
                    "id": row["id"], "label": row["label"], "category": row["category"],
                    "original": "\t".join(expected), "native": line, "comparison": mode, "match": valid,
                })
            write(output / (side.lower() + ".results.json"), results)
            stderr = (output / (side.lower() + "-native.stderr")).read_text("ascii")
            complete = re.fullmatch(
                r"PASS: [1-9][0-9]* nearby gather replay checks; 356 rows\n", stderr
            ) is not None
            report["sides"][side] = {
                "status": "PASS" if not mismatches and len(actual_lines) == len(selected)
                    and process["returncode"] == 0 and complete and exact == 353 and reached == 3 else "FAIL",
                "expected_rows": len(selected), "actual_rows": len(actual_lines),
                "exact_success_rows": exact, "precise_reached_nullable_failures": reached,
                "mismatch_ids": mismatches, "cleanup_completion_confirmed": complete,
                "raw_rows": pin(rows_path), "results": pin(output / (side.lower() + ".results.json")),
            }
        report["producer_after"], report["native_after"] = pin(__file__), pin(native)
        report["python_after"] = pin(sys.executable)
        report["source_inputs_after"] = [pin(repo / path) for path in SOURCES]
        report["input_after"] = pin(input_path)
        report["retained_original_inputs"] = retained
        for item in retained:
            if pin(item["path"]) != item:
                raise RuntimeError("fresh original evidence changed during native replay")
        report["stable"] = report["producer_before"] == report["producer_after"] and \
            report["native_before"] == report["native_after"] and \
            report["python_before"] == report["python_after"] and \
            report["source_inputs_before"] == report["source_inputs_after"] and \
            report["input_before"] == report["input_after"]
        if report["stable"] and all(r["status"] == "PASS" for r in report["sides"].values()):
            report["status"] = "PASS"
    except Exception as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write(output / "report.json", report)
    print(json.dumps({"status": report["status"], "sides": report["sides"],
                      "errors": report["errors"], "output": str(output)}), flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
