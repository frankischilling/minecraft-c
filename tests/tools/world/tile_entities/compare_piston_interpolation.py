"""Replay a frozen paired original Piston interpolation family through native Tiles.

No original JVM is launched. Only the 18,292 interpolation rows per side are
compared. Raw private k/l inputs and the public getter are exercised on actual
native owners; this makes no constructor, NBT, World, geometry or gameplay claim.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PureWindowsPath
import re
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[4]
COUNT = 18292
FROZEN_REPORTS = {
    "execution-a/report.json": (14296, "3963d79661e3e4ce75a98bf1f62c4c5f0148e49f3ebef65aa96b6430f7d34478"),
    "analysis-a/report.json": (15915, "ff46384081cc59e25b26c843fab6e277170092a71476f8609c27023d8028756e"),
}
COMPACT_SHA256 = "4afe590d978b4059cbbab3eebe1e3ee10822d53b287966970cb18ccfecd7b451"
SOURCES = (
    "tests/world/runtime/tile_entities/piston_float_probe.c",
    "tests/tools/world/tile_entities/compare_piston_interpolation.py",
    "src/world/runtime/tile_entities/tile_entities.c",
    "src/world/runtime/tile_entities/tile_entities_internal.h",
    "src/world/runtime/tile_entities/piston_math.c",
    "src/world/runtime/tile_entities/piston_math_internal.h",
    "include/mc/world/runtime/tile_entities/tile_entities.h",
    "src/world/runtime/construction.c",
    "src/world/runtime/construction_internal.h",
    "src/world/runtime/runtime.c",
    "src/world/runtime/runtime_internal.h",
    "include/mc/world/runtime/construction.h",
    "include/mc/world/runtime/runtime.h",
    "src/block/registry/registry.c",
    "src/block/registry/registry_internal.h",
    "src/block/registry/runtime_internal.h",
    "include/mc/block/registry/registry.h",
    "src/core/java/random_profile.c",
    "CMakeLists.txt",
    ".clang-format",
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def pin(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def write(path, value):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, sort_keys=True, indent=2)
        stream.write("\n")


def load(path):
    return json.loads(path.read_text("utf-8"))


def records(data):
    """Remove solely verified, uniform complete LF or CRLF record framing."""
    require(bool(data) and data.endswith(b"\n"), "incomplete protocol record")
    lines = data.split(b"\n")[:-1]
    if all(line.endswith(b"\r") for line in lines):
        ending, lines = "CRLF", [line[:-1] for line in lines]
    else:
        require(all(b"\r" not in line for line in lines), "mixed or lone CR record framing")
        ending = "LF"
    require(all(b"\r" not in line for line in lines), "CR inside record payload")
    return ending, lines


def verify_original(original):
    """Validate immutable receipts, complete processes and every raw compact join.

    Frozen receipts retain historical external JAR/JVM/source pins. Family-local
    paths can be relocated under --original; no historical executable is run or
    represented as the currently installed JVM.
    """
    verified, external = {}, []

    def remember(path):
        actual = pin(path)
        verified[str(Path(path).resolve())] = actual
        return actual

    for name, (size, digest) in FROZEN_REPORTS.items():
        actual = remember(original / name)
        require((actual["bytes"], actual["sha256"]) == (size, digest),
                "unsupported or changed frozen original report: " + name)
    capture = load(original / "execution-a/report.json")
    analysis = load(original / "analysis-a/report.json")
    require(capture["status"] == analysis["status"] == "PASS" and
            capture["errors"] == analysis["errors"] == [], "original family did not complete")
    old_root = PureWindowsPath(capture["producer_before"]["path"]).parent

    def verify(item):
        require(set(item) == {"path", "bytes", "sha256"} and
                isinstance(item["bytes"], int) and item["bytes"] >= 0 and
                re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is not None,
                "malformed historical pin")
        try:
            relative = PureWindowsPath(item["path"]).relative_to(old_root)
        except ValueError:
            if item not in external:
                external.append(item)
            return
        require(".." not in relative.parts, "unsafe historical family path")
        actual = remember(original.joinpath(*relative.parts))
        require((actual["bytes"], actual["sha256"]) == (item["bytes"], item["sha256"]),
                "changed frozen evidence: " + str(relative))

    verify(analysis["capture"])
    verify(analysis["producer"])
    require(analysis["root_process_closure"]["exit_code"] == 0 and
            analysis["root_process_closure"]["termination"] is False,
            "original analysis root process incomplete")
    for item in analysis["verified_inputs"]:
        verify(item)
    for name in ("producer", "helper", "java", "javac", "class", "input"):
        require(capture[name + "_stable"] is True and
                capture[name + "_before"] == capture[name + "_after"],
                "unstable original input: " + name)
        verify(capture[name + "_before"])
    require(capture["jars_stable"] is True and
            capture["jars_before"] == capture["jars_after"], "unstable original JARs")
    for item in list(capture["jars_before"].values()) + capture["authority"] + [
            capture["source_plan"], capture["cases"]]:
        verify(item)

    execution = original / "execution-a"
    cases = load(execution / "cases.json")
    require(len(cases) == capture["expected_rows_per_side"] == 19140,
            "original full corpus count changed")
    expected_input = []
    for index, case in enumerate(cases):
        require(case["id"] == f"p{index:06d}", "original case order changed")
        fields = [case[key] for key in ("id", "operation", "mode", "a", "b", "c",
                                       "extending", "source", "k", "l", "t", "variant")]
        line = "\t".join(map(str, fields))
        require(case["line"] == line, "case description/input join failed")
        expected_input.append(line.encode("ascii"))
    require((execution / "input.tsv").read_bytes() == b"\n".join(expected_input) + b"\n",
            "original full input/case bytes differ")
    expected_labels = ["java-version", "javac-version", "compile-helper",
                       "client-original", "server-original"]
    require([item["label"] for item in capture["processes"]] == expected_labels,
            "original process set/order changed")
    for process in capture["processes"]:
        label = process["label"]
        require(process["returncode"] == 0 and process["reaped"] is True and
                process["terminated"] is False and process["end_ns"] >= process["start_ns"] and
                process["pid"] > 0 and "launch_error" not in process,
                "original process did not complete naturally: " + label)
        require(process["stdin"] == (capture["input_before"] if label.endswith("-original")
                                     else None), "original process stdin changed")
        for suffix in ("command", "live", "result"):
            path = execution / (label + "." + suffix + ".json")
            record = load(path)
            require(all(process.get(key) == value for key, value in record.items()) and
                    (suffix != "result" or record == process),
                    "original process receipt join failed: " + path.name)
            remember(path)
        verify(process["stdout"])
        verify(process["stderr"])

    compact_by_side, summaries = {}, {}
    for side in ("CLIENT", "SERVER"):
        lower = side.lower()
        receipt = capture["sides"][side]
        detail = analysis["sides"][side]
        require(receipt["status"] == "PASS" and receipt["rows"] == 19140 and
                receipt["invalid_case_ids"] == [] and receipt["complete_traces"] is True and
                receipt["actual_exception_rows"] == 8 and detail["interpolation_rows"] == COUNT,
                "original side receipt incomplete: " + side)
        for name in ("rows_capture", "results", "metadata"):
            verify(receipt[name])
        for name in ("models", "priority_observations", "interpolation_probe_input",
                     "nbt_observations"):
            verify(detail[name])
        raw = (execution / (lower + "-original.stdout")).read_bytes()
        original_ending, raw_rows = records(raw)
        require(raw_rows[0] == ("PISTON_FLOAT_V1\t" + side).encode("ascii"),
                "original header mismatch")
        terminator = b"\r\n" if original_ending == "CRLF" else b"\n"
        require(b"".join(row + terminator for row in raw_rows[1:]) ==
                (execution / (lower + ".rows.stdout")).read_bytes(),
                "original full raw stream extraction differs")
        require(len(raw_rows) == 19141, "original raw row count differs")
        results = load(execution / (lower + ".results.json"))
        require(len(results) == 19140, "original result count differs")
        joined, failures = [], []
        for case, raw_row, result in zip(cases, raw_rows[1:], results):
            line = raw_row.decode("ascii")
            fields = line.split("\t")
            require(len(fields) == 18 and fields[:3] ==
                    [case["id"], case["operation"], case["mode"]] and
                    fields[6] == ",".join(case[key] for key in ("k", "l", "t")),
                    "original raw row/case identity or input reread differs")
            require(result == {"id": case["id"], "label": case["label"], "line": line,
                               "valid": True, "failure": None},
                    "original parsed result/raw row join failed")
            if fields[3] != "OK":
                failures.append((case["id"], fields[3]))
            if case["operation"] != "interp":
                continue
            require(fields[3] == "OK" and fields[5] == fields[8] and
                    fields[5].split(",")[-2:] == [case["k"], case["l"]] and
                    fields[11:] == ["-"] * 7 and
                    re.fullmatch(r"[0-9a-f]{8}", fields[7]) is not None,
                    "original interpolation operation/state facts differ")
            joined.append("\t".join([case["id"], case["k"], case["l"], case["t"],
                                      fields[7]]).encode("ascii"))
        _, traces = records((execution / (lower + "-original.stderr")).read_bytes())
        require(len(traces) == len(failures) == 8, "original exception trace count differs")
        for trace, (name, outcome) in zip(traces, failures):
            fields = trace.decode("ascii").split("\t")
            require(len(fields) == 3 and fields[:2] == ["EXCEPTION", name],
                    "original exception identity differs")
            decoded = bytes.fromhex(fields[2]).decode("utf-8")
            require(decoded.startswith(outcome) and "\tat " in decoded,
                    "original exception trace incomplete")
        path = original / "analysis-a" / (lower + ".interpolation.tsv")
        compact = path.read_bytes()
        require(len(compact) == 804848 and hashlib.sha256(compact).hexdigest() == COMPACT_SHA256,
                "compact original identity changed")
        ending, compact_rows = records(compact)
        require(ending == "LF" and compact_rows == joined and len(joined) == COUNT,
                "original compact/raw row join failed")
        for index, row in enumerate(joined):
            require(re.fullmatch(rb"p[0-9]{6}(?:\t[0-9a-f]{8}){4}", row) is not None and
                    row.split(b"\t")[0] == f"p{index + 25:06d}".encode("ascii"),
                    "compact selected row order/shape changed")
        compact_by_side[side] = compact_rows
        summaries[side] = {"full_original_rows": 19140, "selected_rows": COUNT,
                           "excluded_rows": 848, "original_line_endings": original_ending,
                           "compact": remember(path)}
    require(compact_by_side["CLIENT"] == compact_by_side["SERVER"] and
            capture["paired_rows_byte_identical"] is True, "paired original compact rows differ")
    return compact_by_side, summaries, list(verified.values()), external


def build_pins(native):
    """Pin selected CMake build metadata without claiming a full rebuild proof."""
    build = next((parent for parent in native.parents if (parent / "CMakeCache.txt").is_file()), None)
    require(build is not None, "native must belong to a retained CMake build directory")
    files = [build / "CMakeCache.txt"]
    for name in ("build.ninja", "Makefile", "compile_commands.json", "CMakeFiles/rules.ninja"):
        path = build / name
        if path.is_file():
            files.append(path)
    files.extend(sorted(build.glob("*.sln")))
    require(len(files) >= 2, "selected CMake build files unavailable")
    for line in (build / "CMakeCache.txt").read_text("utf-8").splitlines():
        if line.startswith("CMAKE_C_COMPILER:FILEPATH="):
            compiler = Path(line.split("=", 1)[1])
            if compiler.is_file():
                files.append(compiler)
    return [pin(path) for path in files]


def execute(output, label, native, side, input_path):
    command = [str(native), "piston-interpolation", side]
    record = {"label": label, "command": command, "cwd": str(ROOT), "stdin": pin(input_path),
              "start_ns": time.time_ns(), "executable_before": pin(native)}
    write(output / (label + ".command.json"), record)
    process = None
    try:
        with input_path.open("rb") as source, (output / (label + ".stdout")).open("xb") as stdout, \
                (output / (label + ".stderr")).open("xb") as stderr:
            process = subprocess.Popen(command, cwd=ROOT, stdin=source, stdout=stdout, stderr=stderr)
            record["pid"] = process.pid
            try:
                write(output / (label + ".live.json"), record)
            finally:
                # Once launched, always retain streams until natural completion,
                # even if writing the live receipt fails or Ctrl-C arrives.
                while True:
                    try:
                        record["returncode"] = process.wait()
                        break
                    except KeyboardInterrupt:
                        record["interrupted"] = True
                record["reaped"], record["terminated"] = True, False
    except OSError as error:
        record["launch_error" if process is None else "receipt_error"] = str(error)
        if process is None:
            record["returncode"] = None
    record["end_ns"] = time.time_ns()
    record["executable_after"] = pin(native)
    record["stdout"] = pin(output / (label + ".stdout"))
    record["stderr"] = pin(output / (label + ".stderr"))
    write(output / (label + ".result.json"), record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True,
                        help="completed frozen family containing execution-a and analysis-a")
    parser.add_argument("--output", type=Path, required=True, help="fresh evidence directory")
    args = parser.parse_args()
    native, original, output = (path.resolve() for path in
                                (args.native, args.original, args.output))
    if output.exists():
        parser.error("output must be new; retained evidence is never overwritten")
    if output == original or original in output.parents:
        parser.error("output must be outside the frozen original family")
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "FAIL", "scope": "paired actual native Piston interpolation getter replay",
              "rows_per_side": COUNT, "original": str(original), "errors": [], "processes": [],
              "sides": {}, "limitations": [
                  "No original JVM rerun; only this explicitly frozen completed original family is accepted.",
                  "No native constructor, NBT, geometry, World construction or gameplay parity claim.",
                  "Private raw k/l controls are source inputs; the public getter computes output.",
                  "Raw streams are unchanged; comparison removes solely uniform LF/CRLF framing.",
                  "Selected source/build/executable pins do not independently prove rebuild provenance.",
                  "Historical external JVM/JAR/source pins are retained, not current-host validation."]}
    verified, sources, builds, input_pins = [], [], [], []
    try:
        expected, summaries, verified, external = verify_original(original)
        report["original_verified_before"] = verified
        report["historical_external_pins"] = external
        report["original_sides"] = summaries
        sources = [pin(ROOT / name) for name in SOURCES]
        builds = build_pins(native)
        report["sources_before"], report["build_before"] = sources, builds
        report["native_before"], report["python_before"] = pin(native), pin(sys.executable)
        for side in ("CLIENT", "SERVER"):
            lower = side.lower()
            input_path = output / (lower + ".input.tsv")
            expected_path = output / (lower + ".expected.tsv")
            input_path.write_bytes(b"\n".join(row.rsplit(b"\t", 1)[0] for row in expected[side]) + b"\n")
            expected_path.write_bytes(b"\n".join(expected[side]) + b"\n")
            input_pin, expected_pin = pin(input_path), pin(expected_path)
            input_pins.extend([input_pin, expected_pin])
            process = execute(output, lower + "-native", native, side, input_path)
            report["processes"].append(process)
            require(process.get("reaped") is True and process.get("terminated") is False and
                    not any(key in process for key in ("launch_error", "receipt_error", "interrupted")) and
                    process["returncode"] == 0 and process["executable_before"] ==
                    process["executable_after"] == report["native_before"],
                    "native process did not complete naturally: " + side)
            ending, rows = records((output / (lower + "-native.stdout")).read_bytes())
            require(rows[0] == ("PISTON_INTERPOLATION_V1\t" + side).encode("ascii") and
                    len(rows) == COUNT + 1, "native header/count mismatch: " + side)
            _, completion = records((output / (lower + "-native.stderr")).read_bytes())
            require(completion == [("PASS: piston interpolation; side=" + side +
                                    "; rows=18292; cleanup=OK").encode("ascii")],
                    "native completion/cleanup signature mismatch: " + side)
            mismatches = [{"row": index, "case": original_row.split(b"\t")[0].decode("ascii"),
                           "expected": original_row.decode("ascii"),
                           "actual_hex": actual.hex()}
                          for index, (original_row, actual) in
                          enumerate(zip(expected[side], rows[1:])) if original_row != actual]
            write(output / (lower + ".mismatches.json"), mismatches)
            require(not mismatches, "native raw field mismatch: " + side)
            view = output / (lower + ".native.rows.tsv")
            view.write_bytes(b"\n".join(rows[1:]) + b"\n")
            report["sides"][side] = {"status": "PASS", "rows": COUNT, "mismatches": 0,
                                     "native_line_endings": ending, "input": input_pin,
                                     "expected": expected_pin, "native_row_view": pin(view),
                                     "cleanup_signature": completion[0].decode("ascii")}
        report["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        for name, before in (("original_verified", verified), ("sources", sources),
                             ("build", builds), ("generated_inputs", input_pins)):
            try:
                after = [pin(item["path"]) for item in before]
                report[name + "_after"] = after
                require(after == before, "inputs changed during comparison: " + name)
            except Exception as error:
                report["status"] = "FAIL"
                report["errors"].append(type(error).__name__ + ": " + str(error))
        for name, path in (("native", native), ("python", Path(sys.executable))):
            try:
                report[name + "_after"] = pin(path)
                if name + "_before" in report:
                    require(report[name + "_after"] == report[name + "_before"],
                            name + " changed during comparison")
            except Exception as error:
                report["status"] = "FAIL"
                report["errors"].append(type(error).__name__ + ": " + str(error))
        report["all_native_processes_reaped"] = len(report["processes"]) == 2 and all(
            item.get("reaped") is True and item.get("terminated") is False and
            item.get("returncode") == 0 and not any(key in item for key in
                ("launch_error", "receipt_error", "interrupted")) for item in report["processes"])
        if report["status"] == "PASS" and not report["all_native_processes_reaped"]:
            report["status"] = "FAIL"
            report["errors"].append("native process closure incomplete")
        write(output / "report.json", report)
    print(report["status"], str(output / "report.json"), flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
