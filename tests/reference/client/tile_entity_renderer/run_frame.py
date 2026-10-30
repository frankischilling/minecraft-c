"""Compare all 96 LL frame method rows literally, with explicit operand limits.

Original wh(NULL) construction remains original-only. Native uses its genuine
RAW dc/LocalPlayer allocator and the shared inherited SN pose fields. Random,
Math and Watcher absence preservation earns no instantiated-object state parity.
No NaN normalization, tolerance, subset, expected-output input or fake constructor.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
spec = importlib.util.spec_from_file_location("tile_frame_original_schema", HERE / "capture_frame.py")
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
ensure, digest, capture, require_success = original.ensure, original.digest, original.capture, original.require_success
RAW_SHA256 = "dfec04b995618322b5872f2b15ab9dc757fd0efea2ec3f35c4d061197f17fe07"
TAGS = ("TILE_FRAME_DECLARED", "TILE_FRAME_INPUT", "TILE_FRAME_RESULT", "TILE_FRAME_SIDE")


def inventory(rows, cases):
    ensure(isinstance(rows, list) and len(rows) == 96 and len(cases) == 24 and
           all(isinstance(row, str) and row and row == row.strip() and " ".join(row.split()) == row for row in rows),
           "complete exact 24-case/96-row method inventory")
    for index, case in enumerate(cases):
        records = rows[4 * index:4 * index + 4]
        ensure([row.split()[0] for row in records] == list(TAGS) and
               all(row.split()[1] == str(index) for row in records), "all four method records, in source observation order")
        ensure(records[0] == "TILE_FRAME_DECLARED " + original.encode([case])[0], "complete original declared operand row")
        stored = records[1].split()
        ensure(len(stored) == 14 and stored[:3] == [TAGS[1], str(index), case["name"]] and
               all(re.fullmatch(r"[0-9a-f]{8}", word) for word in stored[3:8]) and
               all(re.fullmatch(r"[0-9a-f]{16}", word) for word in stored[8:]), "actual stored input words")
        result = records[2].split()
        ensure(len(result) == 15 and result[:3] == [TAGS[2], str(index), "1"] and
               all(word in ("0", "1") for word in result[3:10]) and
               all(re.fullmatch(r"[0-9a-f]{8}", word) for word in result[10:12]) and
               all(re.fullmatch(r"[0-9a-f]{16}", word) for word in result[12:]), "whole normal return/reference/float/double words")
        side = records[3].split()
        ensure(len(side) == 9 and all(word in ("0", "1") for word in side[2:]), "all actual method-local side-effect flags")


def bindings(executable, input_path):
    paths = [executable, input_path, HERE / "run_frame.py", HERE / "frame-finite.json", HERE / "frame-provenance.json",
             HERE / "frame-inputs.json", HERE / "frame-source-pins.json", HERE / "capture_frame.py",
             HERE / "TileEntityRendererFrameObservation.java", HERE / "FRAME_README.md", HERE / "capture_reference.py",
             HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py",
             REPO / "tests/client/render/test_tile_entity_renderer_frame.c", REPO / "tests/client/render/test_tile_entity_renderer.c",
             REPO / "include/mc/client/render/tile_entity_renderer/tile_entity_renderer.h",
             REPO / "src/client/render/model.c", REPO / "src/client/render/model_graph.c",
             REPO / "src/client/render/model_graph_internal.h", REPO / "src/client/render/model_graph_trace.inc",
             REPO / "src/entity/entity.c", REPO / "src/entity/entity_internal.h", REPO / "src/entity/player/construction.c",
             REPO / "src/entity/player/tick_math_internal.h", REPO / "src/core/java/random_profile_internal.h",
             REPO / "src/world/runtime/construction.c", REPO / "src/world/runtime/tile_entities/tile_entities.c"]
    paths.extend(sorted(path for path in (REPO / "src/client/render/tile_entity_renderer").rglob("*")
                        if path.is_file() and path.suffix in (".c", ".h")))
    return {str(path.resolve(strict=True)): digest(path) for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="tile-entity-renderer-frame-", dir=args.work_dir.resolve()))
    raw = json.loads((HERE / "frame-finite.json").read_text(encoding="utf-8"))["rows"]
    provenance = json.loads((HERE / "frame-provenance.json").read_text(encoding="utf-8"))
    cases = json.loads((HERE / "frame-inputs.json").read_text(encoding="utf-8"))["cases"]
    ensure(digest(HERE / "frame-finite.json") == provenance["fixture_sha256"] == RAW_SHA256 and
           provenance["rows"] == len(raw) == 97 and provenance["method_rows"] == 96 and
           provenance["original_only_constructor_rows"] == 1 and provenance["cases"] == 24,
           "complete unchanged genuine original fixture and provenance")
    ensure(raw[0] == "TILE_FRAME_CTOR wh 1 1 0 1 2 1", "original-only actual Pig constructor outcome remains bound")
    expected = raw[1:]
    inventory(expected, cases)
    # Only declared operands and independently stored source inputs go to C.
    # RESULT/SIDE/CTOR rows never enter this file or executable arguments.
    input_path = out / "inputs-only.tsv"
    input_lines = []
    for index in range(24):
        input_lines.extend(" ".join(expected[4 * index + offset].split()[1:]) for offset in (0, 1))
    input_path.write_text("\n".join(input_lines) + "\n", encoding="ascii", newline="\n")
    before = bindings(executable, input_path)
    report = {"status": "FAIL", "input_before_sha256": before,
              "original_fixture_sha256": RAW_SHA256, "original_only_constructor_row": raw[0],
              "native_input": {"path": str(input_path), "sha256": digest(input_path),
                               "fields": "declared input and actual stored input words only; no expected result/side-effect/constructor values"},
              "credit": {"literal_method_rows": 96, "cases": 24,
                         "qualified_channels": "shared LS/SN pose reads, rounded LL h/i/j/k/l arithmetic and LL identity relative to the passed Living operand; exact whole normal call order",
                         "uncredited_constructor": "original genuine READY wh/Pig versus native genuine RAW dc/LocalPlayer; no runtime actor-Class or constructor equivalence",
                         "uncredited_side_state_channels": {"SIDE token2": "Random shared uniquifier preservation does not qualify constructor consumption",
                                                            "SIDE token3": "absent native actor Random versus original instantiated private Random",
                                                            "SIDE token4": "absent native Math holder versus original instantiated Math Random",
                                                            "SIDE token8": "absent native Watcher versus original instantiated Watcher"},
                         "stored_input_limit": "actual original stored words are supplied; Java reflection setter parity is not compared",
                         "numeric_bound": "all24 measured cases are mandatory; unmeasured operations with two NaNs and a signaling operand remain explicit UnsupportedNumeric at the source PC"},
              "arithmetic_authority": {"jdk_source": "https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/hotspot/cpu/x86/templateTable_x86.cpp",
                                       "source_sections": "fop2 1409..1425; dop2 1479..1495; f2d 1722..1724",
                                       "rules": "quiet two-NaN SUB prefers left, ADD/MUL prefer right; single NaN preserves sign/payload and quiets; f2d preserves sign and shifts float payload29 with quiet bit; other arithmetic is ordered rounded SSE"}}
    try:
        pins = json.loads((HERE / "frame-source-pins.json").read_text(encoding="utf-8"))
        ensure(provenance["client_jar_sha256"] == pins["client_jar_sha256"] == original.base.JAR, "unchanged original JAR authority")
        for name in ("TileEntityRendererFrameObservation.java", "capture_frame.py", "frame-source-pins.json", "frame-inputs.json"):
            ensure(digest(HERE / name) == provenance["capture_input_sha256"][name], "original captured source/input pin: " + name)
        method = [method for method in pins["classes"]["ll"]["methods"] if method["name"] == "a" and
                  method["descriptor"] == "(Lfd;Lji;Lsj;Lls;F)V"]
        ensure(len(method) == 1 and method[0]["code_bytes"] == 153 and method[0]["code_sha256"] ==
               "0da982029051e891f91dc2cfedbc28c37fa529efa69294fab4c728d0c01fdea8", "same complete original LL frame Code")
        command = [str(executable), str(input_path.resolve())]
        receipt = capture(command, out, "native", 60, os.environ.copy(), out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        ensure(receipt["command"] == command, "actual executable received only the input-only file")
        for stream in ("stdout", "stderr"):
            name = receipt[stream + "_file"]
            ensure(Path(name).name == name, "local capture stream name")
            path = out / name
            ensure(path.stat().st_size == receipt[stream + "_bytes"] and digest(path) == receipt[stream + "_sha256"], "complete unchanged stream receipt")
        lines = (out / receipt["stdout_file"]).read_text(encoding="utf-8").splitlines()
        ensure(all(line and line == line.strip() and " ".join(line.split()) == line for line in lines), "complete nonblank output tokens")
        ensure(len(lines) == 100 and lines[0] ==
               "TILE_FRAME_NATIVE_OPERAND dc RAW original=wh constructor=uncredited random=0 math=0 watcher=0",
               "honest native RAW LocalPlayer operand and absent constructor children, no forged original constructor row")
        report["native_operand_receipt"] = lines[0]
        footer = re.fullmatch(r"Source TileEntityRenderer frame: cases=24; rows=96; controls=2; checks=([1-9][0-9]*); cleanup=1", lines[-1])
        ensure(footer is not None, "whole normal method comparison and checked owner cleanup")
        actual = lines[1:97]
        controls = lines[97:99]
        ensure(controls == [
            "TILE_FRAME_CONTROL numeric_late UNSUPPORTED_NUMERIC 93 74 refs=1 completed_hi=1 preserved_jkl=1 native_policy=1",
            "TILE_FRAME_CONTROL mxcsr UNSUPPORTED_FP 45 26 refs=1 preserved_scalars=1 unchanged_control=1 caller_restored=1 native_policy=1"],
            "complete separate authored numeric-prefix/MXCSR controls; no original equality claim")
        report["native_controls"] = {"rows": controls, "count": 2, "original_equality_credit": False,
                                     "scope": "late double-component numeric rejection preserves completed h/i and source references; strict control rejection follows e/g/n and preserves all scalar sentinels/MXCSR"}
        inventory(actual, cases)
        mismatches = [{"index": index, "native": actual[index], "original": expected[index]}
                      for index in range(96) if actual[index] != expected[index]]
        report.update(rows=len(actual), cases=24, checks=int(footer[1]), mismatch_count=len(mismatches), mismatches=mismatches[:20])
        ensure(not mismatches, "native/original exact frame method words differ; see run.json")
        report["status"] = "PASS"
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    finally:
        after = bindings(executable, input_path)
        report["input_after_sha256"] = after
        if before != after:
            report.update(status="FAIL", error="source/reference/input/executable bytes changed")
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "rows": report.get("rows", 0), "report": str(out / "run.json")}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
