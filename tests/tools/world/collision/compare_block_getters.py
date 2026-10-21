"""Fresh original/native comparison of 623 source-unused-World Block getters.

The complete 2,200-row original corpus remains retained. Only successful rows
from its first 672-case getter prefix are compared with the separately built
native probe. Original getters receive null World. Native getters receive a
genuine GAME World for native pool-owner binding; selected original methods
leave their World argument unused. This is not nonnull-World metadata parity.
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

import capture_block_shapes as capture

HERE = Path(__file__).resolve().parent
pin, write = capture.pin, capture.write
SOURCES = (
    "tests/world/runtime/collision/block_shape_probe.c",
    "tests/world/runtime/test_entity_publication.c",
    "src/world/runtime/collision/blocks.c",
    "src/world/runtime/collision/blocks_internal.h",
    "src/world/runtime/collision/boxes.c",
    "src/world/runtime/collision/boxes_internal.h",
    "include/mc/world/runtime/collision/boxes.h",
    "src/entity/bounds/bounds.c",
    "include/mc/entity/bounds/bounds.h",
    "src/entity/classes/classes_internal.h",
    "src/block/registry/registry_internal.h",
    "src/block/registry/runtime_internal.h",
    "include/mc/block/registry/runtime.h",
    "tests/tools/world/collision/BlockShapeObservation.java",
    "tests/tools/world/collision/capture_block_shapes.py",
    "tests/tools/world/collision/compare_block_getters.py",
    "CMakeLists.txt",
)
BOOTSTRAP = [b"16 achievements", b"151 recipes"]


def strict_records(data):
    """Remove only one verified uniform LF/CRLF terminator per record."""
    if not data or not data.endswith(b"\n"):
        raise RuntimeError("protocol must end with a complete LF/CRLF record")
    records = data.split(b"\n")[:-1]
    if all(record.endswith(b"\r") for record in records):
        ending = "CRLF"
        payloads = [record[:-1] for record in records]
    elif all(b"\r" not in record for record in records):
        ending, payloads = "LF", records
    else:
        raise RuntimeError("mixed LF/CRLF or lone CR in protocol")
    if any(b"\r" in payload for payload in payloads):
        raise RuntimeError("lone CR inside protocol record")
    terminator = b"\r\n" if ending == "CRLF" else b"\n"
    return ending, payloads, [payload + terminator for payload in payloads]


def execute(output, label, command, cwd, input_path=None):
    record = {"label": label, "command": command, "cwd": str(cwd),
              "stdin": pin(input_path) if input_path else None, "start_ns": time.time_ns(),
              "executable_before": pin(command[0])}
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
    try:
        record["executable_after"] = pin(command[0])
    except OSError as error:
        record["executable_pin_error"] = str(error)
        record["executable_after"] = None
    record["stdout"], record["stderr"] = pin(output / (label + ".stdout")), pin(output / (label + ".stderr"))
    write(output / (label + ".result.json"), record)
    print(label, record["returncode"], flush=True)
    return record


def verify_original(original, repo, retained):
    def verify(path, expected=None):
        actual = pin(path)
        if expected is not None and actual != expected:
            raise RuntimeError(f"fresh original evidence changed: {path}")
        retained.append(actual)
        return actual

    report_path = original / "report.json"
    report = json.loads(report_path.read_text("utf-8"))
    receipt_pin = verify(report_path)
    if report["status"] != "PASS" or not report["paired_shape_rows_byte_identical"]:
        raise RuntimeError("complete fresh paired original capture did not pass")
    for key in ("producer", "helper", "java", "javac", "class", "input"):
        if not report[key + "_stable"] or report[key + "_before"] != report[key + "_after"]:
            raise RuntimeError("fresh original input instability")
        verify(report[key + "_before"]["path"], report[key + "_before"])
    if report["producer_before"] != pin(HERE / "capture_block_shapes.py") or \
            report["helper_before"] != pin(HERE / "BlockShapeObservation.java") or \
            report["class_before"] != pin(original / "classes/BlockShapeObservation.class"):
        raise RuntimeError("fresh original producer/helper/class join mismatch")
    if sorted(p.name for p in (original / "classes").iterdir()) != ["BlockShapeObservation.class"]:
        raise RuntimeError("unexpected compiled engine/helper classes")
    if not report["jars_stable"] or report["jars_before"] != report["jars_after"]:
        raise RuntimeError("fresh original JAR instability")
    for side, (filename, length, digest) in capture.JARS.items():
        actual = verify(repo / filename, report["jars_before"][side])
        if actual["bytes"] != length or actual["sha256"] != digest:
            raise RuntimeError("fresh original JAR identity mismatch")
    cases = capture.corpus()
    if len(cases) != 2200 or cases != json.loads((original / "cases.json").read_text("utf-8")):
        raise RuntimeError("complete original corpus regeneration mismatch")
    verify(original / "cases.json", report["cases"])
    expected_input = ("\n".join(case["line"] for case in cases) + "\n").encode("ascii")
    if (original / "input.tsv").read_bytes() != expected_input:
        raise RuntimeError("complete original process input mismatch")
    verify(original / "input.tsv", report["input_before"])
    java, javac = report["java_before"]["path"], report["javac_before"]["path"]
    commands = {
        "java-version": [java, "-version"],
        "javac-version": [javac, "-version"],
        "compile-helper": [javac, "-encoding", "UTF-8", "-d", str(original / "classes"),
                           str(HERE / "BlockShapeObservation.java")],
    }
    for side, (filename, _, _) in capture.JARS.items():
        commands[side.lower() + "-original"] = [
            java, "-Xint", "-Dfile.encoding=US-ASCII", "-cp", str(original / "classes"),
            "BlockShapeObservation", str(repo / filename), side,
            str(original / (side.lower() + ".inventory.tsv")),
        ]
    if [process["label"] for process in report["processes"]] != list(commands):
        raise RuntimeError("fresh original process inventory mismatch")
    for process in report["processes"]:
        label = process["label"]
        if process["command"] != commands[label] or process["cwd"] != str(repo) or \
                process["returncode"] != 0 or not process["reaped"] or process["terminated"] or \
                process["end_ns"] < process["start_ns"]:
            raise RuntimeError("fresh original command/natural outcome mismatch")
        if process["stdin"] != (report["input_before"] if label.endswith("-original") else None):
            raise RuntimeError("fresh original process stdin mismatch")
        for suffix in ("command", "live", "result"):
            path = original / (label + "." + suffix + ".json")
            document = json.loads(path.read_text("utf-8"))
            if any(process.get(key) != value for key, value in document.items()) or \
                    suffix == "result" and document != process:
                raise RuntimeError("fresh original process record join mismatch")
            verify(path)
        for stream in ("stdout", "stderr"):
            verify(original / (label + "." + stream), process[stream])
    references = {}
    for side in capture.JARS:
        side_report = report["sides"][side]
        if side_report["status"] != "PASS" or not side_report["complete_exception_traces"]:
            raise RuntimeError("fresh original side/exception capture incomplete")
        for key in ("raw_rows", "results", "bootstrap", "inventory"):
            verify(side_report[key]["path"], side_report[key])
        original_ending, payloads, raw = strict_records(
            (original / (side.lower() + "-original.stdout")).read_bytes())
        header = ("BLOCK_SHAPES_V1\t" + side).encode("ascii")
        if payloads[:2] != BOOTSTRAP or len(raw) != 2203 or payloads[2] != header:
            raise RuntimeError("fresh original actual bootstrap/header/row count mismatch")
        if (original / (side.lower() + ".bootstrap.stdout")).read_bytes() != b"".join(raw[:2]) or \
                (original / (side.lower() + ".rows.stdout")).read_bytes() != b"".join(raw[3:]):
            raise RuntimeError("fresh original raw stream extraction mismatch")
        results = json.loads((original / (side.lower() + ".results.json")).read_text("utf-8"))
        if len(results) != len(cases):
            raise RuntimeError("fresh original recorded result count mismatch")
        fields_by_id, failures = {}, []
        for case, payload, result in zip(cases, payloads[3:], results):
            line = payload.decode("ascii")
            fields = line.split("\t")
            if len(fields) != 19 or fields[:3] != [
                    case["id"], case["operation"], str(case["block_id"])] or not capture.valid_values(fields):
                raise RuntimeError("fresh original row/identity/raw-value mismatch")
            if fields[3] not in ("OK", "java.lang.NullPointerException", "java.lang.ArrayIndexOutOfBoundsException"):
                raise RuntimeError("unexpected original exception class")
            expected = {"id": case["id"], "label": case["label"], "line": line,
                        "classification": capture.classify(case, fields), "valid": True}
            if result != expected:
                raise RuntimeError("fresh original row/result/classification join mismatch")
            if fields[3] != "OK":
                failures.append((case["id"], fields[3]))
            fields_by_id[case["id"]] = fields
        diagnostics = (original / (side.lower() + "-original.stderr")).read_text("ascii").splitlines()
        if len(diagnostics) != len(failures):
            raise RuntimeError("fresh original exception trace inventory mismatch")
        for detail, (identity, outcome) in zip(diagnostics, failures):
            parts = detail.split("\t")
            if len(parts) != 3 or parts[:2] != ["EXCEPTION", identity]:
                raise RuntimeError("fresh original exception trace identity mismatch")
            trace = bytes.fromhex(parts[2]).decode("utf-8")
            if not trace.startswith(outcome) or "\tat " not in trace:
                raise RuntimeError("fresh original exception trace incomplete")
        inventory = (original / (side.lower() + ".inventory.tsv")).read_text("ascii").splitlines()
        if len(inventory) != 97 or inventory[0] != "BLOCK_SHAPE_REGISTRY_V1" or \
                [line.split("\t")[0] for line in inventory[1:]] != [str(i) for i in range(1, 97)]:
            raise RuntimeError("fresh original real registered owner inventory mismatch")
        references[side] = {"raw": raw[3:], "fields": fields_by_id,
                            "payloads": payloads[3:], "line_ending": original_ending}
    if (original / "client.rows.stdout").read_bytes() != (original / "server.rows.stdout").read_bytes():
        raise RuntimeError("fresh paired original raw shape row bytes differ")
    return receipt_pin, cases, references


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True, type=Path,
                        help="separately qualified Block getter values probe executable")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[4],
                        help="repository root containing original JARs and native sources")
    parser.add_argument("--output", required=True, type=Path, help="fresh retained capture/replay directory")
    parser.add_argument("--java", help="Java launcher (default: java on PATH)")
    parser.add_argument("--javac", help="Java compiler (default: javac on PATH)")
    args = parser.parse_args()
    repo, native, output = (path.resolve() for path in (args.repo, args.native, args.output))
    if output.exists():
        parser.error("output must be new; existing evidence is never overwritten")
    if not repo.is_dir() or not native.is_file():
        parser.error("repository and separately qualified native executable must exist")
    output.mkdir(parents=True, exist_ok=False)
    report = {
        "status": "FAIL", "scope": "623 source-unused-World registered Block getter rows per side",
        "binding_difference": "original passes null unused World; native uses genuine GAME World for native pool-owner binding",
        "framing_policy": "uniform LF or CRLF only; compare complete record payload bytes, excluding only verified final record terminators",
        "limitations": [
            "not nonnull-World metadata/Tile behavior parity",
            "not contribution, moving-helper, movement or whole-game acceptance",
            "original 49 null-World failures in prefix remain excluded, not simulated natively",
            "remaining 1528 original corpus rows remain retained outside this comparison",
            "native allocation/resource failures are not Java OOM equivalence",
            "native executable-to-source provenance requires independent native build qualification",
            "selected tools/helper identities do not pin the complete host runtime",
        ],
        "producer_before": pin(__file__), "native_before": pin(native),
        "python_before": pin(sys.executable), "host": {"os": os.name, "python": sys.version},
        "processes": [], "sides": {}, "errors": [],
    }
    retained = []
    try:
        report["source_inputs_before"] = [pin(repo / source) for source in SOURCES]
        original = output / "original"
        command = [sys.executable, str(HERE / "capture_block_shapes.py"),
                   "--repo", str(repo), "--output", str(original)]
        for option in ("java", "javac"):
            if getattr(args, option):
                command.extend(["--" + option, getattr(args, option)])
        process = execute(output, "fresh-original-capture", command, repo)
        report["processes"].append(process)
        if process["returncode"] != 0:
            raise RuntimeError("fresh original family failed; complete attempt retained")
        receipt, cases, references = verify_original(original, repo, retained)
        report["original_capture"] = receipt
        prefix = cases[:672]
        if [case["block_id"] for case in prefix] != list(range(1, 97)) * 7 or \
                any(case["operation"] != "get" or case["bounds"] != "ctor" or
                    case["pool_mode"] != "reset" or case["repeats"] != 1 for case in prefix):
            raise RuntimeError("source getter-prefix protocol changed")
        for side in capture.JARS:
            selected, indexes, excluded = [], [], []
            for index, case in enumerate(cases):
                fields = references[side]["fields"][case["id"]]
                if index < 672 and case["block_id"] not in capture.WORLD_GETTERS:
                    if fields[3] != "OK":
                        raise RuntimeError("selected source-unused-World getter did not succeed")
                    selected.append(case)
                    indexes.append(index)
                else:
                    if index < 672:
                        if fields[3] != "java.lang.NullPointerException" or fields[4:6] != ["0", "0"]:
                            raise RuntimeError("original null-World prefix failure changed")
                        reason = "actual original null-World dependency failure; nonnull-World parity excluded"
                    else:
                        reason = "outside the first 672-case constructor-getter prefix"
                    excluded.append({"id": case["id"], "label": case["label"],
                                     "original_outcome": fields[3], "reason": reason})
            if len(selected) != 623 or len(excluded) != 1577 or \
                    sum(case["block_id"] in capture.WORLD_GETTERS for case in prefix) != 49:
                raise RuntimeError("selected/excluded original scope count mismatch")
            write(output / (side.lower() + ".selected.json"), selected)
            write(output / (side.lower() + ".excluded.json"), excluded)
            input_path = output / (side.lower() + ".input.tsv")
            input_path.write_bytes(("\n".join(case["line"] for case in selected) + "\n").encode("ascii"))
            expected_path = output / (side.lower() + ".original-selected.stdout")
            expected_path.write_bytes(b"".join(references[side]["raw"][index] for index in indexes))
            retained.extend([pin(input_path), pin(expected_path),
                             pin(output / (side.lower() + ".selected.json")),
                             pin(output / (side.lower() + ".excluded.json"))])
            process = execute(output, side.lower() + "-native", [
                str(native), "block-getters", side,
                str(output / (side.lower() + "-world")).encode("utf-8").hex(),
            ], repo, input_path)
            report["processes"].append(process)
            if process["executable_before"] != report["native_before"] or \
                    process["executable_after"] != report["native_before"]:
                raise RuntimeError("native executable identity changed across side invocation")
            native_raw = (output / (side.lower() + "-native.stdout")).read_bytes()
            # Native bootstrap is real platform console output. The probe then
            # switches protocol framing explicitly; verify these sections
            # separately without changing either retained raw stream.
            pieces = native_raw.split(b"\n")
            if len(pieces) < 4 or pieces[-1] != b"":
                raise RuntimeError("native stdout has incomplete bootstrap/protocol records")
            raw = [piece + b"\n" for piece in pieces[:-1]]
            bootstrap_ending, bootstrap_payloads, _ = strict_records(b"".join(raw[:2]))
            native_ending, protocol_payloads, protocol_records = strict_records(b"".join(raw[2:]))
            header = ("BLOCK_SHAPES_V1\t" + side).encode("ascii")
            framing = bootstrap_payloads == BOOTSTRAP and protocol_payloads[0] == header
            bootstrap_path = output / (side.lower() + ".native-bootstrap.stdout")
            header_path = output / (side.lower() + ".native-header.stdout")
            rows_path = output / (side.lower() + ".native-rows.stdout")
            bootstrap_path.write_bytes(b"".join(raw[:2]))
            header_path.write_bytes(protocol_records[0])
            rows_path.write_bytes(b"".join(protocol_records[1:]))
            results, mismatches = [], []
            for position, (case, index) in enumerate(zip(selected, indexes)):
                expected = references[side]["payloads"][index]
                actual = protocol_payloads[position + 1] if position + 1 < len(protocol_payloads) else None
                match = actual == expected and actual is not None and len(actual.split(b"\t")) == 19
                if not match:
                    mismatches.append(case["id"])
                results.append({"id": case["id"], "label": case["label"], "match": match,
                    "original_record_payload": expected.decode("ascii"),
                    "native_record_payload": actual.decode("ascii") if actual is not None else None})
            write(output / (side.lower() + ".results.json"), results)
            stderr = (output / (side.lower() + "-native.stderr")).read_text("ascii")
            complete = re.fullmatch(
                rf"PASS: block getter replay; side={side}; rows=623; checks=[1-9][0-9]*; cleanup=OK\n",
                stderr,
            ) is not None
            expected_payloads = [references[side]["payloads"][index] for index in indexes]
            exact = protocol_payloads[1:] == expected_payloads
            report["sides"][side] = {
                "status": "PASS" if framing and len(raw) == 626 and not mismatches and exact
                    and process["returncode"] == 0 and complete else "FAIL",
                "original_complete_rows": 2200, "getter_prefix_rows": 672,
                "original_null_world_failures_excluded": 49, "other_original_rows_excluded": 1528,
                "selected_rows": len(selected), "excluded_rows": len(excluded),
                "native_rows": max(0, len(protocol_payloads) - 1), "mismatch_ids": mismatches,
                "exact_record_payload_bytes": exact, "exact_bootstrap_and_header": framing,
                "original_line_ending": references[side]["line_ending"],
                "native_line_ending": native_ending,
                "native_bootstrap_line_ending": bootstrap_ending,
                "raw_row_bytes_identical": rows_path.read_bytes() == expected_path.read_bytes(),
                "native_cleanup_completion": complete,
                "input": pin(input_path), "expected_rows": pin(expected_path),
                "native_rows_capture": pin(rows_path), "bootstrap": pin(bootstrap_path),
                "header": pin(header_path), "selected": pin(output / (side.lower() + ".selected.json")),
                "excluded": pin(output / (side.lower() + ".excluded.json")),
                "results": pin(output / (side.lower() + ".results.json")),
            }
        report["retained_inputs"] = retained
        for item in retained:
            if pin(item["path"]) != item:
                raise RuntimeError("original or selected replay evidence changed")
        report["producer_after"], report["native_after"] = pin(__file__), pin(native)
        report["python_after"] = pin(sys.executable)
        report["source_inputs_after"] = [pin(repo / source) for source in SOURCES]
        report["stable"] = report["producer_before"] == report["producer_after"] and \
            report["native_before"] == report["native_after"] and \
            report["python_before"] == report["python_after"] and \
            report["source_inputs_before"] == report["source_inputs_after"]
        if report["stable"] and all(side["status"] == "PASS" for side in report["sides"].values()):
            report["status"] = "PASS"
    except Exception as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write(output / "report.json", report)
    print(json.dumps({"status": report["status"], "sides": report["sides"],
                      "errors": report["errors"], "output": str(output)}), flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
